// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Combat/EnclaveHitResult.h"
#include "EnclaveFireableInterface.generated.h"

class UGameplayEffect;
class AActor;

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveFireableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by whatever can be the triggering source of UEnclaveGA_Fire -
 * a weapon today (AEnclaveFirearmWeapon), potentially a passive item or a
 * deployed piece of equipment (a sentry turret) later. Carries everything
 * the ability needs to know about *how this specific thing fires*, so
 * UEnclaveGA_Fire never references a concrete weapon class: it resolves
 * this interface fresh each activation via
 * UEnclaveGameplayAbility::GetTriggeringSourceAs() and talks only to it.
 *
 * Same interface-first pattern used elsewhere in the project: one stable
 * gameplay capability contract, with implementations resolved by interface
 * rather than concrete cast.
 */
class ENCLAVE_API IEnclaveFireableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Performs this thing's own hit-detection and returns whatever it hit,
	 * each with its own computed per-hit Damage already set
	 * (FEnclaveHitResult) - a single hitscan trace today
	 * (AEnclaveFirearmWeapon::PerformFireHitDetection), a multi-pellet cone
	 * or a projectile spawn later without any change to UEnclaveGA_Fire.
	 * Trace range, aim source, and damage are all this implementation's own
	 * concern, not something the ability supplies or dictates - this
	 * deliberately does NOT take a ViewLocation/ViewRotation computed by the
	 * ability, so a non-eye-based Fireable (a deployed turret tracking its
	 * own target) isn't forced to pretend it aims from InstigatorToIgnore's
	 * camera. An eye-based implementation is free to call
	 * InstigatorToIgnore->GetActorEyesViewPoint() itself.
	 *
	 * Damage is computed per-hit rather than being a single flat
	 * GetFireDamageAmount() so an implementation can vary it using data only
	 * it has at trace time - FHitResult::BoneName for a headshot,
	 * FHitResult::Distance for falloff, which pellet in a future shotgun -
	 * without the ability needing to know any of that exists.
	 *
	 * InstigatorToIgnore is the avatar actor doing the firing (and,
	 * transitively, whatever is attached to it - including this thing
	 * itself) so implementations don't hit their own wielder, and, for an
	 * eye-based implementation, is also the natural place to read an aim
	 * source from.
	 *
	 * Returns an empty array on a miss - never a single "did it hit"
	 * bool - so a multi-hit implementation (a shotgun's pellets) is a
	 * drop-in replacement with no change to how the ability consumes this.
	 */
	virtual TArray<FEnclaveHitResult> PerformFireHitDetection(AActor* InstigatorToIgnore) const = 0;

	/** This thing's fire-rate cooldown Gameplay Effect. May return null - UEnclaveGA_Fire::GetCooldownGameplayEffect() falls back to its own (unset) base CooldownGameplayEffectClass in that case. */
	virtual TSubclassOf<UGameplayEffect> GetFireCooldownEffectClass() const = 0;
};
