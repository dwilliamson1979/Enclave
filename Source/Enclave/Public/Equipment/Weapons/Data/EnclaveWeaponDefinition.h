// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/EnclaveAbilityGrant.h"
#include "EnclaveWeaponDefinition.generated.h"

class USkeletalMesh;

/**
 * Data shared by every wieldable weapon, regardless of what kind of weapon
 * it is - classification, presentation meshes, and granted abilities.
 *
 * Deliberately does NOT carry anything specific to how a weapon deals
 * damage (muzzle/fire-cue presentation, trace range, damage/cooldown
 * effects) - that lives on UEnclaveFirearmWeaponDefinition, a subclass,
 * split out 2026-09-23 because none of it means anything for a non-firearm
 * wieldable (a knife, a baton). Mirrors the equivalent split between
 * AEnclaveWeapon and AEnclaveFirearmWeapon.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UEnclaveWeaponDefinition();

	bool HasItemCapability(const FGameplayTag& CapabilityTag) const { return ItemCapabilities.HasTagExact(CapabilityTag); }
	const FGameplayTagContainer& GetWeaponTags() const { return WeaponTags; }
	const FGameplayTag& GetWeaponType() const { return WeaponType; }

	USkeletalMesh* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMesh* GetThirdPersonMesh() const { return ThirdPersonMesh; }

	const TArray<FEnclaveAbilityGrant>& GetAbilityGrants() const { return AbilityGrants; }
	float GetEquipDuration() const { return EquipDuration; }

private:
	/**
	 * Semantic capabilities exposed to systems that operate on generic items.
	 * Weapon definitions are wieldable by default; additional item capabilities
	 * can be authored here without introducing marker interfaces.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Item.Capability"))
	FGameplayTagContainer ItemCapabilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Weapon"))
	FGameplayTagContainer WeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Weapon.Type"))
	FGameplayTag WeaponType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> FirstPersonMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> ThirdPersonMesh;

	/**
	 * Gameplay-authoritative delay from selecting this weapon until its item
	 * actions become ready. Draw animation may be authored to visually fit this
	 * window, but animation/AnimNotifies never complete the gameplay transition.
	 * 0 = immediately ready.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float EquipDuration = 0.0f;

	/**
	 * Granted to the wielder's ASC while this weapon is equipped, removed
	 * the moment it's unequipped. Each entry pairs an ability with the input
	 * tag that activates it (e.g. UEnclaveGA_Fire + Input.Item.Action.Primary,
	 * UEnclaveGA_Reload + Input.Item.Action.Reload).
	 *
	 * Renamed from GrantedAbilities when its element type changed - existing
	 * data assets need these entries re-entered; the old array's values
	 * don't carry over.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities", meta = (AllowPrivateAccess = "true", TitleProperty = "InputTag"))
	TArray<FEnclaveAbilityGrant> AbilityGrants;
};
