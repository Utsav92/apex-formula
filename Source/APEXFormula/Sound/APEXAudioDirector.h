#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXAudioDirector.generated.h"

class AFormulaVehiclePawn;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;

/**
 * Race audio, all from synthesised loops (Audio/gen_audio.py): the player's engine (low and high layers crossfaded and pitch-shifted by a simple
 * 8-speed rpm model), tyre screech from lateral g, wind from speed, a city bed and a crowd bed that swells near the grandstands, and 3D engine
 * voices for the four nearest AI cars. Command line: -APEXNoAudio disables it.
 */
UCLASS()
class APEXFORMULA_API AAPEXAudioDirector : public AActor
{
	GENERATED_BODY()

public:
	AAPEXAudioDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Pure model, also tested: engine speed 0..1 within the current gear, and the gear (0-based), for a road speed in km/h. */
	static void RevModel(float SpeedKph, float& OutRev01, int32& OutGear);
	/** Pitch and layer volumes for a given rev fraction: x = low pitch, y = high pitch, z = low volume, w = high volume. */
	static FVector4 EngineLayers(float Rev01);

private:
	struct FVoice { UAudioComponent* Low = nullptr; UAudioComponent* High = nullptr; TWeakObjectPtr<AFormulaVehiclePawn> Car; float Rev = 0.2f; };

	UPROPERTY() TObjectPtr<AFormulaVehiclePawn> Player;
	UPROPERTY() TObjectPtr<USoundAttenuation> Atten;
	UPROPERTY() TObjectPtr<UAudioComponent> Tyre;
	UPROPERTY() TObjectPtr<UAudioComponent> Wind;
	UPROPERTY() TObjectPtr<UAudioComponent> City;
	UPROPERTY() TObjectPtr<UAudioComponent> Crowd;
	UPROPERTY() TObjectPtr<USoundBase> ClickWave;
	FVoice PlayerVoice;
	TArray<FVoice> AIVoices;
	bool bEnabled = true;
	float PrevSpeedMps = 0.f, PrevYaw = 0.f, SmoothThrottle = 0.f, SmoothSlip = 0.f, ReassignTimer = 0.f;
	int32 PrevGear = 0;

	UAudioComponent* Spawn2D(const TCHAR* Asset, bool bAutoPlay = true);
	UAudioComponent* Spawn3D(const TCHAR* Asset);
	void FindPlayer();
	void UpdateVoice(FVoice& V, float Rev, float Throttle, float BaseVolume);
	void AssignAIVoices();
};
