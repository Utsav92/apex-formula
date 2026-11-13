#include "Racing/APEXRaceManager.h"
#include "Racing/FormulaTrack.h"
#include "Racing/FormulaLapTimingComponent.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Lighting/APEXLightingDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexRace, Log, All);

AAPEXRaceManager::AAPEXRaceManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

int32 AAPEXRaceManager::ParsePlayerSlotFromCommandLine(int32 DefaultSlot)
{
	int32 Slot = DefaultSlot;
	FParse::Value(FCommandLine::Get(), TEXT("APEXGridSlot="), Slot);
	return FMath::Clamp(Slot, 0, 19);
}

int32 AAPEXRaceManager::GetPlayerPosition() const
{
	for (const FAPEXStanding& S : Standings) { if (S.bPlayer) { return S.Position; } }
	return 0;
}

void AAPEXRaceManager::FindPlayer()
{
	if (Player) { return; }
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AFormulaVehiclePawn> It(World); It; ++It)
		{
			if (!It->IsAI()) { Player = *It; break; }
		}
	}
	if (Player && !Cars.Contains(Player))
	{
		Cars.Insert(Player, 0);
		Player->SetHoldOnGrid(true);
		Player->LapTiming->TargetLaps = TargetLaps;
		Player->ApplyDriverLook(UAPEXGridLibrary::GetTeams()[0], UAPEXGridLibrary::GetDrivers()[0]);
		Player->SetGridLateral(PlayerGridSlot % 2 == 0 ? 220.f : -220.f);
	}
}

void AAPEXRaceManager::SpawnGrid()
{
	UWorld* World = GetWorld();
	AFormulaTrack* Track = nullptr;
	if (TActorIterator<AFormulaTrack> It(World); It) { Track = *It; }
	if (!Track) { UE_LOG(LogApexRace, Warning, TEXT("No track: no grid")); return; }

	const TArray<FAPEXTeam> Teams = UAPEXGridLibrary::GetTeams();
	const TArray<FAPEXDriver> Drivers = UAPEXGridLibrary::GetDrivers();
	int32 NextDriver = 1;   // driver 0 is the player
	for (int32 Slot = 0; Slot < 20; ++Slot)
	{
		if (Slot == PlayerGridSlot || NextDriver >= Drivers.Num()) { continue; }
		const int32 DIdx = NextDriver++;
		const FTransform T = Track->GetGridTransform(Slot);

		// Deferred spawn so the car never auto-possesses the player's controller.
		AFormulaVehiclePawn* Car = World->SpawnActorDeferred<AFormulaVehiclePawn>(AFormulaVehiclePawn::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Car) { continue; }
		Car->AutoPossessPlayer = EAutoReceiveInput::Disabled;
		Car->AutoPossessAI = EAutoPossessAI::Disabled;
		Car->ConfigureAsAI(DIdx, Drivers[DIdx], Teams[Drivers[DIdx].TeamIndex]);
		Car->FinishSpawning(T);
		Car->LapTiming->TargetLaps = TargetLaps;
		Car->SetGridLateral(Slot % 2 == 0 ? 220.f : -220.f);
		Car->ApplyDriverLook(Teams[Drivers[DIdx].TeamIndex], Drivers[DIdx]);
		Cars.Add(Car);
	}
	UE_LOG(LogApexRace, Log, TEXT("Grid: spawned %d AI cars (player on grid slot %d)"), Cars.Num(), PlayerGridSlot);
}

bool AAPEXRaceManager::TryReserveBox(int32 TeamIndex, AFormulaVehiclePawn* Car)
{
	TWeakObjectPtr<AFormulaVehiclePawn>& Owner = BoxOwners.FindOrAdd(TeamIndex);
	if (!Owner.IsValid() || Owner.Get() == Car) { Owner = Car; return true; }
	return false;
}

void AAPEXRaceManager::ReleaseBox(int32 TeamIndex, AFormulaVehiclePawn* Car)
{
	if (TWeakObjectPtr<AFormulaVehiclePawn>* O = BoxOwners.Find(TeamIndex); O && O->Get() == Car) { BoxOwners.Remove(TeamIndex); }
}

void AAPEXRaceManager::BeginPlay()
{
	Super::BeginPlay();
	PlayerGridSlot = ParsePlayerSlotFromCommandLine(PlayerGridSlot);
	int32 Laps = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("APEXLaps="), Laps)) { TargetLaps = FMath::Max(Laps, 1); }
	bFullRateAI = FParse::Param(FCommandLine::Get(), TEXT("APEXAIFullRate"));
	SpawnGrid();
	FindPlayer();

	// Race plans: every car gets a strategy for this race length and a handle back to the manager (box reservation).
	const TArray<FAPEXDriver> Drivers = UAPEXGridLibrary::GetDrivers();
	for (AFormulaVehiclePawn* C : Cars)
	{
		if (!C) { continue; }
		const int32 D = FMath::Clamp(C->GetDriverIndex(), 0, Drivers.Num() - 1);
		C->SetRaceManager(this);
		C->InitTyresAndStrategy(TargetLaps, 0.f, Drivers[D], Drivers[D].TeamIndex);
	}
	if (Player) { Player->SetRaceManager(this); Player->InitTyresAndStrategy(TargetLaps, 0.f, Drivers[0], Drivers[0].TeamIndex); }
}

void AAPEXRaceManager::ReleaseCars()
{
	for (AFormulaVehiclePawn* C : Cars) { if (C) { C->SetHoldOnGrid(false); } }
}

void AAPEXRaceManager::UpdateAwareness()
{
	OverlapsThisSample = 0;
	const int32 Num = Cars.Num();
	struct FCar { AFormulaVehiclePawn* C; float Progress, Lateral, Speed, Dist; };
	TArray<FCar> Info;
	Info.Reserve(Num);
	float L = 3500.f;
	for (AFormulaVehiclePawn* C : Cars)
	{
		if (!C) { continue; }
		L = C->GetLapLengthM();
		Info.Add({ C, FMath::Fmod(C->GetRaceDistanceM() + L * 100.f, L), C->GetLateralCm(), C->GetSpeedMps(), C->GetRaceDistanceM() });
	}

	for (const FCar& A : Info)
	{
		float GapAhead = 9999.f, AheadSpeed = 0.f, AheadLat = 0.f, GapBehind = 9999.f, BehindLat = 0.f, HumanGap = 9999.f, HumanLat = 0.f;
		for (const FCar& B : Info)
		{
			if (A.C == B.C) { continue; }
			float D = B.Progress - A.Progress;                   // signed gap along the track, wrapped to (-L/2, L/2)
			if (D > L * 0.5f) { D -= L; } else if (D < -L * 0.5f) { D += L; }
			// Following and overtaking only care about the car ahead IN MY LANE (within 2.5 m across); cars in the next lane
			// are handled by the alongside logic. Without this, a car a metre ahead in the next lane hides the real leader.
			const bool bSameLane = FMath::Abs(B.Lateral - A.Lateral) < 250.f;
			if (bSameLane && D > -3.f && D < GapAhead) { GapAhead = D; AheadSpeed = B.Speed; AheadLat = B.Lateral; }
			if (bSameLane && D < 0.f && -D < GapBehind) { GapBehind = -D; BehindLat = B.Lateral; }
			// A human-driven car closing from behind in ANY lane within 6 m across (the same-lane filter above would hide an attacker in the next lane).
			if (A.C->IsAI() && B.C->IsHumanDriven() && D < 0.f && -D < HumanGap && FMath::Abs(B.Lateral - A.Lateral) < 600.f) { HumanGap = -D; HumanLat = B.Lateral; }
		}
		A.C->SetNeighbours(GapAhead, AheadSpeed, AheadLat, GapBehind, BehindLat);
		A.C->SetHumanBehind(HumanGap, HumanLat);

		// Alongside: closest conflict within 6 m along and 3 m across.
		bool bAlong = false; float AlongLat = 0.f, BestDiff = 300.f;
		for (const FCar& B : Info)
		{
			if (A.C == B.C) { continue; }
			float D = B.Progress - A.Progress;
			if (D > L * 0.5f) { D -= L; } else if (D < -L * 0.5f) { D += L; }
			const float LatDiff = FMath::Abs(B.Lateral - A.Lateral);
			if (FMath::Abs(D) < 6.f && LatDiff < BestDiff) { bAlong = true; AlongLat = B.Lateral; BestDiff = LatDiff; }
			// Overlap statistic (rectangles: 5.6 m long, 1.9 m wide), counted once per pair, only while racing.
			if (State == EAPEXRaceState::Racing && A.C < B.C && FMath::Abs(D) < 5.6f && LatDiff < 190.f)
			{
				++OverlapsThisSample;
				if (OverlapLogged < 14 && (OverlapTotal + OverlapsThisSample) % 37 == 1)
				{
					++OverlapLogged;
					UE_LOG(LogApexRace, Log, TEXT("OVERLAP t=%.1f drivers %d/%d D=%.1f m latDiff=%.0f cm speeds %.0f/%.0f m/s progress=%.0f/%.0f m lat=%.0f/%.0f"),
						RaceTime, A.C->IsAI() ? A.C->GetDriverIndex() : 0, B.C->IsAI() ? B.C->GetDriverIndex() : 0, D, LatDiff, A.Speed, B.Speed, A.Dist, B.Dist, A.Lateral, B.Lateral);
				}
			}
		}
		A.C->SetAlongside(bAlong, AlongLat);
	}

	if (State == EAPEXRaceState::Racing) { ++OverlapSamples; OverlapTotal += OverlapsThisSample; OverlapPeak = FMath::Max(OverlapPeak, OverlapsThisSample); }

	// Rain slows everyone a little (grip), applied as a pace scale. Wetness comes from the lighting director.
	float Wet = 0.f;
	if (UWorld* World = GetWorld()) { if (TActorIterator<AAPEXLightingDirector> It(World); It) { Wet = It->GetWetness(); } }
	const float WetScale = 1.f - 0.12f * Wet;

	// Level of detail for opponents: visuals only near the player, distant cars tick less often.
	const FVector PlayerPos = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	for (AFormulaVehiclePawn* C : Cars)
	{
		if (!C || C == Player) { continue; }
		const float DistM = FVector::Dist(C->GetActorLocation(), PlayerPos) / 100.f;
		C->SetVisualsActive(DistM < 250.f || !bThrottleDistantAI);   // cinematic runs keep every car's wheels, suspension and lights alive
		C->SetWetnessPaceScale(WetScale);
		float Interval = 0.f;
		if (bThrottleDistantAI && !bFullRateAI) { Interval = DistM > 500.f ? 1.f / 15.f : (DistM > 200.f ? 1.f / 30.f : 0.f); }
		C->SetActorTickInterval(Interval);
	}
	if (Player) { Player->SetWetnessPaceScale(WetScale); }
}

void AAPEXRaceManager::UpdateStandings()
{
	const TArray<FAPEXTeam> Teams = UAPEXGridLibrary::GetTeams();
	const TArray<FAPEXDriver> Drivers = UAPEXGridLibrary::GetDrivers();

	struct FRow { AFormulaVehiclePawn* C; float Dist; bool bFin; float FinTime; };
	TArray<FRow> Rows;
	for (AFormulaVehiclePawn* C : Cars)
	{
		if (!C) { continue; }
		const int32 Idx = C->IsAI() ? C->GetDriverIndex() : 0;
		const bool bFin = C->LapTiming->IsRaceFinished();
		if (bFin && !FinishTimes.Contains(Idx))
		{
			FinishTimes.Add(Idx, RaceTime);
			Classification.Add(Idx);
		}
		Rows.Add({ C, C->GetRaceDistanceM(), bFin, FinishTimes.Contains(Idx) ? FinishTimes[Idx] : 0.f });
	}
	// Finished cars first (by finish time), then by distance covered.
	Rows.Sort([](const FRow& A, const FRow& B)
	{
		if (A.bFin != B.bFin) { return A.bFin; }
		return A.bFin ? A.FinTime < B.FinTime : A.Dist > B.Dist;
	});

	Standings.Reset();
	const float LeaderDist = Rows.Num() ? Rows[0].Dist : 0.f;
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const AFormulaVehiclePawn* C = Rows[i].C;
		const int32 DIdx = C->IsAI() ? C->GetDriverIndex() : 0;
		FAPEXStanding S;
		S.Position = i + 1;
		S.DriverIndex = DIdx;
		S.Code = Drivers[DIdx].Code;
		S.Number = Drivers[DIdx].Number;
		S.TeamColor = Teams[Drivers[DIdx].TeamIndex].Primary;
		S.bPlayer = !C->IsAI();
		S.bFinished = Rows[i].bFin;
		S.Lap = FMath::Clamp(FMath::FloorToInt(Rows[i].Dist / FMath::Max(C->GetLapLengthM(), 1.f)) + 1, 1, TargetLaps);
		S.GapSeconds = (i == 0) ? 0.f : FMath::Max(LeaderDist - Rows[i].Dist, 0.f) / FMath::Max(C->GetSpeedMps(), 30.f);
		Standings.Add(S);
	}
}

void AAPEXRaceManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FindPlayer();
	StateTime += DeltaSeconds;
	if (State == EAPEXRaceState::Racing)   // 60 Hz contact guard (AI vs AI): the rear car matches the speed of a car it would otherwise drive through
	{
		for (AFormulaVehiclePawn* A : Cars)
		{
			if (!A || !A->IsAI()) { continue; }
			const FVector AP = A->GetActorLocation(), AF = A->GetActorForwardVector().GetSafeNormal2D(), AR = A->GetActorRightVector().GetSafeNormal2D();
			for (AFormulaVehiclePawn* B : Cars)
			{
				if (!B || A == B) { continue; }
				const FVector D = B->GetActorLocation() - AP;
				const float Along = FVector::DotProduct(D, AF) / 100.f, Lat = FMath::Abs(FVector::DotProduct(D, AR)) / 100.f;
				if (Along > 0.f && Along < 9.f && Lat < 2.1f) { A->LimitSpeedMps(B->GetSpeedMps() + FMath::Max(Along - 6.2f, 0.f) * 1.2f); }
			}
		}
	}

	if (State == EAPEXRaceState::Countdown)
	{
		LightsOn = FMath::Clamp(FMath::FloorToInt(StateTime / (CountdownSeconds / 6.f)), 0, 5);   // one light per step, then lights out
		if (StateTime >= CountdownSeconds)
		{
			State = EAPEXRaceState::Racing;
			LightsOn = 0;
			RaceTime = 0.f;
			ReleaseCars();
			UE_LOG(LogApexRace, Log, TEXT("LIGHTS OUT: %d cars released"), Cars.Num());
		}
	}
	else if (State == EAPEXRaceState::Racing)
	{
		RaceTime += DeltaSeconds;
	}

	SlowTick += DeltaSeconds;
	if (SlowTick >= 0.1f)             // 10 Hz: awareness, LOD and standings
	{
		SlowTick = 0.f;
		UpdateAwareness();
		UpdateStandings();
		if (!bLoggedFinal && Player && Player->LapTiming->IsRaceFinished())
		{
			bLoggedFinal = true;
			FString Line;
			for (const FAPEXStanding& S : Standings) { Line += FString::Printf(TEXT("%d.%s%s "), S.Position, *S.Code, S.bFinished ? TEXT("(F)") : TEXT("")); }
			UE_LOG(LogApexRace, Log, TEXT("OVERLAPS: %d car-pair overlaps over %d samples (peak %d at once)"), OverlapTotal, OverlapSamples, OverlapPeak);
			UE_LOG(LogApexRace, Log, TEXT("STANDINGS at player finish (race time %.1f s): %s"), RaceTime, *Line);
		}
		if (State == EAPEXRaceState::Racing && Classification.Num() >= Cars.Num() && Cars.Num() > 0)
		{
			State = EAPEXRaceState::Finished;
			UE_LOG(LogApexRace, Log, TEXT("RACE COMPLETE: all %d cars classified"), Classification.Num());
		}
	}
}

AFormulaVehiclePawn* AAPEXRaceManager::CarAtPosition(int32 Position) const
{
	if (!Standings.IsValidIndex(Position - 1)) { return Cars.Num() ? Cars[0].Get() : nullptr; }
	const FAPEXStanding& S = Standings[Position - 1];
	if (S.bPlayer) { return Player.Get(); }
	for (AFormulaVehiclePawn* C : Cars) { if (C && C->IsAI() && C->GetDriverIndex() == S.DriverIndex) { return C; } }
	return nullptr;
}
