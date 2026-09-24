// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveFirearmWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Equipment/Weapons/Data/EnclaveFirearmWeaponDefinition.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"
#include "Collision/EnclaveCollisionChannels.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

// Debug visualization of "how did this weapon's shot travel" is this
// weapon's own trace behavior, not the ability's concern.
static TAutoConsoleVariable<bool> CVarDebugFireTrace(
	TEXT("Enclave.Debug.FireTrace"),
	false,
	TEXT("Draw debug lines and log hit results for a weapon's PerformFireHitDetection."),
	ECVF_Cheat);

// Magazine count visibility before a HUD exists - logs + on-screen print on
// every change, server side on write and client side on replication.
static TAutoConsoleVariable<bool> CVarDebugAmmo(
	TEXT("Enclave.Debug.Ammo"),
	false,
	TEXT("Log and print a firearm's magazine count whenever it changes (server write and client replication)."),
	ECVF_Cheat);

AEnclaveFirearmWeapon::AEnclaveFirearmWeapon()
{
	// Phase 3.b: persistent muzzle flash components, created once here
	// rather than spawned per-shot by HandleWeaponFire. Dormant until
	// re-triggered - bAutoActivate = false means no emission (and no
	// User.Trigger burst) happens just from existing in the world.
	// SetupAttachment alone (no socket yet) since FirstPersonMesh/
	// ThirdPersonMesh don't have their skeletal mesh assigned until
	// ApplyWeaponDefinition runs in BeginPlay - the actual socket attachment
	// and owner-visibility flags are set there too, once the definition (and
	// therefore the mesh/socket/effect) is known.
	MuzzleFlashComponentFirstPerson = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MuzzleFlashComponentFirstPerson"));
	MuzzleFlashComponentFirstPerson->SetupAttachment(FirstPersonMesh);
	MuzzleFlashComponentFirstPerson->SetAutoActivate(false);

	MuzzleFlashComponentThirdPerson = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MuzzleFlashComponentThirdPerson"));
	MuzzleFlashComponentThirdPerson->SetupAttachment(ThirdPersonMesh);
	MuzzleFlashComponentThirdPerson->SetAutoActivate(false);
}

void AEnclaveFirearmWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveFirearmWeapon, CurrentMagazineAmmo);
}

void AEnclaveFirearmWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
		CurrentMagazineAmmo = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetMagazineCapacity() : 0;
	}
}

const UEnclaveFirearmWeaponDefinition* AEnclaveFirearmWeapon::GetFirearmWeaponDefinition() const
{
	return Cast<UEnclaveFirearmWeaponDefinition>(GetWeaponDefinition());
}

void AEnclaveFirearmWeapon::ApplyWeaponDefinition()
{
	Super::ApplyWeaponDefinition();

	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition)
	{
		return;
	}

	ApplyMuzzleFlashSocket(MuzzleFlashComponentFirstPerson, FirstPersonMesh, FirearmWeaponDefinition);
	ApplyMuzzleFlashSocket(MuzzleFlashComponentThirdPerson, ThirdPersonMesh, FirearmWeaponDefinition);
}

void AEnclaveFirearmWeapon::ApplyMuzzleFlashSocket(UNiagaraComponent* MuzzleFlashComponent, USkeletalMeshComponent* MeshComp, const UEnclaveFirearmWeaponDefinition* InWeaponDefinition)
{
	if (!IsValid(MuzzleFlashComponent) || !IsValid(MeshComp) || !InWeaponDefinition || !InWeaponDefinition->GetMuzzleFlashEffect())
	{
		return;
	}

	const FName MuzzleSocketName = InWeaponDefinition->GetMuzzleSocketName();

	if (!MeshComp->DoesSocketExist(MuzzleSocketName))
	{
		return;
	}

	MuzzleFlashComponent->SetAsset(InWeaponDefinition->GetMuzzleFlashEffect());
	MuzzleFlashComponent->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, MuzzleSocketName);

	// A component attached like this does NOT inherit its parent mesh's
	// owner-visibility flags just from being attached to it -
	// SetOnlyOwnerSee/SetOwnerNoSee are per-primitive-component render
	// flags, not something that cascades down the attachment hierarchy (the
	// same lesson learned the hard way with the old spawn-per-shot version
	// of this code). Set once here, at setup time, rather than on every
	// single shot.
	const bool bIsFirstPersonMesh = (MeshComp == FirstPersonMesh);
	MuzzleFlashComponent->SetOnlyOwnerSee(bIsFirstPersonMesh);
	MuzzleFlashComponent->SetOwnerNoSee(!bIsFirstPersonMesh);
}

bool AEnclaveFirearmWeapon::HandleGameplayCue(FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	// Fire and DryFire are one-shot (executed) cues; any other event for
	// them is recognized-but-ignored rather than "unhandled".
	if (CueTag.MatchesTagExact(EnclaveGameplayTags::GameplayCue_Weapon_Fire))
	{
		return EventType == EGameplayCueEvent::Executed ? HandleWeaponFire(Parameters) : true;
	}

	if (CueTag.MatchesTagExact(EnclaveGameplayTags::GameplayCue_Weapon_DryFire))
	{
		return EventType == EGameplayCueEvent::Executed ? HandleWeaponDryFire(Parameters) : true;
	}

	if (CueTag.MatchesTagExact(EnclaveGameplayTags::GameplayCue_Weapon_Reload))
	{
		return HandleWeaponReload(EventType, Parameters);
	}

	// Not silent: a tag reaching here with no branch above means a cue was
	// registered (and is presumably executing against this weapon) without
	// this dispatch being updated to match - exactly the kind of "silently
	// does nothing" bug a missing AnimInstance or a missing socket already
	// cost real debugging time on elsewhere in this class's history.
	UE_LOG(LogTemp, Warning, TEXT("%s: no handler for gameplay cue %s"), *GetName(), *CueTag.ToString());
	return false;
}

bool AEnclaveFirearmWeapon::HandleWeaponFire(const FGameplayCueParameters& Parameters)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition)
	{
		return true;
	}

	// Fire montage - plays on both meshes; which one is actually visible to
	// a given viewer (first-person for the owning client, third-person for
	// everyone else) is this actor's own mesh-visibility setup, not this
	// method's concern.
	PlayMontageOnWeaponMeshes(FirearmWeaponDefinition->GetFireMontage());

	// Muzzle flash (Phase 3.b) - the components are persistent and
	// pre-attached (asset, socket, and owner-visibility flags all resolved
	// once by ApplyMuzzleFlashSocket when the weapon definition is applied),
	// so this just re-fires them instead of spawning/destroying a fresh
	// UNiagaraComponent every shot. IsValid + GetAsset also covers the "no
	// muzzle flash effect assigned, or socket doesn't exist" case, since
	// ApplyMuzzleFlashSocket leaves the component with no asset then.
	for (UNiagaraComponent* MuzzleFlashComponent : { MuzzleFlashComponentFirstPerson, MuzzleFlashComponentThirdPerson })
	{
		if (!IsValid(MuzzleFlashComponent) || !MuzzleFlashComponent->GetAsset())
		{
			continue;
		}

		// bReset = true restarts the system from scratch rather than just
		// resuming a possibly-still-active one - important for automatic
		// weapons re-triggering this every tick they're held, so each shot
		// gets its own clean burst instead of piling onto whatever the
		// previous shot's system was still doing.
		MuzzleFlashComponent->Activate(/*bReset=*/ true);

		// Some purchased/authored Niagara systems (this one included) are
		// built to be re-triggered rather than to emit on spawn/activate -
		// their Emitter Spawn uses a Spawn Burst gated on a User-exposed
		// "Trigger" parameter instead of spawning continuously. Harmless
		// no-op if a particular effect doesn't expose a "Trigger" parameter
		// at all.
		MuzzleFlashComponent->SetVariableBool(FName("Trigger"), true);
	}

	PlaySoundAtMuzzle(FirearmWeaponDefinition->GetFireSound());

	return true;
}

bool AEnclaveFirearmWeapon::HandleWeaponDryFire(const FGameplayCueParameters& Parameters)
{
	if (const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition())
	{
		PlaySoundAtMuzzle(FirearmWeaponDefinition->GetDryFireSound());
	}

	return true;
}

bool AEnclaveFirearmWeapon::HandleWeaponReload(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	UAnimMontage* ReloadMontage = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetReloadMontage() : nullptr;

	if (!ReloadMontage)
	{
		// Recognized, just nothing to show yet.
		return true;
	}

	switch (EventType)
	{
	case EGameplayCueEvent::WhileActive:
	{
		// WhileActive, not OnActive: every machine receives both for one
		// add, and only WhileActive also fires when the cue becomes relevant
		// mid-reload (see IEnclaveGameplayCueResponderInterface).
		//
		// The reload duration is the gameplay number; the montage bends to
		// fit it. RawMagnitude is what UEnclaveGA_Reload is actually waiting
		// on (GetReloadDuration() at activation, attachment modifiers
		// included), so a "hot-swap magazine" speeds the animation up with
		// no extra wiring.
		const float ReloadDuration = Parameters.RawMagnitude;
		const float MontageLength = ReloadMontage->GetPlayLength();
		const float PlayRate = (ReloadDuration > KINDA_SMALL_NUMBER && MontageLength > KINDA_SMALL_NUMBER) ? MontageLength / ReloadDuration : 1.0f;

		PlayMontageOnWeaponMeshes(ReloadMontage, PlayRate);

		// ---------------------------------------------------------------
		// Wielder (arms / body) reload animation - deliberately deferred.
		// The montage above only moves the gun itself. The first-person
		// arms and third-person body need their own montages
		// (FP_AM_<Weapon>_Reload / MM_<Weapon>_Reload), played on the
		// WIELDER's meshes. Planned shape, keeping the rule that a weapon
		// never casts to AEnclaveCharacter:
		//
		//   1. New struct FEnclaveWielderMontages { FirstPerson; ThirdPerson; }
		//      and a WielderReloadMontages field of that type on
		//      UEnclaveFirearmWeaponDefinition.
		//   2. IEnclaveItemWielderInterface (already how the wielder
		//      resolves attachment targets for an item) gains:
		//        virtual void PlayWieldedItemMontages(const FEnclaveWielderMontages& Montages, float PlayRate) = 0;
		//        virtual void StopWieldedItemMontages(const FEnclaveWielderMontages& Montages, float BlendOutTime) = 0;
		//      AEnclaveCharacter implements them against its own FP arms /
		//      TP body meshes.
		//   3. Here:
		//        if (IEnclaveItemWielderInterface* Wielder = Cast<IEnclaveItemWielderInterface>(GetAttachParentActor()))
		//        {
		//            Wielder->PlayWieldedItemMontages(FirearmWeaponDefinition->GetWielderReloadMontages(), PlayRate);
		//        }
		//      and the matching Stop call in the Removed case below.
		//   4. Same treatment later for the fire montage.
		// ---------------------------------------------------------------
		break;
	}

	case EGameplayCueEvent::Removed:
		// Only visible for a cancelled reload - a completed one's montage
		// has already finished (it was scaled to the duration), and
		// Montage_Stop on a finished montage is a no-op.
		StopMontageOnWeaponMeshes(ReloadMontage, /*BlendOutTime=*/ 0.1f);
		break;

	default:
		break;
	}

	return true;
}

void AEnclaveFirearmWeapon::PlayMontageOnWeaponMeshes(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		return;
	}

	for (USkeletalMeshComponent* MeshComp : { FirstPersonMesh, ThirdPersonMesh })
	{
		if (!IsValid(MeshComp))
		{
			continue;
		}

		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage, PlayRate);
		}
	}
}

void AEnclaveFirearmWeapon::StopMontageOnWeaponMeshes(UAnimMontage* Montage, float BlendOutTime)
{
	if (!Montage)
	{
		return;
	}

	for (USkeletalMeshComponent* MeshComp : { FirstPersonMesh, ThirdPersonMesh })
	{
		if (!IsValid(MeshComp))
		{
			continue;
		}

		UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();

		if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(BlendOutTime, Montage);
		}
	}
}

void AEnclaveFirearmWeapon::PlaySoundAtMuzzle(USoundBase* Sound)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!Sound || !FirearmWeaponDefinition)
	{
		return;
	}

	// Only needs to play once; not a per-viewer visibility concern like the
	// montage/muzzle flash. Prefers the third-person mesh since it's the one
	// guaranteed to represent the weapon's world position for every viewer,
	// including the owning client's own third-person body.
	USkeletalMeshComponent* AttachTarget = IsValid(ThirdPersonMesh) ? ThirdPersonMesh : FirstPersonMesh;

	if (IsValid(AttachTarget))
	{
		UGameplayStatics::SpawnSoundAttached(Sound, AttachTarget, FirearmWeaponDefinition->GetMuzzleSocketName());
	}
}

TArray<FEnclaveHitResult> AEnclaveFirearmWeapon::PerformFireHitDetection(AActor* InstigatorToIgnore) const
{
	TArray<FEnclaveHitResult> Results;

	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition || !IsValid(InstigatorToIgnore))
	{
		return Results;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return Results;
	}

	// Aim from the instigator's own eyes, not a muzzle socket - standard for
	// hitscan shooters, since a muzzle-origin trace can hit geometry the
	// crosshair is clearly not aimed at (corners, low walls) whenever the
	// gun model leads or trails the camera. Resolved here rather than being
	// handed in by the ability, so this weapon (or any other
	// IEnclaveFireableInterface implementation) decides its own aim source -
	// GetActorEyesViewPoint works for both a player-controlled pawn and an
	// AI-controlled one, but a future non-eye-based Fireable (a deployed
	// turret tracking its own target) is free to ignore InstigatorToIgnore's
	// eyes entirely and compute something else.
	FVector ViewLocation;
	FRotator ViewRotation;
	InstigatorToIgnore->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * FirearmWeaponDefinition->GetTraceRange());

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(InstigatorToIgnore);

	// Also ignore anything attached to the instigator (this weapon
	// included), since it's effectively part of the instigator for trace
	// purposes.
	TArray<AActor*> AttachedActors;
	InstigatorToIgnore->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, ViewLocation, TraceEnd, ECC_Weapon, QueryParams);

	if (CVarDebugFireTrace.GetValueOnGameThread())
	{
		DrawDebugLine(World, ViewLocation, bHit ? HitResult.ImpactPoint : TraceEnd, bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.0f);
		UE_LOG(LogTemp, Warning, TEXT("%s fire trace hit: %s"), *GetName(), *GetNameSafe(HitResult.GetActor()));
	}

	if (bHit)
	{
		// Damage and DamageEffectClass are both computed here, not read
		// separately by the ability - flat GetDamageAmount()/
		// GetDamageEffectClass() today, but this is the seam for per-hit
		// variation later (HitResult.BoneName for a headshot multiplier,
		// HitResult.Distance for falloff, a different effect class for a
		// different pellet) without touching UEnclaveGA_Fire at all.
		FEnclaveHitResult EnclaveHitResult(HitResult);
		EnclaveHitResult.Damage = FirearmWeaponDefinition->GetDamageAmount();
		EnclaveHitResult.DamageEffectClass = FirearmWeaponDefinition->GetDamageEffectClass();
		Results.Add(EnclaveHitResult);
	}

	return Results;
}

TSubclassOf<UGameplayEffect> AEnclaveFirearmWeapon::GetFireCooldownEffectClass() const
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	return FirearmWeaponDefinition ? FirearmWeaponDefinition->GetCooldownEffectClass() : nullptr;
}

bool AEnclaveFirearmWeapon::CanConsumeAmmo() const
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition)
	{
		return false;
	}

	// No magazine configured = never runs dry.
	if (FirearmWeaponDefinition->GetMagazineCapacity() <= 0)
	{
		return true;
	}

	return CurrentMagazineAmmo >= FirearmWeaponDefinition->GetAmmoPerShot();
}

void AEnclaveFirearmWeapon::ConsumeAmmo()
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!HasAuthority() || !FirearmWeaponDefinition || FirearmWeaponDefinition->GetMagazineCapacity() <= 0)
	{
		return;
	}

	CurrentMagazineAmmo = FMath::Max(0, CurrentMagazineAmmo - FirearmWeaponDefinition->GetAmmoPerShot());
	DebugLogAmmo(TEXT("Fire"));
}

bool AEnclaveFirearmWeapon::CanReload() const
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition || FirearmWeaponDefinition->GetMagazineCapacity() <= 0)
	{
		return false;
	}

	// Once the reserve pool exists this also needs "and there's reserve
	// ammo to load" - reading replicated state only, since this runs on the
	// predicting client too.
	return CurrentMagazineAmmo < FirearmWeaponDefinition->GetMagazineCapacity();
}

float AEnclaveFirearmWeapon::GetReloadDuration() const
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	// Modifier seam: a "hot-swap magazine" attachment or a perk scales the
	// definition's base duration here (e.g. multiply by the product of
	// every equipped attachment's ReloadDurationMultiplier) - the ability
	// and the reload cue both consume this value, so neither changes.
	return FirearmWeaponDefinition ? FMath::Max(0.0f, FirearmWeaponDefinition->GetReloadDuration()) : 0.0f;
}

void AEnclaveFirearmWeapon::CompleteReload()
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!HasAuthority() || !FirearmWeaponDefinition)
	{
		return;
	}

	const int32 RoundsNeeded = FirearmWeaponDefinition->GetMagazineCapacity() - CurrentMagazineAmmo;

	if (RoundsNeeded <= 0)
	{
		return;
	}

	CurrentMagazineAmmo += TakeReserveAmmo(RoundsNeeded);
	DebugLogAmmo(TEXT("Reload"));
}

int32 AEnclaveFirearmWeapon::TakeReserveAmmo(int32 Requested)
{
	// Infinite reserve for now - see the header comment for where the
	// shared pool will plug in.
	return FMath::Max(0, Requested);
}

void AEnclaveFirearmWeapon::OnRep_CurrentMagazineAmmo()
{
	DebugLogAmmo(TEXT("Replicated"));
}

void AEnclaveFirearmWeapon::DebugLogAmmo(const TCHAR* Context) const
{
	if (!CVarDebugAmmo.GetValueOnGameThread())
	{
		return;
	}

	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	const int32 Capacity = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetMagazineCapacity() : 0;
	const FString Message = FString::Printf(TEXT("[%s] %s %s: %d / %d"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"), *GetName(), Context, CurrentMagazineAmmo, Capacity);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, Message);
	}
}
