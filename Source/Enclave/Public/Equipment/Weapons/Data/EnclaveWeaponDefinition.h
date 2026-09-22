// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveWeaponDefinition.generated.h"

class USkeletalMesh;
class UGameplayAbility;
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

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

	// Phase 3: read by AEnclaveWeapon::HandleWeaponFire in response to
	// GameplayCue.Weapon.Fire - this class has no idea a cue or a cue
	// notify exists, it just exposes weapon presentation data the same way
	// it already exposes meshes and abilities.
	UAnimMontage* GetFireMontage() const { return FireMontage; }
	UNiagaraSystem* GetMuzzleFlashEffect() const { return MuzzleFlashEffect; }
	USoundBase* GetFireSound() const { return FireSound; }
	FName GetMuzzleSocketName() const { return MuzzleSocketName; }

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

	/** Played on both the first- and third-person meshes when GameplayCue.Weapon.Fire executes. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Fire Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> FireMontage;

	/** Spawned at MuzzleSocketName on both meshes when GameplayCue.Weapon.Fire executes. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Fire Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** Played from the muzzle when GameplayCue.Weapon.Fire executes. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Fire Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> FireSound;

	/** Socket name expected to exist on both the first- and third-person meshes; the muzzle flash and fire sound anchor here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Fire Cue", meta = (AllowPrivateAccess = "true"))
	FName MuzzleSocketName = "Muzzle";
};