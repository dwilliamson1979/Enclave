// Copyright Lightcarrier Studios

#include "Game/EnclaveSoloMatchGameMode.h"

#include "Game/EnclaveSoloGameState.h"

AEnclaveSoloMatchGameMode::AEnclaveSoloMatchGameMode()
{
	GameStateClass = AEnclaveSoloGameState::StaticClass();
}

void AEnclaveSoloMatchGameMode::HandleMatchHasStarted()
{
	// Before Super: Super ends with the engine spawning every player, and they
	// must spawn into a match with no result yet.
	AEnclaveSoloGameState* SoloGameState = GetGameState<AEnclaveSoloGameState>();
	if (ensureMsgf(SoloGameState, TEXT("AEnclaveSoloMatchGameMode needs an AEnclaveSoloGameState; the result was not cleared.")))
	{
		SoloGameState->SetWinningPlayer(nullptr);
	}

	Super::HandleMatchHasStarted();
}
