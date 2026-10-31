// Copyright Lightcarrier Studios

#include "Combat/EnclaveSSRTimingPolicy.h"

#include "Characters/EnclaveCharacter.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Engine/World.h"
#include "Debug/EnclaveDebug.h"
#include "HAL/IConsoleManager.h"

#if ENCLAVE_INCLUDE_DEBUG_CODE
// Milestone 3 abuse tests: make the owning client lie about its timing evidence.
static TAutoConsoleVariable<int32> CVarDebugSSRTimingFault(
	TEXT("Enclave.Debug.SSRTimingFault"), 0,
	TEXT("Owning client: falsify SSR timing evidence. 0=off, 1=backtrack (echo a REAL received stamp at least FaultMs old), "
		"2=forged stamp (latest + 7 ms, not a recorded frame), 3=future stamp (latest + FaultMs). DEBUG ONLY."),
	ECVF_Cheat);
static TAutoConsoleVariable<float> CVarDebugSSRTimingFaultMs(
	TEXT("Enclave.Debug.SSRTimingFaultMs"), 100.0f,
	TEXT("Offset in ms used by Enclave.Debug.SSRTimingFault. DEBUG ONLY."), ECVF_Cheat);
static TAutoConsoleVariable<bool> CVarDebugSSRReplicateStamps(
	TEXT("Enclave.Debug.SSRReplicateStamps"), true,
	TEXT("Server: replicate SSR history stamps to clients (debug builds always do unless this is 0). For bandwidth A/B runs."),
	ECVF_Cheat);
#endif

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
	const UEnclaveHitTargetComponent* HitTarget = Target->GetHitTargetComponent();
	if (!IsValid(HitTarget))
	{
		return Evidence;
	}
	const FEnclaveSSRReceivedProxyUpdate& Observation = HitTarget->GetLastReceivedSSRObservation();
	const double Elapsed = World->GetTimeSeconds() - Observation.ClientReceiveWorldSeconds;
	const uint32 StampMs = EnclaveSSRTimingPolicy::QuantizeServerTimeMs(Observation.ServerStamp);
#if ENCLAVE_INCLUDE_DEBUG_CODE
	// A cheating client ignores the freshness rule, so faults are applied first.
	if (const int32 Fault = CVarDebugSSRTimingFault.GetValueOnGameThread(); Fault != 0 && Observation.ReceiveCount != 0)
	{
		const double FaultSeconds = FMath::Max(0.0f, CVarDebugSSRTimingFaultMs.GetValueOnGameThread()) / 1000.0;
		double FaultStamp = 0.0;
		if (Fault == 1)
		{
			for (const double Received : HitTarget->DebugGetReceivedSSRStamps())
			{
				if (Received <= Observation.ServerStamp - FaultSeconds) { FaultStamp = Received; }
			}
		}
		else if (Fault == 2)
		{
			FaultStamp = Observation.ServerStamp + 0.007;
		}
		else if (Fault == 3)
		{
			FaultStamp = Observation.ServerStamp + FaultSeconds;
		}
		Evidence.Target = const_cast<AEnclaveCharacter*>(Target);
		Evidence.StampMs = EnclaveSSRTimingPolicy::QuantizeServerTimeMs(FaultStamp);
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTimingFault CLIENT fault=%d target=%s realStamp=%.3f sentStamp=%.3f (DEBUG ONLY)",
			Fault, *GetNameSafe(Target), Observation.ServerStamp, EnclaveSSRTimingPolicy::DequantizeServerTimeMs(Evidence.StampMs));
		return Evidence;
	}
#endif
	if (Observation.ReceiveCount == 0 || StampMs == 0 || !FMath::IsFinite(Elapsed) || Elapsed < 0.0 ||
		Elapsed > EnclaveSSRTimingPolicy::MaxClientStampAgeSeconds)
	{
		return Evidence;
	}

	Evidence.Target = const_cast<AEnclaveCharacter*>(Target);
	Evidence.StampMs = StampMs;
	return Evidence;
}
