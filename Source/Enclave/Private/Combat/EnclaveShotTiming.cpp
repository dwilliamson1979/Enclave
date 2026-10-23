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
