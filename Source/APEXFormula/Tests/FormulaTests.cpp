#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Racing/FormulaLapTimingComponent.h"
#include "Racing/FormulaTrack.h"
#include "Vehicles/FormulaVehiclePawn.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	UFormulaLapTimingComponent* MakeTimer()
	{
		UFormulaLapTimingComponent* T = NewObject<UFormulaLapTimingComponent>(GetTransientPackage());
		T->Checkpoints = { FVector(0, 0, 0), FVector(10000, 0, 0), FVector(10000, 10000, 0), FVector(0, 10000, 0) };
		T->CheckpointRadius = 1500.f;
		T->TargetLaps = 3;
		return T;
	}

	/** Drives one full lap in checkpoint order, 1 s per leg. */
	void DriveLap(UFormulaLapTimingComponent* T)
	{
		for (int32 k = 1; k <= 4; ++k)
		{
			T->UpdatePosition(T->Checkpoints[k % 4], 1.f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexLapCountTest, "APEX.Racing.LapCountsThreeValidLaps", TestFlags)
bool FApexLapCountTest::RunTest(const FString&)
{
	UFormulaLapTimingComponent* T = MakeTimer();
	T->UpdatePosition(T->Checkpoints[0], 0.f); // crossing the line starts timing, it is not a lap
	TestEqual(TEXT("no lap yet"), T->CompletedLaps, 0);

	DriveLap(T);
	TestEqual(TEXT("1 lap"), T->CompletedLaps, 1);
	TestFalse(TEXT("not finished"), T->IsRaceFinished());
	DriveLap(T);
	DriveLap(T);
	TestEqual(TEXT("3 laps"), T->CompletedLaps, 3);
	TestTrue(TEXT("finished"), T->IsRaceFinished());
	TestEqual(TEXT("lap times recorded"), T->LapTimes.Num(), 3);
	TestTrue(TEXT("best lap positive"), T->BestLapTime > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexLapCutTest, "APEX.Racing.CuttingTheCourseInvalidatesLap", TestFlags)
bool FApexLapCutTest::RunTest(const FString&)
{
	UFormulaLapTimingComponent* T = MakeTimer();
	T->UpdatePosition(T->Checkpoints[0], 0.f);
	// Skip checkpoint 1: jump straight to 2 then back to the line.
	T->UpdatePosition(T->Checkpoints[2], 1.f);
	T->UpdatePosition(T->Checkpoints[0], 1.f);
	TestEqual(TEXT("cut lap not counted"), T->CompletedLaps, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexAccelBrakeTest, "APEX.Physics.AccelerationAndBraking", TestFlags)
bool FApexAccelBrakeTest::RunTest(const FString&)
{
	const FFormulaCarSetup Setup;
	float Speed = 0.f, Yaw = 0.f, Down = 0.f;

	float Prev = 0.f;
	for (int32 i = 0; i < 300; ++i) // 5 s at 60 Hz, full throttle
	{
		AFormulaVehiclePawn::StepDynamics(Setup, 1.f, 0.f, 0.f, 1.f / 60.f, Speed, Yaw, Down);
		TestTrue(TEXT("speed never decreases under throttle"), Speed >= Prev - 1e-3f);
		Prev = Speed;
	}
	TestTrue(TEXT("accelerates to a sensible speed in 5 s (m/s)"), Speed > 40.f && Speed <= Setup.TopSpeedMps);
	TestTrue(TEXT("downforce is positive at speed"), Down > 0.f);

	const float StartSpeed = Speed;
	int32 Steps = 0;
	while (Speed > 0.5f && Steps < 2000)
	{
		AFormulaVehiclePawn::StepDynamics(Setup, 0.f, 1.f, 0.f, 1.f / 60.f, Speed, Yaw, Down);
		++Steps;
	}
	const float BrakeTime = Steps / 60.f;
	TestTrue(TEXT("full brake stops the car"), Speed <= 0.5f);
	TestTrue(*FString::Printf(TEXT("brake stop from %.0f m/s takes 1-8 s (was %.2f)"), StartSpeed, BrakeTime), BrakeTime > 1.f && BrakeTime < 8.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexAeroTest, "APEX.Physics.DownforceAndDragScaleWithSpeedSquared", TestFlags)
bool FApexAeroTest::RunTest(const FString&)
{
	const FFormulaCarSetup Setup;
	float S1 = 30.f, S2 = 60.f, Y = 0.f, D1 = 0.f, D2 = 0.f;
	AFormulaVehiclePawn::StepDynamics(Setup, 0.f, 0.f, 0.f, 0.f, S1, Y, D1);
	AFormulaVehiclePawn::StepDynamics(Setup, 0.f, 0.f, 0.f, 0.f, S2, Y, D2);
	TestTrue(TEXT("downforce at 2x speed is 4x"), FMath::IsNearlyEqual(D2 / D1, 4.f, 0.01f));
	// 0.5 * 1.225 * 30^2 * 1.0 * 3.0 = 1653.75 N
	TestTrue(TEXT("downforce matches 0.5*rho*v^2*A*C"), FMath::IsNearlyEqual(D1, 1653.75f, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexSteerGripTest, "APEX.Physics.LateralAccelerationIsGripLimited", TestFlags)
bool FApexSteerGripTest::RunTest(const FString&)
{
	const FFormulaCarSetup Setup;
	float Speed = 80.f, Yaw = 0.f, Down = 0.f;
	AFormulaVehiclePawn::StepDynamics(Setup, 0.f, 0.f, 1.f, 1.f / 60.f, Speed, Yaw, Down);
	const float LatAccel = Speed * FMath::Abs(Yaw);
	const float Limit = Setup.TyreMu * (Setup.MassKg * 9.81f + Down) / Setup.MassKg;
	TestTrue(TEXT("full lock at 80 m/s does not exceed the grip limit"), LatAccel <= Limit + 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTrackTest, "APEX.Track.CenterlineIsClosedAndDrivable", TestFlags)
bool FApexTrackTest::RunTest(const FString&)
{
	constexpr int32 N = 420;
	const TArray<FVector> Line = AFormulaTrack::ComputeCenterline(N);
	TestEqual(TEXT("point count"), Line.Num(), N);

	float TotalCm = 0.f, MaxGap = 0.f, MinGap = TNumericLimits<float>::Max();
	float MinRadiusCm = TNumericLimits<float>::Max();
	for (int32 i = 0; i < N; ++i)
	{
		const FVector& A = Line[i];
		const FVector& B = Line[(i + 1) % N];
		const FVector& C = Line[(i + 2) % N];
		const float Gap = FVector::Dist(A, B); // includes the wrap-around gap at i = N-1
		TotalCm += Gap;
		MaxGap = FMath::Max(MaxGap, Gap);
		MinGap = FMath::Min(MinGap, Gap);

		// Circumradius of three consecutive points.
		const float a = FVector::Dist(B, C), b = FVector::Dist(A, C), c = FVector::Dist(A, B);
		const float Area2 = FMath::Abs(FVector::CrossProduct(B - A, C - A).Z);
		if (Area2 > KINDA_SMALL_NUMBER)
		{
			MinRadiusCm = FMath::Min(MinRadiusCm, (a * b * c) / (2.f * Area2));
		}
	}

	TestTrue(*FString::Printf(TEXT("lap length %.0f m is 3.0-4.2 km"), TotalCm / 100.f), TotalCm / 100.f > 3000.f && TotalCm / 100.f < 4200.f);
	TestTrue(*FString::Printf(TEXT("points evenly spaced (%.1f-%.1f m), loop closes without a jump"), MinGap / 100.f, MaxGap / 100.f), MaxGap < MinGap * 1.25f);
	// Slow enough corners to matter, but a car with full lock (~8 m turn radius) can still make them.
	TestTrue(*FString::Printf(TEXT("tightest corner radius %.0f m is 18-40 m"), MinRadiusCm / 100.f), MinRadiusCm / 100.f >= 18.f && MinRadiusCm / 100.f <= 40.f);

	// Sections that are far apart along the lap must also be far apart on the ground (no crossing, room for barriers).
	float Closest = TNumericLimits<float>::Max();
	for (int32 i = 0; i < N; ++i)
	{
		for (int32 j = i + 1; j < N; ++j)
		{
			const int32 AlongLap = FMath::Min(j - i, N - (j - i));
			if (AlongLap * (TotalCm / N) < 15000.f) { continue; } // skip anything within 150 m along the lap
			Closest = FMath::Min(Closest, FVector::Dist(Line[i], Line[j]));
		}
	}
	TestTrue(*FString::Printf(TEXT("closest approach of separate sections %.0f m >= 60 m"), Closest / 100.f), Closest / 100.f >= 60.f);

	// The start line sits on a straight: the first 300 m must be nearly straight.
	float StraightMaxDev = 0.f;
	for (int32 i = 0; i < 30; ++i) { StraightMaxDev = FMath::Max(StraightMaxDev, FMath::Abs(Line[i].Y - Line[0].Y)); }
	TestTrue(TEXT("start line is on a straight"), StraightMaxDev < 200.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTopSpeedTest, "APEX.Physics.DragSetsTopSpeedNotTheClamp", TestFlags)
bool FApexTopSpeedTest::RunTest(const FString&)
{
	const FFormulaCarSetup Setup;
	float Speed = 0.f, Yaw = 0.f, Down = 0.f;
	for (int32 i = 0; i < 60 * 90; ++i) // 90 s flat out on a long straight
	{
		AFormulaVehiclePawn::StepDynamics(Setup, 1.f, 0.f, 0.f, 1.f / 60.f, Speed, Yaw, Down);
	}
	const float Kph = Speed * 3.6f;
	TestTrue(*FString::Printf(TEXT("terminal speed %.0f km/h is 310-350"), Kph), Kph > 310.f && Kph < 350.f);
	TestTrue(TEXT("limited by drag, not the safety clamp"), Speed < Setup.TopSpeedMps - 2.f);

	// Acceleration is believable: 0-100 km/h in 1.5-3 s, 0-200 km/h in 3-6 s.
	Speed = 0.f;
	float T100 = -1.f, T200 = -1.f, Time = 0.f;
	while (Time < 30.f && T200 < 0.f)
	{
		AFormulaVehiclePawn::StepDynamics(Setup, 1.f, 0.f, 0.f, 1.f / 120.f, Speed, Yaw, Down);
		Time += 1.f / 120.f;
		if (T100 < 0.f && Speed * 3.6f >= 100.f) { T100 = Time; }
		if (T200 < 0.f && Speed * 3.6f >= 200.f) { T200 = Time; }
	}
	TestTrue(*FString::Printf(TEXT("0-100 km/h in %.2f s is 1.5-3.0"), T100), T100 > 1.5f && T100 < 3.0f);
	TestTrue(*FString::Printf(TEXT("0-200 km/h in %.2f s is 3.0-6.0"), T200), T200 > 3.0f && T200 < 6.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
