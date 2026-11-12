// Copyright Lightcarrier Studios

#include "Game/EnclaveSpawnSelection.h"

#include "Math/RandomStream.h"

int32 EnclaveSpawnSelection::ChooseStart(TConstArrayView<FEnclaveSpawnCandidate> Candidates, TConstArrayView<FVector> EnemyLocations,
	const FEnclaveSpawnRules& Rules, FRandomStream& Random)
{
	// Blocked starts only as a last resort.
	const bool bAnyClear = Candidates.ContainsByPredicate([](const FEnclaveSpawnCandidate& Candidate) { return !Candidate.bBlocked; });

	// Score: distance to the nearest living enemy (0 for all with no enemies).
	TArray<int32, TInlineAllocator<32>> Eligible;
	TArray<double, TInlineAllocator<32>> Scores;
	double BestScore = 0.0;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FEnclaveSpawnCandidate& Candidate = Candidates[Index];
		if (bAnyClear && Candidate.bBlocked)
		{
			continue;
		}

		double NearestSquared = EnemyLocations.IsEmpty() ? 0.0 : TNumericLimits<double>::Max();
		for (const FVector& Enemy : EnemyLocations)
		{
			NearestSquared = FMath::Min(NearestSquared, FVector::DistSquared(Candidate.Location, Enemy));
		}

		const double Score = FMath::Sqrt(NearestSquared);
		Eligible.Add(Index);
		Scores.Add(Score);
		BestScore = FMath::Max(BestScore, Score);
	}

	if (Eligible.IsEmpty())
	{
		return INDEX_NONE;
	}

	// Random among the starts within the margin of the best (the best is always in it).
	const double Threshold = BestScore * FMath::Clamp(Rules.RandomMarginFraction, 0.0f, 1.0f);
	TArray<int32, TInlineAllocator<32>> Pool;
	for (int32 Slot = 0; Slot < Eligible.Num(); ++Slot)
	{
		if (Scores[Slot] >= Threshold)
		{
			Pool.Add(Eligible[Slot]);
		}
	}

	return Pool[Random.RandRange(0, Pool.Num() - 1)];
}
