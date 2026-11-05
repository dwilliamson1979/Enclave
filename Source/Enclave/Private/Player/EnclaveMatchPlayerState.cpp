// Copyright Lightcarrier Studios

#include "Player/EnclaveMatchPlayerState.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveMatchPlayerState::AddKill()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddKill called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Kills++;
}

void AEnclaveMatchPlayerState::AddDeath()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddDeath called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Deaths++;
}

void AEnclaveMatchPlayerState::ResetMatchStats()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("ResetMatchStats called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Kills = 0;
	Deaths = 0;
	SetScore(0.0f);
}

void AEnclaveMatchPlayerState::Reset()
{
	Super::Reset();

	ResetMatchStats();
}

void AEnclaveMatchPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// The target can be another class (seamless travel to a different mode):
	// copy only what it has.
	if(AEnclaveMatchPlayerState* MatchPlayerState = Cast<AEnclaveMatchPlayerState>(PlayerState))
	{
		MatchPlayerState->Kills = Kills;
		MatchPlayerState->Deaths = Deaths;
	}
}

void AEnclaveMatchPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveMatchPlayerState, Kills);
	DOREPLIFETIME(AEnclaveMatchPlayerState, Deaths);
}
