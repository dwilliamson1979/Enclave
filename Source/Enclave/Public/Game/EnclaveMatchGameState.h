// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameState.h"
#include "EnclaveMatchGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnclaveKillSignature, APlayerState*, Killer, APlayerState*, Victim);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnclavePhaseChangedSignature);

/**
 * Replicated facts every match has, whatever the mode: when the current phase
 * ends, and the kill event that feeds the elimination feed. Used with
 * AEnclaveMatchGameMode; the solo and team GameStates add their own results.
 *
 * It stores and replicates only; the match GameMode makes every decision and
 * is the only writer (the setters are authority-only). Per-player facts such
 * as kills, deaths and team live on the PlayerState. The match phase is the
 * inherited AGameState::MatchState, so it is not duplicated here.
 *
 * PhaseEndServerTime is when the current phase ends, on the server clock: the
 * warmup countdown during WaitingToStart, the time limit during InProgress, and
 * 0 when the phase has no end (not enough players yet, or no time limit). The
 * HUD reads MatchState to choose its label and computes the remaining time as
 * GetPhaseEndServerTime() - GetServerWorldTimeSeconds(), so the timer
 * replicates once per phase rather than every second.
 *
 * OnPhaseChanged fires when either MatchState or PhaseEndServerTime changes,
 * on clients (OnReps) and on the server (SetMatchState calls OnRep_MatchState
 * itself, GameState.cpp; SetPhaseEndServerTime does the same), so a listen
 * host's HUD hears it too. The two can arrive in either order; read both.
 */
UCLASS()
class ENCLAVE_API AEnclaveMatchGameState : public AEnclaveGameState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Match")
	double GetPhaseEndServerTime() const { return PhaseEndServerTime; }
	void SetPhaseEndServerTime(double ServerTime);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKill(APlayerState* Killer, APlayerState* Victim);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclaveKillSignature OnKill;

	/** MatchState or PhaseEndServerTime changed. */
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclavePhaseChangedSignature OnPhaseChanged;

protected:

	virtual void OnRep_MatchState() override;

private:

	UFUNCTION()
	void OnRep_PhaseEndServerTime();

	UPROPERTY(ReplicatedUsing = OnRep_PhaseEndServerTime)
	double PhaseEndServerTime = 0.0;
};
