#include "Racing/FormulaTrack.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void MakeSolid(UInstancedStaticMeshComponent* Comp, UStaticMesh* Mesh, UMaterialInterface* Mat, const FLinearColor& Colour, UObject* Outer)
	{
		Comp->SetStaticMesh(Mesh);
		if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, Outer);
			MID->SetVectorParameterValue(TEXT("Color"), Colour);
			Comp->SetMaterial(0, MID);
		}
	}
}

AFormulaTrack::AFormulaTrack()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Road = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Road"));
	Road->SetupAttachment(Root);
	// The car drives on the road via a downward trace; it must not block the car's sweep.
	Road->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Road->SetCollisionResponseToAllChannels(ECR_Ignore);
	Road->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Barriers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Barriers"));
	Barriers->SetupAttachment(Root);
	Barriers->SetCollisionProfileName(TEXT("BlockAll"));

	Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(Root);
	Ground->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Ground->SetCollisionResponseToAllChannels(ECR_Ignore);
	Ground->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

TArray<FVector> AFormulaTrack::GetControlPoints()
{
	// Metres, counter-clockwise. Start straight (0,0)->(560,0), fast left sweep, chicane, hairpin near
	// (430,480), a twisty back section, and a long sweeper home. Prototyped and checked in Python first.
	static const float M[][2] = {
		{0,0},{300,0},{560,0},{760,50},{830,190},{780,310},{830,430},{710,525},
		{530,560},{430,480},{480,380},{360,300},{200,335},{60,425},{-120,440},
		{-300,385},{-420,260},{-470,120},{-430,20},{-300,0}
	};
	TArray<FVector> Out;
	for (const float (&P)[2] : M) { Out.Add(FVector(P[0] * 100.f, P[1] * 100.f, 0.f)); }
	return Out;
}

namespace
{
	/** One centripetal Catmull-Rom segment between P1 and P2 (Barry-Goldman form), alpha = 0.5. */
	void SampleCatmullRom(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3,
		int32 Samples, TArray<FVector2D>& Out)
	{
		auto Knot = [](float Ti, const FVector2D& A, const FVector2D& B) { return Ti + FMath::Sqrt(FVector2D::Distance(A, B)); };
		const float T0 = 0.f, T1 = Knot(T0, P0, P1), T2 = Knot(T1, P1, P2), T3 = Knot(T2, P2, P3);
		auto Lerp = [](const FVector2D& A, const FVector2D& B, float Ta, float Tb, float T)
		{
			const float W = (Tb != Ta) ? (T - Ta) / (Tb - Ta) : 0.f;
			return A * (1.f - W) + B * W;
		};
		for (int32 k = 0; k < Samples; ++k)
		{
			const float T = T1 + (T2 - T1) * static_cast<float>(k) / static_cast<float>(Samples);
			const FVector2D A1 = Lerp(P0, P1, T0, T1, T), A2 = Lerp(P1, P2, T1, T2, T), A3 = Lerp(P2, P3, T2, T3, T);
			const FVector2D B1 = Lerp(A1, A2, T0, T2, T), B2 = Lerp(A2, A3, T1, T3, T);
			Out.Add(Lerp(B1, B2, T1, T2, T));
		}
	}
}

TArray<FVector> AFormulaTrack::ComputeCenterline(int32 N)
{
	N = FMath::Max(N, 16);
	const TArray<FVector> CP = GetControlPoints();
	const int32 C = CP.Num();

	// Dense spline through every control point, wrapping around so the loop is closed and smooth.
	TArray<FVector2D> Dense;
	for (int32 i = 0; i < C; ++i)
	{
		auto P = [&](int32 k) { const FVector& V = CP[(k % C + C) % C]; return FVector2D(V.X, V.Y); };
		SampleCatmullRom(P(i - 1), P(i), P(i + 1), P(i + 2), 60, Dense);
	}

	// Resample at even arc length so point spacing (and so curvature maths) is uniform.
	const int32 D = Dense.Num();
	TArray<float> Cum;
	Cum.Reserve(D + 1);
	Cum.Add(0.f);
	for (int32 i = 0; i < D; ++i) { Cum.Add(Cum.Last() + FVector2D::Distance(Dense[i], Dense[(i + 1) % D])); }
	const float Total = Cum.Last();

	TArray<FVector> Out;
	Out.Reserve(N);
	int32 J = 0;
	for (int32 k = 0; k < N; ++k)
	{
		const float S = Total * static_cast<float>(k) / static_cast<float>(N);
		while (Cum[J + 1] < S) { ++J; }
		const float W = (S - Cum[J]) / FMath::Max(Cum[J + 1] - Cum[J], KINDA_SMALL_NUMBER);
		const FVector2D P = FMath::Lerp(Dense[J], Dense[(J + 1) % D], W);
		Out.Add(FVector(P.X, P.Y, 0.f));
	}
	return Out;
}

TArray<FVector> AFormulaTrack::GetCheckpoints() const
{
	const TArray<FVector> Line = GetCenterline();
	TArray<FVector> Out;
	const int32 Count = FMath::Max(NumCheckpoints, 2);
	for (int32 k = 0; k < Count; ++k)
	{
		Out.Add(Line[(k * Line.Num()) / Count]);
	}
	return Out;
}

FTransform AFormulaTrack::GetStartTransform() const
{
	const TArray<FVector> Line = GetCenterline();
	const int32 N = Line.Num();
	const int32 Slot = N - 3; // behind the line, so the first crossing is a real start
	const FVector Dir = (Line[(Slot + 1) % N] - Line[Slot]).GetSafeNormal();
	return FTransform(Dir.Rotation(), Line[Slot] + FVector(0.f, 0.f, 60.f));
}

TArray<FVector> AFormulaTrack::GetPitPath() const
{
	const TArray<FVector> Line = GetCenterline();
	// Entry: the centreline point near X = -320 m on the start straight; exit: near X = +330 m.
	auto Nearest = [&](float TargetX, float MinX, float MaxX)
	{
		int32 Best = 0; float BestD = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Line.Num(); ++i)
		{
			if (FMath::Abs(Line[i].Y) > 700.f || Line[i].X < MinX || Line[i].X > MaxX) { continue; }
			const float D = FMath::Abs(Line[i].X - TargetX);
			if (D < BestD) { BestD = D; Best = i; }
		}
		return Line[Best];
	};
	const FVector S = Nearest(-32000.f, -40000.f, -25000.f);
	const FVector E = Nearest(33000.f, 25000.f, 40000.f);
	const float Y = PitLaneYCm;
	const TArray<FVector> Ctrl = {
		S, FVector(-27000.f, -900.f, 0.f), FVector(-22500.f, -1500.f, 0.f), FVector(-19500.f, Y, 0.f),
		FVector(-12000.f, Y, 0.f), FVector(-4000.f, Y, 0.f), FVector(4000.f, Y, 0.f), FVector(12000.f, Y, 0.f), FVector(20500.f, Y, 0.f),
		FVector(25500.f, -1500.f, 0.f), FVector(29500.f, -900.f, 0.f), E };

	// Open centripetal Catmull-Rom (end points repeated), then resample at even ~3 m spacing.
	TArray<FVector2D> Dense;
	const int32 C = Ctrl.Num();
	for (int32 i = 0; i < C - 1; ++i)
	{
		auto P = [&](int32 k) { const FVector& V = Ctrl[FMath::Clamp(k, 0, C - 1)]; return FVector2D(V.X, V.Y); };
		SampleCatmullRom(P(i - 1), P(i), P(i + 1), P(i + 2), 40, Dense);
	}
	Dense.Add(FVector2D(E.X, E.Y));
	TArray<float> Cum; Cum.Add(0.f);
	for (int32 i = 1; i < Dense.Num(); ++i) { Cum.Add(Cum.Last() + FVector2D::Distance(Dense[i - 1], Dense[i])); }
	const float Total = Cum.Last();
	const int32 N = FMath::Max(FMath::RoundToInt(Total / 300.f), 8);
	TArray<FVector> Out;
	int32 J = 0;
	for (int32 k = 0; k <= N; ++k)
	{
		const float Sd = Total * k / N;
		while (J < Dense.Num() - 2 && Cum[J + 1] < Sd) { ++J; }
		const float W = (Sd - Cum[J]) / FMath::Max(Cum[J + 1] - Cum[J], KINDA_SMALL_NUMBER);
		const FVector2D Pt = FMath::Lerp(Dense[J], Dense[J + 1], FMath::Clamp(W, 0.f, 1.f));
		Out.Add(FVector(Pt.X, Pt.Y, 0.f));
	}
	return Out;
}

FTransform AFormulaTrack::GetGridTransform(int32 Slot) const
{
	const TArray<FVector> Line = GetCenterline();
	const int32 N = Line.Num();
	float LapCm = 0.f;
	for (int32 i = 0; i < N; ++i) { LapCm += FVector::Dist(Line[i], Line[(i + 1) % N]); }
	const float SegCm = LapCm / N;

	// Row spacing 8.5 m; the second column sits 4 m further back (staggered), 2.2 m either side of the centreline.
	const float BackCm = (6.f + (Slot / 2) * 8.5f + (Slot % 2) * 4.f) * 100.f;
	const float IdxF = -BackCm / SegCm;                       // negative: behind point 0 (the line)
	const int32 I0 = FMath::FloorToInt(IdxF);
	const float Frac = IdxF - I0;
	auto At = [&](int32 i) { return Line[((i % N) + N) % N]; };
	const FVector P = FMath::Lerp(At(I0), At(I0 + 1), Frac);
	const FVector Tan = (At(I0 + 1) - At(I0)).GetSafeNormal();
	const FVector Left(-Tan.Y, Tan.X, 0.f);
	const float Lateral = (Slot % 2 == 0) ? 220.f : -220.f;
	return FTransform(Tan.Rotation(), P + Left * Lateral + FVector(0.f, 0.f, 60.f));
}

float AFormulaTrack::GetLapLengthMeters() const
{
	const TArray<FVector> Line = GetCenterline();
	float Length = 0.f;
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		Length += FVector::Dist(Line[i], Line[(i + 1) % Line.Num()]);
	}
	return Length / 100.f;
}

void AFormulaTrack::SetWetness(float Wetness)
{
	WetnessCache = FMath::Clamp(Wetness, 0.f, 1.f);
	if (AsphaltMID) { AsphaltMID->SetScalarParameterValue(TEXT("Wetness"), WetnessCache); }
}

void AFormulaTrack::BeginPlay()
{
	Super::BeginPlay();
	// Tool hook: -APEXExportPit writes the pit path (cm) so Blender/gen_track.py builds the lane surface from the exact same points.
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXExportPit")))
	{
		FString Json = TEXT("[");
		const TArray<FVector> Pit = GetPitPath();
		for (int32 i = 0; i < Pit.Num(); ++i) { Json += FString::Printf(TEXT("%s[%.1f,%.1f]"), i ? TEXT(",") : TEXT(""), Pit[i].X, Pit[i].Y); }
		Json += TEXT("]");
		const FString Out = FPaths::ProjectDir() / TEXT("Blender/export/pit_path.json");
		FFileHelper::SaveStringToFile(Json, *Out);
		UE_LOG(LogTemp, Log, TEXT("Exported pit path: %d points -> %s"), Pit.Num(), *Out);
	}
	BuildGeometry();
	if (BuildVisuals())
	{
		BuildScenery();
	}
}

bool AFormulaTrack::BuildVisuals()
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Tracks/SM_Track.SM_Track"));
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("FormulaTrack: /Game/Tracks/SM_Track not imported; using cube primitives (run Python/ue_import.py)"));
		return false;
	}
	TrackVisual = NewObject<UStaticMeshComponent>(this, TEXT("TrackVisual"));
	TrackVisual->SetMobility(EComponentMobility::Movable); // see BuildScenery: Static gets no indirect light here
	TrackVisual->SetStaticMesh(Mesh);
	TrackVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision); // collision stays on the cubes
	TrackVisual->CastShadow = false; // flat surfaces; keeps the shadow pass cheap
	TrackVisual->SetupAttachment(GetRootComponent());
	TrackVisual->RegisterComponent();
	AsphaltMID = TrackVisual->CreateDynamicMaterialInstance(0); // slot 0 is the asphalt
	if (AsphaltMID) { AsphaltMID->SetScalarParameterValue(TEXT("Wetness"), WetnessCache); }

	// Keep the cubes for collision (road trace, barrier sweep) but do not draw them.
	Road->SetVisibility(false);
	Barriers->SetVisibility(false);
	Ground->SetVisibility(false);
	return true;
}

void AFormulaTrack::BuildScenery()
{
	struct FKind { const TCHAR* Path; UHierarchicalInstancedStaticMeshComponent* Comp = nullptr; };
	auto Make = [this](const TCHAR* Path, float CullEnd) -> UHierarchicalInstancedStaticMeshComponent*
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Mesh) { return nullptr; }
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		// Components made at runtime default to Static mobility, which gets no indirect light without baked
		// lightmaps (shadowed sides render black). Movable uses the dynamic sky light.
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCullDistances(0, static_cast<int32>(CullEnd));
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		SceneryComps.Add(C);
		return C;
	};

	UHierarchicalInstancedStaticMeshComponent* Pine = Make(TEXT("/Game/Environments/SM_Tree_Pine.SM_Tree_Pine"), 90000.f);
	UHierarchicalInstancedStaticMeshComponent* Oak = Make(TEXT("/Game/Environments/SM_Tree_Oak.SM_Tree_Oak"), 90000.f);
	UHierarchicalInstancedStaticMeshComponent* Bld[4] = {
		Make(TEXT("/Game/Environments/SM_Bld_Glass.SM_Bld_Glass"), 400000.f),
		Make(TEXT("/Game/Environments/SM_Bld_Block.SM_Bld_Block"), 400000.f),
		Make(TEXT("/Game/Environments/SM_Bld_Brick.SM_Bld_Brick"), 400000.f),
		Make(TEXT("/Game/Environments/SM_Bld_Slim.SM_Bld_Slim"), 400000.f) };
	UHierarchicalInstancedStaticMeshComponent* Stand = Make(TEXT("/Game/Environments/SM_Grandstand.SM_Grandstand"), 300000.f);
	// Real trees (CC0 Poly Haven models, Nanite, alpha-masked foliage with wind). They replace the primitive
	// cone/sphere trees whenever they have been imported (Python/ue_trees.py).
	struct FRealTree { UHierarchicalInstancedStaticMeshComponent* Comp; float BaseScale; float Weight; };
	TArray<FRealTree> RealTrees;
	auto AddRealTree = [&](const TCHAR* Path, float TargetHeightCm, float Weight)
	{
		if (UHierarchicalInstancedStaticMeshComponent* C = Make(Path, 150000.f))
		{
			const float MeshHeight = FMath::Max(C->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.f, 1.f);
			RealTrees.Add({ C, TargetHeightCm / MeshHeight, Weight });
		}
	};
	AddRealTree(TEXT("/Game/Environments/Trees/SM_Tree_Jacaranda.SM_Tree_Jacaranda"), 1300.f, 0.50f);
	AddRealTree(TEXT("/Game/Environments/Trees/SM_Tree_PineSapling.SM_Tree_PineSapling"), 420.f, 0.25f);  // really a ~1.3 m sapling: understory only
	AddRealTree(TEXT("/Game/Environments/Trees/SM_Tree_FirSapling.SM_Tree_FirSapling"), 460.f, 0.25f);
	float TotalWeight = 0.f;
	for (const FRealTree& R : RealTrees) { TotalWeight += R.Weight; }
	if (RealTrees.Num() > 0)
	{
		// The primitive trees are not used any more; hide them by emptying their (empty) components.
		Pine = nullptr; Oak = nullptr;
	}
	if (!Pine && !Oak && RealTrees.Num() == 0) { return; }

	const TArray<FVector> Line = GetCenterline();
	TArray<FVector2D> WaterCentres;
	auto DistToTrack = [&Line](float X, float Y)
	{
		float Best = TNumericLimits<float>::Max();
		for (const FVector& P : Line) { Best = FMath::Min(Best, FVector2D::DistSquared(FVector2D(X, Y), FVector2D(P.X, P.Y))); }
		return FMath::Sqrt(Best);
	};

	FRandomStream Rand(20261112);
	TArray<FVector> Footprints; // building centres + clearance radius (cm), so trees keep clear of them

	// Grandstands along the start straight, one each side, facing the track.
	// The FBX Y axis is flipped on import: tiers climb toward -Y at yaw 0, so the south stand needs no rotation.
	if (Stand && !FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		// North side (+Y, opposite the pits): two stands facing the track.
		for (const float X : { 2000.f, 9000.f }) { Stand->AddInstance(FTransform(FRotator(0.f, 180.f, 0.f), FVector(X, 2400.f, 0.f))); }
		// South side (-Y): the pit garage occupies x = -182..+40 m, y = -24..-36 m. One stand remains beyond it, set back behind the pit lane.
		Stand->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(9000.f, -3000.f, 0.f)));
		Footprints.Add(FVector(5500.f, 3400.f, 4200.f));
		Footprints.Add(FVector(9000.f, -4000.f, 4200.f));
		for (float X = -17000.f; X <= 4000.f; X += 3500.f) { Footprints.Add(FVector(X, -3000.f, 2600.f)); }   // the garage
		if (UHierarchicalInstancedStaticMeshComponent* Garage = Make(TEXT("/Game/Environments/SM_PitGarage.SM_PitGarage"), 600000.f))
		{
			Garage->AddInstance(FTransform::Identity);
			UE_LOG(LogTemp, Log, TEXT("FormulaTrack: pit garage placed (%d instance)"), Garage->GetInstanceCount());
		}
		else { UE_LOG(LogTemp, Warning, TEXT("FormulaTrack: /Game/Environments/SM_PitGarage not found (run Python/ue_garage.py)")); }
	}

	// Buildings: one per 90 m cell, in clusters (low-frequency noise), well back from the track.
	constexpr float Cell = 9000.f;
	int32 NumBuildings = 0;

	// Real architecture (Blender/gen_buildings.py): modelled facades, balconies, roofs. Placed at 1:1 scale, never stretched.
	struct FRealBld { UHierarchicalInstancedStaticMeshComponent* Comp; float Radius; float Width = 0.f; float Depth = 0.f; };
	TMap<FName, FRealBld> Real;
	auto AddBld = [&](const TCHAR* Key, const TCHAR* Path, float RadiusCm)
	{
		if (UHierarchicalInstancedStaticMeshComponent* C = Make(Path, 600000.f))
		{
			const FVector Ext = C->GetStaticMesh()->GetBounds().BoxExtent;
			Real.Add(FName(Key), { C, RadiusCm, Ext.X * 2.f, Ext.Y * 2.f });
		}
	};
	AddBld(TEXT("Hotel"),  TEXT("/Game/Environments/Buildings/SM_Bld_Hotel_Med.SM_Bld_Hotel_Med"), 2300.f);
	AddBld(TEXT("AptA"),   TEXT("/Game/Environments/Buildings/SM_Bld_Apt_BrickA.SM_Bld_Apt_BrickA"), 2000.f);
	AddBld(TEXT("AptB"),   TEXT("/Game/Environments/Buildings/SM_Bld_Apt_BrickB.SM_Bld_Apt_BrickB"), 1700.f);
	AddBld(TEXT("Office"), TEXT("/Game/Environments/Buildings/SM_Bld_Office_Concrete.SM_Bld_Office_Concrete"), 2600.f);
	AddBld(TEXT("TowerA"), TEXT("/Game/Environments/Buildings/SM_Bld_Tower_GlassA.SM_Bld_Tower_GlassA"), 2200.f);
	AddBld(TEXT("TowerB"), TEXT("/Game/Environments/Buildings/SM_Bld_Tower_GlassB.SM_Bld_Tower_GlassB"), 1800.f);
	AddBld(TEXT("Lodge"),  TEXT("/Game/Environments/Buildings/SM_Bld_Lodge_Alpine.SM_Bld_Lodge_Alpine"), 1600.f);
	AddBld(TEXT("Brown"),  TEXT("/Game/Environments/Buildings/SM_Bld_NYC_Brownstone.SM_Bld_NYC_Brownstone"), 1500.f);
	AddBld(TEXT("Walkup"), TEXT("/Game/Environments/Buildings/SM_Bld_NYC_Walkup.SM_Bld_NYC_Walkup"), 1300.f);
	AddBld(TEXT("Mixed"),  TEXT("/Game/Environments/Buildings/SM_Bld_NYC_Mixed.SM_Bld_NYC_Mixed"), 1700.f);
	AddBld(TEXT("Deco"),   TEXT("/Game/Environments/Buildings/SM_Bld_NYC_Deco.SM_Bld_NYC_Deco"), 2300.f);
	AddBld(TEXT("Ware"),   TEXT("/Game/Environments/Buildings/SM_Bld_NYC_Warehouse.SM_Bld_NYC_Warehouse"), 2600.f);
	const bool bRealBuildings = Real.Num() > 0;
	// New York mode: zone-aware building choice, a rotated street grid filling the whole region, a park. -APEXOldCity restores the random clusters.
	const bool bNYC = Real.Contains(FName("Mixed")) && !FParse::Param(FCommandLine::Get(), TEXT("APEXOldCity"));
	const int32 LineN = Line.Num();
	// Lap zones by fraction around the lap: 0 Midtown, 1 Times Square (neon), 2 Financial District, 3 Waterfront.
	auto ZoneOfFrac = [](float F) -> int32
	{
		if (F >= 0.60f && F < 0.78f) { return 3; }                                   // waterfront
		if (F >= 0.30f && F < 0.52f) { return 1; }                                   // Times Square / neon
		if ((F >= 0.52f && F < 0.60f) || (F >= 0.78f && F < 0.96f)) { return 2; }   // financial district
		if (F >= 0.12f && F < 0.30f) { return 4; }                                   // residential / mixed-use
		return 0;                                                                    // Midtown
	};
	auto NearestFrac = [&Line, LineN](float X, float Y)
	{
		float Best = TNumericLimits<float>::Max(); int32 BI = 0;
		for (int32 i = 0; i < LineN; ++i) { const float D = FVector2D::DistSquared(FVector2D(X, Y), FVector2D(Line[i].X, Line[i].Y)); if (D < Best) { Best = D; BI = i; } }
		return static_cast<float>(BI) / FMath::Max(LineN, 1);
	};
	auto ZonePick = [](int32 Zone, FRandomStream& R) -> FName
	{
		const float U = R.FRand();
		switch (Zone)
		{
		case 3:  return U < 0.45f ? FName("Ware") : (U < 0.70f ? FName("Brown") : (U < 0.85f ? FName("Walkup") : FName("Mixed")));                  // waterfront: warehouses, lofts
		case 1:  return U < 0.42f ? FName("Mixed") : (U < 0.54f ? FName("Deco") : (U < 0.68f ? FName("TowerA") : (U < 0.80f ? FName("Office") : (U < 0.90f ? FName("Hotel") : FName("Walkup")))));
		case 2:  return U < 0.25f ? FName("Deco") : (U < 0.50f ? FName("TowerA") : (U < 0.70f ? FName("TowerB") : (U < 0.85f ? FName("Office") : FName("Mixed"))));
		case 4:  return U < 0.34f ? FName("Brown") : (U < 0.68f ? FName("Walkup") : (U < 0.86f ? FName("Mixed") : (U < 0.94f ? FName("Ware") : FName("AptA"))));   // residential / mixed-use
		default: return U < 0.20f ? FName("TowerA") : (U < 0.36f ? FName("TowerB") : (U < 0.56f ? FName("Office") : (U < 0.70f ? FName("Deco") : (U < 0.84f ? FName("Hotel") : FName("Mixed")))));   // Midtown: towers, offices, hotels
		}
	};

	// Real buildings are not stretched much: NYC types vary 0.9-1.2 in height, the old towers keep their wider range.
	auto HeightScale = [bNYC](FName Key, FRandomStream& R) -> float
	{
		if (bNYC) { return R.FRandRange(0.9f, 1.2f); }
		return R.FRandRange(1.0f, (Key == FName("TowerA") || Key == FName("TowerB")) ? 1.3f : 1.7f);
	};

	// Which building goes where: towers in the cluster cores, offices and hotels around them, low-rise at the edges.
	auto PickKey = [&](float Density) -> FName
	{
		const float R = Rand.FRand();
		if (Density > 0.45f) { return R < 0.30f ? FName("TowerA") : (R < 0.55f ? FName("TowerB") : (R < 0.78f ? FName("Office") : FName("AptA"))); }
		if (Density > 0.30f) { return R < 0.30f ? FName("Office") : (R < 0.55f ? FName("AptA") : (R < 0.80f ? FName("Hotel") : FName("AptB"))); }
		return R < 0.35f ? FName("AptB") : (R < 0.65f ? FName("Hotel") : (R < 0.85f ? FName("AptA") : FName("Lodge")));
	};

	if (!bNYC && (bRealBuildings || Bld[0] || Bld[1] || Bld[2] || Bld[3]))
	{
		for (float X = -110000.f; X <= 150000.f; X += Cell)
		{
			for (float Y = -60000.f; Y <= 120000.f; Y += Cell)
			{
				const float Cx = X + Rand.FRandRange(-2000.f, 2000.f), Cy = Y + Rand.FRandRange(-2000.f, 2000.f);
				if (DistToTrack(Cx, Cy) < 14000.f) { continue; }                       // 140 m clear of the circuit
				const float Density = FMath::PerlinNoise2D(FVector2D(Cx * 0.000018f + 3.1f, Cy * 0.000018f - 1.7f));
				if (Density < 0.18f || Rand.FRand() > 0.75f) { continue; }              // city clusters only
				const FRotator Yaw(0.f, 90.f * Rand.RandRange(0, 3), 0.f);

				if (bRealBuildings)
				{
					FName Key = PickKey(Density);
					if (!Real.Contains(Key)) { Key = Real.begin()->Key; }
					const FRealBld& R = Real[Key];
					R.Comp->AddInstance(FTransform(Yaw, FVector(Cx, Cy, 0.f)));
					Footprints.Add(FVector(Cx, Cy, R.Radius * 1.25f));
					++NumBuildings;
				}
				else
				{
					const float Height = FMath::Clamp(0.6f + (Density - 0.18f) * 3.2f, 0.6f, 1.6f);
					const int32 Kind = (Density > 0.45f && Rand.FRand() < 0.6f) ? (Rand.FRand() < 0.5f ? 0 : 3) : (Rand.FRand() < 0.55f ? 1 : 2);
					if (UHierarchicalInstancedStaticMeshComponent* C = Bld[Kind])
					{
						C->AddInstance(FTransform(Yaw, FVector(Cx, Cy, 0.f), FVector(1.f, 1.f, Height)));
						Footprints.Add(FVector(Cx, Cy, 4200.f));
						++NumBuildings;
					}
				}
			}
		}
	}

	// Urban canyon (Phase 1): a continuous frontage of buildings on both sides of the first 500 m of the start straight, set behind the
	// grandstands / pit garage, with the corridor kept free of trees. Deterministic per slot, so the street does not repeat in an obvious pattern.
	if (bRealBuildings && !FParse::Param(FCommandLine::Get(), TEXT("APEXNoCanyon")) && !FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		const TCHAR* Keys[] = { TEXT("Office"), TEXT("AptA"), TEXT("Hotel"), TEXT("TowerA"), TEXT("AptB"), TEXT("Office"), TEXT("TowerB"), TEXT("AptA"), TEXT("Hotel"), TEXT("AptB") };
		FRandomStream CR(777);
		int32 Slot = 0;
		for (float X = -25000.f; X <= 25000.f; X += 4600.f, ++Slot)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float Sg = Side == 0 ? 1.f : -1.f;
				const float Y = Sg * (Side == 0 ? 6800.f : 7400.f);                           // north: behind the stands, south: behind the garage
				FName Key = bNYC ? ZonePick(0, CR) : FName(Keys[(Slot * 3 + Side * 4 + CR.RandRange(0, 2)) % 10]);
				if (!Real.Contains(Key)) { continue; }
				const FRealBld& R = Real[Key];
				const float H = HeightScale(Key, CR);
				R.Comp->AddInstance(FTransform(FRotator(0.f, Side == 0 ? 180.f : 0.f, 0.f), FVector(X + CR.FRandRange(-250.f, 250.f), Y, 0.f), FVector(1.f, 1.f, H)));
				Footprints.Add(FVector(X, Y, bNYC ? R.Radius : 3200.f));
				++NumBuildings;
			}
		}
		for (float X = -25000.f; X <= 25000.f; X += 3000.f) { Footprints.Add(FVector(X, 0.f, 9500.f)); }   // no trees in the corridor itself

		// Street dressing, on a fixed grid with explicit rules (never inside the corridor, never across the pit entry/exit gaps).
		// Lamps and LED boards sit on the pavement strip just behind the barriers (8 m from the centreline) and face the track.
		UHierarchicalInstancedStaticMeshComponent* Lamp = Make(TEXT("/Game/Environments/Props/SM_Prop_Lamp.SM_Prop_Lamp"), 120000.f);
		UHierarchicalInstancedStaticMeshComponent* Led = FParse::Param(FCommandLine::Get(), TEXT("APEXOldAds")) ? Make(TEXT("/Game/Environments/Props/SM_Prop_Led.SM_Prop_Led"), 150000.f) : nullptr;   // superseded by the sponsor boards (AAPEXAdvertisingManager)
		UHierarchicalInstancedStaticMeshComponent* Hyd = Make(TEXT("/Game/Environments/Props/SM_Prop_Hydrant.SM_Prop_Hydrant"), 40000.f);
		int32 NLamp = 0, NLed = 0, NHyd = 0;
		for (float X = -25000.f; X <= 25000.f; X += 1200.f)
		{
			const bool bPitSide = X > -23000.f && X < 24500.f;                                  // the pit lane occupies this stretch on -Y
			const int32 Idx = FMath::RoundToInt((X + 25000.f) / 1200.f);
			if (Led && Idx % 2 == 0)
			{
				Led->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(X, 960.f, 0.f))); ++NLed;                      // north, faces the track
				if (bPitSide && !IsBarrierGap(X)) { Led->AddInstance(FTransform(FRotator(0.f, 180.f, 0.f), FVector(X, -960.f, 0.f))); ++NLed; }
			}
			if (Lamp && Idx % 2 == 1)
			{
				Lamp->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(X, 1120.f, 0.f))); ++NLamp;
				LampHeads.Add(FVector(X, 1120.f - 325.f, 860.f));                                  // arm points toward -Y (the track) at yaw 0
			}
			if (Hyd && Idx % 7 == 3) { Hyd->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(X, 1260.f, 0.f))); ++NHyd; }
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack street dressing: %d lamps, %d LED boards, %d hydrants"), NLamp, NLed, NHyd);

		// Sidewalk layer on the north side of the straight: a kerb line at 15 m splitting the apron from the walkway, traffic signals and bus
		// shelters on the walkway facing the track, crosswalks at the side-street mouths (stripes run along Y, so they cross a street running N-S).
		UHierarchicalInstancedStaticMeshComponent* Kerb = Make(TEXT("/Game/Environments/Props/SM_Prop_Kerb.SM_Prop_Kerb"), 150000.f);
		UHierarchicalInstancedStaticMeshComponent* Sig = Make(TEXT("/Game/Environments/Props/SM_Prop_Signal.SM_Prop_Signal"), 120000.f);
		UHierarchicalInstancedStaticMeshComponent* Shel = Make(TEXT("/Game/Environments/Props/SM_Prop_Shelter.SM_Prop_Shelter"), 120000.f);
		UHierarchicalInstancedStaticMeshComponent* Zeb = Make(TEXT("/Game/Environments/Props/SM_Prop_Crosswalk.SM_Prop_Crosswalk"), 150000.f);
		int32 NKerb = 0, NSig = 0, NShel = 0, NZeb = 0;
		for (float X = -24000.f; X <= 24000.f; X += 400.f)
		{
			if (Kerb) { Kerb->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(X, 1500.f, 0.f))); ++NKerb; }
		}
		for (float X = -22000.f; X <= 22000.f; X += 6000.f)
		{
			if (Sig) { Sig->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(X + 300.f, 1640.f, 0.f))); ++NSig; }
			if (Zeb) { Zeb->AddInstance(FTransform(FRotator(0.f, 90.f, 0.f), FVector(X + 3000.f, 2300.f, 0.f))); ++NZeb; }
		}
		for (float X : { -15000.f, -3000.f, 9000.f, 19000.f })
		{
			if (Shel) { Shel->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(X, 1900.f, 0.f))); ++NShel; }
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack sidewalk layer: %d kerbs, %d signals, %d shelters, %d crosswalks"), NKerb, NSig, NShel, NZeb);
	}

	// Urban frontage around the rest of the lap: a building every ~42 m on each side of the centreline at 60 m, facing the track, kept clear of
	// other track sections, the start-straight canyon and each other. (Phase 3 of the New York brief: no grass verge anywhere near the track.)
	if (bRealBuildings && !FParse::Param(FCommandLine::Get(), TEXT("APEXNoCanyon")) && !FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		const TCHAR* Keys[] = { TEXT("Office"), TEXT("AptA"), TEXT("Hotel"), TEXT("TowerA"), TEXT("AptB"), TEXT("Office"), TEXT("TowerB"), TEXT("AptA"), TEXT("Hotel"), TEXT("AptB"), TEXT("Lodge") };
		FRandomStream FR(4242);
		int32 NLap = 0;
		for (int32 i = 0; i < Line.Num(); i += 5)
		{
			const FVector P = Line[i];
			if (P.X > -26000.f && P.X < 26000.f && FMath::Abs(P.Y) < 12000.f) { continue; }          // start straight handled above
			const FVector Dir = (Line[(i + 1) % Line.Num()] - Line[(i + Line.Num() - 1) % Line.Num()]).GetSafeNormal2D();
			const FVector Left(-Dir.Y, Dir.X, 0.f);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float Sg = Side == 0 ? 1.f : -1.f;
				const FVector Pos = P + Left * Sg * (5600.f + FR.FRandRange(0.f, 900.f));
				if (DistToTrack(Pos.X, Pos.Y) < 5000.f) { continue; }
				if (Side == 0 && i >= Line.Num() * 60 / 100 && i < Line.Num() * 78 / 100) { continue; }      // waterfront: open water on the left-hand side                                       // inside a bend / next to another section
				bool bClash = false;
				for (const FVector& F : Footprints) { if (FVector2D::DistSquared(FVector2D(F.X, F.Y), FVector2D(Pos.X, Pos.Y)) < FMath::Square(F.Z + 1800.f)) { bClash = true; break; } }
				if (bClash) { continue; }
				FName Key = bNYC ? ZonePick(ZoneOfFrac(static_cast<float>(i) / FMath::Max(Line.Num(), 1)), FR) : FName(Keys[FR.RandRange(0, 10)]);
				if (!Real.Contains(Key)) { continue; }
				const FRealBld& R = Real[Key];
				const FVector ToTrack = -Left * Sg;
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToTrack.Y, ToTrack.X)) - 90.f;    // facade faces the track
				const float H = HeightScale(Key, FR);
				R.Comp->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Pos, FVector(1.f, 1.f, H)));
				Footprints.Add(FVector(Pos.X, Pos.Y, bNYC ? R.Radius : 3200.f));
				++NumBuildings; ++NLap;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack lap frontage: %d buildings"), NLap);

		// Neon district (Times Square-style): over one stretch of the lap, tall video walls and vertical blade signs stand on the pavement
		// at 19 m from the centreline, facing the track. Both use M_APEX_Led, so they glow at night and read as lit displays by day.
		{
			UHierarchicalInstancedStaticMeshComponent* NScr = Make(TEXT("/Game/Environments/Props/SM_Prop_NeonScreen.SM_Prop_NeonScreen"), 400000.f);
			UHierarchicalInstancedStaticMeshComponent* NBld = Make(TEXT("/Game/Environments/Props/SM_Prop_NeonBlade.SM_Prop_NeonBlade"), 400000.f);
			int32 NScreens = 0, NBlades = 0, Count = 0;
			float Acc = 0.f;
			const int32 N = Line.Num();
			for (int32 i = N * 30 / 100; i < N * 52 / 100; ++i)
			{
				Acc += FVector::Dist2D(Line[i], Line[(i + N - 1) % N]);
				if (Acc < 2600.f) { continue; }
				Acc = 0.f;
				const FVector P = Line[i];
				if (P.X > -26000.f && P.X < 26000.f && FMath::Abs(P.Y) < 12000.f) { continue; }
				const FVector Dir = (Line[(i + 1) % N] - Line[(i + N - 1) % N]).GetSafeNormal2D();
				const FVector Left(-Dir.Y, Dir.X, 0.f);
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const float Sg = Side == 0 ? 1.f : -1.f;
					const FVector Pos = P + Left * Sg * 1900.f;
					if (DistToTrack(Pos.X, Pos.Y) < 1500.f) { continue; }                                      // not inside a bend or across another section
					const FVector ToTrack = -Left * Sg;
					// Mesh front is -Y after import (see the lamp), so yaw so that local -Y points at the track.
					const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToTrack.Y, ToTrack.X)) + 90.f;
					UHierarchicalInstancedStaticMeshComponent* C = ((Count + Side) % 3 == 0) ? NBld : NScr;
					if (!C) { continue; }
					C->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Pos));
					(C == NBld ? NBlades : NScreens)++;
				}
				++Count;
			}
			UE_LOG(LogTemp, Log, TEXT("FormulaTrack neon district: %d screens, %d blades"), NScreens, NBlades);
		}
	}

	// Waterfront: over 60-78% of the lap the left-hand side is open harbour. One 33 m x 270 m water plane per ~30 m of track, starting just
	// past the paved ribbon (62 m), with a kerb line along the quay edge. Strips are skipped where they would cross another section.
	{
		UHierarchicalInstancedStaticMeshComponent* Water = Make(TEXT("/Engine/BasicShapes/Plane.Plane"), 900000.f);
		UHierarchicalInstancedStaticMeshComponent* QuayKerb = Make(TEXT("/Game/Environments/Props/SM_Prop_Kerb.SM_Prop_Kerb"), 200000.f);
		if (Water) { Water->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_APEX_Water.M_APEX_Water"))); Water->SetCastShadow(false); }
		float Acc = 0.f; int32 NW = 0;
		const int32 N = Line.Num();
		for (int32 i = N * 60 / 100; i < N * 78 / 100; ++i)
		{
			Acc += FVector::Dist2D(Line[i], Line[(i + N - 1) % N]);
			if (Acc < 3000.f) { continue; }
			Acc = 0.f;
			const FVector P = Line[i];
			const FVector Dir = (Line[(i + 1) % N] - Line[(i + N - 1) % N]).GetSafeNormal2D();
			const FVector Left(-Dir.Y, Dir.X, 0.f);
			const FVector Edge = P + Left * 6200.f, Mid = P + Left * 21000.f;
			if (DistToTrack(Edge.X, Edge.Y) < 5500.f || DistToTrack(Mid.X, Mid.Y) < 7000.f) { continue; }
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
			if (Water) { Water->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Mid + FVector(0.f, 0.f, 3.f), FVector(33.f, 270.f, 1.f))); ++NW; }
			if (QuayKerb) { QuayKerb->AddInstance(FTransform(FRotator(0.f, Yaw + 90.f, 0.f), Edge)); }
			WaterCentres.Add(FVector2D(Mid.X, Mid.Y));
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack waterfront: %d water strips"), NW);
	}

	// ---- New York city fill ------------------------------------------------------------------------------------------------------------------
	// A Manhattan-style grid rotated 29 degrees: 260 m x 80 m blocks separated by 18 m streets, a raised sidewalk plate per block, and a row of
	// buildings along each long edge facing the street. Building types come from the lap zone nearest to the block (Midtown, Times Square,
	// Financial District, Waterfront). Blocks near the circuit, the harbour or the park are skipped. One block-sized park keeps the trees.
	FVector2D ParkMin(1e9f, 1e9f), ParkMax(-1e9f, -1e9f);
	bool bPark = false;
	if (bNYC && !FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		const double T0 = FPlatformTime::Seconds();
		FVector2D Cen(0.f, 0.f);
		for (const FVector& P : Line) { Cen += FVector2D(P.X, P.Y); }
		Cen /= FMath::Max(Line.Num(), 1);
		float BestD = FParse::Param(FCommandLine::Get(), TEXT("APEXPark")) ? TNumericLimits<float>::Max() : -1.f;   // no park unless -APEXPark: the city fills everything
		for (float Cx = -90000.f; Cx <= 125000.f && BestD >= 0.f; Cx += 5000.f)
		{
			for (float Cy = -40000.f; Cy <= 100000.f; Cy += 5000.f)
			{
				const float Dc = FVector2D::Distance(FVector2D(Cx, Cy), Cen);
				if (Dc >= BestD) { continue; }
				bool bOk = true;
				for (const FVector& P : Line)   // no centreline point within 90 m of the rectangle
				{
					if (P.X > Cx - 24000.f && P.X < Cx + 24000.f && P.Y > Cy - 44000.f && P.Y < Cy + 44000.f) { bOk = false; break; }
				}
				for (const FVector2D& W : WaterCentres) { if (FVector2D::Distance(W, FVector2D(Cx, Cy)) < 40000.f) { bOk = false; break; } }
				if (bOk) { BestD = Dc; ParkMin = FVector2D(Cx - 15000.f, Cy - 35000.f); ParkMax = FVector2D(Cx + 15000.f, Cy + 35000.f); bPark = true; }
			}
		}
		auto InPark = [&](float X, float Y, float Pad) { return bPark && X > ParkMin.X - Pad && X < ParkMax.X + Pad && Y > ParkMin.Y - Pad && Y < ParkMax.Y + Pad; };
		if (bPark)
		{
			if (UHierarchicalInstancedStaticMeshComponent* Lawn = Make(TEXT("/Engine/BasicShapes/Plane.Plane"), 900000.f))
			{
				Lawn->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Grass.M_Grass")));
				Lawn->CastShadow = false;
				// M_Grass maps one UV unit to ~8 m, so the lawn is tiled from 16 m planes (UV 0-1 each) rather than one stretched plane.
				for (float X = ParkMin.X + 800.f; X < ParkMax.X; X += 1600.f)
				{
					for (float Y = ParkMin.Y + 800.f; Y < ParkMax.Y; Y += 1600.f) { Lawn->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 3.f), FVector(16.f, 16.f, 1.f))); }
				}
			}
		}
		UHierarchicalInstancedStaticMeshComponent* Plate = Make(TEXT("/Engine/BasicShapes/Cube.Cube"), 900000.f);
		if (Plate) { Plate->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Buildings/MI_Bld_Concrete_Grey.MI_Bld_Concrete_Grey"))); Plate->CastShadow = false; }
		const float RotDeg = 29.f, Rr = FMath::DegreesToRadians(RotDeg);
		const FVector2D Ax(FMath::Cos(Rr), FMath::Sin(Rr)), Ay(-FMath::Sin(Rr), FMath::Cos(Rr));
		const float BU = 13000.f, BV = 4000.f, CellU = 27800.f, CellV = 9800.f;
		const FVector2D Origin(17500.f, 30000.f);
		FRandomStream CityR(909);
		// Sidewalk clutter and scaffolding: a few props in front of each building (kept to the plate, beside the street).
		UHierarchicalInstancedStaticMeshComponent* PScaf = Make(TEXT("/Game/Environments/Props/SM_Prop_Scaffold.SM_Prop_Scaffold"), 120000.f);
		UHierarchicalInstancedStaticMeshComponent* PBin = Make(TEXT("/Game/Environments/Props/SM_Prop_Bin.SM_Prop_Bin"), 50000.f);
		UHierarchicalInstancedStaticMeshComponent* PBags = Make(TEXT("/Game/Environments/Props/SM_Prop_Bags.SM_Prop_Bags"), 50000.f);
		UHierarchicalInstancedStaticMeshComponent* PBol = Make(TEXT("/Game/Environments/Props/SM_Prop_Bollard.SM_Prop_Bollard"), 50000.f);
		UHierarchicalInstancedStaticMeshComponent* PCar = Make(TEXT("/Game/Environments/Props/SM_Traffic_Car.SM_Traffic_Car"), 60000.f);
		int32 NScaf = 0, NClutter = 0;
		int32 NBlocks = 0, NFill = 0;
		const int32 FootprintsBefore = Footprints.Num();   // clash test is against pre-existing structures only; fill buildings are spaced by construction
		int32 PerZone[5] = { 0, 0, 0, 0, 0 };
		// Far field: every block of the 12 km ground that is further than NearR from the circuit gets a whole-block skyline mass (one instance) instead of
		// individually modelled buildings, so no direction ever shows empty ground. Category by district: financial and midtown tall, residential and waterfront low.
		const float NearR = 80000.f;
		UHierarchicalInstancedStaticMeshComponent* FarComp[4][3] = {};
		const TCHAR* FarCat[4] = { TEXT("Low"), TEXT("Mid"), TEXT("High"), TEXT("Fin") };
		for (int32 c = 0; c < 4; ++c) { for (int32 n = 0; n < 3; ++n) { FarComp[c][n] = Make(*FString::Printf(TEXT("/Game/Environments/Far/SM_Far_%s%d.SM_Far_%s%d"), FarCat[c], n, FarCat[c], n), 900000.f); } }
		int32 NFar = 0;
		for (int32 iu = -22; iu <= 22; ++iu)
		{
			for (int32 iv = -62; iv <= 62; ++iv)
			{
				const FVector2D C = Origin + Ax * (iu * CellU) + Ay * (iv * CellV);
				if (FMath::Abs(C.X) > 585000.f || FMath::Abs(C.Y) > 585000.f) { continue; }
				const float DistC = DistToTrack(C.X, C.Y);
				if (DistC > NearR)
				{
					bool bW = false;
					for (const FVector2D& Wc : WaterCentres) { if (FVector2D::DistSquared(Wc, C) < FMath::Square(24000.f)) { bW = true; break; } }
					if (bW) { continue; }
					const int32 Zn = ZoneOfFrac(NearestFrac(C.X, C.Y));
					const float U = CityR.FRand();
					int32 Cat = 1;
					switch (Zn)
					{
					case 2:  Cat = U < 0.70f ? 3 : 2; break;
					case 0:  Cat = U < 0.10f ? 3 : (U < 0.60f ? 2 : 1); break;
					case 1:  Cat = U < 0.50f ? 2 : 1; break;
					case 4:  Cat = U < 0.80f ? 0 : 1; break;
					default: Cat = U < 0.70f ? 0 : 1; break;
					}
					if (UHierarchicalInstancedStaticMeshComponent* FC = FarComp[Cat][CityR.RandRange(0, 2)])
					{
						FC->AddInstance(FTransform(FRotator(0.f, RotDeg, 0.f), FVector(C.X, C.Y, 0.f), FVector(1.f, 1.f, CityR.FRandRange(0.85f, 1.2f))));
						if (Plate) { Plate->AddInstance(FTransform(FRotator(0.f, RotDeg, 0.f), FVector(C.X, C.Y, 7.5f), FVector(BU * 2.f / 100.f, BV * 2.f / 100.f, 0.15f))); }
						++NFar;
					}
					continue;
				}
				if (C.X < -105000.f || C.X > 140000.f || C.Y < -55000.f || C.Y > 115000.f) { /* still within NearR: detailed block below */ }
				if (InPark(C.X, C.Y, BU * 0.5f)) { continue; }
				bool bWater = false;
				for (const FVector2D& W : WaterCentres) { if (FVector2D::DistSquared(W, C) < FMath::Square(24000.f)) { bWater = true; break; } }
				if (bWater) { continue; }
				// sidewalk plate: only if the whole block clears the circuit
				bool bClear = true;
				for (int32 k = 0; k < 9 && bClear; ++k)
				{
					const FVector2D Q = C + Ax * ((k % 3 - 1) * BU) + Ay * ((k / 3 - 1) * BV);
					if (DistToTrack(Q.X, Q.Y) < 6000.f) { bClear = false; }
				}
				if (bClear)
				{
					const FVector2D SA = C + Ax * (-BU) + Ay * (BV + 900.f), SB = C + Ax * BU + Ay * (BV + 900.f), SM = (SA + SB) * 0.5f;
					if (DistToTrack(SA.X, SA.Y) > 4500.f && DistToTrack(SB.X, SB.Y) > 4500.f && DistToTrack(SM.X, SM.Y) > 4500.f) { StreetSegs.Add(FVector4(SA.X, SA.Y, SB.X, SB.Y)); }
				}
				if (bClear && Plate) { Plate->AddInstance(FTransform(FRotator(0.f, RotDeg, 0.f), FVector(C.X, C.Y, 7.5f), FVector(BU * 2.f / 100.f, BV * 2.f / 100.f, 0.15f))); ++NBlocks; }
				const int32 Zone = ZoneOfFrac(NearestFrac(C.X, C.Y));
				for (int32 Row = 0; Row < 2; ++Row)
				{
					const float Sg = Row == 0 ? 1.f : -1.f;
					float U = -BU + 800.f;
					while (U < BU - 800.f)
					{
						const FName Key = ZonePick(Zone, CityR);
						const FRealBld* R = Real.Find(Key);
						if (!R) { U += 2500.f; continue; }
						const float W = R->Width, V = Sg * (BV - R->Depth * 0.5f - 120.f);
						const FVector2D P = C + Ax * (U + W * 0.5f) + Ay * V;
						U += W + CityR.FRandRange(100.f, 500.f);
						if (U > BU + 400.f) { break; }
						if (DistToTrack(P.X, P.Y) < 4800.f + R->Radius || InPark(P.X, P.Y, R->Radius)) { continue; }
						bool bW = false;
						for (const FVector2D& Wc : WaterCentres) { if (FVector2D::DistSquared(Wc, P) < FMath::Square(17000.f)) { bW = true; break; } }
						if (bW) { continue; }
						bool bClash = false;
						for (int32 fi = 0; fi < FootprintsBefore; ++fi) { const FVector& F = Footprints[fi]; if (FVector2D::DistSquared(FVector2D(F.X, F.Y), P) < FMath::Square(F.Z + R->Radius * 0.85f)) { bClash = true; break; } }
						if (bClash) { continue; }
						const float Yaw = RotDeg + (Row == 0 ? 0.f : 180.f);   // front (UE +Y at yaw 0) faces the street on this side of the block
						R->Comp->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, 0.f), FVector(1.f, 1.f, HeightScale(Key, CityR))));
						Footprints.Add(FVector(P.X, P.Y, R->Radius));
						++NFill; ++PerZone[Zone]; ++NumBuildings;
						{
							const float YawRad = FMath::DegreesToRadians(Yaw);
							const FVector2D Lat(FMath::Cos(YawRad), FMath::Sin(YawRad)), Fr(-FMath::Sin(YawRad), FMath::Cos(YawRad));   // building front = local +Y
							const FVector2D FrontFace = P + Fr * (R->Depth * 0.5f);
							const float HS = 1.f;
							if (PScaf && W > 1800.f && Key != FName("Ware") && CityR.FRand() < 0.06f)
							{
								const FVector2D Q = FrontFace + Fr * 25.f;
								PScaf->AddInstance(FTransform(FRotator(0.f, Yaw + 180.f, 0.f), FVector(Q.X, Q.Y, 0.f), FVector(FMath::Clamp(W / 600.f, 1.f, 5.f) * HS, 1.f, 1.f)));
								++NScaf;
							}
							else
							{
								if (PCar && CityR.FRand() < 0.40f)                        // a parked car at the kerb, long axis along the street
								{
									const FVector2D Q = FrontFace + Fr * 350.f + Lat * CityR.FRandRange(-W * 0.40f, W * 0.40f);
									PCar->AddInstance(FTransform(FRotator(0.f, Yaw + (CityR.FRand() < 0.5f ? 0.f : 180.f), 0.f), FVector(Q.X, Q.Y, 0.f)));
									++NClutter;
								}
								if (RealTrees.Num() > 0 && CityR.FRand() < 0.20f)           // street tree at the kerb (the full-size Jacaranda, scaled down to ~7 m)
								{
									const FVector2D Q = FrontFace + Fr * 230.f + Lat * CityR.FRandRange(-W * 0.35f, W * 0.35f);
									const float Sc = RealTrees[0].BaseScale * CityR.FRandRange(0.45f, 0.62f);
									RealTrees[0].Comp->AddInstance(FTransform(FRotator(0.f, CityR.FRandRange(0.f, 360.f), 0.f), FVector(Q.X, Q.Y, 0.f), FVector(Sc)));
									++NClutter;
								}
								if (PBin && CityR.FRand() < 0.30f) { const FVector2D Q = FrontFace + Fr * 90.f + Lat * (W * 0.5f - 160.f); PBin->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Q.X, Q.Y, 0.f))); ++NClutter; }
								if (PBags && CityR.FRand() < 0.22f) { const FVector2D Q = FrontFace + Fr * 100.f - Lat * (W * 0.5f - 200.f); PBags->AddInstance(FTransform(FRotator(0.f, CityR.FRandRange(0.f, 360.f), 0.f), FVector(Q.X, Q.Y, 0.f))); ++NClutter; }
								if (PBol && CityR.FRand() < 0.14f)
								{
									for (int32 b = 0; b < 4; ++b) { const FVector2D Q = FrontFace + Fr * 110.f + Lat * (-300.f + b * 200.f); PBol->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Q.X, Q.Y, 0.f))); ++NClutter; }
								}
							}
						}
					}
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack far field: %d block masses"), NFar);
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack city clutter: %d scaffolds, %d bins/bags/bollards/trees/parked cars, %d street segments"), NScaf, NClutter, StreetSegs.Num());
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack park rect: %.0f,%.0f to %.0f,%.0f"), ParkMin.X, ParkMin.Y, ParkMax.X, ParkMax.Y);
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack city fill: %d blocks, %d buildings (midtown %d, times square %d, financial %d, waterfront %d, residential %d), park %s, %.2f s"),
			NBlocks, NFill, PerZone[0], PerZone[1], PerZone[2], PerZone[3], PerZone[4], bPark ? TEXT("yes") : TEXT("no"), FPlatformTime::Seconds() - T0);
	}

	// ---- Streetlights around the rest of the lap: every ~38 m, alternating sides, arm over the barrier. Only the mesh is placed here; the lights
	// themselves are pooled by AAPEXStreetLights (a few dozen real lights cannot cover hundreds of lamps).
	if (!FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		UHierarchicalInstancedStaticMeshComponent* RingLamp = Make(TEXT("/Game/Environments/Props/SM_Prop_Lamp.SM_Prop_Lamp"), 150000.f);
		float Acc = 0.f; int32 K = 0, NRing = 0;
		for (int32 i = 0; i < Line.Num() && RingLamp; ++i)
		{
			Acc += FVector::Dist2D(Line[i], Line[(i + Line.Num() - 1) % Line.Num()]);
			if (Acc < 3800.f) { continue; }
			Acc = 0.f;
			const FVector P = Line[i];
			if (P.X > -26000.f && P.X < 26000.f && FMath::Abs(P.Y) < 12000.f) { continue; }               // start straight has its own lamps
			const FVector Dir = (Line[(i + 1) % Line.Num()] - Line[(i + Line.Num() - 1) % Line.Num()]).GetSafeNormal2D();
			const FVector Left(-Dir.Y, Dir.X, 0.f);
			const float Sg = (K++ % 2) ? 1.f : -1.f;
			const FVector Pos = P + Left * Sg * 1120.f;
			if (DistToTrack(Pos.X, Pos.Y) < 1000.f) { continue; }
			const FVector Toward(-Left.X * Sg, -Left.Y * Sg, 0.f);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X)) + 90.f;
			RingLamp->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Pos.X, Pos.Y, 0.f)));
			LampHeads.Add(FVector(Pos.X, Pos.Y, 860.f) + Toward * 325.f);
			++NRing;
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack streetlights: %d around the lap, %d heads in total"), NRing, LampHeads.Num());
	}

	// ---- People: spectators along the sidewalks, pit crews at the boxes, marshals behind the barriers -----------------------------------------
	// Bodies are static meshes animated in the vertex shader (per-person bob/jump), so a thousand of them cost nothing per frame on the CPU.
	// The mesh front is -Y at yaw 0, so facing direction d needs yaw = atan2(d.y, d.x) + 90.
	if (!FParse::Param(FCommandLine::Get(), TEXT("APEXNoPeople")) && !FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		UHierarchicalInstancedStaticMeshComponent* PplStand = Make(TEXT("/Game/Environments/People/SM_Person_Stand.SM_Person_Stand"), 60000.f);
		UHierarchicalInstancedStaticMeshComponent* PplCheer = Make(TEXT("/Game/Environments/People/SM_Person_Cheer.SM_Person_Cheer"), 60000.f);
		UHierarchicalInstancedStaticMeshComponent* Marshal = Make(TEXT("/Game/Environments/People/SM_Person_Marshal.SM_Person_Marshal"), 50000.f);
		UHierarchicalInstancedStaticMeshComponent* Crew[10] = {};
		for (int32 k = 0; k < 10; ++k) { Crew[k] = Make(*FString::Printf(TEXT("/Game/Environments/People/SM_Person_Crew_T%d.SM_Person_Crew_T%d"), k, k), 50000.f); }
		auto Rec = [this](uint8 Kind, const FTransform& T) { NPCSlots.Add(T); NPCKinds.Add(Kind); };
		auto YawFacing = [](const FVector2D& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) + 90.f; };
		FRandomStream PR(515);
		int32 NCrowd = 0, NCrew = 0, NMarshal = 0;
		if (PplStand && PplCheer)
		{
			// Start straight, north walkway: two ragged rows behind the kerb, facing the track.
			for (float X = -22000.f; X <= 24000.f; X += PR.FRandRange(90.f, 170.f))
			{
				for (int32 Row = 0; Row < 2; ++Row)
				{
					if (PR.FRand() < 0.2f) { continue; }
					UHierarchicalInstancedStaticMeshComponent* C = PR.FRand() < 0.45f ? PplCheer : PplStand;
					{ const FTransform T(FRotator(0.f, YawFacing(FVector2D(0.f, -1.f)) + PR.FRandRange(-25.f, 25.f), 0.f), FVector(X, 1560.f + Row * 85.f + PR.FRandRange(-20.f, 20.f), 0.f), FVector(PR.FRandRange(0.94f, 1.08f))); C->AddInstance(T); Rec(C == PplCheer ? 1 : 0, T); }
					++NCrowd;
				}
			}
			// Around the lap: density by zone (Times Square busiest, waterfront thinnest), always facing the track.
			const float Density[5] = { 0.30f, 0.65f, 0.22f, 0.10f, 0.35f };
			float Acc = 0.f;
			for (int32 i = 0; i < Line.Num(); ++i)
			{
				Acc += FVector::Dist2D(Line[i], Line[(i + Line.Num() - 1) % Line.Num()]);
				if (Acc < 240.f) { continue; }
				Acc = 0.f;
				const FVector P = Line[i];
				if (P.X > -26000.f && P.X < 26000.f && FMath::Abs(P.Y) < 12000.f) { continue; }               // start straight handled above
				const int32 Zone = ZoneOfFrac(static_cast<float>(i) / FMath::Max(Line.Num(), 1));
				if (PR.FRand() > Density[Zone]) { continue; }
				const FVector Dir = (Line[(i + 1) % Line.Num()] - Line[(i + Line.Num() - 1) % Line.Num()]).GetSafeNormal2D();
				const FVector Left(-Dir.Y, Dir.X, 0.f);
				const float Sg = PR.FRand() < 0.5f ? 1.f : -1.f;
				const FVector Pos = P + Left * Sg * PR.FRandRange(1150.f, 1500.f) + Dir * PR.FRandRange(-100.f, 100.f);
				if (DistToTrack(Pos.X, Pos.Y) < 1050.f) { continue; }
				const FVector2D Face(-Left.X * Sg, -Left.Y * Sg);
				UHierarchicalInstancedStaticMeshComponent* C = PR.FRand() < 0.5f ? PplCheer : PplStand;
				{ const FTransform T(FRotator(0.f, YawFacing(Face) + PR.FRandRange(-30.f, 30.f), 0.f), FVector(Pos.X, Pos.Y, 0.f), FVector(PR.FRandRange(0.94f, 1.08f))); C->AddInstance(T); Rec(C == PplCheer ? 1 : 0, T); }
				++NCrowd;
			}
		}
		// Pit crews: fourteen per team in the bay mouth between the working lane and the garage front, facing the lane (+Y).
		for (int32 k = 0; k < 10; ++k)
		{
			if (!Crew[k]) { continue; }
			const float BoxX = -17000.f + 2200.f * k;
			for (int32 j = 0; j < 14; ++j)
			{
				const float X = BoxX - 650.f + j * 100.f + PR.FRandRange(-15.f, 15.f);
				const float Y = -2260.f - (j % 3) * 45.f;
				{ const FTransform T(FRotator(0.f, YawFacing(FVector2D(0.f, 1.f)) + PR.FRandRange(-20.f, 20.f), 0.f), FVector(X, Y, 0.f)); Rec(static_cast<uint8>(10 + k), T);   /* crew exist only as skeletal NPCs */ }
				++NCrew;
			}
		}
		// Marshals: every ~90 m alternating sides, standing just behind the barrier, facing the track.
		if (Marshal)
		{
			float Acc = 0.f; int32 K = 0;
			for (int32 i = 0; i < Line.Num(); ++i)
			{
				Acc += FVector::Dist2D(Line[i], Line[(i + Line.Num() - 1) % Line.Num()]);
				if (Acc < 9000.f) { continue; }
				Acc = 0.f;
				const FVector P = Line[i];
				const FVector Dir = (Line[(i + 1) % Line.Num()] - Line[(i + Line.Num() - 1) % Line.Num()]).GetSafeNormal2D();
				const FVector Left(-Dir.Y, Dir.X, 0.f);
				const float Sg = (K++ % 2) ? 1.f : -1.f;
				const FVector Pos = P + Left * Sg * 1010.f;
				if (P.X > -26000.f && P.X < 26000.f && FMath::Abs(P.Y) < 12000.f && Pos.Y < 0.f) { continue; }   // not in the pit lane
				if (DistToTrack(Pos.X, Pos.Y) < 900.f) { continue; }
				for (int32 m = 0; m < 2; ++m)
				{
					{ const FTransform T(FRotator(0.f, YawFacing(FVector2D(-Left.X * Sg, -Left.Y * Sg)), 0.f), FVector(Pos.X + Dir.X * m * 130.f, Pos.Y + Dir.Y * m * 130.f, 0.f)); Marshal->AddInstance(T); Rec(3, T); }
					++NMarshal;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("FormulaTrack people: %d spectators, %d pit crew, %d marshals"), NCrowd, NCrew, NMarshal);
	}

	// Trees: jittered grid, forest where the noise is high, kept clear of the track and the buildings.
	int32 NumTrees = 0;
	const bool bSkipTrees = FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase"));   // keep the showcase view clear
	for (float X = -100000.f; X <= 135000.f && !bSkipTrees; X += 2000.f)
	{
		for (float Y = -50000.f; Y <= 110000.f; Y += 2000.f)
		{
			const float Tx = X + Rand.FRandRange(-900.f, 900.f), Ty = Y + Rand.FRandRange(-900.f, 900.f);
			const float Forest = FMath::PerlinNoise2D(FVector2D(Tx * 0.00005f - 2.3f, Ty * 0.00005f + 5.9f));
			if (Rand.FRand() > FMath::Clamp(0.25f + Forest * 1.1f, 0.04f, 0.9f)) { continue; }
			if (DistToTrack(Tx, Ty) < 6200.f) { continue; }
			if (bNYC && !(bPark && Tx > ParkMin.X && Tx < ParkMax.X && Ty > ParkMin.Y && Ty < ParkMax.Y)) { continue; }   // New York: trees only in the park
			bool bInWater = false;
			for (const FVector2D& W : WaterCentres) { if (FVector2D::DistSquared(W, FVector2D(Tx, Ty)) < FMath::Square(17000.f)) { bInWater = true; break; } }
			if (bInWater) { continue; }                            // clear of barrier + 18 m
			bool bNearBuilding = false;
			for (const FVector& F : Footprints) { if (FVector2D::DistSquared(FVector2D(F.X, F.Y), FVector2D(Tx, Ty)) < FMath::Square(F.Z)) { bNearBuilding = true; break; } }
			if (bNearBuilding) { continue; }

			if (RealTrees.Num() > 0)
			{
				float Pick = Rand.FRand() * TotalWeight;
				const FRealTree* Chosen = &RealTrees.Last();
				for (const FRealTree& R : RealTrees) { if (Pick < R.Weight) { Chosen = &R; break; } Pick -= R.Weight; }
				const float Scale = Chosen->BaseScale * Rand.FRandRange(0.8f, 1.3f);   // age / size variation
				const FRotator Lean(Rand.FRandRange(-2.f, 2.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-2.f, 2.f));
				Chosen->Comp->AddInstance(FTransform(Lean, FVector(Tx, Ty, 0.f), FVector(Scale * Rand.FRandRange(0.92f, 1.08f), Scale * Rand.FRandRange(0.92f, 1.08f), Scale)));
			}
			else
			{
				UHierarchicalInstancedStaticMeshComponent* C = (Rand.FRand() < 0.55f && Pine) ? Pine : (Oak ? Oak : Pine);
				const float Scale = Rand.FRandRange(0.8f, 1.5f);
				C->AddInstance(FTransform(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(Tx, Ty, 0.f),
					FVector(Scale * Rand.FRandRange(0.9f, 1.1f), Scale * Rand.FRandRange(0.9f, 1.1f), Scale)));
			}
			++NumTrees;
		}
	}
	// Debug: -APEXBuildingShowcase puts one of each building in two rows south of the start straight, for close inspection.
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXBuildingShowcase")))
	{
		struct FSpot { const TCHAR* Key; float X, Y; };
		const FSpot Spots[] = { { TEXT("Hotel"), -12500.f, -8000.f }, { TEXT("AptA"), -5000.f, -8000.f }, { TEXT("Office"), 3000.f, -8000.f }, { TEXT("Lodge"), 11000.f, -8000.f },
			{ TEXT("AptB"), -9000.f, -16500.f }, { TEXT("TowerB"), -1500.f, -16500.f }, { TEXT("TowerA"), 7500.f, -16500.f } };
		for (const FSpot& Sp : Spots)
		{
			if (const FRealBld* R = Real.Find(FName(Sp.Key))) { R->Comp->AddInstance(FTransform(FRotator(0.f, 0.f, 0.f), FVector(Sp.X, Sp.Y, 0.f))); }
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXTreeShowcase")))
	{
		for (int32 k = 0; k < RealTrees.Num(); ++k)
		{
			const FRealTree& R = RealTrees[k];
			R.Comp->AddInstance(FTransform(FRotator(0.f, 20.f * k, 0.f), FVector(-600.f + 1700.f * k, 1700.f, 0.f), FVector(R.BaseScale)));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("FormulaTrack scenery: %d trees, %d buildings, grandstands %d"), NumTrees, NumBuildings, Stand ? Stand->GetInstanceCount() : 0);
}

void AFormulaTrack::BuildGeometry()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Cube)
	{
		UE_LOG(LogTemp, Error, TEXT("FormulaTrack: /Engine/BasicShapes/Cube missing, no geometry built"));
		return;
	}

	MakeSolid(Road, Cube, Mat, FLinearColor(0.04f, 0.04f, 0.045f), this);
	MakeSolid(Barriers, Cube, Mat, FLinearColor(0.8f, 0.05f, 0.05f), this);

	Ground->SetStaticMesh(Cube);
	if (Mat)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.25f, 0.05f));
		Ground->SetMaterial(0, MID);
	}
	// 4 km square slab whose top face sits 10 cm below the road.
	Ground->SetRelativeScale3D(FVector(4000.f, 4000.f, 1.f));
	Ground->SetRelativeLocation(FVector(0.f, 0.f, -60.f));

	const TArray<FVector> Line = GetCenterline();
	const int32 N = Line.Num();
	const float BarrierOffset = RoadWidth * 0.5f + 100.f;

	for (int32 i = 0; i < N; ++i)
	{
		const FVector A = Line[i];
		const FVector B = Line[(i + 1) % N];
		const FVector Mid = (A + B) * 0.5f;
		const FVector Dir = (B - A).GetSafeNormal();
		const FVector Side = FVector(-Dir.Y, Dir.X, 0.f); // left of travel
		const float SegLen = FVector::Dist(A, B);
		const FRotator Rot = Dir.Rotation();

		// 5% overlap hides seams on the outside of corners.
		const FVector RoadScale(SegLen / 100.f * 1.05f, RoadWidth / 100.f, 0.2f);
		Road->AddInstance(FTransform(Rot, Mid + FVector(0.f, 0.f, -10.f), RoadScale));

		const FVector BarrierScale(SegLen / 100.f * 1.05f, 0.5f, 1.2f);
		Barriers->AddInstance(FTransform(Rot, Mid + Side * BarrierOffset + FVector(0.f, 0.f, 60.f), BarrierScale));
		if (!IsBarrierGap(Mid.X)) { Barriers->AddInstance(FTransform(Rot, Mid - Side * BarrierOffset + FVector(0.f, 0.f, 60.f), BarrierScale)); }   // pit side (-Y): gaps for the entry/exit
	}

	// Pit road: collision proxy (and fallback visual) along the pit path, 8 m wide.
	const TArray<FVector> Pit = GetPitPath();
	for (int32 i = 0; i + 1 < Pit.Num(); ++i)
	{
		const FVector A = Pit[i], B = Pit[i + 1];
		const FVector Mid = (A + B) * 0.5f;
		const float Len = FVector::Dist(A, B);
		// 13 m wide on the straight, shifted towards the garages so it covers both the fast lane and the working lane.
		const float Shift = (Mid.Y < -1400.f) ? PitWorkLaneOffsetCm * 0.5f : 0.f;
		Road->AddInstance(FTransform((B - A).Rotation(), Mid + FVector(0.f, Shift, -10.f), FVector(Len / 100.f * 1.05f, Mid.Y < -1400.f ? 13.0f : 8.0f, 0.2f)));
	}
}
