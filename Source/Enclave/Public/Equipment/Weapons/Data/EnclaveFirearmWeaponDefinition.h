// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"
#include "EnclaveFirearmWeaponDefinition.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

/**
 * Firearm-specific weapon data: muzzle/fire-cue presentation and
 * ranged-combat tuning (damage, trace range, cooldown/damage effects).
 *
 * Split out of the generic UEnclaveWeaponDefinition (2026-09-23) because
 * none of this means anything for a non-firearm wieldable (a knife, a
 * baton) - only AEnclaveFirearmWeapon reads these fields, mirroring the
 * actor-side split between AEnclaveWeapon and AEnclaveFirearmWeapon.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveFirearmWeaponDefinition : public UEnclaveWeaponDefinition
{
	GENERATED_BODY()

public:

	// Phase 3: read by AEnclaveFirearmWeapon::HandleWeaponFire in response to
	// GameplayCue.Weapon.Fire - this class has no idea a cue or a cue
	// notify exists, it just exposes weapon presentation data the same way
	// the base class already exposes meshes and abilities.
	UAnimMontage* GetFireMontage() const { return FireMontage; }
	UNiagaraSystem* GetMuzzleFlashEffect() const { return MuzzleFlashEffect; }
	USoundBase* GetFireSound() const { return FireSound; }
	FName GetMuzzleSocketName() const { return MuzzleSocketName; }

	// Per-weapon combat tuning, read by AEnclaveFirearmWeapon at trace time
	// (resolved off GetCurrentSourceObject(), the same idiom already used to
	// find the weapon for GameplayCue.Weapon.Fire). Living here means one
	// weapon = one data asset, full stop - a new ability or actor subclass
	// is only needed when a weapon's *firing behavior* genuinely differs
	// (e.g. a multi-trace shotgun), not for tuning.
	float GetDamageAmount() const { return DamageAmount; }
	float GetTraceRange() const { return TraceRange; }

	// This weapon's fire-rate cooldown GE. Deliberately per-weapon (not
	// shared) since cooldown tags are already scoped per weapon identity
	// (e.g. Cooldown.Weapon.Rifle.Fire) so two different weapons never share
	// a cooldown even mid-swap.
	TSubclassOf<UGameplayEffect> GetCooldownEffectClass() const { return CooldownEffectClass; }

	// This weapon's damage-application GE, stamped onto each
	// FEnclaveHitResult::DamageEffectClass at trace time - deliberately
	// per-weapon rather than shared, since damage type (ballistic, energy,
	// fire, an EMP hit that only touches a Shields/Electronics attribute)
	// is a property of what fired, not something UEnclaveGA_Fire should
	// assume is the same for everyone implementing IEnclaveFireableInterface.
	TSubclassOf<UGameplayEffect> GetDamageEffectClass() const { return DamageEffectClass; }

	// Phase 4: magazine + reload. Read by AEnclaveFirearmWeapon, which is
	// what UEnclaveGA_Fire/UEnclaveGA_Reload actually talk to (through
	// IEnclaveAmmoConsumerInterface/IEnclaveReloadableInterface) - neither
	// ability reads this asset directly.
	int32 GetMagazineCapacity() const { return MagazineCapacity; }
	int32 GetAmmoPerShot() const { return AmmoPerShot; }
	float GetReloadDuration() const { return ReloadDuration; }

	UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
	USoundBase* GetDryFireSound() const { return DryFireSound; }

private:

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

	/** Rounds a full magazine holds; the weapon spawns full. 0 = no magazine at all (never runs dry, never reloads). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 MagazineCapacity = 12;

	/** Rounds one fire activation spends. A shotgun's pellets are still one round; a 3-round burst weapon would set 3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 AmmoPerShot = 1;

	/**
	 * Base seconds from pressing reload to a full magazine - the gameplay
	 * number; ReloadMontage is play-rate-scaled to match it, not the other
	 * way around. Attachments/perks modify the value
	 * AEnclaveFirearmWeapon::GetReloadDuration() returns, never this field.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ReloadDuration = 1.5f;

	/**
	 * Weapon-mesh reload montage (slide/magazine motion), played on both the
	 * first- and third-person weapon meshes while GameplayCue.Weapon.Reload
	 * is active, play-rate-scaled to fit ReloadDuration. Arm/body montages
	 * for the wielder are a later step. Clip-out/clip-in sounds belong on
	 * this montage as Play Sound anim notifies so they stay in sync with the
	 * scaled play rate. Optional.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Reload Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** Played from the muzzle when firing with an empty magazine (GameplayCue.Weapon.DryFire). Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Presentation|Dry Fire Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> DryFireSound;
};
