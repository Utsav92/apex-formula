#pragma once

#include "CoreMinimal.h"
#include "APEXDrivingTypes.generated.h"

/** Who is allowed to write the player car's driving commands. Manual is the default; AutonomousDemo is opt-in only. */
UENUM(BlueprintType)
enum class EAPEXDrivingMode : uint8
{
	Manual,           // the player drives; nothing else writes throttle, brake or steering
	Assisted,         // the player drives; ABS / traction / stability assists trim the commands
	AutonomousDemo    // the game drives the car (spectator / demo / the headless test runs)
};

UENUM(BlueprintType)
enum class EAPEXSteerPreset : uint8 { Arcade, Sport, Simulation };

/** Gameplay camera views (C cycles them, R shows Rear while held). Cinematic hands the view to the broadcast director. */
UENUM(BlueprintType)
enum class EAPEXCamView : uint8 { Chase, Cockpit, FrontWing, Rear, Nose, Cinematic };

/** Race flow as the player experiences it (derived from the race manager, pause and replay state). */
UENUM(BlueprintType)
enum class EAPEXRaceFlow : uint8 { Menu, Loading, Grid, Countdown, Racing, Paused, Finished, Replay };

USTRUCT(BlueprintType)
struct FAPEXSteerTuning
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float KeyRise = 3.0f;          // steering units per second toward the pressed key
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float KeyFall = 5.0f;          // ... back to centre when released
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Sensitivity = 1.0f;      // overall scale (gamepad too)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinSpeedScale = 0.30f;   // steering authority left at 90 m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAssists = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bManualGearbox = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHandbrake = false;       // arcade only
};

/** Pure driving helpers (no world, no actor): used by the controller, the pawn and the automation tests. */
struct APEXFORMULA_API FAPEXDriveMath
{
	static FAPEXSteerTuning Tuning(EAPEXSteerPreset Preset);
	/** Digital steering: move Current toward Target at the rise rate when growing, the fall rate when shrinking or reversing. */
	static float KeyboardSteerStep(float Current, float Target, float Dt, float Rise, float Fall);
	/** Steering authority 1.0 at low speed falling smoothly to MinScale at 90 m/s. */
	static float SpeedSteerScale(float SpeedMps, float MinScale);
	static float ApplyDeadZone(float Value, float DeadZone);
	/** -1..1 from two opposing digital keys (both held cancel out). */
	static float AxisFromKeys(bool bPositive, bool bNegative) { return (bPositive ? 1.f : 0.f) - (bNegative ? 1.f : 0.f); }
	/** Gear (1-8) a road speed asks for, with hysteresis around the shift points when CurrentGear is already near. */
	static int32 GearForSpeed(float SpeedMps, int32 CurrentGear);
	static float GearTopSpeedMps(int32 Gear);
	/** 0..1 engine speed within the gear (0.3 right after a shift up to 1.0 at the limiter). */
	static float Rev01(float SpeedMps, int32 Gear);
	/** Body attitude for visual suspension: Pitch (deg, + = nose up) from longitudinal accel (m/s^2, braking is negative), Roll (deg, + = right side down) from lateral accel (+ = turning right, the body leans out to the left so roll is negative), Squat (cm) from downforce (N). */
	static void SuspensionPose(float LongAccel, float LatAccel, float DownforceN, float& OutPitch, float& OutRoll, float& OutSquat);
	/** Driver-command ownership: only a human-controlled, non-demo, released car accepts commands from the player controller. */
	static bool CanAcceptDriverInput(bool bIsAI, EAPEXDrivingMode Mode, bool bHeldOnGrid) { return !bIsAI && Mode != EAPEXDrivingMode::AutonomousDemo && !bHeldOnGrid; }
};
