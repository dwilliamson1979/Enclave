# D11D-3 — Cohesive evaluation verification package

## Scope
This package combines deterministic tests for policy/timing/geometry primitives with a debug-only call site exercising the **actual server historical evaluator** on ordinary remote shots. Production mode and damage authority remain unchanged. No new network replication or RPCs.

## Security distinction
`Enclave.Debug.SSRHistoricalProbeAgeMs` is a **server operator test fixture**. It is not client-visible target presentation time and MUST NOT be used for SSR damage decisions. D11D-3C (trusted target-presentation-time design) remains blocked until a separately validated timing mechanism is established. We deliberately do not pass RTT estimates or client-selected historical targets to the evaluator.

## Run
1. Overlay `Source/` on the project, compile Development Editor.
2. Run automation tests in editor via `Automation RunTests Enclave.Combat.SSR` and inspect test output (or the Session Frontend automation UI).
3. 3-player Listen Server PIE, capsule hit detection mode, no network emulation initially. In server console:
   - `Enclave.Debug.SSRProductionReadiness 1`
   - `Enclave.Debug.SSRHistoricalProbe 1`
   - `Enclave.Debug.SSRHistoricalProbeAgeMs 50`
   - `Enclave.Debug.SSRTest.Start 2`
4. After 25–40 attempts: `Enclave.Debug.SSRTest.Stop`; disable both toggles.
5. Verify `SSRHistoricalProbe SERVER` appears for each accepted remote shot, reports server fixture age 50 ms, and reports historical results/unavailability without affecting present-time damage. Check `SSRProductionReadiness ready=0 reason=presentation_time_unavailable` remains true. Diagnostic intended hit/miss categories do **not** establish correctness of the 50 ms fixture; expect mismatches under motion.
6. Repeat with `Enclave.Debug.SSRHistoricalProbeAgeMs 250` (default rewind allowance 200 ms): every eligible shot must report `invalid_fixture_age` and never evaluate historical geometry.
7. Optionally compare higher RTT with fixture 50 ms, keeping the controlled sample intentionally unrelated to rendered target time.

## Known limits and deferred work
- No real client-visible timestamp authority; therefore this is **not** production SSR shadow prediction at true presentation time.
- World occlusion uses present-time world geometry; moving cover is not rewound.
- Missing history anywhere among eligible characters yields Unavailable; a subsequent broadphase/eligibility decision is needed before authority integration.
- The evaluator is intentionally isolated from authoritative FHitResult/damage; its result is not promoted.
- No Unreal toolchain available in this environment; package was statically checked, not compiled or executed here.

## Follow-up before D11D-4
Approve target-time provenance and maximum rewind policy; implement verified timing pipeline and comparative shadow diagnostics; then plan production result conversion and damage authority regression.
