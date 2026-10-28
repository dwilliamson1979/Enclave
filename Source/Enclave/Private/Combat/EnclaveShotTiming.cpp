// Copyright Lightcarrier Studios

#include "Combat/EnclaveShotTiming.h"

bool FEnclaveShotTimingContext::TryResolveTargetHistoryTime(double EffectivePresentationDelaySeconds, double& OutTargetTimeSeconds) const
{
	OutTargetTimeSeconds = 0.0;
	if (!FMath::IsFinite(ClientEstimatedShotTimeSeconds) ||
		!FMath::IsFinite(ServerReceiptTimeSeconds) ||
		!FMath::IsFinite(MaxRewindSeconds) ||
		!FMath::IsFinite(EffectivePresentationDelaySeconds) ||
		ClientEstimatedShotTimeSeconds < 0.0 || ServerReceiptTimeSeconds < 0.0 ||
		MaxRewindSeconds <= 0.0 || EffectivePresentationDelaySeconds < 0.0)
	{
		return false;
	}

	const double CandidateTargetTime = ClientEstimatedShotTimeSeconds - EffectivePresentationDelaySeconds;
	const double RewindAge = ServerReceiptTimeSeconds - CandidateTargetTime;
	if (!FMath::IsFinite(CandidateTargetTime) || CandidateTargetTime < 0.0 ||
		!FMath::IsFinite(RewindAge) || RewindAge < 0.0 || RewindAge > MaxRewindSeconds)
	{
		return false;
	}

	OutTargetTimeSeconds = CandidateTargetTime;
	return true;
}


bool FEnclaveShotTimingContext::TryResolveServerDerivedHistoryTime(double AdditionalSmoothingSeconds, double& OutTargetTimeSeconds) const
{
	OutTargetTimeSeconds = 0.0;
	if (!FMath::IsFinite(ServerReceiptTimeSeconds) ||
		!FMath::IsFinite(ServerObservedRTTSeconds) ||
		!FMath::IsFinite(MaxRewindSeconds) ||
		!FMath::IsFinite(AdditionalSmoothingSeconds) ||
		ServerReceiptTimeSeconds < 0.0 || ServerObservedRTTSeconds < 0.0 ||
		MaxRewindSeconds <= 0.0 || AdditionalSmoothingSeconds < 0.0)
	{
		return false;
	}

	const double EstimatedTargetAge = ServerObservedRTTSeconds + AdditionalSmoothingSeconds;
	const double Candidate = ServerReceiptTimeSeconds - EstimatedTargetAge;
	if (!FMath::IsFinite(EstimatedTargetAge) ||
		EstimatedTargetAge > MaxRewindSeconds ||
		!FMath::IsFinite(Candidate) || Candidate < 0.0)
	{
		return false;
	}

	OutTargetTimeSeconds = Candidate;
	return true;
}

bool FEnclaveShotTimingContext::TryResolveServerAnchoredHistoryTime(double OffsetFromRTTSeconds, double& OutTargetTimeSeconds) const
{
	OutTargetTimeSeconds = 0.0;
	if (!FMath::IsFinite(ServerReceiptTimeSeconds) ||
		!FMath::IsFinite(ServerObservedRTTSeconds) ||
		!FMath::IsFinite(MaxRewindSeconds) ||
		!FMath::IsFinite(OffsetFromRTTSeconds) ||
		ServerReceiptTimeSeconds < 0.0 || ServerObservedRTTSeconds < 0.0 ||
		MaxRewindSeconds <= 0.0)
	{
		return false;
	}

	const double TargetAge = ServerObservedRTTSeconds + OffsetFromRTTSeconds;
	const double Candidate = ServerReceiptTimeSeconds - TargetAge;
	if (!FMath::IsFinite(TargetAge) || TargetAge < 0.0 || TargetAge > MaxRewindSeconds ||
		!FMath::IsFinite(Candidate) || Candidate < 0.0)
	{
		return false;
	}

	OutTargetTimeSeconds = Candidate;
	return true;
}
