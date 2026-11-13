// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameMode.h"
#include "Game/EnclaveMatchSettings.h"
#include "Game/EnclaveHitPolicy.h"
#include "Game/EnclaveSpawnSelection.h"
#include "Math/RandomStream.h"
#include "Combat/EnclaveDamageRequest.h"
#include "EnclaveMatchGameMode.generated.h"

class AEnclaveMatchGameState;
class AEnclaveMatchPlayerState;
class APlayerStart;
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
	 * Always false, so every spawn chooses a start (ChoosePlayerStart). The
	 * engine's version is true once the controller has a StartSpot, which is
	 * set at login and never updated, so every respawn would reuse the login
	 * start (GameModeBase.cpp:842-848, 1168-1176).
	 */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	/**
	 * Also true during warmup (WaitingToStart), so players can move and shoot
	 * while they wait. AGameMode's version is false until the match is in
	 * progress (GameMode.cpp:548-556); this skips that check and keeps the
	 * base one (not a spectator, world loaded; GameModeBase.cpp:1203-1211).
	 */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

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
	 * Warmup is over: resets stats, sets the time limit, removes every warmup
	 * pawn and then calls Super, which spawns every player fresh. Subclasses
	 * that add their own match state (results, team scores, teams) set it up
	 * before calling Super, so the fresh spawns see it.
	 */
	virtual void HandleMatchHasStarted() override;

	/**
	 * A player who arrives during warmup spawns right away. AGameMode spawns
	 * arrivals only once the match is in progress (GameMode.cpp:529-540).
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * Spawns the players who logged in before warmup began. A listen host (and
	 * every player kept over a seamless restart) is started before the world's
	 * BeginPlay (GameInstance.cpp:538 then 566), while the match is still
	 * EnteringMap, so HandleStartingNewPlayer had nothing to do for them;
	 * StartPlay moves to WaitingToStart only afterwards (GameMode.cpp:136-139).
	 */
	virtual void HandleMatchIsWaitingToStart() override;

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

	/** How spawn starts are chosen. See EnclaveSpawnSelection. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Spawning")
	FEnclaveSpawnRules SpawnRules;

	/**
	 * The start for a spawn, away from living enemies (EnclaveSpawnSelection).
	 * A "Play from Here" start in PIE is used for each player's first spawn of
	 * the match only, so respawns still test the real choice. Falls back to
	 * the engine's choice if no start is allowed for the player.
	 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/**
	 * Whether Player may spawn at Start. Always true here; a mode narrows it
	 * (a team mode: the player's team's starts).
	 */
	virtual bool IsStartAllowedFor(const APlayerStart& Start, const AController& Player) const { return true; }

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
	/**
	 * Server: credits the death and kill, scores it, announces it, and schedules
	 * the respawn. A warmup death only schedules the respawn: nothing scores.
	 */
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
	 * A fact like kills and deaths; MatchSettings.AssistScore is added to the
	 * assister's player score (never a team score).
	 */
	void AwardAssists(const APawn& Victim, const AEnclaveMatchPlayerState& VictimPlayerState,
		const AEnclaveMatchPlayerState* Killer, double Now) const;

	/** Respawns Victim's controller after MatchSettings.RespawnDelaySeconds. */
	void ScheduleRespawn(const APawn& Victim);

	/**
	 * Respawn timer callback: respawns the player if they are still here, it's
	 * still a phase with spawning (warmup or in progress), and they have no
	 * living pawn. The last check stops a warmup death's timer, firing after
	 * the match-start reset, from replacing the player's fresh pawn.
	 */
	void RespawnAfterDeath(TWeakObjectPtr<AController> Controller);

	/** Warmup or in progress: the phases where players spawn and respawn. */
	bool IsSpawningPhase() const { return GetMatchState() == MatchState::WaitingToStart || IsMatchInProgress(); }

	/** During warmup: a player with no pawn who may play (not a spectator) and may restart. */
	bool ShouldSpawnForWarmup(APlayerController* Player);

	/** Cancels a running warmup countdown, if any. */
	void CancelWarmup(AEnclaveMatchGameState& MatchGameState);

	/** Server time the warmup ends. Unset while there aren't enough players. */
	TOptional<double> WarmupEndServerTime;

	/** Picks among equally good starts. Seeded in InitGame. */
	FRandomStream SpawnRandom;

	/** Players who have had a pawn on this map (warmup included), for the PIE "Play from Here" start. */
	TSet<TWeakObjectPtr<AController>> SpawnedControllers;
};
