// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Player/EnclaveTeamPlayerState.h"
#include "EnclaveTeamAssignment.generated.h"

struct FRandomStream;

/** How groups without a team are placed. SkillBalance comes when ratings exist. */
UENUM()
enum class EEnclaveTeamAssignment : uint8
{
	/** The smallest team; ties are broken at random, and groups are placed in random order. */
	Random
};

/** Whether a player who leaves mid-match keeps their place on their team. */
UENUM()
enum class EEnclaveTeamSlots : uint8
{
	/** Only connected players hold a place: a leaver's place goes to whoever joins next. */
	Open,
	/**
	 * The match-start teams are this match's roster (FEnclaveTeamRoster): a
	 * player who leaves keeps their place for ReservedSlotHoldSeconds, team
	 * sizes count those places, and a returning player always gets their team.
	 */
	Reserved
};

/**
 * A team mode's assignment rules: editor settings on the GameMode, overridable
 * with URL options (?TeamAssignment=Random?KeepTeams=0?TeamSlots=Reserved?ReservedSlotHold=120).
 */
USTRUCT()
struct ENCLAVE_API FEnclaveTeamAssignmentRules
{
	GENERATED_BODY()

	/** How players without a team are placed. ?TeamAssignment= */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams")
	EEnclaveTeamAssignment Assignment = EEnclaveTeamAssignment::Random;

	/**
	 * At match start, players keep the team they arrived with (from the last
	 * match, or from joining during warmup), then teams are evened out. Off:
	 * everyone is placed anew. ?KeepTeams=1|0
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams")
	bool bKeepTeams = true;

	/** Team sizes may differ by at most this much (at match start, and when a player reconnects). */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams", meta = (ClampMin = "1"))
	int32 MaxTeamSizeDifference = 1;

	/** Whether a player who leaves mid-match keeps their place. ?TeamSlots=Open|Reserved */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams")
	EEnclaveTeamSlots TeamSlots = EEnclaveTeamSlots::Open;

	/** With Reserved slots, how long a leaver's place is held, in seconds; 0 = the whole match. ?ReservedSlotHold= */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams", meta = (ClampMin = "0", EditCondition = "TeamSlots == EEnclaveTeamSlots::Reserved"))
	float ReservedSlotHoldSeconds = 0.0f;

	/** Applies the URL options above. Invalid values are logged and the current value is kept. */
	void ApplyOptions(const FString& Options);

	/** One-line summary for the log. */
	FString ToString() const;
};

/** Players placed together: a solo player is a group of one; a party (later) is one group. */
struct FEnclaveTeamGroup
{
	/** Players in the group. */
	int32 Size = 1;

	/** The team the group arrived with; NoTeam, or any value >= NumTeams, means none. */
	uint8 PreviousTeam = AEnclaveTeamPlayerState::NoTeam;

	/** Lower joined earlier (the PlayerId). Evening out moves the newest first. */
	int32 JoinOrder = 0;
};

/**
 * Team assignment at match start, as a pure function over groups of players.
 * It knows nothing about PlayerStates or the world: the team GameMode builds
 * the groups, calls AssignTeams and applies the result, and tests call it with
 * plain data. One pipeline covers a new server, a restart, or a mix:
 *
 *   1. Seed: with bKeepTeams, each group with a valid previous team keeps it.
 *   2. Place: the other groups go to the smallest team, largest groups first,
 *      as the assignment policy decides.
 *   3. Even out: while team sizes differ by more than MaxTeamSizeDifference,
 *      move a group from a biggest team to a smallest one, smallest groups and
 *      then the newest first, and only if the move narrows the gap. A group is
 *      never split: if no move helps, the result is the best it can do.
 */
namespace EnclaveTeamAssignment
{
	/** One team per group, index-aligned with Groups. NumTeams must be 1..254. */
	ENCLAVE_API TArray<uint8> AssignTeams(TConstArrayView<FEnclaveTeamGroup> Groups, const FEnclaveTeamAssignmentRules& Rules,
		int32 NumTeams, FRandomStream& Random);
}
