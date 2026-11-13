#include "Core/APEXFormulaGameMode.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Racing/FormulaTrack.h"
#include "UI/FormulaHUD.h"
#include "Lighting/APEXLightingDirector.h"
#include "Racing/APEXRaceManager.h"
#include "VFX/APEXWeatherFX.h"
#include "Broadcast/APEXBroadcastDirector.h"
#include "Sound/APEXAudioDirector.h"
#include "VFX/APEXCityFX.h"
#include "Lighting/APEXStreetLights.h"
#include "Crowd/APEXNPCCrowd.h"
#include "Traffic/APEXTraffic.h"
#include "Input/APEXPlayerController.h"
#include "Camera/APEXCameraManager.h"
#include "Sponsors/APEXAdvertisingManager.h"
#include "Cinematic/APEXCinematicDirector.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	/** Lighting is owned by AAPEXLightingDirector. A director already in the map wins. */
	void SetupEnvironment(UWorld* World)
	{
		if (!TActorIterator<AAPEXLightingDirector>(World)) { World->SpawnActor<AAPEXLightingDirector>(FVector::ZeroVector, FRotator::ZeroRotator); }
		if (!TActorIterator<AAPEXWeatherFX>(World)) { World->SpawnActor<AAPEXWeatherFX>(FVector::ZeroVector, FRotator::ZeroRotator); }   // rain + wheel spray
		if (!TActorIterator<AAPEXBroadcastDirector>(World)) { World->SpawnActor<AAPEXBroadcastDirector>(FVector::ZeroVector, FRotator::ZeroRotator); }   // TV cameras + replay
		if (!TActorIterator<AAPEXAudioDirector>(World)) { World->SpawnActor<AAPEXAudioDirector>(FVector::ZeroVector, FRotator::ZeroRotator); }   // engines, tyres, wind, city, crowd
		if (!TActorIterator<AAPEXCityFX>(World)) { World->SpawnActor<AAPEXCityFX>(FVector::ZeroVector, FRotator::ZeroRotator); }   // steam stacks + sparks
		if (!TActorIterator<AAPEXStreetLights>(World)) { World->SpawnActor<AAPEXStreetLights>(FVector::ZeroVector, FRotator::ZeroRotator); }   // pooled real lights on the lamps
		if (!TActorIterator<AAPEXNPCCrowd>(World)) { World->SpawnActor<AAPEXNPCCrowd>(FVector::ZeroVector, FRotator::ZeroRotator); }   // skeletal NPCs near the camera
		if (!TActorIterator<AAPEXTraffic>(World)) { World->SpawnActor<AAPEXTraffic>(FVector::ZeroVector, FRotator::ZeroRotator); }   // ambient city traffic
		if (!TActorIterator<AAPEXCameraManager>(World)) { World->SpawnActor<AAPEXCameraManager>(FVector::ZeroVector, FRotator::ZeroRotator); }   // gameplay camera selection
		if (!TActorIterator<AAPEXAdvertisingManager>(World)) { World->SpawnActor<AAPEXAdvertisingManager>(FVector::ZeroVector, FRotator::ZeroRotator); }   // sponsor registry, boards, gantries
		if (FParse::Param(FCommandLine::Get(), TEXT("APEXCinematic")) || FCommandLine::Get() == FCommandLine::Get() && FString(FCommandLine::Get()).Contains(TEXT("-APEXCinematic=")))
		{ if (!TActorIterator<AAPEXCinematicDirector>(World)) { World->SpawnActor<AAPEXCinematicDirector>(FVector::ZeroVector, FRotator::ZeroRotator); } }
	}
}

AAPEXFormulaGameMode::AAPEXFormulaGameMode()
{
	DefaultPawnClass = AFormulaVehiclePawn::StaticClass();
	PlayerControllerClass = AAPEXPlayerController::StaticClass();   // Enhanced Input driving, pause, cameras
	HUDClass = AFormulaHUD::StaticClass();
}

AActor* AAPEXFormulaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (ProceduralStart.IsValid())
	{
		return ProceduralStart.Get();
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AAPEXFormulaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UWorld* World = GetWorld();
	if (!World) { return; }

	// A track placed in the level wins; otherwise build the procedural test circuit.
	AFormulaTrack* Track = nullptr;
	if (TActorIterator<AFormulaTrack> It(World); It) { Track = *It; }
	if (!Track)
	{
		Track = World->SpawnActor<AFormulaTrack>(FVector::ZeroVector, FRotator::ZeroRotator);

		// We built the track, so we also own the start slot. A map may already contain a PlayerStart
		// (the engine's empty Entry map does, at the origin), and that would put the car off the road.
		if (Track)
		{
			// With the grid, the player starts in their grid slot; -APEXNoGrid keeps the old single-car start.
			const bool bGrid = !FParse::Param(FCommandLine::Get(), TEXT("APEXNoGrid"));
			const FTransform Start = bGrid ? Track->GetGridTransform(AAPEXRaceManager::ParsePlayerSlotFromCommandLine(12)) : Track->GetStartTransform();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ProceduralStart = World->SpawnActor<APlayerStart>(Start.GetLocation(), Start.Rotator(), Params);
		}
	}
	if (!Track) { return; }

	SetupEnvironment(World);
	if (!FParse::Param(FCommandLine::Get(), TEXT("APEXNoGrid")) && !TActorIterator<AAPEXRaceManager>(World))
	{
		World->SpawnActor<AAPEXRaceManager>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
}
