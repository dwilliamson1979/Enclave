// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Combat/EnclaveShotTiming.h"
#include "Debug/EnclaveDebug.h"
#include "EnclaveHitRegistrationSubsystem.generated.h"

class AActor;
struct FEnclaveShotIntent;
struct FEnclaveValidatedShotRay;

/**
 * Server-side hitscan query prepared by a trusted fireable source.
 * Intentionally does not contain damage: weapons retain damage calculation,
 * and damage processing remains downstream of registration.
 *
 * Stage B: a remote player sends untrusted aim intent via GAS Target Data.
 *   The server checks basic shooter-origin, aim, timestamp, and sequence
 *   plausibility and passes an accepted ray to the Fireable. The Fireable
 *   still builds this query using its own range and damage policy.
 *   True historical pose validation / target rewind are NOT yet implemented.
 */
struct ENCLAVE_API FEnclaveHitscanQuery
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Instigator;

	// Supplied only for a remote shot that already passed server validation.
	// Stages C3-C4 read this in shadow diagnostics, not in live tracing.
	bool bHasValidatedClientTiming = false;
	FEnclaveShotTimingContext ValidatedClientTiming;
	uint16 ShotSequence = 0;
};

/**
 * Single point of entry for authoritative hitscan registration. GameMode owns
 * the match policy; this subsystem dispatches to the corresponding strategy.
 * Character collision response changes are committed separately by GameMode.
 *
 * FUTURE strategy seam: CapsuleRewind, AnatomicalHitboxes, AnatomicalRewind.
 * Keep the public RegisterHitscan contract stable as implementations evolve.
 * FUTURE comparison: execute non-mutating shadow queries against independently
 * queryable geometry and record disagreements; ONLY active strategy deals damage.
 */
UCLASS()
class ENCLAVE_API UEnclaveHitRegistrationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Stage C0 instrumentation only; does not enable server-side rewind. */
	static bool IsSSRPreflightDebugEnabled();

	/** Checks untrusted remote player aim against present authoritative shooter
	 * state. Tolerances intentionally allow network delay, and therefore are
	 * NOT a substitute for shooter-history or anti-cheat verification.
	 * No state or ammunition is modified on rejection.
	 */
	bool ValidateClientShotIntent(const FEnclaveShotIntent& Intent, const AActor* Shooter,
		uint16 ExpectedShotSequence, FEnclaveValidatedShotRay& OutValidatedRay,
		FString& OutRejectionReason) const;

	/** Returns true only when a blocking hit was registered; server only. */
	bool RegisterHitscan(const FEnclaveHitscanQuery& Query, FHitResult& OutHit) const;

private:
#if ENCLAVE_INCLUDE_DEBUG_CODE
	void DebugCompareHistoricalCapsules(const FEnclaveHitscanQuery& Query,
		bool bLiveHit, const FHitResult& LiveHit, EEnclaveWeaponHitDetectionMode Mode) const;
#endif
	bool TraceCurrentGeometry(const FEnclaveHitscanQuery& Query, FHitResult& OutHit, EEnclaveWeaponHitDetectionMode Mode) const;
};
