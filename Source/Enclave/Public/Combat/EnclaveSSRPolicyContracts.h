// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "EnclaveSSRPolicyContracts.generated.h"

/** D11D-2 contracts only. No authoritative hit-selection path consumes these types yet. */

/** Outcome of attempting a server-authorized historical query, not a damage verdict. */
UENUM(BlueprintType)
enum class EEnclaveSSRQueryOutcome : uint8
{
	NotEvaluated UMETA(DisplayName = "Not Evaluated"),
	HistoricalHit UMETA(DisplayName = "Historical Hit"),
	HistoricalMiss UMETA(DisplayName = "Historical Miss"),
	Unavailable UMETA(DisplayName = "Historical Evaluation Unavailable")
};

/** Why a historical evaluation could not safely provide an answer. */
UENUM(BlueprintType)
enum class EEnclaveSSRUnavailableReason : uint8
{
	None,
	InvalidOrUntrustedShotIntent,
	TemporalPolicyDisabled,
	UnsupportedGeometry,
	ServerTimingUnavailable,
	RewindWindowExceeded,
	HistoryMissing,
	HistoryDiscontinuity,
	HistoricalQueryNotPrepared,
	InvalidQueryContext
};

/** Match-authorized behavior when a historical answer is genuinely unavailable.
 * This is NOT a permission to retry a valid historical miss or occluded hit.
 */
UENUM(BlueprintType)
enum class EEnclaveSSRUnavailableFallbackPolicy : uint8
{
	RejectHit UMETA(DisplayName = "No Damage Hit"),
	PresentTimeTrace UMETA(DisplayName = "Present-Time Trace")
};

/** Describes what an eventual authoritative selector actually decided. */
UENUM(BlueprintType)
enum class EEnclaveSSRSelection : uint8
{
	NotSelected,
	RejectedShotIntent,
	PresentTimeByMatchPolicy,
	Historical,
	PresentTimeUnavailableFallback,
	NoHitUnavailable
};

/** Typed classification for future selection logic. Never treat bWorldOccluded as Unavailable. */
struct ENCLAVE_API FEnclaveSSRQueryAssessment
{
	EEnclaveSSRQueryOutcome Outcome = EEnclaveSSRQueryOutcome::NotEvaluated;
	EEnclaveSSRUnavailableReason UnavailableReason = EEnclaveSSRUnavailableReason::None;
	bool bWorldOccluded = false;

	/** Return true only if the evaluator has a definitive historical hit/miss outcome. */
	bool HasDefinitiveHistoricalResult() const
	{
		return Outcome == EEnclaveSSRQueryOutcome::HistoricalHit ||
			Outcome == EEnclaveSSRQueryOutcome::HistoricalMiss;
	}
};

/** Audit record of a future server-side selection; not a substitute for FHitResult. */
struct ENCLAVE_API FEnclaveSSRSelectionDecision
{
	EEnclaveSSRSelection Selection = EEnclaveSSRSelection::NotSelected;
	EEnclaveSSRUnavailableReason UnavailableReason = EEnclaveSSRUnavailableReason::None;
	bool bSelectedHit = false;
};
