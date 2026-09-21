// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Fire.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"

#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "GameplayTags/EnclaveAbilityTags.h"

#include "Collision/EnclaveCollisionChannels.h"

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

	if (HasAuthority(&ActivationInfo))
	{
		PerformTraceAndApplyDamage(Handle, ActorInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

void UEnclaveGA_Fire::PerformTraceAndApplyDamage(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo)
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	APlayerController* PlayerController = ActorInfo ? Cast<APlayerController>(ActorInfo->PlayerController.Get()) : nullptr;

	if (!IsValid(AvatarActor) || !IsValid(PlayerController) || !DamageEffectClass)
	{
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

	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * TraceRange);

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
	// facing "damage amount" on the ability.
	SpecHandle.Data->SetSetByCallerMagnitude(EnclaveGameplayTags::Data_Damage, -DamageAmount);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}
