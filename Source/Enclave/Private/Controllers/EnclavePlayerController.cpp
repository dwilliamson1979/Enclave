#include "Controllers/EnclavePlayerController.h"
#include "Characters/EnclaveCharacter.h"
#include "Characters/Components/EnclaveCharacterMovementComponent.h"
#include "Combat/EnclaveHitRegistrationSubsystem.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"
#include "Combat/EnclaveHistoricalCapsuleIntersection.h"
#include "Debug/EnclaveSSRPatrolComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/Core/EnclaveInputConfig.h"
#include "Input/Interfaces/EnclaveInputConfigProviderInterface.h"
#include "Input/Interfaces/EnclaveInputReceiverInterface.h"
#include "Combat/Components/EnclaveCombatFeedbackComponent.h"

#include "Debug/EnclaveDebug.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Math/RotationMatrix.h"
#include "HAL/IConsoleManager.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "TimerManager.h"
#if ENCLAVE_INCLUDE_DEBUG_CODE
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#endif

#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "GameplayTags/EnclaveEquipmentSlotTags.h"
#include "GameplayTags/EnclaveInputTags.h"
#include "Equipment/Weapons/EnclaveFirearmWeapon.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Inventory/Components/EnclaveInventoryComponent.h"
#if ENCLAVE_INCLUDE_DEBUG_CODE
#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"
#endif

// Deliberately independent of Enclave.Debug.FireTrace: one toggles the
// remote comparison, the other the existing authoritative-world line drawing.
static TAutoConsoleVariable<bool> CVarDebugShotAlignment(
	TEXT("Enclave.Debug.ShotAlignment"),
	false,
	TEXT("Compare the predicted owning-client camera ray to the exact authoritative hitscan ray for each GAS Fire activation. Enable on BOTH server and owning client in multi-process PIE."),
	ECVF_Cheat);

#if ENCLAVE_INCLUDE_DEBUG_CODE
static TAutoConsoleVariable<bool> CVarDebugSSRGameplayGate(
	TEXT("Enclave.Debug.SSRGameplayGate"), false,
	TEXT("Shadow-only capsule hit/miss gameplay classifier; use on remote client and listen server. NO damage authority."), ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarDebugSSRScenario(
	TEXT("Enclave.Debug.SSRScenario"), 0,
	TEXT("D1.1C scenario label on SHOOTER: 0=unassigned, 1=stationary, 2=strafe, 3=reverse, 4=crouch, 5=cover. Debug-only metadata."), ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarDebugSSRAutoSeed(
	TEXT("Enclave.Debug.SSRAutoSeed"), 1729,
	TEXT("D1.1B deterministic shot-test random seed; set before Enclave.Debug.SSRAutoFire 1."), ECVF_Cheat);

static TAutoConsoleVariable<float> CVarDebugSSRStrafeDuration(
	TEXT("Enclave.Debug.SSRStrafeDuration"), 30.0f,
	TEXT("Development-only local strafe rig duration (seconds, clamped to 10..180). Set before starting."), ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDebugSSREcho(
	TEXT("Enclave.Debug.SSREcho"), false,
	TEXT("Bounded diagnostic: echo the simulated target CharacterMovement update stamp and compare server history. No gameplay authority."), ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDebugSSRClock(
	TEXT("Enclave.Debug.SSRClock"),
	false,
	TEXT("Stage C4F: correlate client GameState shot clock with server clock via a same-process monotonic clock probe. Enable on both PIE client and server. Diagnostic-only; no effect on shots."),
	ECVF_Cheat);
#endif

// D1.1C.2: Dotted aliases let all SSR test controls use Enclave.Debug.*.
// Existing UFUNCTION(Exec) names remain available for older test scripts.
// A world-aware delegate is necessary in two-player PIE: never route a local
// shooter command to the listen-server pawn (or vice versa).
#if ENCLAVE_INCLUDE_DEBUG_CODE
namespace
{
	enum class EEnclaveSSRConsoleAction : uint8
	{
		Crouch,
		Strafe,
		Protect,
		AutoFire
	};

	void ExecuteEnclaveSSRConsoleAction(const TArray<FString>& Args, UWorld* World, EEnclaveSSRConsoleAction Action)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("0") && Args[0] != TEXT("1")))
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRConsole rejected=expected_single_0_or_1_arg (DEBUG ONLY)");
			return;
		}

		if (!IsValid(World) || !World->IsGameWorld())
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRConsole rejected=no_game_world (DEBUG ONLY)");
			return;
		}

		AEnclavePlayerController* LocalController = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AEnclavePlayerController* Candidate = Cast<AEnclavePlayerController>(It->Get());
			if (IsValid(Candidate) && Candidate->IsLocalController())
			{
				if (LocalController)
				{
					ENCLAVE_LOG(LogEnclaveCombat, Warning,
						"SSRConsole rejected=ambiguous_local_controller (DEBUG ONLY)");
					return;
				}
				LocalController = Candidate;
			}
		}
		if (!LocalController)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRConsole rejected=no_local_enclave_controller (DEBUG ONLY)");
			return;
		}

		const int32 bEnable = Args[0] == TEXT("1") ? 1 : 0;
		switch (Action)
		{
		case EEnclaveSSRConsoleAction::Crouch:
			LocalController->EnclaveDebugSSRCrouch(bEnable);
			break;
		case EEnclaveSSRConsoleAction::Strafe:
			LocalController->EnclaveDebugSSRStrafe(bEnable);
			break;
		case EEnclaveSSRConsoleAction::Protect:
			LocalController->EnclaveDebugSSRProtect(bEnable);
			break;
		case EEnclaveSSRConsoleAction::AutoFire:
			LocalController->EnclaveDebugSSRAutoFire(bEnable);
			break;
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRCrouch(
		TEXT("Enclave.Debug.SSRCrouch"), TEXT("Request/release debug crouch on the local authoritative target (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::Crouch), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRStrafe(
		TEXT("Enclave.Debug.SSRStrafe"), TEXT("Start/stop the local SSR lateral strafe rig (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::Strafe), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRProtect(
		TEXT("Enclave.Debug.SSRProtect"), TEXT("Enable/disable debug-only server-local target damage protection (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::Protect), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRAutoFire(
		TEXT("Enclave.Debug.SSRAutoFire"), TEXT("Start/stop remote-client seeded GAS-fire sampling (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::AutoFire), ECVF_Cheat);
}
#endif

bool AEnclavePlayerController::IsSSRClockDebugEnabled()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	return CVarDebugSSRClock.GetValueOnGameThread();
#else
	return false;
#endif
}

bool AEnclavePlayerController::IsSSRGameplayGateEnabled()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	return CVarDebugSSRGameplayGate.GetValueOnGameThread();
#else
	return false;
#endif
}

bool AEnclavePlayerController::DebugBuildSSRGameplayClaim(int32 PredictionKey, const FVector& ShotOrigin,
	const FVector& ShotDirection, FEnclaveSSRGameplayClaim& OutClaim) const
{
	OutClaim = FEnclaveSSRGameplayClaim();
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsSSRGameplayGateEnabled() || !IsLocalController() || !GetWorld() || !GetPawn())
	{
		return false;
	}

	const FVector Direction = ShotDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	const FVector RayEnd = ShotOrigin + Direction * 10000.0;
	constexpr float MarginCm = 10.0f;
	constexpr float MaximumClearMissCm = 40.0f;

	// Only consider targets intersecting the reference capsule inflated by 40 cm.
	// Beyond 40 cm is an excluded FAR MISS, not a trivially successful clear miss.
	// Select the earliest enlarged-capsule entry along the ray, not the closest
	// center: a tall capsule can be near the ray even when its center is not.
	AEnclaveCharacter* Candidate = nullptr;
	FEnclaveTargetCapsuleFrame SelectedReference;
	float SelectedSmoothingCm = 0.0f;
	double ClosestEntryCm = 10001.0;
	for (TActorIterator<AEnclaveCharacter> It(GetWorld()); It; ++It)
	{
		AEnclaveCharacter* Target = *It;
		if (!IsValid(Target) || Target == GetPawn() || Target->GetLocalRole() != ROLE_SimulatedProxy ||
			!IsValid(Target->GetCapsuleComponent()) || !IsValid(Target->GetMesh()))
		{
			continue;
		}

		const UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
		const FVector MeshSmoothOffset = Capsule->GetComponentTransform().TransformVectorNoScale(
			Target->GetMesh()->GetRelativeLocation() - Target->GetBaseTranslationOffset());
		FEnclaveTargetCapsuleFrame Reference;
		Reference.Center = Capsule->GetComponentLocation() + MeshSmoothOffset;
		Reference.Rotation = Capsule->GetComponentQuat();
		Reference.RadiusCm = Capsule->GetScaledCapsuleRadius();
		Reference.HalfHeightCm = Capsule->GetScaledCapsuleHalfHeight();
		if (Reference.RadiusCm <= MarginCm + 1.0f)
		{
			continue;
		}

		FEnclaveTargetCapsuleFrame Outer40 = Reference;
		Outer40.RadiusCm += MaximumClearMissCm;
		Outer40.HalfHeightCm += MaximumClearMissCm;
		double EntryCm = 0.0;
		FVector EntryPoint;
		if (FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
			ShotOrigin, RayEnd, Outer40, EntryCm, EntryPoint) && EntryCm < ClosestEntryCm)
		{
			ClosestEntryCm = EntryCm;
			Candidate = Target;
			SelectedReference = Reference;
			SelectedSmoothingCm = MeshSmoothOffset.Size();
		}
	}
	if (!IsValid(Candidate))
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRGameplay CLIENT key=%d status=far_miss_excluded (no capsule within 40cm; DEBUG ONLY)",
			PredictionKey);
		return false;
	}

	const FEnclaveSSRReceivedProxyUpdate& Update = Candidate->DebugGetLastReceivedProxyUpdate();
	const double Elapsed = GetWorld()->GetTimeSeconds() - Update.ClientReceiveWorldSeconds;
	const int32 Scenario = FMath::Clamp(CVarDebugSSRScenario.GetValueOnGameThread(), 0, 5);
	const bool bFreshUpdate = Update.ReceiveCount != 0 && FMath::IsFinite(Update.ServerStamp) &&
		Update.ServerStamp > 0.0f && FMath::IsFinite(Elapsed) && Elapsed >= 0.0 && Elapsed <= 0.080;
	// A truly stationary proxy may not replicate fresh movement at all. Preserve
	// its diagnostic classification and explicitly request PRESENT fallback;
	// the server must independently verify stationarity over historical frames.
	// Even without a fresh update, classify the shot and send an INVALID
	// timing claim (stamp=0). The server scores its real present-time fallback
	// instead of quietly dropping difficult shots from the denominator.
	// Scenario 1 can use the explicit server-verified stationary policy.

	// Mesh-anchored collision proxy is a test approximation, NOT the rendered
	// skeletal silhouette. The 10 cm inner/outer margins remain unchanged.
	FEnclaveTargetCapsuleFrame Inner = SelectedReference;
	Inner.RadiusCm -= MarginCm;
	Inner.HalfHeightCm -= MarginCm;
	FEnclaveTargetCapsuleFrame Outer10 = SelectedReference;
	Outer10.RadiusCm += MarginCm;
	Outer10.HalfHeightCm += MarginCm;
	double DistanceCm = 0.0;
	FVector Point;
	const bool bClearHit = FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
		ShotOrigin, RayEnd, Inner, DistanceCm, Point);
	const bool bWithin10Cm = FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
		ShotOrigin, RayEnd, Outer10, DistanceCm, Point);
	// A clear miss intersects Outer40 (candidate selection) but NOT Outer10.
	OutClaim.Classification = bClearHit ? 1 : (bWithin10Cm ? 3 : 2);
	OutClaim.Scenario = static_cast<uint8>(Scenario);
	OutClaim.Target = Candidate;
    const FEnclaveSSRReceivedProxyUpdate& Authoritative = Candidate->GetLastReceivedSSRObservation();
    const double ObservationAgeMs = (GetWorld()->GetTimeSeconds() - Authoritative.ClientReceiveWorldSeconds) * 1000.0;
    if (Authoritative.ReceiveCount > 0)
    {
        ENCLAVE_LOG(LogEnclaveCombat, Log,
            "SSRPairedObservation CLIENT target=%s authStamp=%.6f engineStamp=%.6f receiveAgeMs=%.2f locationDeltaCm=%.2f pairedCount=%u (STAGE A SHADOW; not used by shot claim)",
            *GetNameSafe(Candidate), Authoritative.ServerStamp, Update.ServerStamp, ObservationAgeMs,
            FVector::Dist(Authoritative.ReplicatedLocation, Update.ReplicatedLocation), Authoritative.ReceiveCount);
    }
    // D11C Stage B shadow-only: use the authoritative history-linked capsule
    // observation clock instead of the engine's movement timestamp. The paired
    // capsule is NOT yet proven to match the client's rendered target geometry.
    // Keep the existing classification and live hit authority unchanged.
    const double PairedElapsed = GetWorld()->GetTimeSeconds() - Authoritative.ClientReceiveWorldSeconds;
    const bool bFreshPairedObservation = Authoritative.ReceiveCount != 0 &&
        FMath::IsFinite(Authoritative.ServerStamp) && Authoritative.ServerStamp > 0.0f &&
        FMath::IsFinite(PairedElapsed) && PairedElapsed >= 0.0 && PairedElapsed <= 0.080;
    OutClaim.EchoedMovementStamp = bFreshPairedObservation ? Authoritative.ServerStamp : 0.0;
    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRStageB CLIENT key=%d target=%s authStamp=%.6f engineStamp=%.6f pairedAgeMs=%.2f engineAgeMs=%.2f pairedFresh=%d engineFresh=%d deltaCm=%.2f (SHADOW ONLY; capsule/render association unverified)",
        PredictionKey, *GetNameSafe(Candidate), Authoritative.ServerStamp, Update.ServerStamp,
        PairedElapsed * 1000.0, Elapsed * 1000.0, bFreshPairedObservation ? 1 : 0,
        bFreshUpdate ? 1 : 0, FVector::Dist(Authoritative.ReplicatedLocation, SelectedReference.Center));
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRGameplay CLIENT key=%d target=%s category=%d scenario=%d stamp=%.6f pairedAgeMs=%.1f engineAgeMs=%.1f smoothingCm=%.2f stationaryFallback=%d (stamp-only claim; 10cm hit/miss margin; clear misses 10-40cm outside capsule; approximate mesh anchor; DEBUG ONLY)",
		PredictionKey, *GetNameSafe(Candidate), static_cast<int32>(OutClaim.Classification), Scenario,
		OutClaim.EchoedMovementStamp,
		PairedElapsed * 1000.0, Elapsed * 1000.0, SelectedSmoothingCm, bFreshPairedObservation ? 0 : 1);
	return true;
#else
	(void)PredictionKey;
	(void)ShotOrigin;
	(void)ShotDirection;
	return false;
#endif
}

bool AEnclavePlayerController::IsShotAlignmentDebugEnabled()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	return CVarDebugShotAlignment.GetValueOnGameThread();
#else
	return false;
#endif
}

void AEnclavePlayerController::DebugCaptureLocalShot(int32 PredictionKey)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsShotAlignmentDebugEnabled() || !IsLocalController() || !IsValid(GetPawn()) || !GetWorld())
	{
		return;
	}

	// GetPlayerViewPoint reflects the player camera manager's rendered view,
	// including any camera/spring-arm offset. Never use the cosmetic ADS sight
	// socket as the gameplay source of this diagnostic.
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector LocalEyeLocation;
	FRotator LocalEyeRotation;
	GetPawn()->GetActorEyesViewPoint(LocalEyeLocation, LocalEyeRotation);
	if (const AEnclaveCharacter* EnclaveCharacter = Cast<AEnclaveCharacter>(GetPawn()))
	{
		EnclaveCharacter->DebugLogViewGeometryForShot(PredictionKey, ViewLocation);
	}
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"ShotAlign LOCAL key=%d cameraToPawnEyes=%s localEyeAngle=%.3fdeg",
		PredictionKey, *(LocalEyeLocation - ViewLocation).ToCompactString(),
		FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ViewRotation.Vector(), LocalEyeRotation.Vector()), -1.0f, 1.0f))));

	FDebugShotRay Ray;
	Ray.Start = ViewLocation;
	Ray.End = ViewLocation + ViewRotation.Vector() * 10000.0f; // Diagnostic length only: 100 m.
	Ray.RecordedTime = GetWorld()->GetTimeSeconds();
	DebugPruneShotHistory();
	PendingLocalShots.Add(PredictionKey, Ray);

	DrawDebugLine(GetWorld(), Ray.Start, Ray.End, FColor::Cyan, false, 4.0f, 0, 1.3f);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"ShotAlign CLIENT key=%d origin=%s dir=%s", PredictionKey,
		*Ray.Start.ToCompactString(), *(Ray.End - Ray.Start).GetSafeNormal().ToCompactString());
	DebugCompareShotIfReady(PredictionKey);
#endif
}

void AEnclavePlayerController::DebugProbeSSRShotClock(int32 PredictionKey, double ClientShotServerTimeSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsSSRClockDebugEnabled() || !IsLocalController() || !GetWorld() ||
		!FMath::IsFinite(ClientShotServerTimeSeconds) || ClientShotServerTimeSeconds < 0.0)
	{
		return;
	}

	// Called immediately after GA_Fire reads the client GameState clock.
	// Capture the monotonic timestamp adjacent to that clock read. Neither
	// this RPC nor its payload participates in shot validation or hit authority.
	const double ClientProcessSeconds = FPlatformTime::Seconds();
	const double ClientWorldSeconds = GetWorld()->GetTimeSeconds();
	const uint32 ClientPid = FPlatformProcess::GetCurrentProcessId();
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRClockProbe CLIENT key=%d pid=%u shotEstimate=%.6f localWorld=%.6f clientEstimateMinusLocalWorldMs=%+.1f processCapture=%.6f (DEBUG ONLY; independent of GAS target data)",
		PredictionKey, ClientPid, ClientShotServerTimeSeconds, ClientWorldSeconds,
		(ClientShotServerTimeSeconds - ClientWorldSeconds) * 1000.0, ClientProcessSeconds);
	ServerEnclaveDebugShotClockProbe(PredictionKey, ClientPid, ClientProcessSeconds,
		ClientWorldSeconds, ClientShotServerTimeSeconds);
#else
	(void)PredictionKey;
	(void)ClientShotServerTimeSeconds;
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugShotClockProbe_Implementation(int32 PredictionKey,
	uint32 ClientProcessId, double ClientCaptureProcessSeconds, double ClientCaptureWorldSeconds,
	double ClientEstimatedServerSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsSSRClockDebugEnabled() || !HasAuthority() || !GetWorld() ||
		!FMath::IsFinite(ClientCaptureProcessSeconds) || ClientCaptureProcessSeconds < 0.0 ||
		!FMath::IsFinite(ClientCaptureWorldSeconds) || ClientCaptureWorldSeconds < 0.0 ||
		!FMath::IsFinite(ClientEstimatedServerSeconds) || ClientEstimatedServerSeconds < 0.0)
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!IsValid(GameState))
	{
		return;
	}
	const double ServerReceiptProcessSeconds = FPlatformTime::Seconds();
	const double ServerWorldSeconds = GameState->GetServerWorldTimeSeconds();
	const uint32 ServerPid = FPlatformProcess::GetCurrentProcessId();
	const double ElapsedProcessSeconds = ServerReceiptProcessSeconds - ClientCaptureProcessSeconds;
	const bool bComparable = ClientProcessId == ServerPid &&
		ElapsedProcessSeconds >= 0.0 && ElapsedProcessSeconds <= 0.500;

	// This projection is a DEBUG approximation: assumes unpaused world time
	// advances at the same rate as real monotonic time over the RPC's flight.
	// It is NOT valid across processes, time dilation, or large frame stalls.
	const double EstimatedServerAtClientCapture = bComparable
		? ServerWorldSeconds - ElapsedProcessSeconds : -1.0;
	const double ClockEstimateLeadMs = bComparable
		? (ClientEstimatedServerSeconds - EstimatedServerAtClientCapture) * 1000.0 : 0.0;
	const APlayerState* ShooterPS = GetPlayerState<APlayerState>();
	const double RTTMs = IsValid(ShooterPS) ? ShooterPS->GetPingInMilliseconds() : -1.0;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRClockProbe SERVER key=%d clientPid=%u serverPid=%u mode=%s clockLeadValid=%d estimatedClockLeadMs=%+.1f captureToProbeMs=%.1f apparentShotAgeAtProbeMs=%+.1f clientEstimate=%.6f serverAtProbe=%.6f serverAtCaptureProjected=%.6f clientWorld=%.6f serverWorld=%.6f playerStateRTTMs=%.1f (UNTRUSTED DEBUG RPC; process projection is not render timing or gameplay authority)",
		PredictionKey, ClientProcessId, ServerPid,
		bComparable ? TEXT("shared_process") : TEXT("not_comparable"), bComparable ? 1 : 0, ClockEstimateLeadMs,
		bComparable ? ElapsedProcessSeconds * 1000.0 : -1.0,
		(ServerWorldSeconds - ClientEstimatedServerSeconds) * 1000.0,
		ClientEstimatedServerSeconds, ServerWorldSeconds, EstimatedServerAtClientCapture,
		ClientCaptureWorldSeconds, GetWorld()->GetTimeSeconds(), RTTMs);
#else
	(void)PredictionKey;
	(void)ClientProcessId;
	(void)ClientCaptureProcessSeconds;
	(void)ClientCaptureWorldSeconds;
	(void)ClientEstimatedServerSeconds;
#endif
}

void AEnclavePlayerController::DebugProbeSSREchoedSnapshot(int32 PredictionKey)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!CVarDebugSSREcho.GetValueOnGameThread() || !IsLocalController() || !GetWorld() || !GetPawn())
	{
		return;
	}

	FVector ViewStart;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewStart, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	AEnclaveCharacter* BestTarget = nullptr;
	double BestPerpendicularSq = FMath::Square(125.0);
	for (TActorIterator<AEnclaveCharacter> It(GetWorld()); It; ++It)
	{
		AEnclaveCharacter* Target = *It;
		if (!IsValid(Target) || Target == GetPawn() ||
			Target->GetLocalRole() != ROLE_SimulatedProxy || !Target->GetCapsuleComponent())
		{
			continue;
		}
		const FVector ToTarget = Target->GetCapsuleComponent()->GetComponentLocation() - ViewStart;
		const double Along = FVector::DotProduct(ToTarget, ViewDirection);
		if (Along <= 0.0 || Along > 10000.0)
		{
			continue;
		}
		const double PerpendicularSq = (ToTarget - ViewDirection * Along).SizeSquared();
		if (PerpendicularSq < BestPerpendicularSq)
		{
			BestPerpendicularSq = PerpendicularSq;
			BestTarget = Target;
		}
	}
	if (!IsValid(BestTarget) || !BestTarget->GetMesh() || !BestTarget->GetCharacterMovement())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSREcho CLIENT key=%d status=no_proxy_near_ray (DEBUG ONLY)", PredictionKey);
		return;
	}

	const FEnclaveSSRReceivedProxyUpdate& Update = BestTarget->DebugGetLastReceivedProxyUpdate();
	if (Update.ReceiveCount == 0 || Update.ServerStamp <= 0.0f ||
		!FMath::IsFinite(Update.ClientReceiveWorldSeconds))
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSREcho CLIENT key=%d status=no_paired_movement_update (DEBUG ONLY)", PredictionKey);
		return;
	}

	const double Elapsed = GetWorld()->GetTimeSeconds() - Update.ClientReceiveWorldSeconds;
	if (!FMath::IsFinite(Elapsed) || Elapsed < 0.0 || Elapsed > 0.150)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSREcho CLIENT key=%d status=invalid_update_elapsed elapsedMs=%.2f (DEBUG ONLY)", PredictionKey, Elapsed * 1000.0);
		return;
	}
	const UCapsuleComponent* Capsule = BestTarget->GetCapsuleComponent();
	const FVector CapsuleCenter = Capsule->GetComponentLocation();
	const FVector MeshDisplacement = Capsule->GetComponentTransform().TransformVectorNoScale(
		BestTarget->GetMesh()->GetRelativeLocation() - BestTarget->GetBaseTranslationOffset());
	const FVector MeshAnchor = CapsuleCenter + MeshDisplacement;
	const FVector ClientLinearPrediction = Update.ReplicatedLocation + Update.ReplicatedVelocity * Elapsed;
	const double LocalErrorCm = FVector::Dist(ClientLinearPrediction, CapsuleCenter);
	const double UpdateReceiveErrorCm = FVector::Dist(Update.ReplicatedLocation, Update.CapsuleAtReceive);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSREcho CLIENT key=%d target=%s pairedStamp=%.6f currentStamp=%.6f updates=%u elapsedMs=%.2f localLinearErrorCm=%.2f receivedPosErrorCm=%.2f recvSpeedCmS=%.1f localSpeedCmS=%.1f smoothingCm=%.2f mode=%d (LOCAL GAME-THREAD sample, not rendered pixels; DEBUG ONLY)",
		PredictionKey, *GetNameSafe(BestTarget), Update.ServerStamp,
		BestTarget->GetReplicatedServerLastTransformUpdateTimeStamp(), Update.ReceiveCount,
		Elapsed * 1000.0, LocalErrorCm, UpdateReceiveErrorCm,
		Update.ReplicatedVelocity.Size(), BestTarget->GetVelocity().Size(), MeshDisplacement.Size(),
		static_cast<int32>(BestTarget->GetCharacterMovement()->NetworkSmoothingMode));
	ServerEnclaveDebugEchoedSnapshot(PredictionKey, BestTarget, Update.ServerStamp, static_cast<float>(Elapsed),
		FVector_NetQuantize10(Update.ReplicatedLocation), FVector_NetQuantize10(Update.ReplicatedVelocity),
		FVector_NetQuantize10(Update.CapsuleAtReceive), FVector_NetQuantize10(CapsuleCenter),
		FVector_NetQuantize10(MeshAnchor));
#else
	(void)PredictionKey;
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugEchoedSnapshot_Implementation(int32 PredictionKey,
	AEnclaveCharacter* Target, double EchoedMovementStamp, float ElapsedSinceUpdateSeconds,
	FVector_NetQuantize10 ReplicatedLocation, FVector_NetQuantize10 ReplicatedVelocity,
	FVector_NetQuantize10 CapsuleAtReceive, FVector_NetQuantize10 ClientCapsuleAtShot,
	FVector_NetQuantize10 ClientMeshAnchor)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!HasAuthority() || !CVarDebugSSREcho.GetValueOnGameThread() || !GetWorld() ||
		!IsValid(Target) || Target->GetWorld() != GetWorld() || Target == GetPawn() ||
		!FMath::IsFinite(EchoedMovementStamp) || EchoedMovementStamp <= 0.0f ||
		!FMath::IsFinite(ElapsedSinceUpdateSeconds) || ElapsedSinceUpdateSeconds < 0.0f ||
		ElapsedSinceUpdateSeconds > 0.150f || FVector(ReplicatedLocation).ContainsNaN() ||
		FVector(ReplicatedVelocity).ContainsNaN() || FVector(ClientCapsuleAtShot).ContainsNaN())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSREcho RECON key=%d status=invalid_claim (DEBUG ONLY)", PredictionKey);
		return;
	}

	const UEnclaveTargetHistoryComponent* History = Target->FindComponentByClass<UEnclaveTargetHistoryComponent>();
	const double Receipt = GetWorld()->GetTimeSeconds();
	const double Stamp = EchoedMovementStamp;
	const double Elapsed = static_cast<double>(ElapsedSinceUpdateSeconds);
	const double AgeMs = (Receipt - Stamp) * 1000.0;
	constexpr double MaxEchoAgeSeconds = 0.200; // Existing C4 budget; no authority changes.
	const bool bInWindow = Stamp <= Receipt &&
		Stamp >= Receipt - MaxEchoAgeSeconds;

	FEnclaveTargetCapsuleFrame Base;
	FEnclaveTargetCapsuleFrame ElapsedFrame;
	const double QueryStamp = Stamp;
	bool bBaseFound = false;
	bool bElapsedFound = false;
	const bool bSnappedToEndpoint = false; // Strict double timestamp: never snap.
	if (bInWindow && IsValid(History))
	{
		bBaseFound = History->TryGetCapsuleAtTime(QueryStamp, Base);
		// A: Actual authoritative history at later time, never extrapolated.
		bElapsedFound = History->TryGetCapsuleAtTime(QueryStamp + Elapsed, ElapsedFrame);
	}

	const FVector ActualClientCapsule = FVector(ClientCapsuleAtShot);
	const FVector ReconstructedFromReceived = FVector(ReplicatedLocation) + FVector(ReplicatedVelocity) * Elapsed;
	const double ClientModelErrorCm = FVector::Dist(ReconstructedFromReceived, ActualClientCapsule);
	const double ReceivedVsCapsuleAtReceiveCm = FVector::Dist(FVector(ReplicatedLocation), FVector(CapsuleAtReceive));
	const double StampPairErrorCm = bBaseFound ? FVector::Dist(FVector(ReplicatedLocation), Base.Center) : -1.0;
	const double BaseErrorCm = bBaseFound ? FVector::Dist(Base.Center, ActualClientCapsule) : -1.0;
	const double ActualHistoryErrorCm = bElapsedFound ? FVector::Dist(ElapsedFrame.Center, ActualClientCapsule) : -1.0;
	const FVector ServerLinearPrediction = bBaseFound ? Base.Center + Base.DebugVelocity * Elapsed : FVector::ZeroVector;
	const double ServerModelErrorCm = bBaseFound ? FVector::Dist(ServerLinearPrediction, ActualClientCapsule) : -1.0;
	const double MeshVsServerModelCm = bBaseFound ? FVector::Dist(ServerLinearPrediction, FVector(ClientMeshAnchor)) : -1.0;
	const double ServerSpeedCmS = bBaseFound ? Base.DebugVelocity.Size() : -1.0;
	const bool bDirectionFlip = bBaseFound && !Target->GetVelocity().IsNearlyZero() &&
		!Base.DebugVelocity.IsNearlyZero() && FVector::DotProduct(Base.DebugVelocity, Target->GetVelocity()) < 0.0;
	const bool bPossibleTurnaround = bDirectionFlip || (bBaseFound && ServerSpeedCmS < 400.0);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSREcho RECON key=%d status=%s target=%s ageMs=%.2f elapsedMs=%.2f stamp=%.6f snapped=%d baseFound=%d elapsedFound=%d clientModelErrorCm=%.2f serverModelErrorCm=%.2f actualHistoryErrorCm=%.2f baselineErrorCm=%.2f meshServerModelErrorCm=%.2f receivedPositionDeltaCm=%.2f stampPairErrorCm=%.2f serverSpeedCmS=%.1f turnaroundSuspect=%d (UNTRUSTED CLAIM; GAME-THREAD PROXY, NOT RENDERED; SHADOW ONLY)",
		PredictionKey, !bInWindow ? TEXT("out_of_window") : (!bBaseFound ? TEXT("missing_history") : TEXT("measured")),
		*GetNameSafe(Target), AgeMs, Elapsed * 1000.0, Stamp, bSnappedToEndpoint ? 1 : 0,
		bBaseFound ? 1 : 0, bElapsedFound ? 1 : 0, ClientModelErrorCm, ServerModelErrorCm,
		ActualHistoryErrorCm, BaseErrorCm, MeshVsServerModelCm, ReceivedVsCapsuleAtReceiveCm,
		StampPairErrorCm, ServerSpeedCmS, bPossibleTurnaround ? 1 : 0);
#else
	(void)PredictionKey;
	(void)Target;
	(void)EchoedMovementStamp;
	(void)ElapsedSinceUpdateSeconds;
	(void)ReplicatedLocation;
	(void)ReplicatedVelocity;
	(void)CapsuleAtReceive;
	(void)ClientCapsuleAtShot;
	(void)ClientMeshAnchor;
#endif
}

void AEnclavePlayerController::DebugLogSSRProxySample(int32 PredictionKey, double ClientShotServerTimeSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!UEnclaveHitRegistrationSubsystem::IsSSRPreflightDebugEnabled() || !IsLocalController() || !GetPawn() || !GetWorld())
	{
		return;
	}

	// Explicitly sample a Stage C0 patrol target, not the nearest player. A stationary
	// human simulated proxy may be closer than the moving bot and silently hide
	// the data we are trying to collect. Blueprint-added components are present
	// on the client even when the component itself does not replicate properties.
	AEnclaveCharacter* Closest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();
	int32 SimulatedPatrolCount = 0;
	int32 OtherPatrolCount = 0;
	for (TActorIterator<AEnclaveCharacter> It(GetWorld()); It; ++It)
	{
		AEnclaveCharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == GetPawn() ||
			!Candidate->FindComponentByClass<UEnclaveSSRPatrolComponent>())
		{
			continue;
		}
		if (Candidate->GetLocalRole() != ROLE_SimulatedProxy)
		{
			++OtherPatrolCount;
			continue;
		}
		++SimulatedPatrolCount;
		const double DistSq = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Closest = Candidate;
		}
	}

	const AGameStateBase* GS = GetWorld()->GetGameState();
	const double ServerTimeEstimate = GS ? GS->GetServerWorldTimeSeconds() : -1.0;
	if (!Closest || !Closest->GetMesh() || !Closest->GetCapsuleComponent() || !Closest->GetCharacterMovement())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPreflight PROXY key=%d no simulated SSR patrol target available (simulatedPatrols=%d otherRolePatrols=%d); check BP patrol component, actor replication and client role; clientServerTimeEstimate=%.6f",
			PredictionKey, SimulatedPatrolCount, OtherPatrolCount, ServerTimeEstimate);
		return;
	}

	const USkeletalMeshComponent* Mesh = Closest->GetMesh();
	const UCapsuleComponent* Capsule = Closest->GetCapsuleComponent();
	const UCharacterMovementComponent* Move = Closest->GetCharacterMovement();
	const FVector BaseMeshRelative = Closest->GetBaseTranslationOffset();
	const FVector CurrentMeshRelative = Mesh->GetRelativeLocation();
	const FVector MeshSmoothingDeltaWorld = Capsule->GetComponentTransform().TransformVectorNoScale(CurrentMeshRelative - BaseMeshRelative);
	// Compare matching anatomical anchors: capsule center plus the cosmetic
	// mesh smoothing displacement. Raw Mesh->GetComponentLocation() contains
	// the designed skeletal-mesh base offset and is NOT a capsule center.
	// This is a game-thread visual-proxy estimate, not a pixel/render timestamp.
	const FVector VisualProxyAnchor = Capsule->GetComponentLocation() + MeshSmoothingDeltaWorld;
	// Capture AFTER reading the visual anchor, before dispatching the unreliable
	// RPC. In same-process PIE this monotonic value shares a clock with server
	// history; in separate-process or remote play it must not be compared.
	const double ClientVisualCaptureProcessSeconds = FPlatformTime::Seconds();
	const uint32 ClientProcessId = FPlatformProcess::GetCurrentProcessId();
	const double ClientVisualSampleWorldSeconds = GetWorld()->GetTimeSeconds();
	const double ClientVisualSampleServerEstimateSeconds = GS ? GS->GetServerWorldTimeSeconds() : -1.0;
	if (FMath::IsFinite(ClientShotServerTimeSeconds) && ClientShotServerTimeSeconds >= 0.0)
	{
		ServerEnclaveDebugPatrolVisualSample(PredictionKey, Closest,
			FVector_NetQuantize10(VisualProxyAnchor), ClientShotServerTimeSeconds,
			ClientProcessId, ClientVisualCaptureProcessSeconds,
			ClientVisualSampleWorldSeconds, ClientVisualSampleServerEstimateSeconds);
	}
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRTimingClock CLIENT key=%d pid=%u shotEstimate=%.6f visualEstimate=%.6f localWorld=%.6f gameStateDeltaMs=%+.1f shotToVisualMs=%+.1f visualProcessTime=%.6f (DEBUG ONLY; process clock comparable only when server PID matches)",
		PredictionKey, ClientProcessId, ClientShotServerTimeSeconds,
		ClientVisualSampleServerEstimateSeconds, ClientVisualSampleWorldSeconds,
		(ClientVisualSampleServerEstimateSeconds - ClientVisualSampleWorldSeconds) * 1000.0,
		(ClientVisualSampleServerEstimateSeconds - ClientShotServerTimeSeconds) * 1000.0,
		ClientVisualCaptureProcessSeconds);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRPreflight PROXY key=%d patrolTarget=%s distanceCm=%.1f capsuleWorld=%s meshSmoothingDeltaWorld=%s meshSmoothingMagnitudeCm=%.2f targetSpeedCmS=%.1f smoothingMode=%d smoothingComplete=%d configuredSmoothLocationMs=%.1f configuredSmoothRotationMs=%.1f clientServerTimeEstimate=%.6f (MESH OFFSET ONLY; not visual-time delay)",
		PredictionKey, *GetNameSafe(Closest), FMath::Sqrt(NearestDistSq),
		*Capsule->GetComponentLocation().ToCompactString(), *MeshSmoothingDeltaWorld.ToCompactString(),
		MeshSmoothingDeltaWorld.Size(), Closest->GetVelocity().Size(),
		static_cast<int32>(Move->NetworkSmoothingMode), Move->bNetworkSmoothingComplete ? 1 : 0,
		Move->NetworkSimulatedSmoothLocationTime * 1000.0, Move->NetworkSimulatedSmoothRotationTime * 1000.0,
		ServerTimeEstimate);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRVisual CLIENT key=%d patrolTarget=%s visualProxyAnchor=%s clientShotTime=%.6f (DEBUG RPC, game-thread mesh anchor; not authoritative)",
		PredictionKey, *GetNameSafe(Closest), *VisualProxyAnchor.ToCompactString(), ClientShotServerTimeSeconds);
#else
	(void)PredictionKey;
	(void)ClientShotServerTimeSeconds;
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugPatrolVisualSample_Implementation(int32 PredictionKey,
	AEnclaveCharacter* PatrolTarget, FVector_NetQuantize10 ClientVisualProxyAnchor, double ClientShotServerTimeSeconds,
	uint32 ClientProcessId, double ClientVisualCaptureProcessSeconds,
	double ClientVisualSampleWorldSeconds, double ClientVisualSampleServerEstimateSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!HasAuthority() || !UEnclaveHitRegistrationSubsystem::IsSSRPreflightDebugEnabled() ||
		!GetWorld() || !IsValid(PatrolTarget) || PatrolTarget->GetWorld() != GetWorld() ||
		!PatrolTarget->FindComponentByClass<UEnclaveSSRPatrolComponent>() ||
		!FMath::IsFinite(ClientShotServerTimeSeconds) || ClientShotServerTimeSeconds < 0.0 ||
		!FMath::IsFinite(ClientVisualCaptureProcessSeconds) || ClientVisualCaptureProcessSeconds < 0.0 ||
		!FMath::IsFinite(ClientVisualSampleWorldSeconds) || ClientVisualSampleWorldSeconds < 0.0 ||
		!FMath::IsFinite(ClientVisualSampleServerEstimateSeconds) ||
		ClientVisualSampleServerEstimateSeconds < 0.0 ||
		FVector(ClientVisualProxyAnchor).ContainsNaN())
	{
		return;
	}

	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!IsValid(GS))
	{
		return;
	}

	const FVector CurrentServerCapsuleCenter = PatrolTarget->GetCapsuleComponent()
		? PatrolTarget->GetCapsuleComponent()->GetComponentLocation()
		: PatrolTarget->GetActorLocation();
	const FVector TargetVelocity = PatrolTarget->GetVelocity();
	const FVector SpatialDelta = CurrentServerCapsuleCenter - FVector(ClientVisualProxyAnchor);
	const double SampleReceiptTime = GS->GetServerWorldTimeSeconds();
	const double ServerReceiptProcessSeconds = FPlatformTime::Seconds();
	const uint32 ServerProcessId = FPlatformProcess::GetCurrentProcessId();
	const bool bSharedProcessClock = ClientProcessId == ServerProcessId &&
		ClientVisualCaptureProcessSeconds <= ServerReceiptProcessSeconds &&
		ServerReceiptProcessSeconds - ClientVisualCaptureProcessSeconds < 0.5;
	const UEnclaveTargetHistoryComponent* ReceiptHistory =
		PatrolTarget->FindComponentByClass<UEnclaveTargetHistoryComponent>();
	double OldestHistoryTime = 0.0;
	double NewestHistoryTime = 0.0;
	const bool bHasHistory = IsValid(ReceiptHistory) && ReceiptHistory->GetHistoryTimeBounds(
		OldestHistoryTime, NewestHistoryTime);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRTimingClock SERVER key=%d clientPid=%u serverPid=%u mode=%s serverWorld=%.6f serverEstimate=%.6f serverGameStateDeltaMs=%+.1f visualEstimate=%.6f reportedShotAgeMs=%.1f latestHistoryAgeMs=%.1f captureToReceiptProcessMs=%.1f (DEBUG ONLY; mono comparable ONLY for same process)",
		PredictionKey, ClientProcessId, ServerProcessId,
		bSharedProcessClock ? TEXT("shared_process") : TEXT("not_comparable"),
		GetWorld()->GetTimeSeconds(), SampleReceiptTime,
		(SampleReceiptTime - GetWorld()->GetTimeSeconds()) * 1000.0,
		ClientVisualSampleServerEstimateSeconds,
		(SampleReceiptTime - ClientShotServerTimeSeconds) * 1000.0,
		bHasHistory ? (SampleReceiptTime - NewestHistoryTime) * 1000.0 : -1.0,
		bSharedProcessClock ? (ServerReceiptProcessSeconds - ClientVisualCaptureProcessSeconds) * 1000.0 : -1.0);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRVisual SERVER key=%d patrolTarget=%s clientVisualProxyAnchor=%s serverCapsuleAtProbeReceipt=%s delta=%s deltaCm=%.2f serverTargetSpeedCmS=%.1f clientShotAgeAtProbeReceiptMs=%.1f (UNTRUSTED SAMPLE; independent RPC; diagnostic only)",
		PredictionKey, *GetNameSafe(PatrolTarget), *FVector(ClientVisualProxyAnchor).ToCompactString(),
		*CurrentServerCapsuleCenter.ToCompactString(), *SpatialDelta.ToCompactString(), SpatialDelta.Size(),
		TargetVelocity.Size(), (SampleReceiptTime - ClientShotServerTimeSeconds) * 1000.0);

	// Stage C4: postpone this DEBUG-ONLY comparison briefly so authoritative
	// history includes both sides of the client-supplied shot timestamp. A
	// same-frame comparison cannot see the later server positions that previous
	// patrol logs suggested were closest to the client-visible anchor.
	// The RPC is untrusted; this inverse lookup is NOT a gameplay clock source.
	if (PendingSSRVisualTimingMatches >= 8 ||
		FMath::Abs(SampleReceiptTime - ClientShotServerTimeSeconds) > 0.500)
	{
		return;
	}
	++PendingSSRVisualTimingMatches;
	const TWeakObjectPtr<AEnclaveCharacter> WeakPatrol = PatrolTarget;
	const FVector VisualAnchor = FVector(ClientVisualProxyAnchor);
	const double ShotTime = ClientShotServerTimeSeconds;
	const double ReceiptTime = SampleReceiptTime;
	const int32 ShotKey = PredictionKey;
	// Debug-only telemetry. This capture is never used to choose a rewind time.
	const double VisualCaptureProcessSeconds = ClientVisualCaptureProcessSeconds;
	const double VisualSampleWorldSeconds = ClientVisualSampleWorldSeconds;
	const double VisualSampleEstimateSeconds = ClientVisualSampleServerEstimateSeconds;
	const bool bSameProcess = bSharedProcessClock;
	const double ReceiptProcessSeconds = ServerReceiptProcessSeconds;
	FTimerDelegate TimingMatchDelegate = FTimerDelegate::CreateWeakLambda(this,
		[this, WeakPatrol, VisualAnchor, ShotTime, ReceiptTime, ShotKey,
		 VisualCaptureProcessSeconds, VisualSampleWorldSeconds, VisualSampleEstimateSeconds,
		 bSameProcess, ReceiptProcessSeconds]()
		{
			--PendingSSRVisualTimingMatches;
			const AEnclaveCharacter* Target = WeakPatrol.Get();
			if (!IsValid(Target) || !GetWorld() || !GetWorld()->GetGameState())
			{
				return;
			}
			const UEnclaveTargetHistoryComponent* History =
				Target->FindComponentByClass<UEnclaveTargetHistoryComponent>();
			FEnclaveVisualHistoryTimeMatch Match;
			if (!IsValid(History) || !History->TryMatchVisualAnchorToHistory(
				VisualAnchor, ShotTime, 0.100, 0.100, Match))
			{
				ENCLAVE_LOG(LogEnclaveCombat, Log,
					"SSRTimingMatch SERVER key=%d patrolTarget=%s status=no_matchable_history shotTime=%.6f receiptAgeMs=%.1f (DEBUG ONLY; no gameplay timing)",
					ShotKey, *GetNameSafe(Target), ShotTime, (ReceiptTime - ShotTime) * 1000.0);
				return;
			}

			const bool bPlausibleFit = !Match.bAmbiguous && Match.ResidualCm <= 5.0;
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRTimingMatch SERVER key=%d patrolTarget=%s status=%s clientShotTime=%.6f matchedServerTime=%.6f signedMatchOffsetMs=%+.1f apparentVisualAgeMs=%+.1f residualCm=%.2f serverTargetSpeedCmS=%.1f segments=%d ambiguous=%d receiptAgeMs=%.1f (UNTRUSTED PATROL TELEMETRY; signed trajectory fit, NOT presentation delay or gameplay authority)",
				ShotKey, *GetNameSafe(Target),
				bPlausibleFit ? TEXT("fitted") : TEXT("inconclusive"), ShotTime,
				Match.MatchedServerTimeSeconds, Match.SignedOffsetSeconds * 1000.0,
				-Match.SignedOffsetSeconds * 1000.0, Match.ResidualCm,
				Match.MatchedSpeedCmPerSecond, Match.EvaluatedSegments,
				Match.bAmbiguous ? 1 : 0, (ReceiptTime - ShotTime) * 1000.0);
			// The inverse trajectory fit now includes when its server motion frames
			// were actually sampled, not just what World.Time was assigned to them.
			// Never compare monotonic times from different processes (or hosts).
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRTimingPhase SERVER key=%d status=%s mode=%s matchMinusClientCaptureMs=%+.1f receiptMinusClientCaptureMs=%.1f matchMinusReceiptMs=%+.1f matchWorldMinusClientEstimateMs=%+.1f clientGameStateDeltaMs=%+.1f historyFrames=%llu..%llu (DEBUG ONLY; game-thread anchors, not render timestamps; no gameplay authority)",
				ShotKey, bPlausibleFit ? TEXT("fitted") : TEXT("inconclusive"),
				bSameProcess ? TEXT("shared_process") : TEXT("not_comparable"),
				bSameProcess ? (Match.DebugMatchedProcessSeconds - VisualCaptureProcessSeconds) * 1000.0 : -1.0,
				bSameProcess ? (ReceiptProcessSeconds - VisualCaptureProcessSeconds) * 1000.0 : -1.0,
				bSameProcess ? (Match.DebugMatchedProcessSeconds - ReceiptProcessSeconds) * 1000.0 : -1.0,
				(Match.DebugMatchedWorldSeconds - VisualSampleEstimateSeconds) * 1000.0,
				(VisualSampleEstimateSeconds - VisualSampleWorldSeconds) * 1000.0,
				static_cast<unsigned long long>(Match.DebugOlderFrame),
				static_cast<unsigned long long>(Match.DebugNewerFrame));
		});
	FTimerHandle TimingMatchTimer;
	GetWorld()->GetTimerManager().SetTimer(TimingMatchTimer, TimingMatchDelegate, 0.120f, false);

	// C2 shadow lookup at the client's estimated shot CAPTURE time, not a
	// rewind decision. Client-visible geometry is usually older even at that
	// instant. This residual is NOT the effective target presentation delay.
	if (const UEnclaveTargetHistoryComponent* History = PatrolTarget->FindComponentByClass<UEnclaveTargetHistoryComponent>())
	{
		FEnclaveTargetCapsuleFrame ShotTimeFrame;
		if (History->TryGetCapsuleAtTime(ClientShotServerTimeSeconds, ShotTimeFrame))
		{
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRVisual HISTORY key=%d patrolTarget=%s serverCapsuleAtClientShot=%s clientVisualDeltaAtShotCm=%.2f frames=%d (SHOT-TIME SHADOW LOOKUP ONLY; not visual-delay estimate or hit authority)",
				PredictionKey, *GetNameSafe(PatrolTarget), *ShotTimeFrame.Center.ToCompactString(),
				FVector::Dist(ShotTimeFrame.Center, FVector(ClientVisualProxyAnchor)),
				History->GetHistorySampleCount());
		}
		else
		{
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRVisual HISTORY key=%d patrolTarget=%s unavailable (no bracketed continuous capsule history at client shot time)",
				PredictionKey, *GetNameSafe(PatrolTarget));
		}
	}
#else
	(void)PredictionKey;
	(void)PatrolTarget;
	(void)ClientVisualProxyAnchor;
	(void)ClientShotServerTimeSeconds;
	(void)ClientProcessId;
	(void)ClientVisualCaptureProcessSeconds;
	(void)ClientVisualSampleWorldSeconds;
	(void)ClientVisualSampleServerEstimateSeconds;
#endif
}

void AEnclavePlayerController::DebugBeginAuthoritativeShot(int32 PredictionKey)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (HasAuthority() && IsShotAlignmentDebugEnabled())
	{
		ActiveServerShotKey = PredictionKey;
	}
#endif
}

void AEnclavePlayerController::DebugEndAuthoritativeShot()
{
	ActiveServerShotKey = INDEX_NONE;
}

void AEnclavePlayerController::DebugRecordAuthoritativeTrace(const FVector& Start, const FVector& End, bool bHit, const FVector& ImpactPoint)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!HasAuthority() || !IsShotAlignmentDebugEnabled() || ActiveServerShotKey == INDEX_NONE)
	{
		return;
	}
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"ShotAlign SERVER key=%d origin=%s dir=%s hit=%d point=%s", ActiveServerShotKey,
		*Start.ToCompactString(), *(End - Start).GetSafeNormal().ToCompactString(), bHit ? 1 : 0,
		*ImpactPoint.ToCompactString());
	ClientEnclaveDebugShotAlignment(ActiveServerShotKey, Start, End, bHit, ImpactPoint);
#endif
}

void AEnclavePlayerController::ClientEnclaveDebugShotAlignment_Implementation(int32 PredictionKey, FVector ServerStart, FVector ServerEnd, bool bHit, FVector ImpactPoint)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsShotAlignmentDebugEnabled() || !IsLocalController() || !GetWorld())
	{
		return;
	}

	FDebugServerShot Ray;
	Ray.Start = ServerStart;
	Ray.End = ServerEnd;
	Ray.RecordedTime = GetWorld()->GetTimeSeconds();
	Ray.ImpactPoint = ImpactPoint;
	Ray.bHit = bHit;
	DebugPruneShotHistory();
	PendingServerShots.Add(PredictionKey, Ray);

	// Yellow is the exact server trace, returned to the owning client for
	// a SAME-WORLD overlay. Cyan was captured on the local Fire activation.
	DrawDebugLine(GetWorld(), ServerStart, bHit ? ImpactPoint : ServerEnd, FColor::Yellow, false, 4.0f, 0, 2.0f);
	if (bHit)
	{
		DrawDebugPoint(GetWorld(), ImpactPoint, 10.0f, FColor::Yellow, false, 4.0f);
	}
	DebugCompareShotIfReady(PredictionKey);
#endif
}

void AEnclavePlayerController::DebugCompareShotIfReady(int32 PredictionKey)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	const FDebugShotRay* Client = PendingLocalShots.Find(PredictionKey);
	const FDebugServerShot* Server = PendingServerShots.Find(PredictionKey);
	if (!Client || !Server)
	{
		return; // RPC and local predicted activation can arrive in either order.
	}

	const FVector ClientDirection = (Client->End - Client->Start).GetSafeNormal();
	const FVector ServerDirection = (Server->End - Server->Start).GetSafeNormal();
	const float OriginDeltaCm = FVector::Dist(Client->Start, Server->Start);
	const float DirectionDot = FMath::Clamp(FVector::DotProduct(ClientDirection, ServerDirection), -1.0f, 1.0f);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DirectionDot));

	// Compare the two rays at equal travel distance from their respective
	// origins. Use the server hit depth for hits and 10 m for misses.
	const float DepthCm = Server->bHit ? FVector::Dist(Server->Start, Server->ImpactPoint) : 1000.0f;
	const FVector ClientAtDepth = Client->Start + ClientDirection * DepthCm;
	const FVector ServerAtDepth = Server->Start + ServerDirection * DepthCm;
	const FVector Offset = ServerAtDepth - ClientAtDepth;
	const FRotationMatrix ClientBasis(ClientDirection.Rotation());
	const float HorizontalCm = FVector::DotProduct(Offset, ClientBasis.GetUnitAxis(EAxis::Y));
	const float VerticalCm = FVector::DotProduct(Offset, ClientBasis.GetUnitAxis(EAxis::Z));

	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"ShotAlign COMPARE key=%d hit=%d depth=%.0fcm originDelta=%.1fcm angleDelta=%.3fdeg serverRelativeToCamera Right=%.1fcm Up=%.1fcm (rays captured at separate network times)",
		PredictionKey, Server->bHit ? 1 : 0, DepthCm, OriginDeltaCm, AngleDegrees, HorizontalCm, VerticalCm);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::White,
			FString::Printf(TEXT("ADS shot %d: origin %.1f cm | angle %.3f deg | server @ %.1f m: right %+.1f cm, up %+.1f cm"),
				PredictionKey, OriginDeltaCm, AngleDegrees, DepthCm / 100.0f, HorizontalCm, VerticalCm));
	}

	PendingLocalShots.Remove(PredictionKey);
	PendingServerShots.Remove(PredictionKey);
#endif
}

void AEnclavePlayerController::DebugPruneShotHistory()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = PendingLocalShots.CreateIterator(); It; ++It)
	{
		if (Now - It.Value().RecordedTime > 30.0 || PendingLocalShots.Num() > 128) { It.RemoveCurrent(); }
	}
	for (auto It = PendingServerShots.CreateIterator(); It; ++It)
	{
		if (Now - It.Value().RecordedTime > 30.0 || PendingServerShots.Num() > 128) { It.RemoveCurrent(); }
	}
#endif
}

AEnclavePlayerController::AEnclavePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	DefaultMouseCursor = EMouseCursor::Default;

	CombatFeedbackComp = CreateDefaultSubobject<UEnclaveCombatFeedbackComponent>(TEXT("CombatFeedbackComp"));
}

void AEnclavePlayerController::EnclaveDebugRespawn()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (HasAuthority())
	{
		PerformDebugRespawn();
		return;
	}

	ServerEnclaveDebugRespawn();
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugRespawn_Implementation()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	PerformDebugRespawn();
#endif
}

void AEnclavePlayerController::PerformDebugRespawn()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!HasAuthority())
	{
		return;
	}

	APawn* PreviousPawn = GetPawn();
	AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;

	if (!GameMode)
	{
		ENCLAVE_LOG(LogEnclaveCharacter, Warning, "EnclaveDebugRespawn failed: no authoritative GameMode.");
		return;
	}

	ENCLAVE_LOG(LogEnclaveCharacter, Log, "EnclaveDebugRespawn: recreating pawn %s while preserving PlayerState %s.", *GetNameSafe(PreviousPawn), *GetNameSafe(PlayerState.Get()));

	if (PreviousPawn)
	{
		// Exercise the same explicit lifecycle boundary the real Death path will use.
		// Do this before UnPossess so the old Pawn can still resolve the persistent
		// PlayerState-owned ASC while revoking its item-granted specs/effects.
		if (UEnclaveEquipmentComponent* Equipment = PreviousPawn->FindComponentByClass<UEnclaveEquipmentComponent>())
		{
			Equipment->ClearCurrentWieldedItem();
		}

		if (UEnclaveInventoryComponent* Inventory = PreviousPawn->FindComponentByClass<UEnclaveInventoryComponent>())
		{
			Inventory->DestroyAllItems();
		}

		UnPossess();
		PreviousPawn->Destroy();
	}

	GameMode->RestartPlayer(this);

	ENCLAVE_LOG(LogEnclaveCharacter, Log, "EnclaveDebugRespawn complete. New pawn: %s", *GetNameSafe(GetPawn()));
#endif
}

void AEnclavePlayerController::EnclaveDebugRemoveWieldedItem()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (HasAuthority())
	{
		PerformDebugRemoveWieldedItem();
		return;
	}

	ServerEnclaveDebugRemoveWieldedItem();
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugRemoveWieldedItem_Implementation()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	PerformDebugRemoveWieldedItem();
#endif
}

void AEnclavePlayerController::PerformDebugRemoveWieldedItem()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!HasAuthority())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn))
	{
		ENCLAVE_LOG(LogEnclaveInventory, Warning, "EnclaveDebugRemoveWieldedItem: no valid possessed Pawn.");
		return;
	}

	UEnclaveEquipmentComponent* Equipment = ControlledPawn->FindComponentByClass<UEnclaveEquipmentComponent>();
	UEnclaveInventoryComponent* Inventory = ControlledPawn->FindComponentByClass<UEnclaveInventoryComponent>();
	if (!IsValid(Equipment) || !IsValid(Inventory))
	{
		ENCLAVE_LOG(LogEnclaveInventory, Warning, "EnclaveDebugRemoveWieldedItem: %s requires Equipment and Inventory components.", *GetNameSafe(ControlledPawn));
		return;
	}

	AEnclaveItem* ItemToRemove = Equipment->GetCurrentWieldedItem();
	if (!IsValid(ItemToRemove))
	{
		ENCLAVE_LOG(LogEnclaveInventory, Warning, "EnclaveDebugRemoveWieldedItem: %s has no wielded item.", *GetNameSafe(ControlledPawn));
		return;
	}

	// Only regular inventory membership authorizes removal. This explicitly
	// excludes Equipment's persistent default item without a special item flag.
	if (!Inventory->ContainsItem(ItemToRemove))
	{
		ENCLAVE_LOG(LogEnclaveInventory, Log, "EnclaveDebugRemoveWieldedItem: %s is not in ordinary Inventory; leaving it intact.", *GetNameSafe(ItemToRemove));
		return;
	}

	// RemoveItem broadcasts the source change synchronously. Equipment can
	// cancel/revoke the outgoing item's abilities and wield the default while
	// the old item actor and its GAS SourceObject are still valid.
	if (!Inventory->RemoveItem(ItemToRemove))
	{
		ENCLAVE_LOG(LogEnclaveInventory, Warning, "EnclaveDebugRemoveWieldedItem: failed to remove %s.", *GetNameSafe(ItemToRemove));
		return;
	}

	ENCLAVE_LOG(LogEnclaveInventory, Log, "EnclaveDebugRemoveWieldedItem: removed %s from %s; now wielded: %s.",
		*GetNameSafe(ItemToRemove), *GetNameSafe(ControlledPawn), *GetNameSafe(Equipment->GetCurrentWieldedItem()));

	// This is a destructive development-only test, not a world-drop mechanic.
	ItemToRemove->Destroy();
#endif
}

void AEnclavePlayerController::EnclaveDebugClearSlot(const FString& SlotTagName)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	const FString CleanTagName = SlotTagName.TrimStartAndEnd();
	const FGameplayTag SlotTag = FGameplayTag::RequestGameplayTag(FName(*CleanTagName), /*ErrorIfNotFound=*/false);
	if (!SlotTag.IsValid())
	{
		const FString Message = FString::Printf(TEXT("EnclaveDebugClearSlot: unknown Gameplay Tag '%s'."), *CleanTagName);
		ENCLAVE_LOG_MSG(LogEnclaveEquipment, Warning, Message);
		ENCLAVE_SCREEN(Message, FColor::Red, 5.0f);
		return;
	}

	if (HasAuthority())
	{
		PerformDebugClearSlot(SlotTag);
		return;
	}

	ServerEnclaveDebugClearSlot(SlotTag);
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugClearSlot_Implementation(FGameplayTag SlotTag)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	PerformDebugClearSlot(SlotTag);
#endif
}

void AEnclavePlayerController::PerformDebugClearSlot(FGameplayTag SlotTag)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!HasAuthority())
	{
		return;
	}

	bool bSucceeded = false;
	FString ResultMessage;
	if (!SlotTag.IsValid())
	{
		ResultMessage = TEXT("EnclaveDebugClearSlot: invalid slot tag.");
	}
	else if (SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default) ||
		SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
	{
		ResultMessage = FString::Printf(TEXT("EnclaveDebugClearSlot: %s is reserved; assignment not changed."), *SlotTag.ToString());
	}
	else if (APawn* ControlledPawn = GetPawn(); !IsValid(ControlledPawn))
	{
		ResultMessage = TEXT("EnclaveDebugClearSlot: no valid possessed pawn.");
	}
	else if (UEnclaveEquipmentComponent* Equipment = ControlledPawn->FindComponentByClass<UEnclaveEquipmentComponent>(); !IsValid(Equipment))
	{
		ResultMessage = FString::Printf(TEXT("EnclaveDebugClearSlot: %s has no Equipment component."), *GetNameSafe(ControlledPawn));
	}
	else
	{
		const FString PreviousItemName = GetNameSafe(Equipment->GetItemInSlot(SlotTag));
		bSucceeded = Equipment->ClearSlot(SlotTag);
		ResultMessage = bSucceeded
			? FString::Printf(TEXT("EnclaveDebugClearSlot: cleared %s (%s); item remains in its source."), *SlotTag.ToString(), *PreviousItemName)
			: FString::Printf(TEXT("EnclaveDebugClearSlot: no assignment to clear in %s."), *SlotTag.ToString());
	}

	if (bSucceeded)
	{
		ENCLAVE_LOG_MSG(LogEnclaveEquipment, Log, ResultMessage);
	}
	else
	{
		ENCLAVE_LOG_MSG(LogEnclaveEquipment, Warning, ResultMessage);
	}

	// A remote client cannot see the server Output Log. Report the result to
	// the owning controller so the same command is usable in either PIE window.
	ClientEnclaveDebugClearSlotResult(bSucceeded, ResultMessage);
#endif
}

void AEnclavePlayerController::ClientEnclaveDebugClearSlotResult_Implementation(bool bSucceeded, const FString& ResultMessage)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	ENCLAVE_SCREEN(ResultMessage, bSucceeded ? FColor::Green : FColor::Red, 5.0f);
#endif
}

void AEnclavePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	RefreshPossessedPawnInputState();
}

void AEnclavePlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	// OnPossess is authoritative. Networked respawn reaches the owning client
	// through possession acknowledgement, so refresh the semantic input routing
	// here as well for the newly replicated Pawn.
	RefreshPossessedPawnInputState();
}

void AEnclavePlayerController::OnUnPossess()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	StopDebugSSRAutoFire(TEXT("unpossessed"));
	StopDebugSSRStrafe(TEXT("unpossessed"));
	StopDebugSSRCrouch(TEXT("unpossessed"));
	EnclaveDebugSSRProtect(0);
#endif
	if (ActiveAvatarInputConfig)
	{
		DeactivateInputConfig(ActiveAvatarInputConfig);
	}

	CachedInputReceiver = nullptr;
	CachedInputConfigProvider = nullptr;
	ActiveAvatarInputConfig = nullptr;
	ActiveServerShotKey = INDEX_NONE;
	PendingLocalShots.Reset();
	PendingServerShots.Reset();

	Super::OnUnPossess();
}

void AEnclavePlayerController::BeginPlay()
{
	Super::BeginPlay();

	RefreshPossessedPawnInputState();
}

// D1.1C stationary/crouch test health protection is explicit and local to
// an authoritative listen-server pawn. It does not replicate or bypass GAS.
void AEnclavePlayerController::EnclaveDebugSSRProtect(int32 bEnable)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bEnable == 0)
	{
		if (AEnclaveCharacter* Protected = DebugSSRManualProtectedPawn.Get())
		{
			const bool bStrafeStillOwnsProtection = DebugSSRStrafeProtectedPawn.Get() == Protected;
			if (!bStrafeStillOwnsProtection)
			{
				Protected->DebugSetSSRStrafeDamageSuppressed(false);
			}
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRGameplay PROTECT pawn=%s manual=0 strafeActive=%d (DEBUG ONLY)",
				*GetNameSafe(Protected), bStrafeStillOwnsProtection ? 1 : 0);
		}
		DebugSSRManualProtectedPawn.Reset();
		return;
	}
	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	if (!IsLocalController() || !IsValid(Char) || !Char->HasAuthority() || !IsSSRGameplayGateEnabled())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRGameplay PROTECT rejected=requires_authoritative_local_target_and_gameplay_gate (DEBUG ONLY)");
		return;
	}
	if (DebugSSRManualProtectedPawn.IsValid() && DebugSSRManualProtectedPawn.Get() != Char)
	{
		EnclaveDebugSSRProtect(0);
	}
	Char->DebugSetSSRStrafeDamageSuppressed(true);
	DebugSSRManualProtectedPawn = Char;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRGameplay PROTECT pawn=%s manual=1 (server-local, DEBUG ONLY)", *GetNameSafe(Char));
#else
	(void)bEnable;
#endif
}

// D1.1C.1: isolate true crouch geometry without keyboard-focus ambiguity.
// This runs only on the listen-server target; normal ACharacter movement
// replicates the resulting crouch state/capsule, not an SSR-specific payload.
void AEnclavePlayerController::EnclaveDebugSSRCrouch(int32 bEnable)
{
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
			if (Original->HasAuthority())
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

	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	UCharacterMovementComponent* Movement = IsValid(Char) ? Char->GetCharacterMovement() : nullptr;
	if (!IsLocalController() || !IsValid(Char) || !Char->HasAuthority() ||
		!Char->IsLocallyControlled() || !IsValid(Movement) || !IsSSRGameplayGateEnabled())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerCrouch rejected=requires_authoritative_local_character_and_gameplay_gate (DEBUG ONLY)");
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

#if ENCLAVE_INCLUDE_DEBUG_CODE
void AEnclavePlayerController::StopDebugSSRCrouch(const TCHAR* Reason)
{
	if (AEnclaveCharacter* Original = DebugSSRCrouchPawn.Get())
	{
		if (Original->HasAuthority())
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

void AEnclavePlayerController::TickDebugSSRCrouch(float DeltaSeconds)
{
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
	if (!Char->HasAuthority() || Char != GetPawn())
	{
		StopDebugSSRCrouch(TEXT("authority_or_possession_changed"));
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
				"SSRPlayerCrouch VERIFIED pawn=%s crouched=%d halfHeightCm=%.2f radiusCm=%.2f (authoritative capsule; DEBUG ONLY)",
				*GetNameSafe(Char), bActualCrouched ? 1 : 0,
				Capsule->GetUnscaledCapsuleHalfHeight(), Capsule->GetUnscaledCapsuleRadius());
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
#endif

// A local PlayerController feeds the exact same AddMovementInput path that the
// regular locomotion handler uses. Remote owners therefore still send UE's
// predicted CharacterMovement moves through their normal ServerMove pathway.
void AEnclavePlayerController::EnclaveDebugSSRStrafe(int32 bEnable)
{
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
		if (bDebugSSRCrouchPending && bDebugSSRCrouchDesired && CrouchPawn == GetPawn() &&
			IsValid(CrouchMovement))
		{
			bDebugSSRStrafeQueuedForCrouch = true;
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPlayerStrafe LOCAL QUEUED reason=awaiting_authoritative_crouch_verification (DEBUG ONLY)");
			return;
		}
		if (bDebugSSRCrouchPending || !bDebugSSRCrouchDesired || CrouchPawn != GetPawn() ||
			!IsValid(CrouchMovement) || !CrouchMovement->IsCrouching())
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning,
				"SSRPlayerStrafe LOCAL start_rejected=crouch_state_not_verified (DEBUG ONLY)");
			return;
		}
	}

	// Restarting the rig must always release the prior body's protection.
	StopDebugSSRStrafe(TEXT("restart"));
	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	UEnclaveCharacterMovementComponent* Movement = IsValid(Char) ? Cast<UEnclaveCharacterMovementComponent>(Char->GetCharacterMovement()) : nullptr;
	if (!IsLocalController() || !IsValid(Char) || !Char->IsLocallyControlled() ||
		!IsValid(Movement) || !Movement->IsLocomotionEnabled())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRPlayerStrafe LOCAL start_rejected controller=%s reason=requires_locally_controlled_movable_enclave_character",
			*GetNameSafe(this));
		return;
	}

	const FRotator Yaw(0.0f, GetControlRotation().Yaw, 0.0f);
	DebugSSRStrafeRight = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	DebugSSRStrafeOrigin = Char->GetActorLocation();
	DebugSSRStrafeDirectionSign = 1;
	DebugSSRStrafeElapsedSeconds = 0.0f;
	DebugSSRStrafePauseSeconds = 0.0f;
	DebugSSRStrafeNextStatusSeconds = 2.0f;
	bDebugSSRStrafeEnabled = !DebugSSRStrafeRight.IsNearlyZero();
	if (bDebugSSRStrafeEnabled && Char->HasAuthority() && IsSSRGameplayGateEnabled())
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
		FMath::Clamp(CVarDebugSSRStrafeDuration.GetValueOnGameThread(), 10.0f, 180.0f),
		Movement->IsCrouching() ? 1 : 0);
#else
	(void)bEnable;
#endif
}

// D1.1B is intentionally an ordinary LOCAL input producer: the same Primary
// and Reload tags used by the player's hands flow through the avatar router,
// Equipment grants, and GA_Fire prediction/TargetData. Never invoke the server
// historical query directly or bypass ammo/cooldown/readiness rules.
void AEnclavePlayerController::EnclaveDebugSSRAutoFire(int32 bEnable)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bEnable == 0)
	{
		StopDebugSSRAutoFire(TEXT("manual"));
		return;
	}

	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	const UEnclaveEquipmentComponent* Equipment = IsValid(Char) ? Char->FindComponentByClass<UEnclaveEquipmentComponent>() : nullptr;
	const AEnclaveFirearmWeapon* Firearm = IsValid(Equipment) ? Cast<AEnclaveFirearmWeapon>(Equipment->GetCurrentWieldedItem()) : nullptr;
	if (!IsLocalController() || !GetWorld() || GetNetMode() != NM_Client || !IsValid(Char) ||
		!Char->IsLocallyControlled() || !IsValid(Firearm) || !IsSSRGameplayGateEnabled() ||
		CachedInputReceiver == nullptr)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRAuto START_REJECTED reason=requires_remote_local_client_firearm_and_gameplay_gate (DEBUG ONLY)");
		return;
	}

	StopDebugSSRAutoFire(TEXT("restart"));
	DebugSSRAutoOriginalPawn = Char;
	DebugSSRAutoRandom.Initialize(CVarDebugSSRAutoSeed.GetValueOnGameThread());
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
		CVarDebugSSRAutoSeed.GetValueOnGameThread());
#else
	(void)bEnable;
#endif
}

#if ENCLAVE_INCLUDE_DEBUG_CODE
bool AEnclavePlayerController::ChooseDebugSSRAutoTarget()
{
	DebugSSRAutoTarget.Reset();
	if (!GetWorld() || !IsValid(GetPawn()))
	{
		return false;
	}
	const FVector Eye = GetPawn()->GetPawnViewLocation();
	double BestDistanceSquared = FMath::Square(10000.0);
	for (TActorIterator<AEnclaveCharacter> It(GetWorld()); It; ++It)
	{
		AEnclaveCharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == GetPawn() || Candidate->GetLocalRole() != ROLE_SimulatedProxy ||
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

void AEnclavePlayerController::StopDebugSSRAutoFire(const TCHAR* Reason)
{
	if (bDebugSSRAutoEnabled)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRAuto STOP reason=%s attempts=%d reloads=%d elapsedSeconds=%.1f seed=%d"
			" (attempts are not confirmed GAS activations; score SSRGameplay SERVER by key; DEBUG ONLY)",
			Reason, DebugSSRAutoAttemptCount, DebugSSRAutoReloadCount, DebugSSRAutoElapsedSeconds,
			CVarDebugSSRAutoSeed.GetValueOnGameThread());
	}
	bDebugSSRAutoEnabled = false;
	bDebugSSRAutoHasObservedMovingTarget = false;
	DebugSSRAutoTarget.Reset();
	DebugSSRAutoOriginalPawn.Reset();
}

void AEnclavePlayerController::TickDebugSSRAutoFire(float DeltaSeconds)
{
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
	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	const UEnclaveEquipmentComponent* Equipment = IsValid(Char) ? Char->FindComponentByClass<UEnclaveEquipmentComponent>() : nullptr;
	AEnclaveFirearmWeapon* Firearm = IsValid(Equipment) ? Cast<AEnclaveFirearmWeapon>(Equipment->GetCurrentWieldedItem()) : nullptr;
	if (!IsLocalController() || GetNetMode() != NM_Client || !IsSSRGameplayGateEnabled() ||
		!IsValid(Char) || Char != DebugSSRAutoOriginalPawn.Get() || !Char->IsLocallyControlled() ||
		!IsValid(Firearm) || CachedInputReceiver == nullptr)
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
		HandleInput(FInputActionValue(true), ETriggerEvent::Started,
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
	const float Side = DebugSSRAutoRandom.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
	const float OffsetCm = bAimForClearHit
		? DebugSSRAutoRandom.FRandRange(0.0f, FMath::Max(0.0f, RadiusCm - 20.0f)) * Side
		: (RadiusCm + DebugSSRAutoRandom.FRandRange(18.0f, 30.0f)) * Side;
	const float VerticalOffsetCm = bAimForClearHit
		? DebugSSRAutoRandom.FRandRange(-0.2f, 0.2f) * FMath::Max(0.0f, HalfHeightCm - RadiusCm)
		: 0.0f;
	const FVector AimPoint = ReferenceCenter + Right * OffsetCm + FVector::UpVector * VerticalOffsetCm;
	const FRotator AimRotation = (AimPoint - Eye).Rotation();

	// Set the ACTUAL control rotation used by the pawn-eye gameplay shot ray,
	// then dispatch through PlayerController::HandleInput -> Avatar InputRouter
	// -> Equipment -> GAS. The server still validates every real shot normally.
	SetControlRotation(AimRotation);
	++DebugSSRAutoAttemptCount;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRAuto ATTEMPT number=%d intended=%s target=%s offsetCm=%.1f verticalCm=%.1f"
		" ammoBefore=%d (actual class from SSRGameplay CLIENT/SERVER; DEBUG ONLY)",
		DebugSSRAutoAttemptCount, bAimForClearHit ? TEXT("clear_hit") : TEXT("clear_miss"),
		*GetNameSafe(Target), OffsetCm, VerticalOffsetCm, Firearm->GetCurrentMagazineAmmo());
	HandleInput(FInputActionValue(true), ETriggerEvent::Started,
		EnclaveGameplayTags::Input_Item_Action_Primary, false);
}

void AEnclavePlayerController::StopDebugSSRStrafe(const TCHAR* Reason)
{
	if (bDebugSSRStrafeEnabled)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPlayerStrafe LOCAL STOP pawn=%s reason=%s elapsedSeconds=%.1f (debug only)",
			*GetNameSafe(GetPawn()), Reason, DebugSSRStrafeElapsedSeconds);
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
#endif

#if ENCLAVE_INCLUDE_DEBUG_CODE
void AEnclavePlayerController::DebugTickSSRClockSync()
{
    // The client GameState derives an estimated server time from its local
    // world clock and a synchronized offset. Watch that offset every game tick.
    // This is an observer, NOT an OnRep hook: offset changes may have other
    // causes. In particular, it must never change shot validation or rewind.
    if (!IsSSRClockDebugEnabled() || !IsLocalController() || !GetWorld() ||
        GetWorld()->GetNetMode() != NM_Client)
    {
        bDebugSSRClockSyncHasSample = false;
        return;
    }

    AGameStateBase* GameState = GetWorld()->GetGameState();
    if (!IsValid(GameState))
    {
        bDebugSSRClockSyncHasSample = false;
        return;
    }

    const double ProcessSeconds = FPlatformTime::Seconds();
    const double WorldSeconds = GetWorld()->GetTimeSeconds();
    const double EstimatedServerSeconds = GameState->GetServerWorldTimeSeconds();
    if (!FMath::IsFinite(ProcessSeconds) || !FMath::IsFinite(WorldSeconds) ||
        !FMath::IsFinite(EstimatedServerSeconds))
    {
        bDebugSSRClockSyncHasSample = false;
        return;
    }

    const double EstimatedOffsetSeconds = EstimatedServerSeconds - WorldSeconds;
    if (!bDebugSSRClockSyncHasSample || ProcessSeconds < DebugSSRClockSyncLastProcessSeconds ||
        ProcessSeconds - DebugSSRClockSyncLastProcessSeconds > 10.0)
    {
        ENCLAVE_LOG(LogEnclaveCombat, Log,
            "SSRClockSync CLIENT event=init offsetMs=%+.2f world=%.6f estimate=%.6f process=%.6f frame=%llu (OBSERVED GameState offset, not direct OnRep; DEBUG ONLY)",
            EstimatedOffsetSeconds * 1000.0, WorldSeconds, EstimatedServerSeconds,
            ProcessSeconds, static_cast<unsigned long long>(GFrameCounter));
        bDebugSSRClockSyncHasSample = true;
        DebugSSRClockSyncLastReportProcessSeconds = ProcessSeconds;
        DebugSSRClockSyncLastReportWorldSeconds = WorldSeconds;
        DebugSSRClockSyncLastReportEstimatedSeconds = EstimatedServerSeconds;
        DebugSSRClockSyncLastReportOffsetSeconds = EstimatedOffsetSeconds;
    }
    else
    {
        const double FrameProcessMs = (ProcessSeconds - DebugSSRClockSyncLastProcessSeconds) * 1000.0;
        const double FrameWorldMs = (WorldSeconds - DebugSSRClockSyncLastWorldSeconds) * 1000.0;
        const double FrameEstimateMs = (EstimatedServerSeconds - DebugSSRClockSyncLastEstimatedSeconds) * 1000.0;
        const double OffsetStepMs = (EstimatedOffsetSeconds - DebugSSRClockSyncLastOffsetSeconds) * 1000.0;
        const double SinceReportSeconds = ProcessSeconds - DebugSSRClockSyncLastReportProcessSeconds;
        const bool bOffsetStep = FMath::Abs(OffsetStepMs) >= 1.0;
        const bool bFramePhaseGap = FrameProcessMs >= 35.0 && FMath::Abs(FrameWorldMs - FrameProcessMs) >= 10.0;
        const bool bPeriodic = SinceReportSeconds >= 1.0;
        if (bOffsetStep || bPeriodic || (bFramePhaseGap && SinceReportSeconds >= 0.25))
        {
            const TCHAR* Event = bOffsetStep ? TEXT("offset_step") :
                bFramePhaseGap ? TEXT("frame_phase_gap") : TEXT("periodic");
            const double ReportProcessMs = SinceReportSeconds * 1000.0;
            const double ReportWorldMs = (WorldSeconds - DebugSSRClockSyncLastReportWorldSeconds) * 1000.0;
            const double ReportEstimateMs = (EstimatedServerSeconds - DebugSSRClockSyncLastReportEstimatedSeconds) * 1000.0;
            ENCLAVE_LOG(LogEnclaveCombat, Log,
                "SSRClockSync CLIENT event=%s offsetMs=%+.2f offsetStepMs=%+.2f offsetSinceReportMs=%+.2f frameProcessMs=%.2f frameWorldMs=%.2f frameEstimateMs=%.2f reportProcessMs=%.1f reportWorldMs=%.1f reportEstimateMs=%.1f frame=%llu (OBSERVED offset step, NOT proof of GameState OnRep; DEBUG ONLY)",
                Event, EstimatedOffsetSeconds * 1000.0, OffsetStepMs,
                (EstimatedOffsetSeconds - DebugSSRClockSyncLastReportOffsetSeconds) * 1000.0,
                FrameProcessMs, FrameWorldMs, FrameEstimateMs,
                ReportProcessMs, ReportWorldMs, ReportEstimateMs,
                static_cast<unsigned long long>(GFrameCounter));
            DebugSSRClockSyncLastReportProcessSeconds = ProcessSeconds;
            DebugSSRClockSyncLastReportWorldSeconds = WorldSeconds;
            DebugSSRClockSyncLastReportEstimatedSeconds = EstimatedServerSeconds;
            DebugSSRClockSyncLastReportOffsetSeconds = EstimatedOffsetSeconds;
        }
    }

    DebugSSRClockSyncLastProcessSeconds = ProcessSeconds;
    DebugSSRClockSyncLastWorldSeconds = WorldSeconds;
    DebugSSRClockSyncLastEstimatedSeconds = EstimatedServerSeconds;
    DebugSSRClockSyncLastOffsetSeconds = EstimatedOffsetSeconds;
}
#endif

void AEnclavePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENCLAVE_INCLUDE_DEBUG_CODE
	DebugTickSSRClockSync();
	TickDebugSSRCrouch(DeltaSeconds);
	TickDebugSSRAutoFire(DeltaSeconds);

	if (!bDebugSSRStrafeEnabled)
	{
		return;
	}

	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	UEnclaveCharacterMovementComponent* Movement = IsValid(Char) ? Cast<UEnclaveCharacterMovementComponent>(Char->GetCharacterMovement()) : nullptr;
	if (!IsLocalController() || !IsValid(Char) || !Char->IsLocallyControlled() ||
		!IsValid(Movement) || !Movement->IsLocomotionEnabled())
	{
		StopDebugSSRStrafe(TEXT("pawn_or_movement_unavailable"));
		return;
	}

	DebugSSRStrafeElapsedSeconds += DeltaSeconds;
	if (DebugSSRStrafeElapsedSeconds >= FMath::Clamp(CVarDebugSSRStrafeDuration.GetValueOnGameThread(), 10.0f, 180.0f))
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

void AEnclavePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (ActiveAvatarInputConfig)
	{
		BindInputConfigActions(*EnhancedInput, ActiveAvatarInputConfig);
	}
}

bool AEnclavePlayerController::CanProcessInput(const FEnclaveInputEvent& InputEvent) const
{
	return InputEvent.IsValid() && CachedInputReceiver != nullptr;
}

void AEnclavePlayerController::HandleInput(const FInputActionValue& Value, ETriggerEvent TriggerEvent, FGameplayTag InputTag, bool bRoutesReleaseEvents)
{
	const FEnclaveInputEvent InputEvent(InputTag, Value, TriggerEvent, bRoutesReleaseEvents);

	if (!CanProcessInput(InputEvent))
	{
		return;
	}

	CachedInputReceiver->ProcessInput(InputEvent);
}

void AEnclavePlayerController::RefreshPossessedPawnInputState()
{
	APawn* ControlledPawn = GetPawn();

	if (ActiveAvatarInputConfig)
	{
		DeactivateInputConfig(ActiveAvatarInputConfig);
		ActiveAvatarInputConfig = nullptr;
	}

	CachedInputReceiver = nullptr;
	CachedInputConfigProvider = nullptr;

	if (!ControlledPawn)
	{
		return;
	}

	CachedInputReceiver = Cast<IEnclaveInputReceiverInterface>(ControlledPawn);
	CachedInputConfigProvider = Cast<IEnclaveInputConfigProviderInterface>(ControlledPawn);

	if (!CachedInputConfigProvider)
	{
		return;
	}

	ActiveAvatarInputConfig = CachedInputConfigProvider->GetInputConfig();

	if (ActiveAvatarInputConfig)
	{
		ActivateInputConfig(ActiveAvatarInputConfig);
	}
}

void AEnclavePlayerController::ActivateInputConfig(const UEnclaveInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	AddInputConfigMappingContexts(InputConfig);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput)
	{
		BindInputConfigActions(*EnhancedInput, InputConfig);
	}
}

void AEnclavePlayerController::DeactivateInputConfig(const UEnclaveInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	RemoveInputConfigMappingContexts(InputConfig);
	ClearInputBindings();
}

void AEnclavePlayerController::AddInputConfigMappingContexts(const UEnclaveInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	for (const FEnclaveInputMappingContextEntry& Entry : InputConfig->MappingContexts)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		InputSubsystem->AddMappingContext(Entry.MappingContext, Entry.Priority);
	}
}

void AEnclavePlayerController::RemoveInputConfigMappingContexts(const UEnclaveInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	for (const FEnclaveInputMappingContextEntry& Entry : InputConfig->MappingContexts)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		InputSubsystem->RemoveMappingContext(Entry.MappingContext);
	}
}

void AEnclavePlayerController::BindInputConfigActions(UEnhancedInputComponent& EnhancedInput, const UEnclaveInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	ClearInputBindings();

	TSet<FString> BoundPairs;

	for (const FEnclaveInputActionBinding& Binding : InputConfig->ActionBindings)
	{
		if (!Binding.InputAction || !Binding.InputTag.IsValid())
		{
			continue;
		}

		const bool bRoutesReleaseEvents = Binding.TriggerEvents.Contains(ETriggerEvent::Completed) ||
			Binding.TriggerEvents.Contains(ETriggerEvent::Canceled);

		for (const ETriggerEvent TriggerEvent : Binding.TriggerEvents)
		{
			if (TriggerEvent == ETriggerEvent::None)
			{
				continue;
			}

			const FString PairKey = FString::Printf(TEXT("%s_%d"), *Binding.InputAction->GetPathName(), static_cast<int32>(TriggerEvent));

			if (BoundPairs.Contains(PairKey))
			{
				continue;
			}

			BoundPairs.Add(PairKey);

			const FEnhancedInputActionEventBinding& EnhancedBinding = EnhancedInput.BindAction(Binding.InputAction, TriggerEvent, this, &AEnclavePlayerController::HandleInput, TriggerEvent, Binding.InputTag, bRoutesReleaseEvents);

			ActiveInputBindingHandles.Add(EnhancedBinding.GetHandle());
		}
	}
}

void AEnclavePlayerController::ClearInputBindings()
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		ActiveInputBindingHandles.Reset();
		return;
	}

	for (const uint32 BindingHandle : ActiveInputBindingHandles)
	{
		EnhancedInput->RemoveBindingByHandle(BindingHandle);
	}

	ActiveInputBindingHandles.Reset();
}