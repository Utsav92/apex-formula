#include "Racing/FormulaLapTimingComponent.h"

UFormulaLapTimingComponent::UFormulaLapTimingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFormulaLapTimingComponent::ResetTiming()
{
	CompletedLaps = 0;
	CurrentLapTime = 0.f;
	BestLapTime = 0.f;
	LapTimes.Reset();
	NextCheckpoint = 0;
	bLapStarted = false;
}

bool UFormulaLapTimingComponent::UpdatePosition(const FVector& CarLocation, float DeltaSeconds)
{
	if (Checkpoints.Num() < 2 || IsRaceFinished())
	{
		return false;
	}

	if (bLapStarted)
	{
		CurrentLapTime += DeltaSeconds;
	}

	const float RadiusSq = FMath::Square(CheckpointRadius);
	if (FVector::DistSquared(CarLocation, Checkpoints[NextCheckpoint]) > RadiusSq)
	{
		return false;
	}

	if (NextCheckpoint == 0)
	{
		if (!bLapStarted)
		{
			// First crossing of the line starts timing; it is not a completed lap.
			bLapStarted = true;
			CurrentLapTime = 0.f;
			NextCheckpoint = 1;
			return false;
		}

		// Reached the line again with every checkpoint passed in order.
		++CompletedLaps;
		LapTimes.Add(CurrentLapTime);
		BestLapTime = (BestLapTime <= 0.f) ? CurrentLapTime : FMath::Min(BestLapTime, CurrentLapTime);
		OnLapCompleted.Broadcast(CompletedLaps, CurrentLapTime);
		CurrentLapTime = 0.f;
		NextCheckpoint = 1;
		return true;
	}

	NextCheckpoint = (NextCheckpoint + 1) % Checkpoints.Num();
	return false;
}

void UFormulaLapTimingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bExternalDrive) { return; }
	if (const AActor* Owner = GetOwner())
	{
		UpdatePosition(Owner->GetActorLocation(), DeltaTime);
	}
}
