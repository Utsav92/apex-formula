#include "Cinematic/APEXCinematicDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Lighting/APEXLightingDirector.h"
#include "Racing/APEXRaceManager.h"
#include "Racing/FormulaTrack.h"
#include "Vehicles/FormulaVehiclePawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexCine, Log, All);

int32 FAPEXCineMath::ShotAt(const TArray<FAPEXShot>& Shots, float T)
{
	for (int32 i = 0; i < Shots.Num(); ++i) { if (T >= Shots[i].Start && T < Shots[i].Start + Shots[i].Duration) { return i; } }
	return INDEX_NONE;
}

bool FAPEXCineMath::Contiguous(const TArray<FAPEXShot>& Shots, float Total, float Tol)
{
	float T = 0.f;
	for (const FAPEXShot& S : Shots) { if (FMath::Abs(S.Start - T) > Tol || S.Duration <= 0.f) { return false; } T += S.Duration; }
	return FMath::Abs(T - Total) <= Tol;
}

AAPEXCinematicDirector::AAPEXCinematicDirector() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork; }

bool AAPEXCinematicDirector::LoadShots(const FString& Path, TArray<FAPEXShot>& Out, FString& Error)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { Error = FString::Printf(TEXT("cannot read %s"), *Path); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { Error = TEXT("invalid JSON"); return false; }
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("shots"), Arr)) { Error = TEXT("no shots array"); return false; }
	Out.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O) { continue; }
		FAPEXShot S;
		S.Name = O->GetStringField(TEXT("name")); S.Start = O->GetNumberField(TEXT("start")); S.Duration = O->GetNumberField(TEXT("duration"));
		S.Scene = O->GetIntegerField(TEXT("scene")); S.Preset = O->GetStringField(TEXT("preset")); S.Camera = O->GetStringField(TEXT("camera"));
		S.Rank = O->GetIntegerField(TEXT("rank")); S.Fov = O->GetNumberField(TEXT("fov")); S.bOverlay = O->GetBoolField(TEXT("overlay"));
		const TArray<TSharedPtr<FJsonValue>>* P; if (O->TryGetArrayField(TEXT("params"), P)) { for (const auto& X : *P) { S.P.Add(X->AsNumber()); } }
		Out.Add(S);
	}
	return Out.Num() > 0;
}

AFormulaVehiclePawn* AAPEXCinematicDirector::CarAt(int32 Rank) const
{
	const TActorIterator<AAPEXRaceManager> It(GetWorld());
	return It ? It->CarAtPosition(Rank) : nullptr;
}

void AAPEXCinematicDirector::BeginPlay()
{
	Super::BeginPlay();
	FString Path = FPaths::ProjectDir() / TEXT("Cinematics/shotlist.json");
	FString Given; if (FParse::Value(FCommandLine::Get(), TEXT("APEXCinematic="), Given)) { Path = Given; }
	FString Err;
	if (!LoadShots(Path, Shots, Err)) { UE_LOG(LogApexCine, Error, TEXT("Shot list: %s"), *Err); return; }
	if (!FAPEXCineMath::Contiguous(Shots, 120.f)) { UE_LOG(LogApexCine, Error, TEXT("Shot list does not tile 0-120 s")); }
	FParse::Value(FCommandLine::Get(), TEXT("APEXCinematicFrom="), FromT); FParse::Value(FCommandLine::Get(), TEXT("APEXCinematicTo="), ToT);
	FParse::Value(FCommandLine::Get(), TEXT("APEXCinematicCapture="), CaptureDir);
	FParse::Value(FCommandLine::Get(), TEXT("APEXCinematicRes="), HighRes);
	if (!CaptureDir.IsEmpty()) { FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*CaptureDir); }
	Clock = FromT;
	if (GEngine) { GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0")); }   // film is captured with no motion blur (requested): sharp, consistent frames
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Cam = GetWorld()->SpawnActor<ACineCameraActor>(FVector(0, 0, 5000), FRotator::ZeroRotator, P);
	for (TActorIterator<AAPEXRaceManager> It(GetWorld()); It; ++It) { It->CountdownSeconds = FMath::Max(30.f - FromT, 6.f); It->bThrottleDistantAI = false; }   // every car steps at the full frame rate: distant cars ticking at 15-30 Hz judder when filmed at 60 fps   // lights out at the 30 s mark of the film
	UE_LOG(LogApexCine, Log, TEXT("Cinematic: %d shots from %s, %.1f-%.1f s, capture dir '%s'"), Shots.Num(), *Path, FromT, ToT, *CaptureDir);
}

void AAPEXCinematicDirector::BeginShot(int32 Idx)
{
	CurShot = Idx; ShotClock = 0.f;
	const FAPEXShot& S = Shots[Idx];
	Target = CarAt(S.Rank);
	for (TActorIterator<AAPEXLightingDirector> L(GetWorld()); L; ++L)
	{
		EAPEXLightingPreset Pr = S.Preset == TEXT("GoldenHour") ? EAPEXLightingPreset::GoldenHour : S.Preset == TEXT("RainyRace") ? EAPEXLightingPreset::RainyRace : S.Preset == TEXT("NightRace") ? EAPEXLightingPreset::NightRace : EAPEXLightingPreset::SunnyRace;
		static int32 LastPreset = -1;
		if (LastPreset != static_cast<int32>(Pr)) { L->ApplyPreset(Pr); LastPreset = static_cast<int32>(Pr); }
	}
	if (UCineCameraComponent* C = Cam ? Cast<ACineCameraActor>(Cam)->GetCineCameraComponent() : nullptr)
	{
		C->SetFieldOfView(S.Fov);
	}
	// Telephoto / fixed cameras plant themselves once, ahead of the car.
	if (Target.IsValid())
	{
		const FVector F = Target->GetActorForwardVector(), R = Target->GetActorRightVector();
		const float Ahead = S.P.IsValidIndex(0) ? S.P[0] : 3000.f, Side = S.P.IsValidIndex(1) ? S.P[1] : 300.f, H = S.P.IsValidIndex(2) ? S.P[2] : 160.f;
		ShotAnchor = Target->GetActorLocation() + F * Ahead + R * Side; ShotAnchor.Z = H + 60.f;
		if (S.Camera == TEXT("tele") && S.P.IsValidIndex(1)) { ShotAnchor.Z = S.P[1] > 0 ? 160.f + S.P[1] * 0.2f : 160.f; }
	}
	UE_LOG(LogApexCine, Log, TEXT("t=%.2f shot %d '%s' scene %d cam %s rank %d car %s"), Clock, Idx, *S.Name, S.Scene, *S.Camera, S.Rank, Target.IsValid() ? *Target->GetName() : TEXT("none"));
}

void AAPEXCinematicDirector::UpdateCamera(const FAPEXShot& S, float Dt)
{
	AFormulaVehiclePawn* Car = Target.Get();
	if (!Car) { Car = CarAt(1); Target = Car; }
	if (!Car) { return; }
	const FVector CP = Car->GetActorLocation(), F = Car->GetActorForwardVector().GetSafeNormal2D(), R = Car->GetActorRightVector().GetSafeNormal2D();
	const float A = FMath::Clamp(ShotClock / FMath::Max(S.Duration, 0.01f), 0.f, 1.f), E = FAPEXCineMath::EaseInOut(A);
	auto P = [&S](int32 i, float D) { return S.P.IsValidIndex(i) ? S.P[i] : D; };
	FVector Want = CP, Look = CP + FVector(0, 0, 60); float Half = 0.12f;
	const FString& K = S.Camera;
	if (K == TEXT("aerial_descend"))
	{
		FVector Start(0, 0, 0); for (TActorIterator<AFormulaTrack> It(GetWorld()); It; ++It) { const TArray<FVector> L = It->GetCenterline(); if (L.Num()) { Start = L[0]; } }
		Want = FVector(Start.X - 9000.f - 30000.f * (1.f - E), Start.Y + 5000.f + 26000.f * (1.f - E), FMath::Lerp(P(0, 90000), P(1, 9000), E)); Look = Start + FVector(2000, 0, 0) * E; Half = 0.02f;
	}
	else if (K == TEXT("heli")) { const float Ang = 0.6f + ShotClock * 0.22f; Want = CP + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0) * P(1, 3000) + FVector(0, 0, P(0, 2400)); Half = 0.03f; }
	else if (K == TEXT("orbit")) { const float Ang = FMath::DegreesToRadians(150.f + ShotClock * 11.f * (P(2, 12) < 0 ? -1.f : 1.f)); Want = CP + (F * FMath::Cos(Ang) + R * FMath::Sin(Ang)) * P(0, 400) + FVector(0, 0, P(1, 100)); Half = 0.02f; }
	else if (K == TEXT("side_track")) { Want = CP + R * P(0, 400) * 0.7f + F * (P(1, 100) - 120.f * E) + FVector(0, 0, 55); Half = 0.03f; }
	else if (K == TEXT("chase")) { Want = CP - F * P(0, 700) * 0.62f + FVector(0, 0, P(1, 160) * 0.7f); Look = CP + F * 600.f + FVector(0, 0, 50); Half = 0.03f; }
	else if (K == TEXT("cockpit")) { Want = CP + F * 20.f + FVector(0, 0, 105); Look = CP + F * 2500.f + FVector(0, 0, 90); Half = 0.015f; }
	else if (K == TEXT("front_wing")) { Want = CP + F * P(0, 330) + FVector(0, 0, P(1, 50)); Look = CP + FVector(0, 0, 40); Half = 0.015f; }
	else if (K == TEXT("rear_cam")) { Want = CP - F * 160.f + FVector(0, 0, 130); Look = CP - F * 3000.f + FVector(0, 0, 80); Half = 0.02f; }
	else if (K == TEXT("wheel")) { Want = CP + F * 130.f + R * P(0, 150) + FVector(0, 0, P(1, 35)); Look = CP + F * 130.f + R * 40.f + FVector(0, 0, 30); Half = 0.02f; }
	else if (K == TEXT("helmet")) { Want = CP + F * P(0, 200) + R * 60.f + FVector(0, 0, 105); Look = CP + FVector(0, 0, 100); Half = 0.02f; }
	else if (K == TEXT("tele")) { Want = ShotAnchor; Half = 0.001f; }
	else if (K == TEXT("grid_crane")) { Want = CP + F * P(0, 3500) * (0.8f - 0.9f * E) + R * 900.f + FVector(0, 0, P(1, 2200) * (1.f - 0.45f * E)); Look = CP + F * 500.f; Half = 0.02f; }
	else if (K == TEXT("start_lights")) { Want = CP - F * 3500.f + R * 100.f + FVector(0, 0, 450); Look = CP + F * 4000.f + FVector(0, 0, 600); Half = 0.04f; }
	else if (K == TEXT("pit_lane") || K == TEXT("pit_overhead") || K == TEXT("pit_wall") || K == TEXT("pit_side") || K == TEXT("pit_wheel"))
	{
		if (K == TEXT("pit_overhead")) { Want = CP + FVector(0, 0, P(0, 1800)); Look = CP; Half = 0.04f; }
		else if (K == TEXT("pit_wheel")) { Want = CP + R * P(0, 260) + F * 140.f + FVector(0, 0, P(1, 80)); Look = CP + F * 130.f + FVector(0, 0, 30); Half = 0.02f; }
		else if (K == TEXT("pit_side")) { Want = CP + R * P(0, 900) + FVector(0, 0, P(1, 130)); Look = CP + FVector(0, 0, 50); Half = 0.02f; }
		else { Want = CP + R * P(0, 2000) - F * 1500.f + FVector(0, 0, P(1, 170)); Look = CP; Half = 0.03f; }
	}
	// Every camera is rigidly mounted to the car (or planted in the world): no damped follow. A damped follow lags a car doing 80 m/s by metres, which shows
	// up as the car sliding in the frame, uneven motion blur and "delay". The car transform itself is stepped at the full 60 Hz (see BeginPlay).
	Pos = Want;
	Rot = (Look - Pos).Rotation();
	// Keep the lens out of buildings: nothing moves a camera below the road surface.
	Pos.Z = FMath::Max(Pos.Z, 25.f);
	Cam->SetActorLocationAndRotation(Pos, Rot);
}

void AAPEXCinematicDirector::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Shots.Num() == 0 || !Cam) { return; }
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->GetViewTarget() != Cam) { PC->SetViewTarget(Cam); }
	const float Step = FApp::UseFixedTimeStep() ? static_cast<float>(FApp::GetFixedDeltaTime()) : Dt;
	// Pit scene: a front-runner is asked to stop shortly before the scene so the real pit logic is on screen.
	if (!bPitAsked && Clock >= 80.f) { bPitAsked = true; if (AFormulaVehiclePawn* C = CarAt(1)) { C->RequestPitNow(); } }
	const bool bWarm = Frame < 40;
	if (Frame == 5)   // film-only performance boost so the race reads as fast: more power and traction (faster launches and straights); grip/cornering limits are unchanged
	{
		for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It) { It->Setup.PowerW *= 1.4f; It->Setup.MaxTractionForceN *= 1.3f; It->Setup.TopSpeedMps = FMath::Max(It->Setup.TopSpeedMps, 105.f);
			It->Setup.TyreMu *= 1.4f; It->Setup.MaxBrakeForceN *= 1.25f; It->ScalePaceFactor(1.18f); }   // more cornering grip and braking: the AI plans corners with sqrt(grip) so corner speeds rise ~18%
	}
	const int32 Idx = FAPEXCineMath::ShotAt(Shots, Clock);
	if (Idx != INDEX_NONE && Idx != CurShot) { BeginShot(Idx); }
	if (Idx != INDEX_NONE) { UpdateCamera(Shots[Idx], Step); if (!bWarm) { ShotClock += Step; } }
	if (!CaptureDir.IsEmpty() && !bWarm && Clock >= FromT && Idx != INDEX_NONE)
	{
		const int32 FrameNo = FMath::RoundToInt(Clock * 60.f);
		if (HighRes.IsEmpty()) { FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/frame_%05d.png"), *CaptureDir, FrameNo), /*bShowUI=*/false, /*bAddFilenameSuffix=*/false); }
		else if (GEngine) { GEngine->Exec(GetWorld(), *FString::Printf(TEXT("HighResShot %s filename=\"%s/frame_%05d.png\""), *HighRes, *CaptureDir, FrameNo)); }   // renders off-screen at this size, independent of the window
		++CapturedFrames;
	}
	static float SpeedLogAt = -1.f;
	if (!bWarm && Clock >= SpeedLogAt) { SpeedLogAt = Clock + 5.f; if (AFormulaVehiclePawn* L = CarAt(1)) { UE_LOG(LogApexCine, Log, TEXT("t=%.0f s leader speed %.0f km/h (top-speed setting %.0f km/h)"), Clock, L->GetSpeedKph(), L->Setup.TopSpeedMps * 3.6f); } }
	++Frame; if (bWarm) { return; }   // the film clock holds during warm-up; the race keeps simulating
	Clock += Step;
	if (Clock >= ToT)
	{
		UE_LOG(LogApexCine, Log, TEXT("Cinematic finished: %d frames requested"), CapturedFrames);
		if (FApp::IsUnattended() || !CaptureDir.IsEmpty()) { FPlatformMisc::RequestExit(false); }
	}
}
