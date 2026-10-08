// Copyright Lightcarrier Studios

#include "Abilities/EnclaveItemGameplayAbility.h"

#include "GameplayEffect.h"

#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveEffectTags.h"

UEnclaveItemGameplayAbility::UEnclaveItemGameplayAbility()
{
	// Every ability granted by the wielded item participates in the same broad
	// GAS interaction category. Equip/death/etc. can therefore block or cancel
	// item actions generically without knowing about Fire, Reload, or future
	// concrete actions.
	AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Item);
}

void UEnclaveItemGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect();

	if (!IsValid(CooldownEffect))
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownEffect->GetClass(), GetAbilityLevel());

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	// Dynamic asset tags are metadata on this applied spec; they do not grant
	// Effect.Scope.Avatar to the ASC. This lets generic authored cooldown GEs
	// participate in avatar-lifetime cleanup without requiring a special BP base.
	SpecHandle.Data->AddDynamicAssetTag(EnclaveGameplayTags::Effect_Scope_Avatar);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
