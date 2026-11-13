#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vehicles/APEXDrivingTypes.h"
#include "APEXCameraManager.generated.h"

class AFormulaVehiclePawn;
class AAPEXBroadcastDirector;

/**
 * Owns the player's gameplay camera selection. The views live on the car (chase, cockpit, front wing, rear, nose); Cinematic hands the view to the
 * broadcast director (TV pods). C cycles, R shows Rear while held and restores the previous view on release. Gameplay camera ownership always
 * returns to the player's car: leaving Cinematic puts the director back in Chase (= not owning the view).
 */
UCLASS()
class APEXFORMULA_API AAPEXCameraManager : public AActor
{
	GENERATED_BODY()

public:
	AAPEXCameraManager();
	virtual void Tick(float Dt) override;
	/** Race introduction: the broadcast cameras fly over the circuit for Seconds, then the view returns to the player car. */
	void PlayIntro(float Seconds);
	bool IsIntroPlaying() const { return IntroRemaining > 0.f; }

	void Cycle();
	void SetView(EAPEXCamView View);
	void BeginLookBack();
	void EndLookBack();
	EAPEXCamView GetView() const { return Current; }
	static FString ViewName(EAPEXCamView View);
	/** The view after this one in the C cycle (pure, tested). */
	static EAPEXCamView NextView(EAPEXCamView View);

private:
	EAPEXCamView Current = EAPEXCamView::Chase, Saved = EAPEXCamView::Chase;
	bool bLookingBack = false;
	float IntroRemaining = 0.f;
	AFormulaVehiclePawn* FindPlayerCar() const;
	AAPEXBroadcastDirector* FindDirector() const;
};
