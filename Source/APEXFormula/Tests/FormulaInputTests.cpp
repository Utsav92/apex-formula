#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Input/FormulaMobileInputComponent.h"
#include "Input/FormulaTouchLayout.h"
#include "UI/FormulaHUD.h"

namespace
{
	constexpr EAutomationTestFlags InputTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	// A typical landscape iPhone, with a notch/home-indicator style safe area.
	const FVector2D Phone(2622.f, 1206.f);
	const FVector4 PhoneSafe(132.f, 40.f, 132.f, 40.f);

	UFormulaMobileInputComponent* MakeInput(EFormulaSteerStyle Style, bool bLeftHanded = false)
	{
		UFormulaMobileInputComponent* C = NewObject<UFormulaMobileInputComponent>(GetTransientPackage());
		C->Style = Style;
		C->bLeftHanded = bLeftHanded;
		C->SetViewport(Phone, PhoneSafe);
		return C;
	}

	FVector At(const FFormulaTouchRect& R) { return FVector(R.Center().X, R.Center().Y, 0.f); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTouchLayoutTest, "APEX.Input.LayoutFitsSafeAreaAndDoesNotOverlap", InputTestFlags)
bool FApexTouchLayoutTest::RunTest(const FString&)
{
	for (const bool bLeftHanded : { false, true })
	{
		for (const float Scale : { 0.6f, 1.f, 1.6f })
		{
			const FFormulaTouchLayout L = FFormulaTouchLayout::Compute(Phone, PhoneSafe, bLeftHanded, Scale);
			const FString Tag = FString::Printf(TEXT("[leftHanded=%d scale=%.1f]"), bLeftHanded, Scale);

			const FFormulaTouchRect* Rects[] = { &L.LeftButton, &L.RightButton, &L.GasButton, &L.BrakeButton, &L.StyleButton };
			for (const FFormulaTouchRect* R : Rects)
			{
				TestTrue(*(Tag + TEXT(" inside viewport")), R->X >= 0.f && R->Y >= 0.f && R->X + R->W <= Phone.X && R->Y + R->H <= Phone.Y);
				// Interactive controls respect the horizontal safe margins and bottom margin.
				TestTrue(*(Tag + TEXT(" inside side safe margins")), R->X >= PhoneSafe.X - 0.5f && R->X + R->W <= Phone.X - PhoneSafe.Z + 0.5f);
			}
			for (int32 i = 0; i < UE_ARRAY_COUNT(Rects); ++i)
			{
				for (int32 j = i + 1; j < UE_ARRAY_COUNT(Rects); ++j)
				{
					TestFalse(*(Tag + FString::Printf(TEXT(" rects %d and %d do not overlap"), i, j)), Rects[i]->Overlaps(*Rects[j]));
				}
			}
			TestTrue(*(Tag + TEXT(" wheel inside viewport")),
				L.WheelCenter.X - L.WheelRadius >= 0.f && L.WheelCenter.X + L.WheelRadius <= Phone.X &&
				L.WheelCenter.Y - L.WheelRadius >= 0.f && L.WheelCenter.Y + L.WheelRadius <= Phone.Y);

			// Left button sits to the left of the right button, in both handednesses.
			TestTrue(*(Tag + TEXT(" left of right")), L.LeftButton.X < L.RightButton.X);
		}
	}

	const FFormulaTouchLayout R = FFormulaTouchLayout::Compute(Phone, PhoneSafe, false, 1.f);
	const FFormulaTouchLayout Lh = FFormulaTouchLayout::Compute(Phone, PhoneSafe, true, 1.f);
	TestTrue(TEXT("right-handed: pedals on the right half"), R.GasButton.Center().X > Phone.X * 0.5f);
	TestTrue(TEXT("left-handed: pedals on the left half"), Lh.GasButton.Center().X < Phone.X * 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexShapeAxisTest, "APEX.Input.DeadzoneAndSensitivity", InputTestFlags)
bool FApexShapeAxisTest::RunTest(const FString&)
{
	TestEqual(TEXT("inside deadzone is zero"), UFormulaMobileInputComponent::ShapeAxis(0.04f, 0.05f, 1.f), 0.f);
	TestEqual(TEXT("full travel is full lock"), UFormulaMobileInputComponent::ShapeAxis(1.f, 0.05f, 1.f), 1.f);
	TestEqual(TEXT("sign is preserved"), UFormulaMobileInputComponent::ShapeAxis(-1.f, 0.05f, 1.f), -1.f);
	TestTrue(TEXT("just past deadzone is small, not a jump"), UFormulaMobileInputComponent::ShapeAxis(0.06f, 0.05f, 1.f) < 0.05f);
	TestTrue(TEXT("higher sensitivity steers more"),
		UFormulaMobileInputComponent::ShapeAxis(0.3f, 0.05f, 2.f) > UFormulaMobileInputComponent::ShapeAxis(0.3f, 0.05f, 1.f));
	TestEqual(TEXT("output clamps at 1"), UFormulaMobileInputComponent::ShapeAxis(0.9f, 0.05f, 3.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexButtonsTest, "APEX.Input.ButtonsDriveThrottleBrakeSteer", InputTestFlags)
bool FApexButtonsTest::RunTest(const FString&)
{
	UFormulaMobileInputComponent* C = MakeInput(EFormulaSteerStyle::Buttons);
	const FFormulaTouchLayout& L = C->GetLayout();

	C->UpdateInputs(1.f / 60.f, 0.f);
	TestEqual(TEXT("idle throttle"), C->GetThrottle(), 0.f);
	TestEqual(TEXT("idle steer"), C->GetSteer(), 0.f);

	C->TouchPressed(ETouchIndex::Touch1, At(L.GasButton));
	C->TouchPressed(ETouchIndex::Touch2, At(L.RightButton)); // two fingers at once: gas + steer right
	C->UpdateInputs(0.1f, 0.f);
	TestEqual(TEXT("gas held"), C->GetThrottle(), 1.f);
	TestTrue(TEXT("steering right ramps up"), C->GetSteer() > 0.f && C->GetSteer() < 1.f);
	for (int32 i = 0; i < 60; ++i) { C->UpdateInputs(1.f / 60.f, 0.f); }
	TestEqual(TEXT("steer reaches full lock"), C->GetSteer(), 1.f);

	C->TouchReleased(ETouchIndex::Touch2, At(L.RightButton));
	for (int32 i = 0; i < 60; ++i) { C->UpdateInputs(1.f / 60.f, 0.f); }
	TestEqual(TEXT("steer recentres on release"), C->GetSteer(), 0.f);

	C->TouchReleased(ETouchIndex::Touch1, At(L.GasButton));
	C->TouchPressed(ETouchIndex::Touch1, At(L.BrakeButton));
	C->UpdateInputs(1.f / 60.f, 0.f);
	TestEqual(TEXT("brake held"), C->GetBrake(), 1.f);
	TestEqual(TEXT("throttle released"), C->GetThrottle(), 0.f);

	// Sliding off every control releases it.
	C->TouchMoved(ETouchIndex::Touch1, FVector(5.f, 5.f, 0.f));
	C->UpdateInputs(1.f / 60.f, 0.f);
	TestEqual(TEXT("sliding off releases brake"), C->GetBrake(), 0.f);
	TestFalse(TEXT("no active touches"), C->HasActiveTouch());

	// A press that misses every control is ignored.
	C->TouchPressed(ETouchIndex::Touch3, FVector(Phone.X * 0.5f, Phone.Y * 0.5f, 0.f));
	TestFalse(TEXT("empty-space press is not tracked"), C->HasActiveTouch());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexWheelTest, "APEX.Input.VirtualWheelSteersByRotation", InputTestFlags)
bool FApexWheelTest::RunTest(const FString&)
{
	UFormulaMobileInputComponent* C = MakeInput(EFormulaSteerStyle::Wheel);
	const FFormulaTouchLayout& L = C->GetLayout();
	const FVector2D Ctr = L.WheelCenter;
	const float R = L.WheelRadius * 0.8f;

	// Grab the wheel at the top, drag clockwise by 90 degrees to the right-hand side.
	C->TouchPressed(ETouchIndex::Touch1, FVector(Ctr.X, Ctr.Y - R, 0.f));
	TestTrue(TEXT("wheel grabbed"), C->IsWheelHeld());
	C->TouchMoved(ETouchIndex::Touch1, FVector(Ctr.X + R, Ctr.Y, 0.f));
	for (int32 i = 0; i < 30; ++i) { C->UpdateInputs(1.f / 60.f, 0.f); }
	TestTrue(TEXT("90 degrees clockwise is full right lock"), C->GetSteer() > 0.95f);

	// Counter-clockwise past centre steers left.
	C->TouchMoved(ETouchIndex::Touch1, FVector(Ctr.X - R, Ctr.Y, 0.f));
	for (int32 i = 0; i < 60; ++i) { C->UpdateInputs(1.f / 60.f, 0.f); }
	TestTrue(TEXT("90 degrees anticlockwise is full left lock"), C->GetSteer() < -0.95f);

	C->TouchReleased(ETouchIndex::Touch1, FVector::ZeroVector);
	for (int32 i = 0; i < 60; ++i) { C->UpdateInputs(1.f / 60.f, 0.f); }
	TestTrue(TEXT("releasing the wheel recentres"), FMath::Abs(C->GetSteer()) < 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexTiltAndStyleTest, "APEX.Input.TiltSteersAndStyleButtonCycles", InputTestFlags)
bool FApexTiltAndStyleTest::RunTest(const FString&)
{
	UFormulaMobileInputComponent* C = MakeInput(EFormulaSteerStyle::Tilt);
	for (int32 i = 0; i < 30; ++i) { C->UpdateInputs(1.f / 60.f, 0.6f); }
	TestTrue(TEXT("tilt right steers right"), C->GetSteer() > 0.4f);
	for (int32 i = 0; i < 30; ++i) { C->UpdateInputs(1.f / 60.f, 0.02f); }
	TestEqual(TEXT("tiny tilt is inside the deadzone"), C->GetSteer(), 0.f);

	// Buttons do nothing under Tilt style (steer comes only from the sensor).
	C->TouchPressed(ETouchIndex::Touch1, At(C->GetLayout().RightButton));
	TestFalse(TEXT("steer buttons inactive in tilt style"), C->IsRightHeld());

	// Tapping the style button cycles in enum order: Buttons -> Tilt -> Wheel -> Buttons.
	const FVector StyleBtn = At(C->GetLayout().StyleButton);
	C->TouchPressed(ETouchIndex::Touch1, StyleBtn);
	TestTrue(TEXT("tilt -> wheel"), C->Style == EFormulaSteerStyle::Wheel);
	C->TouchPressed(ETouchIndex::Touch1, StyleBtn);
	TestTrue(TEXT("wheel -> buttons"), C->Style == EFormulaSteerStyle::Buttons);
	C->TouchPressed(ETouchIndex::Touch1, StyleBtn);
	TestTrue(TEXT("buttons -> tilt"), C->Style == EFormulaSteerStyle::Tilt);
	TestFalse(TEXT("style tap is not a held control"), C->HasActiveTouch());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApexHudFormatTest, "APEX.UI.LapTimeFormatting", InputTestFlags)
bool FApexHudFormatTest::RunTest(const FString&)
{
	TestEqual(TEXT("zero"), AFormulaHUD::FormatLapTime(0.f), FString(TEXT("0:00.00")));
	TestEqual(TEXT("35.9 s"), AFormulaHUD::FormatLapTime(35.9f), FString(TEXT("0:35.90")));
	TestEqual(TEXT("1 min 5.25"), AFormulaHUD::FormatLapTime(65.25f), FString(TEXT("1:05.25")));
	TestEqual(TEXT("negative clamps to zero"), AFormulaHUD::FormatLapTime(-3.f), FString(TEXT("0:00.00")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
