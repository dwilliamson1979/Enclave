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

	if (PhaseEndServerTime != ServerTime)
	{
		PhaseEndServerTime = ServerTime;
		// The server gets no OnRep; call it so the listen host's HUD hears it.
		OnRep_PhaseEndServerTime();
	}
}

void AEnclaveMatchGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();

	OnPhaseChanged.Broadcast();
}

void AEnclaveMatchGameState::OnRep_PhaseEndServerTime()
{
	OnPhaseChanged.Broadcast();
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
