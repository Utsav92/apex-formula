#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Racing/APEXRaceTypes.h"
#include "Racing/FormulaTrack.h"

namespace
{
	constexpr EAutomationTestFlags GridFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexGridDataTest, "APEX.Grid.TenTeamsTwentyDriversAreConsistent", GridFlags)
bool FApexGridDataTest::RunTest(const FString&)
{
	const TArray<FAPEXTeam> Teams = UAPEXGridLibrary::GetTeams();
	const TArray<FAPEXDriver> Drivers = UAPEXGridLibrary::GetDrivers();
	TestEqual(TEXT("10 teams"), Teams.Num(), 10);
	TestEqual(TEXT("20 drivers"), Drivers.Num(), 20);

	TSet<int32> Numbers; TSet<FString> Codes, Names; TArray<int32> PerTeam; PerTeam.Init(0, Teams.Num());
	for (const FAPEXDriver& D : Drivers)
	{
		Numbers.Add(D.Number); Codes.Add(D.Code); Names.Add(D.Name);
		TestTrue(*FString::Printf(TEXT("%s has a valid team"), *D.Name), Teams.IsValidIndex(D.TeamIndex));
		if (Teams.IsValidIndex(D.TeamIndex)) { ++PerTeam[D.TeamIndex]; }
		TestTrue(*FString::Printf(TEXT("%s skill 0.9-1.0"), *D.Name), D.Skill >= 0.9f && D.Skill <= 1.f);
		TestTrue(*FString::Printf(TEXT("%s aggression/consistency in range"), *D.Name), D.Aggression >= 0.f && D.Aggression <= 1.f && D.Consistency >= 0.9f && D.Consistency <= 1.f);
		TestEqual(*FString::Printf(TEXT("%s code is 3 letters"), *D.Name), D.Code.Len(), 3);
	}
	TestEqual(TEXT("unique numbers"), Numbers.Num(), 20);
	TestEqual(TEXT("unique 3-letter codes"), Codes.Num(), 20);
	TestEqual(TEXT("unique names"), Names.Num(), 20);
	for (int32 i = 0; i < PerTeam.Num(); ++i) { TestEqual(*FString::Printf(TEXT("team %d has two drivers"), i), PerTeam[i], 2); }
	TestEqual(TEXT("player is driver 0 on team 0, number 27"), Drivers[0].TeamIndex * 100 + Drivers[0].Number, 27);

	// Liveries must be tellable apart in the timing tower.
	for (int32 a = 0; a < Teams.Num(); ++a)
	{
		for (int32 b = a + 1; b < Teams.Num(); ++b)
		{
			const FLinearColor& X = Teams[a].Primary; const FLinearColor& Y = Teams[b].Primary;
			const float Dist = FMath::Sqrt(FMath::Square(X.R - Y.R) + FMath::Square(X.G - Y.G) + FMath::Square(X.B - Y.B));
			TestTrue(*FString::Printf(TEXT("%s and %s liveries differ (%.2f)"), *Teams[a].Name, *Teams[b].Name, Dist), Dist > 0.12f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexGridLayoutTest, "APEX.Grid.TwentySlotsFitOnTheRoadBehindTheLine", GridFlags)
bool FApexGridLayoutTest::RunTest(const FString&)
{
	const AFormulaTrack* Track = GetDefault<AFormulaTrack>();
	const TArray<FVector> Line = Track->GetCenterline();
	TArray<FVector> Pos;
	for (int32 s = 0; s < 20; ++s)
	{
		const FTransform T = Track->GetGridTransform(s);
		Pos.Add(T.GetLocation());

		// Distance to the centreline (lateral offset) must keep the car on the 14 m road.
		float Best = TNumericLimits<float>::Max();
		for (const FVector& P : Line) { Best = FMath::Min(Best, FVector::Dist2D(T.GetLocation(), P)); }
		TestTrue(*FString::Printf(TEXT("slot %d is on the road (%.1f m from centreline)"), s, Best / 100.f), Best < 500.f);
	}
	// Every slot is behind the start line: its nearest centreline index lies in the last part of the lap.
	for (int32 s = 0; s < 20; ++s)
	{
		int32 BestI = 0; float Best = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Line.Num(); ++i) { const float D = FVector::Dist2D(Pos[s], Line[i]); if (D < Best) { Best = D; BestI = i; } }
		TestTrue(*FString::Printf(TEXT("slot %d is behind the line (index %d of %d)"), s, BestI, Line.Num()), BestI > Line.Num() * 8 / 10);
	}
	// Cars are 5.9 m x 2 m: no two slots may overlap.
	float MinGap = TNumericLimits<float>::Max();
	for (int32 a = 0; a < 20; ++a) { for (int32 b = a + 1; b < 20; ++b) { MinGap = FMath::Min(MinGap, FVector::Dist2D(Pos[a], Pos[b])); } }
	TestTrue(*FString::Printf(TEXT("closest two grid slots are %.1f m apart (>= 4.2 m)"), MinGap / 100.f), MinGap / 100.f >= 4.2f);
	// Pole is nearest the line, the last slot furthest back.
	TestTrue(TEXT("slot order runs back from the line"), FVector::Dist2D(Pos[0], Line[0]) < FVector::Dist2D(Pos[19], Line[0]));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
