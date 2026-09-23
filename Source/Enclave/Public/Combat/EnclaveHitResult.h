// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "EnclaveHitResult.generated.h"

/**
 * FHitResult enriched with the gameplay outcome of that specific hit, not
 * just its geometry. Lets a fireable implementation (AEnclaveWeapon today)
 * compute per-hit damage using data only it has at trace time - bone name
 * (headshot detection), distance (falloff), which pellet in a future
 * multi-trace weapon - rather than the ability applying one flat damage
 * number to every hit uniformly. Replaces
 * IEnclaveFireableInterface::GetFireDamageAmount(), which could only ever
 * express a single number for the whole shot.
 *
 * Deliberately not Fire-specific in name or home (Combat/, not
 * Abilities/Interfaces/) - "a hit plus what it did" is a shape a future
 * melee or explosive ability could reuse too, not just gunfire.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveHitResult : public FHitResult
{
	GENERATED_BODY()

	FEnclaveHitResult() = default;

	/** Promotes a plain trace result - construct as FEnclaveHitResult(RawHit), then set Damage (and any future fields) separately. */
	explicit FEnclaveHitResult(const FHitResult& InHitResult)
		: FHitResult(InHitResult)
	{
	}

	/**
	 * Positive damage this specific hit should apply. UEnclaveGA_Fire
	 * negates it when setting the Data.Damage SetByCaller magnitude -
	 * implementations set a positive, designer-facing amount here, same
	 * convention the old GetFireDamageAmount() used.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	float Damage = 0.0f;
};
