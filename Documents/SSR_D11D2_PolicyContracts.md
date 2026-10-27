# D11D-2 — SSR policy contracts (source-only)

**Status:** Contract definitions introduced; SSR remains disabled for authoritative damage. No gameplay dispatch, GameMode property, fallback, historical trace, or damage path was changed.

## Scope

`Source/Enclave/Public/Combat/EnclaveSSRPolicyContracts.h` introduces typed, always-compiled declarations for future SSR selection. Existing `EEnclaveLagCompensationMode` and `EEnclaveWeaponHitDetectionMode` remain independent.

- `EEnclaveSSRQueryOutcome`: separates historical hit, definitive historical miss, and unavailable historical evaluation.
- `EEnclaveSSRUnavailableReason`: records specific reasons a trustworthy historical result cannot be produced.
- `EEnclaveSSRUnavailableFallbackPolicy`: names **possible** future match policies; neither choice is wired up or approved by this stage.
- `EEnclaveSSRSelection`: distinguishes normal present-time policy from historical selection and an exceptional unavailable-history fallback.
- `FEnclaveSSRQueryAssessment` and `FEnclaveSSRSelectionDecision`: small, non-UObject audit and control-plane records. They are not `FHitResult` replacements.

## Normative invariants for future implementation

1. Reject invalid shot intent at the existing Stage B boundary; never use unavailable-history fallback to rescue an invalid shot.
2. `HistoricalMiss` is a terminal miss; never retry the live trace after a definitive historical miss.
3. World-obstructed historical geometry is a definitive miss (not `Unavailable`); never bypass cover by fallback.
4. `Unavailable` must include a reason. Only an approved match policy can choose a fallback. Do not invent/clamp a historical time to force availability.
5. SkeletalHitboxes must not silently use historical capsules. Unsupported geometry must follow an explicit supported mode or approved unavailability rule.
6. Server-only policy owns timing bounds, chosen evaluation strategy, and the final damage-eligible result; never promote client-designated targets or client-echoed timestamps directly to hit authority.
7. No historical query may move live Actors, infer bone/material data from capsule hits, or presume current world cover is historical cover.
8. A valid historical hit is not yet a complete `FHitResult`; conversion/integration is a later stage with separate tests.

## Outstanding decisions (intentionally not selected)

- Whether unavailable valid history produces no hit or executes a present-time trace; whether that choice is per-shot, per-target, or match-wide.
- How to classify discontinuity/respawn/teleport and what constitutes relevant missing history.
- Timing anchor and smoothing allowance, direction-of-latency asymmetry and RTT bound.
- Whether and when to allow authoritative rewind for capsule mode, and what to do when skeletal mode is requested.
- The complete production hit-result representation and world/dynamic-cover constraints.

## Integration checkpoint

D11D-3 should implement server-owned timing and candidate selection **without damage selection**, writing explicit reasons into an audit result. D11D-4 should introduce authoritative strategy dispatch only after fallback rules are approved and regression-tested.

## Compile & smoke check

1. Overlay `Source/Enclave/Public/Combat/EnclaveSSRPolicyContracts.h` into the repo (preserving relative directories).
2. Regenerate project files only if your setup requires it, then compile Development Editor in UE 5.8.
3. Confirm existing present-time firing and SSR shadow Scenario 2 still behave identically. There are no new console commands.
4. Verify no settings, defaults, or GameMode UI options changed.

Suggested commit after successful compilation: `refactor(ssr): define production policy and selection contracts`
