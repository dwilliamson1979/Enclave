// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveWeaponDefinition.generated.h"

class USkeletalMesh;

UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FGameplayTagContainer& GetWeaponTags() const { return WeaponTags; }
	FGameplayTag GetWieldedAttachPoint() const { return WieldedAttachPoint; }

	USkeletalMesh* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMesh* GetThirdPersonMesh() const { return ThirdPersonMesh; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Weapon"))
	FGameplayTagContainer WeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true", Categories = "Item.AttachPoint.Wielded"))
	FGameplayTag WieldedAttachPoint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> FirstPersonMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> ThirdPersonMesh;
};