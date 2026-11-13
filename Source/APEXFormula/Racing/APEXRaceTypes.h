#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "APEXRaceTypes.generated.h"

/** A fictional team: livery colours and a relative car pace (1.0 = the benchmark car). */
USTRUCT(BlueprintType)
struct FAPEXTeam
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Primary = FLinearColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Secondary = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Pace = 1.f;
};

/** A fictional driver. Skill scales pace, Aggression drives overtaking/defending, Consistency drives mistakes. */
USTRUCT(BlueprintType)
struct FAPEXDriver
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Code;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Number = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TeamIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Skill = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Aggression = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Consistency = 0.97f;
};

/** One row of the live timing tower. */
USTRUCT(BlueprintType)
struct FAPEXStanding
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Position = 0;
	UPROPERTY(BlueprintReadOnly) int32 DriverIndex = 0;
	UPROPERTY(BlueprintReadOnly) FString Code;
	UPROPERTY(BlueprintReadOnly) int32 Number = 0;
	UPROPERTY(BlueprintReadOnly) FLinearColor TeamColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) float GapSeconds = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Lap = 1;
	UPROPERTY(BlueprintReadOnly) bool bPlayer = false;
	UPROPERTY(BlueprintReadOnly) bool bFinished = false;
};

UCLASS()
class APEXFORMULA_API UAPEXGridLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** 10 teams. Index 0 is the player's team. */
	UFUNCTION(BlueprintPure, Category="APEX Race") static TArray<FAPEXTeam> GetTeams();
	/** 20 drivers, two per team. Index 0 is the player. All names are invented. */
	UFUNCTION(BlueprintPure, Category="APEX Race") static TArray<FAPEXDriver> GetDrivers();
};
