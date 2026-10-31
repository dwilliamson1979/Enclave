// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "EnclaveMatchSettings.generated.h"

/**
 * Settings shared by every match mode. Each mode sets its defaults (in its
 * constructor or Blueprint); a server can override any of them with a URL
 * option, e.g. MapName?ScoreLimit=3?Warmup=0. All times are in seconds.
 *
 * Settings that only one mode uses (a CTF capture limit, for example) belong
 * on that mode's GameMode, not here.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveMatchSettings
{
	GENERATED_BODY()

	/** Kills (DM) or team kills (TDM) needed to win. 0 = no score limit. ?ScoreLimit= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0"))
	int32 ScoreLimit = 50;

	/** 0 = no time limit. ?TimeLimit= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	int32 TimeLimitSeconds = 600;

	/** Delay between death and automatic respawn. ?RespawnDelay= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	int32 RespawnDelaySeconds = 3;

	/** Countdown once MinPlayersToStart are in. 0 = start immediately. ?Warmup= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	int32 WarmupSeconds = 10;

	/** Players needed before the warmup countdown begins. ?MinPlayers= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 2;

	/** How long the results show before the map restarts. ?PostMatch= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", Units = "s"))
	int32 PostMatchSeconds = 15;

	/** Applies the URL options above. Invalid values are logged and the current value is kept. */
	void ApplyOptions(const FString& Options);

	/** One-line summary for the log. */
	FString ToString() const;
};
