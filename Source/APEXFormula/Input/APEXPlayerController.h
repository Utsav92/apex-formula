#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Vehicles/APEXDrivingTypes.h"
#include "APEXPlayerController.generated.h"

class UAPEXInputConfig;
class AFormulaVehiclePawn;
class AAPEXCameraManager;
class AAPEXRaceManager;
struct FInputActionValue;

/**
 * The player's controller: possesses the player car, installs the Enhanced Input mapping context, turns key / stick / trigger input into normalised
 * throttle, brake and steering (keyboard ramp, speed-sensitive limit, gamepad dead zones) and writes them to the car through the single
 * SetDriverInputs() entry point. Also owns pause, the pause menu, camera switching, reset, driving-mode selection and the input diagnostics.
 *
 * Command line: -APEXInputDebug logs controller / pawn / mode / inputs every 2 s; -APEXInputBot drives the car through REAL injected key events
 * (a scripted test harness that exercises the whole input chain; not a human). -APEXPreset... is unrelated (lighting).
 */
UCLASS()
class APEXFORMULA_API AAPEXPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAPEXPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Driving") EAPEXSteerPreset SteerPreset = EAPEXSteerPreset::Sport;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Driving") float GamepadDeadZone = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Driving") float GamepadSensitivity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Driving") float TriggerMin = 0.05f;       // trigger calibration: below this reads as released
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Driving") bool bVibration = true;

	UFUNCTION(BlueprintCallable, Category="Driving") void SetDrivingMode(EAPEXDrivingMode Mode);
	UFUNCTION(BlueprintPure, Category="Driving") EAPEXDrivingMode GetDrivingMode() const;
	UFUNCTION(BlueprintCallable, Category="Driving") void SetPreset(EAPEXSteerPreset Preset);
	UFUNCTION(BlueprintPure, Category="Driving") EAPEXRaceFlow GetRaceFlow() const;
	static FString FlowName(EAPEXRaceFlow Flow);
	static FString ModeName(EAPEXDrivingMode Mode);
	static FString PresetName(EAPEXSteerPreset Preset);

	AFormulaVehiclePawn* GetCar() const;
	UAPEXInputConfig* GetInputConfig() const { return InputConfig; }
	AAPEXCameraManager* GetCameraManager() const;

	// controller-level processed commands (what was last written to the car)
	float GetThrottleCmd() const { return ThrottleCmd; }
	float GetBrakeCmd() const { return BrakeCmd; }
	float GetSteerCmd() const { return SteerOut; }
	bool IsUsingGamepad() const { return bGamepadActive; }

	// pause menu
	bool IsMenuOpen() const { return bMenuOpen; }
	void TogglePause();
	/** Title, lines and selected row for the HUD. */
	void GetMenuView(FString& OutTitle, TArray<FString>& OutLines, int32& OutSelected) const;
	void Rumble(float Intensity, float Seconds);
	int32 GetRumbleCount() const { return RumbleCount; }
	/** A gamepad vanished mid-race: clear stuck inputs and pause so the player is never left with a car they cannot steer. */
	void HandleDeviceLost();

	/** Race flag the player should see: text and colour (empty when none). */
	FString GetFlag(FLinearColor& OutColour) const;
	bool IsStartMenu() const { return bStartMenu; }
	/** Remapping: begin capturing the next key for a slot (menu), or apply a key directly (tests). */
	void BeginRebind(FName SlotName);
	bool ApplyRebind(FName SlotName, const FKey& Key, FString& OutError);
	bool IsCapturingKey() const { return bCapturing; }
	int32 GetMenuIndex() const { return MenuIndex; }
	int32 GetMenuPage() const { return MenuPage; }
	void ResetBindingsAndSave();
	void StartRace();

	/** One-line telemetry string (also what -APEXInputDebug logs). */
	FString TelemetryLine() const;

private:
	UPROPERTY() TObjectPtr<UAPEXInputConfig> InputConfig;
	float ThrottleIn = 0.f, BrakeIn = 0.f, SteerTarget = 0.f;       // from the Enhanced Input actions
	float ThrottleCmd = 0.f, BrakeCmd = 0.f, SteerRamp = 0.f, SteerOut = 0.f;
	bool bGamepadActive = false;
	int32 RumbleCount = 0;
	float LastRumble = 0.f;
	bool bMenuOpen = false, bStartMenu = false, bShowStartMenu = false, bCapturing = false, bIntroEnabled = false, bIntroStarted = false;
	FName CaptureSlot;
	float CaptureDelay = 0.f;
	FString MenuMessage;
	int32 ParentRow(int32 Page) const;
	int32 MenuPage = 0, MenuIndex = 0;
	float DebugTimer = 0.f;
	bool bLoggedPossession = false, bWarnedNoPawn = false, bWarnedAIControl = false, bWarnedBlocked = false;
	float BlockedTimer = 0.f;
	FDelegateHandle ControllerConnHandle;

	// scripted-input bot (test harness)
	bool bBot = false;
	TMap<FKey, bool> BotKeys;
	float BotSteerPhase = 0.f, BotElapsed = 0.f, BotLogTimer = 0.f, BotFinishedTimer = 0.f;
	float BotMaxSpeedKph = 0.f;
	int32 BotSteerPulses = 0, BotThrottleFrames = 0, BotBrakeFrames = 0, BotResets = 0;
	float BotStuck = 0.f;
	float DbgShotAt = -1.f, DbgPauseAt = -1.f, DbgClock = 0.f, DbgQuitAt = -1.f;   // -APEXShotAt= / -APEXPauseAt= (seconds of play)
	void TickBot(float Dt);

	// scripted feature self-test (-APEXInputSelfTest): every step injects real input events and logs PASS / FAIL
	bool bSelfTest = false;
	int32 SelfStep = 0, SelfPass = 0, SelfFail = 0;
	float SelfT = 0.f, SelfMark0 = 0.f, SelfMark1 = 0.f;
	FVector SelfPos = FVector::ZeroVector;
	TArray<TPair<FKey, float>> PendingReleases;
	void TickSelfTest(float Dt);
	void SelfCheck(bool bOk, const FString& Name, const FString& Detail);
	void SelfTap(const FKey& Key);
	int32 SelfSub = 0;
	void SelfNext() { ++SelfStep; SelfT = 0.f; SelfSub = 0; }
	bool bPadStreamZero = false;
	bool bPreDone = false, bIntroChecked = false, bIntroForced = false; int32 SelfMenuSub = 0; float SelfMenuT = 0.f, SelfPreT = 0.f;
	void BotSet(const FKey& Key, bool bDown);

	void InstallMapping();
	void OnThrottle(const FInputActionValue& V);
	void OnThrottleEnd(const FInputActionValue& V) { ThrottleIn = 0.f; }
	void OnBrake(const FInputActionValue& V);
	void OnBrakeEnd(const FInputActionValue& V) { BrakeIn = 0.f; }
	void OnSteer(const FInputActionValue& V);
	void OnSteerEnd(const FInputActionValue& V) { SteerTarget = 0.f; }
	void OnShiftUp();
	void OnShiftDown();
	void OnHandbrake(bool bOn);
	void OnBoost(bool bOn);
	void OnOvertake(bool bOn);
	void OnCameraSwitch();
	void OnLookBack(bool bOn);
	void OnResetCar();
	void OnPause();
	void OnConfirm();
	void OnCancel();
	void MenuMove(int32 Delta);
	int32 MenuCount() const;
	void RestartRace();
};
