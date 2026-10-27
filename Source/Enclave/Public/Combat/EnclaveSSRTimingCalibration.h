// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Combat/EnclaveSSRPolicyContracts.h"

/** Pure, read-only helpers for D11D-3C shadow sensitivity measurements. */
namespace EnclaveSSRTimingCalibration
{
    constexpr int32 SampleCount = 5;
    constexpr double DefaultStepMs = 25.0;

    // Millisecond offsets from a *diagnostic* server RTT anchor; zero is the
    // anchor and each positive step represents additional presentation delay.
    inline double DelayMs(int32 Index, double StepMs)
    {
        return Index >= 0 && Index < SampleCount && FMath::IsFinite(StepMs) &&
            StepMs >= 0.0 && StepMs <= 100.0 ? Index * StepMs : -1.0;
    }

    inline bool IsDefinitive(EEnclaveSSRQueryOutcome Outcome)
    {
        return Outcome == EEnclaveSSRQueryOutcome::HistoricalHit ||
            Outcome == EEnclaveSSRQueryOutcome::HistoricalMiss;
    }

    /** Invalid/unavailable samples cannot be counted as hits, misses or flips. */
    struct FSummary
    {
        int32 Hits = 0;
        int32 Misses = 0;
        int32 Unavailable = 0;
        int32 Transitions = 0;
        int32 FirstTransitionIndex = -1;
        bool bPreviousDefinitive = false;
        EEnclaveSSRQueryOutcome Previous = EEnclaveSSRQueryOutcome::Unavailable;

        void Add(int32 Index, EEnclaveSSRQueryOutcome Outcome)
        {
            if (Outcome == EEnclaveSSRQueryOutcome::HistoricalHit) { ++Hits; }
            else if (Outcome == EEnclaveSSRQueryOutcome::HistoricalMiss) { ++Misses; }
            else { ++Unavailable; }

            // A gap breaks comparability. Do not infer a transition across it.
            const bool bDefinitive = IsDefinitive(Outcome);
            if (bDefinitive && bPreviousDefinitive && Previous != Outcome)
            {
                ++Transitions;
                if (FirstTransitionIndex < 0) { FirstTransitionIndex = Index; }
            }
            Previous = Outcome;
            bPreviousDefinitive = bDefinitive;
        }
    };
}
