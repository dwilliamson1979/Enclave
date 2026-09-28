// Copyright Lightcarrier Studios

#include "Equipment/Data/EnclaveItemPresentationDefinition.h"

const FEnclaveItemPresentationMapping* UEnclaveItemPresentationDefinition::FindPresentationMapping(const FGameplayTag& ItemType) const
{
	// Exact match first, then each parent in turn (Weapon.Type.Rifle.Marksman
	// -> Weapon.Type.Rifle -> Weapon.Type -> Weapon). RequestDirectParent
	// returns an invalid tag past the root, which ends the walk.
	for (FGameplayTag Tag = ItemType; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const FEnclaveItemPresentationMapping* Mapping = ItemPresentationMappings.Find(Tag))
		{
			return Mapping;
		}
	}

	return nullptr;
}

const FEnclaveWielderMontages* UEnclaveItemPresentationDefinition::FindWielderMontages(const FGameplayTag& ItemType, const FGameplayTag& Action) const
{
	const FEnclaveItemPresentationMapping* Mapping = FindPresentationMapping(ItemType);

	return (Mapping && Action.IsValid()) ? Mapping->WielderMontages.Find(Action) : nullptr;
}
