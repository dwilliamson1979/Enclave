// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveWeaponDefinition.generated.h"

class USkeletalMesh;
class UGameplayAbility;
class UGameplayEffect;
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

	// Per-weapon combat tuning, read by UEnclaveGA_Fire at activation time
	// (resolved off GetCurrentSourceObject(), the same idiom already used to
	// find the weapon for GameplayCue.Weapon.Fire). Previously these lived as
	// EditDefaultsOnly fields on the ability itself, which meant a new weapon
	// with different numbers needed its own Blueprint subclass of the ability
	// (BP_GA_Fire_Rifle, BP_GA_Fire_Shotgun, ...) just to change a float.
	// Living here instead means one weapon = one data asset, full stop - a
	// new ability subclass is only needed when a weapon's *firing behavior*
	// genuinely differs (e.g. a multi-trace shotgun), not for tuning.
	float GetDamageAmount() const { return DamageAmount; }
	float GetTraceRange() const { return TraceRange; }

	// This weapon's fire-rate cooldown GE. Deliberately per-weapon (not
	// shared) since cooldown tags are already scoped per weapon identity
	// (e.g. Cooldown.Weapon.Rifle.Fire) so two different weapons never share
	// a cooldown even mid-swap.
	TSubclassOf<UGameplayEffect> GetCooldownEffectClass() const { return CooldownEffectClass; }

	// This weapon's damage-application GE, read by AEnclaveWeapon at trace
	// time and stamped onto each FEnclaveHitResult::DamageEffectClass -
	// deliberately per-weapon rather than shared, since damage type
	// (ballistic, energy, fire, an EMP hit that only touches a
	// Shields/Electronics attribute) is a property of what fired, not
	// something UEnclaveGA_Fire should assume is the same for everyone
	// implementing IEnclaveFireableInterface.
	TSubclassOf<UGameplayEffect> GetDamageEffectClass() const { return DamageEffectClass; }

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

	/** Flat damage applied per successful hit. Positive; UEnclaveGA_Fire negates it when setting the SetByCaller magnitude. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (AllowPrivateAccess = "true"))
	float DamageAmount = 20.0f;

	/** Max hitscan trace distance from the camera view point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (AllowPrivateAccess = "true"))
	float TraceRange = 10000.0f;

	/** Fire-rate cooldown Gameplay Effect for this specific weapon (e.g. GE_Fire_Cooldown_Rifle). Must carry this weapon's own Cooldown.Weapon.* tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> CooldownEffectClass;

	/** Instant Gameplay Effect this weapon's hits apply. Must carry a modifier on the target attribute with magnitude "Set by Caller", tag Data.Damage. Per-weapon (not shared) so different damage types can route to different attributes - e.g. an EMP weapon's effect modifying Shields/Electronics instead of Health. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};