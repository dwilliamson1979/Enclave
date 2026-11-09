// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameMode.h"
#include "Game/EnclaveTeamAssignment.h"
#include "Game/EnclaveTeamRoster.h"
#include "EnclaveTeamMatchGameMode.generated.h"

/**
 * Base for every mode played in teams (TDM now; CTF, Control and others later).
 * It owns what all team modes share: the team count and team scores, team
 * assignment and balancing, team slots (Open or Reserved), and the team win check.
 * A mode's own rules, such as what scores, stay in the mode subclass.
 *
 * Teams are numbered 0..NumTeams-1, matching AEnclaveTeamGameState's TeamScores.
 * Team state is set up before calling Super in each override, because the base
 * (and the engine below it) spawns players, and spawning needs the teams.
 * Uses AEnclaveTeamGameState and AEnclaveTeamPlayerState.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveTeamMatchGameMode : public AEnclaveMatchGameMode
{
	GENERATED_BODY()

public:
	AEnclaveTeamMatchGameMode();

	int32 GetNumTeams() const { return NumTeams; }

protected:
	/**
	 * Applies the team URL options on top of the match settings:
	 * ?TeamAssignment=, ?KeepTeams=, ?TeamSlots=, ?ReservedSlotHold=,
	 * and ?FriendlyFire=1|0 / ?FriendlyFireScale=
	 * (0 to 1), which set MatchHitPolicy's Friendly row and scale.
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** Sets up the team GameState as soon as it exists, so warmup already shows the teams. */
	virtual void InitGameState() override;

	/**
	 * Creates one zeroed score per team and clears the result, assigns every
	 * player's team (AssignTeamsAtMatchStart), then lets the base start the match.
	 */
	virtual void HandleMatchHasStarted() override;

	/**
	 * With Reserved slots during the match, a player without a held place is
	 * refused ("Server full.") when connected players plus absent players'
	 * held places reach MaxPlayers. The engine counts only connected players
	 * (AGameSession::AtCapacity, GameSession.cpp:338-340), so without this a
	 * newcomer could take the connection an absent player needs to come back.
	 */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	/** With Reserved slots during the match, holds the leaving player's place on the roster. */
	virtual void Logout(AController* Exiting) override;

	/** Notes a reconnect (the engine restored the player's PlayerState) for HandleStartingNewPlayer. */
	virtual bool FindInactivePlayer(APlayerController* PC) override;

	/**
	 * Every way a player arrives (login, reconnect, seamless travel) ends here.
	 * Gives the player a team before Super, which spawns the pawn if the match is
	 * in progress.
	 *  - Before the match starts: a valid team is kept; match start reassigns anyway.
	 *  - After it starts, with Reserved slots: a player whose place is held
	 *    returns to that team, whatever team they carry (their old PlayerState
	 *    may have expired). Spectators lose their place.
	 *  - After it starts, otherwise: a reconnect returns to its old team if the
	 *    team sizes stay within MaxTeamSizeDifference. Anyone else, including a
	 *    seamless traveller who carries last match's team, is placed as a new
	 *    player. With Reserved slots, they join the roster.
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * Runs the team assignment pipeline (EnclaveTeamAssignment::AssignTeams) over
	 * every connected non-spectator, one group per player, and logs the teams.
	 * With Reserved slots, the result becomes the match's roster.
	 */
	void AssignTeamsAtMatchStart();

	/**
	 * The match ends when a team reaches the score limit or time runs out. Either
	 * way, the one team with the top score wins; a shared top score is a draw
	 * (WinningTeam = NoTeam). That covers two teams reaching the limit in the
	 * same frame: polling can't tell which got there first.
	 */
	virtual bool CheckWinCondition(bool bTimeLimitReached) override;

	/**
	 * The team for a player placed as new. Default: the team with the fewest
	 * players, not counting Player itself; ties are broken at random.
	 */
	virtual uint8 ChooseTeamForPlayer(const AController* Player) const;

	/**
	 * Players holding a place on a team, not counting spectators or
	 * ExcludedPlayer: connected players, plus, with Reserved slots, absent
	 * players whose place is still held.
	 */
	int32 CountPlayersOnTeam(uint8 TeamId, const APlayerState* ExcludedPlayer = nullptr) const;

	/** Teams in this mode. At most 254: team IDs are a uint8 and 255 is NoTeam. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams", meta = (ClampMin = "2", ClampMax = "254"))
	int32 NumTeams = 2;

	/** How teams are assigned at match start and kept balanced on reconnect. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams")
	FEnclaveTeamAssignmentRules TeamAssignmentRules;

private:
	bool UsesReservedSlots() const { return TeamAssignmentRules.TeamSlots == EEnclaveTeamSlots::Reserved; }

	/** A player's roster key: their unique net ID as a string, or empty when they have none (then no place is held). */
	static FString GetRosterKey(const FUniqueNetIdRepl& UniqueId);

	/**
	 * This match's roster, used with Reserved slots. Built at match start; a new
	 * GameMode (each match, after travel) starts with an empty one, so it never
	 * carries over: bKeepTeams does that.
	 */
	FEnclaveTeamRoster Roster;

	/**
	 * Set by FindInactivePlayer and read in the same call chain:
	 * AGameMode::PostLogin finds the inactive player, then AGameModeBase::PostLogin
	 * calls HandleStartingNewPlayer (GameMode.cpp:115-117, GameModeBase.cpp:1035).
	 */
	TWeakObjectPtr<APlayerController> ReconnectingPlayer;
};
