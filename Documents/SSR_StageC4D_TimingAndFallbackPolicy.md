# Enclave SSR — Stage C4D: Timing and unavailable-history policy

**Status:** Engineering proposal + *shadow-only* instrumentation. Must be accepted and implemented/tested separately before C5 enables historical damage.

## Preconditions

- Shot intent passes **Stage B** independently (origin, direction, sequence, timestamp plausibility, obstruction). Invalid shot intent is **rejected**, not downgraded to a live hit.
- The server controls `ServerReceiptTimeSeconds`, the `PlayerState` RTT estimate, and the match's `MaxRewindAllowanceMs` (default **200 ms**, bounded 0–500 ms). Client shot time remains untrusted corroborating evidence.
- Historical target data is server-authored; capsule-only geometry. Historical traces may never relocate live Actors, interpolate over discontinuity markers, extrapolate outside retained history, or ignore present-time world obstruction.
- C4D does not turn on `LagCompensationMode::ServerSideRewind`. The match still forces Disabled.

## Timing candidates compared in shadow mode

Define `t_r` = server-owned shot receipt time, `r` = server-observed RTT estimate, `d` = *additional* client presentation/smoothing delay, `t_c` = client's estimated server time at shot capture.

1. **Client-reported baseline:** `t_c` (C3 diagnostic). This is an untrusted estimate and **not** accepted as the sole authoritative anchor.
2. **Server-derived baseline:** `t_r - r - d`. Interpretation: assuming roughly symmetric delivery, server receipt occurs one uplink after capture and the target representation may lag capture by one downlink plus smoothing. For C4D only, **`d = 0`**, so the result does **not** claim to reproduce rendered target position. Server-owned timing removes a direct client rewind-timestamp choice but RTT accuracy, network asymmetry, packet scheduling, and mesh smoothing remain unresolved.

Each candidate is **invalid**, with no clamping, unless it is finite, nonnegative, no later than `t_r`, and at most the server's `MaxRewindSeconds` before `t_r`. The server-derived candidate additionally requires an available nonnegative server RTT estimate. The client timestamp is *not* used in the server-derived formula.

Stage C4D emits `SSRShadow TIMING_POLICY` and `SSRShadow ANCHORS` and runs both geometry candidates when valid. No damage decision uses either result.

## Proposed eventual fallback policy (not active until C5)

After Stage B passes:

- For a **supported capsule** target with complete, valid history within the accepted rewind window, use the selected authoritative historical intersection, subject to present-time world geometry obstruction.
- When the **server-derived time is unavailable or outside the rewind allowance**, or required target history is unavailable, **fall back to ordinary present-time hit registration** rather than inventing a timestamp, extending the window, or fabricating a miss. This is a defined fairness choice for high-ping legitimate players; the live trace may still miss a moving target.
- If the shot **fails Stage B validation**, reject it completely; do not fallback.
- On **SkeletalHitboxes mode** with no skeletal-history support, retain ordinary present-time hit registration. Never use a historical *capsule* hit as a substitute for bone-specific hit registration.
- At lifecycle/teleport boundaries, do not join incompatible historical segments. If a safe historical snapshot is missing, do not synthesize one. Present-time fallback must still respect current death and damage policy.

**Prior to C5:** explicitly document/approve whether the fallback is match-global vs per-target and adjust the broad `MissingHistories` diagnostics (they currently include some unrelated characters). Validate that fallback cannot be invoked to intentionally bypass SSR via crouch, spawn, teleport, or manipulated packet timing. Stage C4C mitigates crouch resets but does not settle every history-unavailability case.

## Test to close C4D

Use listen server + **two remote clients** (three-player same-process PIE or equivalent). Have client B strafe laterally and client A aim/follow/fire two or three shots while `Enclave.Debug.SSRShadow 1` is enabled on the **server**, the GameMode uses **Capsule**, and normal firing works.

Inspect `SSRShadow TIMING_POLICY` and `SSRShadow ANCHORS` (the server candidate identifies a human target with `serverTargetIsPlayer=1` when intersected). Retain diagnostics on misses, too. We expect server baseline to differ from the client baseline and do **not** require agreement on every shot: smoothing time is still unknown, and networked human movement has different frame cadence than the patrol AI.

No dedicated server, lag matrix, high-ping fault injection, or multi-hour gameplay tests at this diagnostic-only checkpoint. Full C5 production validation must cover these later.

## Outstanding decisions for C5

1. Determine server-derived *additional smoothing* delay and its defensible measurement method. Do **not** use C4A inverse-fit offsets as an automatic gameplay correction.
2. Decide/validate treatment of asymmetrical uplink/downlink and stale/smoothed PlayerState ping, high ping beyond 200 ms, and clock-sampling phase.
3. Harden broad `MissingHistories` comparability (current diagnostics are conservative/noisy), and test remote-human strafing with relevant world obstruction.
4. Decide whether/how to support skeletal hitbox history or retain a present-time-only skeletal mode.
5. Convert the proposed present-time fallback into a match-server rule only after security review and relevant regression tests.
