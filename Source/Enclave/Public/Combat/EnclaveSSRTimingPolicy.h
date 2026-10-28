// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Combat/EnclaveLagCompensationMode.h"
#include "Combat/EnclaveShotTiming.h"
#include "Combat/EnclaveSSRPolicyContracts.h"

class AActor;
class AEnclaveCharacter;
class UWorld;

/**
 * D11D-3D server timing policy. See Documents/SSR_D11D3D_TimingPolicy.md for
 * the measurements behind the defaults.
 *
 * Wire format: server stamps travel as whole milliseconds of GameState server
 * world time. The server only accepts an echoed stamp that maps back to a frame
 * it actually recorded for that target, so quantization never lets a client
 * name an arbitrary time.
 */
namespace EnclaveSSRTimingPolicy
{
	/** Fails (returns 0) for non-finite, non-positive or out-of-range times. */
	ENCLAVE_API uint32 QuantizeServerTimeMs(double ServerTimeSeconds);
	inline double DequantizeServerTimeMs(uint32 StampMs) { return static_cast<double>(StampMs) / 1000.0; }

	/** Stamps older than this on the client are not echoed (the proxy has moved on). */
	constexpr double MaxClientStampAgeSeconds = 0.080;

	/** Client evidence trace length. Only timing is gathered; the weapon still owns range. */
	constexpr double ClientEvidenceTraceRangeCm = 50000.0;
}

/** Server-owned match settings consumed by Resolve(). */
struct ENCLAVE_API FEnclaveSSRTimingSettings
{
	EEnclaveSSRTimingSource Source = EEnclaveSSRTimingSource::Hybrid;
	/** Signed offset from the RTT anchor: targetAge = RTT + AnchorOffset. */
	double AnchorOffsetSeconds = -0.025;
	/** Hybrid only: how much younger / older than the anchor age an echoed stamp may be. */
	double StampEarlyToleranceSeconds = 0.040;
	double StampLateToleranceSeconds = 0.060;
};

/** Pure result of the timing decision for one shot. Times are server world seconds. */
struct ENCLAVE_API FEnclaveSSRTimingResolution
{
	bool bValid = false;
	EEnclaveSSRUnavailableReason UnavailableReason = EEnclaveSSRUnavailableReason::None;
	/** Time used for every target without a verified stamp. */
	double BaseTimeSeconds = 0.0;
	/** True when the echoed stamp target is evaluated at StampTimeSeconds. */
	bool bUseStamp = false;
	double StampTimeSeconds = 0.0;
	/** Hybrid diagnostics: a stamp was supplied but fell outside the tolerance window. */
	bool bStampRejected = false;
};

/**
 * Pure, deterministic timing decision.
 * @param bHasVerifiedStamp  The echoed stamp mapped to a recorded frame of the echoed target.
 * @param VerifiedStampTimeSeconds  That recorded frame's exact server time.
 * Never clamps: a time outside [0, MaxRewind] of receipt is rejected.
 */
ENCLAVE_API FEnclaveSSRTimingResolution ResolveSSRTiming(const FEnclaveSSRTimingSettings& Settings,
	const FEnclaveShotTimingContext& Timing, bool bHasVerifiedStamp, double VerifiedStampTimeSeconds);

/** Client-captured timing evidence: which proxy the local ray hit and the stamp it was showing. */
struct ENCLAVE_API FEnclaveSSRClientTimingEvidence
{
	TWeakObjectPtr<AActor> Target;
	uint32 StampMs = 0;

	bool IsSet() const { return Target.IsValid() && StampMs != 0; }
};

/**
 * Owning client only: local Weapon-channel trace along the shot ray. If the first
 * blocking hit is a simulated-proxy Enclave character with a fresh server stamp,
 * returns that character and stamp. The server treats this as bounded evidence,
 * never as a hit claim.
 */
ENCLAVE_API FEnclaveSSRClientTimingEvidence CaptureClientSSRTimingEvidence(UWorld* World,
	AActor* Shooter, const FVector& Origin, const FVector& Direction, double RangeCm);
