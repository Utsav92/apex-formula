#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VFX/APEXCityFX.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexSparkRateTest, "APEX.VFX.SparksOnlyUnderLoadAtSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexSparkRateTest::RunTest(const FString&)
{
	TestEqual(TEXT("no sparks when cruising"), AAPEXCityFX::SparkRate(80.f, 1.0f, 2.f), 0.f);
	TestEqual(TEXT("no sparks when slow even under load"), AAPEXCityFX::SparkRate(20.f, 5.f, 40.f), 0.f);
	TestTrue(TEXT("sparks under hard cornering at speed"), AAPEXCityFX::SparkRate(80.f, 4.5f, 0.f) > 100.f);
	TestTrue(TEXT("sparks under heavy braking at speed"), AAPEXCityFX::SparkRate(80.f, 0.f, 50.f) > 100.f);
	TestTrue(TEXT("rate is bounded"), AAPEXCityFX::SparkRate(100.f, 9.f, 90.f) <= 320.f);
	TestEqual(TEXT("no smoke when cruising"), AAPEXCityFX::SmokeRate(80.f, 1.f, 3.f), 0.f);
	TestTrue(TEXT("smoke when the tyres lock"), AAPEXCityFX::SmokeRate(80.f, 0.f, 55.f) > 20.f);
	return true;
}

#endif
