#include "Debug/EnclaveSSRTestComponent.h"
#include "Controllers/EnclavePlayerController.h"
#include "Debug/EnclaveSSRTestSubsystem.h"
#include "Debug/EnclaveDebug.h"
#include "Characters/EnclaveCharacter.h"
#include "Engine/World.h"
#include "Characters/Components/EnclaveCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "Equipment/Weapons/EnclaveFirearmWeapon.h"
#include "GameplayTags/EnclaveInputTags.h"
#include "InputActionValue.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"
#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"

#include "HAL/IConsoleManager.h"

#if ENCLAVE_INCLUDE_DEBUG_CODE
namespace
{
    int32 GetSSRTestSeed()
    {
        if (const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("Enclave.Debug.SSRAutoSeed"))) return V->GetInt();
        return 1729;
    }
    float GetSSRStrafeDuration()
    {
        if (const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("Enclave.Debug.SSRStrafeDuration"))) return V->GetFloat();
        return 30.0f;
    }
}
#endif

UEnclaveSSRTestComponent::UEnclaveSSRTestComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveSSRTestComponent::ClientEnclaveSSRTestConfigure_Implementation(int32 TestId, int32 TestRole, int32 Scenario, int32 Seed, AEnclaveCharacter* Target)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (!IsValid(PC)) return;
    if (!PC->IsLocalController() || PC->GetNetMode() != NM_Client || (TestRole != 1 && TestRole != 2) ||
        Scenario < 2 || Scenario > 5 || TestId <= 0)
    {
        ServerEnclaveSSRTestReady(TestId, TestRole, false);
        return;
    }
    EnclaveDebugSSRAutoFire(0);
    EnclaveDebugSSRStrafe(0);
    EnclaveDebugSSRCrouch(0);
    ClientTestId = TestId;
    ClientRole = TestRole;
    ForcedTarget = Target;
    auto SetIntCVar = [](const TCHAR* Name, int32 Value) {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name)) CVar->Set(Value, ECVF_SetByCode);
    };
    SetIntCVar(TEXT("Enclave.Debug.SSRGameplayGate"), 1);
    SetIntCVar(TEXT("Enclave.Debug.SSRScenario"), Scenario);
    SetIntCVar(TEXT("Enclave.Debug.SSRAutoSeed"), Seed);
    if (IConsoleVariable* Duration = IConsoleManager::Get().FindConsoleVariable(TEXT("Enclave.Debug.SSRStrafeDuration")))
        Duration->Set(180.0f, ECVF_SetByCode);
    bool bReady = IsValid(Cast<AEnclaveCharacter>(PC->GetPawn()));
    if (TestRole == 1) bReady &= IsValid(Target) && Target->GetLocalRole() == ROLE_SimulatedProxy;
    if (bReady && TestRole == 2)
    {
        if (Scenario == 4) EnclaveDebugSSRCrouch(1);
        if (Scenario != 5) EnclaveDebugSSRStrafe(1);
    }
    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRTest CLIENT_CONFIG id=%d role=%d scenario=%d ready=%d pawn=%s target=%s (DEBUG ONLY)",
        TestId, TestRole, Scenario, bReady ? 1 : 0, *GetNameSafe(PC->GetPawn()), *GetNameSafe(Target));
    ServerEnclaveSSRTestReady(TestId, TestRole, bReady);
#else
    (void)TestId; (void)TestRole; (void)Scenario; (void)Seed; (void)Target;
#endif
}

void UEnclaveSSRTestComponent::ServerEnclaveSSRTestReady_Implementation(int32 TestId, int32 TestRole, bool bReady)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (IsValid(PC) && PC->HasAuthority())
        if (UEnclaveSSRTestSubsystem* Session = GetWorld() ? GetWorld()->GetSubsystem<UEnclaveSSRTestSubsystem>() : nullptr)
            Session->ParticipantReady(PC, TestId, TestRole, bReady);
#endif
}

void UEnclaveSSRTestComponent::ClientEnclaveSSRTestBeginFire_Implementation(int32 TestId)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (IsValid(PC) && PC->IsLocalController() && PC->GetNetMode() == NM_Client && IsCoordinatedShooter() &&
        ClientTestId == TestId && IsValid(ForcedTarget.Get()))
    {
        EnclaveDebugSSRAutoFire(1);
        ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest FIRE_STARTED id=%d started=%d (DEBUG ONLY)",
            TestId, bDebugSSRAutoEnabled ? 1 : 0);
    }
#else
    (void)TestId;
#endif
}

void UEnclaveSSRTestComponent::ClientEnclaveSSRTestCleanup_Implementation(int32 TestId)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (IsValid(PC) && PC->IsLocalController() && ClientTestId == TestId)
    {
        EnclaveDebugSSRAutoFire(0);
        EnclaveDebugSSRStrafe(0);
        EnclaveDebugSSRCrouch(0);
        ForcedTarget.Reset();
        ClientTestId = 0;
        ClientRole = 0;
        ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest CLIENT_CLEANUP id=%d (DEBUG ONLY)", TestId);
    }
#else
    (void)TestId;
#endif
}

void UEnclaveSSRTestComponent::ParticipantUnpossessed()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    EnclaveDebugSSRAutoFire(0);
    EnclaveDebugSSRStrafe(0);
    EnclaveDebugSSRCrouch(0);
    ForcedTarget.Reset();
    ClientTestId = 0;
    ClientRole = 0;
#endif
}

#if ENCLAVE_INCLUDE_DEBUG_CODE
void UEnclaveSSRTestComponent::EnclaveDebugSSRCrouch(int32 bEnable)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (!IsValid(PC)) return;
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bEnable != 0 && bEnable != 1)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch rejected=expected_0_or_1 (DEBUG ONLY)");
		return;
	}

	// Don't introduce a crouch/stand transition in the middle of a strafe run.
	if (bEnable == 0 && bDebugSSRStrafeEnabled)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch rejected=stop_strafe_before_standing (DEBUG ONLY)");
		return;
	}

	// A stop must still be possible after disabling the gameplay gate.
	if (bEnable == 0)
	{
		// Cancels an earlier strafe request queued during the crouch transition.
		bDebugSSRStrafeQueuedForCrouch = false;
		if (AEnclaveCharacter* Original = DebugSSRCrouchPawn.Get())
		{
			if (Original->IsLocallyControlled())
			{
				Original->UnCrouch();
			}
			bDebugSSRCrouchDesired = false;
			bDebugSSRCrouchPending = true;
			DebugSSRCrouchWaitSeconds = 0.0f;
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPlayerCrouch REQUEST pawn=%s crouch=0 (normal ACharacter::UnCrouch; DEBUG ONLY)",
				*GetNameSafe(Original));
		}
		return;
	}

	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(PC->GetPawn());
	UCharacterMovementComponent* Movement = IsValid(Char) ? Char->GetCharacterMovement() : nullptr;
	if (!PC->IsLocalController() || !IsValid(Char) || !Char->IsLocallyControlled() ||
		!IsValid(Movement) || !AEnclavePlayerController::IsSSRGameplayGateEnabled())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch rejected=requires_locally_controlled_character_and_gameplay_gate (DEBUG ONLY)");
		return;
	}
	if (bDebugSSRStrafeEnabled)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch rejected=strafe_active stop_with_Enclave.Debug.SSRStrafe_0 (DEBUG ONLY)");
		return;
	}
	if (!Movement->CanCrouchInCurrentState())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch rejected=cannot_crouch_in_current_state movementMode=%d (DEBUG ONLY)",
			static_cast<int32>(Movement->MovementMode));
		return;
	}
	if (DebugSSRCrouchPawn.IsValid() && DebugSSRCrouchPawn.Get() != Char)
	{
		StopDebugSSRCrouch(TEXT("target_changed"));
	}

	bDebugSSRStrafeQueuedForCrouch = false;
	DebugSSRCrouchPawn = Char;
	bDebugSSRCrouchDesired = true;
	bDebugSSRCrouchPending = true;
	DebugSSRCrouchWaitSeconds = 0.0f;
	Char->Crouch();
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRPlayerCrouch REQUEST pawn=%s crouch=1 verification=pending_next_movement_tick (normal ACharacter::Crouch; DEBUG ONLY)",
		*GetNameSafe(Char));
#else
	(void)bEnable;
#endif
}

void UEnclaveSSRTestComponent::StopDebugSSRCrouch(const TCHAR* Reason)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (AEnclaveCharacter* Original = DebugSSRCrouchPawn.Get())
	{
		if (Original->IsLocallyControlled())
		{
			Original->UnCrouch();
		}
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerCrouch STOP pawn=%s reason=%s restoreStanding=1 (DEBUG ONLY)",
			*GetNameSafe(Original), Reason);
	}
	if (bDebugSSRStrafeQueuedForCrouch)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerStrafe LOCAL QUEUE_CANCELLED reason=%s (DEBUG ONLY)", Reason);
	}
	bDebugSSRStrafeQueuedForCrouch = false;
	DebugSSRCrouchPawn.Reset();
	bDebugSSRCrouchDesired = false;
	bDebugSSRCrouchPending = false;
	DebugSSRCrouchWaitSeconds = 0.0f;
}

void UEnclaveSSRTestComponent::TickDebugSSRCrouch(float DeltaSeconds)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	AEnclaveCharacter* Char = DebugSSRCrouchPawn.Get();
	if (!IsValid(Char))
	{
		if (bDebugSSRStrafeQueuedForCrouch)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRPlayerStrafe LOCAL QUEUE_CANCELLED reason=target_invalid (DEBUG ONLY)");
		}
		bDebugSSRStrafeQueuedForCrouch = false;
		DebugSSRCrouchPawn.Reset();
		bDebugSSRCrouchPending = false;
		bDebugSSRCrouchDesired = false;
		return;
	}
	if (!PC->IsLocalController() || !Char->IsLocallyControlled() || Char != PC->GetPawn())
	{
		StopDebugSSRCrouch(TEXT("local_ownership_or_possession_changed"));
		return;
	}

	const UCharacterMovementComponent* Movement = Char->GetCharacterMovement();
	const UCapsuleComponent* Capsule = Char->GetCapsuleComponent();
	if (!IsValid(Movement) || !IsValid(Capsule))
	{
		StopDebugSSRCrouch(TEXT("movement_or_capsule_missing"));
		return;
	}

	const bool bActualCrouched = Movement->IsCrouching();
	if (bDebugSSRCrouchPending)
	{
		if (bActualCrouched == bDebugSSRCrouchDesired)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPlayerCrouch VERIFIED pawn=%s crouched=%d halfHeightCm=%.2f radiusCm=%.2f authority=%d (local capsule; server must verify remote geometry; DEBUG ONLY)",
				*GetNameSafe(Char), bActualCrouched ? 1 : 0,
				Capsule->GetUnscaledCapsuleHalfHeight(), Capsule->GetUnscaledCapsuleRadius(), Char->HasAuthority() ? 1 : 0);
			bDebugSSRCrouchPending = false;
			if (bDebugSSRCrouchDesired && bDebugSSRStrafeQueuedForCrouch)
			{
				bDebugSSRStrafeQueuedForCrouch = false;
				ENCLAVE_LOG(LogEnclaveCombat, Log,
					"SSRPlayerStrafe LOCAL QUEUE_RELEASED reason=crouch_verified (DEBUG ONLY)");
				EnclaveDebugSSRStrafe(1);
			}
			if (!bDebugSSRCrouchDesired)
			{
				DebugSSRCrouchPawn.Reset();
			}
			return;
		}
		DebugSSRCrouchWaitSeconds += DeltaSeconds;
		if (DebugSSRCrouchWaitSeconds >= 2.0f)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRPlayerCrouch VERIFY_FAILED pawn=%s requested=%d actual=%d halfHeightCm=%.2f canCrouch=%d movementMode=%d (do not score crouch samples; DEBUG ONLY)",
				*GetNameSafe(Char), bDebugSSRCrouchDesired ? 1 : 0,
				bActualCrouched ? 1 : 0, Capsule->GetUnscaledCapsuleHalfHeight(),
				Movement->CanCrouchInCurrentState() ? 1 : 0,
				static_cast<int32>(Movement->MovementMode));
			StopDebugSSRCrouch(TEXT("state_verification_failed"));
		}
		return;
	}

	if (bDebugSSRCrouchDesired && !bActualCrouched)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch STATE_LOST pawn=%s (crouch input or gameplay changed state; scenario no longer valid; DEBUG ONLY)",
			*GetNameSafe(Char));
		StopDebugSSRStrafe(TEXT("crouch_state_lost"));
		StopDebugSSRCrouch(TEXT("crouch_state_lost"));
	}
}

void UEnclaveSSRTestComponent::EnclaveDebugSSRStrafe(int32 bEnable)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (!IsValid(PC)) return;
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bEnable == 0)
	{
		bDebugSSRStrafeQueuedForCrouch = false;
		StopDebugSSRStrafe(TEXT("manual"));
		return;
	}

	if (DebugSSRCrouchPawn.IsValid())
	{
		// A VERIFIED, stable crouch may be strafed deliberately. Transitioning
		// capsules must not be mixed into the stable-crouch acceptance samples.
		const AEnclaveCharacter* CrouchPawn = DebugSSRCrouchPawn.Get();
		const UCharacterMovementComponent* CrouchMovement = CrouchPawn ? CrouchPawn->GetCharacterMovement() : nullptr;
		if (bDebugSSRCrouchPending && bDebugSSRCrouchDesired && CrouchPawn == PC->GetPawn() &&
			IsValid(CrouchMovement))
		{
			bDebugSSRStrafeQueuedForCrouch = true;
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPlayerStrafe LOCAL QUEUED reason=awaiting_authoritative_crouch_verification (DEBUG ONLY)");
			return;
		}
		if (bDebugSSRCrouchPending || !bDebugSSRCrouchDesired || CrouchPawn != PC->GetPawn() ||
			!IsValid(CrouchMovement) || !CrouchMovement->IsCrouching())
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRPlayerStrafe LOCAL start_rejected=crouch_state_not_verified (DEBUG ONLY)");
			return;
		}
	}

	// Restarting the rig must always release the prior body's protection.
	StopDebugSSRStrafe(TEXT("restart"));
	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(PC->GetPawn());
	UEnclaveCharacterMovementComponent* Movement = IsValid(Char) ? Cast<UEnclaveCharacterMovementComponent>(Char->GetCharacterMovement()) : nullptr;
	if (!PC->IsLocalController() || !IsValid(Char) || !Char->IsLocallyControlled() ||
		!IsValid(Movement) || !Movement->IsLocomotionEnabled())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerStrafe LOCAL start_rejected controller=%s reason=requires_locally_controlled_movable_enclave_character",
			*GetNameSafe(PC));
		return;
	}

	const FRotator Yaw(0.0f, PC->GetControlRotation().Yaw, 0.0f);
	DebugSSRStrafeRight = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	DebugSSRStrafeOrigin = Char->GetActorLocation();
	DebugSSRStrafeDirectionSign = 1;
	DebugSSRStrafeElapsedSeconds = 0.0f;
	DebugSSRStrafePauseSeconds = 0.0f;
	DebugSSRStrafeNextStatusSeconds = 2.0f;
	bDebugSSRStrafeEnabled = !DebugSSRStrafeRight.IsNearlyZero();
	if (bDebugSSRStrafeEnabled && Char->HasAuthority() && AEnclavePlayerController::IsSSRGameplayGateEnabled())
	{
		Char->DebugSetSSRStrafeDamageSuppressed(true);
		DebugSSRStrafeProtectedPawn = Char;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerStrafe LOCAL PROTECT pawn=%s damageSuppressed=1 (server-only, debug only)", *GetNameSafe(Char));
	}
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRPlayerStrafe LOCAL %s pawn=%s role=%d laneHalfWidthCm=250 durationSeconds=%.1f crouched=%d (normal CharacterMovement; debug only)",
		bDebugSSRStrafeEnabled ? TEXT("START") : TEXT("start_rejected_zero_direction"),
		*GetNameSafe(Char), static_cast<int32>(Char->GetLocalRole()),
		FMath::Clamp(GetSSRStrafeDuration(), 10.0f, 180.0f),
		Movement->IsCrouching() ? 1 : 0);
#else
	(void)bEnable;
#endif
}

void UEnclaveSSRTestComponent::EnclaveDebugSSRAutoFire(int32 bEnable)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (!IsValid(PC)) return;
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bEnable == 0)
	{
		StopDebugSSRAutoFire(TEXT("manual"));
		return;
	}

	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(PC->GetPawn());
	const UEnclaveEquipmentComponent* Equipment = IsValid(Char) ? Char->FindComponentByClass<UEnclaveEquipmentComponent>() : nullptr;
	const AEnclaveFirearmWeapon* Firearm = IsValid(Equipment) ? Cast<AEnclaveFirearmWeapon>(Equipment->GetCurrentWieldedItem()) : nullptr;
	if (!PC->IsLocalController() || !GetWorld() || PC->GetNetMode() != NM_Client || !IsValid(Char) ||
		!Char->IsLocallyControlled() || !IsValid(Firearm) || !AEnclavePlayerController::IsSSRGameplayGateEnabled() ||
		PC->CachedInputReceiver == nullptr)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRAuto START_REJECTED reason=requires_remote_local_client_firearm_and_gameplay_gate (DEBUG ONLY)");
		return;
	}

	StopDebugSSRAutoFire(TEXT("restart"));
	DebugSSRAutoOriginalPawn = Char;
	DebugSSRAutoRandom.Initialize(GetSSRTestSeed());
	DebugSSRAutoAttemptCount = 0;
	DebugSSRAutoReloadCount = 0;
	DebugSSRAutoConsecutiveReloadFailures = 0;
	bDebugSSRAutoHasObservedMovingTarget = false;
	DebugSSRAutoElapsedSeconds = 0.0f;
	DebugSSRAutoUntilNextShotSeconds = 0.1f;
	DebugSSRAutoReloadDelaySeconds = 0.0f;
	DebugSSRAutoTarget.Reset();
	if (!ChooseDebugSSRAutoTarget())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRAuto START_REJECTED reason=no_simulated_proxy_within_100m (DEBUG ONLY)");
		return;
	}
	bDebugSSRAutoEnabled = true;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRAuto START shooter=%s target=%s seed=%d intervalSeconds=0.40 maxAttempts=240 maxSeconds=180.0"
		" mode=alternating_clear_hit_clear_miss route=Input.Item.Action.Primary via GAS (DEBUG ONLY)",
		*GetNameSafe(Char), *GetNameSafe(DebugSSRAutoTarget.Get()),
		GetSSRTestSeed());
#else
	(void)bEnable;
#endif
}

bool UEnclaveSSRTestComponent::ChooseDebugSSRAutoTarget()
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	DebugSSRAutoTarget.Reset();
	if (IsCoordinatedShooter())
	{
		AEnclaveCharacter* Forced = GetForcedTarget();
		if (IsValid(Forced) && Forced != PC->GetPawn() && Forced->GetLocalRole() == ROLE_SimulatedProxy)
		{
			DebugSSRAutoTarget = Forced;
			return true;
		}
		return false; // Never silently target another player during a coordinated test.
	}
	if (!GetWorld() || !IsValid(PC->GetPawn()))
	{
		return false;
	}
	const FVector Eye = PC->GetPawn()->GetPawnViewLocation();
	double BestDistanceSquared = FMath::Square(10000.0);
	for (TActorIterator<AEnclaveCharacter> It(GetWorld()); It; ++It)
	{
		AEnclaveCharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == PC->GetPawn() || Candidate->GetLocalRole() != ROLE_SimulatedProxy ||
			!IsValid(Candidate->GetCapsuleComponent()) || !IsValid(Candidate->GetMesh()))
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(Eye, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			DebugSSRAutoTarget = Candidate;
		}
	}
	return DebugSSRAutoTarget.IsValid();
}

void UEnclaveSSRTestComponent::StopDebugSSRAutoFire(const TCHAR* Reason)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (bDebugSSRAutoEnabled)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRAuto STOP reason=%s attempts=%d reloads=%d elapsedSeconds=%.1f seed=%d"
			" (attempts are not confirmed GAS activations; score SSRGameplay SERVER by key; DEBUG ONLY)",
			Reason, DebugSSRAutoAttemptCount, DebugSSRAutoReloadCount, DebugSSRAutoElapsedSeconds,
			GetSSRTestSeed());
	}
	bDebugSSRAutoEnabled = false;
	bDebugSSRAutoHasObservedMovingTarget = false;
	DebugSSRAutoTarget.Reset();
	DebugSSRAutoOriginalPawn.Reset();
}

void UEnclaveSSRTestComponent::TickDebugSSRAutoFire(float DeltaSeconds)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (!bDebugSSRAutoEnabled)
	{
		return;
	}
	DebugSSRAutoElapsedSeconds += DeltaSeconds;
	if (DebugSSRAutoElapsedSeconds >= 180.0f || DebugSSRAutoAttemptCount >= 240)
	{
		StopDebugSSRAutoFire(TEXT("bounded_limit"));
		return;
	}
	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(PC->GetPawn());
	const UEnclaveEquipmentComponent* Equipment = IsValid(Char) ? Char->FindComponentByClass<UEnclaveEquipmentComponent>() : nullptr;
	AEnclaveFirearmWeapon* Firearm = IsValid(Equipment) ? Cast<AEnclaveFirearmWeapon>(Equipment->GetCurrentWieldedItem()) : nullptr;
	if (!PC->IsLocalController() || PC->GetNetMode() != NM_Client || !AEnclavePlayerController::IsSSRGameplayGateEnabled() ||
		!IsValid(Char) || Char != DebugSSRAutoOriginalPawn.Get() || !Char->IsLocallyControlled() ||
		!IsValid(Firearm) || PC->CachedInputReceiver == nullptr)
	{
		StopDebugSSRAutoFire(TEXT("invalid_local_shooter_or_firearm"));
		return;
	}
	// Never silently swap targets mid-run: it invalidates scenario identity,
	// latency binning, and repeatability of the shot classifier.
	if (!DebugSSRAutoTarget.IsValid())
	{
		StopDebugSSRAutoFire(TEXT("target_lost"));
		return;
	}
	AEnclaveCharacter* Target = DebugSSRAutoTarget.Get();
	if (!IsValid(Target) || Target->GetLocalRole() != ROLE_SimulatedProxy ||
		!IsValid(Target->GetCapsuleComponent()) || !IsValid(Target->GetMesh()))
	{
		StopDebugSSRAutoFire(TEXT("target_not_simulated_proxy"));
		return;
	}
	if (const UEnclaveAvatarLifecycleComponent* Lifecycle = Target->FindComponentByClass<UEnclaveAvatarLifecycleComponent>())
	{
		if (Lifecycle->IsDead())
		{
			StopDebugSSRAutoFire(TEXT("target_dead"));
			return;
		}
	}
	// Only a target that HAS been moving can be considered stalled. A deliberately
	// stationary scenario has no promise of regular OnRep movement callbacks.
	if (Target->GetVelocity().SizeSquared2D() > FMath::Square(100.0f))
	{
		bDebugSSRAutoHasObservedMovingTarget = true;
	}
	if (bDebugSSRAutoHasObservedMovingTarget)
	{
		const FEnclaveSSRReceivedProxyUpdate& Update = Target->DebugGetLastReceivedProxyUpdate();
		const double SecondsSinceUpdate = GetWorld()->GetTimeSeconds() - Update.ClientReceiveWorldSeconds;
		if (Update.ClientReceiveWorldSeconds < 0.0 || SecondsSinceUpdate > 1.0)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRAuto TARGET_STALLED target=%s lastUpdateAgeMs=%.1f (debug only)",
				*GetNameSafe(Target), SecondsSinceUpdate * 1000.0);
			StopDebugSSRAutoFire(TEXT("target_replication_stalled"));
			return;
		}
	}

	if (DebugSSRAutoReloadDelaySeconds > 0.0f)
	{
		DebugSSRAutoReloadDelaySeconds -= DeltaSeconds;
		return;
	}
	DebugSSRAutoUntilNextShotSeconds -= DeltaSeconds;
	if (DebugSSRAutoUntilNextShotSeconds > 0.0f)
	{
		return;
	}
	DebugSSRAutoUntilNextShotSeconds = 0.40f;

	// Never bypass Fire's ordinary ammo costs or cooldown. Reload by dispatching
	// the same semantic Input.Item.Action.Reload that a user invokes.
	if (!Firearm->CanConsumeAmmo())
	{
		if (++DebugSSRAutoConsecutiveReloadFailures > 3)
		{
			StopDebugSSRAutoFire(TEXT("reload_not_completing"));
			return;
		}
		++DebugSSRAutoReloadCount;
		PC->HandleInput(FInputActionValue(true), ETriggerEvent::Started,
			EnclaveGameplayTags::Input_Item_Action_Reload, false);
		DebugSSRAutoReloadDelaySeconds = 3.5f;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRAuto RELOAD attempt=%d ammo=%d waitSeconds=3.5 (normal semantic GAS input; DEBUG ONLY)",
			DebugSSRAutoReloadCount, Firearm->GetCurrentMagazineAmmo());
		return;
	}
	DebugSSRAutoConsecutiveReloadFailures = 0;

	// Match D1.1A's approximate mesh-anchored reference capsule. The actual
	// server classifier independently decides the resulting shot category.
	const UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
	const FVector SmoothOffset = Capsule->GetComponentTransform().TransformVectorNoScale(
		Target->GetMesh()->GetRelativeLocation() - Target->GetBaseTranslationOffset());
	const FVector ReferenceCenter = Capsule->GetComponentLocation() + SmoothOffset;
	const FVector Eye = Char->GetPawnViewLocation();
	const FVector Forward = (ReferenceCenter - Eye).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	if (Forward.IsNearlyZero() || Right.IsNearlyZero())
	{
		StopDebugSSRAutoFire(TEXT("invalid_target_direction"));
		return;
	}

	const bool bAimForClearHit = (DebugSSRAutoAttemptCount % 2) == 0;
	const float RadiusCm = Capsule->GetScaledCapsuleRadius();
	const float HalfHeightCm = Capsule->GetScaledCapsuleHalfHeight();
	// Leave a 15 cm geometric margin to avoid classifying near-boundary samples
	// as definitive results. Cover the straight capsule section for clear hits;
	// clear misses alternate between lateral, top/bottom and diagonal regions.
	// Client samples are only intentions: the server still owns classification.
	const float MarginCm = 15.0f;
	const float InnerRadiusCm = FMath::Max(0.0f, RadiusCm - MarginCm);
	const float CylinderHalfHeightCm = FMath::Max(0.0f, HalfHeightCm - RadiusCm);
	const float Side = DebugSSRAutoRandom.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
	const float VerticalSign = DebugSSRAutoRandom.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
	float OffsetCm = 0.0f;
	float VerticalOffsetCm = 0.0f;
	if (bAimForClearHit)
	{
		OffsetCm = Side * DebugSSRAutoRandom.FRandRange(0.0f, InnerRadiusCm);
		VerticalOffsetCm = VerticalSign * DebugSSRAutoRandom.FRandRange(0.0f, CylinderHalfHeightCm);
	}
	else
	{
		// Sample only ONE exterior axis at a time. The previous diagonal branch
		// exceeded the 40 cm inflated-capsule classification envelope when both
		// horizontal and vertical offsets were outside by up to 40 cm. This
		// generated unscored far_miss_excluded attempts instead of clear misses.
		// Alternating lateral / top-bottom covers the outside regions without
		// increasing the eligibility tolerance or altering the SSR query.
		const bool bLateralMiss = ((DebugSSRAutoAttemptCount / 2) % 2) == 0;
		const float MissExcessCm = DebugSSRAutoRandom.FRandRange(15.0f, 35.0f);
		if (bLateralMiss)
		{
			OffsetCm = Side * (RadiusCm + MissExcessCm);
			VerticalOffsetCm = VerticalSign * DebugSSRAutoRandom.FRandRange(0.0f, CylinderHalfHeightCm);
		}
		else
		{
			OffsetCm = Side * DebugSSRAutoRandom.FRandRange(0.0f, InnerRadiusCm);
			VerticalOffsetCm = VerticalSign * (HalfHeightCm + MissExcessCm);
		}
	}
	const FVector AimPoint = ReferenceCenter + Right * OffsetCm + FVector::UpVector * VerticalOffsetCm;
	const FRotator AimRotation = (AimPoint - Eye).Rotation();

	// Set the ACTUAL control rotation used by the pawn-eye gameplay shot ray,
	// then dispatch through PlayerController::HandleInput -> Avatar InputRouter
	// -> Equipment -> GAS. The server still validates every real shot normally.
	PC->SetControlRotation(AimRotation);
	++DebugSSRAutoAttemptCount;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRAuto ATTEMPT number=%d intended=%s target=%s offsetCm=%.1f verticalCm=%.1f"
		" ammoBefore=%d (actual class from SSRGameplay CLIENT/SERVER; DEBUG ONLY)",
		DebugSSRAutoAttemptCount, bAimForClearHit ? TEXT("clear_hit") : TEXT("clear_miss"),
		*GetNameSafe(Target), OffsetCm, VerticalOffsetCm, Firearm->GetCurrentMagazineAmmo());
	PC->HandleInput(FInputActionValue(true), ETriggerEvent::Started,
		EnclaveGameplayTags::Input_Item_Action_Primary, false);
}

void UEnclaveSSRTestComponent::StopDebugSSRStrafe(const TCHAR* Reason)
{
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (bDebugSSRStrafeEnabled)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerStrafe LOCAL STOP pawn=%s reason=%s elapsedSeconds=%.1f (debug only)",
			*GetNameSafe(PC->GetPawn()), Reason, DebugSSRStrafeElapsedSeconds);
	}
	// Release the original pawn's protection, even if possession changed.
	if (AEnclaveCharacter* Protected = DebugSSRStrafeProtectedPawn.Get())
	{
		const bool bManuallyProtected = DebugSSRManualProtectedPawn.Get() == Protected;
		if (!bManuallyProtected)
		{
			Protected->DebugSetSSRStrafeDamageSuppressed(false);
		}
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerStrafe LOCAL PROTECT pawn=%s damageSuppressed=%d manualProtection=%d (debug only)",
			*GetNameSafe(Protected), bManuallyProtected ? 1 : 0, bManuallyProtected ? 1 : 0);
	}
	DebugSSRStrafeProtectedPawn.Reset();
	bDebugSSRStrafeEnabled = false;
}

void UEnclaveSSRTestComponent::TickParticipant(float DeltaSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
	if (!IsValid(PC)) return;
	TickDebugSSRCrouch(DeltaSeconds);
	TickDebugSSRAutoFire(DeltaSeconds);
	if (!bDebugSSRStrafeEnabled)
	{
		return;
	}

	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(PC->GetPawn());
	UEnclaveCharacterMovementComponent* Movement = IsValid(Char) ? Cast<UEnclaveCharacterMovementComponent>(Char->GetCharacterMovement()) : nullptr;
	if (!PC->IsLocalController() || !IsValid(Char) || !Char->IsLocallyControlled() ||
		!IsValid(Movement) || !Movement->IsLocomotionEnabled())
	{
		StopDebugSSRStrafe(TEXT("pawn_or_movement_unavailable"));
		return;
	}

	DebugSSRStrafeElapsedSeconds += DeltaSeconds;
	if (DebugSSRStrafeElapsedSeconds >= FMath::Clamp(GetSSRStrafeDuration(), 10.0f, 180.0f))
	{
		StopDebugSSRStrafe(TEXT("timeout"));
		return;
	}

	if (DebugSSRStrafePauseSeconds > 0.0f)
	{
		DebugSSRStrafePauseSeconds = FMath::Max(0.0f, DebugSSRStrafePauseSeconds - DeltaSeconds);
		return;
	}

	const float OffsetCm = FVector::DotProduct(Char->GetActorLocation() - DebugSSRStrafeOrigin, DebugSSRStrafeRight);
	if ((DebugSSRStrafeDirectionSign > 0 && OffsetCm >= 250.0f) ||
		(DebugSSRStrafeDirectionSign < 0 && OffsetCm <= -250.0f))
	{
		DebugSSRStrafeDirectionSign *= -1;
		DebugSSRStrafePauseSeconds = 0.15f;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerStrafe LOCAL TURN pawn=%s offsetCm=%.1f newSign=%d (debug only)",
			*GetNameSafe(Char), OffsetCm, DebugSSRStrafeDirectionSign);
		return;
	}

	Char->AddMovementInput(DebugSSRStrafeRight, static_cast<float>(DebugSSRStrafeDirectionSign));
	if (DebugSSRStrafeElapsedSeconds >= DebugSSRStrafeNextStatusSeconds)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerStrafe LOCAL MOTION pawn=%s role=%d speedCmS=%.1f offsetCm=%.1f (debug only)",
			*GetNameSafe(Char), static_cast<int32>(Char->GetLocalRole()),
			Char->GetVelocity().Size2D(), OffsetCm);
		DebugSSRStrafeNextStatusSeconds += 2.0f;
	}
#endif
}

#endif
