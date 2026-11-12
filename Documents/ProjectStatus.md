# Enclave Project Status

Last updated 2026-11-12, after U6. Work happens on
`main` with a short branch per piece of work.

Enclave is a UE 5.8 C++ multiplayer FPS. This is the one-page overview: what
exists, what is in progress, and what comes next. Details live in the linked
docs. Working knowledge (build, test, gotchas) is in [DevNotes.md](DevNotes.md).

**Start here in a new session.** Read this file, then the plan of the
milestone in progress.

## Done

Only milestone 3 was named as a milestone when it was built. The earlier work is
grouped here by area, oldest first.

| Area | What exists | Key commits |
|---|---|---|
| Course foundation | Project from the FPS course (sections 1-2); Enclave Input Framework 2.0; movement and view components | `84bc97f`..`979e7e0` |
| Items, inventory, equipment | Replicated inventory and starting loadout; slot-oriented equipment with holster, stow and persistent items; wieldable cycling; data-driven item definitions | `9025888`, `17ec1f6`, `c18b038` |
| GAS | ASC on the PlayerState (survives respawn); Fire and Reload abilities granted by items; client-predicted magazine ammo; gameplay cues; item-granted hold focus (aim) | `e0644af`, `981c5c7`, `8d21fd5` |
| Avatar presentation | First- and third-person animation layer interfaces linked per item; data-driven item presentation choreography (draw, reload) | `36cf6dd`, `9dc866d`, `4c56864` |
| Health, damage, lifecycle | GAS-backed health; one damage entry point (`UEnclaveDamageStatics::ApplyDamage`); alive/dead avatar lifecycle with `OnAvatarDied`; avatar-scoped effects | `500eebd`, `13e1c65`, `8e3ccb7` |
| Hit registration and SSR (milestone 3) | Validated client shot intent; server-side rewind of capsules with ServerAnchor timing; dedicated-server validation, abuse tests and a scripted test range | `b2ab923`, `9e81e45` |
| Combat refactors after M3 | SSR settings in `UEnclaveCombatPolicyComponent`; `UEnclaveHitTargetComponent`; lifecycle-to-history event | `1a990e7`, `2c59604`, `3f397b9` |
| Focus (ADS) transition | One eased weight drives FOV, look sensitivity and sight alignment (`EnclaveFocusBlend`, `UEnclaveViewComponent`); per-profile `EnterDuration`, `ExitDuration` and `TransitionEasing` (the exit mirrors the entry); a reversal mid-blend continues from the current view and takes only the time spent. Test: `Enclave.Focus.Blend` | branch `focus-blend` |

SSR docs: [SSR_Walkthrough.md](SSR_Walkthrough.md) (start here),
[SSR_M3_NetworkValidation.md](SSR_M3_NetworkValidation.md) (results),
[SSR_D11D3D_TimingPolicy.md](SSR_D11D3D_TimingPolicy.md) (the timing decision).
The other `SSR_*` and `D11D3_*` docs record earlier stages and are kept for
history only.

## In progress: Milestone 4, the match framework

Plan: [M4_MatchFramework_Plan.md](M4_MatchFramework_Plan.md). Goal: a 4v4
dedicated-server match (TDM and DM) that runs from start through restart three
times in a row with clean logs.

| Item | What | Status |
|---|---|---|
| U1 | PlayerState kills, deaths, team | Done; split into match and team PlayerStates in C1 |
| U2 | GameState scores, phase end, winner, kill event | Done; split into match, solo and team GameStates in C1 |
| C1 | Match GameMode skeleton | Done (six moments, below); its hooks `ScoreKill` and `CheckWinCondition` are implemented in U3 and U4 |
| U3 | Team base rules, then TDM | Done (six moments, below) |
| U4 | Solo base rules, then DM | Done (three moments, below) |
| U6 | Kill credit: damage history, last-damager credit, later assists | Done (three moments, below) |
| C2 | Spawn selection | Done (three steps, below) |
| C3 | Damage policy: `ApplyDamage` asks the game mode's `ResolveDamage` (hit policy per phase) | Done with U3 moment 6 |
| C4 | HUD C++ base | Not started |
| U5 | HUD layouts (UMG) | Not started |
| C5 | Team colors | Not started |
| C6 | Graybox arena map | First pass generated: `TDM_TwinBases`, `DM_Pinwheel` (below) |
| C7 | Tests and match smoke test | Not started (unit tests so far: `Enclave.Match.Settings`, `.URLOptions`, `.TeamAssignment.*`, `.TeamRoster`, `.SpawnSelection`) |

C1 is built in six moments, with the user sketching each:

1. Server startup (`InitGame`, URL options). Done, `23314e4`.
2. Warmup countdown (`ReadyToStartMatch`). Done, `d9cf7cd`.
3. Match start (`HandleMatchHasStarted`), plus the split into solo and team
   modes. Done, `3d70607`.
4. A player joins (team assignment in `HandleStartingNewPlayer`), plus the
   PlayerState split. Done, `a952971`.
5. A death (kill credit, `ScoreKill`, kill event, respawn through
   `AEnclaveGameMode::RespawnPlayer`). Done, `5bfe731`.
6. Match end and restart (`ReadyToEndMatch` works out the time limit and polls
   the mode's `CheckWinCondition`; `HandleMatchHasEnded` shows the post-match
   countdown, then `RestartGame`). Done.

Decided after moment 6: a match with no score limit and no time limit logs a
warning in `InitGame` but is allowed. Freezing players at the end of a match is
done with C4's end screen (see the plan's C4 row).

U3 is built in six moments (balancing on join was already done in C1 moment 4;
moments 3 to 5 were added after moment 2):

1. The team win check (`AEnclaveTeamMatchGameMode::CheckWinCondition`): the
   match ends at the score limit or the time limit, the single top team wins,
   and a shared top score is a draw. Also TDM's `ScoreKill`
   (`AEnclaveTDMGameMode`): a kill of an enemy is +1 team score and +1 player
   score. Done.
2. `BP_TDMGameMode` (`?game=TDM` alias) and the first real matches on
   `EnclaveMap`, dedicated server with 2 clients: two score-limit wins and a
   time-limit draw, each followed by a restart that kept the URL options.
   The match logs every death with the players' stats. Done.
3. Seamless restart (added after moment 2): matches restart with seamless
   travel (`bUseSeamlessTravel` on `AEnclaveMatchGameMode`; no transition map,
   the engine uses an empty dummy world), so clients stay connected and each
   PlayerState is copied into the next match: players keep their team. Match
   stats are reset for every player at match start (`ResetMatchStats`). Verified
   with 2 clients over several restarts: about 0.2 s per restart, teams kept,
   stats from 0, combat works after the restart. Done.
4. Team assignment at match start, one pipeline for a new server, a restart,
   or a mix: build groups (a solo player is a group of one; a party later is
   one group), keep each group's previous team if `bKeepTeams`, place the rest
   with the assignment policy (`Random`; `SkillBalance` when ratings exist),
   then even out to `MaxTeamSizeDifference` (smallest groups, then the newest,
   move first; a group is never split, so with parties the result is best
   effort; splitting a party as a last resort comes with parties).
   `EnclaveTeamAssignment::AssignTeams` (`Game/EnclaveTeamAssignment.h`) with
   `FEnclaveTeamAssignmentRules` on the team mode and `?TeamAssignment=` /
   `?KeepTeams=`. Team slots are Open: only connected players count toward
   team sizes, a player who arrives after the start is placed as new whatever
   team they carry (smallest team, ties at random), and a reconnect to the
   same match returns to its old team if the sizes stay within
   `MaxTeamSizeDifference`, otherwise it is placed as new. Mid-match
   rebalance is a later, separate item (backfill to the smallest team covers
   the common case). Tests: `Enclave.Match.TeamAssignment.*`. Verified with 2
   clients over several restarts with `KeepTeams=1` (teams kept) and
   `KeepTeams=0` (teams redealt), and with 4 clients mid-match: a reconnect
   kept its team twice, a late joiner went to the smaller team, and a
   reconnect whose team had filled went to the other team with its stats.
   Done.
5. Reserved team slots: `TeamSlots` (`Open`, `Reserved`) and
   `ReservedSlotHoldSeconds` (0 = the whole match) in
   `FEnclaveTeamAssignmentRules`, a default in each mode's Blueprint,
   overridden by `?TeamSlots=` and `?ReservedSlotHold=`. Under Reserved, the
   match-start assignment becomes this match's roster (`FEnclaveTeamRoster`,
   keyed by unique net ID, because the engine's `InactivePlayerArray` expires
   after 300 s and holds at most 16). A player who leaves keeps their place for
   the hold time, team sizes count held places, a returning player whose place
   is held gets their team back, late joiners join the roster, and a player who
   returns as a spectator gives up their place. `PreLogin` refuses a newcomer
   ("Server full.") when connected players plus held places reach
   `MaxPlayers`, because the engine counts only connected players. Tests:
   `Enclave.Match.TeamRoster`. Verified on a dedicated server with
   `?TeamSlots=Reserved?MaxPlayers=3` and 3 clients: a player disconnected,
   a 4th client was refused, and the player reconnected to their team. Done.
6. Friendly fire and the hit policy (built before moment 5), with C3. Players
   have an attitude to each other (`GetAttitudeTowards`: Hostile, Friendly,
   Neutral; a spectator or a `NoTeam` player is Neutral). Each match phase has
   a hit policy (`FEnclaveHitPolicy`: a response per kind of hit on a player,
   Hostile, Friendly, Self, Environment; each is Ignore, Simulate or Apply,
   plus `FriendlyDamageScale`). Warmup and post-match simulate every hit (hit
   markers, no damage, Fortnite style); the match applies them, except
   teammates (`?FriendlyFire=1|0`, `?FriendlyFireScale=`). `OnMatchStateSet`
   points `CurrentHitPolicy` at the phase's policy. `AEnclaveMatchGameMode`
   implements `IEnclaveDamagePolicyInterface::ResolveDamage`, which
   `UEnclaveDamageStatics::ApplyDamage` asks for every hit (a game mode
   without it applies all damage). Hits on non-players (dummies,
   destructibles) always apply: their own code decides what damage does;
   hits on neutral players are ignored. The damage request carries the
   attacker's PlayerState (`SourcePlayerState`), so a kill still counts when
   the attacker's pawn has died. A team kill doesn't count as the victim's
   death. Verified on a dedicated server with 3 clients (two teammates):
   `?FriendlyFire=1?FriendlyFireScale=0.5` halved teammate damage and a team
   kill added no death and no kill; `?FriendlyFire=0` ignored teammate hits;
   enemy kills counted; post-match hits were simulated. Warmup is not yet
   testable (see known issues). The server log shows each hit's response with
   `-LogCmds="LogEnclaveCombat Verbose"`. Done.

U4 is built in three moments:

1. The solo win check (`AEnclaveSoloMatchGameMode::CheckWinCondition`): the
   match ends at the score limit or the time limit, the single top player
   wins, a shared top score is a draw, spectators take no part, and a lone
   player wins even on 0. Done.
2. `SuicideScore` and DM's `ScoreKill`. A suicide (killed by yourself, or by
   no player) changes the victim's player score by `SuicideScore`
   (`FEnclaveMatchSettings`, neutral 0, `?SuicideScore=`; -1 in both mode
   Blueprints), applied by the match base before `ScoreKill`; it never
   touches a team score, so a saboteur can't cost a team points.
   `AEnclaveDMGameMode::ScoreKill`: +1 for a kill of an enemy. The penalty
   alone doesn't stop kill denial by suicide (in TDM it touches only the
   scoreboard); U6's last-damager credit does. Done, `9adfc0e`.
3. `BP_DMGameMode` (`?game=DM` alias) and real DM matches, in PIE with 3
   players. Two pieces were added for it: the `EnclaveDebugDamage <Amount>
   [Self|Environment]` console command (development builds; through
   `ApplyDamage` and the hit policy), and death below the kill Z
   (`AEnclaveCharacter::FellOutOfWorld` kills the avatar as an environment
   death through `UEnclaveAvatarLifecycleComponent::Kill`, bypassing the hit
   policy, then removes the body; before, the engine only destroyed the pawn,
   so there was no death and no respawn). Verified: self and environment
   deaths each cost 1 point and respawn; a fall off the map is a death with no
   attacker and respawns; a score-limit win at 3; a time-limit draw with 3
   players on 0; a restart starts everyone at 0; negative scores work. The
   match time settings now use `ForceUnits = "s"`, so the editor shows seconds
   (with `Units`, 600 s showed as 10 min and typed values were read as
   minutes). Done.

Decided in U3: kills and deaths are facts recorded by the match base; score is
each mode's judgement (TDM: +1 per kill; objective modes may pay little or
nothing per kill). Mode-only stats go on a mode PlayerState. Names are short:
`AEnclaveTDMGameMode`, `AEnclaveDMGameMode`. Mode defaults that differ between
modes (the score limit: TDM 50, DM 25) are set in each mode's Blueprint; the
settings struct keeps neutral defaults (`ScoreLimit` 0), because a Blueprint
saves only values that differ from its parent.

U6 is built in three moments (branch `u6-kill-credit`):

1. The damage history. `UEnclaveHealthComponent::NotifyHealthChanged`
   broadcasts `OnDamageTaken(FEnclaveDamageTaken)` (`Combat/EnclaveDamageTaken.h`:
   the request plus `HealthDamage`, the health actually lost) after
   `OnHealthChanged` and before `OnHealthDepleted`, only when a damage request
   was passed and health dropped (so server-only and damage-only; RepNotifies
   pass no request). This matters because death fires synchronously inside
   `ReceiveDamage`, so anything recorded after it returns misses the killing
   blow. `UEnclaveDamageHistoryComponent` (server-only, unreplicated, on the
   character, so a new pawn per life resets it) binds to it and records one
   entry per hit: weak `Attacker` (the request's `SourcePlayerState`), weak
   `DamageCauser`, `HealthDamage`, `ServerTime` (`GetServerWorldTimeSeconds`),
   `DamageTags`. Every hit is recorded (hostile, friendly, self, environment);
   queries filter by attitude. Pruned on record by `MaxEntryAgeSeconds` (30)
   and `MaxEntries` (64, oldest first). `RecordDamage` doesn't know about
   health; `GetEntries()` and `GetEntriesSince(Time)` return views of the tail.
   A temporary `Verbose` dump of the victim's history in `HandleAvatarDied`
   (remove in moment 2). Verified in PIE: a 5-hit kill lists all hits with the
   killing blow at 0.00 s; `EnclaveDebugDamage 1000` records 100 (no
   overkill). A fall below the kill Z adds no entry (it bypasses
   `ApplyDamage`); moment 2 must test shoot-then-fall. Done.
2. Kill credit. `AEnclaveMatchGameMode::ResolveKillCredit`: the killing
   blow's attacker if they are an enemy; otherwise (yourself, no player, a
   teammate) the most recent enemy in the victim's damage history within
   `KillCreditWindowSeconds` (`FEnclaveMatchSettings`, 5, `?KillCreditWindow=`,
   0 = off); otherwise the killing blow's attacker. Everything after it in
   `HandleAvatarDied` uses the credited player, so a credited death is a kill:
   the victim's death counts and there is no `SuicideScore`. A hostile killing
   blow always wins, even over an enemy who did more damage (that is an
   assist, moment 3). The history query is generic
   (`FindMostRecentAttacker(Since, Accept)`); the game mode passes the
   hostility check. Decided: a team kill after enemy damage inside the window
   is the enemy's kill (they did the work); with no enemy damage it stays a
   team kill. No team-kill penalty exists yet; if one is added, `ScoreKill`
   needs the killing-blow player too (it gets only the credited one). The log
   says `Death (credited kill)` plus `Killing blow by ...`; a kill-feed flag is
   C4's. Verified in PIE: credit after a self-kill and after a fall (no
   `SuicideScore`), a suicide outside the window costs a point, and in TDM
   (friendly hits applied) a teammate's finish after enemy damage is the
   enemy's kill. Done.
3. Assists. `AEnclaveMatchGameMode::AwardAssists`, after the death line:
   every enemy of the victim other than the credited killer whose damage
   within `AssistWindowSeconds` (10, `?AssistWindow=`, 0 = off; keep it at or
   under the history's 30 s) adds up to at least `AssistDamagePercent` (25,
   `?AssistDamagePercent=`) of the victim's max health. A share of max health,
   not of the damage dealt, so the bar doesn't move with how many joined in.
   `Assists` on `AEnclaveMatchPlayerState` (added, reset, copied, replicated
   like `Kills`). The log adds `  Assists: A (40, score 7), B (25, score 3).`
   Both helpers take one `Now` (server time) from `HandleAvatarDied`. Decided:
   assists are a fact in every mode, DM included (there it means "you nearly
   had that kill"); C4 decides per mode whether the scoreboard shows them.
   What an assist pays is `AssistScore` (`FEnclaveMatchSettings`, 0,
   `?AssistScore=`, added after U6): player score only, never team score (the
   team already scored the kill), and it counts toward the score limit, so in
   DM an assist can win. 0 in both mode Blueprints. A number rather than an
   on/off flag (0 = counted but unpaid; `AssistWindow=0` turns assists off),
   and no virtual `ScoreAssist` hook until a mode needs more than a number.
   An uncredited suicide can still give assists (the damage counted).
   Verified in PIE: 40 damage gives an assist, 20 doesn't, damage outside the
   window doesn't, a credited kill gives no assist to the killer, and a
   teammate's damage never does. `AssistScore` verified on a dedicated
   server with `?AssistScore=1`: the assister's score rises by 1 and the team
   score only by the kill. Done.

Decided in the U6 discussion, built later, not in U6:
- **History stays separate from the damage receiver.** The receiver
  (`IEnclaveDamageReceiverInterface`, later the pool router) is on every
  damageable thing; the history only on things whose death earns credit, and
  it serves match rules, not combat mechanics. When the router exists, the
  history binds to its `FEnclaveDamageTaken` event instead of the health
  component's.
- **Damage pools:** shield and health damage both count toward credit (as in
  Apex's damage stat). Pools (health, shield, temporary shields) become GAS
  attributes drained in a fixed order where `Damage` is consumed
  (`EnclaveAttributeSet.cpp:43-50`); temporary shields and absorb spells are
  gameplay effects. Not a registry of pool components with priorities (it
  splits state from GAS and makes the order emergent). The damage receiver
  then announces death only after it has produced the `FEnclaveDamageTaken`
  result, instead of inside the attribute delegate.
- **Hit feedback:** `ReceiveDamage` returns `bool` today (enough for the hit
  marker in `UEnclaveGA_Fire::NotifyHitConfirmed`). Damage numbers, kill and
  shield-break markers need it to return the damage-taken breakdown; design it
  with armor or with combat feedback, whichever comes first.

C2 is built in three steps:

1. Spawn selection. Done. `EnclaveSpawnSelection::ChooseStart` is a pure
   function with unit tests: a start's score is its distance to the nearest
   living enemy, and the pick is random among starts scoring at least
   `RandomMarginFraction` (0.8) of the best. Blocked starts are used only when
   all are. `AEnclaveMatchGameMode::ChoosePlayerStart` gathers the allowed
   starts (`IsStartAllowedFor`) and living hostile pawns. Fixed on the way:
   every respawn reused the login start, because `ShouldSpawnAtStartSpot` is
   true once `StartSpot` is set, and it's set only at login
   (`GameModeBase.cpp:842-848`, `1168-1176`); the match mode now returns
   false. A PIE "Play from Here" start is used for each player's first spawn
   only. Verified in PIE (DM, 3 players).
2. Team starts. Done. `AEnclaveTeamPlayerStart` is a start that belongs to one
   team; a plain `APlayerStart` is shared. The team match mode's
   `IsStartAllowedFor` allows a team start only for its team's players (a
   player with no team gets the plain starts). Modes without teams use every
   start, so one map can serve both. A subclass rather than an optional team
   on every start, so "team" stays out of what DM uses.
3. Warmup spawning. Done. Players spawn during warmup too:
   `PlayerCanRestart` skips `AGameMode`'s in-progress check in
   `WaitingToStart` (`GameMode.cpp:548-556`), and `HandleStartingNewPlayer`
   spawns arrivals then (`AGameMode` only does once the match is in progress,
   `GameMode.cpp:529-540`). A warmup death respawns and scores nothing. At
   match start every warmup pawn is removed before the engine's loop spawns
   everyone fresh (`GameMode.cpp:208-215`), after a team mode has assigned
   the teams. `RespawnAfterDeath` skips a player already on a living pawn, so
   a warmup death's timer firing after the start can't replace the fresh
   pawn. The warmup hit policy stays Simulate; switching it to Apply (practice
   fights) is a Blueprint change.

C6, first pass: `Tools/Scripts/Maps/create_graybox_maps.py` (run
`CreateGrayboxMaps.ps1` with the editor closed; re-running rebuilds the
actors labelled `GB.` and keeps the rest) generates two maps in
`/Game/Maps/Graybox`:

- `TDM_TwinBases` (80 x 40 m, 4v4): two mirrored bases across a trench, a
  covered bridge over it, flank ramps out of the trench, and in each base a
  balcony overlooking the courtyard and a screened spawn room with 8 team
  starts.
- `DM_Pinwheel` (50 x 50 m): a raised center with four off-center ramps,
  raised corner platforms, screen walls that make loops along the sides, and
  12 starts spread around.

**Next:** C4 HUD, the last shared piece built on both match branches.

Run a match by hand (close the editor first; server, then one line per client):
`UnrealEditor.exe Enclave.uproject "/Game/Maps/EnclaveMap?game=TDM?ScoreLimit=3?TimeLimit=180?Warmup=5?PostMatch=8" -server -log`
and `UnrealEditor.exe Enclave.uproject 127.0.0.1 -game -windowed -ResX=960 -ResY=540`.
Without a HUD (C4), the server log's `LogEnclaveMatch` lines are the scoreboard.
Add `-LogCmds="LogEnclaveCombat Verbose"` to the server to log every hit's
response (Ignore, Simulate, Apply) and damage. For a reconnect, type
`disconnect`, then `open 127.0.0.1`, in the same client's console: a new
client process is a different player (a new unique net ID). For a suicide,
type `EnclaveDebugDamage 1000` (or `... Environment`) in a client's console.
PIE ignores `?game=` and URL options: it uses the map's World Settings
GameMode Override and the mode Blueprint's values.

## Next milestones

| Milestone | What | Doc |
|---|---|---|
| After M4 | Lag Compensation v2: a central server-only manager, per-bone (hitbox) rewind, scaling to 26-32 players; with it, an `ApplyDamage` overload that takes the damage receiver the manager already holds (the damage policy stays inside `ApplyDamage`) | [LagCompensation_v2_Design.md](LagCompensation_v2_Design.md) (draft) |
| Later | Bots (likely at launch; free-for-all bots need an attitude solver that calls `IsHostileTo`) | none yet |
| Later | More modes (CTF, Control, Sector-style), as new subclasses of the solo or team match GameModes | M4 plan, Architecture |

Gameplay ideas are collected in `GameplayIdeas.txt`.

## Known issues and small leftovers

Fixed only when chosen; none block M4.

- A player who returns to a reserved place after the engine dropped their
  PlayerState (300 s, or more than 16 leavers) gets their team back but not
  their match stats. Keeping stats would mean storing them in the roster too.

- The match ends on the first kill the server receives at the score limit, so
  ping can decide a last-kill race (kills in the same frame are a draw). The
  fair fix (a grace window ranked by fire time) is decision 7 in
  [LagCompensation_v2_Design.md](LagCompensation_v2_Design.md).
- Switching the weapon hit detection mode mid-match is untested (only the
  BeginPlay path). Quick PIE check:
  `Enclave.Debug.SetWeaponHitDetectionMode SkeletalHitboxes`, then shoot.
- The M3 abuse table was measured at 30 Hz and is out of date at 60 Hz.
- The SSR summary's "Placed" column always shows "1 (0 in lane, 0 parked)";
  a reporting bug only.
- Two SSR diagnostics remain on the character (`DebugLogViewGeometryForShot`,
  the `OnRep_ReplicatedMovement` body); they move to the hit target component
  in Lag Compensation v2.
- The server log warns about 16 out-of-date Manny `PoseAsset`s
  (`/Game/Characters/ThirdPerson/Rig/Poses/Manny`); re-saving them fixes it.
- `EnclaveDebugRespawn` should be rechecked in the SSR test range now that item
  teardown happens in `AEnclaveCharacter::UnPossessed`.
