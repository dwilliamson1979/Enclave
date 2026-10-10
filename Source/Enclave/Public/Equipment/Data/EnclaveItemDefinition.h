// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/EnclaveAbilityGrant.h"
#include "Equipment/Data/EnclaveItemActionBinding.h"
#include "EnclaveItemDefinition.generated.h"

class USkeletalMesh;

/**
 * Authored data shared by every physical item definition.
 *
 * Concrete definition subclasses add domain-specific data (weapon tags,
 * firearm tuning, etc.), while generic item systems can source identity,
 * capabilities, presentation meshes, equip timing, and granted abilities
 * from this base without depending on a weapon-specific definition.
 */
UCLASS(Abstract, BlueprintType)
class ENCLAVE_API UEnclaveItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FGameplayTag& GetItemType() const { return ItemType; }
	bool HasItemCapability(const FGameplayTag& CapabilityTag) const { return ItemCapabilities.HasTagExact(CapabilityTag); }

	USkeletalMesh* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMesh* GetThirdPersonMesh() const { return ThirdPersonMesh; }

	float GetEquipDuration() const { return EquipDuration; }
	const TArray<FEnclaveAbilityGrant>& GetAbilityGrants() const { return AbilityGrants; }

    /** Returns an explicit action-slot override, or nullptr for legacy grant-based resolution. */
    const FEnclaveItemActionBinding* FindActionBinding(const FGameplayTag& InputTag) const;

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

	/** First-person presentation mesh for this physical item. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> FirstPersonMesh;

	/** Third-person presentation mesh for this physical item. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> ThirdPersonMesh;

	/**
	 * Gameplay-authoritative delay from selecting this item until its item
	 * actions become ready. Draw animation may be authored to visually fit this
	 * window, but animation/AnimNotifies never complete the gameplay transition.
	 * 0 = immediately ready, immediate visual attachment, and no Draw presentation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float EquipDuration = 0.0f;

	/**
	 * Granted to the wielder's ASC while this item is wielded and removed the
	 * moment it stops being wielded. Empty is valid for items with no actions.
	 * Each entry pairs an ability with the input tag that activates it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Abilities", meta = (AllowPrivateAccess = "true", TitleProperty = "InputTag"))
	TArray<FEnclaveAbilityGrant> AbilityGrants;

    /** Optional contextual slot overrides. Omitted slots retain existing item-grant behavior.
     * Use each InputTag at most once; duplicate entries are ambiguous and the first wins.
     * CharacterFallback is a declaration only until character-action routing is added.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Abilities", meta = (AllowPrivateAccess = "true", TitleProperty = "InputTag"))
    TArray<FEnclaveItemActionBinding> ActionBindings;
};
