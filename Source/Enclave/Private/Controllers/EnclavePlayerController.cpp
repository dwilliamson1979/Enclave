#include "Controllers/EnclavePlayerController.h"
#include "Characters/EnclaveCharacter.h"
#include "Combat/EnclaveHitRegistrationSubsystem.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"
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
#include "Equipment/Items/EnclaveItem.h"
#include "Inventory/Components/EnclaveInventoryComponent.h"

// Deliberately independent of Enclave.Debug.FireTrace: one toggles the
// remote comparison, the other the existing authoritative-world line drawing.
static TAutoConsoleVariable<bool> CVarDebugShotAlignment(
	TEXT("Enclave.Debug.ShotAlignment"),
	false,
	TEXT("Compare the predicted owning-client camera ray to the exact authoritative hitscan ray for each GAS Fire activation. Enable on BOTH server and owning client in multi-process PIE."),
	ECVF_Cheat);

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
				FVector::Distance(ShotTimeFrame.Center, FVector(ClientVisualProxyAnchor)),
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