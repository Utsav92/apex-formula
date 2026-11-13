#include "VFX/APEXCityFX.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Lighting/APEXLightingDirector.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Misc/CommandLine.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexCityFX, Log, All);

namespace
{
	constexpr int32 NumSteam = 700;
	constexpr int32 NumSpark = 400;
	constexpr int32 NumSmoke = 300;
	constexpr int32 NumStacks = 20;
}

AAPEXCityFX::AAPEXCityFX()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SteamISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Steam"));
	SparkISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks"));
	SmokeISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Smoke"));
	StackISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Stacks"));
	for (UInstancedStaticMeshComponent* C : { SteamISM.Get(), SparkISM.Get(), SmokeISM.Get() })
	{
		C->SetupAttachment(GetRootComponent());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetMobility(EComponentMobility::Movable);
		C->SetAbsolute(true, true, true);
	}
	StackISM->SetupAttachment(GetRootComponent());
	StackISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StackISM->SetMobility(EComponentMobility::Movable);
	StackISM->SetAbsolute(true, true, true);
}

float AAPEXCityFX::SparkRate(float SpeedMps, float LatG, float DecelMps2)
{
	if (SpeedMps < 40.f) { return 0.f; }                                                        // the floor only strikes at speed
	const float Load = FMath::Clamp((LatG - 2.8f) / 2.0f, 0.f, 1.f) + FMath::Clamp((DecelMps2 - 25.f) / 25.f, 0.f, 1.f);
	return 320.f * FMath::Clamp(Load, 0.f, 1.f) * FMath::Clamp((SpeedMps - 40.f) / 40.f, 0.f, 1.f);
}

float AAPEXCityFX::SmokeRate(float SpeedMps, float LatG, float DecelMps2)
{
	if (SpeedMps < 30.f) { return 0.f; }
	const float Load = FMath::Clamp((DecelMps2 - 30.f) / 25.f, 0.f, 1.f) + FMath::Clamp((LatG - 3.6f) / 2.0f, 0.f, 1.f);
	return 70.f * FMath::Clamp(Load, 0.f, 1.f);
}

void AAPEXCityFX::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoCityFX"))) { SetActorTickEnabled(false); return; }
	if (TActorIterator<AAPEXLightingDirector> It(GetWorld()); It) { Director = *It; }
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* SteamMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_APEX_Spray.M_APEX_Spray"));
	UMaterialInterface* SparkMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_APEX_Spark.M_APEX_Spark"));
	if (!Sphere || !SteamMat || !SparkMat) { UE_LOG(LogApexCityFX, Warning, TEXT("City FX assets missing (run Python/ue_cityfx_materials.py)")); SetActorTickEnabled(false); return; }
	SteamISM->SetStaticMesh(Sphere); SteamMID = UMaterialInstanceDynamic::Create(SteamMat, this); SteamISM->SetMaterial(0, SteamMID);
	SparkISM->SetStaticMesh(Sphere); SparkMID = UMaterialInstanceDynamic::Create(SparkMat, this); SparkISM->SetMaterial(0, SparkMID);
	SmokeISM->SetStaticMesh(Sphere); SmokeMID = UMaterialInstanceDynamic::Create(SteamMat, this); SmokeISM->SetMaterial(0, SmokeMID);
	Steam.SetNum(NumSteam); Spark.SetNum(NumSpark); Smoke.SetNum(NumSmoke);
	for (int32 i = 0; i < NumSteam; ++i) { SteamISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector::ZeroVector)); }
	for (int32 i = 0; i < NumSmoke; ++i) { SmokeISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector::ZeroVector)); }
	for (int32 i = 0; i < NumSpark; ++i) { SparkISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector::ZeroVector)); }
	if (UStaticMesh* Stack = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environments/Props/SM_Prop_SteamStack.SM_Prop_SteamStack"))) { StackISM->SetStaticMesh(Stack); }
}

void AAPEXCityFX::FindPlayer()
{
	if (Player) { return; }
	for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsAI() && It->GetController() && It->GetController()->IsLocalPlayerController()) { Player = *It; PrevYaw = It->GetActorRotation().Yaw; break; }
	}
	if (Player) { PlaceStacks(); }
}

void AAPEXCityFX::PlaceStacks()
{
	const TArray<FVector>& Path = Player->GetPathPoints();
	if (Path.Num() < 8) { return; }
	for (int32 k = 0; k < NumStacks; ++k)
	{
		const int32 i = (k * Path.Num()) / NumStacks + Path.Num() / (NumStacks * 2);
		const FVector P = Path[i % Path.Num()];
		const FVector Dir = (Path[(i + 1) % Path.Num()] - Path[(i + Path.Num() - 1) % Path.Num()]).GetSafeNormal2D();
		FVector Left(-Dir.Y, Dir.X, 0.f);
		float Side = (k % 2) ? 1.f : -1.f;
		if (P.X > -26000.f && P.X < 26000.f && FMath::Abs(P.Y) < 12000.f) { Side = Left.Y > 0.f ? 1.f : -1.f; }   // start straight: keep to the north side (the pits are south)
		const FVector Pos = P + Left * Side * 1250.f;
		Stacks.Add(Pos);
		StackISM->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(Pos.X, Pos.Y, 0.f)));
	}
	UE_LOG(LogApexCityFX, Log, TEXT("City FX: %d steam stacks"), Stacks.Num());
}

void AAPEXCityFX::Step(TArray<FPuff>& Pool, UInstancedStaticMeshComponent* ISM, float Dt, float Grav, float Drag, float Grow)
{
	TArray<FTransform> T; T.SetNum(Pool.Num());
	for (int32 i = 0; i < Pool.Num(); ++i)
	{
		FPuff& D = Pool[i];
		if (D.Life <= 0.f || D.Age >= D.Life) { T[i] = FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000.f), FVector::ZeroVector); continue; }
		D.Age += Dt;
		D.Vel.Z -= Grav * Dt;
		D.Vel *= FMath::Max(0.f, 1.f - Drag * Dt);
		D.Pos += D.Vel * Dt;
		const float K = 1.f - D.Age / D.Life;
		T[i] = FTransform(FRotator::ZeroRotator, D.Pos, FVector(D.Size * (1.f + Grow * (1.f - K)) * FMath::Sqrt(K) + 0.005f));
	}
	ISM->BatchUpdateInstancesTransforms(0, T, true, true, true);
}

void AAPEXCityFX::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	FindPlayer();
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CamPos = Cam ? Cam->GetCameraLocation() : FVector::ZeroVector;
	const float Bias = Director.IsValid() ? Director->GetExposureBias() : 0.f;
	const float Lum = FMath::Pow(2.f, 10.f - Bias);
	if (SteamMID) { SteamMID->SetScalarParameterValue(TEXT("Luminance"), 0.55f * Lum); SteamMID->SetScalarParameterValue(TEXT("Opacity"), 0.10f); }
	if (SparkMID) { SparkMID->SetScalarParameterValue(TEXT("Luminance"), 0.9f * Lum); }
	if (SmokeMID) { SmokeMID->SetScalarParameterValue(TEXT("Luminance"), 0.35f * Lum); SmokeMID->SetScalarParameterValue(TEXT("Opacity"), 0.22f); }

	// Steam: only stacks within 300 m of the camera emit (about 18 puffs a second each).
	for (const FVector& S : Stacks)
	{
		if (FVector::DistSquared(S, CamPos) > FMath::Square(30000.f)) { continue; }
		float Budget = 18.f * Dt + Rand.FRand();
		while (Budget >= 1.f)
		{
			FPuff& P = Steam[NextSteam]; NextSteam = (NextSteam + 1) % Steam.Num();
			P.Pos = S + FVector(Rand.FRandRange(-8.f, 8.f), Rand.FRandRange(-8.f, 8.f), 265.f);
			P.Vel = FVector(Rand.FRandRange(-25.f, 25.f) + 40.f, Rand.FRandRange(-25.f, 25.f), Rand.FRandRange(110.f, 210.f));   // light breeze along +X
			P.Age = 0.f; P.Life = Rand.FRandRange(2.5f, 4.2f); P.Size = Rand.FRandRange(0.35f, 0.7f);
			Budget -= 1.f;
		}
	}
	Step(Steam, SteamISM, Dt, -25.f, 0.25f, 3.0f);                      // gentle buoyancy, grows 4x as it disperses

	// Sparks from the player's floor.
	if (Player && Dt > 0.f)
	{
		const float V = Player->GetSpeedMps();
		const float Yaw = Player->GetActorRotation().Yaw;
		const float YawRate = FMath::Abs(FMath::FindDeltaAngleDegrees(PrevYaw, Yaw)) / Dt;
		const float LatG = FMath::DegreesToRadians(YawRate) * V / 9.81f;
		const float Decel = FMath::Max(0.f, (PrevSpeed - V) / Dt);
		PrevYaw = Yaw; PrevSpeed = V;
		float Budget = SparkRate(V, LatG, Decel) * Dt + Rand.FRand();
		const FVector Fwd = Player->GetActorForwardVector(), Right = Player->GetActorRightVector();
		float SmokeBudget = SmokeRate(V, LatG, Decel) * Dt + Rand.FRand();
		while (SmokeBudget >= 1.f)
		{
			FPuff& P = Smoke[NextSmoke]; NextSmoke = (NextSmoke + 1) % Smoke.Num();
			const float Side = Rand.FRand() < 0.5f ? -85.f : 85.f;
			P.Pos = Player->GetActorLocation() + Fwd * 140.f + Right * Side - FVector(0, 0, 25.f);          // front tyres lock first
			P.Vel = Fwd * V * 100.f * 0.5f + FVector(Rand.FRandRange(-60.f, 60.f), Rand.FRandRange(-60.f, 60.f), Rand.FRandRange(60.f, 160.f));
			P.Age = 0.f; P.Life = Rand.FRandRange(0.9f, 1.8f); P.Size = Rand.FRandRange(0.20f, 0.40f);
			SmokeBudget -= 1.f;
		}
		while (Budget >= 1.f)
		{
			FPuff& P = Spark[NextSpark]; NextSpark = (NextSpark + 1) % Spark.Num();
			P.Pos = Player->GetActorLocation() - Fwd * Rand.FRandRange(60.f, 190.f) + Right * Rand.FRandRange(-45.f, 45.f) - FVector(0, 0, 18.f);
			P.Vel = Fwd * V * 100.f * 0.35f - Fwd * Rand.FRandRange(300.f, 900.f) + Right * Rand.FRandRange(-300.f, 300.f) + FVector(0, 0, Rand.FRandRange(60.f, 380.f));
			P.Age = 0.f; P.Life = Rand.FRandRange(0.25f, 0.7f); P.Size = Rand.FRandRange(0.012f, 0.03f);
			Budget -= 1.f;
		}
	}
	Step(Spark, SparkISM, Dt, 980.f, 0.6f, -0.2f);
	Step(Smoke, SmokeISM, Dt, -40.f, 1.8f, 4.0f);
}
