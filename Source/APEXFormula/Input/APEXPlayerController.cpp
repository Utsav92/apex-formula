#include "Input/APEXPlayerController.h"
#include "Input/APEXInputConfig.h"
#include "Vehicles/FormulaVehiclePawn.h"
#include "Camera/APEXCameraManager.h"
#include "Racing/APEXRaceManager.h"
#include "Racing/FormulaLapTimingComponent.h"
#include "Broadcast/APEXBroadcastDirector.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "AudioDevice.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexInput, Log, All);

namespace
{
	enum EMenuPage { PageMain = 0, PageControls, PageCamera, PageAssists, PageGraphics, PageAudio, PageStart };
	const TCHAR* MainItems[] = { TEXT("Resume Race"), TEXT("Restart Race"), TEXT("Controls"), TEXT("Camera Settings"), TEXT("Driving Assists"), TEXT("Graphics Settings"), TEXT("Audio Settings"), TEXT("Main Menu"), TEXT("Exit Game") };
	const TCHAR* StartItems[] = { TEXT("Start Race"), TEXT("Controls"), TEXT("Camera Settings"), TEXT("Driving Assists"), TEXT("Graphics Settings"), TEXT("Audio Settings"), TEXT("Quit") };
}

AAPEXPlayerController::AAPEXPlayerController()
{
	bShouldPerformFullTickWhenPaused = true;      // the pause menu must stay responsive while the world is paused
	bShowMouseCursor = false;
}

FString AAPEXPlayerController::FlowName(EAPEXRaceFlow F)
{
	switch (F) { case EAPEXRaceFlow::Menu: return TEXT("Menu"); case EAPEXRaceFlow::Loading: return TEXT("Loading"); case EAPEXRaceFlow::Grid: return TEXT("Grid");
	case EAPEXRaceFlow::Countdown: return TEXT("Countdown"); case EAPEXRaceFlow::Racing: return TEXT("Racing"); case EAPEXRaceFlow::Paused: return TEXT("Paused");
	case EAPEXRaceFlow::Finished: return TEXT("Finished"); case EAPEXRaceFlow::Replay: return TEXT("Replay"); }
	return TEXT("?");
}
FString AAPEXPlayerController::ModeName(EAPEXDrivingMode M) { return M == EAPEXDrivingMode::Manual ? TEXT("Manual") : (M == EAPEXDrivingMode::Assisted ? TEXT("Assisted") : TEXT("AutonomousDemo")); }
FString AAPEXPlayerController::PresetName(EAPEXSteerPreset P) { return P == EAPEXSteerPreset::Arcade ? TEXT("Arcade") : (P == EAPEXSteerPreset::Sport ? TEXT("Sport") : TEXT("Simulation")); }

AFormulaVehiclePawn* AAPEXPlayerController::GetCar() const { return Cast<AFormulaVehiclePawn>(GetPawn()); }

AAPEXCameraManager* AAPEXPlayerController::GetCameraManager() const
{
	if (TActorIterator<AAPEXCameraManager> It(GetWorld()); It) { return *It; }
	return nullptr;
}

EAPEXDrivingMode AAPEXPlayerController::GetDrivingMode() const { const AFormulaVehiclePawn* C = GetCar(); return C ? C->DrivingMode : EAPEXDrivingMode::Manual; }

void AAPEXPlayerController::SetDrivingMode(EAPEXDrivingMode Mode)
{
	if (AFormulaVehiclePawn* C = GetCar()) { C->SetDrivingMode(Mode); }
}

void AAPEXPlayerController::SetPreset(EAPEXSteerPreset Preset)
{
	SteerPreset = Preset;
	if (AFormulaVehiclePawn* C = GetCar()) { C->SetSteerPreset(Preset); }
}

EAPEXRaceFlow AAPEXPlayerController::GetRaceFlow() const
{
	if (!GetPawn()) { return EAPEXRaceFlow::Loading; }
	if (bStartMenu) { return EAPEXRaceFlow::Menu; }
	if (UGameplayStatics::IsGamePaused(this)) { return EAPEXRaceFlow::Paused; }
	for (TActorIterator<AAPEXBroadcastDirector> It(GetWorld()); It; ++It) { if (It->IsReplaying()) { return EAPEXRaceFlow::Replay; } }
	for (TActorIterator<AAPEXRaceManager> It(GetWorld()); It; ++It)
	{
		switch (It->GetState())
		{
		case EAPEXRaceState::Countdown: return It->GetLightsOn() > 0 ? EAPEXRaceFlow::Countdown : EAPEXRaceFlow::Grid;
		case EAPEXRaceState::Racing:    return EAPEXRaceFlow::Racing;
		case EAPEXRaceState::Finished:  return EAPEXRaceFlow::Finished;
		}
	}
	return EAPEXRaceFlow::Racing;       // no race manager (single-car start): free driving
}

void AAPEXPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InputConfig = NewObject<UAPEXInputConfig>(this, TEXT("APEXInputConfig"));
	InputConfig->Build();
	TArray<FString> Problems;
	if (!InputConfig->Validate(Problems)) { for (const FString& P : Problems) { UE_LOG(LogApexInput, Error, TEXT("Input mapping problem: %s"), *P); } }
	InstallMapping();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	bBot = FParse::Param(FCommandLine::Get(), TEXT("APEXInputBot"));
	bSelfTest = FParse::Param(FCommandLine::Get(), TEXT("APEXInputSelfTest"));
	// Interactive launches get the start menu and the intro fly-by; every automation / capture flag skips them.
	const TCHAR* Auto[] = { TEXT("APEXAutoDrive"), TEXT("APEXInputBot"), TEXT("APEXInputSelfTest"), TEXT("APEXNoMenu"), TEXT("APEXCam="), TEXT("APEXHero"), TEXT("APEXShot"), TEXT("APEXShotAt="), TEXT("APEXPauseAt=") };
	bool bAutomation = false;
	for (const TCHAR* F : Auto) { if (FCommandLine::Get() && FCString::Stristr(FCommandLine::Get(), F)) { bAutomation = true; } }
	bShowStartMenu = !bAutomation || FParse::Param(FCommandLine::Get(), TEXT("APEXStartMenu"));
	bIntroForced = FParse::Param(FCommandLine::Get(), TEXT("APEXIntro"));
	bIntroEnabled = FParse::Param(FCommandLine::Get(), TEXT("APEXIntro")) || (!bAutomation && !FParse::Param(FCommandLine::Get(), TEXT("APEXNoIntro")));
	FParse::Value(FCommandLine::Get(), TEXT("APEXShotAt="), DbgShotAt);
	FParse::Value(FCommandLine::Get(), TEXT("APEXPauseAt="), DbgPauseAt);
	ControllerConnHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this, [this](EInputDeviceConnectionState State, FPlatformUserId, FInputDeviceId Id)
	{
		const bool bConnected = State == EInputDeviceConnectionState::Connected;
		UE_LOG(LogApexInput, Log, TEXT("Input device %d %s"), Id.GetId(), bConnected ? TEXT("connected") : TEXT("disconnected"));
		if (!bConnected) { HandleDeviceLost(); }
	});
}

void AAPEXPlayerController::HandleDeviceLost()
{
	if (!bGamepadActive) { return; }
	bGamepadActive = false; ThrottleIn = BrakeIn = SteerTarget = 0.f; ThrottleCmd = BrakeCmd = SteerRamp = 0.f;      // never leave a stuck input behind
	UE_LOG(LogApexInput, Log, TEXT("Gamepad lost: inputs cleared%s (flow %s, menu %d)"), (!bMenuOpen && GetRaceFlow() == EAPEXRaceFlow::Racing) ? TEXT(", race paused") : TEXT(""), *FlowName(GetRaceFlow()), bMenuOpen);
	if (!bMenuOpen && GetRaceFlow() == EAPEXRaceFlow::Racing) { TogglePause(); }
}

void AAPEXPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ControllerConnHandle);
	Super::EndPlay(Reason);
}

void AAPEXPlayerController::InstallMapping()
{
	ULocalPlayer* LP = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Sub = LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
	if (!Sub || !InputConfig || !InputConfig->IMC_APEX_Driving)
	{
		UE_LOG(LogApexInput, Error, TEXT("Input mapping FAILED to install (localPlayer=%d subsystem=%d context=%d)"), LP != nullptr, Sub != nullptr, InputConfig && InputConfig->IMC_APEX_Driving);
		return;
	}
	Sub->ClearAllMappings();
	Sub->AddMappingContext(InputConfig->IMC_APEX_Driving, 0);
	UE_LOG(LogApexInput, Log, TEXT("Input mapping context installed: IMC_APEX_Driving (%d mappings)"), InputConfig->IMC_APEX_Driving->GetMappings().Num());
}

void AAPEXPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC) { UE_LOG(LogApexInput, Error, TEXT("InputComponent is not an EnhancedInputComponent: check DefaultInputComponentClass")); return; }
	// The config is created in BeginPlay, but SetupInputComponent runs earlier: build a throw-away copy of the actions here would diverge, so build now and keep it.
	if (!InputConfig) { InputConfig = NewObject<UAPEXInputConfig>(this, TEXT("APEXInputConfig")); InputConfig->Build(); }
	UAPEXInputConfig* C = InputConfig;
	EIC->BindAction(C->IA_Throttle, ETriggerEvent::Triggered, this, &AAPEXPlayerController::OnThrottle);
	EIC->BindAction(C->IA_Throttle, ETriggerEvent::Completed, this, &AAPEXPlayerController::OnThrottleEnd);
	EIC->BindAction(C->IA_Throttle, ETriggerEvent::Canceled, this, &AAPEXPlayerController::OnThrottleEnd);
	EIC->BindAction(C->IA_Brake, ETriggerEvent::Triggered, this, &AAPEXPlayerController::OnBrake);
	EIC->BindAction(C->IA_Brake, ETriggerEvent::Completed, this, &AAPEXPlayerController::OnBrakeEnd);
	EIC->BindAction(C->IA_Brake, ETriggerEvent::Canceled, this, &AAPEXPlayerController::OnBrakeEnd);
	EIC->BindAction(C->IA_Steer, ETriggerEvent::Triggered, this, &AAPEXPlayerController::OnSteer);
	EIC->BindAction(C->IA_Steer, ETriggerEvent::Completed, this, &AAPEXPlayerController::OnSteerEnd);
	EIC->BindAction(C->IA_Steer, ETriggerEvent::Canceled, this, &AAPEXPlayerController::OnSteerEnd);
	EIC->BindAction(C->IA_ShiftUp, ETriggerEvent::Started, this, &AAPEXPlayerController::OnShiftUp);
	EIC->BindAction(C->IA_ShiftDown, ETriggerEvent::Started, this, &AAPEXPlayerController::OnShiftDown);
	EIC->BindActionValueLambda(C->IA_Handbrake, ETriggerEvent::Started, [this](const FInputActionValue&) { OnHandbrake(true); });
	EIC->BindActionValueLambda(C->IA_Handbrake, ETriggerEvent::Completed, [this](const FInputActionValue&) { OnHandbrake(false); });
	EIC->BindActionValueLambda(C->IA_EnergyBoost, ETriggerEvent::Started, [this](const FInputActionValue&) { OnBoost(true); });
	EIC->BindActionValueLambda(C->IA_EnergyBoost, ETriggerEvent::Completed, [this](const FInputActionValue&) { OnBoost(false); });
	EIC->BindActionValueLambda(C->IA_OvertakeAid, ETriggerEvent::Started, [this](const FInputActionValue&) { OnOvertake(true); });
	EIC->BindActionValueLambda(C->IA_OvertakeAid, ETriggerEvent::Completed, [this](const FInputActionValue&) { OnOvertake(false); });
	EIC->BindAction(C->IA_CameraSwitch, ETriggerEvent::Started, this, &AAPEXPlayerController::OnCameraSwitch);
	EIC->BindActionValueLambda(C->IA_LookBack, ETriggerEvent::Started, [this](const FInputActionValue&) { OnLookBack(true); });
	EIC->BindActionValueLambda(C->IA_LookBack, ETriggerEvent::Completed, [this](const FInputActionValue&) { OnLookBack(false); });
	EIC->BindAction(C->IA_ResetCar, ETriggerEvent::Started, this, &AAPEXPlayerController::OnResetCar);
	EIC->BindAction(C->IA_Pause, ETriggerEvent::Started, this, &AAPEXPlayerController::OnPause);
	EIC->BindAction(C->IA_Confirm, ETriggerEvent::Started, this, &AAPEXPlayerController::OnConfirm);
	EIC->BindAction(C->IA_Cancel, ETriggerEvent::Started, this, &AAPEXPlayerController::OnCancel);
}

void AAPEXPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AFormulaVehiclePawn* Car = Cast<AFormulaVehiclePawn>(InPawn);
	if (!Car) { UE_LOG(LogApexInput, Error, TEXT("Possessed a pawn that is not a formula car: %s"), InPawn ? *InPawn->GetName() : TEXT("null")); return; }
	if (Car->IsAI()) { UE_LOG(LogApexInput, Error, TEXT("The player controller possessed an AI car (driver %d)!"), Car->GetDriverIndex()); }
	Car->SetSteerPreset(SteerPreset);
	UE_LOG(LogApexInput, Log, TEXT("POSSESSION: %s possesses %s, driving mode %s, preset %s"), *GetName(), *Car->GetName(), *ModeName(Car->DrivingMode), *PresetName(SteerPreset));
	bLoggedPossession = true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// input handlers (Enhanced Input)
void AAPEXPlayerController::OnThrottle(const FInputActionValue& V) { ThrottleIn = FAPEXDriveMath::ApplyDeadZone(V.Get<float>(), TriggerMin); }
void AAPEXPlayerController::OnBrake(const FInputActionValue& V) { BrakeIn = FAPEXDriveMath::ApplyDeadZone(V.Get<float>(), TriggerMin); }
void AAPEXPlayerController::OnSteer(const FInputActionValue& V) { SteerTarget = FMath::Clamp(V.Get<float>(), -1.f, 1.f); }
void AAPEXPlayerController::OnShiftUp()   { if (AFormulaVehiclePawn* C = GetCar()) { C->ShiftUp(); } }
void AAPEXPlayerController::OnShiftDown() { if (AFormulaVehiclePawn* C = GetCar()) { C->ShiftDown(); } }
void AAPEXPlayerController::OnHandbrake(bool bOn) { if (AFormulaVehiclePawn* C = GetCar()) { C->SetHandbrake(bOn); } }
void AAPEXPlayerController::OnBoost(bool bOn) { if (AFormulaVehiclePawn* C = GetCar()) { C->SetBoostHeld(bOn); } }
void AAPEXPlayerController::OnOvertake(bool bOn) { if (AFormulaVehiclePawn* C = GetCar()) { C->SetOvertakeHeld(bOn); } }
void AAPEXPlayerController::OnCameraSwitch() { if (!bMenuOpen) { if (AAPEXCameraManager* M = GetCameraManager()) { M->Cycle(); } } }
void AAPEXPlayerController::OnLookBack(bool bOn)
{
	if (bMenuOpen) { return; }
	if (AAPEXCameraManager* M = GetCameraManager()) { bOn ? M->BeginLookBack() : M->EndLookBack(); }
}
void AAPEXPlayerController::OnResetCar()
{
	if (bMenuOpen) { return; }
	if (AFormulaVehiclePawn* C = GetCar()) { if (!C->IsHeldOnGrid()) { C->ResetToTrack(); } }
}

void AAPEXPlayerController::Rumble(float Intensity, float Seconds)
{
	if (bVibration && bGamepadActive) { ++RumbleCount; LastRumble = Intensity; PlayDynamicForceFeedback(Intensity, Seconds, true, true, true, true); }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// pause, start menu and the menu pages
void AAPEXPlayerController::TogglePause()
{
	if (bStartMenu) { return; }                    // the start menu is left only through "Start Race"
	const bool bNowPaused = !UGameplayStatics::IsGamePaused(this);
	UGameplayStatics::SetGamePaused(this, bNowPaused);
	bMenuOpen = bNowPaused; MenuPage = PageMain; MenuIndex = 0; bCapturing = false; MenuMessage.Reset();
	if (bNowPaused) { ThrottleIn = BrakeIn = 0.f; }
	UE_LOG(LogApexInput, Log, TEXT("%s"), bNowPaused ? TEXT("PAUSED") : TEXT("RESUMED: gameplay input restored"));
}

void AAPEXPlayerController::StartRace()
{
	bStartMenu = false; bMenuOpen = false; bCapturing = false;
	UGameplayStatics::SetGamePaused(this, false);
	UE_LOG(LogApexInput, Log, TEXT("START RACE from the main menu"));
}

int32 AAPEXPlayerController::ParentRow(int32 Page) const
{
	const int32 Base = bStartMenu ? 0 : 1;           // Controls is row 1 of the start menu and row 2 of the pause menu
	switch (Page) { case PageControls: return 1 + Base; case PageCamera: return 2 + Base; case PageAssists: return 3 + Base; case PageGraphics: return 4 + Base; case PageAudio: return 5 + Base; }
	return 0;
}

void AAPEXPlayerController::OnPause()
{
	if (bCapturing) { bCapturing = false; MenuMessage = TEXT("cancelled"); return; }
	if (bMenuOpen && MenuPage != PageMain && MenuPage != PageStart) { MenuIndex = ParentRow(MenuPage); MenuPage = bStartMenu ? PageStart : PageMain; return; }   // Escape backs out of a sub page first
	TogglePause();
}

int32 AAPEXPlayerController::MenuCount() const
{
	switch (MenuPage)
	{
	case PageMain: return UE_ARRAY_COUNT(MainItems);
	case PageStart: return UE_ARRAY_COUNT(StartItems);
	case PageControls: return (InputConfig ? InputConfig->GetSlots().Num() : 0) + 2;
	case PageCamera: return 2; case PageAssists: return 4; case PageGraphics: return 3; case PageAudio: return 2;
	}
	return 1;
}
void AAPEXPlayerController::MenuMove(int32 Delta) { const int32 N = MenuCount(); MenuIndex = (MenuIndex + Delta + N) % N; }

void AAPEXPlayerController::OnCancel()
{
	if (!bMenuOpen) { return; }
	if (bCapturing) { bCapturing = false; return; }
	if (MenuPage != PageMain && MenuPage != PageStart) { MenuIndex = ParentRow(MenuPage); MenuPage = bStartMenu ? PageStart : PageMain; }
	else if (!bStartMenu) { TogglePause(); }
}

void AAPEXPlayerController::RestartRace()
{
	UGameplayStatics::SetGamePaused(this, false); bMenuOpen = false;
	UE_LOG(LogApexInput, Log, TEXT("RESTART requested"));
	ConsoleCommand(TEXT("RestartLevel"));
}

void AAPEXPlayerController::ResetBindingsAndSave()
{
	if (!InputConfig) { return; }
	InputConfig->ResetBindings(); InputConfig->SaveBindings(); InstallMapping();
}

void AAPEXPlayerController::BeginRebind(FName SlotName)
{
	bCapturing = true; CaptureSlot = SlotName; CaptureDelay = 0.3f; MenuMessage.Reset();
}

bool AAPEXPlayerController::ApplyRebind(FName SlotName, const FKey& Key, FString& OutError)
{
	if (!InputConfig || !InputConfig->Rebind(SlotName, Key, OutError)) { return false; }
	InputConfig->SaveBindings();
	InstallMapping();                       // rebuild the active mapping so the new key is live immediately
	UE_LOG(LogApexInput, Log, TEXT("REBOUND %s -> %s (saved)"), *SlotName.ToString(), *Key.ToString());
	return true;
}

void AAPEXPlayerController::OnConfirm()
{
	if (!bMenuOpen || bCapturing) { return; }
	AFormulaVehiclePawn* Car = GetCar();
	auto Back = [this]() { MenuIndex = ParentRow(MenuPage); MenuPage = bStartMenu ? PageStart : PageMain; };
	switch (MenuPage)
	{
	case PageStart:
		switch (MenuIndex)
		{
		case 0: StartRace(); break;
		case 1: MenuPage = PageControls; MenuIndex = 0; break;
		case 2: MenuPage = PageCamera; MenuIndex = 0; break;
		case 3: MenuPage = PageAssists; MenuIndex = 0; break;
		case 4: MenuPage = PageGraphics; MenuIndex = 0; break;
		case 5: MenuPage = PageAudio; MenuIndex = 0; break;
		case 6: ConsoleCommand(TEXT("Quit")); break;
		}
		break;
	case PageMain:
		switch (MenuIndex)
		{
		case 0: TogglePause(); break;
		case 1: RestartRace(); break;
		case 2: MenuPage = PageControls; MenuIndex = 0; break;
		case 3: MenuPage = PageCamera; MenuIndex = 0; break;
		case 4: MenuPage = PageAssists; MenuIndex = 0; break;
		case 5: MenuPage = PageGraphics; MenuIndex = 0; break;
		case 6: MenuPage = PageAudio; MenuIndex = 0; break;
		case 7: RestartRace(); break;                       // the level restarts into the start menu
		case 8: ConsoleCommand(TEXT("Quit")); break;
		}
		break;
	case PageControls:
	{
		const int32 N = InputConfig ? InputConfig->GetSlots().Num() : 0;
		if (MenuIndex < N) { BeginRebind(InputConfig->GetSlots()[MenuIndex].Name); }
		else if (MenuIndex == N) { InputConfig->ResetBindings(); InputConfig->SaveBindings(); InstallMapping(); MenuMessage = TEXT("controls reset to defaults"); }
		else { Back(); }
		break;
	}
	case PageCamera:
		if (MenuIndex == 0) { if (AAPEXCameraManager* M = GetCameraManager()) { M->Cycle(); } } else { Back(); }
		break;
	case PageAssists:
		if (MenuIndex == 0 && Car) { Car->SetDrivingMode(Car->DrivingMode == EAPEXDrivingMode::Manual ? EAPEXDrivingMode::Assisted : (Car->DrivingMode == EAPEXDrivingMode::Assisted ? EAPEXDrivingMode::AutonomousDemo : EAPEXDrivingMode::Manual)); }
		else if (MenuIndex == 1) { SetPreset(static_cast<EAPEXSteerPreset>((static_cast<int32>(SteerPreset) + 1) % 3)); }
		else if (MenuIndex == 2 && Car) { Car->SetManualGearbox(!Car->IsManualGearbox()); }
		else { Back(); }
		break;
	case PageGraphics:
		if (MenuIndex == 0)
		{
			IConsoleVariable* SP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
			if (SP) { const float Cur = SP->GetFloat(); SP->Set(Cur > 95.f ? 85.f : (Cur > 75.f ? 70.f : 100.f), ECVF_SetByConsole); }
		}
		else if (MenuIndex == 1)
		{
			IConsoleVariable* MB = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
			if (MB) { MB->Set(MB->GetInt() == 0 ? 4 : 0, ECVF_SetByConsole); }
		}
		else { Back(); }
		break;
	case PageAudio:
		if (MenuIndex == 0)
		{
			static int32 Step = 0; Step = (Step + 1) % 4;
			const float Vol[4] = { 1.f, 0.7f, 0.4f, 0.f };
			if (FAudioDeviceHandle AD = GEngine ? GEngine->GetMainAudioDevice() : FAudioDeviceHandle()) { AD->SetTransientPrimaryVolume(Vol[Step]); }
		}
		else { Back(); }
		break;
	}
}

void AAPEXPlayerController::GetMenuView(FString& OutTitle, TArray<FString>& OutLines, int32& OutSelected) const
{
	OutLines.Reset(); OutSelected = MenuIndex;
	const AFormulaVehiclePawn* Car = GetCar();
	switch (MenuPage)
	{
	case PageStart:
		OutTitle = TEXT("APEX FORMULA   NEW YORK GRAND PRIX");
		for (const TCHAR* L : StartItems) { OutLines.Add(L); }
		break;
	case PageMain:
		OutTitle = TEXT("PAUSED");
		for (const TCHAR* L : MainItems) { OutLines.Add(L); }
		break;
	case PageControls:
		OutTitle = TEXT("CONTROLS   (Enter to rebind a key)");
		if (InputConfig)
		{
			for (const UAPEXInputConfig::FBindSlot& S : InputConfig->GetSlots())
			{
				const bool bThis = bCapturing && CaptureSlot == S.Name;
				FString Alt = S.Alternate.IsValid() ? FString::Printf(TEXT("   (also %s)"), *S.Alternate.GetDisplayName().ToString()) : FString();
				const FString Pad = S.DefaultPad.IsValid() ? FString::Printf(TEXT("   pad [ %s ]"), *S.PadKey.GetDisplayName().ToString()) : FString(TEXT("   pad: analog"));
				OutLines.Add(bThis ? FString::Printf(TEXT("%-14s press a key or pad button...  (Esc cancels)"), *S.Label)
				                   : FString::Printf(TEXT("%-14s [ %s ]%s%s"), *S.Label, *S.Primary.GetDisplayName().ToString(), *Alt, *Pad));
			}
		}
		OutLines.Add(TEXT("Reset to defaults"));
		OutLines.Add(TEXT("Back"));
		OutLines.Add(TEXT("Fixed: Escape / Menu pause, Enter / A confirm, Tab cancel, D-pad menus. Triggers and the left stick are analog."));
		if (!MenuMessage.IsEmpty()) { OutLines.Add(MenuMessage); }
		break;
	case PageCamera:
		OutTitle = TEXT("CAMERA");
		OutLines.Add(FString::Printf(TEXT("View: %s   (Enter to cycle)"), Car ? *AAPEXCameraManager::ViewName(Car->GetCameraView()) : TEXT("-")));
		OutLines.Add(TEXT("Back"));
		break;
	case PageAssists:
		OutTitle = TEXT("DRIVING ASSISTS");
		OutLines.Add(FString::Printf(TEXT("Driving mode: %s"), *ModeName(GetDrivingMode())));
		OutLines.Add(FString::Printf(TEXT("Steering preset: %s"), *PresetName(SteerPreset)));
		OutLines.Add(FString::Printf(TEXT("Gearbox: %s"), Car && Car->IsManualGearbox() ? TEXT("Manual (E / Q)") : TEXT("Automatic")));
		OutLines.Add(TEXT("Back"));
		break;
	case PageGraphics:
		OutTitle = TEXT("GRAPHICS");
		{
			const IConsoleVariable* SP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
			const IConsoleVariable* MB = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
			OutLines.Add(FString::Printf(TEXT("Resolution scale: %.0f %%"), SP ? SP->GetFloat() : 100.f));
			OutLines.Add(FString::Printf(TEXT("Motion blur: %s"), MB && MB->GetInt() == 0 ? TEXT("Off") : TEXT("On")));
		}
		OutLines.Add(TEXT("Back"));
		break;
	case PageAudio:
		OutTitle = TEXT("AUDIO");
		OutLines.Add(TEXT("Master volume: Enter to cycle 100 / 70 / 40 / 0 %"));
		OutLines.Add(TEXT("Back"));
		break;
	}
}

FString AAPEXPlayerController::GetFlag(FLinearColor& OutColour) const
{
	const AFormulaVehiclePawn* Car = GetCar();
	if (!Car) { return FString(); }
	if (Car->LapTiming && Car->LapTiming->IsRaceFinished()) { OutColour = FLinearColor(0.95f, 0.95f, 0.95f); return TEXT("CHEQUERED FLAG"); }
	float RaceTime = 0.f;
	if (TActorIterator<AAPEXRaceManager> It(GetWorld()); It) { RaceTime = It->GetRaceTime(); }
	if (GetRaceFlow() != EAPEXRaceFlow::Racing) { return FString(); }
	if (RaceTime > 12.f && Car->GetSpeedKph() < 15.f) { OutColour = FLinearColor(1.f, 0.85f, 0.05f); return TEXT("YELLOW FLAG: car stopped (Backspace resets)"); }
	if (RaceTime < 10.f) { OutColour = FLinearColor(0.1f, 0.85f, 0.2f); return TEXT("GREEN FLAG"); }
	return FString();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
FString AAPEXPlayerController::TelemetryLine() const
{
	const AFormulaVehiclePawn* C = GetCar();
	if (!C) { return TEXT("no pawn"); }
	return FString::Printf(TEXT("flow=%s pawn=%s owner=%s mode=%s preset=%s IMC=%d thr=%.2f brk=%.2f steer=%.2f speed=%.0f km/h gear=%d view=%s"),
		*FlowName(GetRaceFlow()), *C->GetName(), *GetName(), *ModeName(C->DrivingMode), *PresetName(SteerPreset),
		InputConfig && InputConfig->IMC_APEX_Driving ? 1 : 0, C->GetThrottleInput(), C->GetBrakeInput(), C->GetSteerInput(), C->GetSpeedKph(), C->GetGear(),
		*AAPEXCameraManager::ViewName(C->GetCameraView()));
}

void AAPEXPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	DbgClock += FMath::Max(Dt, 1.f / 120.f);
	if (DbgPauseAt >= 0.f && DbgClock >= DbgPauseAt) { DbgPauseAt = -1.f; if (!bMenuOpen) { TogglePause(); } }
	if (DbgShotAt >= 0.f && DbgClock >= DbgShotAt) { DbgShotAt = -1.f; DbgQuitAt = DbgClock + 4.f; ConsoleCommand(TEXT("HighResShot 1920x1080")); }
	if (DbgQuitAt >= 0.f && DbgClock >= DbgQuitAt) { FPlatformMisc::RequestExit(false); }
	AFormulaVehiclePawn* Car = GetCar();
	const EAPEXRaceFlow Flow = GetRaceFlow();

	if (bShowStartMenu && Car)
	{
		bShowStartMenu = false; bStartMenu = true; bMenuOpen = true; MenuPage = PageStart; MenuIndex = 0;
		UGameplayStatics::SetGamePaused(this, true);
		UE_LOG(LogApexInput, Log, TEXT("Start menu open (flow Menu)"));
	}
	if (bSelfTest && bMenuOpen) { TickSelfTest(Dt); }
	// ---- key capture for rebinding ----
	if (bMenuOpen && bCapturing)
	{
		CaptureDelay -= FMath::Max(Dt, 1.f / 120.f);
		if (CaptureDelay <= 0.f)
		{
			if (WasInputKeyJustPressed(EKeys::Escape)) { bCapturing = false; MenuMessage = TEXT("cancelled"); }
			else
			{
				TArray<FKey> All; EKeys::GetAllKeys(All);
				for (const FKey& K : All)
				{
					if (K == EKeys::AnyKey || !K.IsValid() || K.IsMouseButton() || K.IsAxis1D() || K.IsAxis2D() || K.IsAxis3D() || K == EKeys::Enter || K == EKeys::Tab) { continue; }   // AnyKey is a pseudo key that reads pressed for any key
					if (!WasInputKeyJustPressed(K)) { continue; }
					FString Err;
					if (ApplyRebind(CaptureSlot, K, Err)) { MenuMessage = FString::Printf(TEXT("%s is now %s"), *CaptureSlot.ToString(), *K.GetDisplayName().ToString()); } else { MenuMessage = Err; }
					bCapturing = false;
					break;
				}
			}
		}
		return;
	}
	// ---- menu navigation (the world is paused, so poll keys directly) ----
	if (bMenuOpen)
	{
		if (WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up)) { MenuMove(-1); }
		if (WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down)) { MenuMove(+1); }
		return;
	}

	// ---- diagnostics: ownership, errors ----
	if (!Car)
	{
		if (!bWarnedNoPawn) { bWarnedNoPawn = true; UE_LOG(LogApexInput, Error, TEXT("PlayerController %s possesses NO pawn: possession failed"), *GetName()); }
		return;
	}
	if (Car->IsAI() && !bWarnedAIControl) { bWarnedAIControl = true; UE_LOG(LogApexInput, Error, TEXT("AI car under player control")); }
	if (Car->DrivingMode != EAPEXDrivingMode::AutonomousDemo && Car->IsAutoDriving() && !bWarnedAIControl) { bWarnedAIControl = true; UE_LOG(LogApexInput, Error, TEXT("Player car is auto-driving while the driving mode is %s: AI/auto code took control"), *ModeName(Car->DrivingMode)); }
	if (bCinematicMode) { UE_LOG(LogApexInput, Error, TEXT("Cinematic mode is active during play")); SetCinematicMode(false, false, false); }

	// ---- gamepad detection (for analog vs keyboard steering) ----
	const float PadSteer = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const bool bPadNow = FMath::Abs(PadSteer) > 0.05f || GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.05f || GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > 0.05f;
	if (bPadNow) { bGamepadActive = true; } else if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::D)) { bGamepadActive = false; }

	if (bIntroEnabled && !bIntroStarted && !bMenuOpen && (Flow == EAPEXRaceFlow::Grid || Flow == EAPEXRaceFlow::Countdown))
	{
		bIntroStarted = true;
		if (AAPEXCameraManager* CM = GetCameraManager()) { CM->PlayIntro(4.5f); }
	}
	if (bBot) { TickBot(Dt); }
	if (bSelfTest && !bMenuOpen) { TickSelfTest(Dt); }

	// ---- build the driving commands ----
	const FAPEXSteerTuning T = FAPEXDriveMath::Tuning(SteerPreset);
	ThrottleCmd = FMath::FInterpConstantTo(ThrottleCmd, ThrottleIn, Dt, ThrottleIn > ThrottleCmd ? 6.f : 10.f);       // progressive pedal, no instant 0 -> 1 on a key
	BrakeCmd = FMath::FInterpConstantTo(BrakeCmd, BrakeIn, Dt, BrakeIn > BrakeCmd ? 7.f : 12.f);
	if (bGamepadActive)
	{
		const float Analog = FAPEXDriveMath::ApplyDeadZone(PadSteer, GamepadDeadZone) * GamepadSensitivity;
		SteerRamp = FMath::FInterpTo(SteerRamp, FMath::Clamp(Analog, -1.f, 1.f), Dt, 30.f);                              // analog stick: only light smoothing
	}
	else
	{
		SteerRamp = FAPEXDriveMath::KeyboardSteerStep(SteerRamp, SteerTarget, Dt, T.KeyRise, T.KeyFall);                // digital keys: tunable ramp
	}
	SteerOut = FMath::Clamp(SteerRamp * T.Sensitivity * FAPEXDriveMath::SpeedSteerScale(Car->GetSpeedMps(), T.MinSpeedScale), -1.f, 1.f);

	if (Car->IsHeldOnGrid()) { Car->SetPreStartRev(ThrottleCmd); }       // pre-start: the throttle only revs the engine
	const bool bAccepted = Car->SetDriverInputs(ThrottleCmd, BrakeCmd, SteerOut);

	// "Racing but the car ignores me" check
	if (Flow == EAPEXRaceFlow::Racing && !bAccepted && Car->DrivingMode != EAPEXDrivingMode::AutonomousDemo)
	{
		BlockedTimer += Dt;
		if (BlockedTimer > 2.f && !bWarnedBlocked) { bWarnedBlocked = true; UE_LOG(LogApexInput, Error, TEXT("Inputs are blocked in the Racing state (car held on grid: %d)"), Car->IsHeldOnGrid()); }
	}
	else { BlockedTimer = 0.f; }

	DebugTimer += Dt;
	if (DebugTimer >= 2.f && (FParse::Param(FCommandLine::Get(), TEXT("APEXInputDebug")) || bBot)) { DebugTimer = 0.f; UE_LOG(LogApexInput, Log, TEXT("DRIVE %s"), *TelemetryLine()); }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// Scripted-input bot: converts the autopilot's suggestion into REAL key press / release events injected at the PlayerController, so the Enhanced Input
// mapping, the steering ramp, the speed limit and SetDriverInputs() are all exercised. Steering uses sigma-delta pulses (a key has no analog value).
void AAPEXPlayerController::BotSet(const FKey& Key, bool bDown)
{
	bool* Cur = BotKeys.Find(Key);
	if (Cur && *Cur == bDown) { return; }
	BotKeys.Add(Key, bDown);
	InputKey(FInputKeyParams(Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0 : 0.0, false));
}

void AAPEXPlayerController::TickBot(float Dt)
{
	AFormulaVehiclePawn* Car = GetCar();
	if (!Car) { return; }
	BotElapsed += Dt;
	const EAPEXRaceFlow Flow = GetRaceFlow();
	float T = 0.f, B = 0.f, S = 0.f;
	if (Flow == EAPEXRaceFlow::Racing) { Car->GetAutopilotSuggestion(Dt, T, B, S); }
	BotSet(EKeys::W, T > 0.5f);
	BotSet(EKeys::S, B > 0.3f);
	// the autopilot speaks the model's steering units; turn them into the normalised steering a person would hold
	const float Want = FMath::Clamp(AFormulaVehiclePawn::ModelSteerToNormalised(Car->Setup, S, Car->GetSpeedMps(), Car->GetDownforceForSteering(), Car->GetTyreScaleForSteering()), -1.f, 1.f);
	// Closed loop, like a person watching the car: steer toward the wanted value by pressing the key on the side of the error. The keyboard ramp is an
	// integrator (it rises while a key is held and falls back when released), so a fixed pulse train could never hold an angle; feedback can.
	const FAPEXSteerTuning Tun = FAPEXDriveMath::Tuning(SteerPreset);
	const float Authority = FMath::Max(Tun.Sensitivity * FAPEXDriveMath::SpeedSteerScale(Car->GetSpeedMps(), Tun.MinSpeedScale), 0.05f);
	const float RampWant = FMath::Clamp(Want / Authority, -1.f, 1.f);
	const bool bRight = RampWant > SteerRamp + 0.02f;
	const bool bLeft = RampWant < SteerRamp - 0.02f;
	if (bRight || bLeft) { ++BotSteerPulses; }
	BotSet(EKeys::D, bRight);
	BotSet(EKeys::A, bLeft);
	// a real player crashes sometimes: if the car sits still while racing, press Backspace (tests the reset path end to end)
	BotStuck = (Flow == EAPEXRaceFlow::Racing && Car->GetSpeedKph() < 4.f && BotElapsed > 20.f) ? BotStuck + Dt : 0.f;
	if (BotStuck > 2.5f) { BotStuck = 0.f; ++BotResets; InputKey(FInputKeyParams(EKeys::BackSpace, IE_Pressed, 1.0, false)); InputKey(FInputKeyParams(EKeys::BackSpace, IE_Released, 0.0, false)); }
	BotMaxSpeedKph = FMath::Max(BotMaxSpeedKph, Car->GetSpeedKph());
	if (Car->GetThrottleInput() > 0.3f) { ++BotThrottleFrames; }
	if (Car->GetBrakeInput() > 0.3f) { ++BotBrakeFrames; }

	BotLogTimer += Dt;
	if (BotLogTimer > 10.f)
	{
		BotLogTimer = 0.f;
		UE_LOG(LogApexInput, Log, TEXT("BOT t=%.0fs %s laps=%d"), BotElapsed, *TelemetryLine(), Car->LapTiming ? Car->LapTiming->CompletedLaps : -1);
	}
	if (Car->LapTiming && Car->LapTiming->IsRaceFinished())
	{
		BotFinishedTimer += Dt;
		if (BotFinishedTimer > 1.5f)
		{
			UE_LOG(LogApexInput, Log, TEXT("BOT RESULT: finished %d laps manually through injected keys in %.1f s, best lap %.2f s, max speed %.0f km/h, steering pulses %d, throttle frames %d, brake frames %d, resets %d"),
				Car->LapTiming->CompletedLaps, BotElapsed, Car->LapTiming->BestLapTime, BotMaxSpeedKph, BotSteerPulses, BotThrottleFrames, BotBrakeFrames, BotResets);
			FPlatformMisc::RequestExit(false);
		}
	}
	else if (BotElapsed > 900.f) { UE_LOG(LogApexInput, Error, TEXT("BOT TIMEOUT: laps=%d"), Car->LapTiming ? Car->LapTiming->CompletedLaps : -1); FPlatformMisc::RequestExit(false); }
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------------
void AAPEXPlayerController::SelfCheck(bool bOk, const FString& Name, const FString& Detail)
{
	(bOk ? SelfPass : SelfFail)++;
	if (bOk) { UE_LOG(LogApexInput, Log, TEXT("SELFTEST PASS %s: %s"), *Name, *Detail); }
	else { UE_LOG(LogApexInput, Error, TEXT("SELFTEST FAIL %s: %s"), *Name, *Detail); }
}

void AAPEXPlayerController::SelfTap(const FKey& Key)
{
	InputKey(FInputKeyParams(Key, IE_Pressed, 1.0, false));
	PendingReleases.Add({ Key, 0.06f });
}

void AAPEXPlayerController::TickSelfTest(float Dt)
{
	AFormulaVehiclePawn* Car = GetCar();
	if (!Car) { return; }
	for (int32 i = PendingReleases.Num() - 1; i >= 0; --i)
	{
		PendingReleases[i].Value -= Dt;
		if (PendingReleases[i].Value <= 0.f) { InputKey(FInputKeyParams(PendingReleases[i].Key, IE_Released, 0.0, false)); PendingReleases.RemoveAt(i); }
	}
	if (bPadStreamZero) { InputKey(FInputKeyParams(EKeys::Gamepad_RightTriggerAxis, 0.001, Dt, 1, true)); InputKey(FInputKeyParams(EKeys::Gamepad_LeftX, 0.001, Dt, 1, true)); }
	const EAPEXRaceFlow Flow = GetRaceFlow();
	// ---- start menu (only with -APEXStartMenu) ----
	if (bStartMenu || SelfMenuSub == 1)
	{
		SelfMenuT += Dt;
		if (SelfMenuSub == 0 && SelfMenuT > 0.5f)
		{
			SelfCheck(Flow == EAPEXRaceFlow::Menu && UGameplayStatics::IsGamePaused(this) && bMenuOpen && MenuPage == PageStart, TEXT("T13a start menu is shown, world paused"), FString::Printf(TEXT("flow %s, paused %d, page %d"), *FlowName(Flow), UGameplayStatics::IsGamePaused(this), MenuPage));
			SelfTap(EKeys::Enter); SelfMenuSub = 1; SelfMenuT = 0.f;
		}
		else if (SelfMenuSub == 1 && SelfMenuT > 0.6f)
		{
			SelfCheck(!bStartMenu && !UGameplayStatics::IsGamePaused(this) && Flow != EAPEXRaceFlow::Menu, TEXT("T13b Start Race begins the race"), FString::Printf(TEXT("flow now %s"), *FlowName(Flow)));
			SelfMenuSub = 2;
		}
		if (bStartMenu) { return; }
	}
	// ---- pre-start: the throttle revs the held car but never moves it; intro cameras hand back ----
	if (!bPreDone && (Flow == EAPEXRaceFlow::Countdown || Flow == EAPEXRaceFlow::Grid))
	{
		SelfPreT += Dt;
		BotSet(EKeys::W, true);
		if (bIntroForced && !bIntroChecked && SelfPreT > 0.6f)
		{
			bIntroChecked = true;
			AAPEXCameraManager* CM0 = GetCameraManager();
			SelfCheck(CM0 && CM0->GetView() == EAPEXCamView::Cinematic && CM0->IsIntroPlaying(), TEXT("T10a race intro is playing the broadcast cameras"), FString::Printf(TEXT("view %s, intro playing %d"), CM0 ? *AAPEXCameraManager::ViewName(CM0->GetView()) : TEXT("none"), CM0 ? CM0->IsIntroPlaying() : 0));
		}
		if (SelfPreT > 1.2f)
		{
			bPreDone = true; BotSet(EKeys::W, false);
			SelfCheck(Car->IsHeldOnGrid() && Car->GetRev01() > 0.6f && Car->GetSpeedKph() < 1.f, TEXT("T11 pre-start: throttle revs the held car, it does not move"), FString::Printf(TEXT("held %d, rev %.2f, speed %.1f km/h"), Car->IsHeldOnGrid() ? 1 : 0, Car->GetRev01(), Car->GetSpeedKph()));
		}
	}
	if (SelfStep == 0 && Flow != EAPEXRaceFlow::Racing) { return; }          // wait for the lights to go out
	SelfT += Dt;
	AAPEXCameraManager* Cam = GetCameraManager();
	auto Hold = [this](const FKey& K, bool bDown) { BotSet(K, bDown); };
	auto YawNow = [Car]() { return Car->GetActorRotation().Yaw; };

	switch (SelfStep)
	{
	case 0:   // T1 possession + T2 acceleration
		if (SelfSub == 0 && bIntroForced)
		{
			AAPEXCameraManager* CM1 = GetCameraManager();
			const bool bOk = CM1 && !CM1->IsIntroPlaying() && CM1->GetView() == EAPEXCamView::Chase && GetViewTarget() == Car;
			SelfCheck(bOk, TEXT("T10b after the intro the player has the camera and the car"), FString::Printf(TEXT("view %s, view target is the player car: %d, intro playing %d"), CM1 ? *AAPEXCameraManager::ViewName(CM1->GetView()) : TEXT("none"), GetViewTarget() == Car ? 1 : 0, CM1 ? CM1->IsIntroPlaying() : 0));
		}
		if (SelfSub == 0)
		{
			SelfSub = 1;
			SelfCheck(GetPawn() == Car && !Car->IsAI() && Car->DrivingMode == EAPEXDrivingMode::Manual && !Car->IsAutoDriving(), TEXT("T1 possession"),
				FString::Printf(TEXT("controller possesses %s, mode %s, autoDrive=%d"), *Car->GetName(), *ModeName(Car->DrivingMode), Car->IsAutoDriving() ? 1 : 0));
			SelfMark0 = Car->GetSpeedKph();
		}
		Hold(EKeys::W, true);
		if (SelfT > 2.5f) { SelfCheck(Car->GetSpeedKph() > SelfMark0 + 40.f, TEXT("T2 acceleration"), FString::Printf(TEXT("W held: %.0f -> %.0f km/h in 2.5 s, throttle input %.2f"), SelfMark0, Car->GetSpeedKph(), Car->GetThrottleInput())); SelfMark0 = Car->GetSpeedKph(); SelfNext(); }
		break;
	case 1:   // T3 braking
		Hold(EKeys::W, false); Hold(EKeys::S, true);
		if (SelfT > 1.2f) { SelfCheck(Car->GetSpeedKph() < SelfMark0 - 25.f, TEXT("T3 braking"), FString::Printf(TEXT("S held: %.0f -> %.0f km/h, brake input %.2f"), SelfMark0, Car->GetSpeedKph(), Car->GetBrakeInput())); Hold(EKeys::S, false); SelfNext(); }
		break;
	case 2:   // T4 steering right then left (a little throttle keeps the car moving)
		Hold(EKeys::W, SelfT < 2.0f && Car->GetSpeedKph() < 90.f);
		if (SelfSub == 0) { SelfMark0 = YawNow(); SelfSub = 1; }
		Hold(EKeys::D, SelfT < 0.7f);
		if (SelfT > 1.0f && SelfSub == 1) { SelfSub = 2; SelfMark1 = FMath::FindDeltaAngleDegrees(SelfMark0, YawNow()); SelfCheck(SelfMark1 > 3.f, TEXT("T4a steer right (D)"), FString::Printf(TEXT("heading change %+.1f deg"), SelfMark1)); SelfMark0 = YawNow(); }
		Hold(EKeys::A, SelfT > 1.05f && SelfT < 2.2f);
		if (SelfT > 2.6f && SelfSub == 2) { SelfSub = 3; const float D = FMath::FindDeltaAngleDegrees(SelfMark0, YawNow()); SelfCheck(D < -3.f, TEXT("T4b steer left (A)"), FString::Printf(TEXT("heading change %+.1f deg"), D)); Hold(EKeys::A, false); Hold(EKeys::W, false); SelfNext(); }
		break;
	case 3:   // opposing keys cancel
		Hold(EKeys::A, true); Hold(EKeys::D, true);
		if (SelfT > 0.8f) { SelfCheck(FMath::Abs(SteerTarget) < 0.05f, TEXT("T4c opposing steering keys"), FString::Printf(TEXT("A+D held: steering target %.2f"), SteerTarget)); Hold(EKeys::A, false); Hold(EKeys::D, false); SelfNext(); }
		break;
	case 4:   // T5 gear shifts with the manual gearbox (Simulation preset)
		if (SelfSub == 0) { SelfSub = 1; SetPreset(EAPEXSteerPreset::Simulation); SelfMark0 = Car->GetGear(); SelfTap(EKeys::E); }
		if (SelfT > 0.25f && SelfSub == 1) { SelfSub = 2; SelfMark1 = Car->GetGear(); SelfTap(EKeys::E); }
		if (SelfT > 0.55f && SelfSub == 2) { SelfSub = 3; SelfTap(EKeys::Q); }
		if (SelfT > 0.9f && SelfSub == 3)
		{
			const int32 G = Car->GetGear();
			SelfCheck(SelfMark1 == SelfMark0 + 1 && G == SelfMark0 + 1 && Car->IsManualGearbox(), TEXT("T5 gear shift (E / Q, manual gearbox)"), FString::Printf(TEXT("gear %d -> E -> %d -> E -> Q -> %d (expect %d, then %d)"), static_cast<int32>(SelfMark0), static_cast<int32>(SelfMark1), G, static_cast<int32>(SelfMark0) + 1, static_cast<int32>(SelfMark0) + 1));
			SetPreset(EAPEXSteerPreset::Sport); SelfNext();
		}
		break;
	case 5:   // T6 camera cycle + look back + handoff
	{
		static const EAPEXCamView Seq[6] = { EAPEXCamView::Cockpit, EAPEXCamView::FrontWing, EAPEXCamView::Rear, EAPEXCamView::Nose, EAPEXCamView::Cinematic, EAPEXCamView::Chase };
		const int32 Idx = static_cast<int32>(SelfT / 0.5f);
		if (Idx < 6 && FMath::Fmod(SelfT, 0.5f) < Dt + 0.001f && SelfT > 0.01f && SelfMark0 < Idx + 1) { SelfMark0 = Idx + 1; }
		if (SelfT < 0.02f) { SelfMark0 = 0.f; SelfMark1 = 0.f; }
		if (SelfT > 0.2f && SelfMark1 < 1.f) { SelfTap(EKeys::C); SelfMark1 = 1.f; }
		for (int32 k = 1; k <= 5; ++k) { if (SelfT > 0.2f + 0.5f * k && SelfMark1 < 1.f + k) { SelfCheck(Cam && Cam->GetView() == Seq[k - 1], FString::Printf(TEXT("T6 camera cycle step %d"), k), FString::Printf(TEXT("expected %s, view is %s"), *AAPEXCameraManager::ViewName(Seq[k - 1]), Cam ? *AAPEXCameraManager::ViewName(Cam->GetView()) : TEXT("none"))); SelfTap(EKeys::C); SelfMark1 = 1.f + k; } }
		if (SelfT > 3.2f && SelfMark1 < 7.f) { SelfCheck(Cam && Cam->GetView() == EAPEXCamView::Chase, TEXT("T6 camera cycle wraps to Chase"), FString::Printf(TEXT("view %s"), Cam ? *AAPEXCameraManager::ViewName(Cam->GetView()) : TEXT("none"))); SelfMark1 = 7.f; }
		if (SelfT > 4.4f && SelfT < 4.45f) { InputKey(FInputKeyParams(EKeys::R, IE_Pressed, 1.0, false)); }
		if (SelfT > 4.9f && SelfMark1 < 8.f) { SelfCheck(Cam && Cam->GetView() == EAPEXCamView::Rear && Car->GetCameraView() == EAPEXCamView::Rear, TEXT("T6 look behind (R held)"), TEXT("rear camera active while held")); InputKey(FInputKeyParams(EKeys::R, IE_Released, 0.0, false)); SelfMark1 = 8.f; }
		if (SelfT > 5.4f)
		{
			bool bPlayerView = GetViewTarget() == Car;
			SelfCheck(Cam && Cam->GetView() == EAPEXCamView::Chase && bPlayerView, TEXT("T6/T10 view restored to the player's car"), FString::Printf(TEXT("view %s, view target is player car: %d"), Cam ? *AAPEXCameraManager::ViewName(Cam->GetView()) : TEXT("none"), bPlayerView ? 1 : 0));
			SelfNext();
		}
		break;
	}
	case 6:   // T7 pause: world freezes, resume restores control
		if (SelfSub == 0) { SelfSub = 1; SelfMark0 = 0.f; SelfMark1 = 0.f; SelfTap(EKeys::BackSpace); }       // fresh state; the earlier steering test may have left the car against a barrier
		if (SelfT > 0.3f) { Hold(EKeys::W, SelfMark0 < 2.f || SelfMark0 >= 4.f); }
		if (SelfT > 1.5f && SelfMark0 < 1.f) { SelfMark0 = 1.f; SelfT = 0.f; SelfTap(EKeys::Escape); }
		else if (SelfMark0 >= 1.f && SelfMark0 < 2.f && SelfT > 0.2f)
		{
			SelfCheck(UGameplayStatics::IsGamePaused(this) && bMenuOpen && GetRaceFlow() == EAPEXRaceFlow::Paused, TEXT("T7a Escape pauses"), FString::Printf(TEXT("paused=%d menu=%d flow=%s"), UGameplayStatics::IsGamePaused(this), bMenuOpen, *FlowName(GetRaceFlow())));
			SelfMark0 = 2.f; SelfPos = Car->GetActorLocation(); SelfT = 0.f; SelfMark1 = 0.f;
		}
		else if (SelfMark0 >= 2.f && SelfMark0 < 3.f && SelfT > 0.6f)
		{
			SelfCheck(FVector::Dist(SelfPos, Car->GetActorLocation()) < 1.f, TEXT("T7b the world is frozen while paused"), FString::Printf(TEXT("car moved %.2f cm in 0.6 s paused"), FVector::Dist(SelfPos, Car->GetActorLocation())));
			SelfMark0 = 3.f; SelfTap(EKeys::Escape); SelfT = 0.f;
		}
		else if (SelfMark0 >= 3.f && SelfMark0 < 4.f && SelfT > 0.4f) { SelfMark0 = 4.f; SelfMark1 = Car->GetSpeedKph(); SelfT = 0.f; Hold(EKeys::W, false); SelfCheck(!UGameplayStatics::IsGamePaused(this) && !bMenuOpen, TEXT("T7c Escape resumes"), TEXT("game un-paused, menu closed")); }
		else if (SelfMark0 >= 4.f)
		{
			if (SelfT > 0.2f && SelfSub == 1) { SelfSub = 2; Hold(EKeys::W, true); }        // pressed a few frames after the release
			if (SelfT > 1.7f)
			{
				SelfCheck(Car->GetSpeedKph() > SelfMark1 + 8.f, TEXT("T7d controls work after resume"), FString::Printf(TEXT("W pressed after resume: %.0f -> %.0f km/h (throttle in %.2f, key down %d, car throttle %.2f, paused %d)"), SelfMark1, Car->GetSpeedKph(), ThrottleIn, IsInputKeyDown(EKeys::W) ? 1 : 0, Car->GetThrottleInput(), UGameplayStatics::IsGamePaused(this) ? 1 : 0));
				Hold(EKeys::W, false); SelfNext();
			}
		}
		break;
	case 7:   // reset to a safe spot
		if (SelfT < 0.02f) { SelfPos = Car->GetActorLocation(); }
		if (SelfT > 0.1f && SelfT < 0.15f) { SelfTap(EKeys::BackSpace); }
		if (SelfT > 0.5f)
		{
			SelfCheck(Car->GetSpeedKph() < 3.f && FVector::Dist2D(SelfPos, Car->GetActorLocation()) > 1.f, TEXT("T8a reset (Backspace)"), FString::Printf(TEXT("car stopped at %.0f km/h, moved %.0f cm to a validated spot"), Car->GetSpeedKph(), FVector::Dist2D(SelfPos, Car->GetActorLocation())));
			SelfNext();
		}
		break;
	case 8:   // gamepad analog input
		if (SelfT < 0.02f) { SelfMark0 = 0.f; }
		InputKey(FInputKeyParams(EKeys::Gamepad_RightTriggerAxis, 0.9, Dt, 1, true));
		InputKey(FInputKeyParams(EKeys::Gamepad_LeftX, 0.6, Dt, 1, true));
		if (SelfT > 1.2f)
		{
			SelfCheck(Car->GetThrottleInput() > 0.5f && Car->GetSteerInput() > 0.002f && bGamepadActive, TEXT("T8b gamepad (right trigger + left stick)"), FString::Printf(TEXT("throttle %.2f, steer %.3f, speed %.0f km/h, gamepad active %d"), Car->GetThrottleInput(), Car->GetSteerInput(), Car->GetSpeedKph(), bGamepadActive ? 1 : 0));
			InputKey(FInputKeyParams(EKeys::Gamepad_RightTriggerAxis, 0.001, Dt, 1, true));       // a released trigger reports ~0, below the dead zone
			InputKey(FInputKeyParams(EKeys::Gamepad_LeftX, 0.001, Dt, 1, true));
			bPadStreamZero = true; bGamepadActive = false;      // real hardware keeps streaming the released (~0) value every frame; the test does the same from now on
			SelfNext();
		}
		break;
	case 9:   // energy boost
		if (SelfT < 0.02f) { SelfMark0 = Car->GetEnergy(); }
		Hold(EKeys::W, true); Hold(EKeys::LeftShift, true);
		if (SelfT > 2.0f) { SelfCheck(Car->GetEnergy() < SelfMark0 - 0.05f || Car->GetEnergy() < 0.05f, TEXT("T8c energy boost (Left Shift)"), FString::Printf(TEXT("energy %.2f -> %.2f, boosting=%d"), SelfMark0, Car->GetEnergy(), Car->IsBoosting() ? 1 : 0)); Hold(EKeys::LeftShift, false); Hold(EKeys::W, false); SelfNext(); }
		break;
	case 10:  // remap Throttle through the real menu: open, navigate, capture a key
	{
		UAPEXInputConfig* Cfg = InputConfig;
		auto Primary = [Cfg]() { for (const UAPEXInputConfig::FBindSlot& S : Cfg->GetSlots()) { if (S.Name == FName(TEXT("Throttle"))) { return S.Primary; } } return FKey(); };
		if (SelfSub == 0 && SelfT > 0.1f) { SelfSub = 1; SelfT = 0.f; SelfTap(EKeys::Escape); }
		else if (SelfSub == 1 && SelfT > 0.3f) { SelfSub = 2; SelfT = 0.f; SelfCheck(bMenuOpen && MenuPage == PageMain, TEXT("T12a pause menu opens"), FString::Printf(TEXT("menu %d page %d"), bMenuOpen, MenuPage)); SelfTap(EKeys::Down); }
		else if (SelfSub == 2 && SelfT > 0.25f) { SelfSub = 3; SelfT = 0.f; SelfTap(EKeys::Down); }
		else if (SelfSub == 3 && SelfT > 0.25f) { SelfSub = 4; SelfT = 0.f; SelfCheck(MenuIndex == 2, TEXT("T12b menu navigation (Down, Down)"), FString::Printf(TEXT("selected row %d (Controls = 2)"), MenuIndex)); SelfTap(EKeys::Enter); }
		else if (SelfSub == 4 && SelfT > 0.3f) { SelfSub = 5; SelfT = 0.f; SelfCheck(MenuPage == PageControls, TEXT("T12c Enter opens the Controls page"), FString::Printf(TEXT("page %d"), MenuPage)); SelfTap(EKeys::Enter); }
		else if (SelfSub == 5 && SelfT > 0.3f) { SelfSub = 6; SelfT = 0.f; SelfCheck(bCapturing, TEXT("T12d Enter on Throttle starts key capture"), FString::Printf(TEXT("capturing %d"), bCapturing)); }
		else if (SelfSub == 6 && SelfT > 0.5f) { SelfSub = 7; SelfT = 0.f; SelfTap(EKeys::I); }
		else if (SelfSub == 7 && SelfT > 0.4f)
		{
			SelfSub = 8; SelfT = 0.f;
			FString Saved; if (GConfig) { GConfig->GetString(TEXT("APEXKeyBindings"), TEXT("Throttle"), Saved, GGameUserSettingsIni); }
			SelfCheck(Primary() == EKeys::I && !bCapturing && Saved == TEXT("I"), TEXT("T12e throttle rebound to I and saved"), FString::Printf(TEXT("throttle key %s, saved \"%s\""), *Primary().ToString(), *Saved));
			SelfTap(EKeys::Escape);
		}
		else if (SelfSub == 8 && SelfT > 0.3f) { SelfSub = 9; SelfT = 0.f; SelfTap(EKeys::Escape); }       // back to the main page, then resume
		else if (SelfSub == 9 && SelfT > 0.4f) { SelfSub = 10; SelfT = 0.f; SelfCheck(!UGameplayStatics::IsGamePaused(this), TEXT("T12f menu closed, race running"), TEXT("un-paused")); SelfTap(EKeys::BackSpace); }
		else if (SelfSub == 10 && SelfT > 0.4f) { SelfSub = 11; SelfT = 0.f; Hold(EKeys::W, true); }
		else if (SelfSub == 11 && SelfT > 0.8f) { SelfSub = 12; SelfT = 0.f; SelfMark0 = Car->GetThrottleInput(); Hold(EKeys::W, false); Hold(EKeys::I, true); }
		else if (SelfSub == 12 && SelfT > 0.8f)
		{
			{
				FString Keys; for (const FKey& K : InputConfig->KeysFor(InputConfig->IA_Throttle)) { Keys += K.ToString() + TEXT(" "); }
				SelfCheck(SelfMark0 < 0.15f && Car->GetThrottleInput() > 0.5f, TEXT("T12g the old key no longer drives, the new key does"), FString::Printf(TEXT("W held: throttle %.2f; I held: throttle %.2f; throttle keys now [%s]; pad trigger raw %.2f, gamepad active %d"), SelfMark0, Car->GetThrottleInput(), *Keys, GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis), bGamepadActive ? 1 : 0));
			}
			Hold(EKeys::I, false); ResetBindingsAndSave(); SelfNext();
		}
		break;
	}
	case 11:  // defaults restored
		if (SelfT > 0.3f)
		{
			FKey P; for (const UAPEXInputConfig::FBindSlot& S : InputConfig->GetSlots()) { if (S.Name == FName(TEXT("Throttle"))) { P = S.Primary; } }
			SelfCheck(P == EKeys::W, TEXT("T12h reset to defaults restores W"), FString::Printf(TEXT("throttle key %s"), *P.ToString()));
			SelfNext();
		}
		break;
	case 12:  // rumble request + gamepad disconnect while racing
	{
		if (SelfSub == 0) { SelfSub = 1; SelfT = 0.f; bGamepadActive = true; SelfMark0 = static_cast<float>(RumbleCount); Rumble(0.4f, 0.1f); }
		else if (SelfSub == 1 && SelfT > 0.3f)
		{
			SelfSub = 2; SelfT = 0.f;
			SelfCheck(RumbleCount == static_cast<int32>(SelfMark0) + 1, TEXT("T14a rumble is requested for a pad user"), FString::Printf(TEXT("requests %d -> %d (the hardware effect itself cannot be verified here)"), static_cast<int32>(SelfMark0), RumbleCount));
			bGamepadActive = true; ThrottleIn = 0.9f;                 // a pad user mid-race with the trigger held
			HandleDeviceLost();
		}
		else if (SelfSub == 2 && SelfT > 0.2f)
		{
			SelfSub = 3; SelfT = 0.f;
			SelfCheck(UGameplayStatics::IsGamePaused(this) && bMenuOpen && !bGamepadActive && ThrottleIn == 0.f, TEXT("T14b gamepad lost mid-race: inputs cleared, race paused"), FString::Printf(TEXT("paused %d, menu %d, pad active %d, throttle in %.2f"), UGameplayStatics::IsGamePaused(this), bMenuOpen, bGamepadActive, ThrottleIn));
			SelfTap(EKeys::Escape);
		}
		else if (SelfSub == 3 && SelfT > 0.4f) { SelfCheck(!UGameplayStatics::IsGamePaused(this), TEXT("T14c resume after the pad loss"), TEXT("un-paused")); SelfNext(); }
		break;
	}
	case 13:  // gamepad button rebinding through the real menu: Escape, Controls, Boost row, capture a pad button
	{
		static const FKey Script[] = { EKeys::Escape, EKeys::Down, EKeys::Down, EKeys::Enter, EKeys::Down, EKeys::Down, EKeys::Down, EKeys::Down, EKeys::Down, EKeys::Down, EKeys::Down, EKeys::Enter };
		const int32 N = UE_ARRAY_COUNT(Script);
		if (SelfSub < N) { if (SelfT > 0.3f) { SelfT = 0.f; SelfTap(Script[SelfSub]); ++SelfSub; } }
		else if (SelfSub == N && SelfT > 0.6f)
		{
			SelfCheck(bCapturing && CaptureSlot == FName(TEXT("Boost")), TEXT("T15a navigated to the Boost row and started capture"), FString::Printf(TEXT("capturing %d slot %s"), bCapturing, *CaptureSlot.ToString()));
			SelfSub = N + 1; SelfT = 0.f;
			InputKey(FInputKeyParams(EKeys::Gamepad_LeftThumbstick, IE_Pressed, 1.0, true));
			PendingReleases.Add({ EKeys::Gamepad_LeftThumbstick, 0.08f });
		}
		else if (SelfSub == N + 1 && SelfT > 0.5f)
		{
			FKey Pad; FKey Kb; for (const UAPEXInputConfig::FBindSlot& S : InputConfig->GetSlots()) { if (S.Name == FName(TEXT("Boost"))) { Pad = S.PadKey; Kb = S.Primary; } }
			FString Saved; if (GConfig) { GConfig->GetString(TEXT("APEXKeyBindings"), TEXT("Boost_Pad"), Saved, GGameUserSettingsIni); }
			SelfCheck(Pad == EKeys::Gamepad_LeftThumbstick && Kb == EKeys::LeftShift && !bCapturing && Saved == TEXT("Gamepad_LeftThumbstick"), TEXT("T15b pad button rebound and saved, keyboard key untouched"), FString::Printf(TEXT("pad %s, keyboard %s, saved \"%s\""), *Pad.ToString(), *Kb.ToString(), *Saved));
			SelfSub = N + 2; SelfT = 0.f; SelfTap(EKeys::Escape);
		}
		else if (SelfSub == N + 2 && SelfT > 0.3f) { SelfSub = N + 3; SelfT = 0.f; SelfTap(EKeys::Escape); }
		else if (SelfSub == N + 3 && SelfT > 0.4f) { ResetBindingsAndSave(); SelfNext(); }
		break;
	}
	case 14:  // AI separation
	{
		int32 AICount = 0, Bad = 0;
		for (TActorIterator<AFormulaVehiclePawn> It(GetWorld()); It; ++It)
		{
			if (*It == Car) { continue; }
			++AICount;
			if (!It->IsAI() || It->GetController() == this || (It->GetController() && It->GetController()->IsLocalPlayerController())) { ++Bad; }
		}
		SelfCheck(AICount >= 10 && Bad == 0 && Car->GetController() == this, TEXT("T9 AI / player separation"), FString::Printf(TEXT("%d AI cars, none owned by the player controller (%d violations); player car has no autopilot"), AICount, Bad));
		UE_LOG(LogApexInput, Log, TEXT("SELFTEST COMPLETE: %d passed, %d failed"), SelfPass, SelfFail);
		FPlatformMisc::RequestExit(false);
		SelfNext();
		break;
	}
	default: break;
	}
}
