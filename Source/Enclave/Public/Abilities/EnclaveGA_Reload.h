// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
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
 * - GameplayCue.Weapon.Reload is ADDED (not executed) with
 *   bRemoveOnAbilityEnd, carrying the reload duration in RawMagnitude, so
 *   the responder can start a play-rate-scaled montage on WhileActive and
 *   stop it on Removed - which GAS fires whether the ability completed or
 *   was cancelled (weapon swapped away, later: death, sprint).
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
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Reload : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Reload();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** Adds GameplayCue.Weapon.Reload for the lifetime of this activation (removed automatically on end/cancel). Runs on the predicting client and the server alike. */
	void AddReloadCue(const FGameplayAbilityActorInfo* ActorInfo, float ReloadDuration);

	/** Server only: the reload duration elapsed uninterrupted - refill, then end (which replicates to the owning client and removes the cue everywhere). */
	UFUNCTION()
	void OnReloadDurationElapsed();
};
