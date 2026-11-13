#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "APEXInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Builds the driving Input Actions (IA_Throttle ... IA_Cancel) and the mapping context IMC_APEX_Driving in code, so the controls exist (and are
 * testable) without hand-authored binary assets. Keyboard: W/Up throttle, S/Down brake, A/D or Left/Right steer, E/Q shift, Space handbrake,
 * Left Shift boost, F overtake aid, C camera, R look back, Backspace reset, Escape pause, Enter confirm, Tab cancel.
 * Gamepad: left stick steer, RT throttle, LT brake, bumpers shift, A boost, B overtake aid, Y camera, X look back, Menu pause, View reset.
 */
UCLASS()
class APEXFORMULA_API UAPEXInputConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY() TObjectPtr<UInputAction> IA_Throttle;
	UPROPERTY() TObjectPtr<UInputAction> IA_Brake;
	UPROPERTY() TObjectPtr<UInputAction> IA_Steer;
	UPROPERTY() TObjectPtr<UInputAction> IA_ShiftUp;
	UPROPERTY() TObjectPtr<UInputAction> IA_ShiftDown;
	UPROPERTY() TObjectPtr<UInputAction> IA_Handbrake;
	UPROPERTY() TObjectPtr<UInputAction> IA_EnergyBoost;
	UPROPERTY() TObjectPtr<UInputAction> IA_OvertakeAid;
	UPROPERTY() TObjectPtr<UInputAction> IA_CameraSwitch;
	UPROPERTY() TObjectPtr<UInputAction> IA_LookBack;
	UPROPERTY() TObjectPtr<UInputAction> IA_ResetCar;
	UPROPERTY() TObjectPtr<UInputAction> IA_Pause;
	UPROPERTY() TObjectPtr<UInputAction> IA_Confirm;
	UPROPERTY() TObjectPtr<UInputAction> IA_Cancel;
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_APEX_Driving;

	/** Creates every action and the context. Safe to call once per config object. */
	void Build();

	/** Keys mapped to an action, e.g. for the Controls page and the tests. */
	TArray<FKey> KeysFor(const UInputAction* Action) const;
	/** Appends one line per missing or wrong mapping; true when the context matches the specification. */
	bool Validate(TArray<FString>& Problems) const;

	/** One remappable keyboard binding: the primary key can be changed, the alternate (arrow keys) stays. */
	struct FBindSlot
	{
		FName Name; FString Label; const UInputAction* Action = nullptr; bool bNegate = false;
		FKey DefaultPrimary, Primary, Alternate;
		FKey DefaultPad, PadKey;       // gamepad button (invalid for the analog controls: throttle, brake, steering)
	};
	const TArray<FBindSlot>& GetSlots() const { return Slots; }
	TArray<FBindSlot> Slots;
	/** Rebind a slot: a keyboard key replaces the primary key, a gamepad button replaces the pad button (analog controls have none). Rejects reserved keys (Escape, Enter, Tab, Menu button, D-pad), mouse keys, axes and keys already used elsewhere. */
	bool Rebind(FName SlotName, const FKey& NewKey, FString& OutError);
	void ResetBindings();
	/** Persist the primary keys to GameUserSettings.ini (section APEXKeyBindings); LoadBindings applies them over the defaults. */
	void SaveBindings() const;
	void LoadBindings();
	/** The key now standing in for a default key (so Validate follows rebinding). */
	FKey KeyNow(const FKey& DefaultKey) const;

	/** Controls page text: "Throttle: W, Up, Gamepad Right Trigger" ... */
	TArray<FString> DescribeControls() const;
};
