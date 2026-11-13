#include "Traffic/APEXTraffic.h"
#include "Racing/FormulaTrack.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexTraffic, Log, All);

AAPEXTraffic::AAPEXTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Cars"));
	ISM->SetupAttachment(GetRootComponent());
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetAbsolute(true, true, true);
	ISM->SetCullDistances(0, 90000);
}

float AAPEXTraffic::Advance(float Fraction, float SpeedCmS, float LengthCm, int32 Dir, float Dt)
{
	if (LengthCm < 1.f) { return Fraction; }
	float F = Fraction + Dir * SpeedCmS * Dt / LengthCm;
	if (F > 1.f) { F -= 1.f; } else if (F < 0.f) { F += 1.f; }
	return FMath::Clamp(F, 0.f, 1.f);
}

void AAPEXTraffic::Place(const FVector4& Seg, float Fraction, int32 Dir, float LaneCm, FVector& OutPos, float& OutYaw)
{
	const FVector2D A(Seg.X, Seg.Y), B(Seg.Z, Seg.W);
	const FVector2D D = (B - A).GetSafeNormal() * static_cast<float>(Dir);
	const FVector2D Right(D.Y, -D.X);
	const FVector2D P = FMath::Lerp(A, B, Fraction) + Right * LaneCm;
	OutPos = FVector(P.X, P.Y, 0.f);
	OutYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
}

void AAPEXTraffic::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoTraffic"))) { SetActorTickEnabled(false); return; }
	if (TActorIterator<AFormulaTrack> It(GetWorld()); It) { Track = *It; }
	UStaticMesh* Car = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environments/Props/SM_Traffic_Car.SM_Traffic_Car"));
	if (!Track || !Car || Track->StreetSegs.Num() == 0) { UE_LOG(LogApexTraffic, Warning, TEXT("Traffic: no track, car mesh or streets (run Python/ue_traffic.py)")); SetActorTickEnabled(false); return; }
	ISM->SetStaticMesh(Car);
	int32 N = 90; FParse::Value(FCommandLine::Get(), TEXT("APEXTrafficCars="), N);
	for (int32 i = 0; i < N; ++i)
	{
		FCar C; C.Seg = Rand.RandRange(0, Track->StreetSegs.Num() - 1); C.T = Rand.FRand(); C.Dir = Rand.FRand() < 0.5f ? 1 : -1; C.Speed = Rand.FRandRange(700.f, 1300.f);
		Cars.Add(C);
		ISM->AddInstance(FTransform::Identity);
	}
	UE_LOG(LogApexTraffic, Log, TEXT("Traffic: %d cars on %d streets"), N, Track->StreetSegs.Num());
	{ const FVector4& S0 = Track->StreetSegs[0]; UE_LOG(LogApexTraffic, Log, TEXT("Traffic: first street %.0f,%.0f to %.0f,%.0f"), S0.X, S0.Y, S0.Z, S0.W); }
}

void AAPEXTraffic::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	if (!Track || Cars.Num() == 0) { return; }
	TArray<FTransform> T; T.SetNum(Cars.Num());
	for (int32 i = 0; i < Cars.Num(); ++i)
	{
		FCar& C = Cars[i];
		const FVector4& S = Track->StreetSegs[C.Seg];
		const float Len = FVector2D::Distance(FVector2D(S.X, S.Y), FVector2D(S.Z, S.W));
		C.T = Advance(C.T, C.Speed, Len, C.Dir, Dt);
		FVector P; float Yaw;
		Place(S, C.T, C.Dir, 260.f, P, Yaw);
		T[i] = FTransform(FRotator(0.f, Yaw, 0.f), P);
	}
	ISM->BatchUpdateInstancesTransforms(0, T, true, true, true);
}
