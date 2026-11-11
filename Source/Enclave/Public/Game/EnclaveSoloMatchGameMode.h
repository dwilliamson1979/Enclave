// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameMode.h"
#include "EnclaveSoloMatchGameMode.generated.h"

/**
 * Base for every free-for-all mode (Deathmatch now; Gun Game and others later).
 * Players have no team (they keep the match base's AEnclaveMatchPlayerState),
 * so everyone is an enemy, and the result is a single player. It owns what all
 * free-for-all modes share: the individual result (and, with the win check,
 * "a player reaches the score limit, or the top player wins at the time
 * limit"). A mode's own rules, such as what scores, stay in the mode subclass.
 * Uses AEnclaveSoloGameState.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveSoloMatchGameMode : public AEnclaveMatchGameMode
{
	GENERATED_BODY()

public:
	AEnclaveSoloMatchGameMode();

protected:
	/** Clears the result, then lets the base start the match. */
	virtual void HandleMatchHasStarted() override;

	virtual bool CheckWinCondition(bool bTimeLimitReached) override;
};
