#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "APEXSponsorDataAsset.generated.h"

class UTexture2D;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EAPEXSponsorFormat : uint8 { Static, LED, Banner };

/** Where a sponsor's artwork stands legally. Only OriginalFictional and Licensed may appear in a distributable build. */
UENUM(BlueprintType)
enum class EAPEXSponsorLicense : uint8
{
	OriginalFictional,        // created for this project
	Licensed,                 // official artwork, written permission on file
	UnlicensedPlaceholder     // private-prototype text mockup of a real brand: never shipped
};

/** One advertiser. Visual assets live here; where it appears and when is the advertising manager's business. */
UCLASS(BlueprintType)
class APEXFORMULA_API UAPEXSponsorDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sponsor") FName SponsorId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sponsor") FText SponsorName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Art") TSoftObjectPtr<UTexture2D> Logo;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Art") TSoftObjectPtr<UMaterialInterface> StaticMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Art") TSoftObjectPtr<UMaterialInterface> LEDMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Art") EAPEXSponsorFormat PreferredFormat = EAPEXSponsorFormat::Static;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Art") FLinearColor BackgroundColor = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Art") FLinearColor TextColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") float FadeSeconds = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") float ScrollSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lighting") float EmissiveScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement") TArray<FName> PlacementCategories;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Legal") FString AssetSource;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Legal") EAPEXSponsorLicense License = EAPEXSponsorLicense::OriginalFictional;
	/** The fictional equivalent shown instead when this entry may not be displayed (distribution build). Empty for entries that are always allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Legal") FName FallbackSponsorId;

	bool IsDistributable() const { return License != EAPEXSponsorLicense::UnlicensedPlaceholder; }
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(FPrimaryAssetType(TEXT("APEXSponsor")), SponsorId); }
};
