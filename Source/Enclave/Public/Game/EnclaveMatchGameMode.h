// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameMode.h"
#include "Game/EnclaveMatchSettings.h"
#include "Game/EnclaveHitPolicy.h"
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
 *
 * It is also the damage policy (IEnclaveDamagePolicyInterface): each phase has
 * a hit policy (warmup, match, post-match) that says what hits on players do.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveMatchGameMode : public AEnclaveGameMode, public IEnclaveDamagePolicyInterface
{
	GENERATED_BODY()

public:
	AEnclaveMatchGameMode();

	const FEnclaveMatchSettings& GetMatchSettings() const { return MatchSettings; }

	/** Binds the match's death handling to every pawn RestartPlayer spawns. */
	virtual void SetPlayerDefaults(APawn* Pawn) override;

	/**
	 * IEnclaveDamagePolicyInterface. Hits on non-players (dummies, destructibles)
	 * apply: their own code decides what damage does. Hits on players get the
	 * current phase's hit policy for the kind of hit: Environment (no attacker),
	 * Self, or by the attacker's attitude, Hostile or Friendly (scaled by
	 * FriendlyDamageScale). Hits on neutral players (spectators, no team) are
	 * ignored.
	 */
	virtual EEnclaveHitResponse ResolveDamage(const AActor* Target, FEnclaveDamageRequest& InOutRequest) const override;

	/** The hit policy of the current phase, set by OnMatchStateSet. */
	const FEnclaveHitPolicy& GetCurrentHitPolicy() const { return *CurrentHitPolicy; }

protected:
	/**
	 * Called on every phase change (AGameMode::SetMatchState, GameMode.cpp:327-339).
	 * Points CurrentHitPolicy at the new phase's policy. A mode that adds a
	 * phase (e.g. overtime) overrides this and sets its own policy.
	 */
	virtual void OnMatchStateSet() override;

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

	/** Hits during warmup: players see every hit, nobody gets hurt. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Hits")
	FEnclaveHitPolicy WarmupHitPolicy = FEnclaveHitPolicy::Simulated();

	/** Hits while the match is in progress: enemies, yourself and the environment hurt; teammates don't (team modes: ?FriendlyFire=). */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Hits")
	FEnclaveHitPolicy MatchHitPolicy;

	/** Hits after the match ends: like warmup, the result is already decided. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Hits")
	FEnclaveHitPolicy PostMatchHitPolicy = FEnclaveHitPolicy::Simulated();

	/**
	 * One of the policies above, for the current phase. A pointer, not a copy,
	 * so a change to a policy (e.g. ?FriendlyFire= in InitGame) is seen at once.
	 */
	const FEnclaveHitPolicy* CurrentHitPolicy = &WarmupHitPolicy;

	/**
	 * Awards score for a death. Called for every death while the match is in
	 * progress, including suicides (Attacker == Victim), team kills and deaths
	 * with no player responsible (Attacker null). Attacker is the credited
	 * player (see MatchSettings.KillCreditWindowSeconds): an enemy who damaged
	 * the victim just before a suicide, a fall or a team kill is the Attacker,
	 * and the death is a kill. Use Attacker->IsHostileTo(Victim)
	 * to tell a real kill apart. Victim is never null. (A team kill doesn't
	 * count as the victim's death. A suicide's MatchSettings.SuicideScore is
	 * already applied to the victim's player score.)
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

	/**
	 * The player credited with Victim's death: the killing blow's attacker if
	 * they are an enemy; otherwise the most recent enemy in the victim's damage
	 * history within MatchSettings.KillCreditWindowSeconds; otherwise the
	 * killing blow's attacker (yourself, a teammate, or null for no player).
	 */
	AEnclaveMatchPlayerState* ResolveKillCredit(const APawn& Victim, const AEnclaveMatchPlayerState& VictimPlayerState,
		AEnclaveMatchPlayerState* KillingBlowAttacker, double Now) const;

	/**
	 * Gives an assist to every enemy of the victim, other than Killer, whose
	 * damage to the victim within MatchSettings.AssistWindowSeconds adds up to
	 * at least MatchSettings.AssistDamagePercent of the victim's max health.
	 * A fact like kills and deaths; what an assist is worth is each mode's.
	 */
	void AwardAssists(const APawn& Victim, const AEnclaveMatchPlayerState& VictimPlayerState,
		const AEnclaveMatchPlayerState* Killer, double Now) const;

	/** Respawn timer callback: respawns the player if they are still here and the match is still in progress. */
	void RespawnAfterDeath(TWeakObjectPtr<AController> Controller);

	/** Cancels a running warmup countdown, if any. */
	void CancelWarmup(AEnclaveMatchGameState& MatchGameState);

	/** Server time the warmup ends. Unset while there aren't enough players. */
	TOptional<double> WarmupEndServerTime;
};
