// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"

/**
 * Stage C1 timing contract for a server-validated remote shot.
 *
 * This struct is constructed on authority, never replicated. The client shot
 * timestamp is an UNTRUSTED estimate of synchronized server time, already
 * screened by Stage B for gross age/future anomalies. It is not by itself a
 * historical target timestamp.
 *
 * ServerReceiptTimeSeconds is read from the authoritative GameState clock at
 * shot validation. EffectivePresentationDelaySeconds is deliberately NOT
 * estimated from half-RTT or from mesh-to-capsule smoothing distance here.
 * C2 will supply a measured/bounded visual delay from target history.
 *
 * No C1 code enables rewind or alters current-time hit registration.
 */
struct ENCLAVE_API FEnclaveShotTimingContext
{
	double ClientEstimatedShotTimeSeconds = 0.0;
	double ServerReceiptTimeSeconds = 0.0;
	double MaxRewindSeconds = 0.0;

	/** Measured age at server validation; includes uplink and clock-estimation error. */
	double GetObservedShotAgeSeconds() const
	{
		return ServerReceiptTimeSeconds - ClientEstimatedShotTimeSeconds;
	}

	/**
	 * Resolve a candidate historical target time ONLY when a separate system
	 * provides the effective age of the client-visible target at shot capture.
	 * This age includes network delivery and any presentation smoothing delay.
	 *
	 * Failure is explicit: do not silently clamp to a different historical
	 * time or substitute RTT/2. The later SSR caller must choose its own
	 * safe fallback policy (present-time or no historical hit).
	 */
	bool TryResolveTargetHistoryTime(double EffectivePresentationDelaySeconds, double& OutTargetTimeSeconds) const;
};
