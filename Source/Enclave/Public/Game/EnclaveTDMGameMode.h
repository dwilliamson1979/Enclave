// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveTeamMatchGameMode.h"
#include "EnclaveTDMGameMode.generated.h"

/**
 * 
 */
UCLASS()
class ENCLAVE_API AEnclaveTDMGameMode : public AEnclaveTeamMatchGameMode
{
	GENERATED_BODY()
	
protected:

	virtual void ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim) override;
	virtual bool CheckWinCondition(bool bTimeLimitReached) override;

};