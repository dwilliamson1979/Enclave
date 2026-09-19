// Copyright Lightcarrier Studios

#include "Game/EnclaveGameModeBase.h"

#include "Player/EnclavePlayerState.h"

AEnclaveGameModeBase::AEnclaveGameModeBase()
{
	PlayerStateClass = AEnclavePlayerState::StaticClass();
}
