# Milestone 4: Match Framework (TDM and Deathmatch)

Status: in progress. Done so far: U1 and U2 (commit 97a6082); the rename of
AEnclaveGameModeBase to AEnclaveGameMode on AGameMode, with its debug commands moved
out of the class; AEnclaveMatchGameMode with FEnclaveMatchSettings (C1 started); and
the SSR settings moved from the GameMode into UEnclaveCombatPolicyComponent.

## Goal

A complete, repeatable match on a dedicated server: join, get a team, fight, die,
respawn, score, win, see the results, and start the next match. It is built as a
framework that supports several game modes. Team Deathmatch (TDM) and
free-for-all Deathmatch (DM) are the first two. CTF, Control and Sector-style
modes can be added later as new subclasses, without changing the framework.

Shipping DM alongside TDM is deliberate. A second mode keeps TDM-only assumptions
(for example "every player has a team") out of the base class.

## What already exists

| Piece | Where | Use in M4 |
|---|---|---|
| Death event with killer context | `UEnclaveAvatarLifecycleComponent::OnAvatarDied` (server), carrying `FEnclaveDamageRequest.SourceActor` | The match GameMode binds to it to credit kills |
| ASC survives respawn | `AEnclavePlayerState` owns the ASC; the character is the avatar | Respawn creates a new pawn; abilities and player-scoped effects persist |
| Respawn cleanup path | `AEnclavePlayerController::PerformDebugRespawn` (clear wielded item, destroy inventory, unpossess, destroy, `RestartPlayer`) | Promote to the real respawn path, without the debug-only guard |
| Starting loadout | `AEnclaveCharacter` `StartingSlottedItems` / `StartingInventoryItems` | Each respawn gets the loadout automatically |
| Single damage entry point | `UEnclaveDamageStatics::ApplyDamage` | One place to apply the friendly-fire rule |
| HUD art | `Content/UI/Hud/Art` (elimination feed, ammo counter, accolades) | Use for the HUD widgets |
| Test harness | `Tools/Scripts/SSR/RunSSRSession.ps1`, dedicated server plus N clients | Reused for a match smoke test |

Missing: a GameState, teams, kill/death stats, scoring, match phases, real
respawn, spawn selection, and every HUD element except the reticle.

## Architecture

```
AGameMode (engine: match-state machine)
  AEnclaveGameMode                    every map: player classes, UEnclaveCombatPolicyComponent (SSR, hit registration)
    AEnclaveMatchGameMode             a contest: phases, timer, deaths, respawn, spawns; no teams, no result
      AEnclaveSoloMatchGameMode       free-for-all: everyone NoTeam; individual result and win check
        AEnclaveDeathmatchGameMode    rules only: +1 per kill, first to N
      AEnclaveTeamMatchGameMode       teams: count, team scores, assignment and balancing, friendly fire; team win check
        AEnclaveTeamDeathmatchGameMode rules only: +1 team score per kill, first to N

AGameState (engine)                   one level per GameMode level; each GameMode sets GameStateClass to its own
  AEnclaveGameState                   every map: nothing beyond AGameState yet
    AEnclaveMatchGameState            phase end time, kill event
      AEnclaveSoloGameState           WinningPlayer
      AEnclaveTeamGameState           TeamScores, WinningTeam

APlayerState (engine)                 the same mirror; each GameMode sets PlayerStateClass to its own
  AEnclavePlayerState                 every map: ASC, IsHostileTo (default: anyone else)
    AEnclaveMatchPlayerState          kills, deaths (solo modes use it directly)
      AEnclaveTeamPlayerState         TeamId, OnTeamChanged; IsHostileTo: not the same team
```

Decisions:

- **Rename `AEnclaveGameModeBase` to `AEnclaveGameMode` and reparent it from
  `AGameModeBase` to `AGameMode`.** The Blueprint was renamed to
  `BP_EnclaveGameMode`, and the maps resaved. This
  gives us Unreal's built-in match states (WaitingToStart, InProgress,
  WaitingPostMatch) and requires a GameState derived from `AGameState`. Lyra's
  Experience system is more flexible but much heavier than Enclave needs now.
- **The match base knows nothing about teams; team modes share
  `AEnclaveTeamMatchGameMode`.** Team count, scores, assignment, balancing,
  friendly fire and the player's `TeamId` are team rules and team data. Each
  team override sets up team state before calling `Super`, because the base
  spawns players.
- **Every mode asks "is this player my enemy?", never "is this player on my
  team?".** `AEnclavePlayerState::IsHostileTo` is virtual: anyone else by
  default, other teams in `AEnclaveTeamPlayerState`. Friendly fire, spawn
  selection, friend/enemy colors and (later) bots use it, so they work in every
  mode. A free-for-all can't be modelled as "everyone on `NoTeam`": the engine's
  default attitude solver (`FGenericTeamId`, `AIInterfaces.cpp`) treats two
  equal IDs as friendly, so two `NoTeam` players would be friends. Bots in
  free-for-all modes will need a custom solver that calls `IsHostileTo`.
- **A field or rule belongs at the level where every mode below it has that
  concept.** The result is a player in free-for-all and a team in team modes,
  so neither belongs in the match base: `AEnclaveSoloMatchGameMode` and
  `AEnclaveTeamMatchGameMode` each own theirs, and the GameState tree mirrors
  the GameMode tree level for level. Mode-specific levels below them
  (`AEnclaveDeathmatchGameState` and so on) are added only when a mode has
  replicated state of its own; DM and TDM use the solo and team GameStates
  directly.
- **Test maps keep the base mode.** `SSR_TestRange` stays on
  `BP_EnclaveGameMode` (a child of `AEnclaveGameMode`), which has no match rules, so the SSR harness is
  unaffected by respawns, teams or match end.
- **The framework never knows about mode-specific objects.** Flags and control
  points (later) are actors that report to the GameMode through an interface.
- **Settings are data with URL overrides**, the same as `?LagCompensation=`:
  `?ScoreLimit=`, `?TimeLimit=`, `?RespawnDelay=`, `?Warmup=`, `?MinPlayers=`,
  `?PostMatch=` (all times in seconds; see `FEnclaveMatchSettings`). Testing a
  3-kill match takes a flag, not a rebuild.

### What happens on a kill (all on the server)

1. Health reaches zero, and the lifecycle component broadcasts `OnAvatarDied`.
2. `AEnclaveMatchGameMode` bound to it when it spawned the pawn. It resolves the
   victim and killer controllers. The killer is `SourceActor`'s controller,
   null for environment damage, and the victim for self-damage.
3. The base class records stats (`AddDeath` for the victim, `AddKill` for a
   valid killer), then calls the mode's `ScoreKill`.
4. The base class calls `CheckWinCondition`. If it reports a winner, the match
   moves to WaitingPostMatch.
5. Otherwise the base class schedules a respawn after `RespawnDelay`, choosing
   the start with `ChoosePlayerStart`.
6. The GameState multicasts a kill event, which feeds the elimination feed.

### Contract (agreed signatures)

These are fixed up front so the work can happen in parallel. Names can change in
review.

```cpp
// AEnclavePlayerState  (every map)
virtual bool IsHostileTo(const APlayerState* Other) const;  // default: anyone else

// AEnclaveMatchPlayerState  (U1, split by C1)
int32 GetKills() const;  void AddKill();   // authority only
int32 GetDeaths() const; void AddDeath();  // authority only
// Score: use APlayerState::SetScore/GetScore, which already exists and is replicated.

// AEnclaveTeamPlayerState  (U1, split by C1)
static constexpr uint8 NoTeam = 255;       // not on a team yet (spectator, unassigned)
uint8 GetTeamId() const;
void  SetTeamId(uint8 NewTeamId);          // authority only
FOnEnclaveTeamChanged OnTeamChanged;       // fires on server and clients (OnRep)
// IsHostileTo: hostile unless on the same team

// AEnclaveMatchGameState  (U2, split by C1)
double GetPhaseEndServerTime() const;  // warmup or time-limit end; compare with GetServerWorldTimeSeconds(); 0 = no end
void  SetPhaseEndServerTime(double ServerTime);          // authority only
void  MulticastKill(APlayerState* Killer, APlayerState* Victim);  // unreliable multicast, feed only

// AEnclaveSoloGameState  (null winner in WaitingPostMatch = draw)
APlayerState* GetWinningPlayer() const;
void  SetWinningPlayer(APlayerState* Player);            // authority only

// AEnclaveTeamGameState  (NoTeam winner in WaitingPostMatch = draw)
void  InitTeams(int32 NumTeams);                         // authority only; zeroed scores, no winner
int32 GetNumTeams() const;
int32 GetTeamScore(uint8 TeamId) const;
void  AddTeamScore(uint8 TeamId, int32 Delta);           // authority only
uint8 GetWinningTeam() const;
void  SetWinningTeam(uint8 TeamId);                      // authority only

// AEnclaveTeamMatchGameMode  (C1)
int32 GetNumTeams() const;                               // EditDefaultsOnly NumTeams, default 2; teams are 0..N-1
virtual uint8 ChooseTeamForPlayer(const AController* Player) const;  // default: the smallest team, ties to the lowest ID
// Assigned in HandleStartingNewPlayer before Super (every join path ends there);
// a player who already has a valid team (reconnect, seamless travel) keeps it.
// Solo modes have no team field at all (AEnclaveMatchPlayerState).

// AEnclaveMatchGameMode virtual hooks, overridden by modes (C1 defines them, U3/U4 implement)
virtual void  ScoreKill(AEnclavePlayerState* Killer, AEnclavePlayerState* Victim);
virtual bool  CheckWinCondition();                       // sets the solo or team winner when true
virtual bool  CanDamage(AController* Instigator, AController* Victim) const;  // friendly fire
```

## Work items and ownership

**U** = you write it, and I review the diff. **C** = I write it, and you review.
We work through the items together, one at a time, rather than in parallel, so each
design decision is discussed as it is made.
The U items cover the core Unreal multiplayer skills that interviewers ask about
most: replication, authority and the GameMode/GameState split.

| ID | Item | Owner | Notes |
|---|---|---|---|
| U1 | PlayerState team, kills and deaths | **You** | `Replicated` / `ReplicatedUsing`, `GetLifetimeReplicatedProps`, authority checks, OnRep event. Small and self-contained, so a good first piece. (Later split by C1 into match and team PlayerStates.) |
| U2 | `AEnclaveGameState` | **You** | Replicated team scores, end time and winner; the kill multicast. Set `GameStateClass` in the GameMode. (Later split by C1 into match, solo and team GameStates.) |
| C1 | Reparent to `AGameMode` and add the `AEnclaveMatchGameMode` skeleton | Claude | Phases, warmup and timer, death binding, respawn timer, URL options, virtual hooks with defaults |
| U3 | Team base rules, then TDM rules | **You** | In `AEnclaveTeamMatchGameMode`: team balancing (the smaller team), `CanDamage` (friendly fire) and the team win check. In TDM: `ScoreKill` |
| U4 | Solo base rules, then DM rules | **You** | In `AEnclaveSoloMatchGameMode`: the individual win check. In DM: `ScoreKill`. Short once TDM exists. |
| C2 | Spawn selection | Claude | `AEnclavePlayerStart` with an optional team; reject occupied starts; prefer starts far from living enemies. Written as a pure scoring function with unit tests. |
| C3 | Friendly-fire gate | Claude | `ApplyDamage` asks the match GameMode's `CanDamage`. Self-damage is still allowed. |
| C4 | HUD C++ base and data plumbing | Claude | Widget base classes that read GameState and PlayerState: score and timer bar, elimination feed, Tab scoreboard, death/respawn countdown, end-of-match screen, health and ammo |
| U5 | HUD widget layouts (UMG) | You or your team | Optional. Uses the existing HUD art. I can make plain placeholders if nobody wants it. |
| C5 | Team colors | Claude | Simple mesh tint so friend and enemy are readable. Placeholder until real art. |
| C6 | Graybox arena map | Claude | Generated by a script like the test range: two spawn areas, mid cover, 4v4 scale. Meant as a first pass for you to edit. |
| C7 | Tests | Claude | Automation tests for the rules (team balancing, win check, spawn scoring), plus a match smoke-test script using the existing harness |

## Order

1. **Foundation.** U1, U2, then C1.
2. **Rules.** U3, then U4. In parallel, C2 and C3.
3. **Presentation.** C4, then U5 (or placeholders), then C5.
4. **Playable.** C6 and C7, then a real playtest with people. The milestone is done when
   a 4v4 dedicated-server match runs from start through restart three times in a row,
   with no errors in the logs.

## Defaults to confirm

| Setting | Proposed default | Why |
|---|---|---|
| Team size | 4v4 (up to 8 players) | Already measured at 60 Hz with 8 clients |
| Score limit | TDM 50 team kills; DM 25 kills | Common values. 4v4 at about 1 kill per player per minute ends near 10 min. |
| Time limit | 10 min; at the time limit the higher score wins, and a tie is a draw | Simple. Overtime can come later. |
| Respawn | Automatic after 3 s | Keeps the action moving. Click-to-respawn is a later option. |
| Warmup | 10 s countdown once 2 players are in | Easy to test. Production would wait for more players. |
| Post-match | Results for 15 s, then restart the same map | No map rotation yet |
| Friendly fire | Off | Standard for public TDM |
| Late join | Join in progress, placed on the smaller team | |
| Spawn protection | None at first | Add only if playtests show spawn kills |
| Bots | Out of scope | |

## Out of scope for M4

Bots, progression, loadout selection, map rotation, a matchmaking or lobby menu,
CTF and Control modes, Sector perks. The framework should make these possible
later; M4 doesn't build them.

## Carried forward from M3

- Spawn selection that avoids occupied starts. This is C2.
- Re-measure SSR on production-like servers with real player counts. The playtest in step 4 is a first look.
