// Copyright Lightcarrier Studios

#include "Game/EnclaveMatchSettings.h"

#include "Game/EnclaveURLOptions.h"

void FEnclaveMatchSettings::ApplyOptions(const FString& Options)
{
	EnclaveURLOptions::ApplyInt(Options, TEXT("ScoreLimit"), 0, ScoreLimit);
	EnclaveURLOptions::ApplyInt(Options, TEXT("TimeLimit"), 0, TimeLimitSeconds);
	EnclaveURLOptions::ApplyInt(Options, TEXT("RespawnDelay"), 0, RespawnDelaySeconds);
	EnclaveURLOptions::ApplyInt(Options, TEXT("Warmup"), 0, WarmupSeconds);
	EnclaveURLOptions::ApplyInt(Options, TEXT("MinPlayers"), 1, MinPlayersToStart);
	EnclaveURLOptions::ApplyInt(Options, TEXT("PostMatch"), 0, PostMatchSeconds);
	EnclaveURLOptions::ApplyInt(Options, TEXT("SuicideScore"), MIN_int32, SuicideScore);
}

FString FEnclaveMatchSettings::ToString() const
{
	return FString::Printf(TEXT("ScoreLimit=%d TimeLimit=%ds RespawnDelay=%ds Warmup=%ds MinPlayers=%d PostMatch=%ds SuicideScore=%d"),
		ScoreLimit, TimeLimitSeconds, RespawnDelaySeconds, WarmupSeconds, MinPlayersToStart, PostMatchSeconds, SuicideScore);
}
