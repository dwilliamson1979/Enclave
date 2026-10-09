// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "EnclaveHitRegistrationSubsystem.generated.h"

class AActor;

/**
 * Server-side hitscan query prepared by a trusted fireable source.
 * Intentionally does not contain damage: weapons retain damage calculation,
 * and damage processing remains downstream of registration.
 *
 * FUTURE - Shot intent / rewind (not implemented):
 *   Client sends untrusted (timestamp, aim origin, aim direction) via GAS Target Data.
 *   Server validates against historical shooter view, active weapon, range,
 *   spread, fire cadence and a bounded rewind window before constructing this query.
 *   Registration can then choose present-time or historical target geometry.
 */
struct ENCLAVE_API FEnclaveHitscanQuery
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Instigator;
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
	/** Returns true only when a blocking hit was registered; server only. */
	bool RegisterHitscan(const FEnclaveHitscanQuery& Query, FHitResult& OutHit) const;

private:
	bool TraceCurrentGeometry(const FEnclaveHitscanQuery& Query, FHitResult& OutHit, EEnclaveWeaponHitDetectionMode Mode) const;
};
