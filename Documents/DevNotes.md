# Enclave Developer Notes

Working knowledge for anyone (human or AI) working on Enclave: environment,
build and test, conventions, and the pitfalls already found. Project status is
in [ProjectStatus.md](ProjectStatus.md).

## Environment

- **Repo:** `D:\Repos\Unreal\Enclave`. Game module: `Source/Enclave`. Design
  notes: `Documents/`.
- **Engine:** the installed (launcher) build at `D:\UnrealEngine\Versions\UE_5.8`.
  A packaged dedicated server would need a source build of the engine; all
  testing so far runs `UnrealEditor.exe -server`.
- **Branches:** work happens on `ClaudeEdits`; `main` is separate.
- **"SSR"** means server-side rewind (lag-compensated hit registration), not
  screen-space reflections.
- Windows 11. Bash is Git Bash; PowerShell 5.1 is also available.

## Build and test

- **Build and run all automation tests:**
  `powershell -ExecutionPolicy Bypass -File Tools\Scripts\BuildAndTest.ps1`
  (`-SkipTests`, `-SkipBuild`, `-TestFilter Enclave.Combat.SSR`). About a minute.
  It fails while the editor is open, because Live Coding blocks UBT: close the
  editor first.
- **One SSR session:**
  `Tools\Scripts\SSR\RunSSRSession.ps1 -Dedicated -Map /Game/Maps/Test/SSR_TestRange -Layout open -LagCompensation ServerSideRewind -DurationSeconds 30 -SessionName <name>`
  (also `-Clients N`, `-ExtraServerCmds "cmd1,cmd2"`, `-TimingSource`,
  `-ServerLagMs` / `-ClientLagMs` / `-ShooterLagMs` / `-JitterMs` / `-LossPct`,
  `-ServerTickRate`). Output: `Saved\Automation\SSRSession\<name>\`
  (`server.log`, `summary.md`).
- **SSR suites** (`Tools\Scripts\SSR\`): `RunSSRLayouts.ps1` (accuracy and
  occlusion; `-DurationSeconds 40` for a quicker run), `RunSSRAbuse.ps1` (cheat
  cases and bandwidth; `-Only "baseline,forged"`), `RunSSRMatrix.ps1` (network
  conditions), `RunSSREdgeCases.ps1` (crouch, teleport, lag switch; not death).
  Each runs a warm-up session first. Long suites take more than 10 minutes.
- **Test map:** `CreateSSRTestMap.ps1` regenerates
  `/Game/Maps/Test/SSR_TestRange` from `create_ssr_test_map.py`.
- **Reference results** (dedicated, 60 Hz, about 82 ms RTT): layouts 97-100%
  clear hits, 100% clear misses, 100% not hit behind cover, no fallbacks; abuse
  suite 98-100% / 100% in every case; stamps about 140 B/s per client.
- **Noise:** about ±5 points per run at ~120 shots per category. Compare modes
  with paired sweeps or repeated runs, not single runs.

## Conventions

- **Commits:** `type(scope): summary`, then a body with bullets. `wip(...)`
  marks a sketch committed before review fixes.
- **Line endings:** source files are CRLF (`core.autocrlf=true`,
  `* text=auto`). Check with `file <path>`; fix with `unix2dos -q <path>`.
  `sed -i` in Git Bash strips CRLF; prefer the editor, or a script that
  normalizes and writes CRLF back.
- **Debug code** sits inside `#if ENCLAVE_INCLUDE_DEBUG_CODE`
  (`!UE_BUILD_SHIPPING && !UE_BUILD_TEST`); `ENCLAVE_LOG` compiles out of Test
  and Shipping. Log categories are in `Public/Debug/EnclaveDebug.h`
  (`LogEnclaveMatch` for the match framework).
- **Bad URL options** are logged and the current value is kept (operator input
  never `ensure`s). Integer options are parsed strictly, because the engine
  helpers accept "2.5" as 2 and "abc" as 0.

## Design rules that cut across milestones

- **A field or rule belongs at the level where every class below it has that
  concept.** For example, teams live in the team branch of the match framework,
  not the match base. Split a class when the parent would carry something
  meaningless for some children; use a property, not a subclass, for different
  values; extract shared code on its second real use.
- **Ask "is this player my enemy?" (`IsHostileTo`), never "same team?".**
- **Use the server clock** (`GameState->GetServerWorldTimeSeconds()`) for
  anything the HUD shows, and replicate end timestamps rather than countdowns.
- **Explicit order beats delegates where order matters:** the character's
  `PossessedBy` / `UnPossessed` run their steps in a fixed order (ASC init
  before items; item teardown before `Super`, while the ASC is reachable).
- **Combat code doesn't depend on the GameMode class:** the combat policy is a
  component found with `UEnclaveCombatPolicyComponent::Get(World)`.
- **Past stage docs and gates are reference points**, not requirements.
  Decide with data and best practice.

## Gotchas

- **Headless editor Python checks** need `UnrealEditor.exe` (not `-Cmd`), for
  example in PowerShell:
  `Start-Process -FilePath "D:\UnrealEngine\Versions\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"D:\Repos\Unreal\Enclave\Enclave.uproject`"", "-ExecutePythonScript=`"<script.py>`"", "-unattended", "-nosplash", "-nullrhi", "-log=X.log" -Wait`,
  then read `Saved\Logs\X.log`. End scripts with
  `unreal.SystemLibrary.quit_editor()` in a `finally`. Blueprint `parent_class`
  and `PhysicsAsset.skeletal_body_setups` aren't exposed to Python; use
  `isinstance` on the class default object.
- **The SSR test target can't die:**
  `AEnclaveCharacter::DebugSetSSRStrafeDamageSuppressed` (checked in
  `EnclaveGASHealthComponent.cpp`) protects it, so no SSR suite exercises death.
- **The logs don't record the hit component or bone.** To verify
  SkeletalHitboxes, add a temporary log in `RegisterHitscan`.
- **Abuse suite at 60 Hz** accepts a few `backtrack_50` / `future_50` stamps.
  That's the tick (3 frames = 50 ms), not a bug.
- **Already handled in the harness:** clients are capped at 60 FPS (uncapped,
  the background shooter starves on the shared GPU); non-shooter clients run
  `-nullrhi`; the module opts out of Windows 11 timer throttling (without it a
  60 Hz server ran at about 35 Hz); `powershell -File` passes comma lists as one
  string.
- **The first session after a build hitches**, which is why the suites warm up.
- `Automation_Enclave.slnx` sometimes shows as modified after a build; it's
  generated, so don't commit it.

## Engine facts we rely on (checked in the UE 5.8 source)

- `AGameMode::Tick` polls `ReadyToStartMatch` while WaitingToStart and
  `ReadyToEndMatch` while InProgress (both BlueprintNativeEvents: override
  `_Implementation`). `HandleMatchHasStarted` / `HandleMatchHasEnded` are plain
  virtuals.
- Every join path ends in `HandleStartingNewPlayer` (PostLogin, seamless travel);
  `AGameMode::PostLogin` first restores a reconnecting player's PlayerState.
- `RestartPlayer` reuses a pawn the controller still has
  (`GameModeBase.cpp:1287`), so a respawn must remove the old pawn first.
  `SetPlayerDefaults` runs for each pawn it spawns.
- `?game=` beats the map's World Settings GameMode override
  (`GameInstance.cpp:1531-1549`); short names come from
  `GameModeClassAliases` in `DefaultEngine.ini` (`?game=TDM`).
- `RestartGame` (`?Restart`) reloads the last URL with all its options
  (`UnrealEngine.cpp:15770`; seamless: `World.cpp:9146`), so `?ScoreLimit=`
  and `?game=` survive a restart. Verified in a real match.
- Seamless travel (`bUseSeamlessTravel`) needs no transition map: without one
  the engine travels through an empty dummy world (`World.cpp:8367-8370`).
  `AGameMode::HandleSeamlessTravelPlayer` resets the old PlayerState, then
  copies it into a new one (`CopyProperties`), so each player gets a fresh
  PlayerState and ASC. When the player controller class changes, it copies
  without the reset (`GameMode.cpp:408-441`).
- The ASC lives on the PlayerState, so it outlives each pawn. Anything a pawn
  (or its anim instances) binds to the ASC must unbind when the pawn goes;
  garbage collection is too late.
- A Blueprint saves only values that differ from its parent class. Setting a
  Blueprint property to the inherited value stores nothing.
- The default `FGenericTeamId` attitude solver (`AIInterfaces.cpp`) treats equal
  IDs as friendly, so "everyone on NoTeam" would make free-for-all players
  friends.

## Reading (SSR background)

- Yahn Bernier, "Latency Compensating Methods in Client/Server In-game Protocol
  Design and Optimization" (2001).
- Valve Developer Wiki, "Source Multiplayer Networking" and "Lag Compensation".
- Tim Ford, "Overwatch Gameplay Architecture and Netcode" (GDC 2017).
- Gabriel Gambetta, "Fast-Paced Multiplayer".
