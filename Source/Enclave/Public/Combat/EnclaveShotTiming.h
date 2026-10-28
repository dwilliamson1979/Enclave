// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"

/**
 * Server-owned timing for one validated remote shot. Constructed on authority,
 * never replicated. The client shot timestamp is an UNTRUSTED estimate of
 * synchronized server time, screened by Stage B for gross age/future anomalies;
 * it is provenance only, never a rewind time.
 *
 * ServerReceiptTimeSeconds comes from the authoritative GameState clock at shot
 * validation. The rewind time itself is chosen by ResolveSSRTiming()
 * (Combat/EnclaveSSRTimingPolicy.h), which uses TryResolveServerAnchoredHistoryTime.
 */
struct ENCLAVE_API FEnclaveShotTimingContext
{
	double ClientEstimatedShotTimeSeconds = 0.0;
	double ServerReceiptTimeSeconds = 0.0;
	double MaxRewindSeconds = 0.0;

	/** Approximate round-trip time measured/retained by the authoritative PlayerState.
	 * A negative value means unavailable. Not supplied by the client and not a
	 * render-time or one-way-latency measurement.
	 */
	double ServerObservedRTTSeconds = -1.0;

	/** Measured age at server validation; includes uplink and clock-estimation error. */
	double GetObservedShotAgeSeconds() const
	{
		return ServerReceiptTimeSeconds - ClientEstimatedShotTimeSeconds;
	}

	/**
	 * Server anchor: targetAge = serverObservedRTT + OffsetFromRTTSeconds, within
	 * [0, MaxRewind]. The offset may be negative (rewind less than a full RTT).
	 * Fails instead of clamping when the age leaves that window.
	 */
	bool TryResolveServerAnchoredHistoryTime(double OffsetFromRTTSeconds, double& OutTargetTimeSeconds) const;
};
