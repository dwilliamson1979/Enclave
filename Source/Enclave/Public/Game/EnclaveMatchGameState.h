// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameState.h"
#include "EnclaveMatchGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnclaveKillSignature, APlayerState*, Killer, APlayerState*, Victim);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnclavePhaseChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnclaveScoreLimitChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnclavePlayersChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnclavePlayerScoreChangedSignature, APlayerState*, PlayerState);

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
 *
 * ScoreLimit is the match's score limit (0 = none), copied from the match
 * settings so clients can show it; every mode has one. It is set once per
 * match but still has an OnRep and OnScoreLimitChanged: the HUD may see this
 * GameState before its first properties arrive.
 *
 * OnPlayersChanged fires when a PlayerState joins or leaves PlayerArray, on
 * every machine: the engine registers each PlayerState through the virtual
 * AddPlayerState and RemovePlayerState (PlayerState.cpp: PostInitializeComponents,
 * Destroyed, OnRep_bIsInactive), which this class overrides.
 * OnPlayerScoreChanged fires when any player's score changes; the match
 * PlayerState reports it (NotifyPlayerScoreChanged), so a HUD that ranks
 * players binds here once instead of to every PlayerState.
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

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetScoreLimit() const { return ScoreLimit; }
	/** Authority only. */
	void SetScoreLimit(int32 NewScoreLimit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKill(APlayerState* Killer, APlayerState* Victim);

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclaveKillSignature OnKill;

	/** MatchState or PhaseEndServerTime changed. */
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclavePhaseChangedSignature OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclaveScoreLimitChangedSignature OnScoreLimitChanged;

	/** A PlayerState joined or left PlayerArray. */
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclavePlayersChangedSignature OnPlayersChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclavePlayerScoreChangedSignature OnPlayerScoreChanged;

	/** Called by AEnclaveMatchPlayerState when its score changes, on every machine. */
	void NotifyPlayerScoreChanged(APlayerState* PlayerState);

	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

protected:

	virtual void OnRep_MatchState() override;

private:

	UFUNCTION()
	void OnRep_PhaseEndServerTime();

	UPROPERTY(ReplicatedUsing = OnRep_PhaseEndServerTime)
	double PhaseEndServerTime = 0.0;

	UFUNCTION()
	void OnRep_ScoreLimit();

	UPROPERTY(ReplicatedUsing = OnRep_ScoreLimit)
	int32 ScoreLimit = 0;
};
