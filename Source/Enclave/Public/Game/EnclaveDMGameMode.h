// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveSoloMatchGameMode.h"
#include "EnclaveDMGameMode.generated.h"

/**
 * Deathmatch (free-for-all): each kill of an enemy is +1 to the killer's score.
 * Everyone is an enemy, so every kill of another player counts. The win check
 * (first player to the score limit, or the leader at the time limit) comes from
 * AEnclaveSoloMatchGameMode, and the suicide penalty from the match base; this
 * class holds only what scores.
 */
UCLASS()
class ENCLAVE_API AEnclaveDMGameMode : public AEnclaveSoloMatchGameMode
{
	GENERATED_BODY()

protected:

	/** +1 player score for a kill of an enemy. Suicides are the base's (SuicideScore). */
	virtual void ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim) override;
};
