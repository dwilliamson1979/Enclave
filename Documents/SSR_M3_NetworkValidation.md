# Milestone 3: SSR network validation (in progress)

**Date:** 2026-10-28. Dedicated server, authoritative SSR (`?LagCompensation=ServerSideRewind`), Capsule mode, Hybrid timing unless noted. Earlier stage gates (such as D11C) are used as reference points, not pass/fail requirements.

## Setup

- `Tools/Scripts/SSR/RunSSRSession.ps1 -Dedicated`: `UnrealEditor.exe -server` (headless) plus a shooter client and a strafing target client on one machine. The test subsystem now ticks itself and needs no host player.
- Network emulation is outgoing-only per process: `-ServerLagMs`, `-ClientLagMs`, `-ShooterLagMs` (fake lag), `-JitterMs` (`PktLagVariance`, in order) and `-LossPct`. Results report the **measured** median PlayerState RTT.
- `RunSSRMatrix.ps1` (network conditions) and `RunSSRAbuse.ps1` (cheat cases and bandwidth A/B) each run a warm-up session first. The first launch after a build hitches: it produced an 85.8% outlier in the first matrix run, and a rerun scored 100%.
- Scoring: about 120 clear-hit and 120 clear-miss labeled shots per run (90–150 s). **Single runs differ by up to ±5 points from noise alone**: two identical ServerAnchor runs gave 94/98 and 100/100. Comparisons between modes need repeated runs; the anchor-offset sweep is a paired comparison on the same shots and is reliable.

## Network matrix (Hybrid authoritative)

| Condition | Lag srv/cli | Jitter | Loss | RTT median | Clear hits registered | Clear misses missed | Fallbacks | Anchor sweep, combined % (d = −75/−50/−25/0/+25) |
|---|---|---|---|---|---|---|---|---|
| low (rerun) | 5/5 | 0 | 0 | 108 | 100 | 100 | 0 | 98.3 / 100 / 100 / 100 / 98.3 |
| mid | 25/25 | 0 | 0 | 112 | 96.7 | 100 | 0 | 84.6 / 96.7 / **98.8** / 92.1 / 79.6 |
| high | 50/50 | 0 | 0 | 167 | 97.5 | 96.7 | 0 | 86.2 / 96.2 / **99.2** / 92.9 / 69.2 |
| mid + jitter | 25/25 | ±15 | 0 | 121 | 93.3 | 95.0 | 0 | 86.2 / 95.0 / **96.2** / 89.2 / 75.4 |
| mid + loss | 25/25 | 0 | 2% | 106 | 94.9 | 95.0 | 0 | 81.0 / 93.2 / **96.2** / 91.6 / 79.3 |
| asym, slow down | 45/5 | 0 | 0 | 133 | 100 | 100 | 0 | 98.3 / 100 / 100 / 99.6 / 99.2 |
| asym, slow up | 5/45 | 0 | 0 | 142 | 100 | 100 | 0 | 97.1 / 99.6 / **100** / 99.2 / 95.8 |

Findings:
1. **Anchor offset: the default is now −25 ms (was −50 ms).** In the paired sweep, −25 is as good as or better than −50 in every dedicated condition. The −50 ms value came from a listen server, where the host's frame time inflates the measured ping. The same lag measures about 110 ms RTT on dedicated versus about 160 ms on listen.
2. **Jitter and loss are the weak spots**, costing 3–7 points on both hits and misses, under every timing source. These are the conditions real players see most, so they need more work.
3. **Asymmetric latency is harmless.** The anchor uses round-trip time, and the echoed stamp doesn't depend on direction.
4. **No present-time fallbacks** in any condition.
5. **Hybrid versus ServerAnchor is unresolved.** The differences are within run noise. Hybrid stays the default until a repeated comparison (several runs per mode under jitter and loss) shows a real difference either way.

## Abuse cases (mid condition, ~105–115 ms RTT unless noted)

| Case | What the cheating shooter sends | Stamps sent/verified/used/rejected | Clear hits / misses | Outcome |
|---|---|---|---|---|
| baseline | honest | 93/93/87/6 | 100 / 100 | Used stamps sat −6 to +46 ms from the anchor age |
| backtrack 50 ms | a real received stamp ≥ 50 ms older | 101/101/**0**/101 | 97 / 95 | All rejected by the Hybrid window; shots used the anchor |
| backtrack 100 ms | ≥ 100 ms older | 100/100/**0**/100 | 100 / 100 | All rejected |
| backtrack 150 ms | ≥ 150 ms older | 100/99/**0**/99 | 93 / 96 | All rejected |
| forged | latest + 7 ms (not a recorded frame) | 101/**0**/0/0 | 92 / 100 | Never verified; anchor used |
| future 50 ms | latest + 50 ms | 100/**0**/0/0 | 100 / 100 | Never verified; anchor used |
| fake lag | shooter uplink +75 ms (RTT ~192) | 94/94/84/10 | 98 / 98 | Rewind follows the real RTT, as for any honest high-ping player; capped at 200 ms |

- Backtracking buys at most the gap between honest stamps (up to +46 ms) and the +60 ms late tolerance, about **14 ms**. Tightening the late tolerance to about +50 ms would shrink that further, at the cost of rejecting a few honest stamps under jitter.
- Fake lag isn't a stamp exploit; it is the price of any lag compensation. The defenses are `MaxRewindAllowanceMs` (200 ms) and, later, kicking players whose ping is far above the playlist limit.
- Not yet tested: lag switching (bursty uplink), and whether a stamp from target A works for target B. B's frames share A's timestamps, which is equivalent to an honest stamp at that time, so no advantage is expected.

## Bandwidth

Server outgoing bytes per client over the session window. A dedicated server has no host character, so each client receives exactly one other character.

| Case (30 Hz) | Server out B/s per client |
|---|---|
| First build: stamps replicated | 2718 |
| First build: stamps off (`Enclave.Debug.SSRReplicateStamps 0`) | 2224 |
| **Slimmed stamp: stamps replicated** | **2260** |
| Slimmed stamp: stamps off | 2214 |

The first measurement (+490 B/s per character) was mostly **debug-build traffic**: Development builds also serialized the diagnostic capsule geometry (quantized center plus two floats). Two fixes:
- The wire format is now the packed millisecond stamp only, in every build, so Development measurements equal Shipping. The two diagnostic logs that compared geometry now compare against the proxy's position at receipt.
- A new stamp is published only when the capsule moved more than 0.5 cm, changed size, or crossed a discontinuity. An idle character keeps its old stamp and stops replicating it. With no fresh stamp the shooter simply sends none, and Hybrid uses the anchor.

Result: stamps cost **about +46 B/s per replicated character** (~2%). At 12 players that's about +0.5 KB/s per client.

## Server tick rate (30 Hz vs 60 Hz)

The project inherits `NetServerMaxTickRate=30` from `BaseEngine.ini`. History, movement updates and stamps are all at the server tick rate. Override per run with `RunSSRSession.ps1 -ServerTickRate 60` (passes `-ini:Engine:[/Script/OnlineSubsystemUtils.IpNetDriver]:NetServerMaxTickRate=60`).

Same four conditions, Hybrid authoritative, ~120 + 120 labeled shots each:

| Condition | 30 Hz: RTT / hits / misses | 60 Hz: RTT / hits / misses |
|---|---|---|
| mid | 114 / 97.5 / 97.5 | 98 / 100 / 100 |
| high | 145 / 100 / 100 | 152 / 100 / 100 |
| mid + jitter ±15 | 119 / 100 / 100 | 109 / 100 / 98.3 |
| mid + loss 2% | 112 / 98.3 / 98.3 | 99 / 96.6 / 99.2 |

| Cost (2 players) | 30 Hz | 60 Hz |
|---|---|---|
| Achieved server tick | 30.0 | 59.9 |
| Server CPU, whole run incl. startup | 0.16 cores | 0.21 cores (+33%) |
| Server out per client | 2.66 KB/s | 4.12 KB/s (+55%) |

- **Accuracy:** no difference that clears run noise. Both rates sit at 97–100% here.
- **Latency:** 60 Hz cut measured RTT by 10–16 ms in 3 of 4 conditions at the same emulated lag, consistent with halving the wait for the next server tick. In "high" it measured 7 ms higher, which looks like PlayerState ping noise. Lower latency is felt by players directly, whether or not SSR is involved.
- **Timing tolerance:** at 60 Hz the anchor sweep is flat (≥99%) from −50 to 0 ms in mid and high. 30 Hz is narrower, so 60 Hz is more forgiving of a wrong offset.
- **Cost:** bandwidth scales with the update rate (+55%). CPU is small in absolute terms with two players and must be re-measured with a realistic player count.
- **Not changed.** The default stays 30 Hz until the tick rate is chosen deliberately (see the recommendation in the milestone summary).

## 120 Hz

| Condition | RTT | Hits / misses | Server Hz | Server CPU cores | Out B/s per client |
|---|---|---|---|---|---|
| mid | 90 | 100 / 100 | 119.5 | 0.31 | 4396 |
| high | 152 | 100 / 100 | 119.5 | 0.31 | 4366 |
| mid + jitter | 99 | 99.2 / 100 | 119.1 | 0.30 | 3668 |
| mid + loss | 91 | 98.3 / 100 | 119.5 | 0.31 | 4319 |

From 60 to 120 Hz, bandwidth barely moves (about 4.2 KB/s), because per-character update frequency becomes the limit. Measured RTT drops by about 8 more ms at mid, and CPU rises about 1.5× over 60 Hz. Accuracy is the same at 30, 60 and 120 Hz within noise. The anchor sweep is flat (≥98.8%) from −25 to 0 ms at 120 Hz.

**Recommendation: 60 Hz.** It gets most of the latency gain for moderate bandwidth and CPU. Revisit 120 Hz with a realistic player count on production-like server hardware.

## Frame pacing: the throttled shooter

The client `SSRTest CLIENT_FPS` logging showed that, with uncapped clients, **the shooter ran at ~31–33 FPS (about 40% of frames over 33 ms) while the target ran at ~186 FPS.** Both game windows share one GPU (RTX 3080); the focused, uncapped target window took it, and the background shooter starved. Raising process priority didn't help; capping both clients did (`t.MaxFPS 60`: both at 59.9 FPS, 1 slow frame in 1,200).

**Every session before this fix scored against a throttled, irregular shooter.** That plausibly explains the two 85–88% outliers (both methods shifted together) and much of the ±5-point run-to-run spread. `RunSSRSession.ps1` now caps clients at 60 FPS by default (`-ClientMaxFps`). The matrix adds `mid_shooter30fps`, a steady 30 FPS shooter, as a deliberate condition.

## Hybrid vs ServerAnchor, frame pacing controlled (30 Hz server, 60 FPS clients)

| Condition | Hybrid: hits / misses | ServerAnchor: hits / misses | Same shots in the Hybrid run: Hybrid vs anchor −25 (combined %) |
|---|---|---|---|
| mid | 98.3 / 100 | 100 / 100 | 99.2 vs 99.6 |
| high | 100 / 100 | 100 / 100 | 100 vs 100 |
| mid + jitter | 98.3 / 100 | 95.8 / 98.3 | 99.2 vs 99.6 |
| mid + loss | 92.5 / 99.2 | 95.8 / 100 | 95.9 vs 98.3 |
| shooter at 30 FPS | 99.2 / 100 | 99.2 / 100 | 99.6 vs 100 |

The last column is a paired comparison (the anchor sweep runs on the same shots Hybrid decided). **The anchor at −25 ms matched or beat Hybrid in every condition**; Hybrid was worst under packet loss. A steady 30 FPS shooter did not hurt either method. With frame pacing controlled, the echoed stamp shows no accuracy benefit, while it still costs bandwidth, adds a ~14 ms backtracking window, and adds code paths.

## Decisions (2026-10-28)

- **Default timing source: ServerAnchor** (`RTT − 25 ms`). It matched or beat Hybrid on the same shots in every controlled condition, and needs no client timing evidence, so there is no backtracking window. In non-debug builds stamps stop replicating unless a match selects Hybrid or EchoedStamp. Both remain selectable.
  - Context: most competitive shooters rewind to a client-reported command/view frame, capped by the server (e.g. Counter-Strike: command tick plus interpolation; Overwatch: command frame). That relies on a fixed global simulation tick, where one frame number describes everything the client sees. Unreal replicates each actor independently and extrapolates and smooths simulated proxies, so a per-target stamp did not beat a calibrated server estimate here. Revisit if characters move to a fixed-tick model (Mover / Network Prediction).
- **Dedicated-server tick rate: 60 Hz** (`Config/DefaultEngine.ini`, `NetServerMaxTickRate=60`). That's mainstream for competitive shooters (roughly 60–64 for Overwatch, Halo Infinite and CS2; Valorant runs 128; several battle royales run 20–30). Listen servers and PIE are unaffected; they tick at the host frame rate.
- **Test harness:** clients are capped at 60 FPS by default; the server tick rate comes from project config unless `-ServerTickRate` overrides it.

## Verification on the new defaults (ServerAnchor, 60 Hz server, 60 FPS clients)

| Condition | RTT median | Clear hits / misses | Fallbacks | Server Hz | Server CPU cores | Out B/s per client | Anchor sweep, combined % (−75/−50/−25/0/+25) |
|---|---|---|---|---|---|---|---|
| low | 49 | 100 / 100 | 0 | 59.9 | 0.21 | 3576 | unavail / 35.8* / 100 / 100 / 92.1 |
| mid | 89 | 100 / 100 | 0 | 60.0 | 0.21 | 3560 | 97.1 / 100 / 100 / 100 / 100 |
| high | 137 | 100 / 100 | 0 | 59.9 | 0.21 | 3260 | 97.1 / 100 / 100 / 100 / 100 |
| mid + jitter ±15 | 87 | 100 / 100 | 0 | 59.9 | 0.21 | 3094 | 98.3 / 100 / 100 / 100 / 98.8 |
| mid + loss 2% | 86 | 98.3 / 100 | 0 | 60.0 | 0.20 | 3601 | 75.3 / 93.3 / 99.2 / 99.6 / 93.3 |
| asym, slow down | 85 | 100 / 100 | 0 | 60.0 | 0.20 | 3604 | 74.6 / 93.8 / 100 / 100 / 94.6 |
| asym, slow up | 82 | 100 / 100 | 0 | 59.9 | 0.20 | 3627 | 78.8 / 94.6 / 100 / 100 / 95.0 |
| shooter at 30 FPS | 93 | 100 / 100 | 0 | 59.8 | 0.20 | 3432 | 79.6 / 96.2 / 100 / 99.6 / 91.7 |

\* At ~49 ms RTT, offsets of −50 ms or more put the target age at or below zero, which is unavailable rather than clamped. −25 ms sits in the flat −25…0 plateau in every condition.

With frame pacing controlled and the server at 60 Hz, measured RTT dropped by 20–60 ms compared with the earlier runs at the same emulated lag, and accuracy is at the top of the scale. Packet loss remains the only condition below 100%.

## Next in milestone 3

1. Measure tick rate and CPU with more players (needs more than two clients per machine, or a second machine).
2. More clients (multi-target occlusion, nearest-hit), lag switching, crouch/teleport around fallback.
3. Prune shadow diagnostics that are no longer needed.
