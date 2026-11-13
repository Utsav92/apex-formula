#include "Sound/APEXAudioDirector.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

namespace
{
	// Road speed (km/h) at the bottom of each gear; the top gear ends near the car's 320 km/h.
	constexpr float GearLo[] = { 0.f, 55.f, 95.f, 135.f, 180.f, 225.f, 270.f, 305.f };
	constexpr float GearHi[] = { 55.f, 95.f, 135.f, 180.f, 225.f, 270.f, 305.f, 340.f };
}

AAPEXAudioDirector::AAPEXAudioDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AAPEXAudioDirector::RevModel(float SpeedKph, float& OutRev01, int32& OutGear)
{
	int32 G = 0;
	for (int32 i = 0; i < 8; ++i) { if (SpeedKph >= GearLo[i]) { G = i; } }
	OutGear = G;
	const float T = FMath::Clamp((SpeedKph - GearLo[G]) / (GearHi[G] - GearLo[G]), 0.f, 1.f);
	OutRev01 = FMath::Lerp(0.30f, 1.0f, T);                         // shifts drop the revs to 30%
	if (SpeedKph < 8.f) { OutRev01 = FMath::Lerp(0.12f, 0.30f, SpeedKph / 8.f); }   // idle to first-gear pull-away
}

FVector4 AAPEXAudioDirector::EngineLayers(float Rev01)
{
	const float R = FMath::Clamp(Rev01, 0.f, 1.f);
	const float X = FMath::SmoothStep(0.25f, 0.85f, R);             // 0 = low layer only, 1 = high layer only
	return FVector4(0.75f + 1.9f * R, 0.60f + 1.1f * R, FMath::Cos(X * HALF_PI), FMath::Sin(X * HALF_PI));   // equal-power crossfade
}

UAudioComponent* AAPEXAudioDirector::Spawn2D(const TCHAR* Asset, bool bAutoPlay)
{
	USoundBase* S = LoadObject<USoundBase>(nullptr, Asset);
	if (!S) { UE_LOG(LogTemp, Warning, TEXT("Audio: %s missing (run Python/ue_audio.py)"), Asset); return nullptr; }
	UAudioComponent* C = UGameplayStatics::SpawnSound2D(GetWorld(), S, 0.f, 1.f, 0.f, nullptr, true, false);
	if (C && !bAutoPlay) { C->Stop(); }
	return C;
}

UAudioComponent* AAPEXAudioDirector::Spawn3D(const TCHAR* Asset)
{
	USoundBase* S = LoadObject<USoundBase>(nullptr, Asset);
	if (!S) { return nullptr; }
	return UGameplayStatics::SpawnSoundAtLocation(GetWorld(), S, FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, 0.f, 1.f, 0.f, Atten, nullptr, false);
}

void AAPEXAudioDirector::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoAudio"))) { bEnabled = false; SetActorTickEnabled(false); return; }
	Atten = NewObject<USoundAttenuation>(this);
	Atten->Attenuation.bAttenuate = true;
	Atten->Attenuation.bSpatialize = true;
	Atten->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Atten->Attenuation.AttenuationShapeExtents = FVector(6000.f, 0.f, 0.f);       // full volume inside 60 m
	Atten->Attenuation.FalloffDistance = 30000.f;                                  // gone by ~360 m
	Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
	PlayerVoice.Low = Spawn2D(TEXT("/Game/Audio/SW_engine_low.SW_engine_low"));
	PlayerVoice.High = Spawn2D(TEXT("/Game/Audio/SW_engine_high.SW_engine_high"));
	Tyre = Spawn2D(TEXT("/Game/Audio/SW_tyre_screech.SW_tyre_screech"));
	Wind = Spawn2D(TEXT("/Game/Audio/SW_wind.SW_wind"));
	City = Spawn2D(TEXT("/Game/Audio/SW_city.SW_city"));
	Crowd = Spawn2D(TEXT("/Game/Audio/SW_crowd.SW_crowd"));
	ClickWave = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SW_gear_click.SW_gear_click"));
	for (int32 i = 0; i < 4; ++i)
	{
		FVoice V; V.Low = Spawn3D(TEXT("/Game/Audio/SW_engine_low.SW_engine_low")); V.High = Spawn3D(TEXT("/Game/Audio/SW_engine_high.SW_engine_high"));
		if (V.Low && V.High) { V.Low->SetVolumeMultiplier(0.f); V.High->SetVolumeMultiplier(0.f); AIVoices.Add(V); }
	}
}

void AAPEXAudioDirector::FindPlayer()
{
	if (Player) { return; }
	for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsAI() && It->GetController() && It->GetController()->IsLocalPlayerController()) { Player = *It; PrevYaw = It->GetActorRotation().Yaw; break; }
	}
}

void AAPEXAudioDirector::UpdateVoice(FVoice& V, float Rev, float Throttle, float BaseVolume)
{
	const FVector4 L = EngineLayers(Rev);
	const float Vol = BaseVolume * (0.45f + 0.55f * Throttle);
	if (V.Low) { V.Low->SetPitchMultiplier(L.X); V.Low->SetVolumeMultiplier(Vol * L.Z); }
	if (V.High) { V.High->SetPitchMultiplier(L.Y); V.High->SetVolumeMultiplier(Vol * L.W); }
}

void AAPEXAudioDirector::AssignAIVoices()
{
	TArray<AFormulaVehiclePawn*> Near;
	const FVector P = Player->GetActorLocation();
	for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (*It != Player.Get() && It->IsAI()) { Near.Add(*It); }
	}
	Near.Sort([&P](const AFormulaVehiclePawn& A, const AFormulaVehiclePawn& B) { return FVector::DistSquared(A.GetActorLocation(), P) < FVector::DistSquared(B.GetActorLocation(), P); });
	for (int32 i = 0; i < AIVoices.Num(); ++i) { AIVoices[i].Car = Near.IsValidIndex(i) ? Near[i] : nullptr; }
}

void AAPEXAudioDirector::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bEnabled || Dt <= 0.f) { return; }
	FindPlayer();
	if (!Player) { return; }

	const float V = Player->GetSpeedMps();
	const float Kph = V * 3.6f;
	float Rev; int32 Gear;
	RevModel(Kph, Rev, Gear);
	if (Player->IsHeldOnGrid()) { Rev = Player->GetRev01(); }                // revving on the grid before the lights go out
	const float Accel = (V - PrevSpeedMps) / Dt;
	PrevSpeedMps = V;
	SmoothThrottle = FMath::FInterpTo(SmoothThrottle, Accel > 0.5f ? 1.f : (Accel < -3.f ? 0.f : 0.35f), Dt, 6.f);
	PlayerVoice.Rev = FMath::FInterpTo(PlayerVoice.Rev, Rev, Dt, 14.f);
	UpdateVoice(PlayerVoice, PlayerVoice.Rev, SmoothThrottle, 0.9f);

	if (Gear != PrevGear)
	{
		if (Gear > PrevGear && ClickWave) { UGameplayStatics::PlaySound2D(GetWorld(), ClickWave, 0.35f); }
		PlayerVoice.Rev = Gear > PrevGear ? FMath::Min(PlayerVoice.Rev, 0.7f) : PlayerVoice.Rev;
		PrevGear = Gear;
	}

	const float Yaw = Player->GetActorRotation().Yaw;
	const float YawRate = FMath::Abs(FMath::FindDeltaAngleDegrees(PrevYaw, Yaw)) / Dt;
	PrevYaw = Yaw;
	const float LatG = FMath::DegreesToRadians(YawRate) * V / 981.f * 100.f;         // v (m/s) * rate (rad/s) / g
	const float Slip = FMath::Clamp((LatG - 2.2f) / 2.5f, 0.f, 1.f) + FMath::Clamp((-Accel - 20.f) / 20.f, 0.f, 0.6f);
	SmoothSlip = FMath::FInterpTo(SmoothSlip, Slip, Dt, 10.f);
	if (Tyre) { Tyre->SetVolumeMultiplier(SmoothSlip * 0.55f); Tyre->SetPitchMultiplier(0.85f + 0.25f * SmoothSlip); }
	if (Wind) { const float S = FMath::Clamp(V / 90.f, 0.f, 1.f); Wind->SetVolumeMultiplier(S * S * 0.55f); Wind->SetPitchMultiplier(0.8f + 0.5f * S); }
	if (City) { City->SetVolumeMultiplier(0.30f); }
	if (Crowd)
	{
		// the grandstands sit along the start straight, x = -10 m .. +120 m: swell when the car is near them
		const FVector L = Player->GetActorLocation();
		const float D = FMath::Max(0.f, FVector2D(L.X < 500.f ? 500.f - L.X : (L.X > 12000.f ? L.X - 12000.f : 0.f), L.Y).Size());
		Crowd->SetVolumeMultiplier(0.12f + 0.55f * FMath::Clamp(1.f - D / 40000.f, 0.f, 1.f));
	}

	ReassignTimer -= Dt;
	if (ReassignTimer <= 0.f) { ReassignTimer = 0.25f; AssignAIVoices(); }
	for (FVoice& A : AIVoices)
	{
		AFormulaVehiclePawn* C = A.Car.Get();
		if (!C) { UpdateVoice(A, 0.f, 0.f, 0.f); continue; }
		float R; int32 G; RevModel(C->GetSpeedMps() * 3.6f, R, G);
		A.Rev = FMath::FInterpTo(A.Rev, R, Dt, 8.f);
		const FVector Loc = C->GetActorLocation();
		if (A.Low) { A.Low->SetWorldLocation(Loc); }
		if (A.High) { A.High->SetWorldLocation(Loc); }
		UpdateVoice(A, A.Rev, 0.8f, 0.75f);
	}
}
