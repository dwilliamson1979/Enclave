// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGameplayAbility.h"

UEnclaveGameplayAbility::UEnclaveGameplayAbility()
{
	// Sensible shooter-ability defaults, overridable per subclass.
	// LocalPredicted: the owning client runs it immediately for
	// responsiveness while the server independently validates - this is
	// what Fire will want in Phase 2. A future ability without stakes for
	// fairness (e.g. a non-competitive interaction) can override this on
	// its own subclass.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UEnclaveGameplayAbility::AddAbilityAssetTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(Tag);
	SetAssetTags(AssetTags);
}
