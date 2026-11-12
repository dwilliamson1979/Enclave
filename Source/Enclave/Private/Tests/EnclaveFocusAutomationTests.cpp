// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for the Focus (ADS) transition.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Equipment/Focus/EnclaveFocusBlend.h"
#include "Equipment/Focus/EnclaveFocusProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveFocusBlendTest,
	"Enclave.Focus.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveFocusBlendTest::RunTest(const FString& Parameters)
{
	FEnclaveFocusViewSettings Linear;

	// Linear: weight is progress, both ways.
	TestEqual(TEXT("Linear entering half way"), EnclaveFocusBlend::Weight(0.5f, true, Linear), 0.5f, 1e-4f);
	TestEqual(TEXT("Linear leaving half way"), EnclaveFocusBlend::Weight(0.5f, false, Linear), 0.5f, 1e-4f);

	FEnclaveFocusViewSettings EaseOut;
	EaseOut.TransitionEasing = EEasingFunc::EaseOut;
	EaseOut.TransitionEasingExponent = 2.0f;

	// Both directions agree at the ends.
	for (const bool bEntering : { true, false })
	{
		TestEqual(TEXT("Weight 0 at hip"), EnclaveFocusBlend::Weight(0.0f, bEntering, EaseOut), 0.0f, 1e-4f);
		TestEqual(TEXT("Weight 1 when aimed"), EnclaveFocusBlend::Weight(1.0f, bEntering, EaseOut), 1.0f, 1e-4f);
	}

	// EaseOut starts fast in both directions: a quarter of the time in covers
	// more than a quarter of the way, and a quarter of the time out does too.
	TestTrue(TEXT("EaseOut enters fast"), EnclaveFocusBlend::Weight(0.25f, true, EaseOut) > 0.25f);
	TestTrue(TEXT("EaseOut leaves fast"), 1.0f - EnclaveFocusBlend::Weight(0.75f, false, EaseOut) > 0.25f);

	// A reversal re-solves progress so the weight (the view) doesn't jump.
	for (const float Progress : { 0.1f, 0.3f, 0.5f, 0.8f })
	{
		const float Weight = EnclaveFocusBlend::Weight(Progress, true, EaseOut);
		const float Reversed = EnclaveFocusBlend::ProgressForWeight(Weight, false, EaseOut);
		TestEqual(TEXT("Reversal keeps the weight"), EnclaveFocusBlend::Weight(Reversed, false, EaseOut), Weight, 1e-4f);
		TestEqual(TEXT("Round trip in the same direction"), EnclaveFocusBlend::ProgressForWeight(Weight, true, EaseOut), Progress, 1e-4f);
	}

	// Linear reversal at half way leaves half the way, so it takes half the time.
	TestEqual(TEXT("Linear reversal half way"), EnclaveFocusBlend::ProgressForWeight(0.5f, false, Linear), 0.5f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveFocusLookScaleTest,
	"Enclave.Focus.LookScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveFocusLookScaleTest::RunTest(const FString& Parameters)
{
	// Hip sensitivity is untouched at any hip FOV.
	TestEqual(TEXT("1 at the hip FOV (90)"), EnclaveFocusBlend::LookScaleForFOV(90.0f, 90.0f), 1.0f, 1e-4f);
	TestEqual(TEXT("1 at the hip FOV (103)"), EnclaveFocusBlend::LookScaleForFOV(103.0f, 103.0f), 1.0f, 1e-4f);

	// 90 to 65: tan(32.5) / tan(45), about 0.637.
	TestEqual(TEXT("90 to 65"), EnclaveFocusBlend::LookScaleForFOV(65.0f, 90.0f), 0.6371f, 1e-3f);

	// Narrower is slower.
	TestTrue(TEXT("A 4x-style zoom is slower than an iron sight"),
		EnclaveFocusBlend::LookScaleForFOV(25.0f, 90.0f) < EnclaveFocusBlend::LookScaleForFOV(65.0f, 90.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
