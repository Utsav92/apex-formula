#include "Input/FormulaTouchLayout.h"

FFormulaTouchLayout FFormulaTouchLayout::Compute(const FVector2D& Viewport, const FVector4& SafeMargins, bool bLeftHanded, float UIScale)
{
	FFormulaTouchLayout L;
	const float W = FMath::Max(Viewport.X, 1.f);
	const float H = FMath::Max(Viewport.Y, 1.f);

	// Ignore nonsense margins; never let them eat more than 15% of an axis.
	const float MinMargin = 0.04f * H;
	const float ML = FMath::Clamp(static_cast<float>(SafeMargins.X), MinMargin, 0.15f * W);
	const float MT = FMath::Clamp(static_cast<float>(SafeMargins.Y), MinMargin, 0.15f * H);
	const float MR = FMath::Clamp(static_cast<float>(SafeMargins.Z), MinMargin, 0.15f * W);
	const float MB = FMath::Clamp(static_cast<float>(SafeMargins.W), MinMargin, 0.15f * H);

	const float Base = 0.20f * H * FMath::Clamp(UIScale, 0.6f, 1.6f);
	const float Gap = 0.03f * H;

	// Right-handed geometry; mirrored below for left-handed.
	L.LeftButton  = { ML, H - MB - Base, Base, Base };
	L.RightButton = { ML + Base + Gap, H - MB - Base, Base, Base };

	L.WheelCenter = FVector2D(ML + Base + Gap * 0.5f, H - MB - Base * 1.4f);
	L.WheelRadius = Base * 1.25f;

	L.GasButton   = { W - MR - Base, H - MB - Base * 1.6f, Base, Base * 1.6f };
	L.BrakeButton = { L.GasButton.X - Gap - Base, H - MB - Base, Base, Base };

	L.StyleButton = { W * 0.5f - Base * 0.45f, MT, Base * 0.9f, Base * 0.45f };

	if (bLeftHanded)
	{
		auto Mirror = [W](FFormulaTouchRect& R) { R.X = W - (R.X + R.W); };
		Mirror(L.LeftButton); Mirror(L.RightButton); Mirror(L.GasButton); Mirror(L.BrakeButton);
		L.WheelCenter.X = W - L.WheelCenter.X;
		// Keep "left" meaning steer-left: swap so the button on the screen's left still steers left.
		Swap(L.LeftButton, L.RightButton);
	}
	return L;
}
