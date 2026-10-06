// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "EnclaveHitResult.generated.h"

class UGameplayEffect;

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
 * DamageEffectClass rides along here too, not as a static field on
 * UEnclaveGA_Fire, for the same reason: different weapons (or even
 * different hits) may need genuinely different Gameplay Effects, not just
 * a different magnitude - an EMP hit modifying a Shields/Electronics
 * attribute instead of Health is a different effect entirely, not "ballistic
 * damage with a different number." Safe to compute per-hit rather than
 * ability-level because it's only ever consumed after hit detection
 * returns (inside UEnclaveGA_Fire's per-hit loop) - unlike the fire-rate
 * cooldown effect, which GAS resolves via CommitAbility() before any trace
 * happens and which therefore has to stay reachable independent of this
 * struct (see IEnclaveFireableInterface::GetFireCooldownEffectClass()).
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

	/** Promotes a plain trace result - construct as FEnclaveHitResult(RawHit), then set Damage/DamageEffectClass (and any future fields) separately. */
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

	/**
	 * The Gameplay Effect this specific hit's damage should be applied
	 * through. Instant, must carry a modifier on the target attribute with
	 * magnitude "Set by Caller" tagged Data.Damage - UEnclaveGA_Fire skips
	 * (doesn't apply anything for) a hit whose DamageEffectClass is unset,
	 * rather than falling back to some default, so a misconfigured weapon
	 * fails visibly instead of silently applying the wrong effect.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
