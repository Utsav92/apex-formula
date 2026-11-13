#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXBroadcastDirector.generated.h"

class UCameraComponent;
class AFormulaVehiclePawn;
class AStaticMeshActor;

UENUM()
enum class EAPEXCamMode : uint8 { Chase, TVPod, Helicopter };

/**
 * TV-style camera direction. Keys: T plays an instant replay (the C / R camera keys now belong to AAPEXCameraManager) of the player's last 10 s
 * (shown on a ghost car, the race keeps running underneath). Command line: -APEXCamMode=0|1|2, -APEXReplayAt=<seconds>.
 * The TV view cuts between trackside pods placed about every 70 m along the lap, always the nearest one (with hysteresis), zooming to keep the
 * car at a similar size; the helicopter circles 150 m above and 240 m out.
 */
UCLASS()
class APEXFORMULA_API AAPEXBroadcastDirector : public AActor
{
	GENERATED_BODY()

public:
	AAPEXBroadcastDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	EAPEXCamMode GetMode() const { return Mode; }
	bool IsReplaying() const { return bReplaying; }
	int32 GetPodCount() const { return Pods.Num(); }
	int32 GetCurrentPod() const { return CurPod; }

	void SetMode(EAPEXCamMode M);
	bool StartReplay();
	void StopReplay();

	/** Pure helper (also tested): index of the pod to use, keeping the current one unless another is clearly closer. */
	static int32 ChoosePod(const TArray<FVector>& Pods, const FVector& Target, int32 Current, float Hysteresis = 0.7f);

private:
	UPROPERTY() TObjectPtr<UCameraComponent> Cam;
	UPROPERTY() TObjectPtr<AFormulaVehiclePawn> Player;
	UPROPERTY() TObjectPtr<AStaticMeshActor> Ghost;
	TArray<FVector> Pods;
	int32 CurPod = -1;
	EAPEXCamMode Mode = EAPEXCamMode::Chase, ModeBeforeReplay = EAPEXCamMode::Chase;
	float HeliAngle = 0.f;
	float BaseFov = 0.f;
	float ProfileAt = -1.f;
	bool bViewOwned = false;

	struct FSample { float T; FTransform X; };
	TArray<FSample> History;
	float Clock = 0.f, ReplayT = 0.f, ReplayEnd = 0.f, ReplayAtCmd = -1.f;
	bool bReplaying = false, bReplayCmdDone = false;
	FTransform VisualRel, WheelRel[4];
	TArray<TObjectPtr<UStaticMeshComponent>> GhostWheels;

	void FindPlayer();
	void BuildPods();
	void BuildGhost();
	void Record();
	FTransform SampleAt(float T) const;
	void ApplyView(bool bWantOwned);
	void Say(const FString& S) const;
};
