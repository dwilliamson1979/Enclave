# Enclave SSR — Stage C4E: Two-player human-movement shadow validation

**Status:** Development-only test rig and shadow telemetry. Server-Side Rewind is **not enabled**.

## Why this stage exists

The patrol bot is controlled by server-side AI. That validates historical capsule math but does not exercise how the server records and replicates **player-controlled** CharacterMovement. The goal here is to obtain this evidence using the existing **two-player PIE** setup, not to require an additional human or a new movement implementation.

## Implementation

`AEnclavePlayerController::EnclaveDebugSSRStrafe(int32 bEnable)` is an `Exec` command evaluated **only on the locally owning controller**. While enabled, this controller feeds `AEnclaveCharacter::AddMovementInput` along a fixed right axis captured at activation. The pawn walks between +/-250 cm of its starting position, pauses briefly to reverse, and stops after 30 seconds, manual disable, pawn loss, or movement disable (including death). The test does not teleport actors, directly modify velocity/replication, add new RPCs, or change collision. Development-only code is compiled out of Test/Shipping builds (the command remains a no-op stub, consistent with existing development Exec commands).

`SSRShadow ANCHORS` now also emits `serverPlayerTargets`, `movingPlayerTargets` (speed >=25 cm/s), and `maxPlayerTargetSpeedCmS`. These are independent of the historical hit result: a player may be moving in the world even on a shot that misses. As with all previous stages, the **server-derived anchor has zero additional smoothing** and remains diagnostic-only.

## Minimal test — client fires at a player-controlled moving target (preferred first)

1. Launch the existing **two-player, single-process PIE**: listen-server host and one remote client. Position the players in sight of one another with space to move sideways. Have the **listen-server player face toward the remote client** before activating the strafe, so the host's right axis is approximately lateral to the remote shooter's view.
2. On the **listen server**, configure hit registration to **Capsule** (your existing GameMode policy/command) and run `Enclave.Debug.SSRShadow 1`.
3. In the **listen-server player's console**, run `EnclaveDebugSSRStrafe 1`. Check for `SSRPlayerStrafe LOCAL START` and that the host character moves back and forth; it auto-stops after 30 seconds. `EnclaveDebugSSRStrafe 0` ends it immediately.
4. Switch to the **remote client's window** and fire **two or three** deliberate shots toward the moving host player. It is fine if some miss. Avoid killing the target before gathering useful shots, or respawn it and restart the strafe.
5. Inspect server log: `SSRShadow TIMING_POLICY`, `SSRShadow ANCHORS`, and (if hit) `SSRShadow CAPSULE_CHECK`. For shots during motion, `serverPlayerTargets` should be >=1 and `movingPlayerTargets` should usually be >=1. When the historical capsule is intersected, `serverTargetIsPlayer=1`. Compare `clientHit`, `serverHit`, and the live hit; disagreement is diagnostic data, not automatically a defect.

This tests the firing **remote client**, client-side observation of a **replicated player target**, and player-capsule history. The target's movement is authority-local on the listen server, so it does **not** test batched movement received from a remote autonomous target.

## Optional second test — server fires at a client-owned moving target

1. Stop the host's strafe if active. In the **remote client's console**, run `EnclaveDebugSSRStrafe 1` and leave that client window running. The local owner uses its normal predicted CharacterMovement input, so the server processes standard remote movement updates.
2. Fire two or three shots from the **listen-server host** at the remotely owned moving character.
3. On the server, inspect `SSRShadow ANCHORS` and `SSRShadow CAPSULE_CHECK` where hits occur. `movingPlayerTargets` should demonstrate actual server-observed movement. The listen-server shooter has a different (near-zero) RTT/timing profile from a remote shooter, so these results are **not** interchangeable with the first test.

## What this validates, and what it does not

- **Validates:** A real player target is tracked by server-only history; historical and present-time capsules compare under locomotion; remote-owned movement generates ordinary CharacterMovement replication; remote shooters can shoot a replicated player target; timing-anchor disagreement is visible.
- **Does not validate:** both shooter and target being remote simultaneously (would require a third connection), high-ping asymmetry, a measured client mesh presentation delay, or high-ping fallback exploitation. It also does not make either time anchor authoritative.
- **Pass condition for this diagnostic stage:** commands move characters through normal CharacterMovement, the server identifies a moving player target, histories are available, and shadow query logs without crashes or altering damage. Do **not** demand all anchors match the live hit — timing is precisely what remains under investigation.

## Remaining gates before C5

- Determine a defensible server-derived presentation-timing correction; the zero-smoothing baseline is not production timing.
- Deliberately approve/test rewind-out-of-window policy and how history-unavailability is handled per target.
- Validate discontinuity behavior, non-static obstruction, and remote-versus-remote movement under suitable latency when the test setup allows it.
- Keep capsule-only shadow queries separated from SkeletalHitboxes mode until skeletal history exists.
