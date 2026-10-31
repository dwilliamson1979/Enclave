// Copyright Lightcarrier Studios

#include "Game/EnclaveMatchGameMode.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MatchSettings.ApplyOptions(Options);
	UE_LOG(LogEnclaveMatch, Log, TEXT("Match settings: %s"), *MatchSettings.ToString());
}

bool AEnclaveMatchGameMode::ReadyToStartMatch_Implementation()
{
	if (!Super::ReadyToStartMatch_Implementation())
	{
		return false;
	}

	// Simple check for now; the warmup countdown comes next.
	return GetNumPlayers() >= MatchSettings.MinPlayersToStart;
}
