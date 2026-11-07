// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameMode.h"
#include "Game/EnclaveTeamAssignment.h"
#include "EnclaveTeamMatchGameMode.generated.h"

/**
 * Base for every mode played in teams (TDM now; CTF, Control and others later).
 * It owns what all team modes share: the team count and team scores, team
 * assignment and balancing, the team win check, and (next) friendly fire.
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
	 * ?TeamAssignment=, ?KeepTeams=, and ?FriendlyFire=1|0 / ?FriendlyFireScale=
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

	/** Notes a reconnect (the engine restored the player's PlayerState) for HandleStartingNewPlayer. */
	virtual bool FindInactivePlayer(APlayerController* PC) override;

	/**
	 * Every way a player arrives (login, reconnect, seamless travel) ends here.
	 * Gives the player a team before Super, which spawns the pawn if the match is
	 * in progress.
	 *  - Before the match starts: a valid team is kept; match start reassigns anyway.
	 *  - After it starts: a reconnect returns to its old team if the team sizes
	 *    stay within MaxTeamSizeDifference. Anyone else, including a seamless
	 *    traveller who carries last match's team, is placed as a new player.
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * Runs the team assignment pipeline (EnclaveTeamAssignment::AssignTeams) over
	 * every connected non-spectator, one group per player, and logs the teams.
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
	 * Connected players on a team, not counting spectators or ExcludedPlayer.
	 * Players who left don't count: team slots are open (U3 moment 4).
	 */
	int32 CountPlayersOnTeam(uint8 TeamId, const APlayerState* ExcludedPlayer = nullptr) const;

	/** Teams in this mode. At most 254: team IDs are a uint8 and 255 is NoTeam. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams", meta = (ClampMin = "2", ClampMax = "254"))
	int32 NumTeams = 2;

	/** How teams are assigned at match start and kept balanced on reconnect. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams")
	FEnclaveTeamAssignmentRules TeamAssignmentRules;

private:
	/**
	 * Set by FindInactivePlayer and read in the same call chain:
	 * AGameMode::PostLogin finds the inactive player, then AGameModeBase::PostLogin
	 * calls HandleStartingNewPlayer (GameMode.cpp:115-117, GameModeBase.cpp:1035).
	 */
	TWeakObjectPtr<APlayerController> ReconnectingPlayer;
};
