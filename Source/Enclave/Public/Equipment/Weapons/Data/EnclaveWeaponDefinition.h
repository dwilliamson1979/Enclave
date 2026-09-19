// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveWeaponDefinition.generated.h"

class USkeletalMesh;
class UGameplayAbility;

UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	const FGameplayTagContainer& GetWeaponTags() const { return WeaponTags; }
	const FGameplayTag& GetWeaponType() const { return WeaponType; }

	USkeletalMesh* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMesh* GetThirdPersonMesh() const { return ThirdPersonMesh; }

	const TArray<TSubclassOf<UGameplayAbility>>& GetGrantedAbilities() const { return GrantedAbilities; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Weapon"))
	FGameplayTagContainer WeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Weapon.Type"))
	FGameplayTag WeaponType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> FirstPersonMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> ThirdPersonMesh;

	/** Granted to the wielder's ASC while this weapon is equipped, removed the moment it's unequipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};