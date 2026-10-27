// Copyright Lightcarrier Studios
#pragma once
#include "CoreMinimal.h"

// Pure, deterministic decision for a candidate character at a historical server time.
// The lifecycle state is authority-owned; a missing authoritative death boundary fails closed.
enum class EEnclaveSSRHistoricalEligibility : uint8
{
    Eligible,
    ExcludedDeadAtQuery,
    MissingDeathBoundary,
    InvalidQueryTime
};

struct FEnclaveSSRHistoricalEligibility
{
    static EEnclaveSSRHistoricalEligibility Evaluate(double QueryTimeSeconds,
        bool bCurrentlyDead, double DeathTimeSeconds)
    {
        if (!FMath::IsFinite(QueryTimeSeconds) || QueryTimeSeconds < 0.0)
            return EEnclaveSSRHistoricalEligibility::InvalidQueryTime;
        if (bCurrentlyDead && (!FMath::IsFinite(DeathTimeSeconds) || DeathTimeSeconds <= 0.0))
            return EEnclaveSSRHistoricalEligibility::MissingDeathBoundary;
        if (FMath::IsFinite(DeathTimeSeconds) && DeathTimeSeconds > 0.0 &&
            QueryTimeSeconds >= DeathTimeSeconds)
            return EEnclaveSSRHistoricalEligibility::ExcludedDeadAtQuery;
        return EEnclaveSSRHistoricalEligibility::Eligible;
    }
};
