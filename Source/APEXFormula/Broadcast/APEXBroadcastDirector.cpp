#include "Broadcast/APEXBroadcastDirector.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Camera/CameraComponent.h"
#include "Engine/Scene.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AAPEXBroadcastDirector::AAPEXBroadcastDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("BroadcastCam"));
	SetRootComponent(Cam);
}

int32 AAPEXBroadcastDirector::ChoosePod(const TArray<FVector>& Pods, const FVector& Target, int32 Current, float Hysteresis)
{
	if (Pods.Num() == 0) { return -1; }
	int32 Best = 0; float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Pods.Num(); ++i)
	{
		const float D = FVector::DistSquared(Pods[i], Target);
		if (D < BestD) { BestD = D; Best = i; }
	}
	if (Pods.IsValidIndex(Current) && Current != Best)
	{
		const float CurD = FVector::DistSquared(Pods[Current], Target);
		if (BestD > CurD * Hysteresis * Hysteresis) { return Current; }       // not clearly closer: hold the shot
	}
	return Best;
}

void AAPEXBroadcastDirector::BeginPlay()
{
	Super::BeginPlay();
	int32 M = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-APEXCamMode="), M)) { Mode = static_cast<EAPEXCamMode>(FMath::Clamp(M, 0, 2)); }
	float At = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("-APEXReplayAt="), At)) { ReplayAtCmd = At; }
	FParse::Value(FCommandLine::Get(), TEXT("-APEXProfileGPUAt="), ProfileAt);
}

void AAPEXBroadcastDirector::FindPlayer()
{
	if (Player) { return; }
	for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsAI() && It->GetController() && It->GetController()->IsLocalPlayerController()) { Player = *It; break; }
	}
	if (Player) { BuildPods(); BuildGhost(); }
}

void AAPEXBroadcastDirector::BuildPods()
{
	const TArray<FVector>& Path = Player->GetPathPoints();
	if (Path.Num() < 8) { return; }
	float Acc = 0.f; int32 K = 0;
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		Acc += FVector::Dist2D(Path[i], Path[(i + Path.Num() - 1) % Path.Num()]);
		if (Acc < 7000.f) { continue; }
		Acc = 0.f;
		const FVector Dir = (Path[(i + 1) % Path.Num()] - Path[(i + Path.Num() - 1) % Path.Num()]).GetSafeNormal2D();
		const FVector Left(-Dir.Y, Dir.X, 0.f);
		Pods.Add(Path[i] + Left * ((K++ % 2) ? 1.f : -1.f) * 3200.f + FVector(0.f, 0.f, 550.f));
	}
}

void AAPEXBroadcastDirector::BuildGhost()
{
	if (!Player || !Player->Visual || !Player->Visual->GetStaticMesh()) { return; }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ghost = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, P);
	if (!Ghost) { return; }
	UStaticMeshComponent* B = Ghost->GetStaticMeshComponent();
	B->SetMobility(EComponentMobility::Movable);
	B->SetStaticMesh(Player->Visual->GetStaticMesh());
	for (int32 m = 0; m < Player->Visual->GetNumMaterials(); ++m) { B->SetMaterial(m, Player->Visual->GetMaterial(m)); }
	B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FTransform Inv = Player->GetActorTransform();
	VisualRel = Player->Visual->GetComponentTransform().GetRelativeTransform(Inv);
	for (int32 w = 0; w < 4; ++w)
	{
		if (!Player->WheelMesh[w]) { continue; }
		WheelRel[w] = Player->WheelMesh[w]->GetComponentTransform().GetRelativeTransform(Inv);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Ghost);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Player->WheelMesh[w]->GetStaticMesh());
		for (int32 m = 0; m < Player->WheelMesh[w]->GetNumMaterials(); ++m) { C->SetMaterial(m, Player->WheelMesh[w]->GetMaterial(m)); }
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(B);
		C->RegisterComponent();
		GhostWheels.Add(C);
	}
	Ghost->SetActorHiddenInGame(true);
}

void AAPEXBroadcastDirector::Record()
{
	if (!Player) { return; }
	if (History.Num() && Clock - History.Last().T < 1.f / 30.f) { return; }
	History.Add({ Clock, Player->GetActorTransform() });
	while (History.Num() > 2 && Clock - History[0].T > 11.f) { History.RemoveAt(0, 1, EAllowShrinking::No); }
}

FTransform AAPEXBroadcastDirector::SampleAt(float T) const
{
	if (History.Num() == 0) { return FTransform::Identity; }
	for (int32 i = 1; i < History.Num(); ++i)
	{
		if (History[i].T >= T)
		{
			const float A = FMath::Clamp((T - History[i - 1].T) / FMath::Max(History[i].T - History[i - 1].T, 1e-4f), 0.f, 1.f);
			FTransform R; R.Blend(History[i - 1].X, History[i].X, A); return R;
		}
	}
	return History.Last().X;
}

void AAPEXBroadcastDirector::Say(const FString& S) const
{
	UE_LOG(LogTemp, Log, TEXT("Broadcast: %s"), *S);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(7421, 3.f, FColor::White, S); }
}

void AAPEXBroadcastDirector::SetMode(EAPEXCamMode M)
{
	Mode = M;
	Say(M == EAPEXCamMode::Chase ? TEXT("CAMERA: CHASE") : M == EAPEXCamMode::TVPod ? TEXT("CAMERA: TV PODS") : TEXT("CAMERA: HELICOPTER"));
}

bool AAPEXBroadcastDirector::StartReplay()
{
	if (!Ghost || History.Num() < 60) { Say(TEXT("REPLAY: not enough footage yet")); return false; }
	ModeBeforeReplay = Mode;
	Mode = EAPEXCamMode::TVPod;
	ReplayEnd = History.Last().T;
	ReplayT = FMath::Max(History[0].T, ReplayEnd - 10.f);
	bReplaying = true;
	Ghost->SetActorHiddenInGame(false);
	Say(TEXT("INSTANT REPLAY"));
	return true;
}

void AAPEXBroadcastDirector::StopReplay()
{
	bReplaying = false;
	if (Ghost) { Ghost->SetActorHiddenInGame(true); }
	Mode = ModeBeforeReplay;
}

void AAPEXBroadcastDirector::ApplyView(bool bWantOwned)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !Player) { return; }
	if (bWantOwned && !bViewOwned) { PC->SetViewTargetWithBlend(this, 0.4f); bViewOwned = true; }
	else if (!bWantOwned && bViewOwned) { PC->SetViewTargetWithBlend(Player, 0.4f); bViewOwned = false; }
}

void AAPEXBroadcastDirector::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock += Dt;
	FindPlayer();
	if (!Player) { return; }
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	// C (camera cycle) and R (look behind) belong to the player controller / AAPEXCameraManager now. Instant replay is on T.
	if (PC && PC->WasInputKeyJustPressed(EKeys::T)) { bReplaying ? StopReplay() : (void)StartReplay(); }
	if (ProfileAt >= 0.f && Clock >= ProfileAt) { ProfileAt = -1.f; if (GEngine) { GEngine->Exec(GetWorld(), TEXT("ProfileGPU")); } }   // diagnostic: -APEXProfileGPUAt=<s> dumps the GPU pass timings to the log
	if (ReplayAtCmd >= 0.f && !bReplayCmdDone && Clock >= ReplayAtCmd) { bReplayCmdDone = true; StartReplay(); }

	FVector TargetLoc = Player->GetActorLocation(), TargetVel = Player->GetVelocity();
	if (bReplaying)
	{
		const float Remain = ReplayEnd - ReplayT;
		ReplayT += Dt * (Remain < 3.0f ? 0.4f : 1.0f);                            // slow motion for the last three seconds
		if (ReplayT >= ReplayEnd) { StopReplay(); }
		else
		{
			const FTransform G = SampleAt(ReplayT);
			Ghost->SetActorTransform(G * VisualRel.Inverse());                   // Visual's own offset from the pawn root
			Ghost->GetStaticMeshComponent()->SetWorldTransform(VisualRel * G);
			for (int32 w = 0; w < GhostWheels.Num(); ++w) { GhostWheels[w]->SetWorldTransform(WheelRel[w] * G); }
			TargetLoc = G.GetLocation();
			TargetVel = SampleAt(ReplayT + 0.1f).GetLocation() - TargetLoc;
		}
	}
	else { Record(); }

	// Dynamic FOV on the chase camera: wider with speed (feel of speed), back to base at rest.
	if (Player->ChaseCamera)
	{
		if (BaseFov <= 0.f) { BaseFov = Player->ChaseCamera->FieldOfView; }
		const float Speed01 = FMath::Clamp(Player->GetSpeedMps() / 90.f, 0.f, 1.f);
		Player->ChaseCamera->SetFieldOfView(FMath::FInterpTo(Player->ChaseCamera->FieldOfView, BaseFov + 14.f * Speed01 * Speed01, Dt, 3.f));
	}
	ApplyView(Mode != EAPEXCamMode::Chase);
	if (Mode == EAPEXCamMode::Chase) { return; }

	const FVector Aim = TargetLoc + FVector(0.f, 0.f, 80.f) + TargetVel * 0.15f;
	FVector Pos; float Fov = 40.f;
	if (Mode == EAPEXCamMode::TVPod && Pods.Num())
	{
		CurPod = ChoosePod(Pods, TargetLoc, CurPod);
		Pos = Pods[CurPod];
		Fov = FMath::Clamp(FMath::RadiansToDegrees(2.f * FMath::Atan(1100.f / FMath::Max(FVector::Dist(Pos, Aim), 1.f))), 14.f, 65.f);
	}
	else
	{
		HeliAngle += Dt * 0.12f;
		Pos = TargetLoc + FVector(FMath::Cos(HeliAngle), FMath::Sin(HeliAngle), 0.f) * 24000.f + FVector(0.f, 0.f, 15000.f);
		Fov = 30.f;
	}
	const FRotator Want = (Aim - Pos).Rotation();
	SetActorLocation(Pos);
	SetActorRotation(Want);                                              // fast cars pass pods at close range: pan must keep up, no smoothing
	Cam->SetFieldOfView(FMath::FInterpTo(Cam->FieldOfView, Fov, Dt, 4.f));
	// Broadcast optics: shallow depth of field focused on the car (long lens, f/2.8) and a little handheld drift on the helicopter.
	FPostProcessSettings& PP = Cam->PostProcessSettings;
	Cam->PostProcessBlendWeight = 1.f;
	PP.bOverride_DepthOfFieldFstop = true;             PP.DepthOfFieldFstop = Mode == EAPEXCamMode::Helicopter ? 4.0f : 2.8f;
	PP.bOverride_DepthOfFieldFocalDistance = true;     PP.DepthOfFieldFocalDistance = FMath::Max(FVector::Dist(Pos, Aim), 100.f);
	PP.bOverride_DepthOfFieldSensorWidth = true;       PP.DepthOfFieldSensorWidth = 36.f;
	PP.bOverride_DepthOfFieldMinFstop = true;          PP.DepthOfFieldMinFstop = 1.2f;
	if (Mode == EAPEXCamMode::Helicopter)
	{
		const float T = Clock;
		SetActorRotation(Want + FRotator(0.12f * FMath::Sin(T * 1.7f), 0.15f * FMath::Sin(T * 1.3f + 1.f), 0.f));
	}
}
