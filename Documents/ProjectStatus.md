# Enclave Project Status

Last updated 2026-11-04, after U3 moment 1 (branch `ClaudeEdits`).

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
| U3 | Team base rules, then TDM | **In progress** (moments below) |
| U4 | Solo base rules, then DM | Not started |
| C2 | Spawn selection | Not started |
| C3 | Friendly-fire gate (via `IsHostileTo`) | Not started |
| C4 | HUD C++ base | Not started |
| U5 | HUD layouts (UMG) | Not started |
| C5 | Team colors | Not started |
| C6 | Graybox arena map | Not started |
| C7 | Tests and match smoke test | Not started (only `Enclave.Match.Settings.ApplyOptions` exists) |

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

U3 is built in three moments (balancing on join was already done in C1 moment 4):

1. The team win check (`AEnclaveTeamMatchGameMode::CheckWinCondition`): the
   match ends at the score limit or the time limit, the single top team wins,
   and a shared top score is a draw. Also TDM's `ScoreKill`
   (`AEnclaveTDMGameMode`): a kill of an enemy is +1 team score and +1 player
   score. Done.
2. A TDM Blueprint and the first real match on an existing map (`?game=`).
3. Friendly fire (the team mode's `CanDamage`), then C3 wires it into
   `ApplyDamage`.

Decided in U3: kills and deaths are facts recorded by the match base; score is
each mode's judgement (TDM: +1 per kill; objective modes may pay little or
nothing per kill). Mode-only stats go on a mode PlayerState. Names are short:
`AEnclaveTDMGameMode`, `AEnclaveDMGameMode`.

Nothing has run as a real match yet: there is no Blueprint or map for a concrete
mode. The first playable match is U3 moment 2.

## Next milestones

| Milestone | What | Doc |
|---|---|---|
| After M4 | Lag Compensation v2: a central server-only manager, per-bone (hitbox) rewind, scaling to 26-32 players | [LagCompensation_v2_Design.md](LagCompensation_v2_Design.md) (draft) |
| Later | Bots (likely at launch; free-for-all bots need an attitude solver that calls `IsHostileTo`) | none yet |
| Later | More modes (CTF, Control, Sector-style), as new subclasses of the solo or team match GameModes | M4 plan, Architecture |

Gameplay ideas are collected in `GameplayIdeas.txt`.

## Known issues and small leftovers

Fixed only when chosen; none block M4.

- Kill credit is lost if the killer's character is destroyed before the kill
  lands. Fix: carry the instigator's PlayerState in `FEnclaveDamageRequest`.
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
- Unverified: whether `RestartGame` (`?Restart`) keeps the server's URL
  options, such as `?ScoreLimit=3`. Check in the first real match.
- `EnclaveDebugRespawn` should be rechecked in the SSR test range now that item
  teardown happens in `AEnclaveCharacter::UnPossessed`.
