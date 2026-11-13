#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APEXFormulaGameMode.generated.h"

/**
 * Builds the test circuit, lights and a start slot at world init, so any empty map is a playable level.
 * Once real maps exist, a placed AFormulaTrack in the level takes priority and nothing is spawned.
 */
UCLASS()
class APEXFORMULA_API AAPEXFormulaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAPEXFormulaGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	/** Start slot we spawned for the procedural track; preferred over any PlayerStart already in the map. */
	TWeakObjectPtr<AActor> ProceduralStart;
};
