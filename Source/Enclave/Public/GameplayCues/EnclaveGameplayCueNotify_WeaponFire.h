// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "EnclaveGameplayCueNotify_WeaponFire.generated.h"

class AEnclaveWeapon;

/**
 * Cosmetic response to GameplayCue.Weapon.Fire: fire montage, muzzle flash,
 * fire sound. Everyone with this actor relevant sees this - it is not
 * gated to the owning client (that gating belongs to the hit marker, which
 * is deliberately NOT a Gameplay Cue; see AEnclavePlayerController).
 *
 * Static, not Actor: stateless and one-shot, since it needs to fire once per
 * shot with nothing to persist or clean up between shots, including for
 * automatic weapons re-executing this every tick they're held.
 *
 * Resolves the firing weapon from the cue parameters (EffectCauser /
 * SourceObject, both set to the weapon by UEnclaveGA_Fire) rather than from
 * MyTarget: GAS resolves MyTarget to the ASC's avatar actor, which is the
 * character, not the weapon - and the presentation data this notify needs
 * (montage, muzzle effect, socket) lives on UEnclaveWeaponDefinition, which
 * only the weapon actor can resolve.
 *
 * To wire this up in the editor: create a Blueprint subclass of this class
 * under a folder listed in Project Settings > Gameplay Cue Notify Paths
 * (Config/DefaultEngine.ini already adds /Game/GameplayCues) - a native-only
 * class with no content asset is not picked up by the Gameplay Cue
 * Manager's asset scan. GameplayCueTag defaults to GameplayCue.Weapon.Fire
 * in the constructor below; leave it as-is unless you're intentionally
 * splitting fire cues per weapon type.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GCN Weapon Fire"))
class ENCLAVE_API UEnclaveGameplayCueNotify_WeaponFire : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UEnclaveGameplayCueNotify_WeaponFire();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

private:
	// Const throughout: this notify only ever reads presentation data off the
	// weapon. That also matches FGameplayCueParameters::SourceObject, which
	// is a TObjectPtr<const UObject> - EffectCauser is not const, so a single
	// const return type is what lets both fields resolve through the same
	// variable without a const_cast.
	static const AEnclaveWeapon* ResolveFiringWeapon(AActor* MyTarget, const FGameplayCueParameters& Parameters);

	void PlayFireMontage(const AEnclaveWeapon& Weapon) const;
	void SpawnMuzzleFlash(const AEnclaveWeapon& Weapon) const;
	void PlayFireSound(const AEnclaveWeapon& Weapon) const;
};
