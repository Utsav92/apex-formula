#include "Vehicles/APEXDrivingTypes.h"

namespace
{
	// Top road speed (m/s) of gears 1..8 (55, 95, 135, 180, 225, 270, 305, 340 km/h).
	constexpr float GearTop[8] = { 15.3f, 26.4f, 37.5f, 50.0f, 62.5f, 75.0f, 84.7f, 94.4f };
}

FAPEXSteerTuning FAPEXDriveMath::Tuning(EAPEXSteerPreset Preset)
{
	FAPEXSteerTuning T;
	switch (Preset)
	{
	case EAPEXSteerPreset::Arcade:     T.KeyRise = 4.5f; T.KeyFall = 7.0f; T.Sensitivity = 1.05f; T.MinSpeedScale = 0.85f; T.bAssists = true;  T.bManualGearbox = false; T.bHandbrake = true;  break;
	case EAPEXSteerPreset::Sport:      T.KeyRise = 3.0f; T.KeyFall = 5.0f; T.Sensitivity = 1.00f; T.MinSpeedScale = 0.70f; T.bAssists = false; T.bManualGearbox = false; T.bHandbrake = false; break;
	case EAPEXSteerPreset::Simulation: T.KeyRise = 2.2f; T.KeyFall = 3.5f; T.Sensitivity = 0.95f; T.MinSpeedScale = 0.55f; T.bAssists = false; T.bManualGearbox = true;  T.bHandbrake = false; break;
	}
	return T;
}

float FAPEXDriveMath::KeyboardSteerStep(float Current, float Target, float Dt, float Rise, float Fall)
{
	const bool bGrowing = FMath::Abs(Target) > FMath::Abs(Current) && (Target * Current >= 0.f);
	const float Rate = bGrowing ? Rise : Fall;                       // reversing direction uses the (faster) fall rate to unwind first
	return FMath::Clamp(FMath::FInterpConstantTo(Current, Target, Dt, Rate), -1.f, 1.f);
}

float FAPEXDriveMath::SpeedSteerScale(float SpeedMps, float MinScale)
{
	return FMath::Lerp(1.f, FMath::Clamp(MinScale, 0.05f, 1.f), FMath::SmoothStep(8.f, 90.f, SpeedMps));
}

float FAPEXDriveMath::ApplyDeadZone(float Value, float DeadZone)
{
	const float A = FMath::Abs(Value);
	if (A <= DeadZone) { return 0.f; }
	return FMath::Sign(Value) * FMath::Min((A - DeadZone) / FMath::Max(1.f - DeadZone, 1e-3f), 1.f);   // rescaled so full deflection is still 1.0
}

float FAPEXDriveMath::GearTopSpeedMps(int32 Gear) { return GearTop[FMath::Clamp(Gear, 1, 8) - 1]; }

int32 FAPEXDriveMath::GearForSpeed(float SpeedMps, int32 CurrentGear)
{
	int32 G = 8;
	for (int32 i = 1; i <= 8; ++i) { if (SpeedMps <= GearTopSpeedMps(i) * 0.97f) { G = i; break; } }
	// hysteresis: keep the current gear while within 6 % of its band, so the gearbox does not hunt at a shift point
	const int32 C = FMath::Clamp(CurrentGear, 1, 8);
	const float Lo = C > 1 ? GearTopSpeedMps(C - 1) * 0.90f : 0.f, Hi = GearTopSpeedMps(C) * 1.00f;
	if (SpeedMps >= Lo && SpeedMps <= Hi) { return C; }
	return G;
}

float FAPEXDriveMath::Rev01(float SpeedMps, int32 Gear)
{
	const int32 G = FMath::Clamp(Gear, 1, 8);
	const float Lo = G > 1 ? GearTopSpeedMps(G - 1) : 0.f, Hi = GearTopSpeedMps(G);
	const float T = FMath::Clamp((SpeedMps - Lo) / FMath::Max(Hi - Lo, 1.f), 0.f, 1.f);
	return FMath::Lerp(0.30f, 1.0f, T);
}

void FAPEXDriveMath::SuspensionPose(float LongAccel, float LatAccel, float DownforceN, float& OutPitch, float& OutRoll, float& OutSquat)
{
	OutPitch = FMath::Clamp(LongAccel * 0.07f, -3.0f, 1.8f);          // dive under braking is bigger than squat under power
	OutRoll = FMath::Clamp(-LatAccel * 0.05f, -2.5f, 2.5f);
	OutSquat = FMath::Clamp(DownforceN / 9000.f, 0.f, 3.5f);
}
