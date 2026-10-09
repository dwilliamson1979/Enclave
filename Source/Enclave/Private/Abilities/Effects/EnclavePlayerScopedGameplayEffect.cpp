// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclavePlayerScopedGameplayEffect.h"

#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayTags/EnclaveEffectTags.h"

UEnclavePlayerScopedGameplayEffect::UEnclavePlayerScopedGameplayEffect()
{
	UAssetTagsGameplayEffectComponent* ScopeTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("EffectScopeTags"));

	FInheritedTagContainer AssetTags;
	AssetTags.AddTag(EnclaveGameplayTags::Effect_Scope_Player);
	ScopeTagsComponent->SetAndApplyAssetTagChanges(AssetTags);
	GEComponents.Add(ScopeTagsComponent);
}
