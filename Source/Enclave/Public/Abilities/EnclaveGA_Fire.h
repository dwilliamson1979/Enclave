// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveGA_Fire.generated.h"

class UGameplayEffect;
class AEnclaveWeapon;

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
 * Phase 3: this class triggers cosmetics without knowing what, if
 * anything, responds to them. Fire feedback (montage, muzzle flash, fire
 * sound - see AEnclaveWeapon::HandleWeaponFire, reached via a Blueprint
 * child of UEnclaveGameplayCueNotify forwarding through
 * IEnclaveGameplayCueResponderInterface) is a Gameplay Cue executed
 * unconditionally on every successful activation, including a miss, and
 * including on the predicting owning client for instant local feedback.
 * The hit marker is deliberately NOT a Gameplay Cue - a Cue
 * executed on the shooter's ASC replicates to everyone who has that ASC
 * relevant, which is wrong for feedback only the shooter should see - so
 * this instead looks up whatever on the PlayerController implements
 * IEnclaveHitFeedbackInterface (currently UEnclaveCombatFeedbackComponent)
 * and calls it, the same FindComponentByInterface idiom
 * UEnclaveEquipmentComponent uses for its item source. This class never
 * references AEnclavePlayerController or any other concrete class outside
 * its own domain.
 *
 * DamageAmount, TraceRange, and the fire-rate cooldown effect are now read
 * from the currently wielded weapon's UEnclaveWeaponDefinition (resolved via
 * ResolveWeapon(), same idiom TriggerFireCue already used) rather than
 * stored on this ability. This is the same principle
 * UEnclaveGameplayCueNotify already follows for presentation data - the
 * ability/notify doesn't own per-weapon data, it resolves it fresh each
 * time from whatever is currently wielded. One weapon = one Weapon
 * Definition asset; a Blueprint subclass of this ability (e.g.
 * BP_GA_Fire_Rifle) is only needed when a weapon's firing *behavior*
 * genuinely differs (a multi-trace shotgun, a burst-fire weapon), not to
 * change a number.
 *
 * DamageEffectClass stays ability-level: its magnitude is Set by Caller, so
 * the same effect class is expected to work for every hitscan weapon - there
 * is currently nothing per-weapon left to configure on it.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Fire : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Fire();

	/** Overridden to resolve the currently wielded weapon's own cooldown effect (UEnclaveWeaponDefinition::GetCooldownEffectClass) instead of a static CooldownGameplayEffectClass configured on the ability. Falls back to Super (i.e. CooldownGameplayEffectClass, left unset here) if no weapon can be resolved. */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void PerformTraceAndApplyDamage(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo);

	/** Executes GameplayCue.Weapon.Fire against the currently wielded weapon (GetCurrentSourceObject()). Unconditional - fires on a miss too, and on the predicting owning client as well as the server. Presentation only; does not know or care whether anything is bound to the cue. */
	void TriggerFireCue(const FGameplayAbilityActorInfo* ActorInfo);

	/** Server-only: tells whatever implements IEnclaveHitFeedbackInterface on the owning player's controller that the trace above actually landed a hit and applied damage. Not a Gameplay Cue - see the class comment for why. */
	void NotifyHitConfirmed(const FGameplayAbilityActorInfo* ActorInfo);

	/** Resolves the currently wielded weapon (GetCurrentSourceObject(), cast to AEnclaveWeapon). Centralizes the same cast TriggerFireCue used to do inline, now also used for weapon-tuning lookups and the cooldown effect override. Returns nullptr if nothing valid is currently wielded. */
	AEnclaveWeapon* ResolveWeapon() const;

	/** Instant Gameplay Effect that reduces the target's Health. Must have a modifier on Health with magnitude "Set by Caller", tag Data.Damage. Shared across weapons - see class comment. */
	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
