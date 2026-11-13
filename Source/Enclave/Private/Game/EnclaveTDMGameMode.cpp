// Copyright Lightcarrier Studios


#include "Game/EnclaveTDMGameMode.h"
#include "Game/EnclaveTeamGameState.h"
#include "Player/EnclaveTeamPlayerState.h"

void AEnclaveTDMGameMode::ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim)
{
	// Only a kill of an enemy scores. IsHostileTo is false for a suicide or a
	// team kill; Attacker is null for a death no player caused. None of these
	// costs the team a point (the base applies SuicideScore to the player).
	if (!Attacker || !Attacker->IsHostileTo(Victim))
	{
		return;
	}

	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();
	const AEnclaveTeamPlayerState* TeamAttacker = Cast<AEnclaveTeamPlayerState>(Attacker);
	if (!ensureMsgf(TeamGameState && TeamAttacker,
		TEXT("AEnclaveTDMGameMode needs an AEnclaveTeamGameState and AEnclaveTeamPlayerStates; the kill was not scored.")))
	{
		return;
	}

	// The team score decides the match; the player's score is for the
	// scoreboard. APlayerState's Score is already replicated (a float, exact
	// for whole numbers far beyond any match).
	TeamGameState->AddTeamScore(TeamAttacker->GetTeamId(), 1);
	Attacker->AddScore(1);
}