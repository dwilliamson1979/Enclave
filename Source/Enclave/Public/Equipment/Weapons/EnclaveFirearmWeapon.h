// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EnclaveWeapon.h"
#include "GameplayCues/Interfaces/EnclaveGameplayCueResponderInterface.h"
#include "Abilities/Interfaces/EnclaveFireableInterface.h"
#include "EnclaveFirearmWeapon.generated.h"

class UEnclaveFirearmWeaponDefinition;
class UNiagaraComponent;
class USkeletalMeshComponent;
class UGameplayEffect;

/**
 * Hitscan/ranged weapon subclass of AEnclaveWeapon.
 *
 * Owns everything that only makes sense for something that fires:
 * - IEnclaveFireableInterface, this weapon's contract with UEnclaveGA_Fire
 *   (PerformFireHitDetection owns the line trace itself, including its own
 *   debug visualization and its own choice of aim source; GetFireCooldownEffectClass
 *   exposes this weapon's fire-rate cooldown GE).
 * - IEnclaveGameplayCueResponderInterface, dispatching GameplayCue.Weapon.Fire
 *   and GameplayCue.Weapon.Reload to this weapon's own cosmetic response.
 * - The persistent muzzle flash components and their socket/visibility setup.
 *
 * Split out of the shared AEnclaveWeapon base (2026-09-23, alongside
 * UEnclaveFirearmWeaponDefinition) so a future non-firearm wieldable - a
 * knife, a baton - doesn't inherit a Fireable contract, muzzle sockets, or
 * fire-cue handling it has no use for. Reload (Phase 4) belongs here too,
 * for the same reason: ammo/magazine state isn't a generic-weapon concept
 * either.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveFirearmWeapon : public AEnclaveWeapon, public IEnclaveGameplayCueResponderInterface, public IEnclaveFireableInterface
{
	GENERATED_BODY()

public:

	AEnclaveFirearmWeapon();

	// IEnclaveGameplayCueResponderInterface
	/** Dispatches by exact tag match to this weapon's own HandleWeaponFire/HandleWeaponReload. Logs and returns false for a tag this weapon doesn't recognize, rather than silently doing nothing. */
	virtual bool HandleGameplayCue(FGameplayTag CueTag, const FGameplayCueParameters& Parameters) override;

	// IEnclaveFireableInterface - this weapon's contract with UEnclaveGA_Fire.
	// PerformFireHitDetection is handed only InstigatorToIgnore - it resolves
	// its own aim source (GetActorEyesViewPoint) rather than being told one,
	// so a future non-eye-based Fireable isn't forced into the same contract.
	virtual TArray<FEnclaveHitResult> PerformFireHitDetection(AActor* InstigatorToIgnore) const override;
	virtual TSubclassOf<UGameplayEffect> GetFireCooldownEffectClass() const override;

protected:

	virtual void ApplyWeaponDefinition() override;

private:

	/** WeaponDefinition, cast to this weapon's actual definition subtype. Null if this actor was misconfigured with a base (non-firearm) UEnclaveWeaponDefinition. */
	const UEnclaveFirearmWeaponDefinition* GetFirearmWeaponDefinition() const;

	/** Assigns the muzzle flash asset to MuzzleFlashComponent and snaps it onto MeshComp's muzzle socket, matching MeshComp's own owner-visibility flags. Leaves the component with no asset (inactive) if InWeaponDefinition has no muzzle flash effect or the socket doesn't exist on MeshComp. */
	void ApplyMuzzleFlashSocket(UNiagaraComponent* MuzzleFlashComponent, USkeletalMeshComponent* MeshComp, const UEnclaveFirearmWeaponDefinition* InWeaponDefinition);

	/** GameplayCue.Weapon.Fire response: fire montage, muzzle flash, fire sound - this weapon's own cosmetic reaction to being fired. Returns true (handled) unconditionally; nothing about a weapon not currently having a montage/effect/sound assigned makes this cue "unhandled". */
	bool HandleWeaponFire(const FGameplayCueParameters& Parameters);

	/** GameplayCue.Weapon.Reload response. Not yet wired to a reload ability - currently a no-op placeholder so the tag has somewhere to go once one exists. */
	bool HandleWeaponReload(const FGameplayCueParameters& Parameters);

	/**
	 * Persistent, pre-attached muzzle flash components (Phase 3.b). Dormant
	 * (bAutoActivate = false) until HandleWeaponFire re-triggers them -
	 * replaces spawning/destroying a fresh UNiagaraComponent on every shot,
	 * which matters once automatic/charged weapons start re-triggering this
	 * many times a second. Asset, socket, and owner-visibility flags are all
	 * resolved once, in ApplyMuzzleFlashSocket via ApplyWeaponDefinition, not
	 * per shot. May end up with no asset assigned (and therefore inactive)
	 * if the weapon definition has no muzzle flash effect or the socket
	 * doesn't exist on that mesh.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation|Fire Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> MuzzleFlashComponentFirstPerson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation|Fire Cue", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> MuzzleFlashComponentThirdPerson;

};
