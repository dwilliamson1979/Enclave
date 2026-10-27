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
    const FEnclaveShotTimingContext& ValidatedTiming, double AuthorizedHistoryTimeSeconds)
{
    // The GameMode currently forcibly disables SSR; this evaluator is isolated
    // until Stage B connects a diagnostic-only call site. Never route damage here.
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
    // Do not check GetLagCompensationMode here: Stage A remains callable in
    // shadow-only tests even though executable production SSR is disabled.
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
    const FEnclaveHistoricalHitQueryResult Hit = Query.Trace(AuthorizedHistoryTimeSeconds);
    FEnclaveServerHistoricalAssessment Result;
    Result.EvaluatedServerTimeSeconds = AuthorizedHistoryTimeSeconds;
    Result.AvailableHistoryCount = Hit.Available;
    Result.MissingHistoryCount = Hit.Missing;
    Result.Policy.bWorldOccluded = Hit.bOccluded;
    // The query reports missing history across all other character targets.
    // Treating an apparent miss as definitive would permit a phantom pass-through.
    if (Hit.Missing > 0 || Hit.Available == 0)
    {
        Result.Policy.Outcome = EEnclaveSSRQueryOutcome::Unavailable;
        Result.Policy.UnavailableReason = EEnclaveSSRUnavailableReason::HistoryMissing;
        return Result;
    }
    Result.Target = Hit.Target;
    Result.Point = Hit.Point;
    Result.EntryDistanceCm = Hit.EntryDistanceCm;
    Result.Policy.Outcome = Hit.bHit ? EEnclaveSSRQueryOutcome::HistoricalHit
        : EEnclaveSSRQueryOutcome::HistoricalMiss;
    return Result;
}
