# D11D-3D: SSR timing policy, and D11D-4: authoritative selection

**Status:** Decided (2026-10-28) and implemented behind a server option. **Default timing source changed to ServerAnchor in milestone 3**; see `SSR_M3_NetworkValidation.md`. The match default stays `LagCompensation=Disabled`. Enable SSR with `?LagCompensation=ServerSideRewind` (Capsule hit detection only).

## Decision

The rewind time comes from `ResolveSSRTiming()` (`Combat/EnclaveSSRTimingPolicy.h`). It supports three server-only sources, selected by `AEnclaveGameMode::SSRTimingSource` or `?SSRTimingSource=`:

| Source | Rewind time | Status |
|---|---|---|
| Hybrid | The client's locally hit target uses its verified echoed stamp. Every other character uses the server anchor. | Selectable; no measured benefit (M3) |
| **ServerAnchor** (default since M3) | Every character uses `receipt − (RTT + AnchorOffset)`. | Supported |
| EchoedStamp | Every character uses the echoed stamp. With no verified stamp the historical answer is unavailable, so the shot follows the unavailable-history policy. | Diagnostic |

The D11D-3C exit gate asked for either "server-verifiable presentation metadata" or "a bounded estimation policy". Hybrid uses both. The client says which replicated stamp it was showing, and the server checks that the stamp names a frame it recorded for that target and that it falls inside a bounded window around its own anchor.

## Evidence

Unattended three-process sessions (`Tools/Scripts/SSR/RunSSRSession.ps1`): a listen server with no local player in the test, a remote shooter client and a remote strafing target client, run with `-PktLag` on every process. The scoring reference is the shooter's mesh-anchored, labeled capsule (D11C categories: clear hit = 10 cm inside, clear miss = 10–40 cm outside). Each session has 240 labeled shots (120 clear hits, 120 clear misses), analyzed with `analyze_timing_sweep.py`.

**Server-anchor offset sweep** (shadow). Each cell is % of clear hits that registered / % of clear misses that missed:

| d (ms) | ~62 ms RTT | ~170 ms RTT (run A) | ~180 ms RTT (run B, 10 ms steps) |
|---|---|---|---|
| −75 / −70 | — (age < 0, unavailable) | 91.7 / 95.8 | 94.2 / 99.2 |
| −50 | **97.5 / 100** | **96.7 / 97.5** | **93.3 / 100** |
| −25 / −30 | 90.8 / 96.7 | 95.0 / 94.2 | 90.8 / 97.5 |
| 0 | 75.8 / 90.0 | 78.3 / 84.2 | — |
| +25 | 48.3 / 87.5 | — | — |

The best offset is a constant, about −50 ms, rather than a fraction of RTT. The plateau runs from about −70 to −40 ms. Rewinding a full RTT (d = 0) overshoots, probably because simulated proxies are extrapolated and the PlayerState ping includes frame time. This constant was **measured on one machine with three rendering processes** and must be re-measured on a dedicated server (milestone 3).

**Echoed stamp** (same shots): 96.9–100% of clear hits registered and 99–100% of clear misses missed, across all sessions and both RTT bands.

**Stamp age − anchor age** (ms; ~62–180 ms RTT, ~200 measured stamps per session): median −2 to −12, p5 −22 to −33, p95 +30 to +44, p99 +54 to +64. A symmetric ±30 ms window rejected 7–12% of good stamps, so the window is asymmetric.

**Authoritative D11D-4 runs** (`SSRSelection` records, ~160–175 ms RTT):

| Source | Clear hits registered | Clear misses missed | Present-time fallbacks | Stamps used / rejected |
|---|---|---|---|---|
| Hybrid | 98.3% | 100% | 0 | 94 / 0 |
| ServerAnchor | 100% | 100% | 0 | — |
| EchoedStamp | 79.2% of decided | — | 143 of 240 | 97 / 0 |

EchoedStamp falls back on every shot whose local ray missed: all clear misses, and the clear hits whose stamp was stale. That is why it isn't the default. The ServerAnchor result (100%) against its sweep results (93–97%) shows run-to-run variance at 120 shots per category. Treat differences of a few percent between single runs as noise.

## Settings (GameMode, server-only)

| Property | Default | Meaning |
|---|---|---|
| `LagCompensationMode` / `?LagCompensation=` | Disabled | `ServerSideRewind` enables D11D-4 in Capsule mode |
| `SSRTimingSource` / `?SSRTimingSource=` | ServerAnchor (was Hybrid; see M3) | See above. Dev builds can also switch live with the console command `Enclave.Debug.SetSSRTimingSource` |
| `SSRAnchorOffsetMs` | −25 (was −50; see `SSR_M3_NetworkValidation.md`) | target age = RTT + offset, clamped to [−150, 100] |
| `SSRStampEarlyToleranceMs` / `SSRStampLateToleranceMs` | 40 / 60 | Hybrid: how much younger / older than the anchor age an accepted stamp may be |
| `SSRUnavailableFallbackPolicy` | PresentTimeTrace | What happens when no trustworthy historical answer exists (C4D proposal) |
| `MaxRewindAllowanceMs` | 200 | Hard cap applied to every source. Never clamped: anything over the cap is unavailable |

## Wire format and bandwidth

- **Down (server → clients):** `AEnclaveCharacter::SSRServerCapsuleObservation` is now a packed whole-millisecond stamp, an estimated 3–4 bytes per character update, down from an estimated ~20 bytes with a `double` plus capsule geometry (not yet measured with a network profiler). The geometry is serialized only in builds with `ENCLAVE_INCLUDE_DEBUG_CODE`, and both endpoints must be the same build configuration. In non-debug builds the property is switched off (`DOREPLIFETIME_ACTIVE_OVERRIDE_FAST`) unless SSR is active with a source that reads stamps.
- **Up (shot):** inside the existing GAS `FEnclaveGameplayAbilityTargetData_ShotIntent`, 1 bit when absent, or one actor reference plus a packed stamp when present. No new RPC.
- **Client capture:** `CaptureClientSSRTimingEvidence()` runs one Weapon-channel line trace per shot. A stamp is only sent when the first blocking hit is a simulated-proxy character whose stamp arrived no more than 80 ms ago.

## Security notes

- An echoed stamp is accepted only if it maps to **exactly one recorded frame** of that target (`TryResolveRecordedStampMs`), lies within the rewind cap, and, in Hybrid, lies within −40/+60 ms of the anchor age. A cheater therefore gets at most about 100 ms of "backtrack" choice, and only on the target the shot is aimed at. Anything else silently falls back to the anchor.
- The echoed target is **timing evidence only**. The server still traces every eligible character, nearest entry wins, and present-time world cover still blocks (invariants 3 and 6).
- Fake lag (inflating RTT) affects every lag compensation scheme; `MaxRewindAllowanceMs` caps it. Not yet abuse-tested. That belongs to milestone 3.

## D11D-4 selection (`UEnclaveHitRegistrationSubsystem::SelectHistoricalHit`)

Applies only to validated remote shots while `IsServerSideRewindActive()`. Listen-host, AI and other authority-local shots stay present-time.

1. Verify the echoed stamp against the named target's history, then call `ResolveSSRTiming`.
2. `FEnclaveServerHistoricalEvaluator::EvaluateCapsules` (with a per-target time override for Hybrid).
3. `HistoricalHit` → a blocking `FHitResult` on the target's capsule component, at the historical point, with the historical surface normal, distance and time.
4. `HistoricalMiss`, including world-occluded → **terminal**. Never retried at present time. The result is the present-time world hit if the ray hits a wall, otherwise no hit.
5. `Unavailable` (timing out of window, any missing history, unprepared query) → `SSRUnavailableFallbackPolicy`.

`Enclave.Debug.SSRSelection 1` logs one `SSRSelection SERVER` line per decision; the analyzer scores these against labels.

## Retired

- `Enclave.Debug.SSRProductionReadiness` and its `presentation_time_unavailable` audit.
- `EnclaveSSRTemporalUncertainty::FAssessment::IsAuthorizedForDamage()` (always false).

The timing sweep, candidate comparison, historical probe and D11C claim diagnostics remain. The sweep gained `Enclave.Debug.SSRTimingSweepStartMs` for negative offsets.

## Known limits / next (milestone 3)

- The shooter's pose is not rewound; the server still validates the shot origin against the present pose (Stage B tolerances).
- Missing history on **any** character makes the shot unavailable, which means present-time fallback (conservative, population-wide, as before). A per-target rule is still open.
- The anchor offset and stamp window were calibrated on a single-machine listen server. Re-measure on a dedicated server under jitter, loss and asymmetric latency, and add abuse cases (manipulated stamps, fake lag, crouch/teleport around fallback).
- SkeletalHitboxes stays present-time.
- Re-run: `Tools/Scripts/SSR/RunSSRSession.ps1 -LagCompensation ServerSideRewind -TimingSource Hybrid -RttMs 100`.
