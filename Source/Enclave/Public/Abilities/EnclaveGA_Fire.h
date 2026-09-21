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
 * true - there is no cosmetic reaction yet for a predicting client to show
 * locally, so predicting the damage itself would only risk a
 * client/server mismatch for no visible benefit. No montage, no muzzle
 * flash, no hit marker here at all - those arrive as Gameplay Cues in
 * Phase 3, triggered off this ability's activation without this class
 * knowing anything about them.
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

	/** Instant Gameplay Effect that reduces the target's Health. Must have a modifier on Health with magnitude "Set by Caller", tag Data.Damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	float DamageAmount = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	float TraceRange = 10000.0f;
};
