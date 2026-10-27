// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Combat/EnclaveShotTiming.h"
#include "Combat/EnclaveSSRPolicyContracts.h"

class AActor;
class AEnclaveCharacter;
class UWorld;

/** Read-only D11D-3A assessment. Not an FHitResult and NEVER a damage authority. */
struct ENCLAVE_API FEnclaveServerHistoricalAssessment
{
    FEnclaveSSRQueryAssessment Policy;
    TWeakObjectPtr<AEnclaveCharacter> Target;
    FVector Point = FVector::ZeroVector;
    double EntryDistanceCm = -1.0;
    double EvaluatedServerTimeSeconds = 0.0;
    int32 AvailableHistoryCount = 0;
    int32 MissingHistoryCount = 0;
};

/**
 * D11D-3A server-side historical capsule evaluator, with NO gameplay call site.
 * A future trusted server timing authority must select AuthorizedHistoryTimeSeconds.
 * Neither client shot timestamps, client-selected targets nor RTT estimates can
 * be passed through this interface as a substitute for such an authority.
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
        double AuthorizedHistoryTimeSeconds);
};
