#include "Crowd/APEXNPCCrowd.h"
#include "Racing/FormulaTrack.h"
#include "Racing/APEXRaceTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexNPC, Log, All);

AAPEXNPCCrowd::AAPEXNPCCrowd()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CharacterMeshes = { FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")),
	                    FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")) };
	IdleAnimation = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
}

void AAPEXNPCCrowd::SelectSlots(const TArray<FVector>& Slots, const FVector& Point, float Radius, int32 Count, const TArray<int32>& Current, TArray<int32>& Out)
{
	Out.Reset();
	const float R2 = Radius * Radius;
	TArray<TPair<float, int32>> D;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const float Dist2 = FVector::DistSquared(Slots[i], Point);
		if (Dist2 <= R2) { D.Add({ Dist2, i }); }
	}
	D.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	if (D.Num() > Count) { D.SetNum(Count); }
	TSet<int32> Keep;
	for (const TPair<float, int32>& P : D) { Keep.Add(P.Value); }
	for (int32 C : Current) { if (Keep.Contains(C)) { Out.Add(C); } }          // stable: whoever is already standing there stays
	for (const TPair<float, int32>& P : D) { Out.AddUnique(P.Value); }
}

void AAPEXNPCCrowd::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoNPC"))) { SetActorTickEnabled(false); return; }
	if (TActorIterator<AFormulaTrack> It(GetWorld()); It) { Track = *It; }
	Idle = Cast<UAnimSequence>(IdleAnimation.TryLoad());
	UMaterialInterface* Cloth = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Buildings/M_APEX_NPCCloth.M_APEX_NPCCloth"));
	TArray<USkeletalMesh*> Meshes;
	for (const FSoftObjectPath& P : CharacterMeshes) { if (USkeletalMesh* M = Cast<USkeletalMesh>(P.TryLoad())) { Meshes.Add(M); } }
	if (!Track || !Idle || Meshes.Num() == 0 || !Cloth) { UE_LOG(LogApexNPC, Warning, TEXT("NPC crowd assets missing (meshes %d, idle %d, cloth %d); the block figures stay"), Meshes.Num(), Idle != nullptr, Cloth != nullptr); SetActorTickEnabled(false); return; }
	int32 N = 70; FParse::Value(FCommandLine::Get(), TEXT("APEXNPCPool="), N);
	Pool.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this);
		C->SetSkeletalMesh(Meshes[i % Meshes.Num()]);
		C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		C->SetAnimation(Idle);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		C->bEnableUpdateRateOptimizations = true;
		C->SetupAttachment(GetRootComponent());
		C->SetAbsolute(true, true, true);
		C->RegisterComponent();
		C->Play(true);
		C->SetVisibility(false);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Cloth, this);
		for (int32 m = 0; m < C->GetNumMaterials(); ++m) { C->SetMaterial(m, MID); }
		Comps.Add(C); Mats.Add(MID);
		Pool[i].Comp = C; Pool[i].Mat = MID; Pool[i].Phase = Rand.FRand() * 6.28f;
	}
	if (Track) { for (const FTransform& T : Track->NPCSlots) { SlotPos.Add(T.GetLocation()); } }
	UE_LOG(LogApexNPC, Log, TEXT("NPC crowd: pool of %d skeletal characters over %d slots"), N, SlotPos.Num());
}

void AAPEXNPCCrowd::Assign(FNPC& N, int32 Slot)
{
	N.Slot = Slot;
	const FTransform& T = Track->NPCSlots[Slot];
	const uint8 Kind = Track->NPCKinds[Slot];
	// The mannequin mesh faces +Y in asset space and the block figure faces -Y, so turn 180 degrees from the slot's yaw.
	N.Base = T.GetLocation();
	N.Comp->SetWorldLocationAndRotation(N.Base, FRotator(0.f, T.Rotator().Yaw + 180.f, 0.f));
	N.Comp->SetWorldScale3D(FVector(T.GetScale3D().X));
	N.bHop = (Kind == 1);
	FLinearColor Colour;
	if (Kind >= 10)
	{
		const TArray<FAPEXTeam> Teams = UAPEXGridLibrary::GetTeams();
		Colour = Teams.IsValidIndex(Kind - 10) ? Teams[Kind - 10].Primary : FLinearColor::Red;
	}
	else if (Kind == 3) { Colour = FLinearColor(0.95f, 0.30f, 0.02f); }
	else
	{
		static const FLinearColor Palette[8] = { FLinearColor(0.55f, 0.05f, 0.05f), FLinearColor(0.05f, 0.16f, 0.50f), FLinearColor(0.70f, 0.55f, 0.05f), FLinearColor(0.60f, 0.60f, 0.60f),
			FLinearColor(0.04f, 0.30f, 0.12f), FLinearColor(0.70f, 0.22f, 0.02f), FLinearColor(0.25f, 0.05f, 0.30f), FLinearColor(0.03f, 0.03f, 0.04f) };
		Colour = Palette[(Slot * 7 + 3) % 8];
	}
	N.Mat->SetVectorParameterValue(TEXT("Colour"), Colour);
	N.Comp->SetPosition(Rand.FRand() * Idle->GetPlayLength(), false);
	N.Comp->SetPlayRate(Rand.FRandRange(0.8f, 1.15f));
	N.Comp->SetVisibility(true);
}

void AAPEXNPCCrowd::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock += Dt;
	if (!Track || Pool.Num() == 0) { return; }
	// Cheering NPCs hop in time (the idle clip has no cheer): a small per-character bounce on the component.
	for (FNPC& N : Pool)
	{
		if (N.Slot != INDEX_NONE && N.bHop) { N.Comp->SetWorldLocation(N.Base + FVector(0.f, 0.f, FMath::Abs(FMath::Sin(Clock * 2.4f + N.Phase)) * 10.f)); }
	}
	Timer -= Dt;
	if (Timer > 0.f) { return; }
	Timer = 0.3f;
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) { return; }
	TArray<int32> Current;
	for (const FNPC& N : Pool) { if (N.Slot != INDEX_NONE) { Current.Add(N.Slot); } }
	TArray<int32> Want;
	SelectSlots(SlotPos, Cam->GetCameraLocation(), ActiveRadiusCm, Pool.Num(), Current, Want);
	TSet<int32> WantSet(Want);
	TSet<int32> Taken;
	for (FNPC& N : Pool)                                                   // release characters whose slot left the set, keep the rest
	{
		if (N.Slot != INDEX_NONE && WantSet.Contains(N.Slot)) { Taken.Add(N.Slot); }
		else if (N.Slot != INDEX_NONE) { N.Slot = INDEX_NONE; N.Comp->SetVisibility(false); }
	}
	int32 Next = 0;
	for (FNPC& N : Pool)
	{
		if (N.Slot != INDEX_NONE) { continue; }
		while (Want.IsValidIndex(Next) && Taken.Contains(Want[Next])) { ++Next; }
		if (!Want.IsValidIndex(Next)) { break; }
		Taken.Add(Want[Next]);
		Assign(N, Want[Next]);
	}
}
