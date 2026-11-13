#include "Sponsors/APEXAdvertisingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Racing/FormulaTrack.h"
#include "Racing/APEXRaceManager.h"
#include "Racing/FormulaLapTimingComponent.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexAds, Log, All);

int32 FAPEXAdRules::StrideFor(int32 Count)
{
	for (int32 St = 3; St < 64; ++St) { if (FMath::GreatestCommonDivisor(St, FMath::Max(Count, 1)) == 1) { return St; } }
	return 1;
}

bool FAPEXAdRules::AllowUnlicensedHere()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXAllowPlaceholders"))) { return true; }
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXDistribution"))) { return false; }
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

AAPEXAdvertisingManager::AAPEXAdvertisingManager()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AAPEXAdvertisingManager::LoadRegistry()
{
	Registry.Reset();
	FAssetRegistryModule& Mod = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Found;
	Mod.Get().ScanPathsSynchronous({ TEXT("/Game/Sponsors/Data") }, /*bForceRescan=*/false);   // the background scan may not have reached this folder yet
	Mod.Get().GetAssetsByPath(FName(TEXT("/Game/Sponsors/Data")), Found, /*bRecursive=*/true);
	for (const FAssetData& A : Found)
	{
		if (UAPEXSponsorDataAsset* S = Cast<UAPEXSponsorDataAsset>(A.GetAsset())) { Registry.Add(S); }
	}
	Registry.Sort([](const UAPEXSponsorDataAsset& A, const UAPEXSponsorDataAsset& B) { return A.SponsorId.LexicalLess(B.SponsorId); });
}

const UAPEXSponsorDataAsset* AAPEXAdvertisingManager::Resolve(FName SponsorId) const
{
	for (const UAPEXSponsorDataAsset* S : Registry)
	{
		if (S && S->SponsorId == SponsorId)
		{
			if (FAPEXAdRules::MayDisplay(S->License, bAllowUnlicensed)) { return S; }
			if (!S->FallbackSponsorId.IsNone() && S->FallbackSponsorId != SponsorId) { return Resolve(S->FallbackSponsorId); }   // the fallback is itself checked
			return nullptr;
		}
	}
	return nullptr;
}

FString AAPEXAdvertisingManager::BuildLicenseManifest() const
{
	FString Out = FString::Printf(TEXT("APEX sponsor license manifest (unlicensed placeholders allowed in this build: %s)\n"), bAllowUnlicensed ? TEXT("YES, private prototype") : TEXT("NO, distribution"));
	for (const UAPEXSponsorDataAsset* S : Registry)
	{
		if (!S) { continue; }
		const UAPEXSponsorDataAsset* Shown = Resolve(S->SponsorId);
		Out += FString::Printf(TEXT("%-10s | %-22s | %-22s | shown as: %-10s | source: %s\n"), *S->SponsorId.ToString(), *S->SponsorName.ToString(),
			S->License == EAPEXSponsorLicense::Licensed ? TEXT("Licensed") : (S->License == EAPEXSponsorLicense::OriginalFictional ? TEXT("OriginalFictional") : TEXT("UnlicensedPlaceholder")),
			Shown ? *Shown->SponsorId.ToString() : TEXT("(nothing)"), *S->AssetSource);
	}
	return Out;
}

void AAPEXAdvertisingManager::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoAds"))) { SetActorTickEnabled(false); return; }
	bAllowUnlicensed = FAPEXAdRules::AllowUnlicensedHere();
	LoadRegistry();
	UE_LOG(LogApexAds, Log, TEXT("Advertising: %d sponsors in the registry, unlicensed placeholders %s"), Registry.Num(), bAllowUnlicensed ? TEXT("ALLOWED (private prototype)") : TEXT("BLOCKED (distribution)"));
	UE_LOG(LogApexAds, Log, TEXT("%s"), *BuildLicenseManifest());
	BuildPlacements();
}

UHierarchicalInstancedStaticMeshComponent* AAPEXAdvertisingManager::MakeHISM(UStaticMesh* Mesh, float CullEnd)
{
	UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);          // advertising never touches the racing line or the cars
	C->SetCullDistances(0, static_cast<int32>(CullEnd));
	C->SetupAttachment(GetRootComponent());
	C->RegisterComponent();
	Comps.Add(C);
	return C;
}

void AAPEXAdvertisingManager::Announce(int32 Index, float Seconds)
{
	if (!AnnTextures.IsValidIndex(Index) || !AnnTextures[Index]) { return; }
	AnnIndex = Index; AnnTimer = Seconds;
	for (int32 c = 0; c < Channels.Num() && c < 4; ++c)        // the four channels nearest the start straight carry the announcement
	{
		if (Channels[c].MID) { Channels[c].MID->SetTextureParameterValue(TEXT("TexA"), AnnTextures[Index]); Channels[c].MID->SetTextureParameterValue(TEXT("TexB"), AnnTextures[Index]); Channels[c].MID->SetScalarParameterValue(TEXT("Blend"), 0.f); Channels[c].MID->SetScalarParameterValue(TEXT("Brightness"), 1.0f); }
	}
	UE_LOG(LogApexAds, Log, TEXT("Announcement %d on the digital boards for %.0f s"), Index, Seconds);
}

void AAPEXAdvertisingManager::BuildPlacements()
{
	AFormulaTrack* Track = nullptr;
	if (TActorIterator<AFormulaTrack> It(GetWorld()); It) { Track = *It; }
	UStaticMesh* MBarrier = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sponsors/Meshes/SM_Ad_Barrier.SM_Ad_Barrier"));
	UStaticMesh* MLarge = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sponsors/Meshes/SM_Ad_Large.SM_Ad_Large"));
	UStaticMesh* MGantry = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sponsors/Meshes/SM_Ad_Gantry.SM_Ad_Gantry"));
	UMaterialInterface* LedBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Sponsors/Materials/M_APEX_LED_Advertising.M_APEX_LED_Advertising"));
	if (!Track || !MBarrier || !MLarge || !MGantry || !LedBase || Registry.Num() == 0) { UE_LOG(LogApexAds, Warning, TEXT("Advertising: assets or track missing (run Python/ue_sponsors.py and ue_sponsor_meshes.py)")); return; }
	const TArray<FVector> Line = Track->GetCenterline();
	const int32 N = Line.Num();
	if (N < 16) { return; }

	// ---- sponsor lists by placement category (license-gated, deduplicated) ----
	auto Candidates = [this](FName Cat)
	{
		TArray<const UAPEXSponsorDataAsset*> Out;
		for (const UAPEXSponsorDataAsset* S : Registry)
		{
			if (!S || !S->PlacementCategories.Contains(Cat)) { continue; }
			if (const UAPEXSponsorDataAsset* R = Resolve(S->SponsorId)) { Out.AddUnique(R); }
		}
		return Out;
	};
	const TArray<const UAPEXSponsorDataAsset*> LStraight = Candidates(TEXT("Straight")), LBarrier = Candidates(TEXT("Barrier")), LGantry = Candidates(TEXT("Gantry"));
	TArray<const UAPEXSponsorDataAsset*> LLed;
	for (const UAPEXSponsorDataAsset* S : Registry) { if (S && S->PreferredFormat == EAPEXSponsorFormat::LED) { if (const UAPEXSponsorDataAsset* R = Resolve(S->SponsorId)) { LLed.AddUnique(R); } } }
	if (LStraight.Num() == 0 || LBarrier.Num() == 0 || LGantry.Num() == 0 || LLed.Num() == 0) { UE_LOG(LogApexAds, Warning, TEXT("Advertising: a category has no displayable sponsor")); return; }

	// ---- digital channels: a handful of shared materials, each cycling through the LED sponsors on its own schedule ----
	constexpr int32 NumCh = 6;
	for (int32 c = 0; c < NumCh; ++c)
	{
		FChannel Ch; Ch.MID = UMaterialInstanceDynamic::Create(LedBase, this); ChannelMIDs.Add(Ch.MID);
		Ch.List = LLed; Ch.Cur = (c * 2) % LLed.Num(); Ch.T = c * 2.1f; Ch.Dwell = 10.f + 1.7f * c;      // channels are out of phase so neighbours never flip together
		Channels.Add(Ch);
	}
	for (const TCHAR* A : { TEXT("gp"), TEXT("lights"), TEXT("pb"), TEXT("chequered") }) { AnnTextures.Add(LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Sponsors/Textures/T_Ann_%s.T_Ann_%s"), A, A))); }

	// ---- component pools ----
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> StaticBarrier, StaticLarge;
	TArray<UHierarchicalInstancedStaticMeshComponent*> LedBarrier;
	const int32 PanelSlot = MBarrier->GetMaterialIndex(FName(TEXT("Panel")));
	for (int32 c = 0; c < NumCh; ++c) { UHierarchicalInstancedStaticMeshComponent* C = MakeHISM(MBarrier, 60000.f); C->SetMaterial(PanelSlot, ChannelMIDs[c]); LedBarrier.Add(C); }
	auto StaticComp = [&](TMap<FName, UHierarchicalInstancedStaticMeshComponent*>& Pool, UStaticMesh* Mesh, const UAPEXSponsorDataAsset* S, float Cull) -> UHierarchicalInstancedStaticMeshComponent*
	{
		if (UHierarchicalInstancedStaticMeshComponent** Found = Pool.Find(S->SponsorId)) { return *Found; }
		UHierarchicalInstancedStaticMeshComponent* C = MakeHISM(Mesh, Cull);
		if (UMaterialInterface* Mat = S->StaticMaterial.LoadSynchronous()) { C->SetMaterial(Mesh->GetMaterialIndex(FName(TEXT("Panel"))), Mat); }
		Pool.Add(S->SponsorId, C);
		return C;
	};

	// ---- curvature along the lap ----
	TArray<float> Kr; Kr.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const FVector A = Line[(i + N - 4) % N], B = Line[i], C = Line[(i + 4) % N];
		const FVector T0 = (B - A).GetSafeNormal2D(), T1 = (C - B).GetSafeNormal2D();
		const float Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(T0, T1), -1.f, 1.f));
		Kr[i] = Ang / FMath::Max(FVector::Dist2D(A, C) / 100.f, 0.1f);
	}
	auto DistToTrack = [&Line](const FVector& P)
	{
		float Best = TNumericLimits<float>::Max();
		for (const FVector& L : Line) { Best = FMath::Min(Best, FVector::DistSquared2D(P, L)); }
		return FMath::Sqrt(Best);
	};
	auto InPitZone = [](const FVector& P) { return P.X > -24000.f && P.X < 25500.f && P.Y < 200.f && P.Y > -6000.f; };   // pit lane, garage and the gap around them
	auto YawFacing = [](const FVector& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) + 90.f; };        // mesh front is -Y at yaw 0

	// ---- barrier boards: both sides, every 24 m (second side offset by 12 m), curated sponsor rotation, every third board digital ----
	int32 NBarrierStatic = 0, NBarrierLed = 0, Counter = 0, StaticCounter = 0;
	float Acc = 0.f, NextAt[2] = { 600.f, 1800.f };
	const int32 StrideS = FAPEXAdRules::StrideFor(LStraight.Num()), StrideB = FAPEXAdRules::StrideFor(LBarrier.Num());
	for (int32 i = 0; i < N; ++i)
	{
		Acc += FVector::Dist2D(Line[i], Line[(i + N - 1) % N]);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (Acc < NextAt[Side]) { continue; }
			NextAt[Side] += 2400.f;
			const float Sg = Side == 0 ? 1.f : -1.f;
			const FVector Dir = (Line[(i + 1) % N] - Line[(i + N - 1) % N]).GetSafeNormal2D();
			const FVector Left(-Dir.Y, Dir.X, 0.f);
			const FVector Pos = Line[i] + Left * Sg * 960.f;
			if (InPitZone(Pos) || DistToTrack(Pos) < 850.f) { continue; }
			const FVector Face = -Left * Sg;
			const FTransform T(FRotator(0.f, YawFacing(Face), 0.f), FVector(Pos.X, Pos.Y, 0.f));
			const bool bStraight = FAPEXAdRules::SectionOfCurvature(Kr[i]) == 0;
			if (Counter++ % 3 == 0)
			{
				LedBarrier[(Counter / 3) % NumCh]->AddInstance(T); if (NBarrierLed < 4) { UE_LOG(LogApexAds, Log, TEXT("Digital board %d at (%.0f, %.0f) yaw %.0f"), NBarrierLed, Pos.X, Pos.Y, YawFacing(Face)); } ++NBarrierLed;
			}
			else
			{
				const TArray<const UAPEXSponsorDataAsset*>& L = bStraight ? LStraight : LBarrier;
				const UAPEXSponsorDataAsset* S = L[FAPEXAdRules::NextRotation(StaticCounter++, L.Num(), bStraight ? StrideS : StrideB)];
				StaticComp(StaticBarrier, MBarrier, S, 60000.f)->AddInstance(T); ++NBarrierStatic;
			}
		}
	}

	// ---- large boards on the outside of corner entries, facing the approaching cars ----
	int32 NLarge = 0; float LastLargeAt = -1e9f; Acc = 0.f;
	const int32 StrideL = FAPEXAdRules::StrideFor(LStraight.Num());
	for (int32 i = 0; i < N; ++i)
	{
		Acc += FVector::Dist2D(Line[i], Line[(i + N - 1) % N]);
		const int32 Ahead = (i + 22) % N;
		if (FAPEXAdRules::SectionOfCurvature(Kr[i]) == 0 && FAPEXAdRules::SectionOfCurvature(Kr[Ahead]) == 1 && Acc - LastLargeAt > 25000.f)
		{
			const FVector T0 = (Line[(i + 1) % N] - Line[i]).GetSafeNormal2D(), T1 = (Line[(Ahead + 3) % N] - Line[Ahead]).GetSafeNormal2D();
			const float TurnSign = FVector::CrossProduct(T0, T1).Z >= 0.f ? 1.f : -1.f;      // + = turning left, so the outside is the right-hand side
			const FVector Left(-T0.Y, T0.X, 0.f);
			const FVector Pos = Line[i] + Left * (-TurnSign) * 1500.f;
			if (InPitZone(Pos) || DistToTrack(Pos) < 1300.f) { continue; }
			const UAPEXSponsorDataAsset* S = LStraight[FAPEXAdRules::NextRotation(NLarge + 2, LStraight.Num(), StrideL)];
			StaticComp(StaticLarge, MLarge, S, 90000.f)->AddInstance(FTransform(FRotator(0.f, YawFacing(-T0), 0.f), FVector(Pos.X, Pos.Y, 0.f)));
			LastLargeAt = Acc; ++NLarge;
		}
	}

	// ---- overhead gantries on straights, clear of the pit lane ----
	int32 NGantry = 0;
	TArray<int32> GantryAt;
	const float Fractions[] = { 0.0f, 0.085f, 0.20f, 0.36f, 0.50f, 0.66f, 0.80f, 0.93f };
	const int32 GPanel = MGantry->GetMaterialIndex(FName(TEXT("Panel"))), GPanelB = MGantry->GetMaterialIndex(FName(TEXT("PanelB")));
	const int32 StrideG = FAPEXAdRules::StrideFor(LGantry.Num());
	for (float Fr : Fractions)
	{
		const int32 Target = static_cast<int32>(Fr * N) % N;
		int32 Pick = INDEX_NONE;
		for (int32 d = 0; d < 60 && Pick == INDEX_NONE; ++d)
		{
			for (int32 sgn : { 1, -1 })
			{
				const int32 i = (Target + sgn * d + N) % N;
				bool bClear = true;
				for (int32 k = -18; k <= 18; ++k) { if (FAPEXAdRules::SectionOfCurvature(Kr[(i + k + N) % N]) != 0) { bClear = false; break; } }
				const FVector Dir = (Line[(i + 1) % N] - Line[(i + N - 1) % N]).GetSafeNormal2D(), Left(-Dir.Y, Dir.X, 0.f);
				if (bClear && !InPitZone(Line[i]) && !InPitZone(Line[i] + Left * 1500.f) && !InPitZone(Line[i] - Left * 1500.f) && DistToTrack(Line[i] + Left * 1500.f) < 1560.f && DistToTrack(Line[i] - Left * 1500.f) < 1560.f) { Pick = i; break; }
			}
		}
		if (Pick == INDEX_NONE) { continue; }
		bool bDuplicate = false;
		for (int32 Other : GantryAt) { if (FVector::Dist2D(Line[Other], Line[Pick]) < 8000.f) { bDuplicate = true; break; } }
		if (bDuplicate) { continue; }                       // two target fractions resolved to the same stretch of road
		GantryAt.Add(Pick);
		const FVector Dir = (Line[(Pick + 1) % N] - Line[(Pick + N - 1) % N]).GetSafeNormal2D();
		UHierarchicalInstancedStaticMeshComponent* C = MakeHISM(MGantry, 150000.f);
		C->SetMaterial(GPanel, ChannelMIDs[NGantry % NumCh]);                                                      // front: digital, faces the oncoming cars
		const UAPEXSponsorDataAsset* Back = LGantry[FAPEXAdRules::NextRotation(NGantry + 1, LGantry.Num(), StrideG)];
		if (UMaterialInterface* Mat = Back->StaticMaterial.LoadSynchronous()) { C->SetMaterial(GPanelB, Mat); }     // back: printed, faces the leaving cars
		C->AddInstance(FTransform(FRotator(0.f, YawFacing(-Dir), 0.f), FVector(Line[Pick].X, Line[Pick].Y, 0.f)));
		UE_LOG(LogApexAds, Log, TEXT("Gantry %d at (%.0f, %.0f) facing yaw %.0f"), NGantry, Line[Pick].X, Line[Pick].Y, YawFacing(-Dir));
		++NGantry;
	}

	PlacementCount = NBarrierStatic + NBarrierLed + NLarge + NGantry * 4;      // a gantry carries four panels
	UE_LOG(LogApexAds, Log, TEXT("Advertising placed: %d barrier boards (%d static, %d digital on %d channels), %d large corner-entry boards, %d gantries (4 panels each) = %d panels, %d components"),
		NBarrierStatic + NBarrierLed, NBarrierStatic, NBarrierLed, NumCh, NLarge, NGantry, PlacementCount, Comps.Num());
}

void AAPEXAdvertisingManager::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.1f);
	// One loop for every digital board: each channel crossfades A -> B near the end of its dwell, then B becomes A.
	for (int32 c = 0; c < Channels.Num(); ++c)
	{
		FChannel& Ch = Channels[c];
		if (!Ch.MID || Ch.List.Num() == 0) { continue; }
		if (AnnTimer > 0.f && c < 4) { continue; }                  // an announcement owns these channels
		const UAPEXSponsorDataAsset* A = Ch.List[Ch.Cur % Ch.List.Num()];
		const UAPEXSponsorDataAsset* B = Ch.List[(Ch.Cur + 1) % Ch.List.Num()];
		Ch.T += Dt;
		const float Fade = FMath::Max(A->FadeSeconds, 0.2f);
		if (Ch.T >= Ch.Dwell) { Ch.T = 0.f; Ch.Cur = (Ch.Cur + 1) % Ch.List.Num(); }
		const float Blend = FMath::Clamp((Ch.T - (Ch.Dwell - Fade)) / Fade, 0.f, 1.f);
		if (UTexture2D* TA = A->Logo.LoadSynchronous()) { Ch.MID->SetTextureParameterValue(TEXT("TexA"), TA); }
		if (UTexture2D* TB = B->Logo.LoadSynchronous()) { Ch.MID->SetTextureParameterValue(TEXT("TexB"), TB); }
		Ch.MID->SetScalarParameterValue(TEXT("Blend"), Blend);
		Ch.MID->SetScalarParameterValue(TEXT("Brightness"), FMath::Lerp(A->EmissiveScale, B->EmissiveScale, Blend));
	}
	if (AnnTimer > 0.f) { AnnTimer -= Dt; if (AnnTimer <= 0.f) { AnnIndex = -1; for (int32 c = 0; c < 4 && c < Channels.Num(); ++c) { Channels[c].T = 0.f; } } }

	// Race events -> announcements (polled; cheap).
	static float Poll = 0.f; Poll -= Dt;
	if (Poll <= 0.f)
	{
		Poll = 0.25f;
		if (TActorIterator<AAPEXRaceManager> It(GetWorld()); It)
		{
			const int32 St = static_cast<int32>(It->GetState());
			if (PrevRaceState < 0) { Announce(0, 12.f); }                                              // grand prix title while the cars sit on the grid
			else if (St != PrevRaceState && St == static_cast<int32>(EAPEXRaceState::Racing)) { Announce(1, 6.f); }
			else if (St != PrevRaceState && St == static_cast<int32>(EAPEXRaceState::Finished)) { Announce(3, 12.f); }
			PrevRaceState = St;
		}
		for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
		{
			if (It->IsAI() || !It->LapTiming) { continue; }
			const float Best = It->LapTiming->BestLapTime;
			if (Best > 0.f && (PrevBest <= 0.f || Best < PrevBest - 0.001f) && It->LapTiming->CompletedLaps >= 2) { Announce(2, 8.f); }
			PrevBest = Best;
			break;
		}
	}
}
