// Copyright Lightcarrier Studios

#include "Game/EnclaveSoloGameState.h"

#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveSoloGameState::SetWinningPlayer(APlayerState* Player)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("SetWinningPlayer called on non-authority GameState. This should only be called on the server."));
		return;
	}

	WinningPlayer = Player;
}

void AEnclaveSoloGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveSoloGameState, WinningPlayer);
}
