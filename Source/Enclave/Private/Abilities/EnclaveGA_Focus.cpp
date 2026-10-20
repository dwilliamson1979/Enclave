// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Focus.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveGA_Focus::UEnclaveGA_Focus()
{
	AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Item_Focus);
	ActivationOwnedTags.AddTag(EnclaveGameplayTags::State_Item_Aiming);
	// The shared EnclaveGameplayAbility base uses LocalPredicted and
	// InstancedPerActor. A single activation remains active until release.
}

bool UEnclaveGA_Focus::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// GAS may run this check on the CDO, so resolve the source from the
	// explicitly supplied ability spec, not GetCurrentSourceObject().
	// Reject accidental grants without an actual item source.
	return IsValid(GetTriggeringSourceFromSpecAs<AEnclaveItem>(Handle, ActorInfo));
}

void UEnclaveGA_Focus::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!IsValid(GetTriggeringSourceAs<AEnclaveItem>()) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// WaitInputRelease uses the same GAS input-edge replication already used
	// by Equipment's PressAbilityInput/ReleaseAbilityInput lifecycle.
	// Do not test an initially released server-side spec: its InputPressed
	// flag may not yet mirror the locally predicted press.
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
	if (!ReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	ReleaseTask->OnRelease.AddDynamic(this, &UEnclaveGA_Focus::OnInputReleased);
	ReleaseTask->ReadyForActivation();
}

void UEnclaveGA_Focus::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
