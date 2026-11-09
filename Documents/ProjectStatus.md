# Enclave Project Status

Last updated 2026-11-09, after U3 moment 5, reserved team slots (branch `ClaudeEdits`).

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
| U3 | Team base rules, then TDM | Done (six moments, below); warmup spawning is a known issue |
| U4 | Solo base rules, then DM | Not started |
| C2 | Spawn selection | Not started |
| C3 | Damage policy: `ApplyDamage` asks the game mode's `ResolveDamage` (hit policy per phase) | Done with U3 moment 6 |
| C4 | HUD C++ base | Not started |
| U5 | HUD layouts (UMG) | Not started |
| C5 | Team colors | Not started |
| C6 | Graybox arena map | Not started |
| C7 | Tests and match smoke test | Not started (unit tests so far: `Enclave.Match.Settings`, `.URLOptions`, `.TeamAssignment.*`, `.TeamRoster`) |

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

Decided in U3: kills and deaths are facts recorded by the match base; score is
each mode's judgement (TDM: +1 per kill; objective modes may pay little or
nothing per kill). Mode-only stats go on a mode PlayerState. Names are short:
`AEnclaveTDMGameMode`, `AEnclaveDMGameMode`. Mode defaults that differ between
modes (the score limit: TDM 50, DM 25) are set in each mode's Blueprint; the
settings struct keeps neutral defaults (`ScoreLimit` 0), because a Blueprint
saves only values that differ from its parent.

**Next:** U4 (solo base rules, then DM; the user's item), so both match
branches exist before the shared pieces (C2 spawns with warmup spawning, C4
HUD) are built on top.

Run a match by hand (close the editor first; server, then one line per client):
`UnrealEditor.exe Enclave.uproject "/Game/Maps/EnclaveMap?game=TDM?ScoreLimit=3?TimeLimit=180?Warmup=5?PostMatch=8" -server -log`
and `UnrealEditor.exe Enclave.uproject 127.0.0.1 -game -windowed -ResX=960 -ResY=540`.
Without a HUD (C4), the server log's `LogEnclaveMatch` lines are the scoreboard.
Add `-LogCmds="LogEnclaveCombat Verbose"` to the server to log every hit's
response (Ignore, Simulate, Apply) and damage. For a reconnect, type
`disconnect`, then `open 127.0.0.1`, in the same client's console: a new
client process is a different player (a new unique net ID).

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

- Players have no pawn during warmup: `AGameMode::PlayerCanRestart` is false
  until the match is in progress (`GameMode.cpp:548-556`), so the warmup hit
  policy has nothing to apply to yet. Fortnite-style warmup needs a warmup
  spawning item: restarts allowed in `WaitingToStart`, and every player reset
  (fresh pawn at a spawn point) at match start, together with C2.

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
