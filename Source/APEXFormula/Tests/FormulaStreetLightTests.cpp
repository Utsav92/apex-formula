#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lighting/APEXStreetLights.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexStreetLightPoolTest, "APEX.Lighting.StreetLightPoolPicksNearestAndFollowsNight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexStreetLightPoolTest::RunTest(const FString&)
{
	TArray<FVector> Heads; for (int32 i = 0; i < 10; ++i) { Heads.Add(FVector(i * 1000.f, 0.f, 0.f)); }
	TArray<int32> Near;
	AAPEXStreetLights::NearestHeads(Heads, FVector(4100.f, 0.f, 0.f), 3, Near);
	TestEqual(TEXT("three picked"), Near.Num(), 3);
	TestEqual(TEXT("nearest first"), Near[0], 4);
	TestTrue(TEXT("then its neighbours"), (Near[1] == 5 && Near[2] == 3) || (Near[1] == 3 && Near[2] == 5));
	AAPEXStreetLights::NearestHeads(Heads, FVector::ZeroVector, 50, Near);
	TestEqual(TEXT("never more than exist"), Near.Num(), 10);
	TestEqual(TEXT("lights are off in daylight"), AAPEXStreetLights::NightAmount(0.f), 0.f);
	TestEqual(TEXT("lights are fully on when dark"), AAPEXStreetLights::NightAmount(6.f), 1.f);
	return true;
}

#endif
