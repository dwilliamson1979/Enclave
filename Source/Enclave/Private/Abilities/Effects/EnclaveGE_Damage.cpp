// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclaveGE_Damage.h"

#include "Abilities/Attributes/EnclaveAttributeSet.h"
#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveGE_Damage::UEnclaveGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& DamageModifier = Modifiers.AddDefaulted_GetRef();
	DamageModifier.Attribute = UEnclaveAttributeSet::GetDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat DamageCaller;
	DamageCaller.DataTag = EnclaveGameplayTags::Data_Damage;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageCaller);
}
