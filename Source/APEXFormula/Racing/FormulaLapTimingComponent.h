#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FormulaLapTimingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLapCompleted, int32, LapNumber, float, LapTimeSeconds);

/**
 * Validates laps by requiring every checkpoint to be passed in order before the
 * start/finish line (checkpoint 0) counts. Pure distance checks: no physics volumes needed.
 */
UCLASS(ClassGroup=(Racing), meta=(BlueprintSpawnableComponent))
class APEXFORMULA_API UFormulaLapTimingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFormulaLapTimingComponent();

	/** World-space checkpoint centres. Index 0 is the start/finish line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lap Timing")
	TArray<FVector> Checkpoints;

	/** Pass radius in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lap Timing", meta=(ClampMin="100"))
	float CheckpointRadius = 1500.f;
	/** True while the owner feeds UpdatePosition itself (pit lane: it passes the racing-line projection, because the lane lies outside the gate radius). */
	bool bExternalDrive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lap Timing")
	int32 TargetLaps = 3;

	UPROPERTY(BlueprintReadOnly, Category="Lap Timing")
	int32 CompletedLaps = 0;

	UPROPERTY(BlueprintReadOnly, Category="Lap Timing")
	float CurrentLapTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Lap Timing")
	float BestLapTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Lap Timing")
	TArray<float> LapTimes;

	UPROPERTY(BlueprintAssignable, Category="Lap Timing")
	FOnLapCompleted OnLapCompleted;

	UFUNCTION(BlueprintPure, Category="Lap Timing")
	bool IsRaceFinished() const { return CompletedLaps >= TargetLaps; }

	/** Feed the car position each frame (also callable from tests). Returns true if a lap was just validated. */
	UFUNCTION(BlueprintCallable, Category="Lap Timing")
	bool UpdatePosition(const FVector& CarLocation, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category="Lap Timing")
	void ResetTiming();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Next checkpoint the car must reach. 0 means "waiting for the start line". */
	int32 NextCheckpoint = 0;
	bool bLapStarted = false;
};
