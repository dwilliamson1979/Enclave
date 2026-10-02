// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveItemDefinition.generated.h"

/**
 * Authored identity and generic capabilities shared by all item definitions.
 *
 * Concrete item-definition subclasses add domain-specific data, but systems
 * that operate on generic items can rely on this base for classification and
 * capability queries without depending on a weapon-specific definition.
 */
UCLASS(Abstract, BlueprintType)
class ENCLAVE_API UEnclaveItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FGameplayTag& GetItemType() const { return ItemType; }
	bool HasItemCapability(const FGameplayTag& CapabilityTag) const { return ItemCapabilities.HasTagExact(CapabilityTag); }

protected:
	/** Adds a default capability from a concrete definition constructor. */
	void AddItemCapability(const FGameplayTag& CapabilityTag)
	{
		if (CapabilityTag.IsValid())
		{
			ItemCapabilities.AddTag(CapabilityTag);
		}
	}

private:
	/** Semantic item classification used by generic item/presentation systems. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Classification", meta = (AllowPrivateAccess = "true"))
	FGameplayTag ItemType;

	/** Semantic capabilities exposed to systems that operate on generic items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Classification", meta = (AllowPrivateAccess = "true", Categories = "Item.Capability"))
	FGameplayTagContainer ItemCapabilities;
};
