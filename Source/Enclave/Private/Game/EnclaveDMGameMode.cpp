// Copyright Lightcarrier Studios

#include "Game/EnclaveDMGameMode.h"

#include "Player/EnclaveMatchPlayerState.h"

void AEnclaveDMGameMode::ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim)
{
	// Only a kill of an enemy scores. IsHostileTo is false for a suicide and for
	// a spectator; Attacker is null for a death no player caused. The base has
	// already applied SuicideScore for those.
	if (!Attacker || !Attacker->IsHostileTo(Victim))
	{
		return;
	}

	// APlayerState's Score is replicated, and it is what the solo win check
	// reads, so the result can't disagree with the scoreboard.
	Attacker->AddScore(1);
}
