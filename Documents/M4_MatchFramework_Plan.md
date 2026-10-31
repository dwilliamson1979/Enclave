# Milestone 4: Match Framework (TDM and Deathmatch)

Status: in progress. Done so far: U1 and U2 (commit 97a6082), and the rename of
AEnclaveGameModeBase to AEnclaveGameMode on AGameMode, with its debug commands moved
out of the class.

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
  AEnclaveGameMode                combat config: SSR, hit registration (was AEnclaveGameModeBase)
    AEnclaveMatchGameMode         match framework: phases, timer, deaths, respawn, spawns
      AEnclaveGameMode_Deathmatch     rules only: no teams, first to N kills
      AEnclaveGameMode_TeamDeathmatch rules only: 2 teams, team score to N

AGameState (engine)
  AEnclaveGameState               replicated: team scores, match end time, winner

APlayerState (engine)
  AEnclavePlayerState             (exists) + team ID, kills, deaths
```

Decisions:

- **Rename `AEnclaveGameModeBase` to `AEnclaveGameMode` and reparent it from
  `AGameModeBase` to `AGameMode`.** The Blueprint was renamed to
  `BP_EnclaveGameMode`, and the maps resaved. This
  gives us Unreal's built-in match states (WaitingToStart, InProgress,
  WaitingPostMatch) and requires a GameState derived from `AGameState`. Lyra's
  Experience system is more flexible but much heavier than Enclave needs now.
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
// AEnclavePlayerState  (U1)
static constexpr uint8 NoTeam = 255;
uint8 GetTeamId() const;
void  SetTeamId(uint8 NewTeamId);          // authority only
int32 GetKills() const;  void AddKill();   // authority only
int32 GetDeaths() const; void AddDeath();  // authority only
FOnEnclaveTeamChanged OnTeamChanged;       // fires on server and clients (OnRep)
// Score: use APlayerState::SetScore/GetScore, which already exists and is replicated.

// AEnclaveGameState  (U2)
int32 GetTeamScore(uint8 TeamId) const;
void  AddTeamScore(uint8 TeamId, int32 Delta);           // authority only
double GetMatchEndServerTime() const;  // compare with GetServerWorldTimeSeconds()
void  SetMatchEndServerTime(double ServerTime);          // authority only
uint8 GetWinningTeam() const;  APlayerState* GetWinningPlayer() const;
void  SetWinner(uint8 TeamId, APlayerState* Player);     // authority only
void  MulticastKill(APlayerState* Killer, APlayerState* Victim);  // unreliable multicast, feed only

// AEnclaveMatchGameMode virtual hooks, overridden by modes (C1 defines them, U3/U4 implement)
virtual uint8 ChooseTeamForPlayer(AController* Player);  // DM: NoTeam
virtual void  ScoreKill(AEnclavePlayerState* Killer, AEnclavePlayerState* Victim);
virtual bool  CheckWinCondition();                       // calls SetWinner when true
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
| U1 | PlayerState team, kills and deaths | **You** | `Replicated` / `ReplicatedUsing`, `GetLifetimeReplicatedProps`, authority checks, OnRep event. Small and self-contained, so a good first piece. |
| U2 | `AEnclaveGameState` | **You** | Replicated team scores, end time and winner; the kill multicast. Set `GameStateClass` in the GameMode. |
| C1 | Reparent to `AGameMode` and add the `AEnclaveMatchGameMode` skeleton | Claude | Phases, warmup and timer, death binding, respawn timer, URL options, virtual hooks with defaults |
| U3 | TDM rules | **You** | Team assignment (balance to the smaller team), `ScoreKill`, `CheckWinCondition`, `CanDamage` |
| U4 | DM rules | **You** | Short once TDM exists. It shows the base class really is mode-agnostic. |
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
