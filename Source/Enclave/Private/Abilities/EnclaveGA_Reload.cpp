// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Reload.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"

#include "Abilities/Interfaces/EnclaveReloadableInterface.h"

UEnclaveGA_Reload::UEnclaveGA_Reload()
{
	AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Item_Reload);

	// Reload owns the policy that Fire cannot begin while Reload is active.
	// This matches GAS's native block-by-ability-identity model and means a
	// future alternate Fire ability only needs the Fire identity tag to obey it.
	BlockAbilitiesWithTag.AddTag(EnclaveGameplayTags::Ability_Type_Action_Item_Fire);

	// State remains useful independently of the interaction policy (animation,
	// UI, debugging, etc.).
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

	bReloadPredictionRejected = false;

	if (!HasAuthority(&ActivationInfo))
	{
		// Rejection is broadcast before GAS ends the rejected ability, so
		// the flag is already set by the time EndAbility checks it.
		FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();

		if (ActivationKey.IsValidKey())
		{
			ActivationKey.NewRejectedDelegate().BindUObject(this, &UEnclaveGA_Reload::OnReloadPredictionRejected);
		}
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
	// Same source routing as UEnclaveGA_Fire::ExecuteSourceCue: EffectCauser
	// identifies the originating item carried into the semantic presentation
	// event coordinated by AvatarPresentation.
	AActor* TriggeringActor = GetTriggeringSourceAs<AActor>();

	if (!IsValid(TriggeringActor))
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	CueParameters.EffectCauser = TriggeringActor;
	CueParameters.SourceObject = TriggeringActor;

	// Carries the duration this activation is actually waiting on so
	// AvatarPresentation can apply the same gameplay duration to both avatar
	// and item-local reload presentation without recomputing it.
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
		Reloadable->CompleteReload(CurrentActivationInfo.GetActivationPredictionKey());
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

void UEnclaveGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The owning client never ends its own reload (see the class comment),
	// so a normal end here is the server's ClientEndAbility arriving after
	// CompleteReload ran. IsEndAbilityValid guards the duplicate EndAbility
	// calls GAS can make for an already-ended instance.
	if (!bWasCancelled && !bReloadPredictionRejected && !HasAuthority(&ActivationInfo) && IsEndAbilityValid(Handle, ActorInfo))
	{
		if (IEnclaveReloadableInterface* Reloadable = GetTriggeringSourceAs<IEnclaveReloadableInterface>())
		{
			Reloadable->PredictReloadComplete(ActivationInfo.GetActivationPredictionKey());
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnclaveGA_Reload::OnReloadPredictionRejected()
{
	bReloadPredictionRejected = true;
}
