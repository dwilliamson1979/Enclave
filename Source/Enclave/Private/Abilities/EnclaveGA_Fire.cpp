// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Fire.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"

#include "GameFramework/PlayerController.h"

#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"

#include "Abilities/Interfaces/EnclaveFireableInterface.h"
#include "Abilities/Interfaces/EnclaveAmmoConsumerInterface.h"
#include "Combat/Interfaces/EnclaveHitFeedbackInterface.h"

UEnclaveGA_Fire::UEnclaveGA_Fire()
{
	// Can't fire mid-reload. GAS checks this in CanActivateAbility on both
	// the predicting client and the server.
	ActivationBlockedTags.AddTag(EnclaveGameplayTags::State_Weapon_Reloading);
}

void UEnclaveGA_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Resolved once here and threaded through to ExecuteSourceCue/
	// PerformHitDetection, rather than each of them re-deriving the same
	// two pointers off ActorInfo independently.
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// Empty magazine: click, commit nothing (no cooldown, no cost), end.
	// Checked before CommitAbility rather than via a CheckCost override -
	// see the class comment. A source that doesn't consume ammo at all
	// never gets here. This is also where an "auto-reload on empty" player
	// option would hook in: try-activate the item's reload ability instead
	// of (or after) the dry-fire cue.
	const IEnclaveAmmoConsumerInterface* AmmoConsumer = GetTriggeringSourceAs<IEnclaveAmmoConsumerInterface>();

	if (AmmoConsumer && !AmmoConsumer->CanConsumeAmmo())
	{
		ExecuteSourceCue(EnclaveGameplayTags::GameplayCue_Weapon_DryFire, SourceASC, AvatarActor);
		AcknowledgeUnspentAmmo(ActivationInfo);
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Cooldown or cost check failed (e.g. still on cooldown from the
		// last shot) - nothing fires, nothing traces.
		AcknowledgeUnspentAmmo(ActivationInfo);
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// Unconditional and before the authority check: this runs on the
	// predicting owning client too, so the shot feels instant locally, while
	// GAS's cue replication recognizes the matching prediction key on the
	// server's own execution and does not double-play it there.
	ExecuteSourceCue(EnclaveGameplayTags::GameplayCue_Weapon_Fire, SourceASC, AvatarActor);

	if (HasAuthority(&ActivationInfo))
	{
		PerformHitDetection(Handle, ActorInfo, SourceASC, AvatarActor);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

UGameplayEffect* UEnclaveGA_Fire::GetCooldownGameplayEffect() const
{
	const IEnclaveFireableInterface* Fireable = GetTriggeringSourceAs<IEnclaveFireableInterface>();
	const TSubclassOf<UGameplayEffect> CooldownClass = Fireable ? Fireable->GetFireCooldownEffectClass() : nullptr;

	if (CooldownClass)
	{
		return CooldownClass->GetDefaultObject<UGameplayEffect>();
	}

	// No fireable source resolved, or it has no cooldown effect configured -
	// fall back to the base UGameplayAbility behavior
	// (CooldownGameplayEffectClass, which this ability leaves unset).
	return Super::GetCooldownGameplayEffect();
}

void UEnclaveGA_Fire::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	// Both sides, keyed by this activation's prediction key: the server
	// spends for real and acknowledges the key; the predicting client
	// records a predicted spend under the same key, so its count drops
	// immediately and never lets it predict a shot past its last round.
	// The consumer tells the two sides apart itself.
	if (IEnclaveAmmoConsumerInterface* AmmoConsumer = GetTriggeringSourceAs<IEnclaveAmmoConsumerInterface>())
	{
		AmmoConsumer->ConsumeAmmo(ActivationInfo.GetActivationPredictionKey());
	}
}

void UEnclaveGA_Fire::AcknowledgeUnspentAmmo(const FGameplayAbilityActivationInfo& ActivationInfo)
{
	if (!HasAuthority(&ActivationInfo))
	{
		return;
	}

	if (IEnclaveAmmoConsumerInterface* AmmoConsumer = GetTriggeringSourceAs<IEnclaveAmmoConsumerInterface>())
	{
		AmmoConsumer->AcknowledgeAmmoPrediction(ActivationInfo.GetActivationPredictionKey());
	}
}

void UEnclaveGA_Fire::ExecuteSourceCue(const FGameplayTag& CueTag, UAbilitySystemComponent* SourceASC, AActor* AvatarActor)
{
	// Keep the triggering source actor in the cue parameters. The shared cue
	// notify carries it into the semantic presentation event so
	// AvatarPresentation can coordinate both avatar and item presentation.
	AActor* TriggeringActor = GetTriggeringSourceAs<AActor>();

	if (!IsValid(SourceASC) || !IsValid(TriggeringActor))
	{
		return;
	}

	// EffectCauser is the actor-shaped source used as the presentation event's
	// originating item. SourceObject is retained as the semantic GAS source too.
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = TriggeringActor;
	CueParameters.SourceObject = TriggeringActor;

	SourceASC->ExecuteGameplayCue(CueTag, CueParameters);
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

void UEnclaveGA_Fire::PerformHitDetection(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, UAbilitySystemComponent* SourceASC, AActor* AvatarActor)
{
	if (!IsValid(AvatarActor))
	{
		return;
	}

	IEnclaveFireableInterface* Fireable = GetTriggeringSourceAs<IEnclaveFireableInterface>();

	if (!Fireable)
	{
		// Nothing implementing the contract this ability needs triggered
		// it - there is nothing safe to fire with.
		return;
	}

	// Everything about how this thing aims (camera-based, a muzzle socket, a
	// turret's own tracked target), the shape of its trace (range, channel,
	// single line vs. a future multi-pellet cone or projectile), and each
	// hit's own damage/damage effect (FEnclaveHitResult) is entirely the
	// triggering source's concern now - this ability supplies nothing but
	// "who's doing the shooting" (AvatarActor, so a Fireable doesn't hit its
	// own avatar) and consumes whatever comes back. Deliberately does not
	// resolve or hand in a ViewLocation/ViewRotation - that would presume
	// every Fireable aims from AvatarActor's eyes, which won't hold for a
	// future non-eye-based one (a deployed turret tracking its own target).
	const TArray<FEnclaveHitResult> HitResults = Fireable->PerformFireHitDetection(AvatarActor);

	if (HitResults.IsEmpty())
	{
		return;
	}

	if (!IsValid(SourceASC))
	{
		return;
	}

	for (const FEnclaveHitResult& HitResult : HitResults)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitResult.GetActor());

		if (!IsValid(TargetASC))
		{
			// A valid hit on something with no ASC (world geometry, a prop)
			// is not an error - it just isn't a damageable target.
			continue;
		}

		if (!HitResult.DamageEffectClass)
		{
			// This specific hit didn't come with a damage effect to apply -
			// a misconfigured weapon (no DamageEffectClass set on its
			// UEnclaveWeaponDefinition), not something to silently
			// substitute a default for.
			continue;
		}

		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddSourceObject(GetCurrentSourceObject());
		ContextHandle.AddHitResult(HitResult);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(HitResult.DamageEffectClass, GetAbilityLevel(Handle, ActorInfo), ContextHandle);

		if (!SpecHandle.IsValid())
		{
			continue;
		}

		// Negative: the effect's modifier Operation is Add, so Health +=
		// (-Damage). The magnitude itself stays a positive, designer-facing
		// "damage amount" - read off this specific hit (computed by the
		// triggering source at trace time), so different implementers, and
		// even different hits within the same shot, don't need different
		// ability subclasses.
		SpecHandle.Data->SetSetByCallerMagnitude(EnclaveGameplayTags::Data_Damage, -HitResult.Damage);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		// Fires once per confirmed hit. Fine for a single-hit weapon (0 or 1
		// iteration); worth revisiting if/when a multi-hit weapon (a
		// shotgun) means several pellets landing in one activation - may
		// want at most one hit-marker notification per activation rather
		// than per pellet.
		NotifyHitConfirmed(ActorInfo);
	}
}
