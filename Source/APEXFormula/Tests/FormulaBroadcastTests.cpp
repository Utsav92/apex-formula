#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Broadcast/APEXBroadcastDirector.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexPodChoiceTest, "APEX.Broadcast.PodChoiceHoldsShotUntilClearlyCloser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexPodChoiceTest::RunTest(const FString&)
{
	const TArray<FVector> Pods = { FVector(0, 0, 0), FVector(7000, 0, 0), FVector(14000, 0, 0) };
	TestEqual(TEXT("no pods -> -1"), AAPEXBroadcastDirector::ChoosePod({}, FVector::ZeroVector, -1), -1);
	TestEqual(TEXT("first choice is the nearest pod"), AAPEXBroadcastDirector::ChoosePod(Pods, FVector(6500, 0, 0), -1), 1);
	TestEqual(TEXT("barely closer to the next pod: hold the current shot"), AAPEXBroadcastDirector::ChoosePod(Pods, FVector(3600, 0, 0), 0), 0);
	TestEqual(TEXT("clearly closer: cut to the new pod"), AAPEXBroadcastDirector::ChoosePod(Pods, FVector(5500, 0, 0), 0), 1);
	return true;
}

#endif
