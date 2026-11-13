#include "UI/FormulaHUD.h"
#include "Input/APEXPlayerController.h"
#include "Camera/APEXCameraManager.h"
#include "Racing/APEXTyres.h"
#include "Racing/APEXRaceManager.h"
#include "Racing/FormulaLapTimingComponent.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Racing/FormulaLapTimingComponent.h"
#include "Input/FormulaMobileInputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Racing/APEXRaceManager.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor PanelHeld(1.f, 0.45f, 0.05f, 0.75f);
	const FLinearColor Ink(1.f, 1.f, 1.f, 0.95f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.55f);
}

FString AFormulaHUD::FormatLapTime(float Seconds)
{
	Seconds = FMath::Max(Seconds, 0.f);
	const int32 Minutes = FMath::FloorToInt(Seconds / 60.f);
	const float Rest = Seconds - Minutes * 60.f;
	return FString::Printf(TEXT("%d:%05.2f"), Minutes, Rest);
}

void AFormulaHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }
	if (FParse::Param(FCommandLine::Get(), TEXT("APEXNoHUD"))) { return; } // clean screenshots

	const AFormulaVehiclePawn* Pawn = Cast<AFormulaVehiclePawn>(GetOwningPawn());
	if (!Pawn) { return; }

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	DrawReadouts(Pawn, W, H);
	DrawTimingTower(W, H);
	DrawStartLights(W, H);
	DrawMiniMap(Pawn, W, H);
	DrawDrivingTelemetry(Pawn, W, H);
	DrawPauseMenu(W, H);
	// Touch controls belong to the (later) iOS port. On desktop they are hidden; -APEXTouchUI shows them for testing.
#if PLATFORM_IOS || PLATFORM_ANDROID
	const bool bShowTouch = true;
#else
	const bool bShowTouch = FParse::Param(FCommandLine::Get(), TEXT("APEXTouchUI"));
#endif
	if (bShowTouch) { DrawTouchControls(Pawn); }
}

void AFormulaHUD::DrawCircle(const FVector2D& C, float R, const FLinearColor& Colour, float Thickness)
{
	constexpr int32 Segments = 32;
	FVector2D Prev = C + FVector2D(R, 0.f);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float A = 2.f * PI * i / Segments;
		const FVector2D Next = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
		DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, Colour, Thickness);
		Prev = Next;
	}
}

void AFormulaHUD::DrawButton(const FFormulaTouchRect& R, const TCHAR* Label, bool bHeld)
{
	DrawRect(bHeld ? PanelHeld : Panel, R.X, R.Y, R.W, R.H);
	const float Scale = FMath::Clamp(R.H / 70.f, 1.2f, 2.6f);
	float TW = 0.f, TH = 0.f;
	GetTextSize(Label, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Label, Ink, R.X + (R.W - TW) * 0.5f, R.Y + (R.H - TH) * 0.5f, GEngine->GetMediumFont(), Scale);
}

void AFormulaHUD::DrawReadouts(const AFormulaVehiclePawn* Pawn, float W, float H)
{
	const UFormulaLapTimingComponent* Lap = Pawn->LapTiming;
	const UFormulaMobileInputComponent* Touch = Pawn->MobileInput;
	const FFormulaTouchLayout& L = Touch->GetLayout();
	const float S = H / 1080.f; // text scale follows resolution
	const float Margin = FMath::Max(L.StyleButton.Y, 0.04f * H);
	const float Left = FMath::Min(L.LeftButton.X, L.RightButton.X);

	// Top-left: lap info.
	const int32 LapNow = FMath::Min(Lap->CompletedLaps + 1, Lap->TargetLaps);
	DrawText(FString::Printf(TEXT("LAP %d / %d"), LapNow, Lap->TargetLaps), Ink, Left, Margin, GEngine->GetLargeFont(), 2.2f * S);
	DrawText(FString::Printf(TEXT("TIME  %s"), *FormatLapTime(Lap->CurrentLapTime)), Ink, Left, Margin + 72.f * S, GEngine->GetMediumFont(), 2.4f * S);
	DrawText(FString::Printf(TEXT("BEST  %s"), Lap->BestLapTime > 0.f ? *FormatLapTime(Lap->BestLapTime) : TEXT("--:--.--")),
		Dim, Left, Margin + 120.f * S, GEngine->GetMediumFont(), 2.4f * S);

	// Bottom-centre: speed and a bar against top speed.
	const float Kph = Pawn->GetSpeedKph();
	const FString SpeedText = FString::Printf(TEXT("%.0f"), Kph);
	float TW = 0.f, TH = 0.f;
	GetTextSize(SpeedText, TW, TH, GEngine->GetLargeFont(), 4.5f * S);
	const float CX = W * 0.5f;
	const float BottomY = H - Margin - 190.f * S;
	DrawText(SpeedText, Ink, CX - TW * 0.5f, BottomY, GEngine->GetLargeFont(), 4.5f * S);
	float UW = 0.f, UH = 0.f;
	GetTextSize(TEXT("KM/H"), UW, UH, GEngine->GetMediumFont(), 2.0f * S);
	DrawText(TEXT("KM/H"), Dim, CX - UW * 0.5f, BottomY + TH + 4.f * S, GEngine->GetMediumFont(), 2.0f * S);

	const float BarW = 0.26f * W, BarH = 10.f * S;
	const float Frac = FMath::Clamp(Kph / (Pawn->Setup.TopSpeedMps * 3.6f), 0.f, 1.f);
	const float BarY = H - Margin;
	DrawRect(Panel, CX - BarW * 0.5f, BarY, BarW, BarH);
	DrawRect(FLinearColor(1.f, 0.45f, 0.05f, 0.95f), CX - BarW * 0.5f, BarY, BarW * Frac, BarH);

	// Finish banner.
	if (Lap->IsRaceFinished())
	{
		const FString Banner = FString::Printf(TEXT("FINISHED   BEST %s"), *FormatLapTime(Lap->BestLapTime));
		GetTextSize(Banner, TW, TH, GEngine->GetLargeFont(), 3.0f * S);
		DrawRect(Panel, CX - TW * 0.5f - 24.f * S, H * 0.38f - 12.f * S, TW + 48.f * S, TH + 24.f * S);
		DrawText(Banner, Ink, CX - TW * 0.5f, H * 0.38f, GEngine->GetLargeFont(), 3.0f * S);
	}
}

void AFormulaHUD::DrawTimingTower(float W, float H)
{
	AAPEXRaceManager* Race = nullptr;
	if (TActorIterator<AAPEXRaceManager> It(GetWorld()); It) { Race = *It; }
	if (!Race || Race->GetStandings().Num() == 0) { return; }

	const float S = H / 1080.f;
	const float RowH = 30.f * S, X = 0.04f * H, Y0 = 0.30f * H, Wd = 330.f * S;
	const TArray<FAPEXStanding>& St = Race->GetStandings();
	DrawRect(Panel, X, Y0 - RowH * 1.2f, Wd, RowH * 1.1f);
	DrawText(FString::Printf(TEXT("LAP %d / %d"), St.Num() ? St[0].Lap : 1, Race->TargetLaps), Dim, X + 8.f * S, Y0 - RowH * 1.1f, GEngine->GetSmallFont(), 1.5f * S);

	auto Row = [&](int32 Index, int32 Slot)
	{
		const FAPEXStanding& R = St[Index];
		const float Y = Y0 + Slot * RowH;
		DrawRect(R.bPlayer ? FLinearColor(1.f, 0.45f, 0.05f, 0.78f) : Panel, X, Y, Wd, RowH - 2.f * S);
		DrawRect(R.TeamColor, X + 36.f * S, Y + 3.f * S, 6.f * S, RowH - 8.f * S);
		DrawText(FString::Printf(TEXT("%d"), R.Position), Ink, X + 8.f * S, Y + 3.f * S, GEngine->GetSmallFont(), 1.6f * S);
		DrawText(R.Code, Ink, X + 52.f * S, Y + 3.f * S, GEngine->GetSmallFont(), 1.6f * S);
		const FString Gap = R.bFinished ? FString(TEXT("FIN")) : (R.Position == 1 ? FString(TEXT("LEADER")) : FString::Printf(TEXT("+%.1f"), R.GapSeconds));
		DrawText(Gap, Dim, X + Wd - 100.f * S, Y + 3.f * S, GEngine->GetSmallFont(), 1.6f * S);
	};
	const int32 Shown = FMath::Min(10, St.Num());
	for (int32 i = 0; i < Shown; ++i) { Row(i, i); }
	// If the player is outside the top 10, show their row underneath.
	for (int32 i = Shown; i < St.Num(); ++i) { if (St[i].bPlayer) { Row(i, Shown + 1); break; } }
}

void AFormulaHUD::DrawStartLights(float W, float H)
{
	AAPEXRaceManager* Race = nullptr;
	if (TActorIterator<AAPEXRaceManager> It(GetWorld()); It) { Race = *It; }
	if (!Race || Race->GetState() != EAPEXRaceState::Countdown) { return; }

	const float S = H / 1080.f;
	const float R = 34.f * S, Gap = 22.f * S, Total = 5 * 2 * R + 4 * Gap;
	const float X0 = (W - Total) * 0.5f, Y = 0.16f * H;
	DrawRect(Panel, X0 - 24.f * S, Y - 24.f * S, Total + 48.f * S, 2 * R + 48.f * S);
	for (int32 i = 0; i < 5; ++i)
	{
		const FVector2D C(X0 + R + i * (2 * R + Gap), Y + R);
		const bool bOn = i < Race->GetLightsOn();
		DrawRect(bOn ? FLinearColor(1.f, 0.05f, 0.03f, 1.f) : FLinearColor(0.12f, 0.02f, 0.02f, 1.f), C.X - R, C.Y - R, 2 * R, 2 * R);
	}
}

void AFormulaHUD::DrawMiniMap(const AFormulaVehiclePawn* Pawn, float W, float H)
{
	const TArray<FVector>& Path = Pawn->GetPathPoints();
	if (Path.Num() < 4) { return; }

	FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Max(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	for (const FVector& P : Path)
	{
		Min.X = FMath::Min(Min.X, (float)P.X); Min.Y = FMath::Min(Min.Y, (float)P.Y);
		Max.X = FMath::Max(Max.X, (float)P.X); Max.Y = FMath::Max(Max.Y, (float)P.Y);
	}

	const FFormulaTouchLayout& L = Pawn->MobileInput->GetLayout();
	const float Box = 0.30f * H;
	const float Margin = FMath::Max(L.StyleButton.Y, 0.04f * H);
	const float Right = W - Margin;
	const float Top = Margin;
	const float OriginX = Right - Box, OriginY = Top;
	DrawRect(Panel, OriginX, OriginY, Box, Box);

	const float Pad = 0.08f * Box;
	const float Span = FMath::Max(FMath::Max(Max.X - Min.X, Max.Y - Min.Y), 1.f);
	const float Scale = (Box - 2.f * Pad) / Span;
	auto ToMap = [&](const FVector& P)
	{
		// World X forward/up the page, Y to the right.
		return FVector2D(OriginX + Pad + (P.Y - Min.Y) * Scale, OriginY + Box - Pad - (P.X - Min.X) * Scale);
	};

	for (int32 i = 0; i < Path.Num(); ++i)
	{
		const FVector2D A = ToMap(Path[i]);
		const FVector2D B = ToMap(Path[(i + 1) % Path.Num()]);
		DrawLine(A.X, A.Y, B.X, B.Y, Dim, 2.f);
	}

	// Start/finish tick and the car.
	const FVector2D Line = ToMap(Path[0]);
	DrawRect(Ink, Line.X - 5.f, Line.Y - 1.f, 10.f, 3.f);
	const FVector2D Car = ToMap(Pawn->GetActorLocation());
	DrawRect(FLinearColor(1.f, 0.45f, 0.05f, 1.f), Car.X - 4.f, Car.Y - 4.f, 8.f, 8.f);
}

void AFormulaHUD::DrawTouchControls(const AFormulaVehiclePawn* Pawn)
{
	const UFormulaMobileInputComponent* T = Pawn->MobileInput;
	const FFormulaTouchLayout& L = T->GetLayout();

	DrawButton(L.GasButton, TEXT("GAS"), T->IsGasHeld());
	DrawButton(L.BrakeButton, TEXT("BRAKE"), T->IsBrakeHeld());

	const TCHAR* StyleName = T->Style == EFormulaSteerStyle::Buttons ? TEXT("BUTTONS")
		: T->Style == EFormulaSteerStyle::Tilt ? TEXT("TILT") : TEXT("WHEEL");
	DrawButton(L.StyleButton, StyleName, false);

	switch (T->Style)
	{
	case EFormulaSteerStyle::Buttons:
		DrawButton(L.LeftButton, TEXT("<"), T->IsLeftHeld());
		DrawButton(L.RightButton, TEXT(">"), T->IsRightHeld());
		break;
	case EFormulaSteerStyle::Wheel:
	{
		DrawCircle(L.WheelCenter, L.WheelRadius, T->IsWheelHeld() ? PanelHeld : Dim, 4.f);
		// One spoke pointing up at rest; it rotates with the wheel.
		const float A = FMath::DegreesToRadians(-90.f + T->GetWheelAngleDeg());
		const FVector2D Tip = L.WheelCenter + FVector2D(FMath::Cos(A), FMath::Sin(A)) * L.WheelRadius;
		DrawLine(L.WheelCenter.X, L.WheelCenter.Y, Tip.X, Tip.Y, Ink, 5.f);
		break;
	}
	case EFormulaSteerStyle::Tilt:
	{
		// Steering meter: a centre tick and a block that slides with the steer value.
		const float Wd = L.WheelRadius * 2.f, Y = L.WheelCenter.Y, X = L.WheelCenter.X - L.WheelRadius;
		DrawRect(Panel, X, Y - 8.f, Wd, 16.f);
		DrawRect(Dim, L.WheelCenter.X - 2.f, Y - 14.f, 4.f, 28.f);
		DrawRect(Ink, L.WheelCenter.X + T->GetSteer() * L.WheelRadius - 8.f, Y - 12.f, 16.f, 24.f);
		break;
	}
	}
}


// ---- driving telemetry: real vehicle input data (what the dynamics are actually using), gear, rpm, energy, mode, view, race flow -------------------------
void AFormulaHUD::DrawDrivingTelemetry(const AFormulaVehiclePawn* Pawn, float W, float H)
{
	if (!Pawn) { return; }
	const AAPEXPlayerController* PC = Cast<AAPEXPlayerController>(GetOwningPlayerController());
	const float S = FMath::Clamp(H / 1080.f, 0.6f, 2.f);
	const FLinearColor TPanel(0.f, 0.f, 0.f, 0.55f), TInk(1.f, 1.f, 1.f, 0.95f), TDim(0.7f, 0.72f, 0.78f, 0.9f), Orange(1.f, 0.45f, 0.05f, 0.95f);
	const float X = 24.f * S, BarW = 220.f * S, BarH = 14.f * S;
	float Y = H - 270.f * S;
	DrawRect(TPanel, X - 10.f * S, Y - 10.f * S, BarW + 150.f * S, 240.f * S);

	auto Bar = [&](const TCHAR* Label, float Value01, const FLinearColor& Col)
	{
		DrawText(Label, TDim, X, Y - 2.f * S, GEngine->GetSmallFont(), 1.2f * S);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), X + 70.f * S, Y, BarW - 70.f * S, BarH);
		DrawRect(Col, X + 70.f * S, Y, (BarW - 70.f * S) * FMath::Clamp(Value01, 0.f, 1.f), BarH);
		Y += BarH + 8.f * S;
	};
	Bar(TEXT("THR"), Pawn->GetThrottleInput(), FLinearColor(0.2f, 0.9f, 0.3f, 0.95f));
	Bar(TEXT("BRK"), Pawn->GetBrakeInput(), FLinearColor(0.95f, 0.15f, 0.12f, 0.95f));
	// steering: centred bar, grows left or right
	{
		const float St = Pawn->GetSteerInput();
		const float X0 = X + 70.f * S, Wd = BarW - 70.f * S;
		DrawText(TEXT("STR"), TDim, X, Y - 2.f * S, GEngine->GetSmallFont(), 1.2f * S);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), X0, Y, Wd, BarH);
		DrawRect(Orange, St >= 0.f ? X0 + Wd * 0.5f : X0 + Wd * 0.5f + St * Wd * 0.5f, Y, FMath::Abs(St) * Wd * 0.5f, BarH);
		DrawRect(TInk, X0 + Wd * 0.5f - 1.f, Y - 2.f, 2.f, BarH + 4.f);
		Y += BarH + 8.f * S;
	}
	Bar(TEXT("RPM"), Pawn->GetRev01(), Orange);
	Bar(TEXT("NRG"), Pawn->GetEnergy(), Pawn->IsBoosting() ? FLinearColor(1.f, 0.9f, 0.1f, 0.95f) : FLinearColor(0.1f, 0.6f, 1.f, 0.95f));
	DrawText(FString::Printf(TEXT("GEAR %d%s"), Pawn->GetGear(), Pawn->IsManualGearbox() ? TEXT("  MANUAL") : TEXT("")), TInk, X, Y, GEngine->GetMediumFont(), 1.5f * S);
	Y += 26.f * S;
	DrawText(FString::Printf(TEXT("OVERTAKE %s"), Pawn->IsOvertakeActive() ? TEXT("ACTIVE") : (Pawn->IsOvertakeAvailable() ? TEXT("READY (F)") : TEXT("--"))),
		Pawn->IsOvertakeAvailable() ? Orange : Dim, X, Y, GEngine->GetSmallFont(), 1.3f * S);
	Y += 20.f * S;
	if (PC)
	{
		DrawText(FString::Printf(TEXT("%s | %s | %s"), *AAPEXPlayerController::ModeName(PC->GetDrivingMode()), *AAPEXPlayerController::PresetName(PC->SteerPreset),
			*AAPEXCameraManager::ViewName(Pawn->GetCameraView())), TDim, X, Y, GEngine->GetSmallFont(), 1.2f * S);
		Y += 18.f * S;
		DrawText(FString::Printf(TEXT("RACE: %s%s"), *AAPEXPlayerController::FlowName(PC->GetRaceFlow()), PC->IsUsingGamepad() ? TEXT("   [PAD]") : TEXT("   [KEYS]")), TDim, X, Y, GEngine->GetSmallFont(), 1.2f * S);
	}
	// ---- second panel: tyres, sector splits ----
	{
		const float X2 = X + BarW + 150.f * S, W2 = 250.f * S;
		float Y2 = H - 270.f * S;
		DrawRect(TPanel, X2 - 10.f * S, Y2 - 10.f * S, W2, 240.f * S);
		const EAPEXTyre Comp = Pawn->GetCompound();
		const float Wear = FMath::Clamp(Pawn->GetTyreWear(), 0.f, 1.f);
		const FLinearColor CompCol = Comp == EAPEXTyre::Soft ? FLinearColor(1.f, 0.15f, 0.15f) : (Comp == EAPEXTyre::Medium ? FLinearColor(1.f, 0.85f, 0.1f) : (Comp == EAPEXTyre::Hard ? FLinearColor(0.95f, 0.95f, 0.95f) : (Comp == EAPEXTyre::Intermediate ? FLinearColor(0.2f, 0.85f, 0.3f) : FLinearColor(0.2f, 0.5f, 1.f))));
		DrawText(TEXT("TYRES"), TDim, X2, Y2, GEngine->GetSmallFont(), 1.2f * S);
		DrawRect(CompCol, X2 + 62.f * S, Y2 - 2.f * S, 22.f * S, 22.f * S);
		DrawText(UAPEXTyreLibrary::Letter(Comp), FLinearColor::Black, X2 + 68.f * S, Y2, GEngine->GetSmallFont(), 1.3f * S);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), X2 + 94.f * S, Y2 + 2.f * S, 130.f * S, BarH);
		DrawRect(FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.9f, 0.3f), FLinearColor(0.95f, 0.15f, 0.1f), Wear), X2 + 94.f * S, Y2 + 2.f * S, 130.f * S * (1.f - Wear), BarH);
		Y2 += 34.f * S;
		DrawText(FString::Printf(TEXT("STOPS %d"), Pawn->GetPitStops()), TDim, X2, Y2, GEngine->GetSmallFont(), 1.2f * S);
		Y2 += 26.f * S;
		DrawText(TEXT("SECTORS"), TDim, X2, Y2, GEngine->GetSmallFont(), 1.2f * S);
		Y2 += 22.f * S;
		for (int32 Sct = 0; Sct < 3; ++Sct)
		{
			const bool bLive = Pawn->GetCurrentSector() == Sct;
			const float Last = Pawn->GetLastSectorTime(Sct), Best = Pawn->GetBestSectorTime(Sct);
			const bool bPurple = Last > 0.f && Best > 0.f && FMath::IsNearlyEqual(Last, Best, 0.005f);
			const FLinearColor Col = bLive ? FLinearColor(1.f, 0.45f, 0.05f) : (bPurple ? FLinearColor(0.75f, 0.35f, 1.f) : TInk);
			FString T = bLive ? FString::Printf(TEXT("%.2f"), Pawn->GetLiveSectorTime()) : (Last > 0.f ? FString::Printf(TEXT("%.2f"), Last) : FString(TEXT("--")));
			DrawText(FString::Printf(TEXT("S%d   %s"), Sct + 1, *T), Col, X2, Y2, GEngine->GetSmallFont(), 1.3f * S);
			if (Best > 0.f) { DrawText(FString::Printf(TEXT("best %.2f"), Best), TDim, X2 + 120.f * S, Y2, GEngine->GetSmallFont(), 1.1f * S); }
			Y2 += 22.f * S;
		}
	}
	// ---- race flag banner ----
	if (PC)
	{
		FLinearColor FlagCol; const FString Flag = PC->GetFlag(FlagCol);
		if (!Flag.IsEmpty())
		{
			float TW = 0.f, TH = 0.f; GetTextSize(Flag, TW, TH, GEngine->GetMediumFont(), 1.6f * S);
			DrawRect(FLinearColor(FlagCol.R, FlagCol.G, FlagCol.B, 0.85f), W * 0.5f - TW * 0.5f - 18.f * S, 120.f * S, TW + 36.f * S, TH + 16.f * S);
			DrawText(Flag, FLinearColor::Black, W * 0.5f - TW * 0.5f, 128.f * S, GEngine->GetMediumFont(), 1.6f * S);
		}
	}
	// sector / gap info next to the bars when the lap component has it
	if (const UFormulaLapTimingComponent* Lap = Pawn->LapTiming)
	{
		DrawText(FString::Printf(TEXT("LAP %s"), *FormatLapTime(Lap->CurrentLapTime)), TInk, X + BarW + 10.f * S, H - 262.f * S, GEngine->GetSmallFont(), 1.2f * S);
	}
}

void AFormulaHUD::DrawPauseMenu(float W, float H)
{
	const AAPEXPlayerController* PC = Cast<AAPEXPlayerController>(GetOwningPlayerController());
	if (!PC || !PC->IsMenuOpen()) { return; }
	FString Title; TArray<FString> Lines; int32 Sel = 0;
	PC->GetMenuView(Title, Lines, Sel);
	const float S = FMath::Clamp(H / 1080.f, 0.6f, 2.f);
	const float PW = 760.f * S, PH = (120.f + Lines.Num() * 40.f) * S, PX = (W - PW) * 0.5f, PY = (H - PH) * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, W, H);
	DrawRect(FLinearColor(0.05f, 0.06f, 0.09f, 0.95f), PX, PY, PW, PH);
	DrawText(Title, FLinearColor(1.f, 0.45f, 0.05f, 1.f), PX + 30.f * S, PY + 20.f * S, GEngine->GetLargeFont(), 2.0f * S);
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const float Y = PY + (90.f + i * 40.f) * S;
		if (i == Sel) { DrawRect(FLinearColor(1.f, 0.45f, 0.05f, 0.30f), PX + 16.f * S, Y - 4.f * S, PW - 32.f * S, 34.f * S); }
		DrawText(Lines[i], i == Sel ? FLinearColor::White : FLinearColor(0.75f, 0.77f, 0.82f, 1.f), PX + 34.f * S, Y, GEngine->GetMediumFont(), 1.3f * S);
	}
	DrawText(TEXT("Up / Down: select    Enter: choose    Esc: back / resume"), FLinearColor(0.6f, 0.62f, 0.68f, 1.f), PX + 30.f * S, PY + PH - 28.f * S, GEngine->GetSmallFont(), 1.1f * S);
}
