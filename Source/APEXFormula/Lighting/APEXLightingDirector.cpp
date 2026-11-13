#include "Lighting/APEXLightingDirector.h"
#include "Racing/FormulaTrack.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexLight, Log, All);

AAPEXLightingDirector::AAPEXLightingDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->bUseTemperature = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetRealTimeCaptureEnabled(true);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);
	if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"))) { Clouds->SetMaterial(CloudMat); }
	Clouds->SetLayerBottomAltitude(1.8f);
	Clouds->SetLayerHeight(4.5f);
	Clouds->SetTracingStartMaxDistance(350.f);
	Clouds->SetTracingMaxDistance(60.f);
	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(Root);
	Exposure->bUnbound = true;

#if PLATFORM_IOS
	bMobileSky = true;
#endif
}

FAPEXLightingPresetData AAPEXLightingDirector::GetBuiltinPreset(EAPEXLightingPreset Which)
{
	FAPEXLightingPresetData P;
	switch (Which)
	{
	case EAPEXLightingPreset::SunnyRace:
		P.SunLux = 100000.f; P.SunTemperatureK = 5800.f; P.SunSourceAngleDeg = 0.535f; P.SunElevationDeg = 60.f;
		P.SkylightIntensity = 1.0f; P.FogDensity = 0.0006f; P.FogColor = FLinearColor(0.5f, 0.6f, 0.78f);
		P.ExposureEV100 = 15.0f;
		P.DomeHorizon = FLinearColor(0.60f, 0.75f, 0.92f); P.DomeZenith = FLinearColor(0.07f, 0.22f, 0.62f);
		break;
	case EAPEXLightingPreset::GoldenHour:
		P.SunLux = 15000.f; P.SunTemperatureK = 3800.f; P.SunSourceAngleDeg = 0.535f; P.SunElevationDeg = 8.f;
		P.SkylightIntensity = 1.2f; P.MieScale = 1.5f;            // warm sunlight and a little haze
		P.FogDensity = 0.0014f; P.FogColor = FLinearColor(1.0f, 0.62f, 0.38f);
		P.ExposureEV100 = 9.8f;
		P.DomeHorizon = FLinearColor(1.0f, 0.55f, 0.28f); P.DomeZenith = FLinearColor(0.10f, 0.20f, 0.45f);
		break;
	case EAPEXLightingPreset::CloudyRace:
		P.SunLux = 10000.f; P.SunTemperatureK = 6500.f; P.SunSourceAngleDeg = 4.0f; P.SunElevationDeg = 45.f;
		P.SunDiskScale = 0.f; P.SkylightIntensity = 1.7f;          // overcast: diffuse, soft shadows, low contrast
		P.MieScale = 2.2f; P.FogDensity = 0.0016f; P.FogColor = FLinearColor(0.62f, 0.66f, 0.72f);
		P.ExposureEV100 = 11.5f;
		P.DomeHorizon = FLinearColor(0.62f, 0.64f, 0.67f); P.DomeZenith = FLinearColor(0.38f, 0.41f, 0.46f);
		break;
	case EAPEXLightingPreset::RainyRace:
		P.SunLux = 700.f; P.SunTemperatureK = 7000.f; P.SunSourceAngleDeg = 12.0f; P.SunElevationDeg = 35.f;   // diffuse: cloud cover, soft shadows
		P.SunDiskScale = 0.f; P.SkylightIntensity = 1.7f; P.SkylightTint = FLinearColor(0.78f, 0.88f, 1.0f); // cool fill
		P.MieScale = 3.0f; P.FogDensity = 0.0045f; P.FogColor = FLinearColor(0.35f, 0.40f, 0.46f);
		P.ExposureEV100 = 8.3f; P.Wetness = 1.f;
		P.DomeHorizon = FLinearColor(0.30f, 0.33f, 0.37f); P.DomeZenith = FLinearColor(0.15f, 0.17f, 0.21f);
		break;
	case EAPEXLightingPreset::NightRace:
		P.SunLux = 0.f; P.SunElevationDeg = -25.f; P.SunDiskScale = 0.f;
		P.SkylightIntensity = 0.05f; P.SkylightTint = FLinearColor(0.55f, 0.65f, 1.0f);
		P.FogDensity = 0.0010f; P.FogColor = FLinearColor(0.03f, 0.04f, 0.08f);
		P.ExposureEV100 = 5.5f; P.bFloodlights = true; P.FloodlightCandela = 90000.f;
		P.DomeHorizon = FLinearColor(0.02f, 0.03f, 0.07f); P.DomeZenith = FLinearColor(0.004f, 0.006f, 0.02f);
		break;
	}
	return P;
}

float AAPEXLightingDirector::SunElevationForHour(float Hours, float MaxElevationDeg)
{
	return MaxElevationDeg * FMath::Sin(PI * (Hours - 6.f) / 12.f);
}

float AAPEXLightingDirector::BiasForEV(float EV100, float CalibrationEV)
{
	return CalibrationEV - EV100;
}

float AAPEXLightingDirector::ExposureForLux(float Lux)
{
	return FMath::Log2(FMath::Max(Lux, 0.01f) / 2.5f);
}

void AAPEXLightingDirector::ApplyPreset(EAPEXLightingPreset NewPreset)
{
	Preset = NewPreset;
	ApplyData(PresetOverride ? PresetOverride->Data : GetBuiltinPreset(NewPreset));
}

void AAPEXLightingDirector::ApplyData(const FAPEXLightingPresetData& D)
{
	Current = D;
	if (bUseTimeOfDay)
	{
		Current.SunElevationDeg = SunElevationForHour(TimeOfDayHours);
		Current.SunAzimuthDeg = 90.f + 180.f * (TimeOfDayHours - 6.f) / 12.f; // east in the morning, west in the evening
		if (Current.SunElevationDeg < 0.f) { Current.SunLux = 0.f; }
	}

	// Sun. Pitch is negative so the light points down at positive elevations.
	Sun->SetIntensity(Current.SunLux);
	Sun->bUseTemperature = true;
	Sun->SetTemperature(Current.SunTemperatureK);
	Sun->SetLightSourceAngle(Current.SunSourceAngleDeg);
	Sun->AtmosphereSunDiskColorScale = FLinearColor(Current.SunDiskScale, Current.SunDiskScale, Current.SunDiskScale);
	Sun->SetWorldRotation(FRotator(-Current.SunElevationDeg, Current.SunAzimuthDeg, 0.f));
	Sun->SetVisibility(Current.SunLux > 0.f);

	if (Atmosphere && !bMobileSky)
	{
		Atmosphere->SetRayleighScatteringScale(Current.RayleighScale * 0.0331f);   // Unreal's Earth default is 0.0331
		Atmosphere->SetMieScatteringScale(Current.MieScale * 0.003996f);            // Earth default 0.003996
		Atmosphere->SetMieAnisotropy(Current.MieAnisotropy);
		Atmosphere->SetAtmosphereHeight(Current.AtmosphereHeightKm);
		Atmosphere->SetAerialPespectiveViewDistanceScale(Current.AerialPerspectiveScale);
	}

	SkyLight->SetIntensity(Current.SkylightIntensity);
	SkyLight->SetLightColor(Current.SkylightTint);
	Fog->SetFogDensity(Current.FogDensity);
	Fog->SetFogHeightFalloff(Current.FogHeightFalloff);
	Fog->SetFogInscatteringColor(Current.FogColor);

	// Manual exposure keeps the image tied to the light level (no auto-exposure "lifting" dark scenes).
	FPostProcessSettings& S = Exposure->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureBias = true;
	const float FinalBias = BiasForEV(Current.ExposureEV100, ExposureCalibrationEV) + ExposureOffsetEV;
	S.AutoExposureBias = FinalBias;
	LastBias = FinalBias;

	if (SkyDomeMID)
	{
		const FVector SunDir = -Sun->GetForwardVector();
		SkyDomeMID->SetVectorParameterValue(TEXT("Horizon"), Current.DomeHorizon);
		SkyDomeMID->SetVectorParameterValue(TEXT("Zenith"), Current.DomeZenith);
		SkyDomeMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(SunDir.X, SunDir.Y, SunDir.Z));
		// Measured on the Mac renderer: pixel ~ emissive(cd/m2) * 2^(bias - 10). Invert that so the dome lands at
		// DomeBrightness of display white whatever the preset's exposure is.
		SkyDomeMID->SetScalarParameterValue(TEXT("Luminance"), DomeBrightness * FMath::Pow(2.f, 10.f - FinalBias));
		SkyDomeMID->SetScalarParameterValue(TEXT("SunGlow"), Current.SunLux > 0.f ? FMath::Clamp(Current.SunDiskScale, 0.f, 1.f) : 0.f);
	}

	for (USpotLightComponent* L : Floodlights)
	{
		if (L) { L->SetVisibility(Current.bFloodlights); L->SetIntensity(Current.FloodlightCandela); }
	}
	if (Current.bFloodlights && Floodlights.Num() == 0) { BuildFloodlights(); }

	// Shared material parameters: city windows light up as the sun sets, masonry darkens in rain.
	if (UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_APEX_World.MPC_APEX_World"), nullptr, LOAD_NoWarn))
	{
		const float Night = FMath::Clamp((2.f - Current.SunElevationDeg) / 8.f, 0.f, 1.f);      // 0 above 2 degrees, 1 below -6
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("NightGlow"), Night);
		UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Wetness"), Current.Wetness);
	}

	ApplyCinematicLook();
	SetWetness(Current.Wetness);
	SkyLight->RecaptureSky();
	UE_LOG(LogApexLight, Log, TEXT("Lighting: preset=%d sun=%.0f lux %.0f K elev=%.1f EV100=%.1f wet=%.2f mobileSky=%d"),
		static_cast<int32>(Preset), Current.SunLux, Current.SunTemperatureK, Current.SunElevationDeg,
		S.AutoExposureBias, Current.Wetness, bMobileSky ? 1 : 0);
}

void AAPEXLightingDirector::SetTimeOfDay(float Hours)
{
	TimeOfDayHours = FMath::Clamp(Hours, 0.f, 24.f);
	bUseTimeOfDay = true;
	ApplyData(PresetOverride ? PresetOverride->Data : GetBuiltinPreset(Preset));
}

void AAPEXLightingDirector::SetWetness(float W)
{
	WetnessValue = FMath::Clamp(W, 0.f, 1.f);
	Current.Wetness = WetnessValue;
	ApplyWetnessToTrack();
}

void AAPEXLightingDirector::ApplyWetnessToTrack()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AFormulaTrack> It(World); It; ++It) { It->SetWetness(WetnessValue); }
	}
}

void AAPEXLightingDirector::SetupMobileSky()
{
	// Sky Atmosphere is desktop-class. On mobile, draw a big inside-out sphere with a gradient material.
	// bOvercastOnly (desktop, Rainy/Cloudy presets): keep the atmosphere for lighting and only draw the grey dome in front of the visible sky.
	if (!bOvercastOnly)
	{
		if (Atmosphere) { Atmosphere->DestroyComponent(); Atmosphere = nullptr; }
		Sun->SetAtmosphereSunLight(false);
		SkyLight->SetRealTimeCaptureEnabled(false);
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* DomeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Sky/M_APEX_SkyDome.M_APEX_SkyDome"));
	if (!Sphere || !DomeMat)
	{
		UE_LOG(LogApexLight, Warning, TEXT("Mobile sky: sphere or /Game/Materials/Sky/M_APEX_SkyDome missing (run Python/ue_materials.py)"));
		return;
	}
	SkyDome = NewObject<UStaticMeshComponent>(this, TEXT("SkyDome"));
	SkyDome->SetStaticMesh(Sphere);
	SkyDome->SetMobility(EComponentMobility::Movable);
	SkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyDome->CastShadow = false;
	SkyDome->bAffectDistanceFieldLighting = false;
	SkyDome->SetWorldScale3D(FVector(4000.f)); // engine sphere radius is 50 cm -> 2 km dome
	SkyDome->SetupAttachment(Root);
	SkyDome->RegisterComponent();
	SkyDomeMID = UMaterialInstanceDynamic::Create(DomeMat, this);
	SkyDome->SetMaterial(0, SkyDomeMID);
}

void AAPEXLightingDirector::BuildFloodlights()
{
	AFormulaTrack* Track = nullptr;
	if (UWorld* World = GetWorld()) { if (TActorIterator<AFormulaTrack> It(World); It) { Track = *It; } }
	if (!Track) { return; }

	const TArray<FVector> Line = Track->GetCenterline();
	const int32 N = Line.Num();
	const int32 Count = FMath::Clamp(MaxFloodlights, 1, 64);
	for (int32 k = 0; k < Count; ++k)
	{
		const int32 i = (k * N) / Count;
		const FVector Tangent = (Line[(i + 1) % N] - Line[(i + N - 1) % N]).GetSafeNormal();
		const FVector Left(-Tangent.Y, Tangent.X, 0.f);
		const float Side = (k % 2 == 0) ? 1.f : -1.f;
		const FVector Pos = Line[i] + Left * Side * 1500.f + FVector(0.f, 0.f, 1600.f); // 15 m beside the centreline, 16 m up

		USpotLightComponent* L = NewObject<USpotLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(Root);
		L->SetWorldLocation(Pos);
		L->SetWorldRotation(FRotator(-70.f, (-Left * Side).Rotation().Yaw, 0.f)); // down and across the track
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Current.FloodlightCandela);
		L->SetOuterConeAngle(80.f);
		L->SetInnerConeAngle(35.f);
		L->SetAttenuationRadius(7500.f);
		L->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
		L->SetCastShadows(false);
		L->SetVisibility(Current.bFloodlights);
		L->RegisterComponent();
		Floodlights.Add(L);
	}
}

void AAPEXLightingDirector::BeginPlay()
{
	Super::BeginPlay();

	FString PresetName;
	if (FParse::Value(FCommandLine::Get(), TEXT("APEXPreset="), PresetName))
	{
		if (PresetName.StartsWith(TEXT("Gold"), ESearchCase::IgnoreCase)) { Preset = EAPEXLightingPreset::GoldenHour; }
		else if (PresetName.StartsWith(TEXT("Cloud"), ESearchCase::IgnoreCase)) { Preset = EAPEXLightingPreset::CloudyRace; }
		else if (PresetName.StartsWith(TEXT("Rain"), ESearchCase::IgnoreCase)) { Preset = EAPEXLightingPreset::RainyRace; }
		else if (PresetName.StartsWith(TEXT("Night"), ESearchCase::IgnoreCase)) { Preset = EAPEXLightingPreset::NightRace; }
		else { Preset = EAPEXLightingPreset::SunnyRace; }
	}
	float Hour = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("APEXHour="), Hour)) { bUseTimeOfDay = true; TimeOfDayHours = Hour; }
	FParse::Value(FCommandLine::Get(), TEXT("APEXExposureOffset="), ExposureOffsetEV);
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXMobileSky"))) { bMobileSky = true; }

	if (bMobileSky) { SetupMobileSky(); }
	else if (Preset == EAPEXLightingPreset::RainyRace || Preset == EAPEXLightingPreset::CloudyRace) { bOvercastOnly = true; SetupMobileSky(); }   // desktop overcast sky
	ApplyPreset(Preset);
}


void AAPEXLightingDirector::ApplyCinematicLook()
{
	const bool bNight = Current.SunElevationDeg < -2.f;
	const bool bWet = Current.Wetness > 0.4f;

	// Volumetric fog: the sun and every street light scatter in it (light shafts between the buildings, halos round lamps at night).
	Fog->SetVolumetricFog(true);
	Fog->SetVolumetricFogScatteringDistribution(bNight ? 0.55f : 0.30f);
	Fog->SetVolumetricFogExtinctionScale(bWet ? 2.2f : (bNight ? 1.4f : 1.0f));
	Fog->SetVolumetricFogAlbedo(FColor(210, 220, 235));
	Fog->SetVolumetricFogDistance(bWet ? 40000.f : 70000.f);
	Fog->SetVolumetricFogStartDistance(0.f);
	// Ground haze: a thin, low second layer (more in rain and after dark).
	Fog->SecondFogData.FogDensity = bWet ? 0.035f : (bNight ? 0.020f : 0.008f);
	Fog->SecondFogData.FogHeightFalloff = 0.9f;
	Fog->SecondFogData.FogHeightOffset = -80.f;
	Fog->MarkRenderStateDirty();

	// Sun: volumetric scattering, light-shaft bloom, contact shadows, cloud shadows.
	Sun->SetVolumetricScatteringIntensity(bNight ? 0.f : 1.4f);
	Sun->bEnableLightShaftBloom = !bNight && !bWet;
	Sun->BloomScale = 0.12f;
	Sun->BloomThreshold = 0.f;
	Sun->BloomMaxBrightness = 6.f;
	Sun->ContactShadowLength = 0.04f;
	Sun->ContactShadowLengthInWS = false;
	Sun->bCastCloudShadows = true;
	Sun->CloudShadowStrength = 0.7f;
	Sun->CloudShadowOnAtmosphereStrength = 0.2f;
	Sun->CloudScatteredLuminanceScale = FLinearColor(1.f, 1.f, 1.f);
	SkyLight->bCloudAmbientOcclusion = true;
	SkyLight->CloudAmbientOcclusionStrength = 0.6f;

	// Volumetric clouds belong to the clear presets; the overcast presets keep their painted dome.
	if (Clouds) { Clouds->SetVisibility(!bOvercastOnly && !bMobileSky); }

	// Post-process look.
	FPostProcessSettings& S = Exposure->Settings;
	S.bOverride_BloomIntensity = true;            S.BloomIntensity = bNight ? 0.85f : 0.45f;
	S.bOverride_BloomThreshold = true;            S.BloomThreshold = 1.0f;
	S.bOverride_VignetteIntensity = true;         S.VignetteIntensity = 0.38f;
	S.bOverride_SceneFringeIntensity = true;      S.SceneFringeIntensity = 0.22f;                 // subtle: visible only at the frame edges
	S.bOverride_FilmGrainIntensity = true;        S.FilmGrainIntensity = bNight ? 0.22f : 0.10f;
	S.bOverride_FilmGrainIntensityShadows = true; S.FilmGrainIntensityShadows = 0.6f;
	S.bOverride_LensFlareIntensity = true;        S.LensFlareIntensity = 0.7f;
	S.bOverride_LensFlareThreshold = true;        S.LensFlareThreshold = 2.5f;
	S.bOverride_MotionBlurAmount = true;          S.MotionBlurAmount = 0.35f;
	S.bOverride_MotionBlurMax = true;             S.MotionBlurMax = 4.f;
	S.bOverride_LocalExposureHighlightContrastScale = true; S.LocalExposureHighlightContrastScale = 0.8f;   // pulls bright skies down so the street keeps its exposure
	S.bOverride_LocalExposureShadowContrastScale = true;    S.LocalExposureShadowContrastScale = 0.85f;
	S.bOverride_ColorSaturation = true;           S.ColorSaturation = FVector4(1.06, 1.06, 1.06, 1.0);
	S.bOverride_ColorContrast = true;             S.ColorContrast = FVector4(1.05, 1.05, 1.05, 1.0);
	S.bOverride_WhiteTemp = true;                 S.WhiteTemp = 6500.f;
	S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.45f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.5f;
	S.bOverride_LumenFinalGatherQuality = true;   S.LumenFinalGatherQuality = 1.5f;
	S.bOverride_LumenReflectionQuality = true;    S.LumenReflectionQuality = 1.5f;
	if (IConsoleVariable* Sharp = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen"))) { Sharp->Set(0.35f); }
}

void AAPEXLightingDirector::SetFlashEV(float EV)
{
	Exposure->Settings.bOverride_AutoExposureBias = true;
	Exposure->Settings.AutoExposureBias = LastBias + EV;
}
