// Copyright Lightcarrier Studios

#include "Game/EnclaveTeamRoster.h"

void FEnclaveTeamRoster::Add(const FString& Player, uint8 Team)
{
	FEntry& Entry = Entries.FindOrAdd(Player);
	Entry.Team = Team;
	Entry.LeftTime.Reset();
}

void FEnclaveTeamRoster::MarkLeft(const FString& Player, double Now)
{
	if (FEntry* Entry = Entries.Find(Player))
	{
		Entry->LeftTime = Now;
	}
}

TOptional<uint8> FEnclaveTeamRoster::FindHeldTeam(const FString& Player, double Now, double HoldSeconds) const
{
	const FEntry* Entry = Entries.Find(Player);
	if (Entry && IsHeld(*Entry, Now, HoldSeconds))
	{
		return Entry->Team;
	}
	return {};
}

int32 FEnclaveTeamRoster::CountAbsentHeld(uint8 Team, double Now, double HoldSeconds) const
{
	int32 Count = 0;
	for (const TPair<FString, FEntry>& Pair : Entries)
	{
		if (Pair.Value.Team == Team && Pair.Value.LeftTime.IsSet() && IsHeld(Pair.Value, Now, HoldSeconds))
		{
			++Count;
		}
	}
	return Count;
}

int32 FEnclaveTeamRoster::CountAllAbsentHeld(double Now, double HoldSeconds) const
{
	int32 Count = 0;
	for (const TPair<FString, FEntry>& Pair : Entries)
	{
		if (Pair.Value.LeftTime.IsSet() && IsHeld(Pair.Value, Now, HoldSeconds))
		{
			++Count;
		}
	}
	return Count;
}

bool FEnclaveTeamRoster::IsHeld(const FEntry& Entry, double Now, double HoldSeconds)
{
	// Connected players always hold their place; HoldSeconds 0 holds an absent
	// player's place for the whole match.
	return !Entry.LeftTime.IsSet() || HoldSeconds <= 0.0 || Now - Entry.LeftTime.GetValue() < HoldSeconds;
}
