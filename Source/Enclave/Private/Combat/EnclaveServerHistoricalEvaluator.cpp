// Copyright Lightcarrier Studios
#include "Combat/EnclaveServerHistoricalEvaluator.h"

#include "Characters/EnclaveCharacter.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Combat/EnclaveHistoricalHitQuery.h"
#include "Engine/World.h"
#include "Game/EnclaveGameModeBase.h"
#include "GameFramework/GameStateBase.h"

namespace
{
FEnclaveServerHistoricalAssessment Unavailable(EEnclaveSSRUnavailableReason Reason)
{
    FEnclaveServerHistoricalAssessment Result;
    Result.Policy.Outcome = EEnclaveSSRQueryOutcome::Unavailable;
    Result.Policy.UnavailableReason = Reason;
    return Result;
}
}

FEnclaveServerHistoricalAssessment FEnclaveServerHistoricalEvaluator::EvaluateCapsules(
    UWorld* ServerWorld, AActor* Shooter, const FVector& Start, const FVector& End,
    const FEnclaveShotTimingContext& ValidatedTiming, double AuthorizedHistoryTimeSeconds,
    const AEnclaveCharacter* OverrideTarget, double OverrideTimeSeconds)
{
    // Shared by D11D-4 authoritative selection and the shadow diagnostics.
    if (!IsValid(ServerWorld) || ServerWorld->GetNetMode() == NM_Client ||
        !IsValid(Shooter) || !Shooter->HasAuthority() || Shooter->GetWorld() != ServerWorld ||
        Start.ContainsNaN() || End.ContainsNaN() || Start.Equals(End) ||
        !FMath::IsFinite(FVector::Dist(Start, End)))
    {
        return Unavailable(EEnclaveSSRUnavailableReason::InvalidQueryContext);
    }

    const AEnclaveGameModeBase* GameMode = ServerWorld->GetAuthGameMode<AEnclaveGameModeBase>();
    if (!IsValid(GameMode))
    {
        return Unavailable(EEnclaveSSRUnavailableReason::InvalidQueryContext);
    }
    if (GameMode->GetWeaponHitDetectionMode() != EEnclaveWeaponHitDetectionMode::Capsule)
    {
        return Unavailable(EEnclaveSSRUnavailableReason::UnsupportedGeometry);
    }
    // Do not check GetLagCompensationMode here: shadow diagnostics call this
    // while the match runs present-time.
    const AGameStateBase* GameState = ServerWorld->GetGameState();
    if (!IsValid(GameState) || !FMath::IsFinite(AuthorizedHistoryTimeSeconds) ||
        !FMath::IsFinite(ValidatedTiming.ServerReceiptTimeSeconds) ||
        !FMath::IsFinite(ValidatedTiming.MaxRewindSeconds) ||
        ValidatedTiming.MaxRewindSeconds <= 0.0 ||
        ValidatedTiming.MaxRewindSeconds > GameMode->GetMaxRewindSeconds() ||
        ValidatedTiming.ServerReceiptTimeSeconds <= 0.0)
    {
        return Unavailable(EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
    }
    const double Age = ValidatedTiming.ServerReceiptTimeSeconds - AuthorizedHistoryTimeSeconds;
    if (!FMath::IsFinite(Age) || AuthorizedHistoryTimeSeconds <= 0.0 ||
        Age < 0.0 || Age > ValidatedTiming.MaxRewindSeconds)
    {
        return Unavailable(EEnclaveSSRUnavailableReason::RewindWindowExceeded);
    }
    if (OverrideTarget)
    {
        const double OverrideAge = ValidatedTiming.ServerReceiptTimeSeconds - OverrideTimeSeconds;
        if (!FMath::IsFinite(OverrideAge) || OverrideTimeSeconds <= 0.0 ||
            OverrideAge < 0.0 || OverrideAge > ValidatedTiming.MaxRewindSeconds)
        {
            return Unavailable(EEnclaveSSRUnavailableReason::RewindWindowExceeded);
        }
    }
    // A stale caller-owned timing context cannot turn an old shot into a valid
    // evaluation merely by asking for a time within its old rewind window.
    const double ClockDrift = GameState->GetServerWorldTimeSeconds() - ValidatedTiming.ServerReceiptTimeSeconds;
    if (!FMath::IsFinite(ClockDrift) || ClockDrift < -0.05 || ClockDrift > 0.25)
    {
        return Unavailable(EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
    }

    FEnclaveHistoricalHitQuery Query(ServerWorld, Start, End, Shooter);
    if (!Query.IsPrepared())
    {
        return Unavailable(EEnclaveSSRUnavailableReason::HistoricalQueryNotPrepared);
    }
    const FEnclaveHistoricalHitQueryResult Hit = OverrideTarget
        ? Query.TraceWithOverride(AuthorizedHistoryTimeSeconds, OverrideTarget, OverrideTimeSeconds)
        : Query.Trace(AuthorizedHistoryTimeSeconds);
    FEnclaveServerHistoricalAssessment Result;
    Result.EvaluatedServerTimeSeconds = AuthorizedHistoryTimeSeconds;
    Result.AvailableHistoryCount = Hit.Available;
    Result.MissingHistoryCount = Hit.Missing;
    Result.Policy.bWorldOccluded = Hit.bOccluded;
    Result.WorldHit = Query.GetWorldHit();
    Result.bWorldBlocked = Query.IsWorldBlocked();
    // The query reports missing history across all other character targets.
    // Treating an apparent miss as definitive would permit a phantom pass-through.
    if (Hit.Missing > 0)
    {
        Result.Policy.Outcome = EEnclaveSSRQueryOutcome::Unavailable;
        Result.Policy.UnavailableReason = EEnclaveSSRUnavailableReason::HistoryMissing;
        return Result;
    }
    Result.Target = Hit.Target;
    Result.Capsule = Hit.Capsule;
    Result.Point = Hit.Point;
    Result.EntryDistanceCm = Hit.EntryDistanceCm;
    Result.Policy.Outcome = Hit.bHit ? EEnclaveSSRQueryOutcome::HistoricalHit
        : EEnclaveSSRQueryOutcome::HistoricalMiss;
    return Result;
}
