// Copyright Lightcarrier Studios

#include "Equipment/Data/EnclaveItemPresentationDefinition.h"

const FEnclaveItemPresentationMapping* UEnclaveItemPresentationDefinition::FindPresentationMapping(const FGameplayTag& ItemType) const
{
	if (!ItemType.IsValid())
	{
		return nullptr;
	}

	return ItemPresentationMappings.Find(ItemType);
}