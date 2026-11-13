#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FormulaTrack.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Procedural test circuit: a closed loop built from instanced cubes (road + barriers).
 * The layout is a designed set of control points smoothed with a centripetal Catmull-Rom spline and
 * resampled at even spacing, so the centreline is pure maths and unit-testable.
 * It has a long start straight, a hairpin, a chicane and a long sweeper (about 3.5 km).
 * Units: cm. Road surface is at Z = 0.
 */
UCLASS()
class APEXFORMULA_API AFormulaTrack : public AActor
{
	GENERATED_BODY()

public:
	AFormulaTrack();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") float RoadWidth = 1400.f;
	/** Centreline resolution. ~8 m spacing at the default lap length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") int32 NumSegments = 420;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track") int32 NumCheckpoints = 10;

	/** Designed layout, in cm, counter-clockwise. Point 0 is the start/finish line, heading +X. */
	static TArray<FVector> GetControlPoints();

	/** Pure function: closed, evenly spaced centreline through the control points. Index 0 is the line. */
	static TArray<FVector> ComputeCenterline(int32 NumSegments);

	TArray<FVector> GetCenterline() const { return ComputeCenterline(NumSegments); }
	/** World positions of every streetlight head (set while the scenery is built); the street-light director pools real lights over them at night. */
	TArray<FVector> LampHeads;
	/** Every person placed around the lap: where they stand and what they are (0 stand, 1 cheer, 3 marshal, 10+k crew of team k). The NPC crowd pools animated characters over the nearest ones. */
	TArray<FTransform> NPCSlots;
	TArray<uint8> NPCKinds;
	/** Centrelines of the city streets that clear the circuit (x1, y1, x2, y2 in cm): traffic drives along them. */
	TArray<FVector4> StreetSegs;

	/** Evenly spaced checkpoints; index 0 is the start/finish line. */
	TArray<FVector> GetCheckpoints() const;

	/** Grid slot a few segments behind the line, facing along the track. */
	FTransform GetStartTransform() const;

	// ---- Pit lane (cm / m/s). Parallel to the start straight on the -Y side, 16 m from the centreline (the side the default sun lights: garage fronts face it). ---------
	static constexpr float PitLaneYCm = -1600.f;
	static constexpr float PitBoxFirstXCm = -17000.f;      // team 0's box; one box per team, 22 m apart (a 5.9 m car plus room to ease in and out)
	static constexpr float PitBoxSpacingCm = 2200.f;
	static constexpr float PitLimitStartXCm = -22500.f;    // the speed limit applies between these X values
	static constexpr float PitLimitEndXCm = 24500.f;
	static constexpr float PitSpeedLimitMps = 22.f;        // 79 km/h
	static constexpr float PitWorkLaneOffsetCm = -450.f;    // boxes sit this far beyond the fast lane (garage side), so a stopped car never blocks it

	/** Entry-to-exit path, resampled every ~3 m. First/last points lie on the racing centreline. */
	TArray<FVector> GetPitPath() const;
	/** Where a team's car stops (centre of its box, in the lane). */
	static FVector GetPitBoxLocation(int32 TeamIndex) { return FVector(PitBoxFirstXCm + TeamIndex * PitBoxSpacingCm, PitLaneYCm + PitWorkLaneOffsetCm, 0.f); }
	/** The pit-side (-Y) barrier is open where the pit entry/exit cross it. */
	static bool IsBarrierGap(float X) { return (X > -30000.f && X < -24500.f) || (X > 26500.f && X < 32000.f); }

	/** Starting-grid slot (0 = pole), two staggered columns behind the line, facing along the track. */
	FTransform GetGridTransform(int32 Slot) const;

	/** Approximate lap length in metres. */
	float GetLapLengthMeters() const;

	/** 0 = dry, 1 = fully wet. Drives the asphalt material (darker, glossier). Safe to call before the mesh exists. */
	UFUNCTION(BlueprintCallable, Category="Track") void SetWetness(float Wetness);

	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AsphaltMID;
	float WetnessCache = 0.f;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Road;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Barriers;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Ground;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TrackVisual;
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> SceneryComps;

	void BuildGeometry();
	/** Shows the Blender track mesh (if imported) and hides the cube primitives, which stay as collision. */
	bool BuildVisuals();
	/** Trees, buildings and grandstands scattered away from the track, deterministic (seeded). */
	void BuildScenery();
};
