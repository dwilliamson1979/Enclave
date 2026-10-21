// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Focus.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Abilities/Interfaces/EnclaveFocusProviderInterface.h"
#include "Equipment/Focus/EnclaveFocusProfile.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"
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
	const AEnclaveItem* SourceItem = GetTriggeringSourceFromSpecAs<AEnclaveItem>(Handle, ActorInfo);
	if (!IsValid(SourceItem))
	{
		return false;
	}

	const IEnclaveFocusProviderInterface* Provider = Cast<IEnclaveFocusProviderInterface>(SourceItem);
	return Provider && IsValid(Provider->ResolveFocusProfile());
}

void UEnclaveGA_Focus::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const AEnclaveItem* SourceItem = GetTriggeringSourceAs<AEnclaveItem>();
	const IEnclaveFocusProviderInterface* Provider = IsValid(SourceItem) ? Cast<IEnclaveFocusProviderInterface>(SourceItem) : nullptr;
	if (!Provider || !IsValid(Provider->ResolveFocusProfile()) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// WaitInputRelease uses the GAS press/release lifecycle owned by Equipment.
	// The initial-state check runs only for locally controlled actors, catching
	// a very quick release that arrived before this task started listening. On
	// a remote server copy it does not consult a potentially stale InputPressed.
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
	if (!ReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	ReleaseTask->OnRelease.AddDynamic(this, &UEnclaveGA_Focus::OnInputReleased);
	AddFocusCue(ActorInfo);
	ReleaseTask->ReadyForActivation();
}

void UEnclaveGA_Focus::AddFocusCue(const FGameplayAbilityActorInfo* ActorInfo)
{
	AActor* SourceItem = GetTriggeringSourceAs<AActor>();
	if (!IsValid(SourceItem))
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Instigator = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	Parameters.EffectCauser = SourceItem;
	Parameters.SourceObject = SourceItem;

	// Identical lifecycle semantics to Reload: predicted on the owning client,
	// replicated to observers, removed automatically on EndAbility/cancellation.
	K2_AddGameplayCueWithParams(EnclaveGameplayTags::GameplayCue_Item_Action_Focus, Parameters, /*bRemoveOnAbilityEnd=*/ true);
}

void UEnclaveGA_Focus::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
