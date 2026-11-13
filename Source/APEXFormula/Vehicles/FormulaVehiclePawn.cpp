#include "Vehicles/FormulaVehiclePawn.h"
#include "Racing/FormulaLapTimingComponent.h"
#include "Racing/FormulaTrack.h"
#include "Racing/APEXRaceManager.h"
#include "Input/FormulaMobileInputComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Components/InputComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Input/APEXPlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexCar, Log, All);

namespace
{
	constexpr float Gravity = 9.81f;
	constexpr float CmPerM = 100.f;

	/** Device safe-area insets (L, T, R, B) in pixels, clamped later by the layout. Zero if unavailable. */
	FVector4 GetSafeMargins()
	{
		FVector4 Margins(0, 0, 0, 0);
		if (FSlateApplication::IsInitialized())
		{
			FDisplayMetrics Metrics;
			FSlateApplication::Get().GetDisplayMetrics(Metrics);
			Margins = FVector4(Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y,
				Metrics.TitleSafePaddingSize.Z, Metrics.TitleSafePaddingSize.W);
		}
		return Margins;
	}
}

AFormulaVehiclePawn::AFormulaVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	constexpr float BodyHalfZ = 45.f;
	Body->SetBoxExtent(FVector(280.f, 100.f, BodyHalfZ)); // matches the 5.7 x 2.0 m car mesh, cm
	Body->SetCollisionProfileName(TEXT("Pawn"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // cars pass through each other; avoidance is behavioural for now
	RootComponent = Body;

	// The car mesh's origin is at ground level, so drop it by the box half-height.
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -BodyHalfZ));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CarMesh(TEXT("/Game/Vehicles/SM_F1_Body.SM_F1_Body"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelAsset(TEXT("/Game/Vehicles/SM_F1_Wheel.SM_F1_Wheel"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CarMesh.Succeeded())
	{
		Visual->SetStaticMesh(CarMesh.Object);
	}
	else if (Cube.Succeeded()) // imported art missing: fall back to a box so the game still runs
	{
		Visual->SetStaticMesh(Cube.Object);
		Visual->SetRelativeScale3D(FVector(5.f, 1.8f, 0.8f));
	}

	// Wheels: hub heights match the tyre radii (rear .365 m, front ~.33 m); the front tyre is narrower.
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bFront = i < 2;
		const float Side = (i % 2 == 0) ? -1.f : 1.f;
		WheelPivot[i] = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("WheelPivot%d"), i));
		WheelPivot[i]->SetupAttachment(Body);                                  // wheels stay with the chassis root; the body mesh moves on its "suspension"
		WheelPivot[i]->SetRelativeLocation(FVector(bFront ? 170.f : -170.f, Side * 80.f, (bFront ? 33.f : 36.5f) - BodyHalfZ));
		WheelMesh[i] = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("WheelMesh%d"), i));
		WheelMesh[i]->SetupAttachment(WheelPivot[i]);
		WheelMesh[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (WheelAsset.Succeeded()) { WheelMesh[i]->SetStaticMesh(WheelAsset.Object); }
		if (bFront) { WheelMesh[i]->SetRelativeScale3D(FVector(0.9f, 0.75f, 0.9f)); }
	}

	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(Body);
	ChaseArm->TargetArmLength = 850.f;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, 200.f);
	ChaseArm->bEnableCameraLag = true;
	ChaseArm->CameraLagSpeed = 12.f;
	ChaseArm->bEnableCameraRotationLag = true;
	ChaseArm->CameraRotationLagSpeed = 8.f;
	ChaseArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	ChaseArm->bDoCollisionTest = false; // the camera must not snap in on barriers

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);

	auto MakeCam = [this](const TCHAR* Name, FVector Loc, FRotator Rot, float Fov) -> UCameraComponent*
	{
		UCameraComponent* C = CreateDefaultSubobject<UCameraComponent>(Name);
		C->SetupAttachment(Body);
		C->SetRelativeLocationAndRotation(Loc, Rot);
		C->FieldOfView = Fov;
		C->bAutoActivate = false;
		return C;
	};
	CockpitCamera   = MakeCam(TEXT("CockpitCamera"),   FVector(40.f, 0.f, 92.f),   FRotator(-2.f, 0.f, 0.f),   100.f);
	NoseCamera      = MakeCam(TEXT("NoseCamera"),      FVector(250.f, 0.f, 66.f),  FRotator(-1.f, 0.f, 0.f),   96.f);
	FrontWingCamera = MakeCam(TEXT("FrontWingCamera"), FVector(345.f, 0.f, 24.f),  FRotator(1.f, 0.f, 0.f),    92.f);
	RearCamera      = MakeCam(TEXT("RearCamera"),      FVector(-330.f, 0.f, 150.f), FRotator(-7.f, 180.f, 0.f), 88.f);

	LapTiming = CreateDefaultSubobject<UFormulaLapTimingComponent>(TEXT("LapTiming"));
	MobileInput = CreateDefaultSubobject<UFormulaMobileInputComponent>(TEXT("MobileInput"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AFormulaVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyCarMaterials();

	if (!bIsAI) { SetSteerPreset(SteerPreset); }           // the player's gearbox / assists follow the preset
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXAutoDrive")))
	{
		bAutoDrive = true;
		if (!bIsAI) { DrivingMode = EAPEXDrivingMode::AutonomousDemo; }   // demo/test runs only: the default is Manual
	}

	// Debug beauty-shot mode: car parked, camera orbiting close. -APEXHeroYaw/-APEXHeroPitch/-APEXHeroDist tune it.
	if (!bIsAI && FParse::Param(FCommandLine::Get(), TEXT("APEXHero")))
	{
		float Yaw = 145.f, Pitch = -12.f, Dist = 520.f;
		FParse::Value(FCommandLine::Get(), TEXT("APEXHeroYaw="), Yaw);
		FParse::Value(FCommandLine::Get(), TEXT("APEXHeroPitch="), Pitch);
		FParse::Value(FCommandLine::Get(), TEXT("APEXHeroDist="), Dist);
		bHeroMode = true;
		bAutoDrive = true; DrivingMode = EAPEXDrivingMode::AutonomousDemo;
		ChaseArm->bEnableCameraLag = false;
		ChaseArm->bEnableCameraRotationLag = false;
		ChaseArm->TargetArmLength = Dist;
		ChaseArm->SocketOffset = FVector(0.f, 0.f, 45.f);
		ChaseArm->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));
	}

	// Debug free camera: -APEXCam=X,Y,Z,Pitch,Yaw (cm, degrees; world space). The car is parked (hero mode) so frames are repeatable.
	FString CamArg;
	if (!bIsAI && FParse::Value(FCommandLine::Get(), TEXT("APEXCam="), CamArg, /*bShouldStopOnSeparator=*/false))
	{
		TArray<FString> V; CamArg.ParseIntoArray(V, TEXT(","));
		if (V.Num() >= 5)
		{
			bHeroMode = true; bAutoDrive = true; DrivingMode = EAPEXDrivingMode::AutonomousDemo;
			ChaseArm->bEnableCameraLag = false; ChaseArm->bEnableCameraRotationLag = false;
			ChaseArm->bDoCollisionTest = false; ChaseArm->bUsePawnControlRotation = false;
			ChaseArm->bInheritPitch = false; ChaseArm->bInheritYaw = false; ChaseArm->bInheritRoll = false;
			ChaseArm->SetUsingAbsoluteLocation(true); ChaseArm->SetUsingAbsoluteRotation(true);
			ChaseArm->TargetArmLength = 0.f; ChaseArm->SocketOffset = FVector::ZeroVector;
			ChaseArm->SetWorldLocationAndRotation(FVector(FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2])),
				FRotator(FCString::Atof(*V[3]), FCString::Atof(*V[4]), 0.f));
		}
	}

	if (TActorIterator<AFormulaTrack> It(GetWorld()); It)
	{
		LapTiming->Checkpoints = It->GetCheckpoints();
		AutoPath = It->GetCenterline();
		LapLengthM = It->GetLapLengthMeters();
		TrackRef = *It;
		PitPath = It->GetPitPath();
		SegLenCm = LapLengthM * 100.f / FMath::Max(AutoPath.Num(), 1);
		UE_LOG(LogApexCar, Log, TEXT("Track found: lap %.0f m, %d checkpoints"), It->GetLapLengthMeters(), LapTiming->Checkpoints.Num());
	}

	// Curvature per point (Menger: kappa = 4*Area / (a*b*c)), then the max over 3 neighbours to damp noise.
	const int32 PN = AutoPath.Num();
	TArray<float> Raw;
	Raw.SetNumZeroed(PN);
	for (int32 i = 0; i < PN; ++i)
	{
		const FVector& A = AutoPath[(i + PN - 1) % PN];
		const FVector& B = AutoPath[i];
		const FVector& C = AutoPath[(i + 1) % PN];
		const float a = FVector::Dist2D(B, C), b = FVector::Dist2D(A, C), c = FVector::Dist2D(A, B);
		const float Area2 = FMath::Abs((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X)); // = 2 * area
		Raw[i] = (a * b * c > KINDA_SMALL_NUMBER) ? (2.f * Area2) / (a * b * c) : 0.f;
	}
	PathCurvature.SetNumZeroed(PN);
	PathCurvatureSigned.SetNumZeroed(PN);
	for (int32 i = 0; i < PN; ++i)
	{
		PathCurvature[i] = FMath::Max3(Raw[(i + PN - 1) % PN], Raw[i], Raw[(i + 1) % PN]);
		// Turn direction from the cross product of consecutive segments (+ = left).
		const FVector& A = AutoPath[(i + PN - 1) % PN];
		const FVector& B = AutoPath[i];
		const FVector& C = AutoPath[(i + 1) % PN];
		const float Cross = (B.X - A.X) * (C.Y - B.Y) - (B.Y - A.Y) * (C.X - B.X);
		PathCurvatureSigned[i] = (Cross >= 0.f ? 1.f : -1.f) * PathCurvature[i];
	}

	// Where the pit entry leaves the racing line (nearest centreline point to the first pit-path point).
	if (PitPath.Num() > 0)
	{
		float BestD = TNumericLimits<float>::Max();
		for (int32 i = 0; i < AutoPath.Num(); ++i)
		{
			const float D = FVector::DistSquared2D(AutoPath[i], PitPath[0]);
			if (D < BestD) { BestD = D; PitEntryCentreIdx = i; }
		}
	}
	bUseTyres = !FParse::Param(FCommandLine::Get(), TEXT("APEXNoTyres"));   // debug / baseline: constant grip, no stops
	InitTyresAndStrategy(LapTiming->TargetLaps, 0.f, UAPEXGridLibrary::GetDrivers()[0], 0);

	// Start the pursuit search at the nearest centreline point.
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i < AutoPath.Num(); ++i)
	{
		const float D = FVector::DistSquared2D(GetActorLocation(), AutoPath[i]);
		if (D < Best) { Best = D; PathIndex = i; }
	}

	LapTiming->OnLapCompleted.AddDynamic(this, &AFormulaVehiclePawn::OnLapDone);
}

void AFormulaVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindTouch(IE_Pressed,  MobileInput.Get(), &UFormulaMobileInputComponent::TouchPressed);
	PlayerInputComponent->BindTouch(IE_Repeat,   MobileInput.Get(), &UFormulaMobileInputComponent::TouchMoved);
	PlayerInputComponent->BindTouch(IE_Released, MobileInput.Get(), &UFormulaMobileInputComponent::TouchReleased);
}

void AFormulaVehiclePawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	// We draw our own controls; hide Unreal's default on-screen joysticks (they appear on mobile).
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ActivateTouchInterface(nullptr);
	}
}

void AFormulaVehiclePawn::OnLapDone(int32 LapNumber, float LapTimeSeconds)
{
	if (bIsAI) { return; }
	UE_LOG(LogApexCar, Log, TEXT("LAP %d completed in %.2f s"), LapNumber, LapTimeSeconds);
	if (LapTiming->IsRaceFinished())
	{
		UE_LOG(LogApexCar, Log, TEXT("RACE FINISHED: %d valid laps, best %.2f s"), LapTiming->CompletedLaps, LapTiming->BestLapTime);
	}
}

namespace
{
	float GripYawLimit(const FFormulaCarSetup& Setup, float V, float DownforceN, float TyreScale)
	{
		const float NormalLoad = Setup.MassKg * 9.80665f + DownforceN;
		const float MaxLat = Setup.TyreMu * TyreScale * NormalLoad / Setup.MassKg * 0.97f;
		return MaxLat / FMath::Max(V, 1.f);
	}
}

float AFormulaVehiclePawn::NormalisedToModelSteer(const FFormulaCarSetup& Setup, float Normalised, float SpeedMps, float DownforceN, float TyreScale)
{
	const float V = FMath::Max(SpeedMps, 1.f);
	const float DesiredYaw = FMath::Clamp(Normalised, -1.f, 1.f) * GripYawLimit(Setup, V, DownforceN, TyreScale);
	const float Angle = FMath::Atan(DesiredYaw * Setup.WheelbaseM / V);
	return FMath::Clamp(Angle / FMath::DegreesToRadians(Setup.MaxSteerAngleDeg), -1.f, 1.f);
}

float AFormulaVehiclePawn::ModelSteerToNormalised(const FFormulaCarSetup& Setup, float ModelSteer, float SpeedMps, float DownforceN, float TyreScale)
{
	const float V = FMath::Max(SpeedMps, 1.f);
	const float Angle = FMath::DegreesToRadians(Setup.MaxSteerAngleDeg) * FMath::Clamp(ModelSteer, -1.f, 1.f);
	const float Yaw = (V / Setup.WheelbaseM) * FMath::Tan(Angle);
	return FMath::Clamp(Yaw / FMath::Max(GripYawLimit(Setup, V, DownforceN, TyreScale), 1e-3f), -1.f, 1.f);
}

void AFormulaVehiclePawn::StepDynamics(const FFormulaCarSetup& Setup, float Throttle, float Brake, float Steer,
	float Dt, float& InOutSpeedMps, float& OutYawRateRad, float& OutDownforceN)
{
	const float V = FMath::Max(InOutSpeedMps, 0.f);

	// Aerodynamics: F = 0.5 * rho * v^2 * A * C. Drag opposes motion; downforce adds normal load.
	const float DynamicPressure = 0.5f * Setup.AirDensity * V * V * Setup.FrontalAreaM2;
	const float Drag = DynamicPressure * Setup.DragCoefficient;
	OutDownforceN = DynamicPressure * Setup.LiftCoefficientDown;

	// Longitudinal: power-limited traction, then tyre-limited by (weight + downforce) * mu.
	const float NormalLoad = Setup.MassKg * Gravity + OutDownforceN;
	const float TyreLimit = Setup.TyreMu * NormalLoad;
	const float PowerForce = Setup.PowerW / FMath::Max(V, 5.f);
	const float Traction = FMath::Min(FMath::Min(PowerForce, Setup.MaxTractionForceN), TyreLimit) * FMath::Clamp(Throttle, 0.f, 1.f);
	const float BrakeForce = FMath::Min(Setup.MaxBrakeForceN, TyreLimit) * FMath::Clamp(Brake, 0.f, 1.f);

	const float Net = Traction - BrakeForce - Drag;
	InOutSpeedMps = FMath::Clamp(V + (Net / Setup.MassKg) * Dt, 0.f, Setup.TopSpeedMps);

	// Lateral: bicycle yaw rate, clamped so lateral accel never exceeds mu * normal load / mass.
	const float SteerAngle = FMath::DegreesToRadians(Setup.MaxSteerAngleDeg) * FMath::Clamp(Steer, -1.f, 1.f);
	const float GeometricYawRate = (InOutSpeedMps / Setup.WheelbaseM) * FMath::Tan(SteerAngle);
	const float MaxLatAccel = Setup.TyreMu * NormalLoad / Setup.MassKg;
	const float MaxYawRate = MaxLatAccel / FMath::Max(InOutSpeedMps, 1.f);
	OutYawRateRad = FMath::Clamp(GeometricYawRate, -MaxYawRate, MaxYawRate);
}

void AFormulaVehiclePawn::AdvancePathIndex()
{
	const int32 N = AutoPath.Num();
	if (N < 8) { return; }
	const FVector P = GetActorLocation();
	// Advance to the nearest point within a small forward window (cheap, and never jumps laps).
	float Best = TNumericLimits<float>::Max();
	int32 BestIdx = PathIndex;
	for (int32 k = -3; k <= 12; ++k)
	{
		const int32 i = (PathIndex + k + N) % N;
		const float D = FVector::DistSquared2D(P, AutoPath[i]);
		if (D < Best) { Best = D; BestIdx = i; }
	}
	PathIndex = BestIdx;
}

void AFormulaVehiclePawn::UpdateRaceProgress()
{
	const int32 N = AutoPath.Num();
	if (N < 8) { return; }
	AdvancePathIndex();
	const FVector P = GetActorLocation();
	const FVector A = AutoPath[PathIndex], Z = AutoPath[(PathIndex + N - 1) % N], B = AutoPath[(PathIndex + 1) % N];
	const FVector T = (B - Z).GetSafeNormal2D();
	const float Along = FVector::DotProduct(P - A, T);
	MeasuredLateralCm = FVector::DotProduct(P - A, FVector(-T.Y, T.X, 0.f));
	float Progress = (PathIndex * SegLenCm + Along) / CmPerM;           // metres from the line, 0..LapLength
	Progress = FMath::Fmod(Progress + LapLengthM, LapLengthM);
	UpdateSectors(Progress);
	if (!bProgressInit)
	{
		// Grid slots sit behind the line, so start negative.
		UnwrappedProgressM = (Progress > LapLengthM * 0.5f) ? Progress - LapLengthM : Progress;
		PrevProgressM = Progress;
		bProgressInit = true;
		return;
	}
	float Delta = Progress - PrevProgressM;
	if (Delta < -LapLengthM * 0.5f) { Delta += LapLengthM; }
	else if (Delta > LapLengthM * 0.5f) { Delta -= LapLengthM; }
	UnwrappedProgressM += Delta;
	PrevProgressM = Progress;
}

void AFormulaVehiclePawn::UpdateRacecraft(float Dt)
{
	float Want = 0.f;
	DragScale = 1.f;

	const bool bCloseAhead = GapAheadM < 40.f && GapAheadM > -3.f;
	if (bCloseAhead)
	{
		// Slipstream: sitting in the wake of the car ahead cuts drag (up to 25 % right behind it).
		if (GapAheadM > 2.f && FMath::Abs(MeasuredLateralCm - AheadLateralCm) < 250.f)
		{
			DragScale = FMath::Lerp(0.75f, 1.f, FMath::Clamp(GapAheadM / 40.f, 0.f, 1.f));
		}
		// Overtake when the car ahead is slower than what we could do alone: go round the side it is NOT on.
		if (AheadSpeedMps < LastFreeTargetSpeed - 3.f)
		{
			if (OvertakeSide == 0) { OvertakeSide = (AheadLateralCm > 0.f) ? -1 : 1; }
			Want = OvertakeSide * (300.f + 200.f * Aggression);
		}
	}
	if (!bCloseAhead || GapAheadM < -8.f) { OvertakeSide = 0; }

	// Defend: an aggressive driver moves partway across to the line the car behind is using.
	if (OvertakeSide == 0 && Aggression > 0.6f && GapBehindM < 14.f)
	{
		Want = FMath::Clamp(BehindLateralCm, -200.f, 200.f) * 0.6f;
	}
	// Defend against a PERSON attacking from the next lane too (the same-lane filter cannot see them): cover their line, within the same limits.
	// The autopilot never counts as a person, so demo runs and the AI-only race are unaffected.
	const bool bHumanClose = HumanBehindGapM < 25.f;
	if (OvertakeSide == 0 && bHumanClose && Aggression > 0.35f && !(Aggression > 0.6f && GapBehindM < 14.f))
	{
		Want = FMath::Clamp(HumanBehindLatCm, -200.f, 200.f) * 0.5f;
	}
	if (bHumanClose && Aggression > 0.35f && OvertakeSide == 0)
	{
		if (!bDefendingHuman) { bDefendingHuman = true; ++DefendEvents; UE_LOG(LogApexCar, Log, TEXT("AI DEFEND: driver %d covers the human car %.0f m behind (%.0f cm across)"), DriverIndex, HumanBehindGapM, HumanBehindLatCm); }
	}
	else { bDefendingHuman = false; }
	// Never start a sideways move into a corner: only when the next ~65 m is nearly straight (R > 120 m).
	float KAhead = 0.f;
	for (int32 k = 0; k < 8 && PathCurvature.Num() > 0; ++k) { KAhead = FMath::Max(KAhead, PathCurvature[(PathIndex + k) % PathCurvature.Num()] * CmPerM); }
	if (KAhead > 1.f / 120.f) { Want = 0.f; OvertakeSide = 0; }
	// Grid start: stay in your column until clear of the grid, so the two columns do not merge on the first metres.
	if (GridLateralCm != 0.f && UnwrappedProgressM < 150.f) { Want = GridLateralCm; OvertakeSide = 0; }
	// Side by side: keep ~2.6 m between car centres (cars are 2 m wide and do not collide physically yet).
	if (bHasAlongside)
	{
		const float Side = (MeasuredLateralCm >= AlongsideLatCm) ? 1.f : -1.f;
		Want = AlongsideLatCm + Side * 260.f;
	}
	static const bool bNoOvertake = FParse::Param(FCommandLine::Get(), TEXT("APEXNoOvertake"));   // debug switch
	if (bNoOvertake) { Want = 0.f; OvertakeSide = 0; }
	TargetLateralCm = FMath::Clamp(Want, -400.f, 400.f);        // road half-width is 7 m; car half-width 1 m; keep a margin
	LateralCm = FMath::FInterpConstantTo(LateralCm, TargetLateralCm, Dt, 160.f);

	// Driver mistakes: rarer for consistent drivers. A mistake costs time (a lock-up) for a couple of seconds.
	if (bIsAI)
	{
		MistakeTimer = FMath::Max(MistakeTimer - Dt, 0.f);
		if (MistakeTimer <= 0.f && RaceRand.FRand() < (1.f - Consistency) * Dt * 0.04f) { MistakeTimer = 2.5f; }
	}
}

void AFormulaVehiclePawn::ComputeAutoDrive(float Dt)
{
	const int32 N = AutoPath.Num();
	if (N < 8) { return; }

	AdvancePathIndex();
	UpdateRacecraft(Dt);
	const FVector P = GetActorLocation();
	const float Pace = PaceFactor * WetnessPaceScale * (bUseTyres ? FMath::Sqrt(TyreGripFactor) : 1.f);   // 1.0 for the benchmark car on new mediums in the dry

	auto NormalAt = [&](int32 i) -> FVector
	{
		const FVector T = (AutoPath[(i + 1) % N] - AutoPath[(i + N - 1) % N]).GetSafeNormal2D();
		return FVector(-T.Y, T.X, 0.f);
	};

	// Walk ahead along the path (shifted sideways by the current lateral offset) until the lookahead is covered.
	auto PointAhead = [&](float DistanceCm) -> FVector
	{
		float Walked = 0.f;
		int32 i = PathIndex;
		while (Walked < DistanceCm)
		{
			const int32 j = (i + 1) % N;
			Walked += FVector::Dist2D(AutoPath[i], AutoPath[j]);
			i = j;
		}
		return AutoPath[i] + NormalAt(i) * LateralCm;
	};

	auto AlphaTo = [&](const FVector& Target) -> float
	{
		const FVector2D Fwd(GetActorForwardVector());
		const FVector2D To(Target - P);
		return FMath::Atan2(Fwd.X * To.Y - Fwd.Y * To.X, Fwd.X * To.X + Fwd.Y * To.Y);
	};

	// Pure pursuit steering: delta = atan(2 L sin(alpha) / Ld). Shorter lookahead at low speed so it
	// can follow the hairpin instead of cutting across it.
	const float LookaheadCm = 800.f + SpeedMps * 40.f;
	const float Alpha = AlphaTo(PointAhead(LookaheadCm));
	const float Delta = FMath::Atan2(2.f * Setup.WheelbaseM * FMath::Sin(Alpha), LookaheadCm / CmPerM);
	Steer = FMath::Clamp(Delta / FMath::DegreesToRadians(Setup.MaxSteerAngleDeg), -1.f, 1.f);

	// Speed plan: for every point up to ~300 m ahead, the speed we may carry now so that we can still
	// brake down to that corner's speed: v_now = sqrt(v_corner^2 + 2 * a_brake * distance).
	// Pace scales cornering speed (via lateral accel, speed ~ sqrt(a)) and braking, so a slower car is
	// slower everywhere, not just on the straights.
	const float LateralAccel = 24.f * Pace * Pace;   // m/s^2, below the ~32 the grip model allows: leaves a margin
	const float BrakeDecel = 26.f * Pace;            // m/s^2
	constexpr float PlanHorizonM = 300.f;
	float TargetSpeed = Setup.TopSpeedMps * Pace;
	float DistanceM = 0.f;
	for (int32 k = 0; k < N && DistanceM < PlanHorizonM; ++k)
	{
		const int32 i = (PathIndex + k) % N;
		// An offset line has curvature k / (1 - k * d): tighter on the inside, gentler on the outside.
		const float Ks = PathCurvatureSigned[i];
		const float Denom = FMath::Max(1.f - Ks * LateralCm, 0.3f);
		const float KappaPerM = (FMath::Abs(Ks) / Denom) * CmPerM;
		const float CornerSpeed = FMath::Min(Setup.TopSpeedMps, FMath::Sqrt(LateralAccel / FMath::Max(KappaPerM, 1e-4f)));
		TargetSpeed = FMath::Min(TargetSpeed, FMath::Sqrt(CornerSpeed * CornerSpeed + 2.f * BrakeDecel * DistanceM));
		DistanceM += FVector::Dist2D(AutoPath[i], AutoPath[(i + 1) % N]) / CmPerM;
	}
	LastFreeTargetSpeed = TargetSpeed;

	// Following: in the same lane behind another car, hold a safe gap of 8 m + 0.3 s of speed (centre to centre; a car
	// is 5.9 m long). Applies whether or not the car ahead is slower: tailgating a faster car at 5 m is still tailgating.
	static const bool bNoFollow = FParse::Param(FCommandLine::Get(), TEXT("APEXNoFollow"));       // debug switch
	if (!bNoFollow && GapAheadM < 150.f && GapAheadM > 0.f && FMath::Abs(MeasuredLateralCm - AheadLateralCm) < 220.f)
	{
		const float Safe = 8.f + SpeedMps * 0.30f;
		if (GapAheadM < Safe + 20.f)
		{
			const float Allowed = AheadSpeedMps + (GapAheadM - Safe) * 0.6f;      // below the leader's speed until the gap reopens
			TargetSpeed = FMath::Min(TargetSpeed, FMath::Max(Allowed, AheadSpeedMps * 0.85f));
		}
	}
	if (MistakeTimer > 0.f) { TargetSpeed *= 0.78f; }       // lock-up / run wide

	Throttle = (SpeedMps < TargetSpeed) ? 1.f : 0.f;
	Brake = FMath::Clamp((SpeedMps - TargetSpeed) / 3.f, 0.f, 1.f); // proportional, so it doesn't lock on/off
	EvaluatePit(Dt);
}

void AFormulaVehiclePawn::ConfigureAsAI(int32 InDriverIndex, const FAPEXDriver& Driver, const FAPEXTeam& Team)
{
	bIsAI = true;
	bAutoDrive = true;
	bHoldOnGrid = true;
	DriverIndex = InDriverIndex;
	TeamIndex = Driver.TeamIndex;
	// Pace = team car x driver. The same physics as the player; only the pace target and personality differ.
	PaceFactor = Team.Pace * Driver.Skill;
	Aggression = Driver.Aggression;
	Consistency = Driver.Consistency;
	RaceRand.Initialize(1000 + InDriverIndex);
	Setup.PowerW *= FMath::Lerp(0.97f, 1.f, FMath::Clamp((Team.Pace - 0.97f) / 0.03f, 0.f, 1.f));  // weaker teams: slightly less power
}

void AFormulaVehiclePawn::ApplyDriverLook(const FAPEXTeam& Team, const FAPEXDriver& Driver)
{
	if (!Visual) { return; }
	auto Retint = [this](const TCHAR* SlotName, const FLinearColor& Colour)
	{
		const int32 Idx = Visual->GetMaterialIndex(FName(SlotName));
		if (Idx < 0) { return; }
		if (UMaterialInterface* Current = Visual->GetMaterial(Idx))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Current, this);
			MID->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			Visual->SetMaterial(Idx, MID);
		}
	};
	Retint(TEXT("Paint"), Team.Primary);
	Retint(TEXT("Accent"), Team.Secondary);

	const int32 NumIdx = Visual->GetMaterialIndex(FName(TEXT("DecalNum")));
	if (NumIdx >= 0)
	{
		const FString Path = FString::Printf(TEXT("/Game/Textures/Numbers/T_DecalNum_%d.T_DecalNum_%d"), Driver.Number, Driver.Number);
		if (UTexture* Tex = LoadObject<UTexture>(nullptr, *Path, nullptr, LOAD_NoWarn))
		{
			if (UMaterialInterface* Current = Visual->GetMaterial(NumIdx))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Current, this);
				MID->SetTextureParameterValue(TEXT("DecalTex"), Tex);
				Visual->SetMaterial(NumIdx, MID);
			}
		}
	}
}

void AFormulaVehiclePawn::ApplyCarMaterials()
{
	// Mobile uses simpler permutations (no clear coat / dirt / scratch layers).
	FString Suffix;
#if PLATFORM_IOS
	Suffix = TEXT("_Mobile");
#endif
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXMobileMaterials"))) { Suffix = TEXT("_Mobile"); }
	FString PaintName = TEXT("GlossyRed");
	FParse::Value(FCommandLine::Get(), TEXT("APEXPaint="), PaintName);

	auto Load = [](const FString& Name) -> UMaterialInterface*
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Materials/Car/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn);
	};
	struct FSlot { const TCHAR* Slot; FString Asset; };
	const FSlot BodySlots[] = {
		{ TEXT("Paint"),     FString::Printf(TEXT("MI_APEX_Paint_%s%s"), *PaintName, *Suffix) },
		{ TEXT("Accent"),    FString::Printf(TEXT("MI_APEX_Paint_WhitePearl%s"), *Suffix) },
		{ TEXT("Black"),     FString::Printf(TEXT("MI_APEX_Paint_MatteBlack%s"), *Suffix) },
		{ TEXT("Carbon"),    TEXT("MI_APEX_CarbonFiber") },
		{ TEXT("Metal"),     TEXT("MI_APEX_Metal") },
		{ TEXT("Glass"),     TEXT("MI_APEX_Glass") },
		{ TEXT("DecalNum"),  TEXT("MI_APEX_Decal_Number") },
		{ TEXT("DecalApex"), TEXT("MI_APEX_Decal_Apex") },
	};
	int32 Applied = 0;
	for (const FSlot& S : BodySlots)
	{
		if (UMaterialInterface* M = Load(S.Asset)) { Visual->SetMaterialByName(FName(S.Slot), M); ++Applied; }
	}
	const FSlot WheelSlots[] = {
		{ TEXT("Tyre"),  TEXT("MI_APEX_TireRubber") },
		{ TEXT("Metal"), TEXT("MI_APEX_Metal") },
	};
	for (const FSlot& S : WheelSlots)
	{
		if (UMaterialInterface* M = Load(S.Asset))
		{
			for (UStaticMeshComponent* W : WheelMesh) { if (W) { W->SetMaterialByName(FName(S.Slot), M); } }
			++Applied;
		}
	}
	UE_LOG(LogApexCar, Log, TEXT("Car materials: %d slots set (paint=%s%s)"), Applied, *PaintName, *Suffix);
}

void AFormulaVehiclePawn::UpdateWheels(float Dt)
{
	// Suspension (visual): the body pitches under braking and acceleration, rolls out of corners and squats under downforce, relative to the wheels.
	if (Visual && Dt > 0.f)
	{
		if (!bSuspInit) { bSuspInit = true; VisualBaseZ = Visual->GetRelativeLocation().Z; SuspPrevSpeed = SpeedMps; SuspPrevYaw = GetActorRotation().Yaw; }
		const float Accel = (SpeedMps - SuspPrevSpeed) / Dt;
		const float YawRateRad = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(SuspPrevYaw, GetActorRotation().Yaw)) / Dt;
		SuspPrevSpeed = SpeedMps; SuspPrevYaw = GetActorRotation().Yaw;
		float TP = 0.f, TR = 0.f, TS = 0.f;
		FAPEXDriveMath::SuspensionPose(FMath::Clamp(Accel, -60.f, 40.f), FMath::Clamp(SpeedMps * YawRateRad, -60.f, 60.f), LastDownforceN, TP, TR, TS);
		SuspPitch = FMath::FInterpTo(SuspPitch, TP, Dt, 7.f); SuspRoll = FMath::FInterpTo(SuspRoll, TR, Dt, 7.f); SuspSquat = FMath::FInterpTo(SuspSquat, TS, Dt, 5.f);
		Visual->SetRelativeRotation(FRotator(SuspPitch, 0.f, SuspRoll));
		Visual->SetRelativeLocation(FVector(0.f, 0.f, VisualBaseZ - SuspSquat));
	}
	// Rolling: omega = v / r. In Unreal a negative pitch about Y moves the top of the wheel forward (+X).
	constexpr float WheelRadiusM = 0.35f;
	float SpinRate = FMath::RadiansToDegrees(SpeedMps / WheelRadiusM);
	// Filmed at 60 fps with no motion blur, a wheel turning ~230 deg per frame aliases (looks stopped or backwards). For the cinematic only, the *visual* spin rate is
	// capped to ~16 deg per frame so it always reads as forward rotation. The car's speed is untouched.
	static const bool bCine = FParse::Param(FCommandLine::Get(), TEXT("APEXCinematic")) || FString(FCommandLine::Get()).Contains(TEXT("-APEXCinematic="));
	if (bCine) { SpinRate = FMath::Min(SpinRate, 1000.f); }
	WheelSpinDeg = FMath::Fmod(WheelSpinDeg + SpinRate * Dt, 360.f);
	const float SteerDeg = Steer * Setup.MaxSteerAngleDeg * 0.7f; // visual lock is a bit less than the model's
	for (int32 i = 0; i < 4; ++i)
	{
		if (!WheelMesh[i] || !WheelPivot[i]) { continue; }
		WheelMesh[i]->SetRelativeRotation(FRotator(-WheelSpinDeg, 0.f, 0.f));
		if (i < 2) { WheelPivot[i]->SetRelativeRotation(FRotator(0.f, SteerDeg, 0.f)); }
	}
}

void AFormulaVehiclePawn::ShowDebugText() const
{
	if (!GEngine) { return; }
	GEngine->AddOnScreenDebugMessage(101, 0.f, FColor::White, FString::Printf(
		TEXT("%.0f km/h   downforce %.0f N   lap %d/%d   time %.2f   best %.2f"),
		GetSpeedKph(), LastDownforceN, FMath::Min(LapTiming->CompletedLaps + 1, LapTiming->TargetLaps),
		LapTiming->TargetLaps, LapTiming->CurrentLapTime, LapTiming->BestLapTime));
}

void AFormulaVehiclePawn::SetRaceManager(AAPEXRaceManager* M) { RaceMgr = M; }

void AFormulaVehiclePawn::InitTyresAndStrategy(int32 InTotalLaps, float ForecastWetness, const FAPEXDriver& Driver, int32 InTeamIndex)
{
	TotalLaps = FMath::Max(InTotalLaps, 1);
	TeamIndex = InTeamIndex;
	TrackWetness = ForecastWetness;
	Strategy = UAPEXTyreLibrary::PlanStrategy(TotalLaps, ForecastWetness, Driver);
	Compound = Strategy.StartCompound;
	TyreWear = 0.f;
	UsedCompounds.Reset();
	UsedCompounds.Add(static_cast<uint8>(Compound));
	TyreGripFactor = UAPEXTyreLibrary::GripFactor(Compound, 0.f, TrackWetness);
	DriverIdxForPit = DriverIndex;
}

void AFormulaVehiclePawn::UpdateTyres(float Dt, float LateralAccel)
{
	if (!bUseTyres) { TyreGripFactor = 1.f; return; }
	TyreWear = FMath::Clamp(TyreWear + UAPEXTyreLibrary::WearRate(Compound, LateralAccel, Brake, Throttle, TrackWetness) * Dt, 0.f, 1.f);
	TyreGripFactor = UAPEXTyreLibrary::GripFactor(Compound, TyreWear, TrackWetness);
}

float AFormulaVehiclePawn::PitLimitAt(int32 PathIdx) const
{
	if (!PitPath.IsValidIndex(PathIdx)) { return 1e6f; }
	const float X = PitPath[PathIdx].X;
	return (X >= AFormulaTrack::PitLimitStartXCm && X <= AFormulaTrack::PitLimitEndXCm) ? AFormulaTrack::PitSpeedLimitMps : 1e6f;
}

void AFormulaVehiclePawn::EvaluatePit(float Dt)
{
	if (!bUseTyres || PitPath.Num() < 4 || PitState != EPitState::None || !bAutoDrive || bHoldOnGrid) { return; }

	PitEvalTimer += Dt;
	if (!bPitRequested && PitEvalTimer >= 0.5f)
	{
		PitEvalTimer = 0.f;
		if (UnwrappedProgressM > 400.f)
		{
			const int32 LapsDone = FMath::Clamp(FMath::FloorToInt(UnwrappedProgressM / FMath::Max(LapLengthM, 1.f)), 0, TotalLaps);
			const int32 Planned = Strategy.StopLaps.IsValidIndex(PitStops) ? Strategy.StopLaps[PitStops] : -1;
			if (UAPEXTyreLibrary::ShouldPit(Compound, TyreWear, TrackWetness, LapsDone, TotalLaps, Planned, PitStops))
			{
				// One box per team: if a team-mate is using it, keep circulating and ask again.
				AAPEXRaceManager* M = RaceMgr.Get();
				if (!M || M->TryReserveBox(TeamIndex, this)) { bPitRequested = true; }
			}
		}
	}

	if (bPitRequested)
	{
		const int32 N = AutoPath.Num();
		const int32 Ahead = (PitEntryCentreIdx - PathIndex + N) % N;       // centreline points until the entry
		if (Ahead <= 8) { BeginPitEntry(); }
	}
}

void AFormulaVehiclePawn::BeginPitEntry()
{
	PitState = EPitState::InLane;
	if (LapTiming) { LapTiming->bExternalDrive = true; }
	PitIdx = 0;
	PitLaneElapsed = 0.f;
	PitMaxLimitedSpeed = 0.f;
	const FVector Box = AFormulaTrack::GetPitBoxLocation(TeamIndex);
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < PitPath.Num(); ++i)
	{
		const float D = FVector::DistSquared2D(PitPath[i], Box);
		if (D < BestD) { BestD = D; PitBoxIdx = i; }
	}
	LateralCm = 0.f; TargetLateralCm = 0.f; OvertakeSide = 0;
	UE_LOG(LogApexCar, Log, TEXT("PIT IN driver=%d team=%d lap=%d wear=%.2f %s speed=%.0f m/s"), DriverIndex, TeamIndex,
		FMath::FloorToInt(UnwrappedProgressM / FMath::Max(LapLengthM, 1.f)) + 1, TyreWear, *UAPEXTyreLibrary::Letter(Compound), SpeedMps);
}

void AFormulaVehiclePawn::FinishPitStop()
{
	const int32 LapsDone = FMath::Clamp(FMath::FloorToInt(UnwrappedProgressM / FMath::Max(LapLengthM, 1.f)), 0, TotalLaps);
	const int32 Remaining = TotalLaps - LapsDone;
	const bool bWet = TrackWetness >= 0.35f;
	const bool bTwoUsed = UsedCompounds.Num() >= 2;
	EAPEXTyre Next = (!bWet && Strategy.NextCompounds.IsValidIndex(PitStops)) ? Strategy.NextCompounds[PitStops]
		: UAPEXTyreLibrary::ChooseNextCompound(Remaining, TrackWetness, Compound, bTwoUsed);
	if (!bWet && !bTwoUsed && Next == Compound) { Next = UAPEXTyreLibrary::ChooseNextCompound(Remaining, TrackWetness, Compound, false); }
	UE_LOG(LogApexCar, Log, TEXT("PIT STOP driver=%d lap=%d wear=%.2f %s->%s stationary=%.2fs"), DriverIndex, LapsDone + 1, TyreWear,
		*UAPEXTyreLibrary::Letter(Compound), *UAPEXTyreLibrary::Letter(Next), UAPEXTyreLibrary::PitStopSeconds(UAPEXGridLibrary::GetTeams()[FMath::Clamp(TeamIndex, 0, 9)], DriverIndex));
	Compound = Next;
	TyreWear = 0.f;
	UsedCompounds.Add(static_cast<uint8>(Compound));
	TyreGripFactor = UAPEXTyreLibrary::GripFactor(Compound, 0.f, TrackWetness);
	++PitStops;
	PitState = EPitState::Exiting;
}

void AFormulaVehiclePawn::LeavePitLane()
{
	PitState = EPitState::None;
	if (LapTiming) { LapTiming->bExternalDrive = false; }
	bPitRequested = false;
	// Re-synchronise with the racing line from wherever the lane ended.
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i < AutoPath.Num(); ++i)
	{
		const float D = FVector::DistSquared2D(GetActorLocation(), AutoPath[i]);
		if (D < Best) { Best = D; PathIndex = i; }
	}
	LateralCm = 0.f; TargetLateralCm = 0.f;
	if (AAPEXRaceManager* M = RaceMgr.Get()) { M->ReleaseBox(TeamIndex, this); }
	UE_LOG(LogApexCar, Log, TEXT("PIT OUT driver=%d laneTime=%.1fs maxSpeedInLimit=%.1f m/s (limit %.0f) now on %s"), DriverIndex, PitLaneElapsed,
		PitMaxLimitedSpeed, AFormulaTrack::PitSpeedLimitMps, *UAPEXTyreLibrary::Letter(Compound));
}

void AFormulaVehiclePawn::ComputePitDrive(float Dt)
{
	const int32 M = PitPath.Num();
	if (M < 4) { PitState = EPitState::None; return; }
	const FVector P = GetActorLocation();

	// Lap timing in the lane: the gates lie on the racing line, so feed the line's projection of the car's position.
	if (LapTiming && AutoPath.Num() > 0)
	{
		const int32 NA = AutoPath.Num();
		float BestD = TNumericLimits<float>::Max();
		int32 BestK = PathIndex;
		for (int32 k = -3; k <= 14; ++k)
		{
			const int32 i = ((PathIndex + k) % NA + NA) % NA;
			const float D = FVector::DistSquared2D(P, AutoPath[i]);
			if (D < BestD) { BestD = D; BestK = i; }
		}
		PathIndex = BestK;
		LapTiming->UpdatePosition(AutoPath[PathIndex], Dt);
	}

	if (PitState == EPitState::Stopped)
	{
		Throttle = 0.f; Brake = 1.f; Steer = 0.f; SpeedMps = 0.f;
		PitLaneElapsed += Dt;
		PitTimer -= Dt;
		if (PitTimer <= 0.f) { FinishPitStop(); }
		return;
	}
	PitLaneElapsed += Dt;

	// Follow the pit path index (window search, like the racing line).
	float Best = TNumericLimits<float>::Max();
	int32 BI = PitIdx;
	for (int32 k = -2; k <= 10; ++k)
	{
		const int32 i = FMath::Clamp(PitIdx + k, 0, M - 1);
		const float D = FVector::DistSquared2D(P, PitPath[i]);
		if (D < Best) { Best = D; BI = i; }
	}
	PitIdx = BI;
	if (PitIdx >= M - 3) { LeavePitLane(); return; }

	// Working-lane offset: a car that is stopping eases over towards the garages before its box and back after it, so a
	// stopped car never blocks the fast lane. (dx = X distance past the box centre.)
	const float BoxX = PitPath[FMath::Clamp(PitBoxIdx, 0, M - 1)].X;
	auto WorkOffset = [&](float X)
	{
		const float dx = X - BoxX;
		const float In = FMath::SmoothStep(-1800.f, -300.f, dx);
		const float Out = FMath::SmoothStep(300.f, 1800.f, dx);
		return AFormulaTrack::PitWorkLaneOffsetCm * (In - Out);
	};

	// Pure pursuit along the lane.
	const float LookaheadCm = 500.f + SpeedMps * 30.f;
	float Walked = 0.f; int32 T = PitIdx;
	while (Walked < LookaheadCm && T < M - 1) { Walked += FVector::Dist2D(PitPath[T], PitPath[T + 1]); ++T; }
	const FVector2D Fwd(GetActorForwardVector());
	const FVector2D To(FVector2D(PitPath[T]) + FVector2D(0.f, WorkOffset(PitPath[T].X)) - FVector2D(P));
	const float Alpha = FMath::Atan2(Fwd.X * To.Y - Fwd.Y * To.X, Fwd.X * To.X + Fwd.Y * To.Y);
	const float Delta = FMath::Atan2(2.f * Setup.WheelbaseM * FMath::Sin(Alpha), LookaheadCm / CmPerM);
	Steer = FMath::Clamp(Delta / FMath::DegreesToRadians(Setup.MaxSteerAngleDeg), -1.f, 1.f);

	// Speed plan: brake down to the lane limit before the limited zone, and to a stop at the box.
	const float Pace = PaceFactor * WetnessPaceScale * (bUseTyres ? FMath::Sqrt(TyreGripFactor) : 1.f);
	float Target = Setup.TopSpeedMps * Pace;
	float Dist = 0.f;
	for (int32 i = PitIdx; i < M - 1 && Dist < 400.f; ++i)
	{
		Dist += FVector::Dist2D(PitPath[i], PitPath[i + 1]) / CmPerM;
		const float L = PitLimitAt(i + 1);
		if (L < 1e5f) { const float LM = L - 1.5f; Target = FMath::Min(Target, FMath::Sqrt(LM * LM + 2.f * 11.f * Dist)); }
	}
	const float LimitHere = PitLimitAt(PitIdx);
	if (LimitHere < 1e5f)
	{
		Target = FMath::Min(Target, LimitHere);
		if (SpeedMps > LimitHere) { SpeedMps = FMath::FInterpConstantTo(SpeedMps, LimitHere, Dt, 30.f); }   // the speed limiter
		PitMaxLimitedSpeed = FMath::Max(PitMaxLimitedSpeed, SpeedMps);
	}
	if (PitState == EPitState::InLane)
	{
		// Signed distance along the lane to the box (positive = still ahead), not quantised to the 3 m path spacing.
		const FVector Tan = (PitPath[FMath::Min(PitBoxIdx + 1, M - 1)] - PitPath[FMath::Max(PitBoxIdx - 1, 0)]).GetSafeNormal2D();
		const float ToBox = FVector::DotProduct(PitPath[PitBoxIdx] - P, Tan) / CmPerM;
		Target = FMath::Min(Target, FMath::Sqrt(2.f * 12.f * FMath::Max(ToBox - 0.3f, 0.f)) + 0.5f);
		if ((ToBox < 0.6f && SpeedMps < 3.f) || ToBox < -0.5f)
		{
			PitState = EPitState::Stopped;
			const TArray<FAPEXTeam> Teams = UAPEXGridLibrary::GetTeams();
			PitTimer = UAPEXTyreLibrary::PitStopSeconds(Teams[FMath::Clamp(TeamIndex, 0, Teams.Num() - 1)], DriverIndex);
			SetActorLocation(FVector(PitPath[PitBoxIdx].X, PitPath[PitBoxIdx].Y + AFormulaTrack::PitWorkLaneOffsetCm, GetActorLocation().Z));   // settle onto the box marks
			SpeedMps = 0.f; Throttle = 0.f; Brake = 1.f;
			return;
		}
	}
	Throttle = (SpeedMps < Target - 0.4f) ? 1.f : 0.f;
	Brake = FMath::Clamp((SpeedMps - Target) / 1.5f, 0.f, 1.f);
}

void AFormulaVehiclePawn::UpdateSectors(float ProgressM)
{
	const int32 Sec = FMath::Clamp(static_cast<int32>(ProgressM / FMath::Max(LapLengthM / 3.f, 1.f)), 0, 2);
	if (CurrentSector < 0) { CurrentSector = Sec; SectorClock = 0.f; bSectorClean = false; return; }   // spawned mid-lap: the first sector is not a clean split
	if (Sec == CurrentSector) { return; }
	if (Sec == (CurrentSector + 1) % 3)
	{
		if (bSectorClean && SectorClock > 5.f)
		{
			LastSector[CurrentSector] = SectorClock;
			if (BestSector[CurrentSector] <= 0.f || SectorClock < BestSector[CurrentSector]) { BestSector[CurrentSector] = SectorClock; }
		}
		bSectorClean = true;
	}
	else { bSectorClean = false; }          // went backwards (a reset) or skipped: not a valid split
	CurrentSector = Sec;
	SectorClock = 0.f;
}

void AFormulaVehiclePawn::SetDrivingMode(EAPEXDrivingMode Mode)
{
	DrivingMode = Mode;
	if (bIsAI) { return; }
	bAutoDrive = (Mode == EAPEXDrivingMode::AutonomousDemo);
	if (!bAutoDrive) { Throttle = 0.f; Brake = 0.f; Steer = 0.f; }
	UE_LOG(LogApexCar, Log, TEXT("Driving mode: %d (0=Manual 1=Assisted 2=AutonomousDemo) autoDrive=%d"), static_cast<int32>(Mode), bAutoDrive ? 1 : 0);
}

bool AFormulaVehiclePawn::SetDriverInputs(float InThrottle, float InBrake, float InSteer)
{
	if (!FAPEXDriveMath::CanAcceptDriverInput(bIsAI, DrivingMode, bHoldOnGrid)) { return false; }
	Throttle = FMath::Clamp(InThrottle, 0.f, 1.f);
	Brake = FMath::Clamp(InBrake, 0.f, 1.f);
	Steer = FMath::Clamp(InSteer, -1.f, 1.f);
	return true;
}

void AFormulaVehiclePawn::SetSteerPreset(EAPEXSteerPreset Preset)
{
	SteerPreset = Preset;
	const FAPEXSteerTuning T = FAPEXDriveMath::Tuning(Preset);
	bManualGearbox = T.bManualGearbox;
	bAssistsOn = T.bAssists;
}

void AFormulaVehiclePawn::ShiftUp()   { if (bManualGearbox) { Gear = FMath::Clamp(Gear + 1, 1, 8); } }
void AFormulaVehiclePawn::ShiftDown() { if (bManualGearbox) { Gear = FMath::Clamp(Gear - 1, 1, 8); } }

void AFormulaVehiclePawn::ComputeEffectiveInputs(float Dt)
{
	PowerScaleNow = 1.f; OvertakeDragScale = 1.f; YawBoost = 1.f; bBoostActive = false;
	if (bIsAI || bAutoDrive)
	{
		EffThrottle = Throttle; EffBrake = Brake; EffSteer = Steer;
		bOvertakeAvailable = bOvertakeActive = false;
		Gear = FAPEXDriveMath::GearForSpeed(SpeedMps, Gear);
		return;
	}
	float Thr = Throttle, Brk = Brake, Str = Steer;

	// Gearbox. Automatic follows road speed; manual enforces the rev limiter and bogs when lugging in a tall gear.
	if (!bManualGearbox) { Gear = FAPEXDriveMath::GearForSpeed(SpeedMps, Gear); }
	else
	{
		const float Top = FAPEXDriveMath::GearTopSpeedMps(Gear);
		const float Lo = Gear > 1 ? FAPEXDriveMath::GearTopSpeedMps(Gear - 1) : 0.f;
		if (SpeedMps > Top) { Thr = 0.f; }                                  // limiter
		else if (Gear > 1 && SpeedMps < Lo * 0.55f) { Thr *= (SpeedMps < 2.f ? 0.15f : 0.4f); }   // lugging / pulling away in a tall gear
	}

	// Assists (Assisted mode, or a preset that includes them). Manual with a non-assist preset leaves the commands untouched.
	if (AreAssistsOn())
	{
		if (Brk > 0.9f && FMath::Abs(Str) > 0.25f) { Brk = 0.9f; }              // ABS: no lock-up while turning
		if (SpeedMps < 45.f) { Thr *= 1.f - 0.45f * FMath::Abs(Str); }          // traction control in slow corners
		if (SpeedMps > 70.f) { Str *= 0.9f; }                                   // stability: calmer steering at speed
	}

	// Handbrake (arcade preset only): scrubs speed and tightens the turn-in so the tail steps out.
	if (bHandbrakeHeld && FAPEXDriveMath::Tuning(SteerPreset).bHandbrake) { Brk = FMath::Max(Brk, 0.4f); YawBoost = 1.4f; }

	// Energy deployment: +16 % power while held, drains 7.5 %/s; refills slowly and faster under braking.
	if (bBoostHeld && Energy > 0.02f && Thr > 0.4f) { PowerScaleNow = 1.16f; Energy -= 0.075f * Dt; bBoostActive = true; }
	else { Energy += (0.010f + 0.05f * Brk) * Dt; }
	Energy = FMath::Clamp(Energy, 0.f, 1.f);

	// Overtaking aid: available within ~1 s of the car ahead, at speed, on a straight. Cuts drag by 22 % while held.
	bOvertakeAvailable = GapAheadM < 100.f && SpeedMps > 50.f && FMath::Abs(Str) < 0.25f;
	bOvertakeActive = bOvertakeHeld && bOvertakeAvailable;
	if (bOvertakeActive) { OvertakeDragScale = 0.78f; }

	EffThrottle = Thr; EffBrake = Brk;
	EffSteer = NormalisedToModelSteer(Setup, Str, SpeedMps, LastDownforceN, bUseTyres ? TyreGripFactor : 1.f);   // player steering law (see header)
}

void AFormulaVehiclePawn::ResolveCarContacts(float Dt)
{
	ContactCooldown = FMath::Max(0.f, ContactCooldown - Dt);
	OtherCarsAge += Dt;
	if (OtherCarsAge > 0.5f)
	{
		OtherCarsAge = 0.f; OtherCars.Reset();
		for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It) { if (*It != this) { OtherCars.Add(*It); } }
	}
	const FVector Fwd = GetActorForwardVector(), Right = GetActorRightVector();
	const float HalfLen = 290.f, HalfWid = 95.f;                 // 5.8 m x 1.9 m footprints
	for (const TWeakObjectPtr<AFormulaVehiclePawn>& W : OtherCars)
	{
		AFormulaVehiclePawn* O = W.Get();
		if (!O) { continue; }
		const FVector Rel = O->GetActorLocation() - GetActorLocation();
		if (Rel.SizeSquared2D() > FMath::Square(900.f)) { continue; }
		const float Dx = FVector::DotProduct(Rel, Fwd), Dy = FVector::DotProduct(Rel, Right);
		const float OverX = 2.f * HalfLen - FMath::Abs(Dx), OverY = 2.f * HalfWid - FMath::Abs(Dy);
		if (OverX <= 0.f || OverY <= 0.f) { continue; }
		if (OverX / (2.f * HalfLen) < OverY / (2.f * HalfWid))
		{
			// nose-to-tail: share momentum (the faster car loses speed, the slower gains), then separate along the car
			const float Mean = 0.5f * (SpeedMps + O->SpeedMps) * 0.985f;
			SpeedMps = Mean; O->ReceiveContact(Mean, 0.f);
			AddActorWorldOffset(-Fwd * FMath::Sign(Dx) * OverX * 0.5f, true);
			O->AddActorWorldOffset(Fwd * FMath::Sign(Dx) * OverX * 0.5f, true);
		}
		else
		{
			// side-by-side: shove apart, a little speed lost on both
			SpeedMps *= 0.985f;
			O->ReceiveContact(O->SpeedMps * 0.985f, FMath::Sign(Dy) * OverY * 0.5f);
			AddActorWorldOffset(-Right * FMath::Sign(Dy) * OverY * 0.5f, true);
		}
		if (ContactCooldown <= 0.f)
		{
			ContactCooldown = 0.4f;
			if (AAPEXPlayerController* PC = Cast<AAPEXPlayerController>(GetController())) { PC->Rumble(0.6f, 0.2f); }
			UE_LOG(LogApexCar, Log, TEXT("CONTACT: player vs driver %d at %.0f km/h"), O->GetDriverIndex(), GetSpeedKph());
		}
	}
}

void AFormulaVehiclePawn::ReceiveContact(float NewSpeedMps, float LateralShoveCm)
{
	SpeedMps = FMath::Max(0.f, NewSpeedMps);
	if (!FMath::IsNearlyZero(LateralShoveCm)) { AddActorWorldOffset(GetActorRightVector() * LateralShoveCm, true); }
}

int32 AFormulaVehiclePawn::ResetToTrack()
{
	const int32 N = AutoPath.Num();
	if (N < 8) { return INDEX_NONE; }
	const FVector P = GetActorLocation();
	int32 Best = 0; float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < N; ++i) { const float D = FVector::DistSquared2D(AutoPath[i], P); if (D < BestD) { BestD = D; Best = i; } }
	int32 Idx = Best, Tries = 0;
	for (; Tries < 24; ++Tries)
	{
		bool bClear = true;
		for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
		{
			if (*It != this && FVector::DistSquared2D(It->GetActorLocation(), AutoPath[Idx]) < FMath::Square(1200.f)) { bClear = false; break; }
		}
		if (bClear) { break; }
		Idx = (Idx + N - 8) % N;                          // step back along the lap (~60 m) until the spot is free
	}
	const FVector Tan = (AutoPath[(Idx + 1) % N] - AutoPath[Idx]).GetSafeNormal2D();
	SetActorLocationAndRotation(FVector(AutoPath[Idx].X, AutoPath[Idx].Y, GetActorLocation().Z), Tan.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	SpeedMps = 0.f; Throttle = 0.f; Brake = 1.f; Steer = 0.f; StuckTimer = 0.f; LateralCm = 0.f; TargetLateralCm = 0.f;
	UE_LOG(LogApexCar, Log, TEXT("RESET: car placed on the centreline at path index %d after %d blocked candidates"), Idx, Tries);
	return Idx;
}

void AFormulaVehiclePawn::GetAutopilotSuggestion(float Dt, float& OutThrottle, float& OutBrake, float& OutSteer)
{
	const float T0 = Throttle, B0 = Brake, S0 = Steer;
	ComputeAutoDrive(Dt);
	OutThrottle = Throttle; OutBrake = Brake; OutSteer = Steer;
	Throttle = T0; Brake = B0; Steer = S0;
}

UCameraComponent* AFormulaVehiclePawn::ActiveCameraComponent() const
{
	switch (CameraView)
	{
	case EAPEXCamView::Cockpit:   return CockpitCamera;
	case EAPEXCamView::FrontWing: return FrontWingCamera;
	case EAPEXCamView::Nose:      return NoseCamera;
	case EAPEXCamView::Rear:      return RearCamera;
	default:                      return ChaseCamera;
	}
}

void AFormulaVehiclePawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (UCameraComponent* C = ActiveCameraComponent()) { C->GetCameraView(DeltaTime, OutResult); return; }
	Super::CalcCamera(DeltaTime, OutResult);
}

void AFormulaVehiclePawn::RecoverToTrack()
{
	const int32 N = AutoPath.Num();
	if (N < 8) { return; }
	const int32 Idx = (PathIndex + 2) % N;
	const FVector Tan = (AutoPath[(Idx + 1) % N] - AutoPath[Idx]).GetSafeNormal2D();
	SetActorLocationAndRotation(AutoPath[Idx] + FVector(0.f, 0.f, GetActorLocation().Z), Tan.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	SpeedMps = 8.f; LateralCm = 0.f; TargetLateralCm = 0.f; OvertakeSide = 0; StuckTimer = 0.f; MistakeTimer = 0.f;
	UE_LOG(LogApexCar, Log, TEXT("RECOVERED driver=%d ai=%d pathIdx=%d measuredLateral=%.0f cm"), DriverIndex, bIsAI ? 1 : 0, PathIndex, MeasuredLateralCm);
}

void AFormulaVehiclePawn::SimStep(float Dt)
{
	SectorClock += Dt;
	ComputeEffectiveInputs(Dt);
	FFormulaCarSetup Eff = Setup;
	Eff.DragCoefficient *= DragScale * OvertakeDragScale;   // slipstream (1.0 when alone) x overtaking aid
	Eff.PowerW *= PowerScaleNow;                             // energy deployment
	if (bUseTyres) { Eff.TyreMu *= TyreGripFactor; }   // compound x weather x wear
	float YawRate = 0.f;
	StepDynamics(Eff, EffThrottle, EffBrake, EffSteer, Dt, SpeedMps, YawRate, LastDownforceN);
	YawRate *= YawBoost;
	UpdateTyres(Dt, SpeedMps * FMath::Abs(YawRate));

	// Hard no-pass-through: cars do not collide physically yet, so a same-lane follower may never close inside its
	// own length (5.9 m) plus a margin on the car ahead; it takes the leader's speed instead. The gap data is a 10 Hz
	// snapshot, so allow for the distance closed since then.
	if (bAutoDrive && GapAheadM > 0.f && FMath::Abs(MeasuredLateralCm - AheadLateralCm) < 190.f)
	{
		const float Closing = FMath::Max(SpeedMps - AheadSpeedMps, 0.f);
		if (GapAheadM - Closing * 0.1f < 6.4f) { SpeedMps = FMath::Min(SpeedMps, AheadSpeedMps); }
	}

	SetActorRotation(GetActorRotation() + FRotator(0.f, FMath::RadiansToDegrees(YawRate) * Dt, 0.f));

	const FVector Delta = GetActorForwardVector() * SpeedMps * CmPerM * Dt;
	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		// Wall contact: bleed speed and slide along the surface.
		SpeedMps *= 0.4f;
		AddActorWorldOffset(FVector::VectorPlaneProject(Delta, Hit.ImpactNormal) * (1.f - Hit.Time), true);
		if (!bIsAI) { if (AAPEXPlayerController* PC = Cast<AAPEXPlayerController>(GetController())) { PC->Rumble(FMath::Clamp(SpeedMps / 70.f, 0.25f, 1.f), 0.25f); } }
	}

	// Stay on the ground: trace down from above and snap.
	FHitResult Ground;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 200.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FormulaGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Ground, Start, Start - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Params))
	{
		FVector L = GetActorLocation();
		L.Z = Ground.ImpactPoint.Z + Body->GetScaledBoxExtent().Z;
		SetActorLocation(L);
	}

	UpdateRaceProgress();
	if (!bIsAI && !bAutoDrive) { ResolveCarContacts(Dt); }

	// Auto-driven car pinned against something (a spin, a wall): put it back on the racing line.
	if (bAutoDrive && !bHoldOnGrid && !bHeroMode && PitState == EPitState::None)
	{
		StuckTimer = (SpeedMps < 2.f) ? StuckTimer + Dt : 0.f;
		if (StuckTimer > 3.f) { RecoverToTrack(); }
	}
}

void AFormulaVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsAI)
	{
		// Competitors: fixed 60 Hz sub-steps so the result does not depend on how often the manager lets them tick.
		float Remaining = FMath::Min(DeltaSeconds, 0.25f);
		while (Remaining > 1e-4f)
		{
			const float Step = FMath::Min(Remaining, 1.f / 60.f);
			if (bHoldOnGrid) { Throttle = 0.f; Brake = 1.f; Steer = 0.f; SpeedMps = 0.f; }
			else if (PitState != EPitState::None) { ComputePitDrive(Step); }
			else { ComputeAutoDrive(Step); }
			SimStep(Step);
			Remaining -= Step;
		}
		if (bVisualsActive) { UpdateWheels(DeltaSeconds); }
		return;
	}

	FrameAccum += DeltaSeconds; ++FrameCount; FrameMax = FMath::Max(FrameMax, DeltaSeconds);

	// Clamp dt so a hitch on a phone cannot blow up the integration.
	const float Dt = FMath::Min(DeltaSeconds, 1.f / 20.f);

	if (bAutoDrive)
	{
		if (PitState != EPitState::None) { ComputePitDrive(Dt); } else { ComputeAutoDrive(Dt); }
		if (bHeroMode) { Throttle = 0.f; Brake = 1.f; Steer = 0.f; }

		// Headless verification: log progress, then quit when the race ends (or after a safety limit).
		AutoDriveElapsed += Dt;
		if (AutoDriveElapsed >= NextAutoLogTime)
		{
			NextAutoLogTime += 10.f;
			UE_LOG(LogApexCar, Log, TEXT("t=%.0fs speed=%.0f km/h pos=(%.0f, %.0f) pathIdx=%d laps=%d frame avg=%.1f ms (%.0f fps) max=%.1f ms"),
				AutoDriveElapsed, GetSpeedKph(), GetActorLocation().X, GetActorLocation().Y, PathIndex, LapTiming->CompletedLaps,
				FrameCount ? 1000.0 * FrameAccum / FrameCount : 0.0, FrameAccum > 0.0 ? FrameCount / FrameAccum : 0.0, FrameMax * 1000.f);
			FrameAccum = 0.0; FrameCount = 0; FrameMax = 0.f;
		}
		// Debug: -APEXShot captures one screenshot ~12 s into the auto-drive, for checking the HUD visually.
		float ShotTime = 12.f;
		FParse::Value(FCommandLine::Get(), TEXT("APEXShotTime="), ShotTime);
		if (!bShotTaken && AutoDriveElapsed >= ShotTime && FParse::Param(FCommandLine::Get(), TEXT("APEXShot")))
		{
			bShotTaken = true;
			if (APlayerController* ShotPC = Cast<APlayerController>(GetController()))
			{
				ShotPC->ConsoleCommand(TEXT("HighResShot 1920x1080"));
			}
		}
		if (LapTiming->IsRaceFinished()) { FinishedElapsed += Dt; }
		if ((LapTiming->IsRaceFinished() && FinishedElapsed > 1.5f) || AutoDriveElapsed > (FParse::Param(FCommandLine::Get(), TEXT("APEXShot")) ? ShotTime + 8.f : 600.f))
		{
			UE_LOG(LogApexCar, Log, TEXT("AUTODRIVE END: laps=%d finished=%d elapsed=%.1fs"),
				LapTiming->CompletedLaps, LapTiming->IsRaceFinished() ? 1 : 0, AutoDriveElapsed);
			FPlatformMisc::RequestExit(false);
		}
	}
	else if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// Keyboard and gamepad no longer polled here: AAPEXPlayerController (Enhanced Input) is the single writer, through SetDriverInputs().

		// Touch: layout follows the viewport and safe area; tilt comes from the motion sensors.
		int32 VW = 0, VH = 0;
		PC->GetViewportSize(VW, VH);
		if (VW > 0 && VH > 0)
		{
			MobileInput->SetViewport(FVector2D(VW, VH), GetSafeMargins());
		}

		float TiltNormalised = 0.f;
		if (MobileInput->Style == EFormulaSteerStyle::Tilt)
		{
			// UNVERIFIED on device: which gravity axis is "roll" in landscape must be checked on an iPhone.
			FVector Tilt, RotationRate, GravityVec, Accel;
			PC->GetInputMotionState(Tilt, RotationRate, GravityVec, Accel);
			const float MaxSin = FMath::Sin(FMath::DegreesToRadians(MobileInput->TiltMaxAngleDeg));
			TiltNormalised = FMath::Clamp(static_cast<float>(GravityVec.Y) / FMath::Max(MaxSin, KINDA_SMALL_NUMBER), -1.f, 1.f);
		}
		MobileInput->UpdateInputs(Dt, TiltNormalised);

		// On iPhone the touch controls always drive. Elsewhere keyboard drives unless a finger is down.
#if PLATFORM_IOS
		const bool bUseTouch = true;
#else
		const bool bUseTouch = MobileInput->HasActiveTouch();
#endif
		if (bUseTouch)
		{
			Throttle = MobileInput->GetThrottle();
			Brake = MobileInput->GetBrake();
			Steer = MobileInput->GetSteer();
		}
		// else: the controller's last SetDriverInputs() values stand
	}

	if (bHoldOnGrid) { Throttle = 0.f; Brake = 1.f; Steer = 0.f; }
	SimStep(Dt);
	if (!bIsAI && ChaseArm && !bHeroMode)        // the chase camera leans into the corner and settles back
	{
		FVector SO = ChaseArm->SocketOffset;
		SO.Y = FMath::FInterpTo(SO.Y, -EffSteer * 45.f, Dt, 4.f);
		SO.Z = FMath::FInterpTo(SO.Z, 60.f - EffBrake * 12.f + EffThrottle * 6.f, Dt, 3.f);
		ChaseArm->SocketOffset = SO;
	}

	UpdateWheels(Dt);
	ShowDebugText();
}

void AFormulaVehiclePawn::RequestPitNow()
{
	AAPEXRaceManager* M = nullptr;
	{ TActorIterator<AAPEXRaceManager> It(GetWorld()); if (It) { M = *It; } }
	if (!bPitRequested && PitState == EPitState::None && (!M || M->TryReserveBox(TeamIndex, this))) { bPitRequested = true; }
}
