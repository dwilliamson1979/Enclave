// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveGA_Fire.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;

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
 * DamageEffectClass is NOT configured here - it lives on
 * FEnclaveHitResult, stamped on per-hit by whatever implements
 * IEnclaveFireableInterface (AEnclaveWeapon, from its
 * UEnclaveWeaponDefinition). Different weapons - and in principle
 * different hits within the same shot - can need genuinely different
 * damage effects (an EMP hit modifying Shields/Electronics instead of
 * Health), not just a different magnitude, so this ability treats it as
 * per-hit data rather than a single shared class it owns.
 *
 * Ammo (Phase 4) is optional and also comes from the triggering source,
 * via IEnclaveAmmoConsumerInterface. An empty magazine does not fail
 * activation (CheckCost is deliberately not overridden - a failed cost
 * check stops GAS before ActivateAbility ever runs, leaving nowhere to play
 * a dry-fire click); ActivateAbility checks first and executes
 * GameplayCue.Weapon.DryFire instead of firing. The round itself is spent
 * in ApplyCost, i.e. only once CommitAbility has passed its cooldown check,
 * and only where the ability has authority - the magazine replicates back
 * to the owning client. Blocked outright while State.Weapon.Reloading is
 * owned (UEnclaveGA_Reload).
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Fire : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Fire();

	/** Overridden to resolve the triggering source's own cooldown effect (IEnclaveFireableInterface::GetFireCooldownEffectClass) instead of a static CooldownGameplayEffectClass configured on the ability. Falls back to Super (i.e. CooldownGameplayEffectClass, left unset here) if no fireable source can be resolved. */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	/** Super (any GE cost), then spends one activation's ammo from the triggering source if it implements IEnclaveAmmoConsumerInterface. Authority only. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** SourceASC/AvatarActor are resolved once in ActivateAbility and passed in here, rather than each callee re-deriving them from ActorInfo on its own. */
	void PerformHitDetection(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, UAbilitySystemComponent* SourceASC, AActor* AvatarActor);

	/** Executes CueTag (GameplayCue.Weapon.Fire, or .DryFire on an empty magazine) against the triggering source (GetCurrentSourceObject(), cast to AActor - the cue system only needs an actor implementing IEnclaveGameplayCueResponderInterface, not specifically a weapon). Runs on the predicting owning client as well as the server. Presentation only; does not know or care whether anything is bound to the cue. SourceASC/AvatarActor are resolved once in ActivateAbility and passed in, same as PerformHitDetection. */
	void ExecuteSourceCue(const FGameplayTag& CueTag, UAbilitySystemComponent* SourceASC, AActor* AvatarActor);

	/** Server-only: tells whatever implements IEnclaveHitFeedbackInterface on the owning player's controller that the trace above actually landed a hit and applied damage. Not a Gameplay Cue - see the class comment for why. Safe to call for an AI-controlled avatar too - PlayerController resolves null there and this just no-ops. */
	void NotifyHitConfirmed(const FGameplayAbilityActorInfo* ActorInfo);
};
