// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for production SSR primitives.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/EnclaveSSRPolicyContracts.h"
#include "Combat/EnclaveShotTiming.h"
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
#endif // WITH_DEV_AUTOMATION_TESTS
