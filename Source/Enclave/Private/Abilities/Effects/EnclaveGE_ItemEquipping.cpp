// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclaveGE_ItemEquipping.h"

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveGE_ItemEquipping::UEnclaveGE_ItemEquipping()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Equipment supplies the current item's authored equip duration on the
	// outgoing spec. Using SetByCaller makes the runtime duration part of the GE
	// definition rather than maintaining a second timer outside GAS.
	FSetByCallerFloat DurationCaller;
	DurationCaller.DataTag = EnclaveGameplayTags::Data_Item_EquipDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationCaller);

	// Gameplay Effects are component-based in modern GAS. In a GE constructor,
	// create the component as a default subobject rather than using AddComponent
	// / FindOrAddComponent, which are runtime mutation helpers.
	UTargetTagsGameplayEffectComponent* TargetTagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(EnclaveGameplayTags::State_Item_Equipping);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);

	// The equip window itself owns the readiness restriction: while this GE is
	// active, GAS blocks every ability classified as an item action. Individual
	// item abilities do not need to know that Equipping exists.
	UBlockAbilityTagsGameplayEffectComponent* BlockItemActionsComponent = CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(TEXT("BlockItemActions"));
	FInheritedTagContainer BlockedAbilityTags;
	BlockedAbilityTags.AddTag(EnclaveGameplayTags::Ability_Type_Action_Item);
	BlockItemActionsComponent->SetAndApplyBlockedAbilityTagChanges(BlockedAbilityTags);
	GEComponents.Add(BlockItemActionsComponent);
}
