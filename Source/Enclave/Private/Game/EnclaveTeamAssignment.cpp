// Copyright Lightcarrier Studios

#include "Game/EnclaveTeamAssignment.h"

#include "Algo/StableSort.h"
#include "Math/RandomStream.h"

#include "Game/EnclaveURLOptions.h"

void FEnclaveTeamAssignmentRules::ApplyOptions(const FString& Options)
{
	EnclaveURLOptions::ApplyEnum(Options, TEXT("TeamAssignment"), Assignment);
	EnclaveURLOptions::ApplyBool(Options, TEXT("KeepTeams"), bKeepTeams);
}

FString FEnclaveTeamAssignmentRules::ToString() const
{
	return FString::Printf(TEXT("TeamAssignment=%s KeepTeams=%d MaxTeamSizeDifference=%d"),
		*StaticEnum<EEnclaveTeamAssignment>()->GetNameStringByValue(static_cast<int64>(Assignment)), bKeepTeams ? 1 : 0, MaxTeamSizeDifference);
}

namespace
{
	/** A team with the fewest players; ties are broken at random. */
	int32 PickSmallestTeam(const TArray<int32>& TeamSizes, FRandomStream& Random)
	{
		const int32 MinSize = FMath::Min(TeamSizes);
		TArray<int32, TInlineAllocator<8>> Smallest;
		for (int32 Team = 0; Team < TeamSizes.Num(); ++Team)
		{
			if (TeamSizes[Team] == MinSize)
			{
				Smallest.Add(Team);
			}
		}
		return Smallest[Random.RandRange(0, Smallest.Num() - 1)];
	}
}

TArray<uint8> EnclaveTeamAssignment::AssignTeams(TConstArrayView<FEnclaveTeamGroup> Groups, const FEnclaveTeamAssignmentRules& Rules,
	int32 NumTeams, FRandomStream& Random)
{
	TArray<uint8> Result;
	Result.Init(AEnclaveTeamPlayerState::NoTeam, Groups.Num());
	if (!ensureMsgf(NumTeams >= 1 && NumTeams < AEnclaveTeamPlayerState::NoTeam, TEXT("AssignTeams: NumTeams %d is out of range."), NumTeams))
	{
		return Result;
	}

	TArray<int32> TeamSizes;
	TeamSizes.Init(0, NumTeams);

	// 1. Seed: keep valid previous teams.
	TArray<int32> Unplaced;
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		const FEnclaveTeamGroup& Group = Groups[Index];
		if (Rules.bKeepTeams && Group.PreviousTeam < NumTeams)
		{
			Result[Index] = Group.PreviousTeam;
			TeamSizes[Group.PreviousTeam] += Group.Size;
		}
		else
		{
			Unplaced.Add(Index);
		}
	}

	// 2. Place the rest. Random: shuffle, then largest first (a stable sort, so
	// the shuffle still orders groups of equal size). A large group placed last
	// can't be balanced, which is why size comes first.
	switch (Rules.Assignment)
	{
	case EEnclaveTeamAssignment::Random:
	default:
		for (int32 Index = Unplaced.Num() - 1; Index > 0; --Index)
		{
			Unplaced.Swap(Index, Random.RandRange(0, Index));
		}
		Algo::StableSortBy(Unplaced, [&Groups](int32 Index) { return Groups[Index].Size; }, TGreater<>());
		for (const int32 Index : Unplaced)
		{
			const int32 Team = PickSmallestTeam(TeamSizes, Random);
			Result[Index] = static_cast<uint8>(Team);
			TeamSizes[Team] += Groups[Index].Size;
		}
		break;
	}

	// 3. Even out. A move of S players from a team of Big to a team of Small
	// narrows their gap only if S < Big - Small; each such move lowers the sum
	// of squared team sizes, so the loop always ends.
	const int32 MaxDifference = FMath::Max(1, Rules.MaxTeamSizeDifference);
	while (true)
	{
		const int32 BigSize = FMath::Max(TeamSizes);
		const int32 SmallSize = FMath::Min(TeamSizes);
		const int32 Gap = BigSize - SmallSize;
		if (Gap <= MaxDifference)
		{
			break;
		}

		// From any biggest team: the smallest group that narrows the gap, then the newest.
		int32 MoveIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Groups.Num(); ++Index)
		{
			const FEnclaveTeamGroup& Group = Groups[Index];
			if (Result[Index] >= NumTeams || TeamSizes[Result[Index]] != BigSize || Group.Size >= Gap)
			{
				continue;
			}
			if (MoveIndex == INDEX_NONE
				|| Group.Size < Groups[MoveIndex].Size
				|| (Group.Size == Groups[MoveIndex].Size && Group.JoinOrder > Groups[MoveIndex].JoinOrder))
			{
				MoveIndex = Index;
			}
		}
		if (MoveIndex == INDEX_NONE)
		{
			break;   // best effort: no move narrows the gap without splitting a group
		}

		const int32 ToTeam = PickSmallestTeam(TeamSizes, Random);
		TeamSizes[Result[MoveIndex]] -= Groups[MoveIndex].Size;
		TeamSizes[ToTeam] += Groups[MoveIndex].Size;
		Result[MoveIndex] = static_cast<uint8>(ToTeam);
	}

	return Result;
}
