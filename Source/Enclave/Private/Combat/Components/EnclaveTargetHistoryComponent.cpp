// Copyright Lightcarrier Studios

#include "Combat/Components/EnclaveTargetHistoryComponent.h"
#include "Combat/EnclaveSSRTimingPolicy.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Combat/Components/EnclaveCombatPolicyComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"
#include "Debug/EnclaveDebug.h"
#include "Engine/World.h"
#if ENCLAVE_INCLUDE_DEBUG_CODE
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"
#endif

#if ENCLAVE_INCLUDE_DEBUG_CODE
#include "HAL/IConsoleManager.h"
static TAutoConsoleVariable<int32> CVarDebugSSRHistory(
	TEXT("Enclave.Debug.SSRHistory"), 0,
	TEXT("Log a 2-second server-side summary of Stage C2 capsule history (0=off, 1=on). Does not enable rewind."),
	ECVF_Cheat);
#endif

namespace EnclaveTargetHistory
{
	// C2 records every authoritative frame, including dedicated-server frames.
	// A fixed extra margin keeps a little history beyond the policy's rewind cap.
	constexpr double ExtraRetentionSeconds = 0.125;
	constexpr double MaxCaptureGapSeconds = 0.150;
	constexpr int32 MaxFrames = 128;
	constexpr double MaxContinuousSpeedCmPerSecond = 2000.0;
	constexpr double MovementSlackCm = 30.0;
	constexpr float ShapeChangeToleranceCm = 0.5f;

	// Crouching changes the capsule center and half-height together. Compare
	// the actual bottom of the capsule (along its world-space axis) so a
	// normal crouch does not look like a teleport at high server tick rates.
	static FVector GetCapsuleBottom(const FEnclaveTargetCapsuleFrame& Frame)
	{
		return Frame.Center - Frame.Rotation.GetNormalized().GetAxisZ() * static_cast<double>(Frame.HalfHeightCm);
	}

	static bool HasShapeStep(const FEnclaveTargetCapsuleFrame& Older, const FEnclaveTargetCapsuleFrame& Newer)
	{
		return FMath::Abs(Older.RadiusCm - Newer.RadiusCm) > ShapeChangeToleranceCm ||
			FMath::Abs(Older.HalfHeightCm - Newer.HalfHeightCm) > ShapeChangeToleranceCm;
	}
}

UEnclaveTargetHistoryComponent::UEnclaveTargetHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

void UEnclaveTargetHistoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// This component is present on replicated avatars, but ONLY the server
	// records target geometry. Clients neither tick nor store historical frames.
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() ||
		!GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Frames.Reserve(48);

	if (UEnclaveAvatarLifecycleComponent* Lifecycle = GetOwner()->FindComponentByClass<UEnclaveAvatarLifecycleComponent>())
	{
		OwnerLifecycle = Lifecycle;
		bOwnerDead = Lifecycle->IsDead();
		Lifecycle->OnLifecycleStateChanged().AddUObject(this, &UEnclaveTargetHistoryComponent::HandleOwnerLifecycleStateChanged);
	}
}

void UEnclaveTargetHistoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEnclaveAvatarLifecycleComponent* Lifecycle = OwnerLifecycle.Get())
	{
		Lifecycle->OnLifecycleStateChanged().RemoveAll(this);
	}
	OwnerLifecycle.Reset();

	InvalidateHistory();
	Super::EndPlay(EndPlayReason);
}

void UEnclaveTargetHistoryComponent::HandleOwnerLifecycleStateChanged(EEnclaveAvatarLifecycleState OldState, EEnclaveAvatarLifecycleState NewState)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (NewState == EEnclaveAvatarLifecycleState::Dead)
	{
		bOwnerDead = true;
		// Retain a bounded pre-death rewind window for legitimate trade kills.
		// Historical queries at/after this authoritative death time exclude the corpse.
		// A missing GameState passes 0, which MarkDeath treats as invalid and clears
		// the history; TickComponent then re-marks death on the next valid tick.
		const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		MarkDeath(IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : 0.0);
	}
	else if (OldState == EEnclaveAvatarLifecycleState::Dead)
	{
		bOwnerDead = false;
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (CVarDebugSSRHistory.GetValueOnGameThread() != 0)
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRHistoryLifecycle SERVER actor=%s event=revived historyReset=1 priorDeathStamp=%.6f",
				*GetNameSafe(GetOwner()), DeathTimeSeconds);
#endif
		// A resurrected avatar is a fresh life and cannot inherit older capsule samples.
		InvalidateHistory();
	}
}

void UEnclaveTargetHistoryComponent::InvalidateHistory()
{
	Frames.Reset();
	bBreakBeforeNextCapture = false;
	DeathTimeSeconds = -1.0;
}

void UEnclaveTargetHistoryComponent::MarkDeath(double ServerDeathTimeSeconds)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() ||
		!FMath::IsFinite(ServerDeathTimeSeconds) || ServerDeathTimeSeconds <= 0.0)
	{
		// Never infer a death timestamp from untrusted client clocks.
		InvalidateHistory();
		return;
	}
#if ENCLAVE_INCLUDE_DEBUG_CODE
    if (CVarDebugSSRHistory.GetValueOnGameThread() != 0)
        ENCLAVE_LOG(LogEnclaveCombat, Log,
            "SSRHistoryLifecycle SERVER actor=%s event=death deathStamp=%.6f retainedFrames=%d",
            *GetNameSafe(GetOwner()), ServerDeathTimeSeconds, Frames.Num());
#endif
	DeathTimeSeconds = ServerDeathTimeSeconds;
	bBreakBeforeNextCapture = true;
}

bool UEnclaveTargetHistoryComponent::WasDeadAtTime(double ServerTimeSeconds) const
{
	return FMath::IsFinite(ServerTimeSeconds) && DeathTimeSeconds > 0.0 &&
		ServerTimeSeconds >= DeathTimeSeconds;
}

bool UEnclaveTargetHistoryComponent::HasRecordedFrameStamp(double ServerTimeSeconds) const
{
	if (!FMath::IsFinite(ServerTimeSeconds) || ServerTimeSeconds <= 0.0) return false;
	for (const FEnclaveTargetCapsuleFrame& Frame : Frames)
	{
		if (Frame.ServerTimeSeconds == ServerTimeSeconds) return true;
	}
	return false;
}

bool UEnclaveTargetHistoryComponent::TryResolveRecordedStampMs(uint32 StampMs, double& OutServerTimeSeconds) const
{
	OutServerTimeSeconds = 0.0;
	if (StampMs == 0) return false;
	int32 Matches = 0;
	for (const FEnclaveTargetCapsuleFrame& Frame : Frames)
	{
		if (EnclaveSSRTimingPolicy::QuantizeServerTimeMs(Frame.ServerTimeSeconds) == StampMs)
		{
			OutServerTimeSeconds = Frame.ServerTimeSeconds;
			++Matches;
		}
	}
	if (Matches != 1)
	{
		OutServerTimeSeconds = 0.0;
		return false;
	}
	return true;
}

void UEnclaveTargetHistoryComponent::TrimExpiredFrames(double CurrentServerTimeSeconds, double MaxRewindSeconds)
{
	if (!FMath::IsFinite(CurrentServerTimeSeconds) || !FMath::IsFinite(MaxRewindSeconds) ||
		MaxRewindSeconds <= 0.0)
	{
		InvalidateHistory();
		return;
	}
	const double EarliestAllowed = CurrentServerTimeSeconds - MaxRewindSeconds - EnclaveTargetHistory::ExtraRetentionSeconds;
	int32 Expired = 0;
	while (Expired < Frames.Num() && Frames[Expired].ServerTimeSeconds < EarliestAllowed)
	{
		++Expired;
	}
	if (Expired > 0) Frames.RemoveAt(0, Expired, EAllowShrinking::No);
	if (Frames.Num() > EnclaveTargetHistory::MaxFrames)
		Frames.RemoveAt(0, Frames.Num() - EnclaveTargetHistory::MaxFrames, EAllowShrinking::No);
}

void UEnclaveTargetHistoryComponent::MarkHistoryDiscontinuity()
{
	// The next authoritative sample starts a new continuous segment.
	// Keep the previous segment within the normal retention limit: a shot
	// captured before the teleport can still query the old position.
	if (IsValid(GetOwner()) && GetOwner()->HasAuthority())
	{
		bBreakBeforeNextCapture = true;
	}
}

bool UEnclaveTargetHistoryComponent::GetHistoryTimeBounds(double& OutOldestTime, double& OutNewestTime) const
{
	OutOldestTime = 0.0;
	OutNewestTime = 0.0;
	if (Frames.IsEmpty())
	{
		return false;
	}

	OutOldestTime = Frames[0].ServerTimeSeconds;
	OutNewestTime = Frames.Last().ServerTimeSeconds;
	return true;
}

void UEnclaveTargetHistoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	if (!IsValid(Character) || !Character->HasAuthority() || !IsValid(GameState) || !IsValid(Capsule))
	{
		InvalidateHistory();
		return;
	}
	if (bOwnerDead)
	{
		// Keep pre-death history for the remaining rewind window, never sample corpse geometry.
		if (DeathTimeSeconds <= 0.0) MarkDeath(GameState->GetServerWorldTimeSeconds());
		const UEnclaveCombatPolicyComponent* DeathCombatPolicy = UEnclaveCombatPolicyComponent::Get(World);
		if (IsValid(DeathCombatPolicy))
			TrimExpiredFrames(GameState->GetServerWorldTimeSeconds(), DeathCombatPolicy->GetMaxRewindSeconds());
		else InvalidateHistory();
		return;
	}
	// Revives are handled by HandleOwnerLifecycleStateChanged, which clears the history.

	FEnclaveTargetCapsuleFrame Next;
	Next.ServerTimeSeconds = GameState->GetServerWorldTimeSeconds();
	Next.Center = Capsule->GetComponentLocation();
	Next.Rotation = Capsule->GetComponentQuat();
	Next.RadiusCm = Capsule->GetScaledCapsuleRadius();
	Next.HalfHeightCm = Capsule->GetScaledCapsuleHalfHeight();
	if (!FMath::IsFinite(Next.ServerTimeSeconds) || Next.ServerTimeSeconds < 0.0 ||
		Next.Center.ContainsNaN() || Next.Rotation.ContainsNaN() ||
		!FMath::IsFinite(Next.RadiusCm) || !FMath::IsFinite(Next.HalfHeightCm) ||
		Next.RadiusCm <= 0.0f || Next.HalfHeightCm < Next.RadiusCm)
	{
		InvalidateHistory();
		return;
	}

	const UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(World);
	if (!IsValid(CombatPolicy))
	{
		// No authoritative rewind policy. Do not retain history under a guessed cap.
		InvalidateHistory();
		return;
	}

	Next.bBreakBefore = bBreakBeforeNextCapture;
	if (!Frames.IsEmpty())
	{
		const FEnclaveTargetCapsuleFrame& Previous = Frames.Last();
		const double SampleGap = Next.ServerTimeSeconds - Previous.ServerTimeSeconds;
		if (SampleGap == 0.0)
		{
			// GameState time did not advance. A duplicate timestamp would break
			// the ordered history; wait for the next authoritative sample.
			return;
		}
		if (SampleGap < 0.0)
		{
			// Authoritative clock reset: old sample timestamps are no longer
			// comparable to new timestamps. This is an identity/timebase reset,
			// not a teleport, so a full invalidation is required.
			InvalidateHistory();
		}
		else
		{
			const double MaxContinuousDisplacement = EnclaveTargetHistory::MaxContinuousSpeedCmPerSecond * SampleGap +
				EnclaveTargetHistory::MovementSlackCm;
			const bool bGap = SampleGap > EnclaveTargetHistory::MaxCaptureGapSeconds;
			// Capsule center movement includes the height adjustment from
			// crouch/uncrouch. The bottom remains fixed for an ordinary
			// standing-on-ground shape change, regardless of server tick rate.
			const bool bJump = !bGap &&
				FVector::DistSquared(EnclaveTargetHistory::GetCapsuleBottom(Previous),
					EnclaveTargetHistory::GetCapsuleBottom(Next)) > FMath::Square(MaxContinuousDisplacement);
			Next.bBreakBefore |= bGap || bJump;
#if ENCLAVE_INCLUDE_DEBUG_CODE
			if (CVarDebugSSRHistory.GetValueOnGameThread() != 0)
			{
				if (Next.bBreakBefore)
				{
					ENCLAVE_LOG(LogEnclaveCombat, Log,
						"SSRHistory BREAK target=%s reason=%s gapMs=%.1f retained=%d (no interpolation across boundary)",
						*GetNameSafe(Character), bBreakBeforeNextCapture ? TEXT("explicit") : (bGap ? TEXT("gap") : TEXT("jump")),
						SampleGap * 1000.0, Frames.Num());
				}
				const bool bShapeChange = EnclaveTargetHistory::HasShapeStep(Previous, Next);
				if (bShapeChange)
				{
					ENCLAVE_LOG(LogEnclaveCombat, Log,
						"SSRHistory SHAPE target=%s halfHeight=%.1f->%.1f radius=%.1f->%.1f retained=%d (shape step, no reset)",
						*GetNameSafe(Character), Previous.HalfHeightCm, Next.HalfHeightCm,
						Previous.RadiusCm, Next.RadiusCm, Frames.Num());
				}
			}
#endif
		}
	}

	// There can be no earlier segment to break after a reset/first capture.
	if (Frames.IsEmpty())
	{
		Next.bBreakBefore = false;
	}
	Frames.Add(Next);
	// Subscribers receive the exact authoritative frame that was appended.
	CapsuleFrameRecorded.Broadcast(Frames.Last());
	bBreakBeforeNextCapture = false;

	TrimExpiredFrames(Next.ServerTimeSeconds, CombatPolicy->GetMaxRewindSeconds());

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (CVarDebugSSRHistory.GetValueOnGameThread() != 0 &&
		(LastDebugSummaryTime < 0.0 || Next.ServerTimeSeconds - LastDebugSummaryTime >= 2.0))
	{
		LastDebugSummaryTime = Next.ServerTimeSeconds;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRHistory SERVER target=%s frames=%d oldest=%.6f newest=%.6f spanMs=%.1f capsule=%s (recording only; no rewind)",
			*GetNameSafe(Character), Frames.Num(), Frames[0].ServerTimeSeconds,
			Frames.Last().ServerTimeSeconds,
			(Frames.Last().ServerTimeSeconds - Frames[0].ServerTimeSeconds) * 1000.0,
			*Next.Center.ToCompactString());
	}
#endif
}

bool UEnclaveTargetHistoryComponent::TryGetCapsuleAtTime(double ServerTimeSeconds,
	FEnclaveTargetCapsuleFrame& OutFrame) const
{
	OutFrame = FEnclaveTargetCapsuleFrame();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || !Character->HasAuthority() || WasDeadAtTime(ServerTimeSeconds) ||
		!FMath::IsFinite(ServerTimeSeconds) || Frames.IsEmpty() ||
		ServerTimeSeconds < Frames[0].ServerTimeSeconds)
	{
		return false;
	}

	// Authoritative timestamps are transported losslessly as double.
	// Never snap future times onto a recorded frame.
	if (ServerTimeSeconds > Frames.Last().ServerTimeSeconds)
	{
		return false;
	}

	// The history is short and strictly ordered. Never extrapolate before
	// first sample, after last sample, or across a cleared discontinuity.
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		const FEnclaveTargetCapsuleFrame& Older = Frames[Index];
		if (ServerTimeSeconds == Older.ServerTimeSeconds)
		{
			OutFrame = Older;
			return true;
		}
		if (Index + 1 >= Frames.Num())
		{
			break;
		}
		const FEnclaveTargetCapsuleFrame& Newer = Frames[Index + 1];
		if (ServerTimeSeconds > Older.ServerTimeSeconds && ServerTimeSeconds < Newer.ServerTimeSeconds)
		{
			const double Span = Newer.ServerTimeSeconds - Older.ServerTimeSeconds;
			if (Newer.bBreakBefore || Span <= 0.0 || Span > EnclaveTargetHistory::MaxCaptureGapSeconds)
			{
				return false;
			}
			// A shape transition is atomic: interpolating center/orientation while
			// retaining the older dimensions creates a capsule that never existed
			// (and may penetrate the floor). Use the whole older geometry up to
			// the newer sample, which is returned exactly at its own timestamp.
			if (EnclaveTargetHistory::HasShapeStep(Older, Newer))
			{
				OutFrame = Older;
				OutFrame.ServerTimeSeconds = ServerTimeSeconds;
				return true;
			}

			const float Alpha = static_cast<float>((ServerTimeSeconds - Older.ServerTimeSeconds) / Span);
			OutFrame.ServerTimeSeconds = ServerTimeSeconds;
			OutFrame.Center = FMath::Lerp(Older.Center, Newer.Center, Alpha);
			OutFrame.Rotation = FQuat::Slerp(Older.Rotation, Newer.Rotation, Alpha).GetNormalized();
			OutFrame.RadiusCm = FMath::Lerp(Older.RadiusCm, Newer.RadiusCm, Alpha);
			OutFrame.HalfHeightCm = FMath::Lerp(Older.HalfHeightCm, Newer.HalfHeightCm, Alpha);
			return true;
		}
	}
	return false;
}


