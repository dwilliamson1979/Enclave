// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameMode.h"
#include "EnclaveTeamMatchGameMode.generated.h"

/**
 * Base for every mode played in teams (TDM now; CTF, Control and others later).
 * It owns what all team modes share: the team count and team scores, and (next)
 * team assignment and balancing, then friendly fire. A mode's own rules, such as
 * what scores and how a team wins, stay in the mode subclass.
 *
 * Teams are numbered 0..NumTeams-1, matching AEnclaveTeamGameState's TeamScores.
 * Team state is set up before calling Super in each override, because the base
 * (and the engine below it) spawns players, and spawning needs the teams.
 * Uses AEnclaveTeamGameState.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveTeamMatchGameMode : public AEnclaveMatchGameMode
{
	GENERATED_BODY()

public:
	AEnclaveTeamMatchGameMode();

	int32 GetNumTeams() const { return NumTeams; }

protected:
	/** Creates one zeroed score per team and clears the result, then lets the base start the match. */
	virtual void HandleMatchHasStarted() override;

	/** Teams in this mode. At most 254: team IDs are a uint8 and 255 is NoTeam. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Teams", meta = (ClampMin = "2", ClampMax = "254"))
	int32 NumTeams = 2;
};
