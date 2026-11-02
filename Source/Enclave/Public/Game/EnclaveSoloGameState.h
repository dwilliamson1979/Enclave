// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveMatchGameState.h"
#include "EnclaveSoloGameState.generated.h"

/**
 * Replicated state for free-for-all modes (used with AEnclaveSoloMatchGameMode):
 * the result is a single player. Each player's score is the engine's replicated
 * APlayerState::Score, so it needs no field here.
 *
 * A null WinningPlayer while the match state is WaitingPostMatch is a draw;
 * before that it means "no result yet".
 */
UCLASS()
class ENCLAVE_API AEnclaveSoloGameState : public AEnclaveMatchGameState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Match")
	APlayerState* GetWinningPlayer() const { return WinningPlayer; }

	/** nullptr = no winner (a new match, or a draw). Authority only. */
	void SetWinningPlayer(APlayerState* Player);

private:

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> WinningPlayer;
};
