#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Racing/APEXRaceTypes.h"
#include "APEXTyres.generated.h"

UENUM(BlueprintType)
enum class EAPEXTyre : uint8 { Soft, Medium, Hard, Intermediate, Wet };

/** A race plan: starting tyre plus the lap on which to stop and the tyre to fit at each stop. */
USTRUCT(BlueprintType)
struct FAPEXStrategy
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) EAPEXTyre StartCompound = EAPEXTyre::Medium;
	UPROPERTY(BlueprintReadWrite) TArray<int32> StopLaps;                 // completed laps at which to pit (1 = after lap 1)
	UPROPERTY(BlueprintReadWrite) TArray<EAPEXTyre> NextCompounds;
};

/**
 * Tyre model and strategy rules, as pure functions so they can be unit-tested.
 * Grip multiplies the tyre friction coefficient; wear runs 0 (new) to 1 (gone).
 */
UCLASS()
class APEXFORMULA_API UAPEXTyreLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Grip of a new tyre on a dry track (1.0 = the medium). */
	UFUNCTION(BlueprintPure, Category="APEX Tyres") static float DryGrip(EAPEXTyre T);
	/** Wear per second at a nominal load. */
	UFUNCTION(BlueprintPure, Category="APEX Tyres") static float BaseWearPerSecond(EAPEXTyre T);
	/** Wetness (0..1) at which the compound works best: slicks 0, intermediate 0.5, full wet 1.0. */
	UFUNCTION(BlueprintPure, Category="APEX Tyres") static float WetOptimum(EAPEXTyre T);
	/** Total multiplier on the friction coefficient: compound x weather match x wear (with a cliff near the end). */
	UFUNCTION(BlueprintPure, Category="APEX Tyres") static float GripFactor(EAPEXTyre T, float Wear, float Wetness);
	/** Wear per second under load: harder cornering and braking wear faster; the wrong tyre for the weather wears much faster. */
	UFUNCTION(BlueprintPure, Category="APEX Tyres") static float WearRate(EAPEXTyre T, float LateralAccelMps2, float Brake, float Throttle, float Wetness);

	UFUNCTION(BlueprintPure, Category="APEX Tyres") static FString Letter(EAPEXTyre T);
	UFUNCTION(BlueprintPure, Category="APEX Tyres") static FLinearColor Colour(EAPEXTyre T);

	/** Choose a plan from race length, forecast wetness and the driver's character. */
	UFUNCTION(BlueprintPure, Category="APEX Strategy") static FAPEXStrategy PlanStrategy(int32 TotalLaps, float ForecastWetness, const FAPEXDriver& Driver);
	/** Which tyre to fit at a stop. */
	UFUNCTION(BlueprintPure, Category="APEX Strategy") static EAPEXTyre ChooseNextCompound(int32 LapsRemaining, float Wetness, EAPEXTyre Current, bool bAlreadyUsedTwoCompounds);
	/** Should the car come in now? Reactive rules on top of the plan: worn tyres, wrong tyres for the weather, planned lap. */
	UFUNCTION(BlueprintPure, Category="APEX Strategy") static bool ShouldPit(EAPEXTyre Current, float Wear, float Wetness, int32 LapsDone, int32 TotalLaps, int32 PlannedStopLap, int32 StopsDone);
	/** Stationary time in the box, from the team's crew quality. */
	UFUNCTION(BlueprintPure, Category="APEX Strategy") static float PitStopSeconds(const FAPEXTeam& Team, int32 DriverIndex);
};
