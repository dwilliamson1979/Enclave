// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Fire.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Combat/EnclaveShotIntent.h"
#include "Combat/EnclaveHitRegistrationSubsystem.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameplayPrediction.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Controllers/EnclavePlayerController.h"
#include "Debug/EnclaveDebug.h"
#include "HAL/IConsoleManager.h"

#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"

#include "Abilities/Interfaces/EnclaveFireableInterface.h"
#include "Abilities/Interfaces/EnclaveAmmoConsumerInterface.h"
#include "Combat/Interfaces/EnclaveHitFeedbackInterface.h"
#include "Combat/EnclaveDamageRequest.h"
#include "Combat/EnclaveDamageStatics.h"

#if ENCLAVE_INCLUDE_DEBUG_CODE
// Owning-client-only test injection. The server must reject bad data without
// spending ammo or performing hit registration. Never enabled by default.
static TAutoConsoleVariable<int32> CVarDebugShotIntentFault(
	TEXT("Enclave.Debug.ShotIntentFault"),
	0,
	TEXT("Client shot validation test: 0=normal, 1=origin +1000cm, 2=reversed direction, 3=time -5s, 4=wrong sequence, 5=nonzero spread seed."),
	ECVF_Cheat);
#endif

UEnclaveGA_Fire::UEnclaveGA_Fire()
{
	AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Item_Fire);
	// A remote shot may remain alive waiting for its Target Data while the
	// next shot is activated. Never reuse the same ability instance for both.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
}

void UEnclaveGA_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(SourceASC) || !IsValid(AvatarActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*replicate*/ HasAuthority(&ActivationInfo), /*cancelled*/ true);
		return;
	}

	const bool bAuthority = HasAuthority(&ActivationInfo);
	const bool bRemotePlayerOnServer = bAuthority && !ActorInfo->IsLocallyControlled() &&
		IsValid(Cast<APlayerController>(ActorInfo->PlayerController.Get()));

	// Retain the current dry-fire semantics; never send data for predicted dry fire.
	const IEnclaveAmmoConsumerInterface* AmmoConsumer = GetTriggeringSourceAs<IEnclaveAmmoConsumerInterface>();
	if (AmmoConsumer && !AmmoConsumer->CanConsumeAmmo())
	{
		ExecuteSourceCue(EnclaveGameplayTags::GameplayCue_Item_Action_DryFire, SourceASC, AvatarActor);
		AcknowledgeUnspentAmmo(ActivationInfo);
		if (bRemotePlayerOnServer)
		{
			SourceASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, bAuthority, true);
		return;
	}

	if (bRemotePlayerOnServer)
	{
		// Wait for the matching client shot intent before validating, committing,
		// playing a cue, or tracing. Local authority still fires immediately.
		PendingTargetDataASC = SourceASC;
		bWaitingForShotData = true;
		ShotDataDelegateHandle = SourceASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UEnclaveGA_Fire::OnShotTargetDataReceived);
		GetWorld()->GetTimerManager().SetTimer(ShotDataTimeoutHandle, this, &UEnclaveGA_Fire::OnShotDataTimeout,
			ShotDataTimeoutSeconds, false);
		// Target data can arrive before server ActivateAbility starts.
		SourceASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
		return;
	}

	// Local predicting client, listen host, AI/turret authority: existing
	// commit/cue/hit semantics remain immediate.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		AcknowledgeUnspentAmmo(ActivationInfo);
		EndAbility(Handle, ActorInfo, ActivationInfo, bAuthority, true);
		return;
	}

	if (!bAuthority)
	{
		// Capture the exact pawn-eye viewpoint at Fire activation (not at
		// Focus start). It is identical to CameraComp after Stage 3B.
		FVector Origin;
		FRotator ViewRotation;
		AvatarActor->GetActorEyesViewPoint(Origin, ViewRotation);

		FGameplayAbilityTargetDataHandle TargetData;
		FEnclaveGameplayAbilityTargetData_ShotIntent* ShotData = new FEnclaveGameplayAbilityTargetData_ShotIntent();
		ShotData->Shot.Origin = Origin;
		ShotData->Shot.Direction = ViewRotation.Vector();
		ShotData->Shot.ShotSequence = static_cast<uint16>(ActivationInfo.GetActivationPredictionKey().Current);
		if (const UWorld* World = AvatarActor->GetWorld())
		{
			if (const AGameStateBase* GameState = World->GetGameState())
			{
				ShotData->Shot.ClientServerTimeSeconds = GameState->GetServerWorldTimeSeconds();
			}
		}
#if ENCLAVE_INCLUDE_DEBUG_CODE
		// Negative-path verification without hand-editing network packets.
		// The valid client still predicts the shot, but the server must not
		// commit ammo/cooldown, cue, trace, or damage for malformed intent.
		switch (CVarDebugShotIntentFault.GetValueOnGameThread())
		{
		case 1:
			ShotData->Shot.Origin = FVector(ShotData->Shot.Origin) + FVector(1000.0, 0.0, 0.0);
			break;
		case 2:
			ShotData->Shot.Direction = -ViewRotation.Vector();
			break;
		case 3:
			ShotData->Shot.ClientServerTimeSeconds -= 5.0;
			break;
		case 4:
			++ShotData->Shot.ShotSequence;
			break;
		case 5:
			ShotData->Shot.SpreadSeed = 1;
			break;
		default:
			break;
		}
#endif
		TargetData.Add(ShotData);

		// GAS caches this by spec + ORIGINAL activation prediction key.
		FScopedPredictionWindow PredictionWindow(SourceASC, true);
		SourceASC->CallServerSetReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey(),
			TargetData, FGameplayTag(), SourceASC->ScopedPredictionKey);
#if ENCLAVE_INCLUDE_DEBUG_CODE
		// Avoid one Log-level message per shot in normal play. This diagnostic
		// shares the existing CVar with the owning-client ray comparison.
		if (AEnclavePlayerController::IsShotAlignmentDebugEnabled())
		{
			UE_LOG(LogEnclaveCombat, Log, TEXT("FireIntent SENT key=%d sequence=%u origin=%s direction=%s time=%.6f"),
				ActivationInfo.GetActivationPredictionKey().Current, ShotData->Shot.ShotSequence,
				*Origin.ToString(), *ViewRotation.Vector().ToString(), ShotData->Shot.ClientServerTimeSeconds);
		}
#endif
	}

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (AEnclavePlayerController* DebugController = Cast<AEnclavePlayerController>(ActorInfo->PlayerController.Get());
		IsValid(DebugController) && DebugController->IsLocalController() && AEnclavePlayerController::IsShotAlignmentDebugEnabled())
	{
		DebugController->DebugCaptureLocalShot(ActivationInfo.GetActivationPredictionKey().Current);
	}
#endif

	ExecuteSourceCue(EnclaveGameplayTags::GameplayCue_Item_Action_Fire, SourceASC, AvatarActor);
	if (bAuthority)
	{
		PerformHitDetection(ActorInfo, AvatarActor, ActivationInfo);
	}

	// Client endings must NOT RPC-end the pending server activation.
	EndAbility(Handle, ActorInfo, ActivationInfo, bAuthority, false);
}

void UEnclaveGA_Fire::OnShotTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (!bWaitingForShotData)
	{
		return;
	}
	UAbilitySystemComponent* ASC = PendingTargetDataASC.Get();
	if (!IsValid(ASC))
	{
		OnShotDataTimeout();
		return;
	}

	// Copy values before consuming and invalidating the ASC's replicated cache.
	const FGameplayAbilityTargetData* BaseData = Data.Num() == 1 ? Data.Get(0) : nullptr;
	const FEnclaveGameplayAbilityTargetData_ShotIntent* ShotData =
		BaseData && BaseData->GetScriptStruct() == FEnclaveGameplayAbilityTargetData_ShotIntent::StaticStruct()
		? static_cast<const FEnclaveGameplayAbilityTargetData_ShotIntent*>(BaseData) : nullptr;
	const FEnclaveShotIntent Shot = ShotData ? ShotData->Shot : FEnclaveShotIntent();

	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	ReleaseShotDataDelegate();

	const FVector Origin = Shot.Origin;
	const FVector Direction = Shot.Direction;
	const bool bStructurallyValid = ShotData && !Origin.ContainsNaN() && !Direction.ContainsNaN() &&
		FMath::IsFinite(Shot.ClientServerTimeSeconds) && Shot.ClientServerTimeSeconds >= 0.0 &&
		FMath::IsNearlyEqual(Direction.SizeSquared(), 1.0, 0.03);

	if (!bStructurallyValid)
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("FireIntent REJECT transport: invalid payload key=%d"),
			CurrentActivationInfo.GetActivationPredictionKey().Current);
		AcknowledgeUnspentAmmo(CurrentActivationInfo);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

#if ENCLAVE_INCLUDE_DEBUG_CODE
	// Transport succeeded; keep the routine trace behind the same CVar.
	if (AEnclavePlayerController::IsShotAlignmentDebugEnabled())
	{
		UE_LOG(LogEnclaveCombat, Log,
			TEXT("FireIntent RECEIVED key=%d sequence=%u origin=%s direction=%s time=%.6f (untrusted; awaiting server validation)"),
			CurrentActivationInfo.GetActivationPredictionKey().Current, Shot.ShotSequence,
			*Origin.ToString(), *Direction.ToString(), Shot.ClientServerTimeSeconds);
	}
#endif

	UWorld* World = GetWorld();
	UEnclaveHitRegistrationSubsystem* Registration = World ? World->GetSubsystem<UEnclaveHitRegistrationSubsystem>() : nullptr;
	const AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	FEnclaveValidatedShotRay ValidatedRay;
	FString RejectReason;
	if (!IsValid(Registration) || !Registration->ValidateClientShotIntent(Shot, Avatar,
		static_cast<uint16>(CurrentActivationInfo.GetActivationPredictionKey().Current), ValidatedRay, RejectReason))
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("FireIntent REJECT validation key=%d reason=%s"),
			CurrentActivationInfo.GetActivationPredictionKey().Current,
			IsValid(Registration) ? *RejectReason : TEXT("hit registration subsystem unavailable"));
		AcknowledgeUnspentAmmo(CurrentActivationInfo);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (AEnclavePlayerController::IsShotAlignmentDebugEnabled())
	{
		UE_LOG(LogEnclaveCombat, Log, TEXT("FireIntent ACCEPTED key=%d origin=%s direction=%s time=%.6f"),
			CurrentActivationInfo.GetActivationPredictionKey().Current,
			*ValidatedRay.Origin.ToString(), *ValidatedRay.Direction.ToString(), ValidatedRay.ClientServerTimeSeconds);
	}
#endif
	CompleteAuthoritativeShot(ValidatedRay);
}

void UEnclaveGA_Fire::CompleteAuthoritativeShot(const FEnclaveValidatedShotRay& ValidatedRay)
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	const FGameplayAbilityActivationInfo ActivationInfo = CurrentActivationInfo;
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Avatar) || !IsValid(ASC) || !IsValid(GetCurrentSourceObject()) || !GetTriggeringSourceAs<IEnclaveFireableInterface>() ||
		!CommitAbility(CurrentSpecHandle, ActorInfo, ActivationInfo))
	{
		AcknowledgeUnspentAmmo(ActivationInfo);
		EndAbility(CurrentSpecHandle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ExecuteSourceCue(EnclaveGameplayTags::GameplayCue_Item_Action_Fire, ASC, Avatar);
	// Accepted shot geometry now drives the actual authoritative hitscan.
	// Collision and damage remain exclusively server-owned.
	PerformHitDetection(ActorInfo, Avatar, ActivationInfo, &ValidatedRay);
	EndAbility(CurrentSpecHandle, ActorInfo, ActivationInfo, true, false);
}

void UEnclaveGA_Fire::OnShotDataTimeout()
{
	if (!bWaitingForShotData) { return; }
	UE_LOG(LogEnclaveCombat, Warning, TEXT("FireIntent TIMEOUT key=%d; no authoritative shot"),
		CurrentActivationInfo.GetActivationPredictionKey().Current);
	if (UAbilitySystemComponent* ASC = PendingTargetDataASC.Get())
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	ReleaseShotDataDelegate();
	AcknowledgeUnspentAmmo(CurrentActivationInfo);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UEnclaveGA_Fire::ReleaseShotDataDelegate()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotDataTimeoutHandle);
	}
	if (UAbilitySystemComponent* ASC = PendingTargetDataASC.Get(); ShotDataDelegateHandle.IsValid() && IsValid(ASC))
	{
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			.Remove(ShotDataDelegateHandle);
	}
	ShotDataDelegateHandle.Reset();
	PendingTargetDataASC.Reset();
	bWaitingForShotData = false;
}

void UEnclaveGA_Fire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ReleaseShotDataDelegate();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UGameplayEffect* UEnclaveGA_Fire::GetCooldownGameplayEffect() const
{
	// InstancedPerExecution abilities can be queried on their CDO during
	// CanActivateAbility/CheckCooldown. The CDO has no current spec, source,
	// actor info or ASC; GetCurrentSourceObject() would assert. The source-
	// aware eligibility check is handled by CheckCooldown(Handle, ActorInfo).
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return Super::GetCooldownGameplayEffect();
	}

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

bool UEnclaveGA_Fire::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Preserve normal GAS checks for any cooldown configured on the ability.
	// On the CDO, GetCooldownGameplayEffect() above intentionally returns
	// only the base ability's configured effect, not the item-supplied one.
	if (!Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags))
	{
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (AEnclavePlayerController::IsShotAlignmentDebugEnabled())
		{
			UE_LOG(LogEnclaveCombat, Log, TEXT("FireIntent cooldown check rejected by base GAS cooldown policy; avatar=%s"),
				*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
		}
#endif
		return false;
	}

	// Match GAS's global cooldown bypass policy as well.
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC))
	{
		return false;
	}

	// IMPORTANT: This can execute on the CDO. Unlike the instance-only
	// GetTriggeringSourceAs(), this accessor uses the explicit ability spec.
	const IEnclaveFireableInterface* Fireable =
		GetTriggeringSourceFromSpecAs<IEnclaveFireableInterface>(Handle, ActorInfo);
	const TSubclassOf<UGameplayEffect> CooldownClass = Fireable ? Fireable->GetFireCooldownEffectClass() : nullptr;
	if (!CooldownClass)
	{
		return true; // Same no-item-cooldown fallback as before Stage A.
	}

	const UGameplayEffect* CooldownEffect = CooldownClass->GetDefaultObject<UGameplayEffect>();
	if (!IsValid(CooldownEffect))
	{
		return true;
	}

	const FGameplayTagContainer& GrantedCooldownTags = CooldownEffect->GetGrantedTags();
	if (GrantedCooldownTags.IsEmpty() || !ASC->HasAnyMatchingGameplayTags(GrantedCooldownTags))
	{
		return true;
	}

	if (OptionalRelevantTags)
	{
		const FGameplayTag FailTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		if (FailTag.IsValid())
		{
			OptionalRelevantTags->AddTag(FailTag);
		}
	}
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (AEnclavePlayerController::IsShotAlignmentDebugEnabled())
	{
		UE_LOG(LogEnclaveCombat, Log, TEXT("FireIntent cooldown check rejected by item effect; avatar=%s tags=%s"),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr), *GrantedCooldownTags.ToStringSimple());
	}
#endif
	return false;
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

void UEnclaveGA_Fire::PerformHitDetection(const FGameplayAbilityActorInfo* ActorInfo, AActor* AvatarActor,
	const FGameplayAbilityActivationInfo& ActivationInfo, const FEnclaveValidatedShotRay* ValidatedRay)
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

	// The fireable owns range, hit shape and damage. It receives a server-
	// accepted ray only for remote players; host and AI paths pass nullptr
	// so the implementation resolves its own authoritative aiming source.
	// The hit-registration subsystem observes the precise authoritative
	// query. Bracket this synchronous call with the GAS activation key so
	// its owning-client result can match the predicted camera sample.
#if ENCLAVE_INCLUDE_DEBUG_CODE
	AEnclavePlayerController* DebugController = ActorInfo ? Cast<AEnclavePlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	if (!IsValid(DebugController))
	{
		if (const APawn* Pawn = Cast<APawn>(AvatarActor))
		{
			DebugController = Cast<AEnclavePlayerController>(Pawn->GetController());
		}
	}
	const bool bCaptureDebugShot = IsValid(DebugController) && AEnclavePlayerController::IsShotAlignmentDebugEnabled();
	if (bCaptureDebugShot)
	{
		DebugController->DebugBeginAuthoritativeShot(ActivationInfo.GetActivationPredictionKey().Current);
	}
#endif
	const TArray<FEnclaveHitResult> HitResults = Fireable->PerformFireHitDetection(AvatarActor, ValidatedRay);
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bCaptureDebugShot)
	{
		DebugController->DebugEndAuthoritativeShot();
	}
#endif

	if (HitResults.IsEmpty())
	{
		return;
	}

	AActor* DamageCauser = GetTriggeringSourceAs<AActor>();

	for (const FEnclaveHitResult& HitResult : HitResults)
	{
		AActor* Target = HitResult.GetActor();
		if (!IsValid(Target) || HitResult.Damage <= 0.0f)
		{
			continue;
		}

		FEnclaveDamageRequest DamageRequest;
		DamageRequest.Damage = HitResult.Damage;
		DamageRequest.SourceActor = AvatarActor;
		DamageRequest.DamageCauser = DamageCauser;
		DamageRequest.HitResult = HitResult;
		DamageRequest.DamageTags = HitResult.DamageTags;

		if (!UEnclaveDamageStatics::ApplyDamage(Target, DamageRequest))
		{
			// World geometry and actors with no Enclave damage receiver are valid
			// trace hits; they simply do not consume an Enclave damage request.
			continue;
		}

		// Fires once per confirmed damage application. Fine for a single-hit
		// weapon (0 or 1 iteration); worth revisiting if/when a multi-hit weapon
		// should produce at most one hit-marker notification per activation.
		NotifyHitConfirmed(ActorInfo);
	}
}
