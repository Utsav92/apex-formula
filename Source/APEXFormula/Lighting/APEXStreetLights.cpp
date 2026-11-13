#include "Lighting/APEXStreetLights.h"
#include "Lighting/APEXLightingDirector.h"
#include "Racing/FormulaTrack.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"

namespace
{
	constexpr int32 NumLights = 28;
}

AAPEXStreetLights::AAPEXStreetLights()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

float AAPEXStreetLights::NightAmount(float ExposureBias)
{
	return FMath::SmoothStep(1.0f, 4.0f, ExposureBias);
}

void AAPEXStreetLights::NearestHeads(const TArray<FVector>& Heads, const FVector& Point, int32 Count, TArray<int32>& Out)
{
	TArray<TPair<float, int32>> D;
	D.Reserve(Heads.Num());
	for (int32 i = 0; i < Heads.Num(); ++i) { D.Add({ FVector::DistSquared(Heads[i], Point), i }); }
	const int32 N = FMath::Min(Count, D.Num());
	if (N <= 0) { Out.Reset(); return; }
	D.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	Out.Reset();
	for (int32 i = 0; i < N; ++i) { Out.Add(D[i].Value); }
}

void AAPEXStreetLights::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoStreetLights"))) { SetActorTickEnabled(false); return; }
	if (TActorIterator<AAPEXLightingDirector> It(GetWorld()); It) { Director = *It; }
	if (TActorIterator<AFormulaTrack> It(GetWorld()); It) { Track = *It; }
	for (int32 i = 0; i < NumLights; ++i)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(GetRootComponent());
		L->SetLightColor(FLinearColor(1.0f, 0.78f, 0.52f));        // warm sodium-ish LED
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.f);
		L->SetAttenuationRadius(2600.f);
		L->SetSourceRadius(18.f);
		L->SetCastShadows(false);
		L->RegisterComponent();
		Lights.Add(L);
	}
}

void AAPEXStreetLights::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Track || Lights.Num() == 0) { return; }
	Night = FMath::FInterpTo(Night, NightAmount(Director.IsValid() ? Director->GetExposureBias() : 0.f), Dt, 1.5f);
	Timer -= Dt;
	if (Timer > 0.f) { return; }
	Timer = 0.25f;
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) { return; }
	TArray<int32> Near;
	NearestHeads(Track->LampHeads, Cam->GetCameraLocation(), Lights.Num(), Near);
	for (int32 i = 0; i < Lights.Num(); ++i)
	{
		if (!Near.IsValidIndex(i) || Night < 0.02f) { Lights[i]->SetIntensity(0.f); continue; }
		Lights[i]->SetWorldLocation(Track->LampHeads[Near[i]] - FVector(0.f, 0.f, 30.f));
		Lights[i]->SetIntensity(9000.f * Night);                   // candelas: roughly a 14,000 lm street luminaire
	}
}
