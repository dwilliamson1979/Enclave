// Copyright Lightcarrier Studios

#include "Combat/Components/EnclaveTargetHistoryComponent.h"

#include "Characters/EnclaveCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Game/EnclaveGameModeBase.h"
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
}

void UEnclaveTargetHistoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InvalidateHistory();
	Super::EndPlay(EndPlayReason);
}

void UEnclaveTargetHistoryComponent::InvalidateHistory()
{
	Frames.Reset();
	bBreakBeforeNextCapture = false;
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

	const AEnclaveCharacter* Character = Cast<AEnclaveCharacter>(GetOwner());
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const UCapsuleComponent* Capsule = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	const UEnclaveAvatarLifecycleComponent* Lifecycle = IsValid(Character)
		? Character->FindComponentByClass<UEnclaveAvatarLifecycleComponent>() : nullptr;
	if (!IsValid(Character) || !Character->HasAuthority() || !IsValid(GameState) ||
		!IsValid(Capsule) || (IsValid(Lifecycle) && Lifecycle->IsDead()))
	{
		InvalidateHistory();
		return;
	}

	FEnclaveTargetCapsuleFrame Next;
	Next.ServerTimeSeconds = GameState->GetServerWorldTimeSeconds();
	Next.Center = Capsule->GetComponentLocation();
	Next.Rotation = Capsule->GetComponentQuat();
	Next.RadiusCm = Capsule->GetScaledCapsuleRadius();
	Next.HalfHeightCm = Capsule->GetScaledCapsuleHalfHeight();
#if ENCLAVE_INCLUDE_DEBUG_CODE
	// The transform and process clock are sampled together after physics. This
	// monotonic sample is for same-process PIE diagnosis ONLY, not hit authority.
	Next.DebugVelocity = Character->GetVelocity();
	Next.DebugCaptureProcessSeconds = FPlatformTime::Seconds();
	Next.DebugCaptureWorldSeconds = World->GetTimeSeconds();
	Next.DebugCaptureFrame = GFrameCounter;
#endif
	if (!FMath::IsFinite(Next.ServerTimeSeconds) || Next.ServerTimeSeconds < 0.0 ||
		Next.Center.ContainsNaN() || Next.Rotation.ContainsNaN() ||
		!FMath::IsFinite(Next.RadiusCm) || !FMath::IsFinite(Next.HalfHeightCm) ||
		Next.RadiusCm <= 0.0f || Next.HalfHeightCm < Next.RadiusCm)
	{
		InvalidateHistory();
		return;
	}

	const AEnclaveGameModeBase* GameMode = World->GetAuthGameMode<AEnclaveGameModeBase>();
	if (!IsValid(GameMode))
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
    // Stage A: the replicated observation is derived from the exact recorded sample.
    // Normal actor property replication may deliver it later; it does not change movement.
    const_cast<AEnclaveCharacter*>(Character)->RecordSSRServerCapsuleObservation(
        Next.ServerTimeSeconds, Next.Center, Next.RadiusCm, Next.HalfHeightCm);
	bBreakBeforeNextCapture = false;

	const double MaxRewindSeconds = GameMode->GetMaxRewindSeconds();
	const double EarliestAllowedTime = Next.ServerTimeSeconds - MaxRewindSeconds - EnclaveTargetHistory::ExtraRetentionSeconds;
	int32 NumExpired = 0;
	while (NumExpired < Frames.Num() && Frames[NumExpired].ServerTimeSeconds < EarliestAllowedTime)
	{
		++NumExpired;
	}
	if (NumExpired > 0)
	{
		Frames.RemoveAt(0, NumExpired, EAllowShrinking::No);
	}
	if (Frames.Num() > EnclaveTargetHistory::MaxFrames)
	{
		Frames.RemoveAt(0, Frames.Num() - EnclaveTargetHistory::MaxFrames, EAllowShrinking::No);
	}

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
	const AEnclaveCharacter* Character = Cast<AEnclaveCharacter>(GetOwner());
	const UEnclaveAvatarLifecycleComponent* Lifecycle = IsValid(Character)
		? Character->FindComponentByClass<UEnclaveAvatarLifecycleComponent>() : nullptr;
	if (!IsValid(Character) || !Character->HasAuthority() ||
		(IsValid(Lifecycle) && Lifecycle->IsDead()) ||
		!FMath::IsFinite(ServerTimeSeconds) || Frames.IsEmpty() ||
		ServerTimeSeconds < Frames[0].ServerTimeSeconds ||
		ServerTimeSeconds > Frames.Last().ServerTimeSeconds)
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
#if ENCLAVE_INCLUDE_DEBUG_CODE
			OutFrame.DebugVelocity = FMath::Lerp(Older.DebugVelocity, Newer.DebugVelocity, Alpha);
#endif
			OutFrame.Rotation = FQuat::Slerp(Older.Rotation, Newer.Rotation, Alpha).GetNormalized();
			OutFrame.RadiusCm = FMath::Lerp(Older.RadiusCm, Newer.RadiusCm, Alpha);
			OutFrame.HalfHeightCm = FMath::Lerp(Older.HalfHeightCm, Newer.HalfHeightCm, Alpha);
			return true;
		}
	}
	return false;
}


bool UEnclaveTargetHistoryComponent::TryMatchVisualAnchorToHistory(const FVector& VisualAnchor,
	double ClientShotTimeSeconds, double LookbackSeconds, double LookaheadSeconds,
	FEnclaveVisualHistoryTimeMatch& OutMatch) const
{
	OutMatch = FEnclaveVisualHistoryTimeMatch();
	const AEnclaveCharacter* Character = Cast<AEnclaveCharacter>(GetOwner());
	const UEnclaveAvatarLifecycleComponent* Lifecycle = IsValid(Character)
		? Character->FindComponentByClass<UEnclaveAvatarLifecycleComponent>() : nullptr;
	if (!IsValid(Character) || !Character->HasAuthority() ||
		(IsValid(Lifecycle) && Lifecycle->IsDead()) || VisualAnchor.ContainsNaN() ||
		!FMath::IsFinite(ClientShotTimeSeconds) || ClientShotTimeSeconds < 0.0 ||
		!FMath::IsFinite(LookbackSeconds) || !FMath::IsFinite(LookaheadSeconds) ||
		LookbackSeconds < 0.0 || LookaheadSeconds < 0.0 || Frames.Num() < 2)
	{
		return false;
	}

	// Limit this diagnostic even if an incorrect caller supplies a huge window.
	const double Start = ClientShotTimeSeconds - FMath::Min(LookbackSeconds, 0.150);
	const double End = ClientShotTimeSeconds + FMath::Min(LookaheadSeconds, 0.150);
	if (Start > Frames.Last().ServerTimeSeconds || End < Frames[0].ServerTimeSeconds)
	{
		return false;
	}

	struct FCandidate
	{
		double TimeSeconds;
		double ResidualCm;
	};
	TArray<FCandidate, TInlineAllocator<128>> Candidates;
	double BestResidualCm = TNumericLimits<double>::Max();
	double BestMatchTime = 0.0;
	double BestSpeedCmPerSecond = 0.0;

	for (int32 Index = 1; Index < Frames.Num(); ++Index)
	{
		const FEnclaveTargetCapsuleFrame& Older = Frames[Index - 1];
		const FEnclaveTargetCapsuleFrame& Newer = Frames[Index];
		const double Span = Newer.ServerTimeSeconds - Older.ServerTimeSeconds;
		// This diagnostic fits a linear center trajectory. A shape step is
		// atomic in the authoritative query, so it must not be fitted as a
		// continuous visual motion segment either.
		if (Newer.bBreakBefore || Span <= 0.0 || Span > EnclaveTargetHistory::MaxCaptureGapSeconds ||
			EnclaveTargetHistory::HasShapeStep(Older, Newer))
		{
			continue;
		}

		const double SegmentStart = FMath::Max(Start, Older.ServerTimeSeconds);
		const double SegmentEnd = FMath::Min(End, Newer.ServerTimeSeconds);
		if (SegmentStart >= SegmentEnd)
		{
			continue;
		}

		const FVector PositionStart = FMath::Lerp(Older.Center, Newer.Center,
			(SegmentStart - Older.ServerTimeSeconds) / Span);
		const FVector PositionEnd = FMath::Lerp(Older.Center, Newer.Center,
			(SegmentEnd - Older.ServerTimeSeconds) / Span);
		const FVector Motion = PositionEnd - PositionStart;
		const double MotionLengthSq = Motion.SizeSquared();
		const double Speed = FMath::Sqrt(MotionLengthSq) / (SegmentEnd - SegmentStart);
		// A stationary capsule cannot identify *when* the client viewed it.
		if (Speed < 150.0 || MotionLengthSq < FMath::Square(0.1))
		{
			continue;
		}

		const double Alpha = FMath::Clamp(FVector::DotProduct(VisualAnchor - PositionStart, Motion) /
			MotionLengthSq, 0.0, 1.0);
		const double MatchTime = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
		const double Residual = FVector::Dist(VisualAnchor, PositionStart + Alpha * Motion);
		Candidates.Add({ MatchTime, Residual });
		if (Residual < BestResidualCm)
		{
			BestResidualCm = Residual;
			BestMatchTime = MatchTime;
			BestSpeedCmPerSecond = Speed;
#if ENCLAVE_INCLUDE_DEBUG_CODE
			const double FullSegmentAlpha = (MatchTime - Older.ServerTimeSeconds) / Span;
			OutMatch.DebugMatchedProcessSeconds = FMath::Lerp(
				Older.DebugCaptureProcessSeconds, Newer.DebugCaptureProcessSeconds, FullSegmentAlpha);
			OutMatch.DebugMatchedWorldSeconds = FMath::Lerp(
				Older.DebugCaptureWorldSeconds, Newer.DebugCaptureWorldSeconds, FullSegmentAlpha);
			OutMatch.DebugOlderFrame = Older.DebugCaptureFrame;
			OutMatch.DebugNewerFrame = Newer.DebugCaptureFrame;
#endif
		}
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	OutMatch.MatchedServerTimeSeconds = BestMatchTime;
	OutMatch.SignedOffsetSeconds = BestMatchTime - ClientShotTimeSeconds;
	OutMatch.ResidualCm = BestResidualCm;
	OutMatch.MatchedSpeedCmPerSecond = BestSpeedCmPerSecond;
	OutMatch.EvaluatedSegments = Candidates.Num();
	// Another similarly fitting point more than 25ms away makes the inverse
	// time underdetermined (e.g. near a patrol reversal). Report, do not guess.
	for (const FCandidate& Candidate : Candidates)
	{
		if (FMath::Abs(Candidate.TimeSeconds - BestMatchTime) > 0.025 &&
			Candidate.ResidualCm <= BestResidualCm + 1.5)
		{
			OutMatch.bAmbiguous = true;
			break;
		}
	}
	return true;
}
