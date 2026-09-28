// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EnclaveWeapon.h"
#include "GameplayCues/Interfaces/EnclaveGameplayCueResponderInterface.h"
#include "Abilities/Interfaces/EnclaveFireableInterface.h"
#include "Abilities/Interfaces/EnclaveAmmoConsumerInterface.h"
#include "Abilities/Interfaces/EnclaveReloadableInterface.h"
#include "GameplayPrediction.h"
#include "EnclaveFirearmWeapon.generated.h"

class UEnclaveFirearmWeaponDefinition;

/**
 * The magazine as the server knows it, replicated as one unit so the owning
 * client always receives a round count together with the newest prediction
 * key that count already accounts for. That pairing is what makes
 * client-side ammo prediction reconcile exactly: any predicted change whose
 * key is at or before LastAcknowledgedPredictionKey is already reflected in
 * Rounds and can be dropped, all in the same OnRep, with no window where
 * the two disagree. (A separate key channel - the ASC's own prediction-key
 * catch-up, which replicates on the PlayerState - can arrive before or
 * after the weapon's update, and would briefly double-count or un-count a
 * shot.)
 */
USTRUCT()
struct FEnclaveMagazineState
{
	GENERATED_BODY()

	/** Rounds in the magazine. Server truth. */
	UPROPERTY()
	int32 Rounds = 0;

	/**
	 * Newest owning-client prediction key (FPredictionKey::Current) whose
	 * ammo effect - a spend, a refill, or "processed, nothing spent" - is
	 * already reflected in Rounds. 0 = none yet. Keys are only meaningful
	 * to the client that generated them, so this must be reset to 0 if the
	 * weapon ever changes hands (future pickup/drop).
	 */
	UPROPERTY()
	int16 LastAcknowledgedPredictionKey = 0;
};

/** Owning client only: one ammo change it predicted that the server hasn't acknowledged yet. */
struct FEnclavePendingAmmoPrediction
{
	FPredictionKey::KeyType PredictionKey = 0;

	/** Negative for a predicted spend, positive for a predicted refill. */
	int32 RoundsDelta = 0;
};
class UNiagaraComponent;
class USkeletalMeshComponent;
class UGameplayEffect;
class UAnimMontage;
class USoundBase;

/**
 * Hitscan/ranged weapon subclass of AEnclaveWeapon.
 *
 * Owns everything that only makes sense for something that fires:
 * - IEnclaveFireableInterface, this weapon's contract with UEnclaveGA_Fire
 *   (PerformFireHitDetection owns the line trace itself, including its own
 *   debug visualization and its own choice of aim source; GetFireCooldownEffectClass
 *   exposes this weapon's fire-rate cooldown GE).
 * - IEnclaveGameplayCueResponderInterface, dispatching GameplayCue.Weapon.Fire,
 *   .DryFire and .Reload to this weapon's own cosmetic response.
 * - IEnclaveAmmoConsumerInterface/IEnclaveReloadableInterface (Phase 4), this
 *   weapon's contracts with UEnclaveGA_Fire's ammo cost and UEnclaveGA_Reload.
 *   Ammo is client-predicted: the owning client applies its own spends and
 *   refills immediately (PendingAmmoPredictions) on top of the server's
 *   replicated MagazineState, so it never predicts a shot past its own last
 *   round, and its HUD count never lags. The server stays the only
 *   authority; see FEnclaveMagazineState for how the two reconcile.
 *   The magazine lives here, on the weapon actor, not in an Attribute Set:
 *   the ASC is on the PlayerState and holds one set of numbers per player,
 *   while every firearm has its own magazine - and a weapon dropped on death
 *   should take its magazine with it.
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
class ENCLAVE_API AEnclaveFirearmWeapon : public AEnclaveWeapon, public IEnclaveGameplayCueResponderInterface, public IEnclaveFireableInterface,
	public IEnclaveAmmoConsumerInterface, public IEnclaveReloadableInterface
{
	GENERATED_BODY()

public:

	AEnclaveFirearmWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IEnclaveGameplayCueResponderInterface
	/** Dispatches by exact tag match to this weapon's own HandleWeaponFire/HandleWeaponDryFire/HandleWeaponReload. Logs and returns false for a tag this weapon doesn't recognize, rather than silently doing nothing. */
	virtual bool HandleGameplayCue(FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	// IEnclaveFireableInterface - this weapon's contract with UEnclaveGA_Fire.
	// PerformFireHitDetection is handed only InstigatorToIgnore - it resolves
	// its own aim source (GetActorEyesViewPoint) rather than being told one,
	// so a future non-eye-based Fireable isn't forced into the same contract.
	virtual TArray<FEnclaveHitResult> PerformFireHitDetection(AActor* InstigatorToIgnore) const override;
	virtual TSubclassOf<UGameplayEffect> GetFireCooldownEffectClass() const override;

	// IEnclaveAmmoConsumerInterface - UEnclaveGA_Fire's ammo cost.
	virtual bool CanConsumeAmmo() const override;
	virtual void ConsumeAmmo(const FPredictionKey& PredictionKey) override;
	virtual void AcknowledgeAmmoPrediction(const FPredictionKey& PredictionKey) override;

	// IEnclaveReloadableInterface - UEnclaveGA_Reload's contract.
	virtual bool CanReload() const override;
	virtual float GetReloadDuration() const override;
	virtual void CompleteReload(const FPredictionKey& PredictionKey) override;
	virtual void PredictReloadComplete(const FPredictionKey& PredictionKey) override;

	/**
	 * Rounds in the magazine as this machine should treat them: the
	 * server's count plus, on the owning client, every predicted change the
	 * server hasn't acknowledged yet. What activation checks use, and what
	 * a HUD ammo counter should bind to.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetCurrentMagazineAmmo() const;

protected:

	/** Server fills the magazine from the definition's MagazineCapacity - a freshly spawned weapon (loadout, respawn) starts full. */
	virtual void BeginPlay() override;

	virtual void ApplyWeaponDefinition() override;

private:

	/** WeaponDefinition, cast to this weapon's actual definition subtype. Null if this actor was misconfigured with a base (non-firearm) UEnclaveWeaponDefinition. */
	const UEnclaveFirearmWeaponDefinition* GetFirearmWeaponDefinition() const;

	/** Assigns the muzzle flash asset to MuzzleFlashComponent and snaps it onto MeshComp's muzzle socket, matching MeshComp's own owner-visibility flags. Leaves the component with no asset (inactive) if InWeaponDefinition has no muzzle flash effect or the socket doesn't exist on MeshComp. */
	void ApplyMuzzleFlashSocket(UNiagaraComponent* MuzzleFlashComponent, USkeletalMeshComponent* MeshComp, const UEnclaveFirearmWeaponDefinition* InWeaponDefinition);

	/** GameplayCue.Weapon.Fire response: fire montage, muzzle flash, fire sound - this weapon's own cosmetic reaction to being fired. Returns true (handled) unconditionally; nothing about a weapon not currently having a montage/effect/sound assigned makes this cue "unhandled". */
	bool HandleWeaponFire(const FGameplayCueParameters& Parameters);

	/** GameplayCue.Weapon.DryFire response: dry-fire click from the muzzle, and tells the wielder (Wielder.Action.DryFire). */
	bool HandleWeaponDryFire(const FGameplayCueParameters& Parameters);

	/**
	 * GameplayCue.Weapon.Reload response (a duration cue, added/removed by
	 * UEnclaveGA_Reload). WhileActive starts the weapon-mesh reload montage
	 * (play-rate-scaled so it ends when the reload does -
	 * Parameters.RawMagnitude carries the duration the ability is waiting
	 * on) and tells the wielder Wielder.Action.Reload Started with that
	 * duration; Removed stops the montage and tells the wielder Ended.
	 */
	bool HandleWeaponReload(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	/** Plays Montage on both weapon meshes' anim instances. Which one a given viewer sees is the meshes' own owner-visibility setup. */
	void PlayMontageOnWeaponMeshes(UAnimMontage* Montage, float PlayRate = 1.0f);

	/** Stops Montage on both weapon meshes if it's still playing there. No-op for a montage that already finished. */
	void StopMontageOnWeaponMeshes(UAnimMontage* Montage, float BlendOutTime);

	/** Plays Sound attached at the muzzle socket, preferring the third-person mesh (the one representing the weapon's world position for every viewer). */
	void PlaySoundAtMuzzle(USoundBase* Sound);

	/**
	 * Reserve-ammo seam: returns how many of Requested rounds are actually
	 * available to load, removing them from the reserve. Returns Requested
	 * unconditionally for now - reserve is infinite until the shared
	 * per-ammo-type pool exists. That pool will live with the pawn's
	 * inventory (it drops on death and resets on respawn, same as the
	 * weapons), and this becomes the one place that asks it for rounds.
	 */
	int32 TakeReserveAmmo(int32 Requested);

	/** Server only: records that PredictionKey's ammo effect is now in MagazineState.Rounds. Ignores invalid and server-initiated keys (no client is waiting on those). */
	void AcknowledgePredictionKey(const FPredictionKey& PredictionKey);

	/** Owning client only: records a predicted change under PredictionKey, unless the server has already acknowledged that key. bSettleOnRejection binds the key's rejection delegate so a shot the server never ran is undone. */
	void AddPendingAmmoPrediction(const FPredictionKey& PredictionKey, int32 RoundsDelta, bool bSettleOnRejection);

	/** Owning client: the server refused to run the activation for Key at all - undo whatever was predicted under it. */
	void HandleAmmoPredictionRejected(FPredictionKey::KeyType Key);

	/** True if Key is at or before AcknowledgedKey in prediction-key order (keys count up from 1 and wrap back to 1 after 32767). False while nothing has been acknowledged. */
	static bool IsPredictionKeyAcknowledged(FPredictionKey::KeyType Key, FPredictionKey::KeyType AcknowledgedKey);

	/** Drops every pending prediction the newly replicated MagazineState already accounts for. */
	UFUNCTION()
	void OnRep_MagazineState();

	/** Logs/prints the magazine count when Enclave.Debug.Ammo is on - lets PIE verify ammo flow before any HUD exists (same pattern as Enclave.Debug.HitMarker). */
	void DebugLogAmmo(const TCHAR* Context) const;

	/**
	 * Server truth for the magazine (see FEnclaveMagazineState). Only
	 * ConsumeAmmo/AcknowledgeAmmoPrediction/CompleteReload write it, all
	 * authority-gated. Not owner-only: a spectator HUD or a dropped
	 * weapon's pickup prompt may want the count too.
	 */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_MagazineState, Category = "Weapon|Ammo")
	FEnclaveMagazineState MagazineState;

	/** Owning client only, never replicated: predicted changes awaiting the server's acknowledgement, oldest first. Always empty on the server and on other clients. */
	TArray<FEnclavePendingAmmoPrediction> PendingAmmoPredictions;

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
