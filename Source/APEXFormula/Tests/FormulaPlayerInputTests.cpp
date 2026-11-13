#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Input/APEXInputConfig.h"
#include "Input/APEXPlayerController.h"
#include "Camera/APEXCameraManager.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Vehicles/APEXDrivingTypes.h"
#include "Core/APEXFormulaGameMode.h"
#include "InputMappingContext.h"
#include "InputAction.h"

namespace { constexpr EAutomationTestFlags PIFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexInputMappingTest, "APEX.Input.MappingContextMatchesSpecification", PIFlags)
bool FApexInputMappingTest::RunTest(const FString&)
{
	UAPEXInputConfig* C = NewObject<UAPEXInputConfig>(GetTransientPackage());
	C->Build();
	TArray<FString> Problems;
	const bool bOk = C->Validate(Problems);
	for (const FString& P : Problems) { AddError(P); }
	TestTrue(TEXT("every required action is mapped to its keys"), bOk);
	TestEqual(TEXT("IMC name"), C->IMC_APEX_Driving->GetFName().ToString(), FString(TEXT("IMC_APEX_Driving")));
	TestEqual(TEXT("steering sums opposing keys instead of picking one"), C->IA_Steer->AccumulationBehavior, EInputActionAccumulationBehavior::Cumulative);
	TestTrue(TEXT("pause works while paused"), C->IA_Pause->bTriggerWhenPaused);
	TestTrue(TEXT("controls page has text"), C->DescribeControls().Num() >= 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexRebindTest, "APEX.Input.RebindingKeepsMappingConsistent", PIFlags)
bool FApexRebindTest::RunTest(const FString&)
{
	UAPEXInputConfig* C = NewObject<UAPEXInputConfig>(GetTransientPackage());
	C->Build();
	FString Err;
	TestTrue(TEXT("rebind Throttle to I"), C->Rebind(FName(TEXT("Throttle")), EKeys::I, Err));
	const TArray<FKey> Keys = C->KeysFor(C->IA_Throttle);
	TestTrue(TEXT("new key is mapped"), Keys.Contains(EKeys::I));
	TestFalse(TEXT("old key is unmapped"), Keys.Contains(EKeys::W));
	TestTrue(TEXT("the arrow-key alternate stays"), Keys.Contains(EKeys::Up));
	TArray<FString> Problems;
	TestTrue(TEXT("the mapping still validates after rebinding"), C->Validate(Problems));
	TestFalse(TEXT("a key already in use is refused"), C->Rebind(FName(TEXT("Brake")), EKeys::I, Err));
	TestFalse(TEXT("reserved keys are refused"), C->Rebind(FName(TEXT("Brake")), EKeys::Escape, Err));
	TestFalse(TEXT("the AnyKey pseudo-key is refused"), C->Rebind(FName(TEXT("Brake")), EKeys::AnyKey, Err));
	TestFalse(TEXT("gamepad keys are refused"), C->Rebind(FName(TEXT("Brake")), EKeys::Gamepad_FaceButton_Bottom, Err));
	TestTrue(TEXT("rebind a negated steering key keeps the negation"), C->Rebind(FName(TEXT("SteerLeft")), EKeys::J, Err));
	TArray<FString> P2; TestTrue(TEXT("steering sign still correct"), C->Validate(P2));
	// gamepad buttons
	TestTrue(TEXT("rebind Boost to the left stick click"), C->Rebind(FName(TEXT("Boost")), EKeys::Gamepad_LeftThumbstick, Err));
	TestTrue(TEXT("pad key moved"), C->KeysFor(C->IA_EnergyBoost).Contains(EKeys::Gamepad_LeftThumbstick) && !C->KeysFor(C->IA_EnergyBoost).Contains(EKeys::Gamepad_FaceButton_Bottom));
	TestTrue(TEXT("the keyboard key of the same slot is untouched"), C->KeysFor(C->IA_EnergyBoost).Contains(EKeys::LeftShift));
	TestFalse(TEXT("a pad button already in use is refused"), C->Rebind(FName(TEXT("Overtake")), EKeys::Gamepad_FaceButton_Left, Err));
	TestFalse(TEXT("the Menu button is reserved"), C->Rebind(FName(TEXT("Overtake")), EKeys::Gamepad_Special_Right, Err));
	TestFalse(TEXT("throttle is analog: no button binding"), C->Rebind(FName(TEXT("Throttle")), EKeys::Gamepad_FaceButton_Top, Err));
	TArray<FString> PP; TestTrue(TEXT("mapping validates after pad rebinding"), C->Validate(PP));
	C->ResetBindings();
	TArray<FString> P3; TestTrue(TEXT("reset restores the defaults"), C->Validate(P3) && C->KeysFor(C->IA_Throttle).Contains(EKeys::W));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexSteerRampTest, "APEX.Input.KeyboardSteeringRampAndSpeedLimit", PIFlags)
bool FApexSteerRampTest::RunTest(const FString&)
{
	const FAPEXSteerTuning T = FAPEXDriveMath::Tuning(EAPEXSteerPreset::Sport);
	float S = 0.f;
	for (int32 i = 0; i < 2; ++i) { S = FAPEXDriveMath::KeyboardSteerStep(S, 1.f, 0.1f, T.KeyRise, T.KeyFall); }     // 0.2 s held
	TestTrue(TEXT("rises at the rise rate (0.6 after 0.2 s at 3/s), not instantly"), FMath::IsNearlyEqual(S, 0.6f, 0.02f));
	for (int32 i = 0; i < 10; ++i) { S = FAPEXDriveMath::KeyboardSteerStep(S, 1.f, 0.1f, T.KeyRise, T.KeyFall); }
	TestEqual(TEXT("clamps at 1"), S, 1.f);
	const float After = FAPEXDriveMath::KeyboardSteerStep(1.f, 0.f, 0.1f, T.KeyRise, T.KeyFall);
	TestTrue(TEXT("returns toward centre when released"), After < 1.f && After > 0.f);
	TestTrue(TEXT("reversing unwinds at the fall rate"), FAPEXDriveMath::KeyboardSteerStep(0.5f, -1.f, 0.1f, 3.f, 5.f) <= 0.0f + 0.001f);
	TestEqual(TEXT("A and D together cancel"), FAPEXDriveMath::AxisFromKeys(true, true), 0.f);
	TestEqual(TEXT("D alone is +1"), FAPEXDriveMath::AxisFromKeys(true, false), 1.f);
	TestEqual(TEXT("A alone is -1"), FAPEXDriveMath::AxisFromKeys(false, true), -1.f);
	TestEqual(TEXT("dead zone swallows small stick values"), FAPEXDriveMath::ApplyDeadZone(0.05f, 0.1f), 0.f);
	TestEqual(TEXT("full deflection stays 1 after the dead zone"), FAPEXDriveMath::ApplyDeadZone(1.f, 0.1f), 1.f);
	float Prev = 2.f;
	for (float V = 0.f; V <= 95.f; V += 5.f) { const float K = FAPEXDriveMath::SpeedSteerScale(V, 0.3f); TestTrue(TEXT("authority never grows with speed"), K <= Prev + 1e-4f); Prev = K; }
	TestEqual(TEXT("full authority at standstill"), FAPEXDriveMath::SpeedSteerScale(0.f, 0.3f), 1.f);
	TestTrue(TEXT("reduced authority at 90 m/s"), FAPEXDriveMath::SpeedSteerScale(90.f, 0.3f) < 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexPresetsAndGearsTest, "APEX.Driving.PresetsGearboxAndOwnership", PIFlags)
bool FApexPresetsAndGearsTest::RunTest(const FString&)
{
	const FAPEXSteerTuning A = FAPEXDriveMath::Tuning(EAPEXSteerPreset::Arcade), Sp = FAPEXDriveMath::Tuning(EAPEXSteerPreset::Sport), Si = FAPEXDriveMath::Tuning(EAPEXSteerPreset::Simulation);
	TestTrue(TEXT("arcade is the quickest to steer, simulation the slowest"), A.KeyRise > Sp.KeyRise && Sp.KeyRise > Si.KeyRise);
	TestTrue(TEXT("only arcade has the handbrake and assists"), A.bHandbrake && A.bAssists && !Sp.bHandbrake && !Si.bHandbrake && !Sp.bAssists && !Si.bAssists);
	TestTrue(TEXT("only simulation has the manual gearbox"), Si.bManualGearbox && !A.bManualGearbox && !Sp.bManualGearbox);
	int32 G = 1, Shifts = 0;
	for (float V = 0.f; V <= 94.f; V += 0.5f) { const int32 N = FAPEXDriveMath::GearForSpeed(V, G); TestTrue(TEXT("gears only go up while accelerating"), N >= G); if (N > G) { ++Shifts; } G = N; }
	TestEqual(TEXT("seven upshifts to top gear"), G, 8); TestEqual(TEXT("seven shifts"), Shifts, 7);
	for (int32 g = 1; g <= 8; ++g) { const float R = FAPEXDriveMath::Rev01(FAPEXDriveMath::GearTopSpeedMps(g), g); TestTrue(TEXT("rev is 1.0 at the gear's limit"), FMath::IsNearlyEqual(R, 1.f, 1e-3f)); }
	// command ownership: the AI path and demo mode can never be written by the player controller
	TestTrue(TEXT("a released manual player car accepts input"), FAPEXDriveMath::CanAcceptDriverInput(false, EAPEXDrivingMode::Manual, false));
	TestTrue(TEXT("assisted accepts input"), FAPEXDriveMath::CanAcceptDriverInput(false, EAPEXDrivingMode::Assisted, false));
	TestFalse(TEXT("an AI car rejects player input"), FAPEXDriveMath::CanAcceptDriverInput(true, EAPEXDrivingMode::Manual, false));
	TestFalse(TEXT("demo mode rejects player input"), FAPEXDriveMath::CanAcceptDriverInput(false, EAPEXDrivingMode::AutonomousDemo, false));
	TestFalse(TEXT("a car held on the grid rejects input"), FAPEXDriveMath::CanAcceptDriverInput(false, EAPEXDrivingMode::Manual, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexSuspensionTest, "APEX.Driving.SuspensionPoseDivesRollsAndSquats", PIFlags)
bool FApexSuspensionTest::RunTest(const FString&)
{
	float P, R, S;
	FAPEXDriveMath::SuspensionPose(-45.f, 0.f, 0.f, P, R, S);
	TestTrue(TEXT("hard braking dives the nose (negative pitch)"), P < -1.5f);
	FAPEXDriveMath::SuspensionPose(12.f, 0.f, 0.f, P, R, S);
	TestTrue(TEXT("acceleration squats the tail (positive pitch, smaller than braking dive)"), P > 0.3f && P < 1.9f);
	FAPEXDriveMath::SuspensionPose(0.f, 40.f, 0.f, P, R, S);
	TestTrue(TEXT("a right-hand corner leans the body to the left (negative roll)"), R < -1.f);
	FAPEXDriveMath::SuspensionPose(0.f, -40.f, 0.f, P, R, S);
	TestTrue(TEXT("a left-hand corner leans the body to the right (positive roll)"), R > 1.f);
	FAPEXDriveMath::SuspensionPose(0.f, 0.f, 12000.f, P, R, S);
	TestTrue(TEXT("downforce squats the car by a couple of cm"), S > 1.f && S < 3.6f);
	FAPEXDriveMath::SuspensionPose(500.f, 500.f, 1e6f, P, R, S);
	TestTrue(TEXT("poses are clamped"), P <= 1.8f && FMath::Abs(R) <= 2.5f && S <= 3.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexSteeringLawTest, "APEX.Driving.PlayerSteeringLawIsSpeedAware", PIFlags)
bool FApexSteeringLawTest::RunTest(const FString&)
{
	const FFormulaCarSetup Setup;
	const float Low = AFormulaVehiclePawn::NormalisedToModelSteer(Setup, 1.f, 8.f, 0.f, 1.f);
	const float High = AFormulaVehiclePawn::NormalisedToModelSteer(Setup, 1.f, 80.f, 8000.f, 1.f);
	TestTrue(TEXT("full lock at low speed"), Low > 0.95f);
	TestTrue(TEXT("a few percent of lock is enough at 80 m/s"), High > 0.f && High < 0.1f);
	TestEqual(TEXT("centre stays centre"), AFormulaVehiclePawn::NormalisedToModelSteer(Setup, 0.f, 50.f, 3000.f, 1.f), 0.f);
	TestTrue(TEXT("left is the mirror of right"), FMath::IsNearlyEqual(AFormulaVehiclePawn::NormalisedToModelSteer(Setup, -0.5f, 50.f, 3000.f, 1.f), -AFormulaVehiclePawn::NormalisedToModelSteer(Setup, 0.5f, 50.f, 3000.f, 1.f), 1e-5f));
	for (float V : { 15.f, 30.f, 60.f, 90.f })
	{
		const float N = 0.6f;
		const float M = AFormulaVehiclePawn::NormalisedToModelSteer(Setup, N, V, 4000.f, 1.f);
		TestTrue(TEXT("round trip"), FMath::IsNearlyEqual(AFormulaVehiclePawn::ModelSteerToNormalised(Setup, M, V, 4000.f, 1.f), N, 0.02f));
	}
	// the law must stay within the physics: the resulting yaw never exceeds what the tyres can hold
	float Yaw = 0.f, Speed = 70.f, Down = 0.f;
	AFormulaVehiclePawn::StepDynamics(Setup, 0.f, 0.f, AFormulaVehiclePawn::NormalisedToModelSteer(Setup, 1.f, 70.f, 6000.f, 1.f), 0.016f, Speed, Yaw, Down);
	TestTrue(TEXT("full normalised steering turns the car (positive yaw rate)"), Yaw > 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexPlayerDefaultsTest, "APEX.Driving.ManualIsTheDefaultAndPlayerControllerIsInstalled", PIFlags)
bool FApexPlayerDefaultsTest::RunTest(const FString&)
{
	const AFormulaVehiclePawn* Pawn = GetDefault<AFormulaVehiclePawn>();
	TestEqual(TEXT("default driving mode is Manual"), Pawn->DrivingMode, EAPEXDrivingMode::Manual);
	TestFalse(TEXT("autopilot is off by default"), Pawn->bAutoDrive);
	const AAPEXFormulaGameMode* GM = GetDefault<AAPEXFormulaGameMode>();
	TestTrue(TEXT("the game mode installs the APEX player controller"), GM->PlayerControllerClass == AAPEXPlayerController::StaticClass());
	TestTrue(TEXT("the default pawn is the formula car"), GM->DefaultPawnClass == AFormulaVehiclePawn::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexCameraCycleTest, "APEX.Camera.CycleVisitsSixViewsAndWraps", PIFlags)
bool FApexCameraCycleTest::RunTest(const FString&)
{
	EAPEXCamView V = EAPEXCamView::Chase;
	TSet<int32> Seen;
	for (int32 i = 0; i < 6; ++i) { Seen.Add(static_cast<int32>(V)); V = AAPEXCameraManager::NextView(V); }
	TestEqual(TEXT("six distinct views"), Seen.Num(), 6);
	TestEqual(TEXT("the cycle returns to the chase camera"), V, EAPEXCamView::Chase);
	TestEqual(TEXT("chase is followed by cockpit"), AAPEXCameraManager::NextView(EAPEXCamView::Chase), EAPEXCamView::Cockpit);
	TestEqual(TEXT("cinematic is last"), AAPEXCameraManager::NextView(EAPEXCamView::Nose), EAPEXCamView::Cinematic);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	// Heading change (degrees) after a keyboard tap of OnSeconds at a steady speed, simulated through the real keyboard ramp, speed-sensitive limit,
	// player steering law and the dynamics model. This is the "feel" number: how far one quick tap turns the car.
	float TapHeadingDeg(EAPEXSteerPreset Preset, float SpeedMps, float OnSeconds)
	{
		const FAPEXSteerTuning T = FAPEXDriveMath::Tuning(Preset);
		const FFormulaCarSetup Setup;
		float Ramp = 0.f, Speed = SpeedMps, Heading = 0.f, Down = SpeedMps * SpeedMps * 0.5f * 1.225f * 3.0f;
		const float Dt = 1.f / 120.f;
		for (float t = 0.f; t < 2.0f; t += Dt)
		{
			Ramp = FAPEXDriveMath::KeyboardSteerStep(Ramp, t < OnSeconds ? 1.f : 0.f, Dt, T.KeyRise, T.KeyFall);
			const float Norm = Ramp * T.Sensitivity * FAPEXDriveMath::SpeedSteerScale(Speed, T.MinSpeedScale);
			const float Model = AFormulaVehiclePawn::NormalisedToModelSteer(Setup, FMath::Clamp(Norm, -1.f, 1.f), Speed, Down, 1.f);
			float Yaw = 0.f, Df = 0.f; float V = Speed;
			AFormulaVehiclePawn::StepDynamics(Setup, 0.f, 0.f, Model, Dt, V, Yaw, Df);
			Heading += FMath::RadiansToDegrees(Yaw) * Dt;
		}
		return Heading;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexSteeringFeelTest, "APEX.Driving.KeyboardTapTurnsTheCarByAControllableAmount", PIFlags)
bool FApexSteeringFeelTest::RunTest(const FString&)
{
	for (EAPEXSteerPreset P : { EAPEXSteerPreset::Arcade, EAPEXSteerPreset::Sport, EAPEXSteerPreset::Simulation })
	{
		for (float V : { 20.f, 50.f, 80.f })
		{
			const float Tap = TapHeadingDeg(P, V, 0.12f), Long = TapHeadingDeg(P, V, 0.35f);
			AddInfo(FString::Printf(TEXT("%s @ %.0f m/s: 0.12 s tap -> %.2f deg, 0.35 s hold -> %.2f deg"), *UEnum::GetValueAsString(P), V, Tap, Long));
			TestTrue(TEXT("a tap turns the car the right way"), Tap > 0.f);
			TestTrue(TEXT("a longer hold turns more"), Long > Tap);
			// design targets: fine corrections at speed, enough turn-in at low speed
			if (V < 30.f) { TestTrue(TEXT("20 m/s: a tap is 1-4 degrees"), Tap > 1.f && Tap < 4.f); TestTrue(TEXT("20 m/s: a short hold turns 8+ degrees"), Long > 8.f); }
			if (V > 70.f) { TestTrue(TEXT("80 m/s: a tap is 0.3-2 degrees (no twitchiness)"), Tap > 0.3f && Tap < 2.f); TestTrue(TEXT("80 m/s: a short hold still turns 3+ degrees"), Long > 3.f); }
		}
	}
	for (float V : { 20.f, 50.f, 80.f })
	{
		const float A = TapHeadingDeg(EAPEXSteerPreset::Arcade, V, 0.12f), Sp = TapHeadingDeg(EAPEXSteerPreset::Sport, V, 0.12f), Si = TapHeadingDeg(EAPEXSteerPreset::Simulation, V, 0.12f);
		TestTrue(TEXT("Arcade turns faster than Sport, Sport than Simulation"), A > Sp && Sp > Si);
	}
	return true;
}
#endif
