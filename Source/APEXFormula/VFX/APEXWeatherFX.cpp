#include "VFX/APEXWeatherFX.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Lighting/APEXLightingDirector.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexFX, Log, All);

namespace
{
	constexpr int32 NumRain = 4500;
	constexpr int32 NumDrops = 900;
	constexpr float BoxHalfCm = 3000.f;     // 60 m x 60 m rain box around the camera
}

AAPEXWeatherFX::AAPEXWeatherFX()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	RainISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rain"));
	SprayISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Spray"));
	for (UInstancedStaticMeshComponent* C : { RainISM.Get(), SprayISM.Get() })
	{
		C->SetupAttachment(GetRootComponent());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetMobility(EComponentMobility::Movable);
	}
	SprayISM->SetAbsolute(true, true, true);          // droplets are placed in world space
	RainISM->bUseAsOccluder = false;
}

void AAPEXWeatherFX::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoWeatherFX"))) { SetActorTickEnabled(false); return; }
	FParse::Value(FCommandLine::Get(), TEXT("APEXForceWet="), ForceWet);
	bNoLightning = FParse::Param(FCommandLine::Get(), TEXT("APEXNoLightning"));
	float FirstStrike = 0.f; if (FParse::Value(FCommandLine::Get(), TEXT("APEXLightningAt="), FirstStrike)) { NextStrike = FirstStrike; }
	if (TActorIterator<AAPEXLightingDirector> It(GetWorld()); It) { Director = *It; }

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* RainMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_APEX_Rain.M_APEX_Rain"));
	UMaterialInterface* SprayMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_APEX_Spray.M_APEX_Spray"));
	if (!Cube || !Sphere || !RainMat || !SprayMat) { UE_LOG(LogApexFX, Warning, TEXT("Weather FX assets missing (run Python/ue_weather_materials.py)")); SetActorTickEnabled(false); return; }

	RainISM->SetStaticMesh(Cube); RainMID = UMaterialInstanceDynamic::Create(RainMat, this); RainISM->SetMaterial(0, RainMID);
	SprayISM->SetStaticMesh(Sphere); SprayMID = UMaterialInstanceDynamic::Create(SprayMat, this); SprayISM->SetMaterial(0, SprayMID);

	// Streaks: 1.2 cm x 1.2 cm x 70 cm, scattered on one plane in the box (height is animated in the material).
	for (int32 i = 0; i < NumRain; ++i)
	{
		const FVector Loc(Rand.FRandRange(-BoxHalfCm, BoxHalfCm), Rand.FRandRange(-BoxHalfCm, BoxHalfCm), 0.f);
		RainISM->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), Loc, FVector(0.012f, 0.012f, 0.70f)));
	}
	Drops.SetNum(NumDrops);
	for (int32 i = 0; i < NumDrops; ++i) { SprayISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector::ZeroVector)); }
	UE_LOG(LogApexFX, Log, TEXT("Weather FX ready: %d rain streaks, %d spray droplets"), NumRain, NumDrops);
}

void AAPEXWeatherFX::Emit(const FVector& P, const FVector& CarVel, const FVector& Back)
{
	FDrop& D = Drops[NextDrop]; NextDrop = (NextDrop + 1) % Drops.Num();
	D.Pos = P + FVector(Rand.FRandRange(-8.f, 8.f), Rand.FRandRange(-8.f, 8.f), Rand.FRandRange(0.f, 6.f));
	const float Speed = CarVel.Size();
	// A rooster tail: carried partly by the car's momentum, thrown up and back, with lateral scatter.
	D.Vel = CarVel * 0.55f + Back * Speed * Rand.FRandRange(0.05f, 0.18f) + FVector(Rand.FRandRange(-120.f, 120.f), Rand.FRandRange(-120.f, 120.f), Rand.FRandRange(250.f, 650.f));
	D.Age = 0.f; D.Life = Rand.FRandRange(0.5f, 1.1f); D.Size = Rand.FRandRange(0.05f, 0.13f);
}

void AAPEXWeatherFX::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	Wet = ForceWet >= 0.f ? ForceWet : (Director.IsValid() ? Director->GetWetness() : 0.f);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CamPos = Cam ? Cam->GetCameraLocation() : FVector::ZeroVector;

	// Brightness calibrated like the sky dome: pixel ~ emissive * 2^(bias - 10).
	const float Bias = Director.IsValid() ? Director->GetExposureBias() : 0.f;
	const float Lum = FMath::Pow(2.f, 10.f - Bias);
	if (RainMID) { RainMID->SetScalarParameterValue(TEXT("Luminance"), 0.85f * Lum); RainMID->SetScalarParameterValue(TEXT("Opacity"), 0.32f); }
	if (SprayMID) { SprayMID->SetScalarParameterValue(TEXT("Luminance"), 0.85f * Lum); }

	// Rain: the box follows the camera, snapped so the streak pattern does not slide; the material fades with the Wetness parameter.
	const bool bRain = Wet > 0.4f;
	RainISM->SetVisibility(bRain);
	if (bRain)
	{
		const FVector Snap(FMath::GridSnap(CamPos.X, 100.f), FMath::GridSnap(CamPos.Y, 100.f), CamPos.Z + 300.f);
		RainISM->SetWorldLocation(Snap);
	}

	// Lightning: only in real rain. Exposure flashes up +3.5 / +2 EV in a double flicker (0.5 s), thunder follows 0.5-3 s later.
	if (Director.IsValid() && Wet > 0.6f && !bNoLightning)
	{
		StrikeClock += Dt;
		if (StrikeT < 0.f && StrikeClock >= NextStrike) { StrikeT = 0.f; ThunderIn = Rand.FRandRange(0.5f, 3.f); NextStrike = StrikeClock + Rand.FRandRange(8.f, 22.f); }
		if (StrikeT >= 0.f)
		{
			StrikeT += Dt;
			float EV = 0.f;
			if (StrikeT < 0.08f) { EV = 3.5f; } else if (StrikeT < 0.16f) { EV = 0.4f; } else if (StrikeT < 0.30f) { EV = 2.2f * (1.f - (StrikeT - 0.16f) / 0.14f) + 0.f; }
			Director->SetFlashEV(EV);
			if (StrikeT > 0.30f) { Director->SetFlashEV(0.f); }
			ThunderIn -= Dt;
			if (ThunderIn <= 0.f && ThunderIn > -1.f)
			{
				if (USoundBase* Thunder = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SW_thunder.SW_thunder"))) { UGameplayStatics::PlaySound2D(this, Thunder, Rand.FRandRange(0.7f, 1.0f), Rand.FRandRange(0.85f, 1.1f)); }
				ThunderIn = -10.f;
			}
			if (StrikeT > 0.35f && ThunderIn <= -1.f) { StrikeT = -1.f; }
		}
	}

	// Cars (rescanned once a second).
	Rescan -= Dt;
	if (Rescan <= 0.f)
	{
		Rescan = 1.f; Cars.Reset();
		for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It) { Cars.Add(*It); }
	}

	// Emit from the rear tyres of cars that are fast, close to the camera, and on a wet track.
	if (Wet > 0.25f)
	{
		for (const TWeakObjectPtr<AFormulaVehiclePawn>& W : Cars)
		{
			AFormulaVehiclePawn* C = W.Get();
			if (!C || C->GetSpeedMps() < 14.f || FVector::DistSquared(C->GetActorLocation(), CamPos) > FMath::Square(18000.f)) { continue; }
			const float Rate = 450.f * FMath::Clamp(C->GetSpeedMps() / 70.f, 0.f, 1.f) * Wet;   // droplets per second per car
			const FVector Fwd = C->GetActorForwardVector(), Right = C->GetActorRightVector();
			const FVector Vel = Fwd * C->GetSpeedMps() * 100.f;
			float Budget = Rate * Dt + Rand.FRand();
			while (Budget >= 1.f)
			{
				const float Side = Rand.FRand() < 0.5f ? -85.f : 85.f;
				Emit(C->GetActorLocation() - Fwd * 165.f + Right * Side - FVector(0, 0, 25.f), Vel, -Fwd);
				Budget -= 1.f;
			}
		}
	}

	// Integrate the droplets and write all transforms in one batch.
	TArray<FTransform> T; T.SetNum(Drops.Num());
	for (int32 i = 0; i < Drops.Num(); ++i)
	{
		FDrop& D = Drops[i];
		if (D.Life <= 0.f || D.Age >= D.Life) { T[i] = FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector::ZeroVector); continue; }
		D.Age += Dt;
		D.Vel.Z -= 980.f * Dt;
		D.Vel *= (1.f - 1.6f * Dt);                         // air drag so the cloud hangs behind the car
		D.Pos += D.Vel * Dt;
		const float K = 1.f - D.Age / D.Life;
		T[i] = FTransform(FRotator::ZeroRotator, D.Pos, FVector(D.Size * (0.4f + 1.2f * (1.f - K)) * K + 0.01f));   // grows as it disperses, then fades out
	}
	SprayISM->BatchUpdateInstancesTransforms(0, T, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
}
