#include "Input/APEXInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type, bool bTriggerWhenPaused = false)
	{
		UInputAction* A = NewObject<UInputAction>(Outer, Name);
		A->ValueType = Type;
		A->bTriggerWhenPaused = bTriggerWhenPaused;
		return A;
	}
}

void UAPEXInputConfig::Build()
{
	IA_Throttle    = MakeAction(this, TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	IA_Brake       = MakeAction(this, TEXT("IA_Brake"), EInputActionValueType::Axis1D);
	IA_Steer       = MakeAction(this, TEXT("IA_Steer"), EInputActionValueType::Axis1D);
	IA_Steer->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;   // A and D held together cancel to 0 instead of picking one
	IA_ShiftUp     = MakeAction(this, TEXT("IA_ShiftUp"), EInputActionValueType::Boolean);
	IA_ShiftDown   = MakeAction(this, TEXT("IA_ShiftDown"), EInputActionValueType::Boolean);
	IA_Handbrake   = MakeAction(this, TEXT("IA_Handbrake"), EInputActionValueType::Boolean);
	IA_EnergyBoost = MakeAction(this, TEXT("IA_EnergyBoost"), EInputActionValueType::Boolean);
	IA_OvertakeAid = MakeAction(this, TEXT("IA_OvertakeAid"), EInputActionValueType::Boolean);
	IA_CameraSwitch = MakeAction(this, TEXT("IA_CameraSwitch"), EInputActionValueType::Boolean);
	IA_LookBack    = MakeAction(this, TEXT("IA_LookBack"), EInputActionValueType::Boolean);
	IA_ResetCar    = MakeAction(this, TEXT("IA_ResetCar"), EInputActionValueType::Boolean);
	IA_Pause       = MakeAction(this, TEXT("IA_Pause"), EInputActionValueType::Boolean, /*bTriggerWhenPaused=*/true);
	IA_Confirm     = MakeAction(this, TEXT("IA_Confirm"), EInputActionValueType::Boolean, true);
	IA_Cancel      = MakeAction(this, TEXT("IA_Cancel"), EInputActionValueType::Boolean, true);

	IMC_APEX_Driving = NewObject<UInputMappingContext>(this, TEXT("IMC_APEX_Driving"));
	UInputMappingContext* C = IMC_APEX_Driving;

	// ---- keyboard ----
	C->MapKey(IA_Throttle, EKeys::W);  C->MapKey(IA_Throttle, EKeys::Up);
	C->MapKey(IA_Brake, EKeys::S);     C->MapKey(IA_Brake, EKeys::Down);
	C->MapKey(IA_Steer, EKeys::D);     C->MapKey(IA_Steer, EKeys::Right);                            // +1
	{
		FEnhancedActionKeyMapping& L = C->MapKey(IA_Steer, EKeys::A);  L.Modifiers.Add(NewObject<UInputModifierNegate>(this));   // -1
		FEnhancedActionKeyMapping& L2 = C->MapKey(IA_Steer, EKeys::Left); L2.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	C->MapKey(IA_ShiftUp, EKeys::E);   C->MapKey(IA_ShiftDown, EKeys::Q);
	C->MapKey(IA_Handbrake, EKeys::SpaceBar);
	C->MapKey(IA_EnergyBoost, EKeys::LeftShift);
	C->MapKey(IA_OvertakeAid, EKeys::F);
	C->MapKey(IA_CameraSwitch, EKeys::C);
	C->MapKey(IA_LookBack, EKeys::R);
	C->MapKey(IA_ResetCar, EKeys::BackSpace);
	C->MapKey(IA_Pause, EKeys::Escape);
	C->MapKey(IA_Confirm, EKeys::Enter);
	C->MapKey(IA_Cancel, EKeys::Tab);

	// ---- gamepad (platform independent keys; dead zone on the stick, triggers are 0..1 axes) ----
	{
		FEnhancedActionKeyMapping& S = C->MapKey(IA_Steer, EKeys::Gamepad_LeftX);
		UInputModifierDeadZone* DZ = NewObject<UInputModifierDeadZone>(this); DZ->LowerThreshold = 0.10f; DZ->UpperThreshold = 1.0f; DZ->Type = EDeadZoneType::Axial;
		S.Modifiers.Add(DZ);
	}
	C->MapKey(IA_Throttle, EKeys::Gamepad_RightTriggerAxis);
	C->MapKey(IA_Brake, EKeys::Gamepad_LeftTriggerAxis);
	C->MapKey(IA_ShiftUp, EKeys::Gamepad_RightShoulder);
	C->MapKey(IA_ShiftDown, EKeys::Gamepad_LeftShoulder);
	C->MapKey(IA_EnergyBoost, EKeys::Gamepad_FaceButton_Bottom);
	C->MapKey(IA_OvertakeAid, EKeys::Gamepad_FaceButton_Right);
	C->MapKey(IA_CameraSwitch, EKeys::Gamepad_FaceButton_Top);
	C->MapKey(IA_LookBack, EKeys::Gamepad_FaceButton_Left);
	C->MapKey(IA_Pause, EKeys::Gamepad_Special_Right);
	C->MapKey(IA_ResetCar, EKeys::Gamepad_Special_Left);
	C->MapKey(IA_Confirm, EKeys::Gamepad_FaceButton_Bottom);
	C->MapKey(IA_Handbrake, EKeys::Gamepad_RightThumbstick);

	auto Slot = [this](const TCHAR* Name, const TCHAR* Label, const UInputAction* A, bool bNeg, FKey Primary, FKey Alt, FKey Pad = FKey())
	{
		FBindSlot S; S.Name = Name; S.Label = Label; S.Action = A; S.bNegate = bNeg; S.DefaultPrimary = Primary; S.Primary = Primary; S.Alternate = Alt; S.DefaultPad = Pad; S.PadKey = Pad;
		Slots.Add(S);
	};
	Slots.Reset();
	Slot(TEXT("Throttle"), TEXT("Throttle"), IA_Throttle, false, EKeys::W, EKeys::Up);
	Slot(TEXT("Brake"), TEXT("Brake"), IA_Brake, false, EKeys::S, EKeys::Down);
	Slot(TEXT("SteerLeft"), TEXT("Steer left"), IA_Steer, true, EKeys::A, EKeys::Left);
	Slot(TEXT("SteerRight"), TEXT("Steer right"), IA_Steer, false, EKeys::D, EKeys::Right);
	Slot(TEXT("ShiftUp"), TEXT("Shift up"), IA_ShiftUp, false, EKeys::E, FKey(), EKeys::Gamepad_RightShoulder);
	Slot(TEXT("ShiftDown"), TEXT("Shift down"), IA_ShiftDown, false, EKeys::Q, FKey(), EKeys::Gamepad_LeftShoulder);
	Slot(TEXT("Handbrake"), TEXT("Handbrake"), IA_Handbrake, false, EKeys::SpaceBar, FKey(), EKeys::Gamepad_RightThumbstick);
	Slot(TEXT("Boost"), TEXT("Energy boost"), IA_EnergyBoost, false, EKeys::LeftShift, FKey(), EKeys::Gamepad_FaceButton_Bottom);
	Slot(TEXT("Overtake"), TEXT("Overtake aid"), IA_OvertakeAid, false, EKeys::F, FKey(), EKeys::Gamepad_FaceButton_Right);
	Slot(TEXT("Camera"), TEXT("Camera"), IA_CameraSwitch, false, EKeys::C, FKey(), EKeys::Gamepad_FaceButton_Top);
	Slot(TEXT("LookBack"), TEXT("Look behind"), IA_LookBack, false, EKeys::R, FKey(), EKeys::Gamepad_FaceButton_Left);
	Slot(TEXT("Reset"), TEXT("Reset car"), IA_ResetCar, false, EKeys::BackSpace, FKey(), EKeys::Gamepad_Special_Left);
	LoadBindings();
}

FKey UAPEXInputConfig::KeyNow(const FKey& DefaultKey) const
{
	for (const FBindSlot& S : Slots) { if (S.DefaultPrimary == DefaultKey) { return S.Primary; } if (S.DefaultPad.IsValid() && S.DefaultPad == DefaultKey) { return S.PadKey; } }
	return DefaultKey;
}

bool UAPEXInputConfig::Rebind(FName SlotName, const FKey& NewKey, FString& OutError)
{
	FBindSlot* Slot = Slots.FindByPredicate([&](const FBindSlot& S) { return S.Name == SlotName; });
	if (!Slot || !IMC_APEX_Driving) { OutError = TEXT("unknown binding"); return false; }
	if (!NewKey.IsValid() || NewKey == EKeys::AnyKey) { OutError = TEXT("not a real key"); return false; }
	if (NewKey == Slot->Primary) { return true; }
	if (NewKey == EKeys::Escape || NewKey == EKeys::Enter || NewKey == EKeys::Tab) { OutError = TEXT("that key is reserved for the menus"); return false; }
	if (NewKey.IsMouseButton() || NewKey.IsAxis1D() || NewKey.IsAxis2D() || NewKey.IsAxis3D()) { OutError = TEXT("analog axes and mouse buttons cannot be bound"); return false; }
	if (NewKey.IsGamepadKey())
	{
		if (!Slot->DefaultPad.IsValid()) { OutError = TEXT("this control is analog (stick / trigger) and has no button binding"); return false; }
		if (NewKey == Slot->PadKey) { return true; }
		if (NewKey == EKeys::Gamepad_Special_Right || NewKey == EKeys::Gamepad_DPad_Up || NewKey == EKeys::Gamepad_DPad_Down || NewKey == EKeys::Gamepad_DPad_Left || NewKey == EKeys::Gamepad_DPad_Right) { OutError = TEXT("that button is reserved for the menus"); return false; }
		for (const FBindSlot& O : Slots) { if (&O != Slot && O.PadKey == NewKey) { OutError = FString::Printf(TEXT("%s is already used by %s"), *NewKey.GetDisplayName().ToString(), *O.Label); return false; } }
		IMC_APEX_Driving->UnmapKey(Slot->Action, Slot->PadKey);
		IMC_APEX_Driving->MapKey(Slot->Action, NewKey);
		Slot->PadKey = NewKey;
		return true;
	}
	for (const FBindSlot& O : Slots)
	{
		if (&O != Slot && (O.Primary == NewKey || O.Alternate == NewKey)) { OutError = FString::Printf(TEXT("%s is already used by %s"), *NewKey.GetDisplayName().ToString(), *O.Label); return false; }
	}
	IMC_APEX_Driving->UnmapKey(Slot->Action, Slot->Primary);
	FEnhancedActionKeyMapping& M = IMC_APEX_Driving->MapKey(Slot->Action, NewKey);
	if (Slot->bNegate) { M.Modifiers.Add(NewObject<UInputModifierNegate>(this)); }
	Slot->Primary = NewKey;
	return true;
}

void UAPEXInputConfig::ResetBindings()
{
	// two passes so a swap of defaults cannot trip the conflict check
	for (FBindSlot& S : Slots)
	{
		IMC_APEX_Driving->UnmapKey(S.Action, S.Primary); S.Primary = FKey();
		if (S.DefaultPad.IsValid()) { IMC_APEX_Driving->UnmapKey(S.Action, S.PadKey); S.PadKey = FKey(); }
	}
	for (FBindSlot& S : Slots)
	{
		FEnhancedActionKeyMapping& M = IMC_APEX_Driving->MapKey(S.Action, S.DefaultPrimary);
		if (S.bNegate) { M.Modifiers.Add(NewObject<UInputModifierNegate>(this)); }
		S.Primary = S.DefaultPrimary;
		if (S.DefaultPad.IsValid()) { IMC_APEX_Driving->MapKey(S.Action, S.DefaultPad); S.PadKey = S.DefaultPad; }
	}
}

void UAPEXInputConfig::SaveBindings() const
{
	if (!GConfig) { return; }
	for (const FBindSlot& S : Slots)
	{
		GConfig->SetString(TEXT("APEXKeyBindings"), *S.Name.ToString(), *S.Primary.ToString(), GGameUserSettingsIni);
		if (S.DefaultPad.IsValid()) { GConfig->SetString(TEXT("APEXKeyBindings"), *(S.Name.ToString() + TEXT("_Pad")), *S.PadKey.ToString(), GGameUserSettingsIni); }
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UAPEXInputConfig::LoadBindings()
{
	if (!GConfig) { return; }
	TArray<TPair<FName, FKey>> Wanted;
	for (const FBindSlot& S : Slots)
	{
		FString V;
		if (GConfig->GetString(TEXT("APEXKeyBindings"), *S.Name.ToString(), V, GGameUserSettingsIni) && !V.IsEmpty() && FKey(*V) != S.Primary && FKey(*V).IsValid()) { Wanted.Add({ S.Name, FKey(*V) }); }
		if (S.DefaultPad.IsValid() && GConfig->GetString(TEXT("APEXKeyBindings"), *(S.Name.ToString() + TEXT("_Pad")), V, GGameUserSettingsIni) && !V.IsEmpty() && FKey(*V) != S.PadKey && FKey(*V).IsValid()) { Wanted.Add({ S.Name, FKey(*V) }); }
	}
	for (const TPair<FName, FKey>& W : Wanted) { FString E; if (!Rebind(W.Key, W.Value, E)) { UE_LOG(LogTemp, Warning, TEXT("Saved binding %s -> %s ignored: %s"), *W.Key.ToString(), *W.Value.ToString(), *E); } }
}

TArray<FKey> UAPEXInputConfig::KeysFor(const UInputAction* Action) const
{
	TArray<FKey> Out;
	if (!IMC_APEX_Driving) { return Out; }
	for (const FEnhancedActionKeyMapping& M : IMC_APEX_Driving->GetMappings()) { if (M.Action == Action) { Out.Add(M.Key); } }
	return Out;
}

bool UAPEXInputConfig::Validate(TArray<FString>& Problems) const
{
	struct FExpect { const UInputAction* A; const TCHAR* Name; TArray<FKey> Keys; };
	const TArray<FExpect> Want = {
		{ IA_Throttle, TEXT("IA_Throttle"), { EKeys::W, EKeys::Up, EKeys::Gamepad_RightTriggerAxis } },
		{ IA_Brake, TEXT("IA_Brake"), { EKeys::S, EKeys::Down, EKeys::Gamepad_LeftTriggerAxis } },
		{ IA_Steer, TEXT("IA_Steer"), { EKeys::A, EKeys::D, EKeys::Left, EKeys::Right, EKeys::Gamepad_LeftX } },
		{ IA_ShiftUp, TEXT("IA_ShiftUp"), { EKeys::E, EKeys::Gamepad_RightShoulder } },
		{ IA_ShiftDown, TEXT("IA_ShiftDown"), { EKeys::Q, EKeys::Gamepad_LeftShoulder } },
		{ IA_Handbrake, TEXT("IA_Handbrake"), { EKeys::SpaceBar } },
		{ IA_EnergyBoost, TEXT("IA_EnergyBoost"), { EKeys::LeftShift, EKeys::Gamepad_FaceButton_Bottom } },
		{ IA_OvertakeAid, TEXT("IA_OvertakeAid"), { EKeys::F, EKeys::Gamepad_FaceButton_Right } },
		{ IA_CameraSwitch, TEXT("IA_CameraSwitch"), { EKeys::C, EKeys::Gamepad_FaceButton_Top } },
		{ IA_LookBack, TEXT("IA_LookBack"), { EKeys::R, EKeys::Gamepad_FaceButton_Left } },
		{ IA_ResetCar, TEXT("IA_ResetCar"), { EKeys::BackSpace } },
		{ IA_Pause, TEXT("IA_Pause"), { EKeys::Escape, EKeys::Gamepad_Special_Right } },
		{ IA_Confirm, TEXT("IA_Confirm"), { EKeys::Enter } },
		{ IA_Cancel, TEXT("IA_Cancel"), { EKeys::Tab } },
	};
	if (!IMC_APEX_Driving) { Problems.Add(TEXT("IMC_APEX_Driving missing")); return false; }
	for (const FExpect& E : Want)
	{
		if (!E.A) { Problems.Add(FString::Printf(TEXT("%s missing"), E.Name)); continue; }
		const TArray<FKey> Have = KeysFor(E.A);
		for (const FKey& K0 : E.Keys) { const FKey K = KeyNow(K0); if (!Have.Contains(K)) { Problems.Add(FString::Printf(TEXT("%s is not mapped to %s"), E.Name, *K.ToString())); } }
	}
	// A and Left must carry the Negate modifier (-1), D and Right must not.
	for (const FEnhancedActionKeyMapping& M : IMC_APEX_Driving->GetMappings())
	{
		if (M.Action != IA_Steer) { continue; }
		bool bNegate = false;
		for (const UInputModifier* Mod : M.Modifiers) { if (Mod && Mod->IsA<UInputModifierNegate>()) { bNegate = true; } }
		const bool bShouldNegate = (M.Key == KeyNow(EKeys::A) || M.Key == EKeys::Left);
		const bool bShouldPlain = (M.Key == KeyNow(EKeys::D) || M.Key == EKeys::Right);
		if (bShouldNegate && !bNegate) { Problems.Add(FString::Printf(TEXT("steering key %s must be negated"), *M.Key.ToString())); }
		if (bShouldPlain && bNegate) { Problems.Add(FString::Printf(TEXT("steering key %s must not be negated"), *M.Key.ToString())); }
	}
	return Problems.Num() == 0;
}

TArray<FString> UAPEXInputConfig::DescribeControls() const
{
	TArray<FString> Lines;
	auto Add = [&](const TCHAR* Label, const UInputAction* A)
	{
		FString Keys;
		TSet<FString> Seen;
		for (const FKey& K : KeysFor(A)) { const FString N = K.GetDisplayName().ToString(); if (!Seen.Contains(N)) { Seen.Add(N); Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(", ")) + N; } }
		Lines.Add(FString::Printf(TEXT("%-16s %s"), Label, *Keys));
	};
	Add(TEXT("Throttle"), IA_Throttle); Add(TEXT("Brake"), IA_Brake); Add(TEXT("Steer"), IA_Steer); Add(TEXT("Shift up"), IA_ShiftUp); Add(TEXT("Shift down"), IA_ShiftDown);
	Add(TEXT("Handbrake"), IA_Handbrake); Add(TEXT("Energy boost"), IA_EnergyBoost); Add(TEXT("Overtake aid"), IA_OvertakeAid); Add(TEXT("Camera"), IA_CameraSwitch);
	Add(TEXT("Look behind"), IA_LookBack); Add(TEXT("Reset car"), IA_ResetCar); Add(TEXT("Pause"), IA_Pause);
	return Lines;
}
