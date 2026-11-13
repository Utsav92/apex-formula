#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "APEXLightingDirector.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EAPEXLightingPreset : uint8
{
	SunnyRace,
	GoldenHour,
	CloudyRace,
	RainyRace,
	NightRace
};

/** Everything that defines a lighting condition. One table feeds the actor, the data assets and the tests. */
USTRUCT(BlueprintType)
struct FAPEXLightingPresetData
{
	GENERATED_BODY()

	// Sun (physical units: lux, kelvin, degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SunLux = 100000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SunTemperatureK = 5800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SunSourceAngleDeg = 0.535f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SunElevationDeg = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SunAzimuthDeg = -50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SunDiskScale = 1.f;

	// Sky and ambient
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SkylightIntensity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor SkylightTint = FLinearColor::White;

	// Atmosphere (scales apply to Unreal's Earth defaults)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RayleighScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MieScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MieAnisotropy = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AtmosphereHeightKm = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AerialPerspectiveScale = 1.f;

	// Height fog / distance haze
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FogDensity = 0.002f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FogHeightFalloff = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.7f);

	// Camera exposure in EV100 (manual, so the picture is stable and tied to the light levels)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExposureEV100 = 15.f;

	// Weather and night
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Wetness = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFloodlights = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FloodlightCandela = 50000.f;

	// Mobile-safe procedural sky dome colours (used instead of the atmosphere on iPhone)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor DomeHorizon = FLinearColor(0.55f, 0.7f, 0.9f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor DomeZenith = FLinearColor(0.08f, 0.22f, 0.6f);
};

/** Editable preset asset (created by Python/ue_lighting_assets.py from the built-in table). */
UCLASS(BlueprintType)
class APEXFORMULA_API UAPEXLightingPresetAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FAPEXLightingPresetData Data;
};

/**
 * Master lighting: sun, sky atmosphere, sky light, height fog, fixed exposure, floodlights, wetness.
 * On iPhone (or with -APEXMobileSky) the desktop-class Sky Atmosphere is replaced by a cheap procedural
 * sky dome. Command line: -APEXPreset=Sunny|Golden|Cloudy|Rainy|Night, -APEXHour=<0-24>, -APEXExposureOffset=<EV>.
 */
UCLASS()
class APEXFORMULA_API AAPEXLightingDirector : public AActor
{
	GENERATED_BODY()

public:
	AAPEXLightingDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") EAPEXLightingPreset Preset = EAPEXLightingPreset::SunnyRace;
	/** If set, this asset overrides the built-in table. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") TObjectPtr<UAPEXLightingPresetAsset> PresetOverride;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") bool bUseTimeOfDay = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting", meta=(ClampMin="0", ClampMax="24")) float TimeOfDayHours = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") bool bMobileSky = false;
	/** Desktop: draw only the grey overcast dome (Rainy/Cloudy) while Sky Atmosphere keeps driving the lighting. */
	bool bOvercastOnly = false;
	float LastBias = 0.f;
	/**
	 * Unreal's manual-exposure bias is brighter when HIGHER, while a physical EV100 is darker when higher, so
	 * bias = ExposureCalibrationEV - EV100. 11 was measured on the Mac renderer (sunny preset: EV 15 -> bias -4).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") float ExposureCalibrationEV = 11.f;
	/** Mobile sky dome brightness as a fraction of display white; tuned against screenshots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") float DomeBrightness = 0.5f;
	/** Added to every preset's EV100; use it to calibrate exposure against the real renderer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") float ExposureOffsetEV = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting") int32 MaxFloodlights = 56; // ~60 m apart on a 3.5 km lap so the lit patches overlap

	UPROPERTY(BlueprintReadOnly, Category="Lighting") FAPEXLightingPresetData Current;

	UFUNCTION(BlueprintCallable, Category="Lighting") void ApplyPreset(EAPEXLightingPreset NewPreset);
	UFUNCTION(BlueprintCallable, Category="Lighting") void ApplyData(const FAPEXLightingPresetData& Data);
	UFUNCTION(BlueprintCallable, Category="Lighting") void SetTimeOfDay(float Hours);
	UFUNCTION(BlueprintCallable, Category="Lighting") void SetWetness(float Wetness);
	UFUNCTION(BlueprintPure, Category="Lighting") float GetWetness() const { return WetnessValue; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Lighting") static FAPEXLightingPresetData GetBuiltinPreset(EAPEXLightingPreset Which);
	/** Approximate solar elevation for a local hour (06:00 sunrise, 12:00 noon, 18:00 sunset). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Lighting") static float SunElevationForHour(float Hours, float MaxElevationDeg = 65.f);
	/** The manual-exposure bias Unreal needs for a physical EV100 (see ExposureCalibrationEV). */
	/** The manual exposure bias currently applied (used to calibrate emissive-only effects such as rain). */
	float GetExposureBias() const { return LastBias; }
	/** Volumetric fog, light shafts, clouds, contact shadows and the post-process look (bloom, vignette, grain, fringe, lens flare, local exposure, motion blur, grading). Called by ApplyData. */
	void ApplyCinematicLook();
	/** Lightning: temporarily lifts the exposure by this many stops (0 restores it). Only the scene brightens; sky dome and effects keep their calibration. */
	void SetFlashEV(float EV);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Lighting") static float BiasForEV(float EV100, float CalibrationEV = 11.f);
	/** Incident-light exposure for an illuminance: EV100 = log2(E / 2.5). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Lighting") static float ExposureForLux(float Lux);

	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Exposure;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SkyDome;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SkyDomeMID;
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Floodlights;

	float WetnessValue = 0.f;
	void SetupMobileSky();
	void BuildFloodlights();
	void ApplyWetnessToTrack();
};
