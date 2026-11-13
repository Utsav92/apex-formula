#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXTraffic.generated.h"

class UInstancedStaticMeshComponent;
class AFormulaTrack;

/**
 * Ambient city traffic: cars drive two-way along the street centrelines the track records (streets that clear the circuit). Purely visual: no collision,
 * no interaction with the race. One instanced component, one batched transform update per frame. Command line: -APEXNoTraffic, -APEXTrafficCars=<n> (default 90).
 */
UCLASS()
class APEXFORMULA_API AAPEXTraffic : public AActor
{
	GENERATED_BODY()

public:
	AAPEXTraffic();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Pure step (tested): advance a car along a segment, returning the new fraction (wraps at the ends) given speed (cm/s), length (cm), direction (+1/-1). */
	static float Advance(float Fraction, float SpeedCmS, float LengthCm, int32 Dir, float Dt);
	/** World position and yaw (degrees) for a car on a segment: lane offset to the right of travel. */
	static void Place(const FVector4& Seg, float Fraction, int32 Dir, float LaneCm, FVector& OutPos, float& OutYaw);

private:
	struct FCar { int32 Seg = 0; float T = 0.f; int32 Dir = 1; float Speed = 900.f; };

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM;
	UPROPERTY() TObjectPtr<AFormulaTrack> Track;
	TArray<FCar> Cars;
	FRandomStream Rand{20261117};
};
