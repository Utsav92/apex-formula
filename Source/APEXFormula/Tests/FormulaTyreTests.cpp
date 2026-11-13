#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Racing/APEXTyres.h"
#include "Racing/APEXRaceTypes.h"
#include "Racing/FormulaTrack.h"

namespace
{
	constexpr EAutomationTestFlags TyreFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTyreGripOrderTest, "APEX.Tyres.DryGripOrderedAndWearReducesIt", TyreFlags)
bool FApexTyreGripOrderTest::RunTest(const FString&)
{
	// Fresh dry slicks: soft > medium > hard, and a slick beats a wet-weather tyre on a dry track.
	const float S = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, 0.f, 0.f);
	const float M = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Medium, 0.f, 0.f);
	const float H = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Hard, 0.f, 0.f);
	const float I = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Intermediate, 0.f, 0.f);
	TestTrue(TEXT("soft > medium > hard when new"), S > M && M > H);
	TestTrue(TEXT("hard > intermediate on a dry track"), H > I);
	TestTrue(TEXT("medium is the 1.0 reference"), FMath::IsNearlyEqual(M, 1.f, 0.01f));

	// Wear only ever reduces grip, with a cliff near the end.
	float Prev = 10.f;
	for (float W = 0.f; W <= 1.001f; W += 0.05f)
	{
		const float G = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, W, 0.f);
		TestTrue(*FString::Printf(TEXT("grip falls monotonically (wear %.2f)"), W), G <= Prev + 1e-4f);
		Prev = G;
	}
	const float Mid = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, 0.5f, 0.f);
	const float End = UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, 1.f, 0.f);
	TestTrue(TEXT("a dead soft is worse than a fresh hard (it is time to stop)"), End < H);
	TestTrue(TEXT("the cliff drops more in the last 15% than in the previous 15%"),
		(UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, 0.85f, 0.f) - End) > (UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, 0.70f, 0.f) - UAPEXTyreLibrary::GripFactor(EAPEXTyre::Soft, 0.85f, 0.f)) * 0.9f);
	TestTrue(TEXT("mid-life grip sits between fresh and dead"), Mid < S && Mid > End);

	// Weather: a wet tyre wins on a soaked track, a slick loses a lot.
	TestTrue(TEXT("wet tyre beats slick in heavy rain"), UAPEXTyreLibrary::GripFactor(EAPEXTyre::Wet, 0.f, 1.f) > UAPEXTyreLibrary::GripFactor(EAPEXTyre::Medium, 0.f, 1.f));
	TestTrue(TEXT("intermediate beats slick on a damp track"), UAPEXTyreLibrary::GripFactor(EAPEXTyre::Intermediate, 0.f, 0.5f) > UAPEXTyreLibrary::GripFactor(EAPEXTyre::Medium, 0.f, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTyreWearTest, "APEX.Tyres.WearRatesAndLife", TyreFlags)
bool FApexTyreWearTest::RunTest(const FString&)
{
	const float Soft = UAPEXTyreLibrary::WearRate(EAPEXTyre::Soft, 20.f, 0.f, 1.f, 0.f);
	const float Med = UAPEXTyreLibrary::WearRate(EAPEXTyre::Medium, 20.f, 0.f, 1.f, 0.f);
	const float Hard = UAPEXTyreLibrary::WearRate(EAPEXTyre::Hard, 20.f, 0.f, 1.f, 0.f);
	TestTrue(TEXT("soft wears fastest, hard slowest"), Soft > Med && Med > Hard);
	TestTrue(TEXT("cornering load increases wear"), UAPEXTyreLibrary::WearRate(EAPEXTyre::Medium, 35.f, 0.f, 0.f, 0.f) > UAPEXTyreLibrary::WearRate(EAPEXTyre::Medium, 0.f, 0.f, 0.f, 0.f));
	TestTrue(TEXT("braking increases wear"), UAPEXTyreLibrary::WearRate(EAPEXTyre::Medium, 0.f, 1.f, 0.f, 0.f) > UAPEXTyreLibrary::WearRate(EAPEXTyre::Medium, 0.f, 0.f, 0.f, 0.f));
	TestTrue(TEXT("a wet tyre overheats on a dry track"), UAPEXTyreLibrary::WearRate(EAPEXTyre::Wet, 20.f, 0.f, 1.f, 0.f) > UAPEXTyreLibrary::WearRate(EAPEXTyre::Wet, 20.f, 0.f, 1.f, 1.f) * 2.f);
	// Life at an average load: a soft survives ~3-4 laps of 60 s, a hard more than 8.
	const float AvgLoad = 1.4f;
	TestTrue(TEXT("soft lasts 150-300 s"), (1.f / (UAPEXTyreLibrary::BaseWearPerSecond(EAPEXTyre::Soft) * AvgLoad)) > 150.f && (1.f / (UAPEXTyreLibrary::BaseWearPerSecond(EAPEXTyre::Soft) * AvgLoad)) < 300.f);
	TestTrue(TEXT("hard lasts more than ~400 s (about 7 laps)"), (1.f / (UAPEXTyreLibrary::BaseWearPerSecond(EAPEXTyre::Hard) * AvgLoad)) > 400.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexStrategyTest, "APEX.Tyres.StrategyAndPitRules", TyreFlags)
bool FApexStrategyTest::RunTest(const FString&)
{
	const TArray<FAPEXDriver> Drivers = UAPEXGridLibrary::GetDrivers();
	for (const FAPEXDriver& D : Drivers)
	{
		const FAPEXStrategy Short = UAPEXTyreLibrary::PlanStrategy(3, 0.f, D);
		TestEqual(*FString::Printf(TEXT("%s: no stop in a 3-lap race"), *D.Name), Short.StopLaps.Num(), 0);
		const FAPEXStrategy Eight = UAPEXTyreLibrary::PlanStrategy(8, 0.f, D);
		TestEqual(*FString::Printf(TEXT("%s: one stop in 8 laps"), *D.Name), Eight.StopLaps.Num(), 1);
		if (Eight.StopLaps.Num() == 1)
		{
			TestTrue(*FString::Printf(TEXT("%s: stop lap inside the race"), *D.Name), Eight.StopLaps[0] >= 1 && Eight.StopLaps[0] <= 7);
			TestTrue(*FString::Printf(TEXT("%s: starts on slicks, not intermediates"), *D.Name), Eight.StartCompound <= EAPEXTyre::Hard);
		}
		const FAPEXStrategy Long = UAPEXTyreLibrary::PlanStrategy(20, 0.f, D);
		TestEqual(*FString::Printf(TEXT("%s: two stops in 20 laps"), *D.Name), Long.StopLaps.Num(), 2);
		TestEqual(*FString::Printf(TEXT("%s: one stop in 10 laps"), *D.Name), UAPEXTyreLibrary::PlanStrategy(10, 0.f, D).StopLaps.Num(), 1);
		TestEqual(TEXT("wet forecast starts on wets"), UAPEXTyreLibrary::PlanStrategy(8, 0.9f, D).StartCompound, EAPEXTyre::Wet);
		TestEqual(TEXT("damp forecast starts on intermediates"), UAPEXTyreLibrary::PlanStrategy(8, 0.5f, D).StartCompound, EAPEXTyre::Intermediate);
	}

	// Pit rules.
	TestFalse(TEXT("fresh tyres: stay out"), UAPEXTyreLibrary::ShouldPit(EAPEXTyre::Medium, 0.2f, 0.f, 2, 8, 4, 0));
	TestTrue(TEXT("planned lap reached: pit"), UAPEXTyreLibrary::ShouldPit(EAPEXTyre::Medium, 0.5f, 0.f, 4, 8, 4, 0));
	TestTrue(TEXT("tyres gone: pit"), UAPEXTyreLibrary::ShouldPit(EAPEXTyre::Soft, 0.95f, 0.f, 3, 8, -1, 0));
	TestFalse(TEXT("never stop on the last lap"), UAPEXTyreLibrary::ShouldPit(EAPEXTyre::Soft, 1.f, 0.f, 7, 8, -1, 0));
	TestTrue(TEXT("slicks in the rain: pit"), UAPEXTyreLibrary::ShouldPit(EAPEXTyre::Medium, 0.1f, 0.9f, 2, 8, -1, 0));
	TestTrue(TEXT("dry-race rule: next tyre differs from the current one"), UAPEXTyreLibrary::ChooseNextCompound(5, 0.f, EAPEXTyre::Hard, false) != EAPEXTyre::Hard);
	TestEqual(TEXT("rain: next tyre is a wet-weather one"), UAPEXTyreLibrary::ChooseNextCompound(5, 0.9f, EAPEXTyre::Soft, true), EAPEXTyre::Wet);

	// Stop times: realistic range, better crews are quicker.
	const TArray<FAPEXTeam> Teams = UAPEXGridLibrary::GetTeams();
	float Fast = 1e9f, Slow = 0.f;
	for (int32 i = 0; i < 20; ++i)
	{
		const float T = UAPEXTyreLibrary::PitStopSeconds(Teams[Drivers[i].TeamIndex], i);
		Fast = FMath::Min(Fast, T); Slow = FMath::Max(Slow, T);
	}
	TestTrue(TEXT("fastest stop 2.0-2.8 s"), Fast >= 2.0f && Fast <= 2.8f);
	TestTrue(TEXT("slowest stop under 6 s"), Slow < 6.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexPitGeometryTest, "APEX.Tyres.PitLaneGeometry", TyreFlags)
bool FApexPitGeometryTest::RunTest(const FString&)
{
	// Boxes: one per team, evenly spaced, inside the limited zone, on the garage side of the fast lane.
	for (int32 t = 0; t < 10; ++t)
	{
		const FVector B = AFormulaTrack::GetPitBoxLocation(t);
		TestTrue(*FString::Printf(TEXT("box %d inside the speed-limit zone"), t), B.X > AFormulaTrack::PitLimitStartXCm && B.X < AFormulaTrack::PitLimitEndXCm);
		TestTrue(*FString::Printf(TEXT("box %d beyond the fast lane"), t), FMath::IsNearlyEqual(B.Y, AFormulaTrack::PitLaneYCm + AFormulaTrack::PitWorkLaneOffsetCm, 1.f));
		if (t > 0) { TestTrue(TEXT("boxes at least 20 m apart (car length 5.9 m + the ease in/out)"), FMath::Abs(B.X - AFormulaTrack::GetPitBoxLocation(t - 1).X) >= 2000.f); }
	}
	TestTrue(TEXT("work lane at least 3.5 m from the fast lane (no contact with passing cars)"), FMath::Abs(AFormulaTrack::PitWorkLaneOffsetCm) >= 350.f);
	TestTrue(TEXT("lane limit is 80 km/h or less"), AFormulaTrack::PitSpeedLimitMps * 3.6f <= 80.f);
	TestTrue(TEXT("barrier gaps cover the entry and exit"), AFormulaTrack::IsBarrierGap(-27000.f) && AFormulaTrack::IsBarrierGap(29000.f) && !AFormulaTrack::IsBarrierGap(0.f));
	return true;
}

#endif
