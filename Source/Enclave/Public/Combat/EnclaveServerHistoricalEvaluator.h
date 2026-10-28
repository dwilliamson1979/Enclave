// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Combat/EnclaveShotTiming.h"
#include "Combat/EnclaveSSRPolicyContracts.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"

class AActor;
class AEnclaveCharacter;
class UWorld;

/** Read-only historical assessment. Not an FHitResult: D11D-4 selection converts
 * a HistoricalHit into one; shadow diagnostics only log it. */
struct ENCLAVE_API FEnclaveServerHistoricalAssessment
{
    FEnclaveSSRQueryAssessment Policy;
    TWeakObjectPtr<AEnclaveCharacter> Target;
    FEnclaveTargetCapsuleFrame Capsule;
    FVector Point = FVector::ZeroVector;
    double EntryDistanceCm = -1.0;
    double EvaluatedServerTimeSeconds = 0.0;
    int32 AvailableHistoryCount = 0;
    int32 MissingHistoryCount = 0;
    /** Present-time world (non-character) blocking hit along the shot, if any. */
    FHitResult WorldHit;
    bool bWorldBlocked = false;
};

/**
 * Server-side historical capsule evaluator. AuthorizedHistoryTimeSeconds (and the
 * optional per-target override) must come from ResolveSSRTiming(); this class
 * re-checks both against the rewind window but does not choose times itself.
 *
 * The full eligible character population is searched (no target argument).
 * Missing history makes a miss ambiguous, so assessments fail closed.
 * World obstruction is present-time static-world approximation only.
 */
class ENCLAVE_API FEnclaveServerHistoricalEvaluator
{
public:
    static FEnclaveServerHistoricalAssessment EvaluateCapsules(
        UWorld* ServerWorld, AActor* Shooter, const FVector& Start, const FVector& End,
        const FEnclaveShotTimingContext& ValidatedTiming,
        double AuthorizedHistoryTimeSeconds,
        const AEnclaveCharacter* OverrideTarget = nullptr, double OverrideTimeSeconds = 0.0);
};
