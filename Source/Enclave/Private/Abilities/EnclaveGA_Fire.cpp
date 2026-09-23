// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Fire.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"

#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"

#include "Collision/EnclaveCollisionChannels.h"

#include "Equipment/Weapons/EnclaveWeapon.h"
#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"
#include "Combat/Interfaces/EnclaveHitFeedbackInterface.h"

// EnclaveGA_Fire.cpp, near the top, after the includes

static TAutoConsoleVariable<bool> CVarDebugFireTrace(
	TEXT("Enclave.Debug.FireTrace"),
	false,
	TEXT("Draw debug lines and log hit results for the Fire ability's line trace."),
	ECVF_Cheat);

UEnclaveGA_Fire::UEnclaveGA_Fire()
{
}

void UEnclaveGA_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Cooldown or cost check failed (e.g. still on cooldown from the
		// last shot) - nothing fires, nothing traces.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// Unconditional and before the authority check: this runs on the
	// predicting owning client too, so the shot feels instant locally, while
	// GAS's cue replication recognizes the matching prediction key on the
	// server's own execution and does not double-play it there.
	TriggerFireCue(ActorInfo);

	if (HasAuthority(&ActivationInfo))
	{
		PerformTraceAndApplyDamage(Handle, ActorInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

UGameplayEffect* UEnclaveGA_Fire::GetCooldownGameplayEffect() const
{
	const AEnclaveWeapon* Weapon = ResolveWeapon();
	const UEnclaveWeaponDefinition* WeaponDefinition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	const TSubclassOf<UGameplayEffect> CooldownClass = WeaponDefinition ? WeaponDefinition->GetCooldownEffectClass() : nullptr;

	if (CooldownClass)
	{
		return CooldownClass->GetDefaultObject<UGameplayEffect>();
	}

	// No weapon resolved, or its Weapon Definition has no cooldown effect
	// configured - fall back to the base UGameplayAbility behavior
	// (CooldownGameplayEffectClass, which this ability leaves unset).
	return Super::GetCooldownGameplayEffect();
}

AEnclaveWeapon* UEnclaveGA_Fire::ResolveWeapon() const
{
	return Cast<AEnclaveWeapon>(GetCurrentSourceObject());
}

void UEnclaveGA_Fire::TriggerFireCue(const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AEnclaveWeapon* Weapon = ResolveWeapon();

	if (!IsValid(SourceASC) || !IsValid(Weapon))
	{
		return;
	}

	// EffectCauser (not just SourceObject) is set to the weapon because
	// UEnclaveGameplayCueNotify::ResolveCueResponder reads specifically off
	// EffectCauser (a non-const TWeakObjectPtr<AActor>) to find whatever
	// implements IEnclaveGameplayCueResponderInterface - SourceObject is
	// const and isn't used for that resolution.
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = Weapon;
	CueParameters.SourceObject = Weapon;

	SourceASC->ExecuteGameplayCue(EnclaveGameplayTags::GameplayCue_Weapon_Fire, CueParameters);
}

void UEnclaveGA_Fire::NotifyHitConfirmed(const FGameplayAbilityActorInfo* ActorInfo)
{
	APlayerController* PlayerController = ActorInfo ? Cast<APlayerController>(ActorInfo->PlayerController.Get()) : nullptr;

	if (!IsValid(PlayerController))
	{
		return;
	}

	// Depends only on the interface - never on AEnclavePlayerController or
	// any other concrete class that happens to implement it, matching how
	// UEnclaveEquipmentComponent finds its item source.
	IEnclaveHitFeedbackInterface* HitFeedback = Cast<IEnclaveHitFeedbackInterface>(
		PlayerController->FindComponentByInterface(UEnclaveHitFeedbackInterface::StaticClass()));

	if (HitFeedback)
	{
		HitFeedback->NotifyWeaponHitConfirmed();
	}
}

void UEnclaveGA_Fire::PerformTraceAndApplyDamage(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo)
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	APlayerController* PlayerController = ActorInfo ? Cast<APlayerController>(ActorInfo->PlayerController.Get()) : nullptr;

	if (!IsValid(AvatarActor) || !IsValid(PlayerController) || !DamageEffectClass)
	{
		return;
	}

	const AEnclaveWeapon* Weapon = ResolveWeapon();
	const UEnclaveWeaponDefinition* WeaponDefinition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;

	if (!WeaponDefinition)
	{
		// No weapon, or a weapon with no Weapon Definition assigned - there
		// is no per-weapon damage/range to read, so there is nothing safe to
		// fire with.
		return;
	}

	UWorld* World = AvatarActor->GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	// Aim from the camera, not a weapon muzzle socket - standard for hitscan
	// shooters, since a muzzle-origin trace can hit geometry the crosshair
	// is clearly not aimed at (corners, low walls) whenever the gun model
	// leads or trails the camera.
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * WeaponDefinition->GetTraceRange());

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor);

	// Also ignore the wielded weapon actor (and anything else attached to
	// the character), since it's attached to - and therefore effectively
	// part of - the avatar for trace purposes.
	TArray<AActor*> AttachedActors;
	AvatarActor->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, ViewLocation, TraceEnd, ECC_Weapon, QueryParams);

	if (CVarDebugFireTrace.GetValueOnGameThread())
	{
		DrawDebugLine(World, ViewLocation, bHit ? HitResult.ImpactPoint : TraceEnd, bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.0f);
		UE_LOG(LogTemp, Warning, TEXT("Fire hit: %s"), *GetNameSafe(HitResult.GetActor()));
	}

	if (!bHit)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitResult.GetActor());

	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		// A valid hit on something with no ASC (world geometry, a prop) is
		// not an error - it just isn't a damageable target.
		return;
	}

	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetCurrentSourceObject());
	ContextHandle.AddHitResult(HitResult);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(Handle, ActorInfo), ContextHandle);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	// Negative: the effect's modifier Operation is Add, so Health +=
	// (-DamageAmount). The magnitude itself stays a positive, designer-
	// facing "damage amount" - now read from the weapon rather than the
	// ability, so different weapons don't need different ability subclasses.
	SpecHandle.Data->SetSetByCallerMagnitude(EnclaveGameplayTags::Data_Damage, -WeaponDefinition->GetDamageAmount());

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	NotifyHitConfirmed(ActorInfo);
}
