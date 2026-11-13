#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXStreetLights.generated.h"

class UPointLightComponent;
class AFormulaTrack;
class AAPEXLightingDirector;

/**
 * Calibrated streetlights: a pool of real point lights (no shadows) is moved, a few times a second, onto the lamp heads nearest the camera, and
 * switched on as the scene gets dark. Lamps further away only have their emissive lens (visible, but they light nothing).
 * Command line: -APEXNoStreetLights.
 */
UCLASS()
class APEXFORMULA_API AAPEXStreetLights : public AActor
{
	GENERATED_BODY()

public:
	AAPEXStreetLights();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** 0 in daylight, 1 at night, from the camera exposure bias (dark scenes are exposed up). Pure, tested. */
	static float NightAmount(float ExposureBias);
	/** Indices of the Count lamp heads nearest to Point (all of them if fewer). Pure, tested. */
	static void NearestHeads(const TArray<FVector>& Heads, const FVector& Point, int32 Count, TArray<int32>& Out);

private:
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY() TObjectPtr<AFormulaTrack> Track;
	TWeakObjectPtr<AAPEXLightingDirector> Director;
	float Timer = 0.f, Night = 0.f;
};
