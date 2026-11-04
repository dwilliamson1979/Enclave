// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveTeamMatchGameMode.h"
#include "EnclaveTDMGameMode.generated.h"

/**
 * Team Deathmatch: each kill of an enemy is +1 for the killer's team and +1 to
 * the killer's own score (for the scoreboard). Teams,
 * balancing, friendly fire and the win check (first team to the score limit,
 * or the leader at the time limit) come from AEnclaveTeamMatchGameMode; this
 * class holds only what scores.
 */
UCLASS()
class ENCLAVE_API AEnclaveTDMGameMode : public AEnclaveTeamMatchGameMode
{
	GENERATED_BODY()

protected:

	/** +1 team and player score for a kill of an enemy; nothing for suicides, team kills or deaths with no attacker. */
	virtual void ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim) override;

};