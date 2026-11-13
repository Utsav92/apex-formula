#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APEXCinematicDirector.generated.h"

class ACameraActor;
class AFormulaVehiclePawn;

/** One edited shot of the showcase film (loaded from Cinematics/shotlist.json, the same file the Sequencer generator reads). */
struct FAPEXShot
{
	FString Name; float Start = 0.f, Duration = 1.f; int32 Scene = 1; FString Preset, Camera; int32 Rank = 1; float Fov = 45.f; TArray<float> P; bool bOverlay = false;
};

/** Pure camera maths, kept free of actors so it can be unit tested. */
struct FAPEXCineMath
{
	/** Index of the shot that is on screen at this time (-1 outside the film). Shots are contiguous. */
	static int32 ShotAt(const TArray<FAPEXShot>& Shots, float T);
	/** True when the shots tile [0, Total] with no gap or overlap (tolerance in seconds). */
	static bool Contiguous(const TArray<FAPEXShot>& Shots, float Total, float Tol = 1e-3f);
	/** Critically damped follow factor that is independent of frame rate. */
	static float Damp(float Dt, float Halflife) { return 1.f - FMath::Pow(0.5f, Dt / FMath::Max(Halflife, 1e-3f)); }
	static float EaseInOut(float A) { A = FMath::Clamp(A, 0.f, 1.f); return A * A * (3.f - 2.f * A); }
};

/**
 * Directs the 120 s showcase film on top of the live race: cuts between cameras per the shot list, switches lighting/weather per scene, starts the
 * race at the 30 s mark, asks a car to pit for the pit-stop scene, and (with -APEXCinematicCapture) writes one PNG per frame at a fixed 60 Hz step.
 * Command line: -APEXCinematic[=<shotlist path>], -APEXCinematicCapture=<dir>, -APEXCinematicRes=WxH, -APEXCinematicFrom=<s>, -APEXCinematicTo=<s>.
 */
UCLASS()
class APEXFORMULA_API AAPEXCinematicDirector : public AActor
{
	GENERATED_BODY()
public:
	AAPEXCinematicDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	static bool LoadShots(const FString& Path, TArray<FAPEXShot>& Out, FString& Error);
	int32 GetShotIndex() const { return CurShot; }

private:
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	TArray<FAPEXShot> Shots;
	float Clock = 0.f, FromT = 0.f, ToT = 120.f;
	int32 CurShot = -1, Frame = 0, CapturedFrames = 0;
	FString CaptureDir, HighRes; bool bPitAsked = false, bTitleShown = false;
	FVector Pos = FVector::ZeroVector, ShotAnchor = FVector::ZeroVector; FRotator Rot = FRotator::ZeroRotator;
	TWeakObjectPtr<AFormulaVehiclePawn> Target;
	float ShotClock = 0.f;

	AFormulaVehiclePawn* CarAt(int32 Rank) const;
	void BeginShot(int32 Idx);
	void UpdateCamera(const FAPEXShot& S, float Dt);
};
