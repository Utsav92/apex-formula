#pragma once

#include "CoreMinimal.h"
#include "Vehicles/APEXDrivingTypes.h"
#include "GameFramework/Pawn.h"
#include "Racing/APEXRaceTypes.h"
#include "Racing/APEXTyres.h"
#include "FormulaVehiclePawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UFormulaLapTimingComponent;
class AAPEXRaceManager;
class AFormulaTrack;
class UFormulaMobileInputComponent;

/** Tunable car parameters. SI units except where noted. Fictional values. */
USTRUCT(BlueprintType)
struct FFormulaCarSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MassKg = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WheelbaseM = 3.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PowerW = 700000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxTractionForceN = 12000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxBrakeForceN = 26000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSteerAngleDeg = 22.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TyreMu = 1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AirDensity = 1.225f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FrontalAreaM2 = 1.0f;
	/** CdA = Cd * FrontalArea. 1.45 with 700 kW settles at ~92 m/s (331 km/h), like a high-downforce open-wheeler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DragCoefficient = 1.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LiftCoefficientDown = 3.0f; // positive = downforce
	/** Safety clamp only. Drag, not this cap, should set the real top speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TopSpeedMps = 100.f;
};

/**
 * Phase 1 car: a grip-limited bicycle model with real drag/downforce formulas.
 * Deliberately not Chaos-based yet: deterministic, cheap on iPhone, and testable.
 */
UCLASS()
class APEXFORMULA_API AFormulaVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AFormulaVehiclePawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> Visual;
	/** 0,1 = front left/right; 2,3 = rear left/right. Pivot steers (fronts), mesh spins. */
	UPROPERTY() TObjectPtr<USceneComponent> WheelPivot[4];
	UPROPERTY() TObjectPtr<UStaticMeshComponent> WheelMesh[4];
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USpringArmComponent> ChaseArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> ChaseCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UFormulaLapTimingComponent> LapTiming;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UFormulaMobileInputComponent> MobileInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car") FFormulaCarSetup Setup;

	/** Track centreline the car knows about (for the HUD map and the auto-driver). Empty if no track. */
	const TArray<FVector>& GetPathPoints() const { return AutoPath; }

	/** Inputs, all normalised. Touch UI, tilt and controllers write these. */
	UFUNCTION(BlueprintCallable, Category="Input") void SetThrottle(float Value) { Throttle = FMath::Clamp(Value, 0.f, 1.f); }
	UFUNCTION(BlueprintCallable, Category="Input") void SetBrake(float Value)    { Brake    = FMath::Clamp(Value, 0.f, 1.f); }
	UFUNCTION(BlueprintCallable, Category="Input") void SetSteer(float Value)    { Steer    = FMath::Clamp(Value, -1.f, 1.f); }

	UFUNCTION(BlueprintPure, Category="Car") float GetSpeedKph() const { return SpeedMps * 3.6f; }
	UFUNCTION(BlueprintPure, Category="Car") float GetDownforceN() const { return LastDownforceN; }

	// ---- Player driving (written by AAPEXPlayerController; the AI path never goes through these) ---------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Driving") EAPEXDrivingMode DrivingMode = EAPEXDrivingMode::Manual;
	/** Manual is the default and the only mode the game starts in; AutonomousDemo is opt-in (-APEXAutoDrive or the pause menu). */
	void SetDrivingMode(EAPEXDrivingMode Mode);
	/** The controller's single write point: applies only when this car is human-driven, not a demo and released from the grid (see FAPEXDriveMath::CanAcceptDriverInput). */
	bool SetDriverInputs(float InThrottle, float InBrake, float InSteer);
	void SetSteerPreset(EAPEXSteerPreset Preset);
	EAPEXSteerPreset GetSteerPreset() const { return SteerPreset; }
	void ShiftUp();
	void ShiftDown();
	void SetManualGearbox(bool bManual) { bManualGearbox = bManual; }
	bool IsManualGearbox() const { return bManualGearbox; }
	void SetHandbrake(bool bOn) { bHandbrakeHeld = bOn; }
	void SetBoostHeld(bool bOn) { bBoostHeld = bOn; }
	void SetOvertakeHeld(bool bOn) { bOvertakeHeld = bOn; }
	void SetAssists(bool bOn) { bAssistsOn = bOn; }
	bool AreAssistsOn() const { return bAssistsOn || DrivingMode == EAPEXDrivingMode::Assisted; }
	/** Puts the car back on the track centre at a validated clear spot (never inside a barrier or on top of another car). Returns the path index used. */
	int32 ResetToTrack();
	/** Called by another car (or a wall) hitting this one: velocity change along the car, and a sideways shove in cm. */
	void ReceiveContact(float NewSpeedMps, float LateralShoveCm);

	UFUNCTION(BlueprintPure, Category="Driving") float GetThrottleInput() const { return EffThrottle; }
	UFUNCTION(BlueprintPure, Category="Driving") float GetBrakeInput() const { return EffBrake; }
	UFUNCTION(BlueprintPure, Category="Driving") float GetSteerInput() const { return EffSteer; }
	UFUNCTION(BlueprintPure, Category="Driving") int32 GetGear() const { return Gear; }
	UFUNCTION(BlueprintPure, Category="Driving") float GetRev01() const { return bHoldOnGrid ? FMath::Lerp(0.15f, 0.95f, PreStartRev) : FAPEXDriveMath::Rev01(SpeedMps, Gear); }
	/** On the grid the car is held, but the driver may blip the throttle: it only revs the engine (HUD and audio), it never moves the car. */
	void SetPreStartRev(float Value) { PreStartRev = FMath::Clamp(Value, 0.f, 1.f); }
	float GetPreStartRev() const { return PreStartRev; }

	// ---- sector timing (lap split in three equal parts of the lap distance; recorded only for cleanly driven sectors) ----
	int32 GetCurrentSector() const { return CurrentSector; }
	float GetLiveSectorTime() const { return SectorClock; }
	float GetLastSectorTime(int32 S) const { return LastSector[FMath::Clamp(S, 0, 2)]; }
	float GetBestSectorTime(int32 S) const { return BestSector[FMath::Clamp(S, 0, 2)]; }
	UFUNCTION(BlueprintPure, Category="Driving") float GetEnergy() const { return Energy; }
	UFUNCTION(BlueprintPure, Category="Driving") bool IsBoosting() const { return bBoostActive; }
	UFUNCTION(BlueprintPure, Category="Driving") bool IsOvertakeAvailable() const { return bOvertakeAvailable; }
	UFUNCTION(BlueprintPure, Category="Driving") bool IsOvertakeActive() const { return bOvertakeActive; }
	/** Last commanded driving inputs before gearbox / assists, for the telemetry HUD and the tests. */
	float GetRawThrottle() const { return Throttle; }
	float GetRawBrake() const { return Brake; }
	float GetRawSteer() const { return Steer; }
	/** Test harness: what the autopilot would command right now, without applying it (used by the scripted-input bot). */
	void GetAutopilotSuggestion(float Dt, float& OutThrottle, float& OutBrake, float& OutSteer);

	// ---- Gameplay cameras ------------------------------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> CockpitCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> NoseCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> FrontWingCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UCameraComponent> RearCamera;
	void SetCameraView(EAPEXCamView View) { CameraView = View; }
	EAPEXCamView GetCameraView() const { return CameraView; }
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	/** Player steering law: a normalised input of +-1 means "turn at the tyre limit" at any speed (full lock at low speed, a few tenths of a degree at 90 m/s),
	 *  so one key press does not slam the car into the grip clamp. Returns the model's steer value (-1..1 of MaxSteerAngle). AI cars do not use this. */
	static float NormalisedToModelSteer(const FFormulaCarSetup& Setup, float Normalised, float SpeedMps, float DownforceN, float TyreScale);
	/** Inverse of the above (used by the scripted-input bot to turn the autopilot's model steering into key presses). */
	static float ModelSteerToNormalised(const FFormulaCarSetup& Setup, float ModelSteer, float SpeedMps, float DownforceN, float TyreScale);
	float GetTyreScaleForSteering() const { return bUseTyres ? TyreGripFactor : 1.f; }
	float GetDownforceForSteering() const { return LastDownforceN; }

	/** Pure model step: no world, no actor. Used by Tick and by the automation tests. */
	static void StepDynamics(const FFormulaCarSetup& Setup, float Throttle, float Brake, float Steer,
		float Dt, float& InOutSpeedMps, float& OutYawRateRad, float& OutDownforceN);

	/** Drives the centreline with pure pursuit. Also enabled by the -APEXAutoDrive command-line flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bAutoDrive = false;

	// ---- Racing (grid / AI) ----------------------------------------------------------------------------------
	/** Turn this car into a computer-controlled competitor. Call before FinishSpawning. */
	void ConfigureAsAI(int32 InDriverIndex, const FAPEXDriver& Driver, const FAPEXTeam& Team);
	/** Livery (team colours, number decal) for any car, player included. */
	void ApplyDriverLook(const FAPEXTeam& Team, const FAPEXDriver& Driver);

	bool IsAI() const { return bIsAI; }
	/** Ask for a stop now (used by the cinematic): takes the team box if it is free, then the normal pit logic runs. */
	void RequestPitNow();
	/** Contact guard: never exceed this speed this tick (used so the rear car matches the speed of a car ahead in its lane). */
	void LimitSpeedMps(float Mps) { SpeedMps = FMath::Min(SpeedMps, FMath::Max(Mps, 0.f)); }
	int32 GetDriverIndex() const { return DriverIndex; }
	float GetSpeedMps() const { return SpeedMps; }
	/** Metres since the start line, continuous across laps (negative while still on the grid). */
	float GetRaceDistanceM() const { return UnwrappedProgressM; }
	/** Where the car actually is across the road (0 = centreline, + = left). Not the controller's target offset. */
	float GetLateralCm() const { return MeasuredLateralCm; }
	float GetLapLengthM() const { return LapLengthM; }
	void SetHoldOnGrid(bool bHold) { bHoldOnGrid = bHold; }
	bool IsHeldOnGrid() const { return bHoldOnGrid; }
	bool IsAutoDriving() const { return bAutoDrive; }
	/** Set by the race manager (~10 Hz): the nearest car ahead/behind and whether to animate visuals. */
	void SetNeighbours(float InGapAheadM, float InAheadSpeed, float InAheadLateralCm, float InGapBehindM, float InBehindLateralCm)
	{ GapAheadM = InGapAheadM; AheadSpeedMps = InAheadSpeed; AheadLateralCm = InAheadLateralCm; GapBehindM = InGapBehindM; BehindLateralCm = InBehindLateralCm; }
	void SetVisualsActive(bool bActive) { bVisualsActive = bActive; }
	/** A person (not the autopilot, not an AI) is driving this car. */
	bool IsHumanDriven() const { return !bIsAI && !bAutoDrive; }
	/** Set by the race manager (~10 Hz): the nearest human-driven car behind within 25 m, in any lane within 6 m across (9999 = none). */
	void SetHumanBehind(float GapM, float AcrossCm) { HumanBehindGapM = GapM; HumanBehindLatCm = AcrossCm; }
	int32 GetDefendEvents() const { return DefendEvents; }
	/** A car alongside (within ~6 m along, ~3 m across): keep side-by-side separation. */
	void SetAlongside(bool bHas, float OtherLateralCm) { bHasAlongside = bHas; AlongsideLatCm = OtherLateralCm; }
	/** Grid column (+220 left / -220 right): held until the car is clear of the grid. Also places the controller's line. */
	void SetGridLateral(float Cm) { GridLateralCm = Cm; LateralCm = Cm; TargetLateralCm = Cm; }
	void SetWetnessPaceScale(float Scale) { WetnessPaceScale = Scale; }
	float GetPaceFactor() const { return PaceFactor; }
	void ScalePaceFactor(float K) { PaceFactor *= K; }

	// ---- Tyres, strategy and pit stops -------------------------------------------------------------------------
	/** Set the race plan. Called by the race manager for every car (and by BeginPlay with a default for a lone car). */
	void InitTyresAndStrategy(int32 InTotalLaps, float ForecastWetness, const FAPEXDriver& Driver, int32 InTeamIndex);
	void SetTrackWetness(float W) { TrackWetness = W; }
	void SetRaceManager(AAPEXRaceManager* M);
	EAPEXTyre GetCompound() const { return Compound; }
	float GetTyreWear() const { return TyreWear; }
	float GetTyreGrip() const { return TyreGripFactor; }
	int32 GetPitStops() const { return PitStops; }
	bool IsInPit() const { return PitState != EPitState::None; }
	bool IsPitRequested() const { return bPitRequested; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

private:
	UFUNCTION()
	void OnLapDone(int32 LapNumber, float LapTimeSeconds);

	void ComputeAutoDrive(float Dt);
	void AdvancePathIndex();
	void UpdateRacecraft(float Dt);
	/** One fixed simulation step: dynamics, movement, collision, ground snap, race progress. */
	void SimStep(float Dt);
	void UpdateRaceProgress();
	void ShowDebugText() const;

	float Throttle = 0.f;
	float Brake = 0.f;
	float Steer = 0.f;
	// effective commands after gearbox / assists / handbrake (these, not the raw ones, drive the dynamics of a player car)
	float EffThrottle = 0.f, EffBrake = 0.f, EffSteer = 0.f;
	int32 Gear = 1;
	bool bManualGearbox = false, bHandbrakeHeld = false, bBoostHeld = false, bOvertakeHeld = false, bAssistsOn = false;
	bool bBoostActive = false, bOvertakeAvailable = false, bOvertakeActive = false;
	float Energy = 1.f;                    // 0..1 deployable reserve
	EAPEXSteerPreset SteerPreset = EAPEXSteerPreset::Sport;
	EAPEXCamView CameraView = EAPEXCamView::Chase;
	float HumanBehindGapM = 9999.f, HumanBehindLatCm = 0.f;
	bool bDefendingHuman = false;
	int32 DefendEvents = 0;
	float SuspPitch = 0.f, SuspRoll = 0.f, SuspSquat = 0.f, SuspPrevSpeed = 0.f, SuspPrevYaw = 0.f, VisualBaseZ = 0.f;
	bool bSuspInit = false;
	float PreStartRev = 0.f, SectorClock = 0.f, LastSector[3] = { 0.f, 0.f, 0.f }, BestSector[3] = { 0.f, 0.f, 0.f };
	int32 CurrentSector = -1;
	bool bSectorClean = false;
	void UpdateSectors(float ProgressM);
	float ContactCooldown = 0.f, PowerScaleNow = 1.f, OvertakeDragScale = 1.f, YawBoost = 1.f, OtherCarsAge = 99.f;
	TArray<TWeakObjectPtr<AFormulaVehiclePawn>> OtherCars;
	TWeakObjectPtr<AController> LastRumbleTarget;
	void ComputeEffectiveInputs(float Dt);
	void ResolveCarContacts(float Dt);
	UCameraComponent* ActiveCameraComponent() const;
	float SpeedMps = 0.f;
	float LastDownforceN = 0.f;

	TArray<FVector> AutoPath;
	TArray<float> PathCurvature; // 1/R in 1/cm per centreline point, smoothed over 3 points
	TArray<float> PathCurvatureSigned; // same magnitude, + = turning left
	float StuckTimer = 0.f;
	void RecoverToTrack();

	enum class EPitState : uint8 { None, InLane, Stopped, Exiting };
	EPitState PitState = EPitState::None;
	bool bUseTyres = true;
	EAPEXTyre Compound = EAPEXTyre::Medium;
	float TyreWear = 0.f, TyreGripFactor = 1.f, TrackWetness = 0.f;
	FAPEXStrategy Strategy;
	TSet<uint8> UsedCompounds;
	int32 PitStops = 0, TotalLaps = 3, TeamIndex = 0, DriverIdxForPit = 0;
	bool bPitRequested = false;
	float PitEvalTimer = 0.f, PitTimer = 0.f, PitLaneElapsed = 0.f, PitMaxLimitedSpeed = 0.f;
	TArray<FVector> PitPath;
	int32 PitIdx = 0, PitBoxIdx = 0, PitEntryCentreIdx = 0;
	TWeakObjectPtr<AAPEXRaceManager> RaceMgr;
	TWeakObjectPtr<AFormulaTrack> TrackRef;
	float PitLimitAt(int32 PathIdx) const;
	void UpdateTyres(float Dt, float LateralAccel);
	void EvaluatePit(float Dt);
	void BeginPitEntry();
	void ComputePitDrive(float Dt);
	void FinishPitStop();
	void LeavePitLane();
	int32 PathIndex = 0;
	float WheelSpinDeg = 0.f;
	void UpdateWheels(float Dt);
	/** Applies /Game/Materials/Car/MI_APEX_* by slot name. -APEXPaint=GlossyRed|MetallicBlue|MatteBlack|WhitePearl|OrangeRacing. */
	void ApplyCarMaterials();
	float AutoDriveElapsed = 0.f;
	float NextAutoLogTime = 0.f;
	bool bShotTaken = false;
	float FinishedElapsed = 0.f;
	bool bHeroMode = false;

	// race state
	bool bIsAI = false;
	bool bHoldOnGrid = false;
	bool bVisualsActive = true;
	int32 DriverIndex = 0;
	float PaceFactor = 1.f, Aggression = 0.5f, Consistency = 0.97f, WetnessPaceScale = 1.f;
	float LateralCm = 0.f, TargetLateralCm = 0.f;   // the line the controller steers along (offset from the centreline)
	float MeasuredLateralCm = 0.f;                  // where the car really is; written only by UpdateRaceProgress
	int32 OvertakeSide = 0;
	float GapAheadM = 9999.f, AheadSpeedMps = 0.f, AheadLateralCm = 0.f, GapBehindM = 9999.f, BehindLateralCm = 0.f;
	float DragScale = 1.f, MistakeTimer = 0.f, LastFreeTargetSpeed = 90.f;
	float GridLateralCm = 0.f, AlongsideLatCm = 0.f;
	bool bHasAlongside = false;
	float LapLengthM = 3500.f, SegLenCm = 835.f;
	float UnwrappedProgressM = 0.f, PrevProgressM = 0.f;
	bool bProgressInit = false;
	FRandomStream RaceRand;
	// Real frame-time stats over each 10 s log window (unclamped DeltaSeconds).
	double FrameAccum = 0.0; int32 FrameCount = 0; float FrameMax = 0.f;
};
