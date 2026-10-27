// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for production SSR primitives.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/EnclaveSSRPolicyContracts.h"
#include "Combat/EnclaveShotTiming.h"
#include "Combat/EnclaveSSRTimingCalibration.h"
#include "Combat/EnclaveSSRTemporalUncertainty.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRBoundedTimingTest,
    "Enclave.Combat.SSR.BoundedTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRBoundedTimingTest::RunTest(const FString& Parameters)
{
    FEnclaveShotTimingContext Timing;
    Timing.ServerReceiptTimeSeconds = 100.0;
    Timing.ClientEstimatedShotTimeSeconds = 99.95;
    Timing.MaxRewindSeconds = 0.2;
    Timing.ServerObservedRTTSeconds = 0.10;
    double Resolved = -1.0;
    TestTrue(TEXT("Validated presentation delay resolves candidate"),
        Timing.TryResolveTargetHistoryTime(0.05, Resolved));
    TestTrue(TEXT("Resolved time is 99.90"), FMath::IsNearlyEqual(Resolved, 99.90, 1e-6));
    TestFalse(TEXT("Too-old candidate rejected, not clamped"),
        Timing.TryResolveTargetHistoryTime(0.3, Resolved));
    TestTrue(TEXT("Rejected candidate clears output"), FMath::IsNearlyZero(Resolved));
    TestFalse(TEXT("Negative delay rejected"),
        Timing.TryResolveTargetHistoryTime(-0.1, Resolved));
    TestTrue(TEXT("Diagnostic server anchor resolves under cap"),
        Timing.TryResolveServerDerivedHistoryTime(0.05, Resolved));
    TestFalse(TEXT("Diagnostic server anchor above cap rejected"),
        Timing.TryResolveServerDerivedHistoryTime(0.2, Resolved));
    // Two candidate clocks remain independently bounded. The two are
    // diagnostic alternatives, not evidence of actual client presentation.
    Timing.ClientEstimatedShotTimeSeconds = 99.97;
    Timing.ServerObservedRTTSeconds = 0.08;
    double ShotClock = 0.0;
    double ServerAnchor = 0.0;
    TestTrue(TEXT("Client clock candidate bounded"),
        Timing.TryResolveTargetHistoryTime(0.02, ShotClock));
    TestTrue(TEXT("Server RTT anchor bounded"),
        Timing.TryResolveServerDerivedHistoryTime(0.02, ServerAnchor));
    TestTrue(TEXT("Distinct candidate clocks remain distinct"),
        FMath::IsNearlyEqual(ShotClock - ServerAnchor, 0.05, 1e-6));
    TestFalse(TEXT("Server anchor rejects negative smoothing"),
        Timing.TryResolveServerDerivedHistoryTime(-0.01, ServerAnchor));
    TestFalse(TEXT("Client candidate rejects beyond rewind allowance"),
        Timing.TryResolveTargetHistoryTime(0.30, ShotClock));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSSRTemporalUncertaintyTest,
    "Enclave.Combat.SSR.TemporalUncertainty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSSRTemporalUncertaintyTest::RunTest(const FString& Parameters)
{
    using namespace EnclaveSSRTemporalUncertainty;
    FAssessment Stable;
    TestEqual(TEXT("No samples"), Stable.Classification, EClassification::NoSamples);
    Stable.Add(EEnclaveSSRQueryOutcome::HistoricalHit);
    Stable.Add(EEnclaveSSRQueryOutcome::HistoricalHit);
    TestEqual(TEXT("All hit samples"), Stable.Classification, EClassification::StableHit);
    TestFalse(TEXT("Stable results are never presentation proof"), Stable.IsAuthorizedForDamage());
    Stable.Add(EEnclaveSSRQueryOutcome::HistoricalMiss);
    TestEqual(TEXT("Conflicting samples"), Stable.Classification, EClassification::ConflictingResults);
    FAssessment Missing;
    Missing.Add(EEnclaveSSRQueryOutcome::HistoricalMiss);
    Missing.Add(EEnclaveSSRQueryOutcome::Unavailable);
    TestEqual(TEXT("Missing history is not stable miss"), Missing.Classification, EClassification::Unavailable);
    Missing.Add(EEnclaveSSRQueryOutcome::HistoricalHit);
    TestEqual(TEXT("Missing history retains precedence"), Missing.Classification, EClassification::Unavailable);
    FAssessment AllMiss;
    AllMiss.Add(EEnclaveSSRQueryOutcome::HistoricalMiss);
    AllMiss.Add(EEnclaveSSRQueryOutcome::HistoricalMiss);
    TestEqual(TEXT("Stable misses"), AllMiss.Classification, EClassification::StableMiss);
    TestFalse(TEXT("Stable miss is not authorization"), AllMiss.IsAuthorizedForDamage());
    return true;
}
#endif
