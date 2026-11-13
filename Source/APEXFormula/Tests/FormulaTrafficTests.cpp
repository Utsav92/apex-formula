#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Traffic/APEXTraffic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTrafficMotionTest, "APEX.Traffic.CarsAdvanceWrapAndKeepRight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexTrafficMotionTest::RunTest(const FString&)
{
	TestEqual(TEXT("moves forward"), AAPEXTraffic::Advance(0.2f, 1000.f, 10000.f, 1, 1.f), 0.3f);
	TestEqual(TEXT("moves backward"), AAPEXTraffic::Advance(0.2f, 1000.f, 10000.f, -1, 1.f), 0.1f);
	TestTrue(TEXT("wraps past the end"), FMath::IsNearlyEqual(AAPEXTraffic::Advance(0.95f, 1000.f, 10000.f, 1, 1.f), 0.05f, 1e-4f));
	TestTrue(TEXT("wraps past the start"), FMath::IsNearlyEqual(AAPEXTraffic::Advance(0.05f, 1000.f, 10000.f, -1, 1.f), 0.95f, 1e-4f));
	FVector P; float Yaw;
	AAPEXTraffic::Place(FVector4(0, 0, 10000, 0), 0.5f, 1, 260.f, P, Yaw);          // heading +X: right-hand lane is -Y
	TestTrue(TEXT("keeps to the right: -Y when heading +X"), P.Y < -200.f && FMath::IsNearlyEqual(P.X, 5000.f, 1.f));
	TestTrue(TEXT("yaw points along travel"), FMath::IsNearlyZero(Yaw, 0.1f));
	AAPEXTraffic::Place(FVector4(0, 0, 10000, 0), 0.5f, -1, 260.f, P, Yaw);         // heading -X: right-hand lane is +Y
	TestTrue(TEXT("opposite direction uses the other lane"), P.Y > 200.f);
	return true;
}

#endif
