// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameMode.h"
#include "Game/EnclaveMatchSettings.h"
#include "Combat/EnclaveDamageRequest.h"
#include "EnclaveMatchGameMode.generated.h"

class AEnclaveMatchGameState;
class AEnclaveMatchPlayerState;
class UEnclaveAvatarLifecycleComponent;

/**
 * Match framework shared by every mode: phases (warmup, in progress, post-match),
 * the time limit, death handling, respawn and restart. It has no rules of its
 * own; each mode is a subclass that overrides the rule hooks.
 *
 * It holds only what every match has, so it knows nothing about teams or about
 * who won. Modes derive from AEnclaveSoloMatchGameMode (free-for-all) or
 * AEnclaveTeamMatchGameMode, which own their results. Uses AEnclaveMatchGameState
 * and AEnclaveMatchPlayerState.
 *
 * Phases come from AGameMode's match state machine: this class answers
 * ReadyToStartMatch / ReadyToEndMatch and reacts to the Handle* events.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveMatchGameMode : public AEnclaveGameMode
{
	GENERATED_BODY()

public:
	AEnclaveMatchGameMode();

	const FEnclaveMatchSettings& GetMatchSettings() const { return MatchSettings; }

	/** Binds the match's death handling to every pawn RestartPlayer spawns. */
	virtual void SetPlayerDefaults(APawn* Pawn) override;

protected:
	/** Applies match URL options on top of the defaults. See FEnclaveMatchSettings. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/**
	 * Polled every frame while WaitingToStart. Runs the warmup countdown once
	 * MinPlayersToStart are in, cancels it if the count drops below that, and
	 * returns true when the countdown has finished.
	 */
	virtual bool ReadyToStartMatch_Implementation() override;

	/**
	 * Warmup is over: sets the time limit and then calls Super, which spawns
	 * every player. Subclasses that add their own match state (results, team
	 * scores) set it up before calling Super.
	 */
	virtual void HandleMatchHasStarted() override;

	/**
	 * Polled every frame while InProgress, so a match has one way to end, whatever
	 * changed (a kill, an objective, a player leaving, added time). Works out
	 * whether the time limit is reached and asks the mode's CheckWinCondition.
	 */
	virtual bool ReadyToEndMatch_Implementation() override;

	/** Shows the post-match countdown, then restarts the map after PostMatchSeconds. */
	virtual void HandleMatchHasEnded() override;

	/** Defaults for this mode. Subclasses set their own in their constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	FEnclaveMatchSettings MatchSettings;

	/**
	 * Whether Attacker may damage Victim. Either may be null: no player behind
	 * the damage (the environment), or a target that isn't a player (a prop).
	 * Default for every mode: allowed when either is null or for self-damage;
	 * between two players, only an enemy (Attacker->IsHostileTo(Victim)), so
	 * teammates, neutral players and spectators are safe. Team modes add
	 * friendly fire (teammates, GetAttitudeTowards Friendly).
	 */
	UFUNCTION(BlueprintPure, Category = "Match")
	virtual bool CanDamage(const AEnclaveMatchPlayerState* Attacker, const AEnclaveMatchPlayerState* Victim) const;

	/**
	 * Awards score for a death. Called for every death while the match is in
	 * progress, including suicides (Attacker == Victim), team kills and deaths
	 * with no player responsible (Attacker null). Use Attacker->IsHostileTo(Victim)
	 * to tell a real kill apart. Victim is never null.
	 */
	virtual void ScoreKill(AEnclaveMatchPlayerState* Attacker, AEnclaveMatchPlayerState* Victim)
		PURE_VIRTUAL(AEnclaveMatchGameMode::ScoreKill, );

	/**
	 * Polled every frame while the match is in progress (see ReadyToEndMatch).
	 * Returns true when the match is over, and sets the result before returning:
	 * the winner once someone reaches MatchSettings.ScoreLimit (0 = no score
	 * limit); otherwise, if bTimeLimitReached, the leader, or no winner for a tie
	 * (a draw). Implemented once per branch (solo, team), because whose score
	 * counts is the branch's concept. Keep it cheap: it runs every frame.
	 */
	virtual bool CheckWinCondition(bool bTimeLimitReached)
		PURE_VIRTUAL(AEnclaveMatchGameMode::CheckWinCondition, return false;);

private:
	/** Server: credits the death and kill, scores it, announces it, and schedules the respawn. */
	UFUNCTION()
	void HandleAvatarDied(UEnclaveAvatarLifecycleComponent* LifecycleComp, FEnclaveDamageRequest DamageRequest);

	/** Respawn timer callback: respawns the player if they are still here and the match is still in progress. */
	void RespawnAfterDeath(TWeakObjectPtr<AController> Controller);

	/** Cancels a running warmup countdown, if any. */
	void CancelWarmup(AEnclaveMatchGameState& MatchGameState);

	/** Server time the warmup ends. Unset while there aren't enough players. */
	TOptional<double> WarmupEndServerTime;
};
