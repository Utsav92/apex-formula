#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXNPCCrowd.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class UMaterialInstanceDynamic;
class AFormulaTrack;

/**
 * Animated skeletal NPCs for spectators, pit crews and marshals. A fixed pool of characters is moved, a few times a second, onto the people-slots
 * nearest the camera (the track records every slot); further away the block figures (instanced meshes with a shader bob) stand in, and they
 * collapse out of existence inside 35 m so the two never overlap.
 *
 * Meshes: Manny / Quinn (Epic's mannequins, copied from the engine templates) are the stand-ins today. The mesh list is data (CharacterMeshes), so real
 * MetaHuman Crowd skeletal meshes, or any skeletal mesh that plays the idle animation, can be dropped in without code changes.
 * Command line: -APEXNoNPC disables the crowd; -APEXNPCPool=<n> sets the pool size (default 70).
 */
UCLASS()
class APEXFORMULA_API AAPEXNPCCrowd : public AActor
{
	GENERATED_BODY()

public:
	AAPEXNPCCrowd();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Skeletal meshes the pool draws from (cycled). Replace with MetaHuman crowd meshes to upgrade the look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") TArray<FSoftObjectPath> CharacterMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") FSoftObjectPath IdleAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC") float ActiveRadiusCm = 3500.f;

	/** Pure helper (tested): indices of the slots to occupy: up to Count slots within Radius of Point, nearest first, keeping Current ones that are still inside. */
	static void SelectSlots(const TArray<FVector>& Slots, const FVector& Point, float Radius, int32 Count, const TArray<int32>& Current, TArray<int32>& Out);

private:
	struct FNPC { USkeletalMeshComponent* Comp = nullptr; UMaterialInstanceDynamic* Mat = nullptr; int32 Slot = INDEX_NONE; float Phase = 0.f; bool bHop = false; FVector Base = FVector::ZeroVector; };

	UPROPERTY() TObjectPtr<AFormulaTrack> Track;
	UPROPERTY() TObjectPtr<UAnimSequence> Idle;
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Comps;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	TArray<FNPC> Pool;
	TArray<FVector> SlotPos;
	float Timer = 0.f, Clock = 0.f;
	FRandomStream Rand{20261116};

	void Assign(FNPC& N, int32 Slot);
};
