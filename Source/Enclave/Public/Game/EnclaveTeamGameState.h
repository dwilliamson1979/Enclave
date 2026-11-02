// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameState.h"
#include "Player/EnclavePlayerState.h"
#include "EnclaveTeamGameState.generated.h"

/**
 * Replicated state for team modes (used with AEnclaveTeamMatchGameMode): one
 * score per team and the winning team.
 *
 * Team IDs are sequential from 0, so TeamScores is indexed by team ID (a TMap
 * can't replicate). WinningTeam == NoTeam while the match state is
 * WaitingPostMatch is a draw; before that it means "no result yet".
 */
UCLASS()
class ENCLAVE_API AEnclaveTeamGameState : public AEnclaveMatchGameState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Starts a new match: NumTeams zeroed scores and no winner. Authority only. */
	void InitTeams(int32 NumTeams);

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetNumTeams() const { return TeamScores.Num(); }

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetTeamScore(uint8 TeamId) const;
	void  AddTeamScore(uint8 TeamId, int32 Delta);

	UFUNCTION(BlueprintPure, Category = "Match")
	uint8 GetWinningTeam() const { return WinningTeam; }

	/** NoTeam = no winner (a draw). Authority only. */
	void SetWinningTeam(uint8 TeamId);

private:

	UPROPERTY(Replicated)
	TArray<int32> TeamScores;

	UPROPERTY(Replicated)
	uint8 WinningTeam = AEnclavePlayerState::NoTeam;
};
