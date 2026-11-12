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
	EnclaveURLOptions::ApplyInt(Options, TEXT("KillCreditWindow"), 0, KillCreditWindowSeconds);
	EnclaveURLOptions::ApplyInt(Options, TEXT("AssistWindow"), 0, AssistWindowSeconds);
	EnclaveURLOptions::ApplyInt(Options, TEXT("AssistDamagePercent"), 0, AssistDamagePercent);
}

FString FEnclaveMatchSettings::ToString() const
{
	return FString::Printf(TEXT("ScoreLimit=%d TimeLimit=%ds RespawnDelay=%ds Warmup=%ds MinPlayers=%d PostMatch=%ds SuicideScore=%d KillCreditWindow=%ds AssistWindow=%ds AssistDamagePercent=%d"),
		ScoreLimit, TimeLimitSeconds, RespawnDelaySeconds, WarmupSeconds, MinPlayersToStart, PostMatchSeconds, SuicideScore,
		KillCreditWindowSeconds, AssistWindowSeconds, AssistDamagePercent);
}
