#include "Camera/APEXCameraManager.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Broadcast/APEXBroadcastDirector.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexCam, Log, All);

AAPEXCameraManager::AAPEXCameraManager()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

FString AAPEXCameraManager::ViewName(EAPEXCamView V)
{
	switch (V)
	{
	case EAPEXCamView::Chase: return TEXT("CHASE");
	case EAPEXCamView::Cockpit: return TEXT("COCKPIT");
	case EAPEXCamView::FrontWing: return TEXT("FRONT WING");
	case EAPEXCamView::Rear: return TEXT("REAR");
	case EAPEXCamView::Nose: return TEXT("NOSE");
	case EAPEXCamView::Cinematic: return TEXT("CINEMATIC");
	}
	return TEXT("?");
}

EAPEXCamView AAPEXCameraManager::NextView(EAPEXCamView V)
{
	switch (V)
	{
	case EAPEXCamView::Chase: return EAPEXCamView::Cockpit;
	case EAPEXCamView::Cockpit: return EAPEXCamView::FrontWing;
	case EAPEXCamView::FrontWing: return EAPEXCamView::Rear;
	case EAPEXCamView::Rear: return EAPEXCamView::Nose;
	case EAPEXCamView::Nose: return EAPEXCamView::Cinematic;
	default: return EAPEXCamView::Chase;
	}
}

AFormulaVehiclePawn* AAPEXCameraManager::FindPlayerCar() const
{
	for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsAI() && It->GetController() && It->GetController()->IsLocalPlayerController()) { return *It; }
	}
	return nullptr;
}

AAPEXBroadcastDirector* AAPEXCameraManager::FindDirector() const
{
	if (TActorIterator<AAPEXBroadcastDirector> It(GetWorld()); It) { return *It; }
	return nullptr;
}

void AAPEXCameraManager::SetView(EAPEXCamView V)
{
	AFormulaVehiclePawn* Car = FindPlayerCar();
	AAPEXBroadcastDirector* Dir = FindDirector();
	if (!Car) { UE_LOG(LogApexCam, Warning, TEXT("Camera: no player car to follow")); return; }
	Current = V;
	if (V == EAPEXCamView::Cinematic)
	{
		Car->SetCameraView(EAPEXCamView::Chase);
		if (Dir) { Dir->SetMode(EAPEXCamMode::TVPod); }
	}
	else
	{
		if (Dir && Dir->GetMode() != EAPEXCamMode::Chase && !Dir->IsReplaying()) { Dir->SetMode(EAPEXCamMode::Chase); }   // give the view back to the player's car
		Car->SetCameraView(V);
	}
	UE_LOG(LogApexCam, Log, TEXT("Camera view: %s"), *ViewName(V));
	if (GEngine) { GEngine->AddOnScreenDebugMessage(7430, 2.f, FColor::White, FString::Printf(TEXT("CAMERA: %s"), *ViewName(V))); }
}

void AAPEXCameraManager::Cycle()
{
	if (bLookingBack) { return; }
	SetView(NextView(Current));
}

void AAPEXCameraManager::BeginLookBack()
{
	if (bLookingBack) { return; }
	bLookingBack = true; Saved = Current;
	SetView(EAPEXCamView::Rear);
}

void AAPEXCameraManager::EndLookBack()
{
	if (!bLookingBack) { return; }
	bLookingBack = false;
	SetView(Saved);
}

void AAPEXCameraManager::PlayIntro(float Seconds)
{
	IntroRemaining = Seconds;
	SetView(EAPEXCamView::Cinematic);
	UE_LOG(LogApexCam, Log, TEXT("Race intro: %.1f s of broadcast cameras, then the view returns to the player car"), Seconds);
}

void AAPEXCameraManager::Tick(float Dt)
{
	Super::Tick(Dt);
	if (IntroRemaining > 0.f)
	{
		IntroRemaining -= Dt;
		if (IntroRemaining <= 0.f) { SetView(EAPEXCamView::Chase); UE_LOG(LogApexCam, Log, TEXT("Race intro finished: gameplay camera restored")); }
	}
}
