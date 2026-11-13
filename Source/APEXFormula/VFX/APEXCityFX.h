#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXCityFX.generated.h"

class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class AFormulaVehiclePawn;
class AAPEXLightingDirector;

/**
 * City effects: steam from street stacks around the lap, and sparks from the player's car under hard braking or cornering.
 * Both use pooled instanced spheres (like the weather FX), so they cost one component update per frame each.
 * There is no heat haze: it needs a screen-space refraction pass that this project does not have yet.
 * Command line: -APEXNoCityFX disables the actor.
 */
UCLASS()
class APEXFORMULA_API AAPEXCityFX : public AActor
{
	GENERATED_BODY()

public:
	AAPEXCityFX();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Spark emission rate (per second) for a given speed (m/s), lateral g and deceleration (m/s^2). Pure, tested. */
	static float SparkRate(float SpeedMps, float LatG, float DecelMps2);
	/** Tyre-smoke emission rate (per second): lock-ups under heavy braking and cars sliding at the limit. Pure, tested. */
	static float SmokeRate(float SpeedMps, float LatG, float DecelMps2);

private:
	struct FPuff { FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector; float Age = 0.f, Life = 0.f, Size = 0.f; };

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SteamISM;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SparkISM;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SmokeISM;
	UPROPERTY() TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StackISM;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SteamMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SparkMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SmokeMID;
	UPROPERTY() TObjectPtr<AFormulaVehiclePawn> Player;
	TWeakObjectPtr<AAPEXLightingDirector> Director;
	TArray<FPuff> Steam, Spark, Smoke;
	TArray<FVector> Stacks;
	int32 NextSteam = 0, NextSpark = 0, NextSmoke = 0;
	float PrevSpeed = 0.f, PrevYaw = 0.f;
	FRandomStream Rand{20261115};

	void FindPlayer();
	void PlaceStacks();
	void Step(TArray<FPuff>& Pool, UInstancedStaticMeshComponent* ISM, float Dt, float Grav, float Drag, float Grow);
};
