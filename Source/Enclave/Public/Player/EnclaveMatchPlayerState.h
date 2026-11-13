// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Player/EnclavePlayerState.h"
#include "EnclaveMatchPlayerState.generated.h"

/**
 * Per-player facts every match has: kills and deaths. Used with
 * AEnclaveMatchGameMode; free-for-all modes use it directly, and team modes
 * use AEnclaveTeamPlayerState. The player's score is the engine's replicated
 * APlayerState::Score.
 *
 * The match GameMode is the only writer (the setters are authority-only).
 * Change the score only through AddScore (or ResetMatchStats), never
 * APlayerState::SetScore: SetScore is not virtual (PlayerState.h), so this
 * class can't see it, and every change must reach the GameState's
 * OnPlayerScoreChanged. Clients report it from OnRep_Score.
 */
UCLASS()
class ENCLAVE_API AEnclaveMatchPlayerState : public AEnclavePlayerState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void AddKill();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetKills() const { return Kills; }

	void AddAssist();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetAssists() const { return Assists; }

	void AddDeath();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetDeaths() const { return Deaths; }

	/** Adds Points (negative to take away) to the score. Authority only. */
	void AddScore(int32 Points);

	virtual void OnRep_Score() override;

	/** Zeroes this match's stats: kills, deaths and score. Authority only. */
	void ResetMatchStats();

	/** Engine reset (ResetLevel, and before a seamless travel copies the PlayerState): clears the match stats. */
	virtual void Reset() override;

protected:

	virtual void CopyProperties(APlayerState* PlayerState) override;

private:

	/** Sets the score and reports it, as OnRep_Score does on clients. */
	void SetMatchScore(float NewScore);

	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Assists = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;
};
