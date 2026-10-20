// Copyright Lightcarrier Studios
#include "Abilities/EnclaveGA_CharacterAim.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveGA_CharacterAim::UEnclaveGA_CharacterAim()
{
    AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action);
    AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Character);
    AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Character_Aim);
    ActivationOwnedTags.AddTag(EnclaveGameplayTags::State_Character_Aiming);
}

void UEnclaveGA_CharacterAim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // This native test ability intentionally does not modify the camera or weapon.
    // It proves that a locally predicted character action survives until the
    // original input is released or GAS cancels it during a wield transition.
    UAbilityTask_WaitInputRelease* WaitTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
    if (!IsValid(WaitTask))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    WaitTask->OnRelease.AddDynamic(this, &UEnclaveGA_CharacterAim::OnInputReleased);
    WaitTask->ReadyForActivation();
}

void UEnclaveGA_CharacterAim::OnInputReleased(float TimeHeld)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
