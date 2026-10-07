// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclaveGE_Dead.h"

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/CancelAbilityTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveGE_Dead::UEnclaveGE_Dead()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(EnclaveGameplayTags::State_Lifecycle_Dead);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);

	// Death is a semantic interruption boundary. Cancel every currently-active
	// action without knowing which concrete abilities happen to be running.
	UCancelAbilityTagsGameplayEffectComponent* CancelActionsComponent = CreateDefaultSubobject<UCancelAbilityTagsGameplayEffectComponent>(TEXT("CancelActions"));
	FInheritedTagContainer CancelWithTags;
	CancelWithTags.AddTag(EnclaveGameplayTags::Ability_Type_Action);
	FInheritedTagContainer CancelWithoutTags;
	CancelActionsComponent->SetAndApplyCanceledAbilityTagChanges(CancelWithTags, CancelWithoutTags);
	GEComponents.Add(CancelActionsComponent);

	// The same semantic family remains unavailable for as long as Dead is
	// active. Respawn removes this GE when a new avatar lifecycle initializes.
	UBlockAbilityTagsGameplayEffectComponent* BlockActionsComponent = CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(TEXT("BlockActions"));
	FInheritedTagContainer BlockedAbilityTags;
	BlockedAbilityTags.AddTag(EnclaveGameplayTags::Ability_Type_Action);
	BlockActionsComponent->SetAndApplyBlockedAbilityTagChanges(BlockedAbilityTags);
	GEComponents.Add(BlockActionsComponent);
}
