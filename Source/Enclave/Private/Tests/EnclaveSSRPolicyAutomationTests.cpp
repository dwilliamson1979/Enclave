// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for production SSR primitives.
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/EnclaveSSRPolicyContracts.h"
#include "Combat/EnclaveShotTiming.h"
#include "Combat/EnclaveSSRTimingPolicy.h"
#include "Combat/EnclaveSSRTimingCalibration.h"
#include "Combat/EnclaveSSRHistoricalEligibility.h"
#include "Combat/EnclaveHistoricalCapsuleIntersection.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRPolicyClassificationTest,
    "Enclave.Combat.SSR.PolicyClassification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRPolicyClassificationTest::RunTest(const FString& Parameters)
{
    FEnclaveSSRQueryAssessment Result;
    TestFalse(TEXT("Default assessment is not a hit or miss"), Result.HasDefinitiveHistoricalResult());
    Result.Outcome = EEnclaveSSRQueryOutcome::HistoricalHit;
    TestTrue(TEXT("Historical hit is definitive"), Result.HasDefinitiveHistoricalResult());
    Result.Outcome = EEnclaveSSRQueryOutcome::HistoricalMiss;
    Result.bWorldOccluded = true;
    TestTrue(TEXT("Occluded historical miss is definitive"), Result.HasDefinitiveHistoricalResult());
    Result.Outcome = EEnclaveSSRQueryOutcome::Unavailable;
    Result.UnavailableReason = EEnclaveSSRUnavailableReason::HistoryMissing;
    TestFalse(TEXT("Missing history is not a miss"), Result.HasDefinitiveHistoricalResult());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRServerAnchoredTimingTest,
    "Enclave.Combat.SSR.ServerAnchoredTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRServerAnchoredTimingTest::RunTest(const FString& Parameters)
{
    FEnclaveShotTimingContext Timing;
    Timing.ServerReceiptTimeSeconds = 100.0;
    Timing.ServerObservedRTTSeconds = 0.150;
    Timing.MaxRewindSeconds = 0.200;
    double Time = 0.0;
    TestTrue(TEXT("Negative offset rewinds less than RTT"), Timing.TryResolveServerAnchoredHistoryTime(-0.050, Time));
    TestEqual(TEXT("Age is RTT plus offset"), Time, 99.9, 1e-9);
    TestTrue(TEXT("Zero offset is the RTT anchor"), Timing.TryResolveServerAnchoredHistoryTime(0.0, Time));
    TestEqual(TEXT("RTT anchor time"), Time, 99.85, 1e-9);
    TestFalse(TEXT("Age beyond rewind window fails, no clamp"), Timing.TryResolveServerAnchoredHistoryTime(0.051, Time));
    TestFalse(TEXT("Negative age (future) fails"), Timing.TryResolveServerAnchoredHistoryTime(-0.151, Time));
    Timing.ServerObservedRTTSeconds = -1.0;
    TestFalse(TEXT("Missing RTT fails"), Timing.TryResolveServerAnchoredHistoryTime(-0.050, Time));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRStampQuantizationTest,
    "Enclave.Combat.SSR.StampQuantization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRStampQuantizationTest::RunTest(const FString& Parameters)
{
    using namespace EnclaveSSRTimingPolicy;
    TestEqual(TEXT("Rounds to nearest ms"), QuantizeServerTimeMs(12.3456), 12346u);
    TestEqual(TEXT("Round trip within half a ms"), DequantizeServerTimeMs(QuantizeServerTimeMs(98.7654)), 98.765, 1e-9);
    TestEqual(TEXT("Zero time is invalid"), QuantizeServerTimeMs(0.0), 0u);
    TestEqual(TEXT("Negative time is invalid"), QuantizeServerTimeMs(-1.0), 0u);
    TestEqual(TEXT("NaN is invalid"), QuantizeServerTimeMs(std::numeric_limits<double>::quiet_NaN()), 0u);
    TestEqual(TEXT("Beyond uint32 ms is invalid"), QuantizeServerTimeMs(5.0e6), 0u);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRTimingResolutionTest,
    "Enclave.Combat.SSR.TimingResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRTimingResolutionTest::RunTest(const FString& Parameters)
{
    FEnclaveShotTimingContext Timing;
    Timing.ServerReceiptTimeSeconds = 100.0;
    Timing.ServerObservedRTTSeconds = 0.150;
    Timing.MaxRewindSeconds = 0.200;
    // Explicit values: the test checks the policy, not the tunable defaults.
    FEnclaveSSRTimingSettings Settings;
    Settings.AnchorOffsetSeconds = -0.050;
    Settings.StampEarlyToleranceSeconds = 0.040;
    Settings.StampLateToleranceSeconds = 0.060;
    const double Anchor = 100.0 - 0.100;

    // Server anchor ignores client evidence entirely.
    Settings.Source = EEnclaveSSRTimingSource::ServerAnchor;
    FEnclaveSSRTimingResolution R = ResolveSSRTiming(Settings, Timing, true, 99.88);
    TestTrue(TEXT("Anchor valid"), R.bValid);
    TestEqual(TEXT("Anchor time is receipt - (RTT + offset)"), R.BaseTimeSeconds, Anchor, 1e-9);
    TestFalse(TEXT("Anchor never uses a stamp"), R.bUseStamp);

    // Hybrid: stamp inside the window is used for its target only.
    Settings.Source = EEnclaveSSRTimingSource::Hybrid;
    R = ResolveSSRTiming(Settings, Timing, true, 99.88); // age 120 ms, anchor age 100 ms: +20 ms
    TestTrue(TEXT("Hybrid valid"), R.bValid);
    TestEqual(TEXT("Hybrid base stays at anchor"), R.BaseTimeSeconds, Anchor, 1e-9);
    TestTrue(TEXT("Hybrid uses in-window stamp"), R.bUseStamp);
    TestEqual(TEXT("Hybrid stamp time"), R.StampTimeSeconds, 99.88, 1e-9);

    R = ResolveSSRTiming(Settings, Timing, true, 99.83); // age 170 ms: +70 ms, beyond +60
    TestTrue(TEXT("Too-old stamp still resolves via anchor"), R.bValid && !R.bUseStamp);
    TestTrue(TEXT("Too-old stamp flagged"), R.bStampRejected);
    R = ResolveSSRTiming(Settings, Timing, true, 99.95); // age 50 ms: -50 ms, beyond -40
    TestTrue(TEXT("Too-new stamp falls back to anchor"), R.bValid && !R.bUseStamp && R.bStampRejected);
    R = ResolveSSRTiming(Settings, Timing, false, 0.0);
    TestTrue(TEXT("No stamp: anchor only, not flagged"), R.bValid && !R.bUseStamp && !R.bStampRejected);

    // Echoed stamp only: no verifiable stamp means unavailable, never the anchor.
    Settings.Source = EEnclaveSSRTimingSource::EchoedStamp;
    R = ResolveSSRTiming(Settings, Timing, true, 99.83);
    TestTrue(TEXT("Echo uses any in-window stamp"), R.bValid && R.bUseStamp);
    TestEqual(TEXT("Echo base is the stamp"), R.BaseTimeSeconds, 99.83, 1e-9);
    R = ResolveSSRTiming(Settings, Timing, false, 0.0);
    TestFalse(TEXT("Echo without stamp is unavailable"), R.bValid);
    TestEqual(TEXT("Echo without stamp reason"), R.UnavailableReason, EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
    R = ResolveSSRTiming(Settings, Timing, true, 99.7);
    TestFalse(TEXT("Echo stamp outside rewind window"), R.bValid);
    TestEqual(TEXT("Echo outside window reason"), R.UnavailableReason, EEnclaveSSRUnavailableReason::RewindWindowExceeded);
    R = ResolveSSRTiming(Settings, Timing, true, 100.01);
    TestFalse(TEXT("Echo stamp from the future"), R.bValid);

    // Anchor beyond the rewind window is unavailable, not clamped.
    Settings.Source = EEnclaveSSRTimingSource::Hybrid;
    Timing.ServerObservedRTTSeconds = 0.300;
    R = ResolveSSRTiming(Settings, Timing, true, 99.85);
    TestFalse(TEXT("High ping anchor unavailable"), R.bValid);
    TestEqual(TEXT("High ping reason"), R.UnavailableReason, EEnclaveSSRUnavailableReason::RewindWindowExceeded);
    Timing.ServerObservedRTTSeconds = -1.0;
    R = ResolveSSRTiming(Settings, Timing, false, 0.0);
    TestEqual(TEXT("Missing RTT reason"), R.UnavailableReason, EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRCapsuleSurfaceNormalTest,
    "Enclave.Combat.SSR.CapsuleSurfaceNormal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRCapsuleSurfaceNormalTest::RunTest(const FString& Parameters)
{
    FEnclaveTargetCapsuleFrame Capsule;
    Capsule.Center = FVector(100., 0., 0.);
    Capsule.RadiusCm = 30.f;
    Capsule.HalfHeightCm = 90.f;
    const FVector Fallback(-1., 0., 0.);
    TestTrue(TEXT("Side hit faces outward"), FEnclaveHistoricalCapsuleIntersection::SurfaceNormal(
        Capsule, FVector(70., 0., 20.), Fallback).Equals(FVector(-1., 0., 0.), 1e-6));
    TestTrue(TEXT("Top cap faces up"), FEnclaveHistoricalCapsuleIntersection::SurfaceNormal(
        Capsule, FVector(100., 0., 90.), Fallback).Equals(FVector(0., 0., 1.), 1e-6));
    TestTrue(TEXT("On-axis point uses fallback"), FEnclaveHistoricalCapsuleIntersection::SurfaceNormal(
        Capsule, FVector(100., 0., 10.), Fallback).Equals(Fallback, 1e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRHistoricalCapsuleGeometryTest,
    "Enclave.Combat.SSR.HistoricalCapsuleGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRHistoricalCapsuleGeometryTest::RunTest(const FString& Parameters)
{
    FEnclaveTargetCapsuleFrame Capsule;
    Capsule.Center = FVector(100., 0., 0.);
    Capsule.Rotation = FQuat::Identity;
    Capsule.RadiusCm = 30.f;
    Capsule.HalfHeightCm = 80.f;
    double Distance = -1.;
    FVector Point = FVector::ZeroVector;
    TestTrue(TEXT("Central ray intersects historical capsule"),
        FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
            FVector::ZeroVector, FVector(200., 0., 0.), Capsule, Distance, Point));
    TestTrue(TEXT("First entry is 70 cm"), FMath::IsNearlyEqual(Distance, 70.0, 0.01));
    TestFalse(TEXT("Offset ray misses historical capsule"),
        FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
            FVector(0., 60., 0.), FVector(200., 60., 0.), Capsule, Distance, Point));
    Capsule.RadiusCm = 0.f;
    TestFalse(TEXT("Invalid capsule fails closed"),
        FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
            FVector::ZeroVector, FVector(200., 0., 0.), Capsule, Distance, Point));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRTimingCalibrationTest,
    "Enclave.Combat.SSR.TimingCalibration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRTimingCalibrationTest::RunTest(const FString& Parameters)
{
    using namespace EnclaveSSRTimingCalibration;
    TestEqual(TEXT("Sweep starts at zero extra delay"), DelayMs(0, 25.0), 0.0);
    TestEqual(TEXT("Fifth sample at 100ms"), DelayMs(4, 25.0), 100.0);
    TestTrue(TEXT("Negative index rejected"), DelayMs(-1, 25.0) < 0.0);
    TestTrue(TEXT("Out-of-range index rejected"), DelayMs(5, 25.0) < 0.0);
    TestTrue(TEXT("Invalid step rejected"), DelayMs(1, -1.0) < 0.0);

    FSummary Summary;
    Summary.Add(0, EEnclaveSSRQueryOutcome::HistoricalMiss);
    Summary.Add(1, EEnclaveSSRQueryOutcome::HistoricalHit);
    Summary.Add(2, EEnclaveSSRQueryOutcome::Unavailable);
    Summary.Add(3, EEnclaveSSRQueryOutcome::HistoricalMiss);
    Summary.Add(4, EEnclaveSSRQueryOutcome::HistoricalHit);
    TestEqual(TEXT("Two hits"), Summary.Hits, 2);
    TestEqual(TEXT("Two misses"), Summary.Misses, 2);
    TestEqual(TEXT("One unavailable"), Summary.Unavailable, 1);
    TestEqual(TEXT("Unavailable interrupts transitions"), Summary.Transitions, 2);
    TestEqual(TEXT("First transition at sample one"), Summary.FirstTransitionIndex, 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRHistoricalEligibilityTest,
    "Enclave.Combat.SSR.HistoricalEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRHistoricalEligibilityTest::RunTest(const FString& Parameters)
{
    using E = EEnclaveSSRHistoricalEligibility;
    TestEqual(TEXT("Living target eligible"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(100.0, false, -1.0), (int32)E::Eligible);
    TestEqual(TEXT("Trade kill: dead now but alive at query"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(99.97, true, 100.0), (int32)E::Eligible);
    TestEqual(TEXT("Dead exactly at boundary is excluded"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(100.0, true, 100.0), (int32)E::ExcludedDeadAtQuery);
    TestEqual(TEXT("Corpse excluded after death"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(100.05, true, 100.0), (int32)E::ExcludedDeadAtQuery);
    TestEqual(TEXT("Dead now with no server death stamp fails closed"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(100.05, true, -1.0), (int32)E::MissingDeathBoundary);
    TestEqual(TEXT("Invalid non-finite query time rejected"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(std::numeric_limits<double>::quiet_NaN(), true, 100.0), (int32)E::InvalidQueryTime);
    TestEqual(TEXT("Prior-life death boundary still excludes old corpse"),
        (int32)FEnclaveSSRHistoricalEligibility::Evaluate(100.1, false, 100.0), (int32)E::ExcludedDeadAtQuery);
    return true;
}

#endif

