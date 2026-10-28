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
 * A future trusted timing policy must establish that delay. C4 patrol-trajectory
 * matches are untrusted diagnostics, NOT a measured delay or clock correction.
 *
 * No C1 code enables rewind or alters current-time hit registration.
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
	 * Resolve a candidate historical target time ONLY when a separate system
	 * provides the effective age of the client-visible target at shot capture.
	 * This age includes network delivery and any presentation smoothing delay.
	 *
	 * Failure is explicit: do not silently clamp to a different historical
	 * time or substitute RTT/2. The later SSR caller must choose its own
	 * safe fallback policy (present-time or no historical hit).
	 */
	bool TryResolveTargetHistoryTime(double EffectivePresentationDelaySeconds, double& OutTargetTimeSeconds) const;

	/**
	 * Server-derived diagnostic anchor, independent of the client's timestamp:
	 *   targetTime = serverReceipt - serverObservedRTT - additionalSmoothing.
	 * This symmetric-path / interpolation approximation is NOT yet a proven
	 * authoritative target-render time. Never clamp a failed rewind to the oldest
	 * frame. Only C4D shadow comparisons may consume it until C5 is reviewed.
	 */
	bool TryResolveServerDerivedHistoryTime(double AdditionalSmoothingSeconds, double& OutTargetTimeSeconds) const;

	/**
	 * Like TryResolveServerDerivedHistoryTime, but the offset from the RTT anchor
	 * may be negative (rewind less than a full RTT):
	 *   targetAge = serverObservedRTT + OffsetFromRTTSeconds, within [0, MaxRewind].
	 * Fails instead of clamping when the age leaves that window.
	 */
	bool TryResolveServerAnchoredHistoryTime(double OffsetFromRTTSeconds, double& OutTargetTimeSeconds) const;
};
