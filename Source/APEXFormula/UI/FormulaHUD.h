#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FormulaHUD.generated.h"

class AFormulaVehiclePawn;

/**
 * Canvas HUD: speed, lap info, track map and the on-screen touch controls. Canvas drawing needs no
 * binary UMG assets and is cheap on mobile. The touch rectangles come from FFormulaTouchLayout,
 * the same data the input component hit-tests, so what you see is what you can press.
 */
UCLASS()
class APEXFORMULA_API AFormulaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** "m:ss.cc" */
	static FString FormatLapTime(float Seconds);

private:
	void DrawReadouts(const AFormulaVehiclePawn* Pawn, float W, float H);
	void DrawMiniMap(const AFormulaVehiclePawn* Pawn, float W, float H);
	void DrawTouchControls(const AFormulaVehiclePawn* Pawn);
	void DrawTimingTower(float W, float H);
	void DrawStartLights(float W, float H);
	void DrawDrivingTelemetry(const AFormulaVehiclePawn* Pawn, float W, float H);
	void DrawPauseMenu(float W, float H);
	void DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Colour, float Thickness);
	void DrawButton(const struct FFormulaTouchRect& R, const TCHAR* Label, bool bHeld);
};
