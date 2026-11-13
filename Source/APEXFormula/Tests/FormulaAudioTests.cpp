#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Sound/APEXAudioDirector.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexRevModelTest, "APEX.Audio.RevModelShiftsAndStaysInRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexRevModelTest::RunTest(const FString&)
{
	float R; int32 G; float PrevR = 0.f; int32 PrevG = 0; int32 Shifts = 0;
	for (float K = 0.f; K <= 330.f; K += 1.f)
	{
		AAPEXAudioDirector::RevModel(K, R, G);
		TestTrue(TEXT("rev in 0..1"), R >= 0.f && R <= 1.f);
		TestTrue(TEXT("gear 0..7"), G >= 0 && G <= 7);
		if (G > PrevG) { ++Shifts; TestTrue(TEXT("revs drop on an upshift"), R < PrevR); }
		PrevR = R; PrevG = G;
	}
	TestEqual(TEXT("seven upshifts from standstill to top speed"), Shifts, 7);
	const FVector4 Lo = AAPEXAudioDirector::EngineLayers(0.1f), Hi = AAPEXAudioDirector::EngineLayers(1.f);
	TestTrue(TEXT("pitch rises with revs"), Hi.X > Lo.X && Hi.Y > Lo.Y);
	TestTrue(TEXT("low layer dominates at low revs, high layer at high revs"), Lo.Z > Lo.W && Hi.W > Hi.Z);
	return true;
}

#endif
