// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"

/**
 * This match's roster for Reserved team slots: who holds a place on which team.
 * Built from the match-start assignment; late joiners are added as they are
 * placed. A player who leaves keeps their place for the hold time (0 = the
 * whole match), and a returning player whose place is held goes back to it.
 *
 * Plain data, no world: the team GameMode fills and asks it, and tests call it
 * directly. Players are keyed by their unique net ID as a string, the identity
 * the engine itself uses to recognise a reconnect (GameMode.cpp:705-718). The
 * engine's own copy of a leaver (InactivePlayerArray) can't be used: it
 * expires after 300 s and holds at most 16 (GameMode.cpp:46-47).
 */
class ENCLAVE_API FEnclaveTeamRoster
{
public:
	/** Empties the roster. */
	void Reset() { Entries.Reset(); }

	/** Puts Player on Team, connected. Replaces an earlier entry (a returning player). */
	void Add(const FString& Player, uint8 Team);

	/** Takes Player off the roster, freeing their place (e.g. they came back as a spectator). */
	void Remove(const FString& Player) { Entries.Remove(Player); }

	/** Player left at Now; their place is held from then. Does nothing if they aren't on the roster. */
	void MarkLeft(const FString& Player, double Now);

	/**
	 * Player's team if they hold a place at Now: connected, or absent for less
	 * than HoldSeconds (0 = the whole match). Unset if not on the roster or expired.
	 */
	TOptional<uint8> FindHeldTeam(const FString& Player, double Now, double HoldSeconds) const;

	/** Absent players whose place on Team is still held at Now. */
	int32 CountAbsentHeld(uint8 Team, double Now, double HoldSeconds) const;

	/** Absent players whose place on any team is still held at Now (they count toward server capacity). */
	int32 CountAllAbsentHeld(double Now, double HoldSeconds) const;

private:
	struct FEntry
	{
		uint8 Team = 0;

		/** When the player left; unset while they are connected. */
		TOptional<double> LeftTime;
	};

	static bool IsHeld(const FEntry& Entry, double Now, double HoldSeconds);

	TMap<FString, FEntry> Entries;
};
