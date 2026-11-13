#include "Racing/APEXTyres.h"

float UAPEXTyreLibrary::DryGrip(EAPEXTyre T)
{
	switch (T)
	{
	case EAPEXTyre::Soft: return 1.06f;
	case EAPEXTyre::Medium: return 1.00f;
	case EAPEXTyre::Hard: return 0.95f;
	case EAPEXTyre::Intermediate: return 0.86f;
	default: return 0.78f;
	}
}

float UAPEXTyreLibrary::BaseWearPerSecond(EAPEXTyre T)
{
	// Calibrated against the measured average load multiplier (~1.4 over a lap): a soft lasts ~170 s of driving (about
	// 3 laps), a medium ~280 s, a hard ~430 s. (The first 8-lap run wore softs out in ~95 s before this was rescaled.)
	switch (T)
	{
	case EAPEXTyre::Soft: return 1.f / 240.f;
	case EAPEXTyre::Medium: return 1.f / 390.f;
	case EAPEXTyre::Hard: return 1.f / 600.f;
	case EAPEXTyre::Intermediate: return 1.f / 360.f;
	default: return 1.f / 420.f;
	}
}

float UAPEXTyreLibrary::WetOptimum(EAPEXTyre T)
{
	switch (T)
	{
	case EAPEXTyre::Intermediate: return 0.5f;
	case EAPEXTyre::Wet: return 1.0f;
	default: return 0.f;
	}
}

float UAPEXTyreLibrary::GripFactor(EAPEXTyre T, float Wear, float Wetness)
{
	Wear = FMath::Clamp(Wear, 0.f, 1.f);
	Wetness = FMath::Clamp(Wetness, 0.f, 1.f);
	const float Mismatch = FMath::Abs(Wetness - WetOptimum(T));
	const float Weather = 1.f - 0.45f * FMath::Pow(Mismatch, 1.3f);                 // a slick on a soaked track keeps ~55 %
	float Worn = 1.f - 0.20f * FMath::Pow(Wear, 1.5f);                              // gradual loss
	if (Wear > 0.85f) { Worn -= 0.15f * (Wear - 0.85f) / 0.15f; }                  // the cliff
	return DryGrip(T) * Weather * Worn;
}

float UAPEXTyreLibrary::WearRate(EAPEXTyre T, float LateralAccelMps2, float Brake, float Throttle, float Wetness)
{
	const float Load = 0.55f + 0.9f * FMath::Clamp(FMath::Abs(LateralAccelMps2) / 30.f, 0.f, 1.5f) + 0.35f * FMath::Clamp(Brake, 0.f, 1.f) + 0.2f * FMath::Clamp(Throttle, 0.f, 1.f);
	float Mismatch = 1.f;
	if (T == EAPEXTyre::Intermediate && Wetness < 0.25f) { Mismatch = 3.f; }       // wets overheat on a dry track
	else if (T == EAPEXTyre::Wet && Wetness < 0.6f) { Mismatch = 3.f; }
	else if (T <= EAPEXTyre::Hard) { Mismatch = 1.f + 0.5f * Wetness; }             // slicks slide and tear in the wet
	return BaseWearPerSecond(T) * Load * Mismatch;
}

FString UAPEXTyreLibrary::Letter(EAPEXTyre T)
{
	switch (T) { case EAPEXTyre::Soft: return TEXT("S"); case EAPEXTyre::Medium: return TEXT("M"); case EAPEXTyre::Hard: return TEXT("H"); case EAPEXTyre::Intermediate: return TEXT("I"); default: return TEXT("W"); }
}

FLinearColor UAPEXTyreLibrary::Colour(EAPEXTyre T)
{
	switch (T)
	{
	case EAPEXTyre::Soft: return FLinearColor(0.9f, 0.05f, 0.05f);
	case EAPEXTyre::Medium: return FLinearColor(0.95f, 0.8f, 0.05f);
	case EAPEXTyre::Hard: return FLinearColor(0.9f, 0.9f, 0.9f);
	case EAPEXTyre::Intermediate: return FLinearColor(0.1f, 0.7f, 0.2f);
	default: return FLinearColor(0.1f, 0.3f, 0.9f);
	}
}

FAPEXStrategy UAPEXTyreLibrary::PlanStrategy(int32 TotalLaps, float ForecastWetness, const FAPEXDriver& D)
{
	FAPEXStrategy S;
	if (ForecastWetness >= 0.7f) { S.StartCompound = EAPEXTyre::Wet; return S; }              // weather decides; stops are reactive
	if (ForecastWetness >= 0.35f) { S.StartCompound = EAPEXTyre::Intermediate; return S; }

	const bool bAggressive = D.Aggression > 0.7f;
	if (TotalLaps <= 3)                                                  // short race: no stop
	{
		S.StartCompound = bAggressive ? EAPEXTyre::Soft : ((D.TeamIndex % 2) ? EAPEXTyre::Hard : EAPEXTyre::Medium);
		return S;
	}
	if (TotalLaps <= 10)                                                 // one stop, around half distance (capped by the start tyre's life)
	{
		S.StartCompound = bAggressive ? EAPEXTyre::Soft : EAPEXTyre::Medium;
		const int32 MaxStartLaps = (S.StartCompound == EAPEXTyre::Soft) ? 4 : 6;   // ~ life of the compound at a 60 s lap
		S.StopLaps.Add(FMath::Clamp(FMath::RoundToInt(TotalLaps * 0.45f) + (D.TeamIndex % 3) - 1, 1, FMath::Min(TotalLaps - 1, MaxStartLaps)));
		S.NextCompounds.Add(EAPEXTyre::Hard);
		return S;
	}
	S.StartCompound = bAggressive ? EAPEXTyre::Soft : EAPEXTyre::Medium;                 // long race: two stops
	S.StopLaps.Add(FMath::Max(1, TotalLaps / 3 + (D.TeamIndex % 2)));
	S.StopLaps.Add(FMath::Clamp((2 * TotalLaps) / 3 + (D.TeamIndex % 2), S.StopLaps[0] + 1, TotalLaps - 1));
	S.NextCompounds.Add(EAPEXTyre::Hard);
	S.NextCompounds.Add(bAggressive ? EAPEXTyre::Soft : EAPEXTyre::Medium);
	return S;
}

EAPEXTyre UAPEXTyreLibrary::ChooseNextCompound(int32 LapsRemaining, float Wetness, EAPEXTyre Current, bool bAlreadyUsedTwoCompounds)
{
	if (Wetness >= 0.7f) { return EAPEXTyre::Wet; }
	if (Wetness >= 0.35f) { return EAPEXTyre::Intermediate; }
	EAPEXTyre Pick = LapsRemaining <= 2 ? EAPEXTyre::Soft : (LapsRemaining <= 4 ? EAPEXTyre::Medium : EAPEXTyre::Hard);
	if (!bAlreadyUsedTwoCompounds && Pick == Current)                    // dry-race rule: use at least two different slick compounds
	{
		Pick = (Current == EAPEXTyre::Hard) ? EAPEXTyre::Medium : EAPEXTyre::Hard;
	}
	return Pick;
}

bool UAPEXTyreLibrary::ShouldPit(EAPEXTyre Current, float Wear, float Wetness, int32 LapsDone, int32 TotalLaps, int32 PlannedStopLap, int32 StopsDone)
{
	const int32 LapsLeft = TotalLaps - LapsDone;
	const bool bWrongForWeather = (Current <= EAPEXTyre::Hard && Wetness > 0.5f) || (Current == EAPEXTyre::Intermediate && (Wetness > 0.85f || Wetness < 0.15f)) || (Current == EAPEXTyre::Wet && Wetness < 0.3f);
	if (bWrongForWeather && LapsLeft >= 1) { return true; }
	if (LapsLeft <= 1) { return false; }                                 // never stop on the last lap: a stop costs ~22 s, dead tyres far less
	if (Wear > 0.92f) { return true; }                                   // tyres are gone
	if (PlannedStopLap > 0 && LapsDone >= PlannedStopLap && StopsDone == 0) { return LapsLeft >= 1; }
	if (PlannedStopLap > 0 && LapsDone >= PlannedStopLap) { return true; }
	return Wear > 0.78f && LapsLeft >= 2;                                // reactive: worn with distance left to run
}

float UAPEXTyreLibrary::PitStopSeconds(const FAPEXTeam& Team, int32 DriverIndex)
{
	return 2.3f + (1.f - Team.Pace) * 8.f + 0.04f * (DriverIndex % 5);
}
