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
 * Phase 3: this class triggers cosmetics without knowing what, if
 * anything, responds to them. Fire feedback (montage, muzzle flash, fire
 * sound - see UEnclaveGameplayCueNotify_WeaponFire) is a Gameplay Cue
 * executed unconditionally on every successful activation, including a
 * miss, and including on the predicting owning client for instant local
 * feedback. The hit marker is deliberately NOT a Gameplay Cue - a Cue
 * executed on the shooter's ASC replicates to everyone who has that ASC
 * relevant, which is wrong for feedback only the shooter should see - so
 * this instead looks up whatever on the PlayerController implements
 * IEnclaveHitFeedbackInterface (currently UEnclaveCombatFeedbackComponent)
 * and calls it, the same FindComponentByInterface idiom
 * UEnclaveEquipmentComponent uses for its item source. This class never
 * references AEnclavePlayerController or any other concrete class outside
 * its own domain.
 *
 * DamageEffectClass and the inherited CooldownGameplayEffectClass are
 * EditDefaultsOnly, which in practice means tuning a specific weapon
 * requires a Blueprint subclass of this class (e.g. BP_GA_Fire_Rifle) with
 * those defaults set, granted by that weapon's Weapon Definition instead of
 * this raw C++ class.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Fire : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Fire();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void PerformTraceAndApplyDamage(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo);

	/** Executes GameplayCue.Weapon.Fire against the currently wielded weapon (GetCurrentSourceObject()). Unconditional - fires on a miss too, and on the predicting owning client as well as the server. Presentation only; does not know or care whether anything is bound to the cue. */
	void TriggerFireCue(const FGameplayAbilityActorInfo* ActorInfo);

	/** Server-only: tells whatever implements IEnclaveHitFeedbackInterface on the owning player's controller that the trace above actually landed a hit and applied damage. Not a Gameplay Cue - see the class comment for why. */
	void NotifyHitConfirmed(const FGameplayAbilityActorInfo* ActorInfo);

	/** Instant Gameplay Effect that reduces the target's Health. Must have a modifier on Health with magnitude "Set by Caller", tag Data.Damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	float DamageAmount = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	float TraceRange = 10000.0f;
};
