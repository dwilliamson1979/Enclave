// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Combat/EnclaveSSRPolicyContracts.h"

/**
 * Diagnostic-only classification of a bounded set of candidate historical queries
 * (the timing sweep). Authoritative timing is ResolveSSRTiming(); this type never
 * selects damage. Unavailable samples invalidate stability.
 */
namespace EnclaveSSRTemporalUncertainty
{
    enum class EClassification : uint8
    {
        NoSamples,
        Unavailable,
        StableHit,
        StableMiss,
        ConflictingResults
    };

    struct FAssessment
    {
        int32 Samples = 0;
        int32 Hits = 0;
        int32 Misses = 0;
        int32 Unavailable = 0;
        EClassification Classification = EClassification::NoSamples;

        void Add(EEnclaveSSRQueryOutcome Outcome)
        {
            ++Samples;
            switch (Outcome)
            {
            case EEnclaveSSRQueryOutcome::HistoricalHit: ++Hits; break;
            case EEnclaveSSRQueryOutcome::HistoricalMiss: ++Misses; break;
            default: ++Unavailable; break;
            }
            if (Unavailable > 0)
            {
                Classification = EClassification::Unavailable;
            }
            else if (Hits && Misses)
            {
                Classification = EClassification::ConflictingResults;
            }
            else
            {
                Classification = Hits ? EClassification::StableHit : EClassification::StableMiss;
            }
        }
    };
}
