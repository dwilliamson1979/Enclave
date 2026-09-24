// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Reload.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"

#include "Abilities/Interfaces/EnclaveReloadableInterface.h"

UEnclaveGA_Reload::UEnclaveGA_Reload()
{
	// Owned for the whole activation; UEnclaveGA_Fire is blocked by it.
	// Re-pressing reload mid-reload needs no extra tag - an
	// InstancedPerActor ability that is already active doesn't re-activate
	// (bRetriggerInstancedAbility defaults to false).
	ActivationOwnedTags.AddTag(EnclaveGameplayTags::State_Weapon_Reloading);
}

bool UEnclaveGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Spec-handle variant, not GetTriggeringSourceAs(): GAS may evaluate
	// this on the class default object, which has no current spec.
	const IEnclaveReloadableInterface* Reloadable = GetTriggeringSourceFromSpecAs<IEnclaveReloadableInterface>(Handle, ActorInfo);

	// No reloadable source, or nothing to reload (magazine full) - refusing
	// here rather than activating and immediately ending keeps a
	// full-magazine reload press from flashing State.Weapon.Reloading (and
	// blocking fire) for a frame.
	return Reloadable && Reloadable->CanReload();
}

void UEnclaveGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	const IEnclaveReloadableInterface* Reloadable = GetTriggeringSourceAs<IEnclaveReloadableInterface>();

	if (!Reloadable)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// Read once, at activation: the same number drives the server's wait
	// and (via the cue's RawMagnitude) the montage play rate, so they can't
	// disagree even if a modifier changes mid-reload.
	const float ReloadDuration = FMath::Max(0.0f, Reloadable->GetReloadDuration());

	AddReloadCue(ActorInfo, ReloadDuration);

	if (!HasAuthority(&ActivationInfo))
	{
		// Predicting client: stay active until the server ends the ability
		// (see the class comment for why the client never ends it itself).
		return;
	}

	if (ReloadDuration <= KINDA_SMALL_NUMBER)
	{
		// A zero-length timer never fires (SetTimer with a non-positive
		// rate clears instead of scheduling), so an instant reload has to
		// complete directly.
		OnReloadDurationElapsed();
		return;
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadDuration);
	WaitTask->OnFinish.AddDynamic(this, &UEnclaveGA_Reload::OnReloadDurationElapsed);
	WaitTask->ReadyForActivation();
}

void UEnclaveGA_Reload::AddReloadCue(const FGameplayAbilityActorInfo* ActorInfo, float ReloadDuration)
{
	// Same responder resolution as UEnclaveGA_Fire::ExecuteSourceCue:
	// EffectCauser is what UEnclaveGameplayCueNotify forwards to.
	AActor* TriggeringActor = GetTriggeringSourceAs<AActor>();

	if (!IsValid(TriggeringActor))
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	CueParameters.EffectCauser = TriggeringActor;
	CueParameters.SourceObject = TriggeringActor;

	// Carries the duration this activation is actually waiting on, so the
	// responder can scale its montage to finish exactly when the reload
	// does - modifiers included, without the responder recomputing them.
	CueParameters.RawMagnitude = ReloadDuration;

	// The ability-level helper (rather than ASC->AddGameplayCue directly)
	// scopes the add to this activation's prediction key and, with
	// bRemoveOnAbilityEnd, removes the cue in EndAbility for both a
	// completed and a cancelled reload - no bookkeeping here.
	K2_AddGameplayCueWithParams(EnclaveGameplayTags::GameplayCue_Weapon_Reload, CueParameters, /*bRemoveOnAbilityEnd=*/ true);
}

void UEnclaveGA_Reload::OnReloadDurationElapsed()
{
	if (IEnclaveReloadableInterface* Reloadable = GetTriggeringSourceAs<IEnclaveReloadableInterface>())
	{
		Reloadable->CompleteReload();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
