// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveItemGameplayAbility.h"
#include "EnclaveGA_Reload.generated.h"

/**
 * Reload ability - Phase 4. Same shape as UEnclaveGA_Fire: this class owns
 * the GAS lifecycle and nothing else, and everything about what reloading
 * means for the thing that triggered it comes through
 * IEnclaveReloadableInterface (AEnclaveFirearmWeapon today), resolved off
 * the spec's SourceObject. Never references a weapon class.
 *
 * Lifecycle:
 * - CanActivateAbility: the source must implement IEnclaveReloadableInterface
 *   and report CanReload() (magazine not already full). Checked on both the
 *   predicting client and the server.
 * - ActivationOwnedTags: State.Weapon.Reloading while active, which
 *   UEnclaveGA_Fire lists in its ActivationBlockedTags.
 * - Inherits the common item-ability readiness gate from
 *   UEnclaveItemGameplayAbility, so reload cannot start while the current
 *   wield relationship owns State.Item.Wielded.Equipping.
 * - GameplayCue.Weapon.Reload is ADDED (not executed) with
 *   bRemoveOnAbilityEnd, carrying the reload duration in RawMagnitude.
 *   Its concrete UEnclaveGameplayCueNotify Blueprint is configured as a
 *   Duration presentation cue, so WhileActive/Removed become semantic Reload
 *   Started/Ended events; AvatarPresentation coordinates both the avatar and
 *   originating item's cosmetic response.
 * - Timing is data-driven: the server waits GetReloadDuration() seconds,
 *   then calls CompleteReload() and ends the ability. The animation bends
 *   to fit the number, never the other way around, so reload speed
 *   modifiers (a "hot-swap magazine" attachment) only ever touch
 *   GetReloadDuration(). Switching later to animation-driven timing (a
 *   montage notify sending a gameplay event) would replace only the wait
 *   in ActivateAbility - UAbilityTask_WaitGameplayEvent instead of
 *   UAbilityTask_WaitDelay - with no change to the interface or the weapon.
 *
 * Why only the server waits: the predicting client activates first, so a
 * client-side timer would finish first too, and a client EndAbility
 * replicates to the server - ending the server's instance before its own
 * timer fired, i.e. before CompleteReload ever ran. Instead the client's
 * instance just stays active (keeping State.Weapon.Reloading and the cue
 * up locally) until the server ends the ability and that end replicates
 * back. Firing is therefore blocked on the client for about one extra
 * round trip after the reload really finished - the cost of never losing
 * a completed reload.
 *
 * Ammo prediction: when that server-sent end arrives (not cancelled, not
 * rejected), the client predicts the refill on the reloadable
 * (PredictReloadComplete) instead of waiting for the refilled count to
 * replicate - the end RPC and the weapon's count travel on different
 * actors, so the count can land later, and firing in that gap would
 * dry-fire into a magazine that's actually full. Both sides pass this
 * activation's prediction key so the prediction settles exactly.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Reload : public UEnclaveItemGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Reload();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** On the owning client, a normal (not cancelled, not rejected) end means the server completed the reload - predicts the refill before Super ends the ability. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** Adds GameplayCue.Weapon.Reload for the lifetime of this activation (removed automatically on end/cancel). Runs on the predicting client and the server alike. */
	void AddReloadCue(const FGameplayAbilityActorInfo* ActorInfo, float ReloadDuration);

	/** Server only: the reload duration elapsed uninterrupted - refill, then end (which replicates to the owning client and removes the cue everywhere). */
	UFUNCTION()
	void OnReloadDurationElapsed();

	/** Owning client: the server refused to run this reload at all. */
	void OnReloadPredictionRejected();

	/**
	 * Set when the server rejects this activation. Needed because GAS ends
	 * a rejected predicted ability as a normal, NOT cancelled, end
	 * (ClientActivateAbilityFailed -> K2_EndAbility) - without this flag,
	 * EndAbility couldn't tell a rejected reload from a completed one and
	 * would predict a refill that never happened. Reset each activation.
	 */
	bool bReloadPredictionRejected = false;
};
