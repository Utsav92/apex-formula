#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sponsors/APEXSponsorDataAsset.h"
#include "APEXAdvertisingManager.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UTexture2D;

/** Pure advertising rules (no world, testable). */
struct APEXFORMULA_API FAPEXAdRules
{
	/** May this sponsor be displayed? Unlicensed placeholders only when the build allows them. */
	static bool MayDisplay(EAPEXSponsorLicense License, bool bAllowUnlicensed) { return License != EAPEXSponsorLicense::UnlicensedPlaceholder || bAllowUnlicensed; }
	/** Whether unlicensed placeholder art is allowed in this process: development builds yes, Shipping or -APEXDistribution no; -APEXAllowPlaceholders forces yes. */
	static bool AllowUnlicensedHere();
	/** Next index in a rotation that never repeats the previous neighbour (curated, not random). */
	static int32 NextRotation(int32 Index, int32 Count, int32 Stride = 3) { return Count > 0 ? (Index * Stride + 1) % Count : 0; }
	/** Smallest stride >= 3 that is coprime with Count, so a rotation visits every entry before repeating. */
	static int32 StrideFor(int32 Count);
	/** Circuit section from curvature (rad per metre): 0 straight, 1 corner. Threshold is a 250 m radius. */
	static int32 SectionOfCurvature(float RadPerMetre) { return RadPerMetre > 0.004f ? 1 : 0; }
};

/**
 * Owns the sponsor registry (data assets in /Game/Sponsors/Data), resolves what may be shown (license gating with fictional fallbacks), builds the
 * trackside placements from the circuit, and runs ONE update loop for every digital board (a handful of shared material channels crossfade between
 * sponsors on a schedule), so no board needs its own Tick. Command line: -APEXNoAds, -APEXDistribution, -APEXAllowPlaceholders.
 */
UCLASS()
class APEXFORMULA_API AAPEXAdvertisingManager : public AActor
{
	GENERATED_BODY()

public:
	AAPEXAdvertisingManager();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

	/** Sponsor for an id after license gating (fallback if it may not be shown, nullptr if neither exists). */
	const UAPEXSponsorDataAsset* Resolve(FName SponsorId) const;
	const TArray<TObjectPtr<UAPEXSponsorDataAsset>>& GetRegistry() const { return Registry; }
	bool AllowUnlicensed() const { return bAllowUnlicensed; }
	int32 GetPlacementCount() const { return PlacementCount; }
	int32 GetDigitalChannelCount() const { return Channels.Num(); }
	/** Race announcement on the digital boards (index into the announcement textures: 0 grand prix, 1 lights out, 2 personal best, 3 chequered flag). */
	void Announce(int32 Index, float Seconds = 8.f);
	/** Writes the license manifest (one line per sponsor with status, source, what is shown in this build). */
	FString BuildLicenseManifest() const;

private:
	UPROPERTY() TArray<TObjectPtr<UAPEXSponsorDataAsset>> Registry;
	bool bAllowUnlicensed = true;
	int32 PlacementCount = 0;
	void LoadRegistry();
	void BuildPlacements();

	struct FChannel { UMaterialInstanceDynamic* MID = nullptr; TArray<const UAPEXSponsorDataAsset*> List; int32 Cur = 0; float T = 0.f; float Dwell = 12.f; };
	TArray<FChannel> Channels;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ChannelMIDs;
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Comps;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> AnnTextures;
	float AnnTimer = 0.f; int32 AnnIndex = -1;
	int32 PrevRaceState = -1; float PrevBest = 0.f;
	UHierarchicalInstancedStaticMeshComponent* MakeHISM(UStaticMesh* Mesh, float CullEnd);
};
