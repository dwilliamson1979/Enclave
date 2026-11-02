// Copyright Lightcarrier Studios

#include "Game/EnclaveTeamMatchGameMode.h"

#include "Game/EnclaveTeamGameState.h"

AEnclaveTeamMatchGameMode::AEnclaveTeamMatchGameMode()
{
	GameStateClass = AEnclaveTeamGameState::StaticClass();
}

void AEnclaveTeamMatchGameMode::HandleMatchHasStarted()
{
	// Before Super: Super ends with the engine spawning every player, and they
	// must spawn into a match whose team state is already set up.
	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();
	if (ensureMsgf(TeamGameState, TEXT("AEnclaveTeamMatchGameMode needs an AEnclaveTeamGameState; teams were not set up.")))
	{
		TeamGameState->InitTeams(NumTeams);
	}

	Super::HandleMatchHasStarted();
}
