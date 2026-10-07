// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "EnclaveHitResult.generated.h"

/**
 * FHitResult enriched with the gameplay outcome of that specific hit, not
 * just its geometry. Lets a fireable implementation compute per-hit damage
 * using data only it has at trace time - bone name (headshot detection),
 * distance (falloff), which pellet in a future multi-trace weapon - rather
 * than the ability applying one flat damage number to every hit uniformly.
 *
 * The hit result deliberately carries no GameplayEffect class. How damage is
 * implemented is a property of the receiving side of Enclave's damage
 * contract, not something a weapon or hit result should prescribe.
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

	/** Promotes a plain trace result; set Damage (and future semantic fields) separately. */
	explicit FEnclaveHitResult(const FHitResult& InHitResult)
		: FHitResult(InHitResult)
	{
	}

	/** Positive damage this specific hit should submit to Enclave's damage system. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	float Damage = 0.0f;
};
