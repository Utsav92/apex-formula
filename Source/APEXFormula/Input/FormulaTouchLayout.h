#pragma once

#include "CoreMinimal.h"

/** Axis-aligned rectangle in viewport pixels. */
struct FFormulaTouchRect
{
	float X = 0.f, Y = 0.f, W = 0.f, H = 0.f;

	bool Contains(const FVector2D& P) const { return P.X >= X && P.X <= X + W && P.Y >= Y && P.Y <= Y + H; }
	FVector2D Center() const { return FVector2D(X + W * 0.5f, Y + H * 0.5f); }
	bool Overlaps(const FFormulaTouchRect& O) const { return X < O.X + O.W && O.X < X + W && Y < O.Y + O.H && O.Y < Y + H; }
};

/**
 * Where the on-screen controls live. Pure function of viewport size, safe-area margins and settings,
 * so the HUD (drawing) and the input component (hit testing) always agree, and it can be unit tested.
 * Default is right-handed: steering on the left, pedals on the right.
 */
struct FFormulaTouchLayout
{
	FFormulaTouchRect LeftButton, RightButton, GasButton, BrakeButton, StyleButton;
	FVector2D WheelCenter = FVector2D::ZeroVector;
	float WheelRadius = 0.f;

	/** SafeMargins = (Left, Top, Right, Bottom) in pixels. */
	static FFormulaTouchLayout Compute(const FVector2D& Viewport, const FVector4& SafeMargins, bool bLeftHanded, float UIScale);
};
