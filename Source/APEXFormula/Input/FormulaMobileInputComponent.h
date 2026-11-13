#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "Input/FormulaTouchLayout.h"
#include "FormulaMobileInputComponent.generated.h"

UENUM(BlueprintType)
enum class EFormulaSteerStyle : uint8
{
	Buttons,
	Tilt,
	Wheel
};

/**
 * Turns touches (and tilt) into throttle / brake / steer. It owns no engine input binding itself:
 * the pawn forwards BindTouch events here, so the logic is testable without a player controller.
 */
UCLASS(ClassGroup=(Input), meta=(BlueprintSpawnableComponent))
class APEXFORMULA_API UFormulaMobileInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFormulaMobileInputComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input") EFormulaSteerStyle Style = EFormulaSteerStyle::Buttons;
	/** Multiplies steering response. 1 = default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input", meta=(ClampMin="0.3", ClampMax="3.0")) float Sensitivity = 1.f;
	/** Fraction of full travel ignored around centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input", meta=(ClampMin="0.0", ClampMax="0.5")) float Deadzone = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input") bool bLeftHanded = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input", meta=(ClampMin="0.6", ClampMax="1.6")) float UIScale = 1.f;
	/** Phone roll (degrees) that equals full steering lock in Tilt style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input") float TiltMaxAngleDeg = 25.f;
	/** Wheel rotation (degrees) that equals full lock in Wheel style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobile Input") float WheelLockDeg = 90.f;

	UFUNCTION(BlueprintPure, Category="Mobile Input") float GetThrottle() const { return Throttle; }
	UFUNCTION(BlueprintPure, Category="Mobile Input") float GetBrake() const { return Brake; }
	UFUNCTION(BlueprintPure, Category="Mobile Input") float GetSteer() const { return Steer; }

	bool IsLeftHeld() const { return CountHeld(EControl::Left) > 0; }
	bool IsRightHeld() const { return CountHeld(EControl::Right) > 0; }
	bool IsGasHeld() const { return CountHeld(EControl::Gas) > 0; }
	bool IsBrakeHeld() const { return CountHeld(EControl::Brake) > 0; }
	bool IsWheelHeld() const { return CountHeld(EControl::Wheel) > 0; }
	bool HasActiveTouch() const { return Fingers.Num() > 0; }

	const FFormulaTouchLayout& GetLayout() const { return Layout; }

	/** Call every frame (or from tests). Margins = (L, T, R, B) pixels. */
	void SetViewport(const FVector2D& Size, const FVector4& SafeMargins);

	void TouchPressed(ETouchIndex::Type Finger, FVector Location);
	void TouchMoved(ETouchIndex::Type Finger, FVector Location);
	void TouchReleased(ETouchIndex::Type Finger, FVector Location);

	/** TiltNormalised in [-1, 1] (roll / TiltMaxAngle); ignored unless Style == Tilt. */
	void UpdateInputs(float DeltaSeconds, float TiltNormalised);

	/** Dead zone then sensitivity on a [-1, 1] axis. Public and static so it can be tested. */
	static float ShapeAxis(float Raw, float Deadzone, float Sensitivity);

	void CycleStyle();

private:
	enum class EControl : uint8 { None, Left, Right, Gas, Brake, Wheel, StyleButton };

	EControl HitTest(const FVector2D& P) const;
	int32 CountHeld(EControl C) const;
	float WheelAngleDegrees(const FVector2D& P) const;

	FFormulaTouchLayout Layout;
	FVector2D Viewport = FVector2D(1920.f, 1080.f);
	FVector4 Margins = FVector4(0, 0, 0, 0);

	TMap<ETouchIndex::Type, EControl> Fingers;
	ETouchIndex::Type WheelFinger = ETouchIndex::Touch1;
	float WheelStartAngle = 0.f;
	float WheelAngle = 0.f; // degrees, positive = clockwise on screen

	float Throttle = 0.f;
	float Brake = 0.f;
	float Steer = 0.f;

public:
	/** Wheel angle in degrees for the HUD to draw (0 when idle). */
	float GetWheelAngleDeg() const { return WheelAngle; }
};
