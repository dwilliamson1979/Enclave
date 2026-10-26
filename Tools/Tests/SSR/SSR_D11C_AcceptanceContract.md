# Enclave — SSR D1.1C acceptance contract (locked before testing)

2026-10-24. Development-only **shadow** evaluation. No authoritative damage changes. Review as a deliberate engineering proposal; once the full runs start, do not revise thresholds in response to results.

## Scored reference

The shooter captures a **mesh-anchored approximation of the client target capsule** at fire time. This is not the rendered skeletal silhouette. Inner radius/height shrinks by 10 cm for `clear_hit`; `clear_miss` lies 10–40 cm outside; `near_miss` is the ±10 cm band and does not influence classification accuracy. Very distant shots are excluded and counted separately. Client labels are untrusted **test metadata only**.

- **Primary candidate:** snapshot stamp only. Always select this outcome for the first gate; do not pick the better policy per shot.
- **Challenger:** snapshot stamp + locally measured elapsed time. Report alongside primary; do not substitute on a failed primary shot.
- Missing history and rejected timing **count using actual present-time fallback** in accuracy denominators. No silently dropped eligible server-side classifications.
- **Clear hits**: at least 99% of ordinary (non-cover) clear-hit shots are hits.
- **Clear misses**: no more than 1% of ordinary (non-cover) clear-miss shots are hits.
- **Sample minimum per emulated RTT band:** at least 100 clear hits and 100 clear misses overall (25 per category in each of stationary/strafe/reverse/crouch). Do not declare a pass with fewer samples.
- **Rejection/unavailable threshold:** <= 5% of non-stationary valid classified shots (`rejected_time`, `missing_history`, or `query_unavailable`); report unresolvable claims separately as failures. Verified `stationary_present` is an **intentional present-time fallback**, reported independently and never called successful historical SSR.

## Timing policy locked for feasibility run

- Diagnostic client update age **0 to 80 ms**; stale updates produce an invalid timestamp claim and are scored through fallback.
- Server independently enforces stamp not in the future (0.5 ms float tolerance), 200 ms GameMode rewind budget, `age <= observed RTT + 50 ms` (plus 0.5 ms tolerance), and the `stamp + elapsed <= receipt` check.
- The 80 ms limit is based on observed ~17/34/50 ms update-age clusters, **not advertised NetUpdateFrequency**. It is a policy setting for this test, not a shipping protocol or a proven safe anti-cheat bound. No adjustment after looking at results.
- Stationary special case: no fresh snapshot required only when server-side history independently shows <= 1 cm movement over 100 ms with a midpoint check, compatible capsule shape and current low velocity. This is **present-time fallback**, not rewind. Its hit/miss outcome remains counted.
- The engine native float timestamp is retained **only for feasibility**. Its replacement and movement-snapshot pairing still require separate production protocol review.

## Scenario labels and expected cover rule

The shooter sets `Enclave.Debug.SSRScenario` before each run: `1 stationary`, `2 strafe`, `3 reverse`, `4 crouch`, `5 static-cover`; `0` is untagged and fails final reporting. Scenario labels represent test setup, not independently verified movement classifications. Manually verify the requested movement occurred and retain the session recording/log.

**Cover policy:** immutable static world cover along the historical shot segment blocks a historical capsule hit, even if the target is visibly present behind that cover. Dynamic doors, newly spawned/destructible cover, and cover that changes between shot and receipt are outside this slice and must be listed as unsupported, NOT silently passed. When testing `scenario=5`, aim through a static wall known to block `ECC_Weapon`, with the target capsule geometrically behind the wall. Require at least 10 *actually occluded* (`stampOccluded=1` AND `elapsedOccluded=1`) clear-hit claim samples per RTT band and zero incorrect final hits for either policy. `worldBlocked=1` alone is not evidence: it might be backdrop beyond the target. Cover samples do not count toward ordinary clear-hit accuracy (they have deliberately different expected outcomes).

## Test plan

- Run independently at **50, 100 and 150 ms intended RTT**, recording actual `rttMs` distribution for each band. Classify by configured emulation run, do NOT re-bin individual samples based on noisy PlayerState ping.
- Use **three PIE players** for the acceptance configuration. For clear-hit/miss sampling, only the designated shooter and target should lie on the shot ray; third player is an independent potential occluder (production nearest-target behavior remains OUTSIDE scope of this test).
- Each RTT band: stationary, normal strafe, direction reversal, crouch, then cover, labeled as above. The debug auto-strafe rig contains reversals but `scenario=3` must be observed near actual reversal windows; otherwise the 'reverse' requirement is not validated even if logs are tagged.
- Console on shooter and listen server: `Enclave.Debug.SSRGameplayGate 1`. Shooter: `Enclave.Debug.SSRAutoSeed 1729`, `Enclave.Debug.SSRScenario <scenario>`, `EnclaveDebugSSRAutoFire 1`; stop with `EnclaveDebugSSRAutoFire 0`. Target: `Enclave.Debug.SSRStrafeDuration 180`, `EnclaveDebugSSRStrafe 1` for moving cases. Damage suppression is scoped to active debug strafe, not always-on. Stationary and crouch need safe target health configuration and normal movement/lifecycle validity.
- Use `python analyze_d11c.py --target-rtt 50 logs_50.txt` for one band (also 100 and 150). Concatenate same-RTT scenario session logs for reporting. Analyzer treats rejections as scored outcomes. `--json` produces machine-readable results.
- **Pass** on hit/miss accuracy, sample coverage, rejection and cover gates **for all three RTT bands**, followed by separate network/CPU and abuse review. **Fail** means reduce supported RTT range or defer SSR; no exploratory clock-patch spiral.

## Performance budgets (not proven by this analyzer)

- No extra shot RPC and no replication of history. Scenario tag adds **one byte only in Debug/Development** to the existing GAS shot-claim payload when enabled.
- No increase to normal movement update frequency. Native debug movement stamp forcing remains experimental; remove/revisit before production.
- Compare additional server CPU and network throughput with SSR shadow disabled in a **dedicated-server stress test**: preliminary budget <=5% incremental server frame-time and <=5% incremental outbound bandwidth. Query `shadowCpuMs` is not full game-thread/packet overhead.
- Shipping gate requires dedicated-server, multi-target nearest-hit, security, and network-profile testing. D1.1C alone cannot authorize SSR in production.

For stationary/crouching samples, the listen-server target may run `EnclaveDebugSSRProtect 1` while the gameplay gate is active; disable explicitly with `EnclaveDebugSSRProtect 0` at the end. The debug command is server-local, nonreplicated, and does not affect the historical query.
