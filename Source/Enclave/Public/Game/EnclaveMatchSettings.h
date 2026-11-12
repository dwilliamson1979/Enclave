// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "EnclaveMatchSettings.generated.h"

/**
 * Settings shared by every match mode. Each mode sets its defaults in its
 * Blueprint (settings are data); a server can override any of them with a URL
 * option, e.g. MapName?ScoreLimit=3?Warmup=0. All times are in seconds.
 *
 * Defaults here are values every mode shares, or neutral (ScoreLimit 0) where
 * modes differ. A Blueprint saves only values that differ from its parent, so
 * a mode's value that happened to equal the struct default would not be
 * stored, and would change silently with it.
 *
 * Settings that only one mode uses (a CTF capture limit, for example) belong
 * on that mode's GameMode, not here.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveMatchSettings
{
	GENERATED_BODY()

	/**
	 * Score needed to win; the mode decides what scores (TDM 50, DM 25, set in
	 * each mode's Blueprint). 0 = no score limit. ?ScoreLimit=
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0"))
	int32 ScoreLimit = 0;

	/** 0 = no time limit. ?TimeLimit= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ForceUnits = "s"))
	int32 TimeLimitSeconds = 600;

	/** Delay between death and automatic respawn. ?RespawnDelay= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ForceUnits = "s"))
	int32 RespawnDelaySeconds = 3;

	/** Countdown once MinPlayersToStart are in. 0 = start immediately. ?Warmup= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ForceUnits = "s"))
	int32 WarmupSeconds = 10;

	/** Players needed before the warmup countdown begins. ?MinPlayers= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 2;

	/** How long the results show before the map restarts. ?PostMatch= */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ForceUnits = "s"))
	int32 PostMatchSeconds = 15;

	/**
	 * Added to the player's own score for a suicide (killed by yourself, or by
	 * no player), usually negative. Never touches a team score, so one player
	 * can't cost their team points on purpose. 0 = no penalty. ?SuicideScore=
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	int32 SuicideScore = 0;

	/**
	 * A death whose killing blow wasn't an enemy's (yourself, no player, a
	 * teammate) goes to the most recent enemy who damaged the victim within
	 * this many seconds, so a kill can't be denied by suicide or a fall.
	 * 0 = off: the killing blow alone decides. ?KillCreditWindow=
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ForceUnits = "s"))
	int32 KillCreditWindowSeconds = 5;

	/**
	 * An enemy's damage to the victim counts toward an assist if it landed
	 * within this many seconds before the death. Keep it at or under the damage
	 * history's MaxEntryAgeSeconds (30), which prunes older entries.
	 * 0 = no assists. ?AssistWindow=
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ForceUnits = "s"))
	int32 AssistWindowSeconds = 10;

	/**
	 * Damage an enemy must have dealt within AssistWindowSeconds for an assist,
	 * as a percent of the victim's max health. ?AssistDamagePercent=
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0", ClampMax = "100"))
	int32 AssistDamagePercent = 25;

	/**
	 * Added to the player's own score for each assist. Never touches a team
	 * score: the team already scored the kill. Counts toward the score limit
	 * like any score. 0 = assists are counted but pay nothing (to turn assists
	 * off entirely, set AssistWindowSeconds to 0). ?AssistScore=
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0"))
	int32 AssistScore = 0;

	/** Applies the URL options above. Invalid values are logged and the current value is kept. */
	void ApplyOptions(const FString& Options);

	/** One-line summary for the log. */
	FString ToString() const;
};
