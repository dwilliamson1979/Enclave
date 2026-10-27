# D11D-3C — Timing candidate comparison (non-authoritative)

## What is implemented
- New optional `Enclave.Debug.SSRTimingCandidates` server-only shadow diagnostic evaluates two independently bounded candidate historical times against the same validated server shot ray and all eligible historical targets.
- Server anchor = server receipt - server-observed RTT - assumed extra smoothing.
- Shot-clock candidate = untrusted client estimated server-time-at-fire - assumed extra smoothing.
- The adjustable `Enclave.Debug.SSRTimingSmoothingMs` parameter is a test assumption, **not an observed presentation delay**.
- Logs validity, timestamp separation, query outcome, unavailability reasons, comparability, and agreement; **does not affect FHitResult or damage**.

## Important limitations / unresolved design choices
- No confirmed server knowledge of per-target client render/interpolation timing; neither candidate is eligible for production authority.
- Network RTT is neither a measured one-way delay nor proof of when a target was rendered.
- Client shot-clock data is client-controlled even if the server plausibility-validates it.
- History capsule snapshot time does not necessarily equal actual rendered mesh pose time.
- The moving-cover obstruction query remains current-world, not historical.
- Establish target representation provenance, bounded latency policy, and exploitation safeguards before D11D-4.

## Acceptance
1. Apply the `Source/` overlay, compile Development Editor.
2. `Automation RunTests Enclave.Combat.SSR` must pass.
3. In 3-player Listen Server PIE, using the **server console**, run:
   `Enclave.Debug.SSRProductionReadiness 1`
   `Enclave.Debug.SSRTimingCandidates 1`
   `Enclave.Debug.SSRTimingSmoothingMs 0`
   `Enclave.Debug.SSRTest.Start 2`
4. Allow 25–40 shots. End with `Enclave.Debug.SSRTest.Stop`, `Enclave.Debug.SSRTimingCandidates 0`, `Enclave.Debug.SSRProductionReadiness 0`.
5. Expect one `SSRTimingCandidates SERVER` line for every validated remote shot; monitor candidateDeltaMs, outcomesAgree, and unavailable reasons. Continue seeing `SSRProductionReadiness ready=0 reason=presentation_time_unavailable` and no damage authority.
6. Repeat with **artificial network latency**, and separately run with `Enclave.Debug.SSRTimingSmoothingMs 30` to see sensitivity. The 30ms is a scenario parameter, not an approved policy.

## Exit gate
This phase produces *comparative evidence*, not an authoritative timing model. Do not enable SSR gameplay damage based on these two candidate clocks. Decide whether to add server-verifiable presentation metadata or choose an explicitly limited bounded estimation policy with anti-exploitation constraints and broad regression evidence.
