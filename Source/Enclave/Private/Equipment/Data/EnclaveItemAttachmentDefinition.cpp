// Copyright Lightcarrier Studios

#include "Equipment/Data/EnclaveItemAttachmentDefinition.h"

const FEnclaveItemAttachmentMapping* UEnclaveItemAttachmentDefinition::FindAttachmentMapping(const FGameplayTag& ItemType) const
{
	if (!ItemType.IsValid())
	{
		return nullptr;
	}

	return ItemAttachmentMappings.Find(ItemType);
}