#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Crowd/APEXNPCCrowd.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexNPCSlotSelectionTest, "APEX.Crowd.SlotSelectionIsNearestAndStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexNPCSlotSelectionTest::RunTest(const FString&)
{
	TArray<FVector> Slots; for (int32 i = 0; i < 20; ++i) { Slots.Add(FVector(i * 500.f, 0.f, 0.f)); }
	TArray<int32> Out;
	AAPEXNPCCrowd::SelectSlots(Slots, FVector(0.f, 0.f, 0.f), 3000.f, 4, {}, Out);
	TestEqual(TEXT("count capped"), Out.Num(), 4);
	TestEqual(TEXT("nearest first"), Out[0], 0);
	AAPEXNPCCrowd::SelectSlots(Slots, FVector(0.f, 0.f, 0.f), 3000.f, 100, {}, Out);
	TestEqual(TEXT("only slots inside the radius (0..6 -> 7)"), Out.Num(), 7);
	AAPEXNPCCrowd::SelectSlots(Slots, FVector(1000.f, 0.f, 0.f), 3000.f, 8, { 6, 15 }, Out);
	TestTrue(TEXT("a current slot still inside is kept and listed first"), Out.Num() > 0 && Out[0] == 6);
	TestFalse(TEXT("a current slot outside is dropped"), Out.Contains(15));
	return true;
}

#endif
