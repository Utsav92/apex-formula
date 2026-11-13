#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXWeatherFX.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class AFormulaVehiclePawn;
class AAPEXLightingDirector;

/**
 * Rain and wheel spray, scaled by the track's wetness (lighting director).
 *  - Rain: ~3000 streak instances in a box that follows the camera; the fall animation runs in the vertex shader (M_APEX_Rain), so the CPU
 *    only moves one component per frame.
 *  - Spray: a pooled set of droplets simulated ballistically from each car's rear tyres (speed, wetness and distance gated).
 * Debug: -APEXForceWet=<0..1> overrides wetness; -APEXNoWeatherFX disables the actor.
 */
UCLASS()
class APEXFORMULA_API AAPEXWeatherFX : public AActor
{
	GENERATED_BODY()

public:
	AAPEXWeatherFX();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FDrop { FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector; float Age = 0.f, Life = 0.f, Size = 0.f; };

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RainISM;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SprayISM;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RainMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SprayMID;
	TArray<FDrop> Drops;
	int32 NextDrop = 0;
	TArray<TWeakObjectPtr<AFormulaVehiclePawn>> Cars;
	TWeakObjectPtr<AAPEXLightingDirector> Director;
	float Wet = 0.f, ForceWet = -1.f, Rescan = 0.f;
	// Lightning: a strike every 8-22 s in heavy rain: a double flash of exposure, then thunder after the light-to-sound delay.
	float NextStrike = 6.f, StrikeT = -1.f, ThunderIn = -1.f, StrikeClock = 0.f;
	bool bNoLightning = false;
	FRandomStream Rand{20261114};

	void Emit(const FVector& P, const FVector& CarVel, const FVector& Back);
};
