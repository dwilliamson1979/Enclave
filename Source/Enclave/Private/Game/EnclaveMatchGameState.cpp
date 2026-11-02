// Copyright Lightcarrier Studios

#include "Game/EnclaveMatchGameState.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveMatchGameState::SetPhaseEndServerTime(double ServerTime)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("SetPhaseEndServerTime called on non-authority GameState. This should only be called on the server."));
		return;
	}

	PhaseEndServerTime = ServerTime;
}

void AEnclaveMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveMatchGameState, PhaseEndServerTime);
}

void AEnclaveMatchGameState::MulticastKill_Implementation(APlayerState* Killer, APlayerState* Victim)
{
	OnKill.Broadcast(Killer, Victim);
}
