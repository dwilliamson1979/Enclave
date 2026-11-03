// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameMode.h"
#include "EnclaveTeamMatchGameMode.generated.h"

/**
 * Base for every mode played in teams (TDM now; CTF, Control and others later).
 * It owns what all team modes share: the team count and team scores, team
 * assignment and balancing, and (next) friendly fire and the team win check.
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
	/** Sets up the team GameState as soon as it exists, so warmup already shows the teams. */
	virtual void InitGameState() override;

	/** Creates one zeroed score per team and clears the result, then lets the base start the match. */
	virtual void HandleMatchHasStarted() override;

	/**
	 * Every way a player arrives (login, reconnect, seamless travel) ends here.
	 * Gives the player a team before Super, which spawns the pawn if the match is
	 * in progress. A player who already has a valid team keeps it: on a
	 * reconnect, AGameMode::PostLogin has already restored their old PlayerState.
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * The team for a player who has none. Default: the team with the fewest
	 * players; a tie goes to the lowest team ID, so assignment is predictable.
	 */
	virtual uint8 ChooseTeamForPlayer(const AController* Player) const;

	/** Players on a team, not counting spectators or players who have left. */
	int32 CountPlayersOnTeam(uint8 TeamId) const;

	/** Teams in this mode. At most 254: team IDs are a uint8 and 255 is NoTeam. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams", meta = (ClampMin = "2", ClampMax = "254"))
	int32 NumTeams = 2;
};
