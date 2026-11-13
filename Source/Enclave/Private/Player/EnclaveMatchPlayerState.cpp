// Copyright Lightcarrier Studios

#include "Player/EnclaveMatchPlayerState.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"
#include "Game/EnclaveMatchGameState.h"

void AEnclaveMatchPlayerState::AddKill()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddKill called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Kills++;
}

void AEnclaveMatchPlayerState::AddAssist()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddAssist called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Assists++;
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

void AEnclaveMatchPlayerState::AddScore(int32 Points)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddScore called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	if (Points != 0)
	{
		SetMatchScore(GetScore() + static_cast<float>(Points));
	}
}

void AEnclaveMatchPlayerState::SetMatchScore(float NewScore)
{
	if (GetScore() != NewScore)
	{
		SetScore(NewScore);
		// The server gets no OnRep; call it so the listen host's HUD hears it.
		OnRep_Score();
	}
}

void AEnclaveMatchPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	// No GameState yet (it replicates separately): nothing is listening, and
	// the HUD reads every score when the GameState arrives.
	const UWorld* World = GetWorld();
	if (AEnclaveMatchGameState* GameState = World ? World->GetGameState<AEnclaveMatchGameState>() : nullptr)
	{
		GameState->NotifyPlayerScoreChanged(this);
	}
}

void AEnclaveMatchPlayerState::ResetMatchStats()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("ResetMatchStats called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Kills = 0;
	Assists = 0;
	Deaths = 0;
	SetMatchScore(0.0f);
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
		MatchPlayerState->Assists = Assists;
		MatchPlayerState->Deaths = Deaths;
	}
}

void AEnclaveMatchPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveMatchPlayerState, Kills);
	DOREPLIFETIME(AEnclaveMatchPlayerState, Assists);
	DOREPLIFETIME(AEnclaveMatchPlayerState, Deaths);
}
