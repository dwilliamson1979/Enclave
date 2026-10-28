// Copyright Lightcarrier Studios

#include "Combat/EnclaveShotTiming.h"

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
