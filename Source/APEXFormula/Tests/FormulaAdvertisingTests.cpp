#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Sponsors/APEXAdvertisingManager.h"
#include "Sponsors/APEXSponsorDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexAdLicenseRulesTest, "APEX.Ads.UnlicensedArtOnlyAppearsInPrototypeBuilds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApexAdLicenseRulesTest::RunTest(const FString&)
{
	TestTrue(TEXT("fictional art is always allowed"), FAPEXAdRules::MayDisplay(EAPEXSponsorLicense::OriginalFictional, false));
	TestTrue(TEXT("licensed art is always allowed"), FAPEXAdRules::MayDisplay(EAPEXSponsorLicense::Licensed, false));
	TestFalse(TEXT("an unlicensed placeholder is blocked in a distribution build"), FAPEXAdRules::MayDisplay(EAPEXSponsorLicense::UnlicensedPlaceholder, false));
	TestTrue(TEXT("an unlicensed placeholder is allowed in a private prototype"), FAPEXAdRules::MayDisplay(EAPEXSponsorLicense::UnlicensedPlaceholder, true));
	// rotation never repeats the previous neighbour and covers the whole list
	TSet<int32> Seen; int32 Prev = -1;
	for (int32 i = 0; i < 16; ++i) { const int32 N = FAPEXAdRules::NextRotation(i, 8); TestTrue(TEXT("neighbours differ"), N != Prev); Prev = N; Seen.Add(N); }
	TestEqual(TEXT("the rotation visits every sponsor"), Seen.Num(), 8);
	for (int32 Count : { 3, 4, 5, 6, 8, 9, 12 })
	{
		const int32 Stride = FAPEXAdRules::StrideFor(Count);
		TSet<int32> Visited; for (int32 i = 0; i < Count; ++i) { Visited.Add(FAPEXAdRules::NextRotation(i, Count, Stride)); }
		TestEqual(*FString::Printf(TEXT("a rotation over %d sponsors visits all of them"), Count), Visited.Num(), Count);
	}
	TestEqual(TEXT("a 300 m radius bend is still a straight for advertising"), FAPEXAdRules::SectionOfCurvature(1.f / 300.f), 0);
	TestEqual(TEXT("a 100 m radius bend is a corner"), FAPEXAdRules::SectionOfCurvature(1.f / 100.f), 1);
	UAPEXSponsorDataAsset* A = NewObject<UAPEXSponsorDataAsset>(GetTransientPackage());
	A->License = EAPEXSponsorLicense::UnlicensedPlaceholder;
	TestFalse(TEXT("a placeholder is not distributable"), A->IsDistributable());
	A->License = EAPEXSponsorLicense::Licensed;
	TestTrue(TEXT("licensed is distributable"), A->IsDistributable());
	return true;
}

#endif
