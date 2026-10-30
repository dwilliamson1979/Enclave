// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Player/EnclavePlayerState.h"
#include "EnclaveGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnclaveKillSignature, APlayerState*, Killer, APlayerState*, Victim);

/**
 * Replicated, match-wide facts every client can read: team scores, the match
 * end time and the result, plus the kill event that feeds the elimination feed.
 *
 * It stores and replicates only; the match GameMode makes every decision and
 * is the only writer (the setters are authority-only). Per-player facts such
 * as team, kills and deaths live on AEnclavePlayerState. The match phase is the
 * inherited AGameState::MatchState, so it is not duplicated here.
 *
 * Remaining time is computed on each client as
 * GetMatchEndServerTime() - GetServerWorldTimeSeconds(), so the timer replicates
 * once rather than every second.
 */
UCLASS()
class ENCLAVE_API AEnclaveGameState : public AGameState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitTeamScores(int32 NumTeams);

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetTeamScore(uint8 TeamId) const;
	void  AddTeamScore(uint8 TeamId, int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Match")
	double GetMatchEndServerTime() const { return MatchEndServerTime; }
	void SetMatchEndServerTime(double ServerTime);

	UFUNCTION(BlueprintPure, Category = "Match")
	uint8 GetWinningTeam() const { return WinningTeam; }
	UFUNCTION(BlueprintPure, Category = "Match")
	APlayerState* GetWinningPlayer() const { return WinningPlayer; }

	void SetWinner(uint8 TeamId, APlayerState* Player);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKill(APlayerState* Killer, APlayerState* Victim);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclaveKillSignature OnKill;

private:

	//We are assuming that team IDs are sequential and start at 0.
	UPROPERTY(Replicated)
	TArray<int32> TeamScores;

	UPROPERTY(Replicated)
	double MatchEndServerTime = 0.0;

	UPROPERTY(Replicated)
	uint8 WinningTeam = AEnclavePlayerState::NoTeam;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> WinningPlayer;
	
};