// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveItemGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "TimerManager.h"
#include "EnclaveGA_Fire.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
struct FEnclaveValidatedShotRay;

/**
 * Hitscan fire ability.
 *
 * Runs on the owning client too (LocalPredicted, inherited from
 * UEnclaveGameplayAbility through UEnclaveItemGameplayAbility) so the cooldown/cost commit feels instant, but
 * the trace and damage application only happen where HasAuthority() is
 * true - there is no cosmetic reaction to the damage itself for a
 * predicting client to show locally, so predicting the damage would only
 * risk a client/server mismatch for no visible benefit.
 *
 * This class owns the GAS lifecycle (commit, authority gating, cue
 * triggering, cooldown) and the handoff of confirmed hits into Enclave's
 * common damage contract. It never
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
 * Fire feedback is a Gameplay Cue executed through
 * UEnclaveGameplayCueNotify. The notify translates the cue into one semantic
 * item action for AvatarPresentation, which coordinates both the avatar and
 * the originating item's local presentation (weapon montage, muzzle flash,
 * fire sound). The cue is executed
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
 * Damage application is target-implementation agnostic. The fireable returns
 * positive per-hit damage in FEnclaveHitResult; this ability turns each hit
 * into FEnclaveDamageRequest and submits it through UEnclaveDamageStatics.
 * Simple and GAS-backed targets therefore share the same external contract,
 * and future damage semantics belong in the request/routing layers rather
 * than leaking GameplayEffect classes into the weapon hit result.
 *
 * Ammo (Phase 4) is optional and also comes from the triggering source,
 * via IEnclaveAmmoConsumerInterface. An empty magazine does not fail
 * activation (CheckCost is deliberately not overridden - a failed cost
 * check stops GAS before ActivateAbility ever runs, leaving nowhere to play
 * a dry-fire click); ActivateAbility checks first and executes
 * GameplayCue.Item.Action.DryFire instead of firing. The round itself is spent
 * in ApplyCost, i.e. only once CommitAbility has passed its cooldown check,
 * on BOTH the predicting client and the server, each passing the
 * activation's prediction key - the ammo consumer predicts the spend on the
 * client and reconciles it against the server's (see
 * AEnclaveFirearmWeapon). Server-side activations that spend nothing (dry
 * fire, failed commit) still acknowledge their key so a client that
 * predicted differently settles. Blocked outright while
 * Reload blocks the Fire ability identity while it is active, and the equip
 * Gameplay Effect blocks the broader Ability.Type.Action.Item family.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Fire : public UEnclaveItemGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Fire();

	/** Overridden to resolve the triggering source's own cooldown effect (IEnclaveFireableInterface::GetFireCooldownEffectClass) instead of a static CooldownGameplayEffectClass configured on the ability. Falls back to Super (i.e. CooldownGameplayEffectClass, left unset here) if no fireable source can be resolved. */
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	/** GAS may call this on the Fire CDO before creating an execution instance.
	 * Resolve the item-provided cooldown from Handle/ActorInfo, never from
	 * GetCurrentSourceObject() in this pre-activation path.
	 */
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

	/** Super (any GE cost), then spends one activation's ammo from the triggering source if it implements IEnclaveAmmoConsumerInterface - really on the server, predicted on the owning client. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** Server receives target data via GAS, keyed by spec and ORIGINAL activation key. */
	void OnShotTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnShotDataTimeout();
	void ReleaseShotDataDelegate();
	void CompleteAuthoritativeShot(const FEnclaveValidatedShotRay& ValidatedRay);

	/** Transport timeout only, not a rewind allowance. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Shot Intent", meta = (ClampMin = "0.25", ClampMax = "5.0", Units = "s"))
	float ShotDataTimeoutSeconds = 1.0f;

	TWeakObjectPtr<UAbilitySystemComponent> PendingTargetDataASC;
	FDelegateHandle ShotDataDelegateHandle;
	FTimerHandle ShotDataTimeoutHandle;
	bool bWaitingForShotData = false;

	/** Server-only hit detection followed by target-agnostic Enclave damage dispatch for each confirmed hit. */
	void PerformHitDetection(const FGameplayAbilityActorInfo* ActorInfo, AActor* AvatarActor, const FGameplayAbilityActivationInfo& ActivationInfo, const FEnclaveValidatedShotRay* ValidatedRay = nullptr);

	/** Executes CueTag (GameplayCue.Item.Action.Fire, or .DryFire on an empty magazine) with the triggering source in EffectCauser/SourceObject and the ASC avatar as the cue target. UEnclaveGameplayCueNotify fans the cue out to avatar presentation and item-local cue response. Runs on the predicting owning client as well as the server. Presentation only. SourceASC/AvatarActor are resolved once in ActivateAbility and passed in, same as PerformHitDetection. */
	void ExecuteSourceCue(const FGameplayTag& CueTag, UAbilitySystemComponent* SourceASC, AActor* AvatarActor);

	/** Server only: this activation spent no ammo (dry fire / failed commit) - acknowledge its prediction key anyway so the owning client drops any spend it predicted for it. No-op for a source that doesn't consume ammo. */
	void AcknowledgeUnspentAmmo(const FGameplayAbilityActivationInfo& ActivationInfo);

	/** Server-only: tells whatever implements IEnclaveHitFeedbackInterface on the owning player's controller that the trace above actually landed a hit and applied damage. Not a Gameplay Cue - see the class comment for why. Safe to call for an AI-controlled avatar too - PlayerController resolves null there and this just no-ops. */
	void NotifyHitConfirmed(const FGameplayAbilityActorInfo* ActorInfo);
};
