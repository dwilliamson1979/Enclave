// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveFirearmWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Equipment/Weapons/Data/EnclaveFirearmWeaponDefinition.h"
#include "GameplayTags/EnclavePresentationTags.h"
#include "Collision/EnclaveCollisionChannels.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Debug/EnclaveDebug.h"

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

	DOREPLIFETIME(AEnclaveFirearmWeapon, MagazineState);
}

void AEnclaveFirearmWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
		MagazineState.Rounds = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetMagazineCapacity() : 0;
		MagazineState.LastAcknowledgedPredictionKey = 0;
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

void AEnclaveFirearmWeapon::StartPresentationAction(const FGameplayTag& Action, float Duration)
{
	if (Action.MatchesTagExact(EnclaveGameplayTags::Presentation_Item_Action_Fire))
	{
		HandleWeaponFire();
		return;
	}

	if (Action.MatchesTagExact(EnclaveGameplayTags::Presentation_Item_Action_DryFire))
	{
		HandleWeaponDryFire();
		return;
	}

	if (Action.MatchesTagExact(EnclaveGameplayTags::Presentation_Item_Action_Reload))
	{
		StartWeaponReloadPresentation(Duration);
	}
}

void AEnclaveFirearmWeapon::EndPresentationAction(const FGameplayTag& Action)
{
	if (Action.MatchesTagExact(EnclaveGameplayTags::Presentation_Item_Action_Reload))
	{
		EndWeaponReloadPresentation();
	}
}

void AEnclaveFirearmWeapon::HandleWeaponFire()
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition)
	{
		return;
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
}

void AEnclaveFirearmWeapon::HandleWeaponDryFire()
{
	if (const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition())
	{
		PlaySoundAtMuzzle(FirearmWeaponDefinition->GetDryFireSound());
	}
}

void AEnclaveFirearmWeapon::StartWeaponReloadPresentation(float Duration)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	UAnimMontage* ReloadMontage = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetReloadMontage() : nullptr;

	if (!ReloadMontage)
	{
		return;
	}

	// Duration is the remaining presentation window supplied by
	// AvatarPresentation when the avatar montage reaches its authored item
	// choreography notify. Gameplay still owns the overall reload duration;
	// this montage bends to finish inside the remaining window.
	const float MontageLength = ReloadMontage->GetPlayLength();
	const float PlayRate = (Duration > KINDA_SMALL_NUMBER && MontageLength > KINDA_SMALL_NUMBER) ? MontageLength / Duration : 1.0f;

	PlayMontageOnWeaponMeshes(ReloadMontage, PlayRate);
}

void AEnclaveFirearmWeapon::EndWeaponReloadPresentation()
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	UAnimMontage* ReloadMontage = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetReloadMontage() : nullptr;

	// Completed or cancelled alike. Only visible for a cancelled reload - a
	// completed montage should already have finished because it was scaled to
	// the gameplay duration. Stopping a finished montage is a no-op.
	StopMontageOnWeaponMeshes(ReloadMontage, /*BlendOutTime=*/ 0.1f);
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

		const FString BoneName = HitResult.BoneName.IsNone() ? TEXT("None") : HitResult.BoneName.ToString();

		ENCLAVE_NET_LOG(
			LogEnclaveWeapon,
			Log,
			this,
			FString::Printf(
				TEXT("fire trace hit=%s component=%s bone=%s"),
				*GetNameSafe(HitResult.GetActor()),
				*GetNameSafe(HitResult.GetComponent()),
				*BoneName));
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

int32 AEnclaveFirearmWeapon::GetCurrentMagazineAmmo() const
{
	int32 Rounds = MagazineState.Rounds;

	// Empty everywhere except the owning client between predicting a change
	// and the server acknowledging it.
	for (const FEnclavePendingAmmoPrediction& Pending : PendingAmmoPredictions)
	{
		Rounds += Pending.RoundsDelta;
	}

	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	const int32 Capacity = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetMagazineCapacity() : 0;

	return Capacity > 0 ? FMath::Clamp(Rounds, 0, Capacity) : FMath::Max(0, Rounds);
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

	// Predicted count on the owning client - the whole point: it already
	// includes shots the server hasn't answered yet, so the client can't
	// predict a shot past its own last round.
	return GetCurrentMagazineAmmo() >= FirearmWeaponDefinition->GetAmmoPerShot();
}

void AEnclaveFirearmWeapon::ConsumeAmmo(const FPredictionKey& PredictionKey)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition || FirearmWeaponDefinition->GetMagazineCapacity() <= 0)
	{
		return;
	}

	const int32 AmmoPerShot = FirearmWeaponDefinition->GetAmmoPerShot();

	if (HasAuthority())
	{
		// The real spend, stamped with the key of the shot it belongs to so
		// the owning client can drop its matching prediction in the same
		// replication update that delivers the new count.
		MagazineState.Rounds = FMath::Max(0, MagazineState.Rounds - AmmoPerShot);
		AcknowledgePredictionKey(PredictionKey);
		DebugLogAmmo(TEXT("Fire"));
		return;
	}

	// Owning client, inside its predicted activation. Settle on rejection
	// too: if the server refuses to run this activation at all, no
	// acknowledgement for this key will ever come.
	AddPendingAmmoPrediction(PredictionKey, -AmmoPerShot, /*bSettleOnRejection=*/ true);
	DebugLogAmmo(TEXT("Fire (predicted)"));
}

void AEnclaveFirearmWeapon::AcknowledgeAmmoPrediction(const FPredictionKey& PredictionKey)
{
	if (!HasAuthority())
	{
		return;
	}

	AcknowledgePredictionKey(PredictionKey);
	DebugLogAmmo(TEXT("Acknowledged (nothing spent)"));
}

bool AEnclaveFirearmWeapon::CanReload() const
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!FirearmWeaponDefinition || FirearmWeaponDefinition->GetMagazineCapacity() <= 0)
	{
		return false;
	}

	// Once the reserve pool exists this also needs "and there's reserve
	// ammo to load". Predicted count on the owning client, same as
	// CanConsumeAmmo.
	return GetCurrentMagazineAmmo() < FirearmWeaponDefinition->GetMagazineCapacity();
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

void AEnclaveFirearmWeapon::CompleteReload(const FPredictionKey& PredictionKey)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (!HasAuthority() || !FirearmWeaponDefinition)
	{
		return;
	}

	const int32 RoundsNeeded = FirearmWeaponDefinition->GetMagazineCapacity() - MagazineState.Rounds;

	if (RoundsNeeded > 0)
	{
		MagazineState.Rounds += TakeReserveAmmo(RoundsNeeded);
	}

	// Acknowledged even if nothing was needed, so the client's predicted
	// refill for this key always settles. The reload's key is newer than
	// every shot fired before it, so this also sweeps up any older shot
	// prediction still pending on the client.
	AcknowledgePredictionKey(PredictionKey);
	DebugLogAmmo(TEXT("Reload"));
}

void AEnclaveFirearmWeapon::PredictReloadComplete(const FPredictionKey& PredictionKey)
{
	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();

	if (HasAuthority() || !FirearmWeaponDefinition)
	{
		return;
	}

	// Enough to bring the predicted count to full. Once the reserve pool
	// exists, clamp this to the reserve the client knows about - the server
	// corrects any difference when it acknowledges the key.
	const int32 RoundsToFull = FirearmWeaponDefinition->GetMagazineCapacity() - GetCurrentMagazineAmmo();

	if (RoundsToFull <= 0)
	{
		return;
	}

	// No rejection binding: the reload ability only calls this after the
	// server has already ended the reload successfully, so the key can no
	// longer be rejected.
	AddPendingAmmoPrediction(PredictionKey, RoundsToFull, /*bSettleOnRejection=*/ false);
	DebugLogAmmo(TEXT("Reload (predicted)"));
}

void AEnclaveFirearmWeapon::AcknowledgePredictionKey(const FPredictionKey& PredictionKey)
{
	// Invalid: nothing was predicted (e.g. a listen-server host's own
	// activation). Server-initiated: generated by the server, so no client
	// has a pending prediction under it - and its value comes from a
	// different counter, which would corrupt the ordering below.
	if (!PredictionKey.IsValidKey() || PredictionKey.IsServerInitiatedKey())
	{
		return;
	}

	// Only ever move forward. Activations are processed in the order the
	// client made them, so this is normally every key in turn, but never
	// let an older one pull the acknowledgement backwards.
	if (!IsPredictionKeyAcknowledged(PredictionKey.Current, MagazineState.LastAcknowledgedPredictionKey))
	{
		MagazineState.LastAcknowledgedPredictionKey = PredictionKey.Current;
	}
}

void AEnclaveFirearmWeapon::AddPendingAmmoPrediction(const FPredictionKey& PredictionKey, int32 RoundsDelta, bool bSettleOnRejection)
{
	if (!PredictionKey.IsValidKey() || RoundsDelta == 0)
	{
		return;
	}

	// The server's answer for this key already arrived (possible for a
	// predicted refill, which is only recorded once the reload has ended):
	// its effect is already in MagazineState.Rounds, so recording it again
	// would count it twice.
	if (IsPredictionKeyAcknowledged(PredictionKey.Current, MagazineState.LastAcknowledgedPredictionKey))
	{
		return;
	}

	PendingAmmoPredictions.Add({ PredictionKey.Current, RoundsDelta });

	if (bSettleOnRejection)
	{
		// Rejection is broadcast when the server refuses to run the
		// activation at all (ClientActivateAbilityFailed). A copy is fine -
		// the delegate is keyed on the key's value, not this instance.
		FPredictionKey KeyCopy = PredictionKey;
		KeyCopy.NewRejectedDelegate().BindUObject(this, &AEnclaveFirearmWeapon::HandleAmmoPredictionRejected, PredictionKey.Current);
	}
}

void AEnclaveFirearmWeapon::HandleAmmoPredictionRejected(FPredictionKey::KeyType Key)
{
	const int32 NumRemoved = PendingAmmoPredictions.RemoveAll([Key](const FEnclavePendingAmmoPrediction& Pending)
	{
		return Pending.PredictionKey == Key;
	});

	if (NumRemoved > 0)
	{
		DebugLogAmmo(TEXT("Prediction rejected"));
	}
}

bool AEnclaveFirearmWeapon::IsPredictionKeyAcknowledged(FPredictionKey::KeyType Key, FPredictionKey::KeyType AcknowledgedKey)
{
	if (AcknowledgedKey <= 0 || Key <= 0)
	{
		return false;
	}

	// Keys run 1..32767 and then wrap back to 1, so compare by distance
	// around that cycle rather than raw value: a key more than half a cycle
	// "ahead" is really behind (it's from before the wrap). A client can
	// never have anywhere near half a cycle of shots in flight, so this is
	// unambiguous in practice.
	constexpr int32 KeyCycle = 32767;
	int32 Distance = static_cast<int32>(AcknowledgedKey) - static_cast<int32>(Key);

	if (Distance > KeyCycle / 2)
	{
		Distance -= KeyCycle;
	}
	else if (Distance < -KeyCycle / 2)
	{
		Distance += KeyCycle;
	}

	return Distance >= 0;
}

int32 AEnclaveFirearmWeapon::TakeReserveAmmo(int32 Requested)
{
	// Infinite reserve for now - see the header comment for where the
	// shared pool will plug in.
	return FMath::Max(0, Requested);
}

void AEnclaveFirearmWeapon::OnRep_MagazineState()
{
	// Rounds and the acknowledgement arrive together (one struct), so every
	// prediction dropped here is exactly one already counted in the new
	// Rounds - the predicted total doesn't jump.
	const FPredictionKey::KeyType AcknowledgedKey = MagazineState.LastAcknowledgedPredictionKey;

	PendingAmmoPredictions.RemoveAll([AcknowledgedKey](const FEnclavePendingAmmoPrediction& Pending)
	{
		return IsPredictionKeyAcknowledged(Pending.PredictionKey, AcknowledgedKey);
	});

	DebugLogAmmo(TEXT("Replicated"));
}

void AEnclaveFirearmWeapon::DebugLogAmmo(const TCHAR* Context) const
{
	// Whole body compiled out of Test/Shipping - the CVar still exists
	// there, but there is nothing to print.
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!CVarDebugAmmo.GetValueOnGameThread())
	{
		return;
	}

	const UEnclaveFirearmWeaponDefinition* FirearmWeaponDefinition = GetFirearmWeaponDefinition();
	const int32 Capacity = FirearmWeaponDefinition ? FirearmWeaponDefinition->GetMagazineCapacity() : 0;

	// Effective count first; server count and pending-prediction count
	// show the reconciliation at work on the owning client.
	// Net mode rather than HasAuthority(): distinguishes ListenServer /
	// DedicatedServer / Client, and doesn't label Standalone as "Server".
	const FString Message = FString::Printf(TEXT("[%s] %s %s: %d / %d (server %d, pending %d, ack key %d)"),
		*EnclaveDebug::GetNetModeString(this), *GetName(), Context,
		GetCurrentMagazineAmmo(), Capacity, MagazineState.Rounds, PendingAmmoPredictions.Num(),
		static_cast<int32>(MagazineState.LastAcknowledgedPredictionKey));

	ENCLAVE_LOG_MSG(LogEnclaveWeapon, Log, Message);
	ENCLAVE_SCREEN(Message, FColor::Yellow, 2.0f);
#endif
}
