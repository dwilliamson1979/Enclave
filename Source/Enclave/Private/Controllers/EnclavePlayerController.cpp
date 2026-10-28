#include "Controllers/EnclavePlayerController.h"
#include "Debug/EnclaveSSRTestSubsystem.h"
#include "Debug/EnclaveSSRTestComponent.h"
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
		TEXT("Enclave.Debug.SSRCrouch"), TEXT("Request/release crouch on the locally controlled SSR test target (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::Crouch), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRStrafe(
		TEXT("Enclave.Debug.SSRStrafe"), TEXT("Start/stop the local SSR lateral strafe rig (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::Strafe), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRProtect(
		TEXT("Enclave.Debug.SSRProtect"), TEXT("Enable/disable debug-only server-local target damage protection (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::Protect), ECVF_Cheat);

	void ExecuteSSRTestCoordinator(const TArray<FString>& Args, UWorld* World, int32 Action)
	{
		if (!IsValid(World) || !World->IsGameWorld() || World->GetNetMode() != NM_ListenServer)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest rejected=listen_server_game_world_required (DEBUG ONLY)");
			return;
		}
		AEnclavePlayerController* Host = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(It->Get());
			if (IsValid(PC) && PC->HasAuthority() && PC->IsLocalController())
			{
				Host = PC;
				break;
			}
		}
		if (!Host)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest rejected=no_host_controller (DEBUG ONLY)");
			return;
		}
		if (Action == 0)
		{
			if (Args.Num() != 1 || !Args[0].IsNumeric())
			{
				ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START usage=Enclave.Debug.SSRTest.Start_2_or_3_or_4_or_5 (DEBUG ONLY)");
				return;
			}
			Host->EnclaveDebugSSRTestStart(FCString::Atoi(*Args[0]));
		}
		else if (Args.IsEmpty())
		{
			if (Action == 1) Host->EnclaveDebugSSRTestStop();
			else Host->EnclaveDebugSSRTestStatus();
		}
		else
		{
			ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest rejected=unexpected_args (DEBUG ONLY)");
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CCmdSSRTestStart(
		TEXT("Enclave.Debug.SSRTest.Start"), TEXT("Listen server: coordinate SSR test scenario 2/3/4/5 across two remote clients."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteSSRTestCoordinator, 0), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs CCmdSSRTestStop(
		TEXT("Enclave.Debug.SSRTest.Stop"), TEXT("Listen server: stop and clean up coordinated SSR test."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteSSRTestCoordinator, 1), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs CCmdSSRTestStatus(
		TEXT("Enclave.Debug.SSRTest.Status"), TEXT("Listen server: report coordinated SSR test state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteSSRTestCoordinator, 2), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CCmdDebugSSRAutoFire(
		TEXT("Enclave.Debug.SSRAutoFire"), TEXT("Start/stop remote-client seeded GAS-fire sampling (0/1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteEnclaveSSRConsoleAction, EEnclaveSSRConsoleAction::AutoFire), ECVF_Cheat);
}
#endif

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
		// Movement and the server-authored frame may initialize in different updates.
		// Never interpret an uninitialized engine location as a geometry mismatch.
		const bool bMovementComparisonReady = Update.ServerStamp > 0.0f &&
			FMath::IsFinite(Update.ServerStamp) && FMath::IsFinite(Update.ReplicatedLocation.X) &&
			FMath::IsFinite(Update.ReplicatedLocation.Y) && FMath::IsFinite(Update.ReplicatedLocation.Z);
		if (bMovementComparisonReady)
		{
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPairedObservation CLIENT target=%s authStamp=%.6f engineStamp=%.6f receiveAgeMs=%.2f locationDeltaCm=%.2f pairedCount=%u (stamp used by shadow claim; geometry comparison initialized)",
				*GetNameSafe(Candidate), Authoritative.ServerStamp, Update.ServerStamp, ObservationAgeMs,
				FVector::Dist(Authoritative.ReplicatedLocation, Update.ReplicatedLocation), Authoritative.ReceiveCount);
		}
		else
		{
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPairedObservation CLIENT target=%s authStamp=%.6f engineStamp=%.6f receiveAgeMs=%.2f locationDeltaCm=NA pairedCount=%u geometryStatus=movement_uninitialized (stamp used by shadow claim)",
				*GetNameSafe(Candidate), Authoritative.ServerStamp, Update.ServerStamp, ObservationAgeMs, Authoritative.ReceiveCount);
		}
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
#if ENCLAVE_INCLUDE_DEBUG_CODE
	SSRTestComponent = CreateDefaultSubobject<UEnclaveSSRTestComponent>(TEXT("SSRTestComponent"));
#endif
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
	if (HasAuthority())
        if (UEnclaveSSRTestSubsystem* Session = GetWorld() ? GetWorld()->GetSubsystem<UEnclaveSSRTestSubsystem>() : nullptr) Session->ParticipantUnpossessed(this);
	if (SSRTestComponent) SSRTestComponent->ParticipantUnpossessed();
	
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

void AEnclavePlayerController::EnclaveDebugSSRTestStart(int32 Scenario)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    if (UEnclaveSSRTestSubsystem* Session = GetWorld() ? GetWorld()->GetSubsystem<UEnclaveSSRTestSubsystem>() : nullptr) Session->Start(Scenario);
#endif
}
void AEnclavePlayerController::EnclaveDebugSSRTestStop()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    if (UEnclaveSSRTestSubsystem* Session = GetWorld() ? GetWorld()->GetSubsystem<UEnclaveSSRTestSubsystem>() : nullptr) Session->Stop(TEXT("manual"));
#endif
}
void AEnclavePlayerController::EnclaveDebugSSRTestStatus()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    if (UEnclaveSSRTestSubsystem* Session = GetWorld() ? GetWorld()->GetSubsystem<UEnclaveSSRTestSubsystem>() : nullptr) Session->Status();
#endif
}









void AEnclavePlayerController::BeginPlay()
{
	Super::BeginPlay();

	RefreshPossessedPawnInputState();
}

// D1.1C test-only damage protection. A remote client's controller forwards
// the request to its own authoritative PlayerController, never to an arbitrary pawn.
void AEnclavePlayerController::EnclaveDebugSSRProtect(int32 bEnable)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (bEnable != 0 && bEnable != 1)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRGameplay PROTECT rejected=expected_0_or_1 (DEBUG ONLY)");
		return;
	}
	if (!HasAuthority())
	{
		if (!IsLocalController())
		{
			return;
		}
		ServerEnclaveDebugSSRProtect(bEnable != 0);
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRGameplay PROTECT requested=%d remote=1 (DEBUG ONLY)", bEnable);
		return;
	}
	if (bEnable == 0)
	{
		if (AEnclaveCharacter* Protected = SSRTestComponent->DebugSSRManualProtectedPawn.Get())
		{
			const bool bStrafeStillOwnsProtection = SSRTestComponent->DebugSSRStrafeProtectedPawn.Get() == Protected;
			if (!bStrafeStillOwnsProtection)
			{
				Protected->DebugSetSSRStrafeDamageSuppressed(false);
			}
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRGameplay PROTECT pawn=%s manual=0 strafeActive=%d (DEBUG ONLY)",
				*GetNameSafe(Protected), bStrafeStillOwnsProtection ? 1 : 0);
		}
		SSRTestComponent->DebugSSRManualProtectedPawn.Reset();
		return;
	}
	AEnclaveCharacter* Char = Cast<AEnclaveCharacter>(GetPawn());
	if (!IsValid(Char) || !Char->HasAuthority() || !IsSSRGameplayGateEnabled())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning,
			"SSRGameplay PROTECT rejected=requires_authoritative_possessed_target_and_gameplay_gate (DEBUG ONLY)");
		return;
	}
	if (SSRTestComponent->DebugSSRManualProtectedPawn.IsValid() && SSRTestComponent->DebugSSRManualProtectedPawn.Get() != Char)
	{
		EnclaveDebugSSRProtect(0);
	}
	Char->DebugSetSSRStrafeDamageSuppressed(true);
	SSRTestComponent->DebugSSRManualProtectedPawn = Char;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRGameplay PROTECT pawn=%s manual=1 (server-authoritative, DEBUG ONLY)", *GetNameSafe(Char));
#else
	(void)bEnable;
#endif
}

void AEnclavePlayerController::ServerEnclaveDebugSSRProtect_Implementation(bool bEnable)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	EnclaveDebugSSRProtect(bEnable ? 1 : 0);
#else
	(void)bEnable;
#endif
}

// C3B2: drive the locally possessed pawn through ordinary CharacterMovement
// crouch, including the remote autonomous proxy. Client capsule verification
// is local/predicted; the server must still validate the historical capsule.
void AEnclavePlayerController::EnclaveDebugSSRCrouch(int32 bEnable)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (SSRTestComponent) SSRTestComponent->EnclaveDebugSSRCrouch(bEnable);
#else
	(void)bEnable;
#endif
}


// A local PlayerController feeds the exact same AddMovementInput path that the
// regular locomotion handler uses. Remote owners therefore still send UE's
// predicted CharacterMovement moves through their normal ServerMove pathway.
void AEnclavePlayerController::EnclaveDebugSSRStrafe(int32 bEnable)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (SSRTestComponent) SSRTestComponent->EnclaveDebugSSRStrafe(bEnable);
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
	if (SSRTestComponent) SSRTestComponent->EnclaveDebugSSRAutoFire(bEnable);
#else
	(void)bEnable;
#endif
}


#if ENCLAVE_INCLUDE_DEBUG_CODE
#endif

void AEnclavePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (SSRTestComponent) SSRTestComponent->TickParticipant(DeltaSeconds);
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