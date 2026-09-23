// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveGA_Fire.generated.h"

class UGameplayEffect;

/**
 * Hitscan fire ability.
 *
 * Runs on the owning client too (LocalPredicted, inherited from
 * UEnclaveGameplayAbility) so the cooldown/cost commit feels instant, but
 * the trace and damage application only happen where HasAuthority() is
 * true - there is no cosmetic reaction to the damage itself for a
 * predicting client to show locally, so predicting the damage would only
 * risk a client/server mismatch for no visible benefit.
 *
 * This class owns the GAS lifecycle (commit, authority gating, cue
 * triggering, effect application, cooldown) and nothing else - it never
 * references AEnclaveWeapon, UEnclaveWeaponDefinition, or any other
 * concrete class outside its own domain. Everything about *how* the
 * triggering thing fires (its trace/hit-scan shape, its damage amount, its
 * cooldown effect) comes from IEnclaveFireableInterface, resolved fresh
 * each activation via GetTriggeringSourceAs<IEnclaveFireableInterface>()
 * (UEnclaveGameplayAbility). A weapon implements that interface today;
 * anything else that should be able to trigger this ability (a passive
 * item, deployed equipment) just implements the same interface - this
 * class doesn't need to know or care which.
 *
 * Fire feedback (montage, muzzle flash, fire sound - see
 * AEnclaveWeapon::HandleWeaponFire, reached via a Blueprint child of
 * UEnclaveGameplayCueNotify forwarding through
 * IEnclaveGameplayCueResponderInterface) is a Gameplay Cue executed
 * unconditionally on every successful activation, including a miss, and
 * including on the predicting owning client for instant local feedback.
 * The hit marker is deliberately NOT a Gameplay Cue - a Cue executed on
 * the shooter's ASC replicates to everyone who has that ASC relevant,
 * which is wrong for feedback only the shooter should see - so this
 * instead looks up whatever on the PlayerController implements
 * IEnclaveHitFeedbackInterface (currently UEnclaveCombatFeedbackComponent)
 * and calls it, the same FindComponentByInterface idiom
 * UEnclaveEquipmentComponent uses for its item source.
 *
 * DamageEffectClass stays ability-level: its magnitude is Set by Caller, so
 * the same effect class is expected to work for whatever implements
 * IEnclaveFireableInterface - there is currently nothing per-implementer
 * left to configure on it.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Fire : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Fire();

	/** Overridden to resolve the triggering source's own cooldown effect (IEnclaveFireableInterface::GetFireCooldownEffectClass) instead of a static CooldownGameplayEffectClass configured on the ability. Falls back to Super (i.e. CooldownGameplayEffectClass, left unset here) if no fireable source can be resolved. */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void PerformTraceAndApplyDamage(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo);

	/** Executes GameplayCue.Weapon.Fire against the triggering source (GetCurrentSourceObject(), cast to AActor - the cue system only needs an actor implementing IEnclaveGameplayCueResponderInterface, not specifically a weapon). Unconditional - fires on a miss too, and on the predicting owning client as well as the server. Presentation only; does not know or care whether anything is bound to the cue. */
	void TriggerFireCue(const FGameplayAbilityActorInfo* ActorInfo);

	/** Server-only: tells whatever implements IEnclaveHitFeedbackInterface on the owning player's controller that the trace above actually landed a hit and applied damage. Not a Gameplay Cue - see the class comment for why. */
	void NotifyHitConfirmed(const FGameplayAbilityActorInfo* ActorInfo);

	/** Instant Gameplay Effect that reduces the target's Health. Must have a modifier on Health with magnitude "Set by Caller", tag Data.Damage. Shared - see class comment. */
	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
