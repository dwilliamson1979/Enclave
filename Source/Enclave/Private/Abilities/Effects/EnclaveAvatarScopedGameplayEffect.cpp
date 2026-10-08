// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclaveAvatarScopedGameplayEffect.h"

#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayTags/EnclaveEffectTags.h"

UEnclaveAvatarScopedGameplayEffect::UEnclaveAvatarScopedGameplayEffect()
{
	// Asset tags describe the effect itself and are not granted to the target.
	// That makes Effect.Scope.Avatar suitable for querying/removing active
	// effects without polluting the avatar's owned gameplay tags.
	UAssetTagsGameplayEffectComponent* ScopeTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("EffectScopeTags"));

	FInheritedTagContainer AssetTags;
	AssetTags.AddTag(EnclaveGameplayTags::Effect_Scope_Avatar);
	ScopeTagsComponent->SetAndApplyAssetTagChanges(AssetTags);
	GEComponents.Add(ScopeTagsComponent);
}
