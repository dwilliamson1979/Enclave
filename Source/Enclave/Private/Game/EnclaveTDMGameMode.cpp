// Copyright Lightcarrier Studios


#include "Game/EnclaveTDMGameMode.h"
#include "Game/EnclaveTeamGameState.h"
#include "Player/EnclaveTeamPlayerState.h"

void AEnclaveTDMGameMode::ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim)
{
	//Could we pass in a bIsHostile parameter instead of doing this check? It would be more efficient since we already perform the check in the calling function. It takes away some of the control from this class, though.
	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();
	AEnclaveTeamPlayerState* AttackerTeamPlayer = Cast<AEnclaveTeamPlayerState>(Attacker);
	AEnclaveTeamPlayerState* VictimTeamPlayer = Cast<AEnclaveTeamPlayerState>(Victim);

	if(TeamGameState && AttackerTeamPlayer && VictimTeamPlayer)
	{
		// Only score a kill if the attacker is on a different team than the victim
		if(AttackerTeamPlayer->GetTeamId() != VictimTeamPlayer->GetTeamId())
		{
			TeamGameState->AddTeamScore(AttackerTeamPlayer->GetTeamId(), 1);
		}
	}
}

bool AEnclaveTDMGameMode::CheckWinCondition(bool bTimeLimitReached)
{
	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();

	if(!IsValid(TeamGameState))
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("CheckWinCondition called with invalid GameState."));
		return false;
	}



	return false;
}