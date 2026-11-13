#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Racing/APEXRaceTypes.h"
#include "APEXRaceManager.generated.h"

class AFormulaVehiclePawn;
class AFormulaTrack;

UENUM(BlueprintType)
enum class EAPEXRaceState : uint8
{
	Countdown,   // cars held on the grid, start lights counting
	Racing,
	Finished
};

/**
 * Runs the race: spawns the 19 AI cars on the grid, holds everyone until the lights go out, keeps the running order,
 * and gives each car awareness of its neighbours (~10 Hz, so AI cost does not scale with frame rate).
 * Distant cars tick less often; they sub-step internally, so their result is the same as at full rate.
 * Command line: -APEXNoGrid (single car, as before), -APEXGridSlot=<0-19> (player start), -APEXLaps=<n>, -APEXAIFullRate.
 */
UCLASS()
class APEXFORMULA_API AAPEXRaceManager : public AActor
{
	GENERATED_BODY()

public:
	AAPEXRaceManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Race") int32 PlayerGridSlot = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Race") int32 TargetLaps = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Race") float CountdownSeconds = 6.f;
	/** Cars far from the player tick at a lower rate (their physics still sub-steps at 60 Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Race") bool bThrottleDistantAI = true;

	UFUNCTION(BlueprintPure, Category="Race") const TArray<FAPEXStanding>& GetStandings() const { return Standings; }
	UFUNCTION(BlueprintPure, Category="Race") EAPEXRaceState GetState() const { return State; }
	/** Number of red start lights currently lit (0-5). */
	UFUNCTION(BlueprintPure, Category="Race") int32 GetLightsOn() const { return LightsOn; }
	UFUNCTION(BlueprintPure, Category="Race") float GetRaceTime() const { return RaceTime; }
	UFUNCTION(BlueprintPure, Category="Race") int32 GetPlayerPosition() const;
	UFUNCTION(BlueprintPure, Category="Race") int32 GetNumCars() const { return Cars.Num(); }
	/** Finish order so far (driver indices). */
	const TArray<int32>& GetClassification() const { return Classification; }

	/** The car currently running in this race position (1 = leader); null if unknown. */
	AFormulaVehiclePawn* CarAtPosition(int32 Position) const;
	/** Where the car in this grid slot (0 = pole) should be placed. */
	static int32 ParsePlayerSlotFromCommandLine(int32 DefaultSlot);

	/** One pit box per team: a car asks before committing to a stop; a team-mate already in the lane blocks it. */
	bool TryReserveBox(int32 TeamIndex, AFormulaVehiclePawn* Car);
	void ReleaseBox(int32 TeamIndex, AFormulaVehiclePawn* Car);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() TArray<TObjectPtr<AFormulaVehiclePawn>> Cars;     // every car, player included
	UPROPERTY() TObjectPtr<AFormulaVehiclePawn> Player;
	TArray<FAPEXStanding> Standings;
	TArray<int32> Classification;
	TMap<int32, float> FinishTimes;
	TMap<int32, TWeakObjectPtr<AFormulaVehiclePawn>> BoxOwners;   // team -> car using its box
	EAPEXRaceState State = EAPEXRaceState::Countdown;
	float StateTime = 0.f, RaceTime = 0.f, SlowTick = 0.f, FinishedTime = 0.f;
	int32 LightsOn = 0;
	bool bFullRateAI = false;
	bool bLoggedFinal = false;
	int32 OverlapLogged = 0;
	int32 OverlapsThisSample = 0, OverlapTotal = 0, OverlapSamples = 0, OverlapPeak = 0;

	void SpawnGrid();
	void FindPlayer();
	void UpdateAwareness();
	void UpdateStandings();
	void ReleaseCars();
};
