// Copyright Lightcarrier Studios

#include "GameplayCues/EnclaveGameplayCueNotify_WeaponFire.h"

#include "GameplayTags/EnclaveGameplayCueTags.h"

#include "Equipment/Weapons/EnclaveWeapon.h"
#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

UEnclaveGameplayCueNotify_WeaponFire::UEnclaveGameplayCueNotify_WeaponFire()
{
	GameplayCueTag = EnclaveGameplayTags::GameplayCue_Weapon_Fire;
}

bool UEnclaveGameplayCueNotify_WeaponFire::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const AEnclaveWeapon* Weapon = ResolveFiringWeapon(MyTarget, Parameters);

	if (!IsValid(Weapon))
	{
		return false;
	}

	PlayFireMontage(*Weapon);
	SpawnMuzzleFlash(*Weapon);
	PlayFireSound(*Weapon);

	return false;
}

const AEnclaveWeapon* UEnclaveGameplayCueNotify_WeaponFire::ResolveFiringWeapon(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (const AEnclaveWeapon* Weapon = Cast<AEnclaveWeapon>(Parameters.EffectCauser))
	{
		return Weapon;
	}

	if (const AEnclaveWeapon* Weapon = Cast<AEnclaveWeapon>(Parameters.SourceObject))
	{
		return Weapon;
	}

	// Fallback only - should not be hit given how UEnclaveGA_Fire builds the
	// cue parameters, but avoids silently doing nothing if that ever changes.
	return Cast<AEnclaveWeapon>(MyTarget);
}

void UEnclaveGameplayCueNotify_WeaponFire::PlayFireMontage(const AEnclaveWeapon& Weapon) const
{
	const UEnclaveWeaponDefinition* WeaponDefinition = Weapon.GetWeaponDefinition();
	UAnimMontage* FireMontage = WeaponDefinition ? WeaponDefinition->GetFireMontage() : nullptr;

	if (!FireMontage)
	{
		return;
	}

	// Plays on both meshes - which one is actually visible to a given viewer
	// (first-person for the owning client, third-person for everyone else)
	// is a presentation concern for the character/weapon's mesh visibility
	// setup, not this notify's.
	for (USkeletalMeshComponent* MeshComp : { Weapon.GetFirstPersonMesh(), Weapon.GetThirdPersonMesh() })
	{
		if (!IsValid(MeshComp))
		{
			continue;
		}

		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Play(FireMontage);
		}
	}
}

void UEnclaveGameplayCueNotify_WeaponFire::SpawnMuzzleFlash(const AEnclaveWeapon& Weapon) const
{
	const UEnclaveWeaponDefinition* WeaponDefinition = Weapon.GetWeaponDefinition();

	if (!WeaponDefinition || !WeaponDefinition->GetMuzzleFlashEffect())
	{
		return;
	}

	const FName MuzzleSocketName = WeaponDefinition->GetMuzzleSocketName();

	// First-person and third-person meshes are two separate components on
	// the same weapon actor (see AEnclaveItem) - each visible to a different
	// set of viewers, so the effect is spawned on both rather than picking
	// just one.
	for (USkeletalMeshComponent* MeshComp : { Weapon.GetFirstPersonMesh(), Weapon.GetThirdPersonMesh() })
	{
		if (!IsValid(MeshComp) || !MeshComp->DoesSocketExist(MuzzleSocketName))
		{
			continue;
		}

		UNiagaraComponent* MuzzleFlashComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			WeaponDefinition->GetMuzzleFlashEffect(),
			MeshComp,
			MuzzleSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			/*bAutoDestroy=*/ true);

		if (IsValid(MuzzleFlashComponent))
		{
			// A spawned component does NOT inherit its parent mesh's owner-
			// visibility flags just by being attached to it - SetOnlyOwnerSee/
			// SetOwnerNoSee are per-primitive-component render flags, not
			// something that cascades down the attachment hierarchy. Without
			// this, both flashes are visible to every viewer regardless of
			// which mesh they're attached to, defeating the whole point of
			// AEnclaveItem's per-mesh visibility split (FirstPersonMesh:
			// OnlyOwnerSee; ThirdPersonMesh: OwnerNoSee). Mirror whichever
			// mesh this instance is attached to.
			const bool bIsFirstPersonMesh = (MeshComp == Weapon.GetFirstPersonMesh());
			MuzzleFlashComponent->SetOnlyOwnerSee(bIsFirstPersonMesh);
			MuzzleFlashComponent->SetOwnerNoSee(!bIsFirstPersonMesh);

			// Some purchased/authored Niagara systems (this one included) are
			// built to be re-triggered rather than to emit on spawn - their
			// Emitter Spawn uses a Spawn Burst gated on a User-exposed
			// "Trigger" parameter instead of spawning continuously.
			// SpawnSystemAttached only creates the component; it has no idea
			// that parameter exists, so without this the system sits there
			// emitting nothing, silently, exactly like PlayFireMontage did
			// with a missing AnimInstance and this function did with a
			// missing socket. Harmless no-op if a particular effect doesn't
			// expose a "Trigger" parameter at all, and redundant (but still
			// harmless) now that the asset's own default was also set true.
			MuzzleFlashComponent->SetVariableBool(FName("Trigger"), true);
		}
	}
}

void UEnclaveGameplayCueNotify_WeaponFire::PlayFireSound(const AEnclaveWeapon& Weapon) const
{
	const UEnclaveWeaponDefinition* WeaponDefinition = Weapon.GetWeaponDefinition();

	if (!WeaponDefinition || !WeaponDefinition->GetFireSound())
	{
		return;
	}

	// Only needs to play once - unlike the montage and muzzle flash, sound
	// isn't a per-viewer visibility concern, so one 3D-attenuated source at
	// the weapon's location is enough. Prefers the third-person mesh since
	// it's the one guaranteed to represent the weapon's world position for
	// every viewer, including the owning client's own third-person body.
	USkeletalMeshComponent* AttachTarget = IsValid(Weapon.GetThirdPersonMesh()) ? Weapon.GetThirdPersonMesh() : Weapon.GetFirstPersonMesh();

	if (!IsValid(AttachTarget))
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		WeaponDefinition->GetFireSound(),
		AttachTarget,
		WeaponDefinition->GetMuzzleSocketName());
}
