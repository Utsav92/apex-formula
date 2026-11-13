#include "Input/FormulaMobileInputComponent.h"

UFormulaMobileInputComponent::UFormulaMobileInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // driven by the pawn
	Layout = FFormulaTouchLayout::Compute(Viewport, Margins, bLeftHanded, UIScale);
}

float UFormulaMobileInputComponent::ShapeAxis(float Raw, float Dead, float Sens)
{
	const float A = FMath::Abs(Raw);
	if (A <= Dead) { return 0.f; }
	const float Rescaled = (A - Dead) / FMath::Max(1.f - Dead, KINDA_SMALL_NUMBER);
	return FMath::Sign(Raw) * FMath::Clamp(Rescaled * Sens, 0.f, 1.f);
}

void UFormulaMobileInputComponent::SetViewport(const FVector2D& Size, const FVector4& SafeMargins)
{
	Viewport = Size;
	Margins = SafeMargins;
	Layout = FFormulaTouchLayout::Compute(Viewport, Margins, bLeftHanded, UIScale);
}

void UFormulaMobileInputComponent::CycleStyle()
{
	const uint8 Next = (static_cast<uint8>(Style) + 1) % 3;
	Style = static_cast<EFormulaSteerStyle>(Next);
	// Drop steering fingers so nothing stays "held" under the old style.
	for (auto It = Fingers.CreateIterator(); It; ++It)
	{
		if (It.Value() == EControl::Left || It.Value() == EControl::Right || It.Value() == EControl::Wheel) { It.RemoveCurrent(); }
	}
	WheelAngle = 0.f;
}

UFormulaMobileInputComponent::EControl UFormulaMobileInputComponent::HitTest(const FVector2D& P) const
{
	if (Layout.StyleButton.Contains(P)) { return EControl::StyleButton; }
	if (Layout.GasButton.Contains(P))   { return EControl::Gas; }
	if (Layout.BrakeButton.Contains(P)) { return EControl::Brake; }
	if (Style == EFormulaSteerStyle::Buttons)
	{
		if (Layout.LeftButton.Contains(P))  { return EControl::Left; }
		if (Layout.RightButton.Contains(P)) { return EControl::Right; }
	}
	else if (Style == EFormulaSteerStyle::Wheel)
	{
		if (FVector2D::Distance(P, Layout.WheelCenter) <= Layout.WheelRadius * 1.2f) { return EControl::Wheel; }
	}
	return EControl::None;
}

int32 UFormulaMobileInputComponent::CountHeld(EControl C) const
{
	int32 N = 0;
	for (const TPair<ETouchIndex::Type, EControl>& Pair : Fingers) { if (Pair.Value == C) { ++N; } }
	return N;
}

float UFormulaMobileInputComponent::WheelAngleDegrees(const FVector2D& P) const
{
	// Screen Y points down, so atan2 grows clockwise, which is steering right.
	const FVector2D D = P - Layout.WheelCenter;
	return FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
}

void UFormulaMobileInputComponent::TouchPressed(ETouchIndex::Type Finger, FVector Location)
{
	const FVector2D P(Location.X, Location.Y);
	const EControl C = HitTest(P);
	if (C == EControl::StyleButton)
	{
		CycleStyle();
		return;
	}
	if (C == EControl::None) { return; }

	Fingers.Add(Finger, C);
	if (C == EControl::Wheel)
	{
		WheelFinger = Finger;
		WheelStartAngle = WheelAngleDegrees(P);
		WheelAngle = 0.f;
	}
}

void UFormulaMobileInputComponent::TouchMoved(ETouchIndex::Type Finger, FVector Location)
{
	EControl* Held = Fingers.Find(Finger);
	if (!Held) { return; }
	const FVector2D P(Location.X, Location.Y);

	if (*Held == EControl::Wheel)
	{
		// Wrap the delta into [-180, 180] so crossing the +-180 seam does not jump.
		const float Delta = FMath::UnwindDegrees(WheelAngleDegrees(P) - WheelStartAngle);
		WheelAngle = FMath::Clamp(Delta, -WheelLockDeg * 1.5f, WheelLockDeg * 1.5f);
		return;
	}

	// Sliding a finger between buttons switches the control; sliding off releases it.
	const EControl Now = HitTest(P);
	if (Now == EControl::Left || Now == EControl::Right || Now == EControl::Gas || Now == EControl::Brake)
	{
		*Held = Now;
	}
	else
	{
		Fingers.Remove(Finger);
	}
}

void UFormulaMobileInputComponent::TouchReleased(ETouchIndex::Type Finger, FVector)
{
	if (const EControl* Held = Fingers.Find(Finger))
	{
		if (*Held == EControl::Wheel) { WheelAngle = 0.f; }
	}
	Fingers.Remove(Finger);
}

void UFormulaMobileInputComponent::UpdateInputs(float Dt, float TiltNormalised)
{
	Throttle = IsGasHeld() ? 1.f : 0.f;
	Brake = IsBrakeHeld() ? 1.f : 0.f;

	float Target = 0.f;
	float Rate = 0.f; // max change per second

	switch (Style)
	{
	case EFormulaSteerStyle::Buttons:
		Target = (IsRightHeld() ? 1.f : 0.f) - (IsLeftHeld() ? 1.f : 0.f);
		// Ramp in gently; snap back to centre quickly. Reversing direction is also quick.
		Rate = (Target != 0.f && FMath::Sign(Target) == FMath::Sign(Steer == 0.f ? Target : Steer)) ? 3.f * Sensitivity : 6.f;
		break;
	case EFormulaSteerStyle::Wheel:
		Target = IsWheelHeld() ? ShapeAxis(WheelAngle / WheelLockDeg, Deadzone, Sensitivity) : 0.f;
		Rate = 15.f;
		break;
	case EFormulaSteerStyle::Tilt:
		Target = ShapeAxis(TiltNormalised, Deadzone, Sensitivity);
		Rate = 15.f;
		break;
	}

	Steer = FMath::Clamp(Steer + FMath::Clamp(Target - Steer, -Rate * Dt, Rate * Dt), -1.f, 1.f);
}
