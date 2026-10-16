// Copyright Lightcarrier Studios
#include "Abilities/EnclaveGA_Unstow.h"
#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "GameFramework/Actor.h"

UEnclaveGA_Unstow::UEnclaveGA_Unstow()
{
	// Discrete Equipment selection is server-authoritative. Local input still
	// activates this item-granted spec through the regular GAS input path.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UEnclaveGA_Unstow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UEnclaveEquipmentComponent* Equipment = IsValid(Avatar) ? Avatar->FindComponentByClass<UEnclaveEquipmentComponent>() : nullptr;

	// The switch revokes this item's granted ability specs. End first so the
	// switching path never tries to cancel the currently executing ability.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);

	if (IsValid(Avatar) && Avatar->HasAuthority() && IsValid(Equipment))
	{
		Equipment->RestoreStowedSelection();
	}
}
