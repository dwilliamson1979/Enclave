// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "GameplayCues/Interfaces/EnclaveGameplayCueResponderInterface.h"
#include "Abilities/Interfaces/EnclaveFireableInterface.h"
#include "EnclaveWeapon.generated.h"

class UEnclaveWeaponDefinition;
class UNiagaraComponent;
class UGameplayEffect;

/**
 * Base class for wieldable weapons.
 *
 * Defines common weapon representation and translates generic wieldable
 * actions into weapon-specific behavior.
 *
 * Networking and detailed firing/reloading mechanics are intentionally left
 * to more specialized weapon implementations as those systems are introduced.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveWeapon : public AEnclaveItem, public IEnclaveWieldableInterface, public IEnclaveGameplayCueResponderInterface, public IEnclaveFireableInterface
{
	GENERATED_BODY()

public:

	AEnclaveWeapon();

	virtual FGameplayTag GetItemType() const override;
	virtual TArray<TSubclassOf<UGameplayAbility>> GetGrantedAbilityClasses() const override;

	// IEnclaveWieldableInterface
	virtual bool HandlePrimaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;
	virtual bool HandleSecondaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;
	virtual bool HandleTertiaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;

	// IEnclaveGameplayCueResponderInterface
	/** Dispatches by exact tag match to this weapon's own HandleWeaponFire/HandleWeaponReload/etc. Logs and returns false for a tag this weapon doesn't recognize, rather than silently doing nothing. */
	virtual bool HandleGameplayCue(FGameplayTag CueTag, const FGameplayCueParameters& Parameters) override;

	// IEnclaveFireableInterface - this weapon's contract with UEnclaveGA_Fire.
	// Everything here forwards to WeaponDefinition (the actual data/config
	// store); PerformFireTrace also owns the line trace itself, including its
	// own debug visualization, since "how does this weapon aim" is entirely
	// this class's concern now, not the ability's.
	virtual TArray<FEnclaveHitResult> PerformFireTrace(const FVector& ViewLocation, const FRotator& ViewRotation, AActor* InstigatorToIgnore) const override;
	virtual TSubclassOf<UGameplayEffect> GetFireCooldownEffectClass() const override;

	const UEnclaveWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

protected:

	virtual void BeginPlay() override;

	virtual bool TryPrimaryAction(const FEnclaveInputEvent& InputEvent);
	virtual bool TrySecondaryAction(const FEnclaveInputEvent& InputEvent);
	virtual bool TryTertiaryAction(const FEnclaveInputEvent& InputEvent);

private:

	void ApplyWeaponDefinition();

	/** Assigns the muzzle flash asset to MuzzleFlashComponent and snaps it onto MeshComp's muzzle socket, matching MeshComp's own owner-visibility flags. Leaves the component with no asset (inactive) if InWeaponDefinition has no muzzle flash effect or the socket doesn't exist on MeshComp. */
	void ApplyMuzzleFlashSocket(UNiagaraComponent* MuzzleFlashComponent, USkeletalMeshComponent* MeshComp, const UEnclaveWeaponDefinition* InWeaponDefinition);

	/** GameplayCue.Weapon.Fire response: fire montage, muzzle flash, fire sound - this weapon's own cosmetic reaction to being fired, forwarded here by whatever Blueprint child of UEnclaveGameplayCueNotify is registered for that tag (see IEnclaveGameplayCueResponderInterface). Returns true (handled) unconditionally; nothing about a weapon not currently having a montage/effect/sound assigned makes this cue "unhandled". */
	bool HandleWeaponFire(const FGameplayCueParameters& Parameters);

	/** GameplayCue.Weapon.Reload response. Not yet wired to a reload ability - currently a no-op placeholder so the tag has somewhere to go once one exists. */
	bool HandleWeaponReload(const FGameplayCueParameters& Parameters);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveWeaponDefinition> WeaponDefinition;

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