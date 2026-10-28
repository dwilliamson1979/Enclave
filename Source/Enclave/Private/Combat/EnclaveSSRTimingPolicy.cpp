// Copyright Lightcarrier Studios

#include "Combat/EnclaveSSRTimingPolicy.h"

#include "Characters/EnclaveCharacter.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Engine/World.h"

uint32 EnclaveSSRTimingPolicy::QuantizeServerTimeMs(double ServerTimeSeconds)
{
	if (!FMath::IsFinite(ServerTimeSeconds) || ServerTimeSeconds <= 0.0)
	{
		return 0;
	}
	const double Ms = FMath::RoundToDouble(ServerTimeSeconds * 1000.0);
	return Ms >= 1.0 && Ms <= static_cast<double>(MAX_uint32) ? static_cast<uint32>(Ms) : 0;
}

FEnclaveSSRTimingResolution ResolveSSRTiming(const FEnclaveSSRTimingSettings& Settings,
	const FEnclaveShotTimingContext& Timing, bool bHasVerifiedStamp, double VerifiedStampTimeSeconds)
{
	FEnclaveSSRTimingResolution Result;
	const auto Fail = [&Result](EEnclaveSSRUnavailableReason Reason)
	{
		Result = FEnclaveSSRTimingResolution();
		Result.UnavailableReason = Reason;
		return Result;
	};

	if (!FMath::IsFinite(Timing.ServerReceiptTimeSeconds) || Timing.ServerReceiptTimeSeconds <= 0.0 ||
		!FMath::IsFinite(Timing.MaxRewindSeconds) || Timing.MaxRewindSeconds <= 0.0)
	{
		return Fail(EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
	}

	// A verified stamp still has to fall inside the server's own rewind window.
	const double StampAge = Timing.ServerReceiptTimeSeconds - VerifiedStampTimeSeconds;
	const bool bStampInWindow = bHasVerifiedStamp && FMath::IsFinite(VerifiedStampTimeSeconds) &&
		FMath::IsFinite(StampAge) && StampAge >= 0.0 && StampAge <= Timing.MaxRewindSeconds;

	if (Settings.Source == EEnclaveSSRTimingSource::EchoedStamp)
	{
		if (!bStampInWindow)
		{
			return Fail(bHasVerifiedStamp ? EEnclaveSSRUnavailableReason::RewindWindowExceeded
				: EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
		}
		Result.bValid = true;
		Result.BaseTimeSeconds = VerifiedStampTimeSeconds;
		Result.bUseStamp = true;
		Result.StampTimeSeconds = VerifiedStampTimeSeconds;
		return Result;
	}

	double AnchorTime = 0.0;
	if (!Timing.TryResolveServerAnchoredHistoryTime(Settings.AnchorOffsetSeconds, AnchorTime))
	{
		const bool bHasRTT = FMath::IsFinite(Timing.ServerObservedRTTSeconds) && Timing.ServerObservedRTTSeconds >= 0.0;
		return Fail(bHasRTT ? EEnclaveSSRUnavailableReason::RewindWindowExceeded
			: EEnclaveSSRUnavailableReason::ServerTimingUnavailable);
	}
	Result.bValid = true;
	Result.BaseTimeSeconds = AnchorTime;

	if (Settings.Source == EEnclaveSSRTimingSource::Hybrid && bStampInWindow)
	{
		// Bound "backtrack": the stamp may only move the target a little way
		// from where the server anchor already places it.
		const double AnchorAge = Timing.ServerReceiptTimeSeconds - AnchorTime;
		const double Delta = StampAge - AnchorAge;
		if (Delta >= -FMath::Max(0.0, Settings.StampEarlyToleranceSeconds) &&
			Delta <= FMath::Max(0.0, Settings.StampLateToleranceSeconds))
		{
			Result.bUseStamp = true;
			Result.StampTimeSeconds = VerifiedStampTimeSeconds;
		}
		else
		{
			Result.bStampRejected = true;
		}
	}
	else if (Settings.Source == EEnclaveSSRTimingSource::Hybrid && bHasVerifiedStamp)
	{
		Result.bStampRejected = true;
	}
	return Result;
}

FEnclaveSSRClientTimingEvidence CaptureClientSSRTimingEvidence(UWorld* World,
	AActor* Shooter, const FVector& Origin, const FVector& Direction, double RangeCm)
{
	FEnclaveSSRClientTimingEvidence Evidence;
	if (!IsValid(World) || World->GetNetMode() != NM_Client || !IsValid(Shooter) ||
		Origin.ContainsNaN() || Direction.ContainsNaN() || !FMath::IsFinite(RangeCm) || RangeCm <= 0.0)
	{
		return Evidence;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnclaveSSRClientTimingEvidence), true);
	Params.AddIgnoredActor(Shooter);
	TArray<AActor*> AttachedActors;
	Shooter->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction.GetSafeNormal() * RangeCm, ECC_Weapon, Params))
	{
		return Evidence;
	}

	const AEnclaveCharacter* Target = Cast<AEnclaveCharacter>(Hit.GetActor());
	if (!IsValid(Target) || Target->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return Evidence;
	}
	const FEnclaveSSRReceivedProxyUpdate& Observation = Target->GetLastReceivedSSRObservation();
	const double Elapsed = World->GetTimeSeconds() - Observation.ClientReceiveWorldSeconds;
	const uint32 StampMs = EnclaveSSRTimingPolicy::QuantizeServerTimeMs(Observation.ServerStamp);
	if (Observation.ReceiveCount == 0 || StampMs == 0 || !FMath::IsFinite(Elapsed) || Elapsed < 0.0 ||
		Elapsed > EnclaveSSRTimingPolicy::MaxClientStampAgeSeconds)
	{
		return Evidence;
	}

	Evidence.Target = const_cast<AEnclaveCharacter*>(Target);
	Evidence.StampMs = StampMs;
	return Evidence;
}
