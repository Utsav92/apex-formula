#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Lighting/APEXLightingDirector.h"

namespace
{
	constexpr EAutomationTestFlags LightFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	FAPEXLightingPresetData P(EAPEXLightingPreset W) { return AAPEXLightingDirector::GetBuiltinPreset(W); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexLightPresetValuesTest, "APEX.Lighting.PresetsMatchTheSpec", LightFlags)
bool FApexLightPresetValuesTest::RunTest(const FString&)
{
	const FAPEXLightingPresetData Sunny = P(EAPEXLightingPreset::SunnyRace);
	TestEqual(TEXT("sunny lux"), Sunny.SunLux, 100000.f);
	TestEqual(TEXT("sunny temperature"), Sunny.SunTemperatureK, 5800.f);
	TestEqual(TEXT("sunny source angle"), Sunny.SunSourceAngleDeg, 0.535f);
	TestEqual(TEXT("sunny elevation"), Sunny.SunElevationDeg, 60.f);
	TestEqual(TEXT("sunny skylight scale"), Sunny.SkylightIntensity, 1.0f);

	const FAPEXLightingPresetData Golden = P(EAPEXLightingPreset::GoldenHour);
	TestEqual(TEXT("golden lux"), Golden.SunLux, 15000.f);
	TestEqual(TEXT("golden temperature"), Golden.SunTemperatureK, 3800.f);
	TestEqual(TEXT("golden elevation"), Golden.SunElevationDeg, 8.f);
	TestEqual(TEXT("golden source angle"), Golden.SunSourceAngleDeg, 0.535f);
	TestTrue(TEXT("golden has extra haze (Mie)"), Golden.MieScale > 1.f);

	const FAPEXLightingPresetData Cloudy = P(EAPEXLightingPreset::CloudyRace);
	TestEqual(TEXT("cloudy lux"), Cloudy.SunLux, 10000.f);
	TestEqual(TEXT("cloudy temperature"), Cloudy.SunTemperatureK, 6500.f);
	TestTrue(TEXT("cloudy shadows are softened (wide source angle)"), Cloudy.SunSourceAngleDeg > 2.f);
	TestTrue(TEXT("cloudy sky fill is raised"), Cloudy.SkylightIntensity > Sunny.SkylightIntensity);

	const FAPEXLightingPresetData Rainy = P(EAPEXLightingPreset::RainyRace);
	TestTrue(TEXT("rainy sun substantially attenuated"), Rainy.SunLux < Cloudy.SunLux * 0.5f);
	TestEqual(TEXT("rainy track fully wet"), Rainy.Wetness, 1.f);
	TestTrue(TEXT("rainy fill is cool (blue >= red)"), Rainy.SkylightTint.B >= Rainy.SkylightTint.R);

	const FAPEXLightingPresetData Night = P(EAPEXLightingPreset::NightRace);
	TestTrue(TEXT("night sun below horizon"), Night.SunElevationDeg < 0.f);
	TestEqual(TEXT("night sun off"), Night.SunLux, 0.f);
	TestTrue(TEXT("night floodlights on"), Night.bFloodlights);
	TestTrue(TEXT("night skylight low"), Night.SkylightIntensity < 0.2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexLightExposureTest, "APEX.Lighting.ExposureIsTiedToLightLevel", LightFlags)
bool FApexLightExposureTest::RunTest(const FString&)
{
	const float Sunny = P(EAPEXLightingPreset::SunnyRace).ExposureEV100;
	const float Cloudy = P(EAPEXLightingPreset::CloudyRace).ExposureEV100;
	const float Golden = P(EAPEXLightingPreset::GoldenHour).ExposureEV100;
	const float Rainy = P(EAPEXLightingPreset::RainyRace).ExposureEV100;
	const float Night = P(EAPEXLightingPreset::NightRace).ExposureEV100;
	TestTrue(TEXT("darker conditions use lower EV: sunny > cloudy > golden > rainy > night"),
		Sunny > Cloudy && Cloudy > Golden && Golden > Rainy && Rainy > Night);

	TestTrue(TEXT("EV100 for 100,000 lux is ~15.3"), FMath::IsNearlyEqual(AAPEXLightingDirector::ExposureForLux(100000.f), 15.29f, 0.05f));
	for (const EAPEXLightingPreset W : { EAPEXLightingPreset::SunnyRace, EAPEXLightingPreset::GoldenHour, EAPEXLightingPreset::CloudyRace })
	{
		const FAPEXLightingPresetData D = P(W);
		const float Physical = AAPEXLightingDirector::ExposureForLux(D.SunLux);
		TestTrue(*FString::Printf(TEXT("preset %d exposure %.1f is within 3 EV of the sun-lux exposure %.1f"), static_cast<int32>(W), D.ExposureEV100, Physical),
			FMath::Abs(D.ExposureEV100 - Physical) <= 3.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexLightBiasTest, "APEX.Lighting.ExposureBiasMappingIsMonotonicAndCalibrated", LightFlags)
bool FApexLightBiasTest::RunTest(const FString&)
{
	// Regression for the all-white bug: darker scenes must get a HIGHER manual bias, never lower.
	const float Sunny = AAPEXLightingDirector::BiasForEV(P(EAPEXLightingPreset::SunnyRace).ExposureEV100);
	const float Night = AAPEXLightingDirector::BiasForEV(P(EAPEXLightingPreset::NightRace).ExposureEV100);
	TestTrue(TEXT("night bias is higher than sunny bias"), Night > Sunny);
	TestTrue(*FString::Printf(TEXT("sunny bias %.1f is the measured -4 (not the +15 that rendered pure white)"), Sunny), FMath::IsNearlyEqual(Sunny, -4.f, 0.5f));
	TestTrue(TEXT("every preset bias is within the range that rendered visibly on the Mac renderer (-8..8)"),
		Sunny >= -8.f && Night <= 8.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexLightSanityTest, "APEX.Lighting.AllPresetsAreSane", LightFlags)
bool FApexLightSanityTest::RunTest(const FString&)
{
	for (int32 i = 0; i < 5; ++i)
	{
		const FAPEXLightingPresetData D = P(static_cast<EAPEXLightingPreset>(i));
		const FString Tag = FString::Printf(TEXT("[preset %d]"), i);
		TestTrue(*(Tag + TEXT(" lux >= 0")), D.SunLux >= 0.f);
		TestTrue(*(Tag + TEXT(" temperature 1500-12000 K")), D.SunTemperatureK >= 1500.f && D.SunTemperatureK <= 12000.f);
		TestTrue(*(Tag + TEXT(" elevation -90..90")), D.SunElevationDeg >= -90.f && D.SunElevationDeg <= 90.f);
		TestTrue(*(Tag + TEXT(" source angle >= 0.5")), D.SunSourceAngleDeg >= 0.5f);
		TestTrue(*(Tag + TEXT(" EV100 3..17")), D.ExposureEV100 >= 3.f && D.ExposureEV100 <= 17.f);
		TestTrue(*(Tag + TEXT(" wetness 0..1")), D.Wetness >= 0.f && D.Wetness <= 1.f);
		TestTrue(*(Tag + TEXT(" fog density sane")), D.FogDensity >= 0.f && D.FogDensity <= 0.02f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTimeOfDayTest, "APEX.Lighting.TimeOfDayElevation", LightFlags)
bool FApexTimeOfDayTest::RunTest(const FString&)
{
	TestTrue(TEXT("noon is the highest sun"), FMath::IsNearlyEqual(AAPEXLightingDirector::SunElevationForHour(12.f), 65.f, 0.01f));
	TestTrue(TEXT("06:00 is on the horizon"), FMath::Abs(AAPEXLightingDirector::SunElevationForHour(6.f)) < 0.01f);
	TestTrue(TEXT("18:00 is on the horizon"), FMath::Abs(AAPEXLightingDirector::SunElevationForHour(18.f)) < 0.01f);
	TestTrue(TEXT("midnight is below the horizon"), AAPEXLightingDirector::SunElevationForHour(0.f) < 0.f);
	TestTrue(TEXT("09:00 and 15:00 are symmetric"),
		FMath::IsNearlyEqual(AAPEXLightingDirector::SunElevationForHour(9.f), AAPEXLightingDirector::SunElevationForHour(15.f), 0.01f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
