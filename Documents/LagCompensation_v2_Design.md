# Lag Compensation v2: Central Manager and Skeletal Rewind

Status: draft for discussion (2026-10-31). Not started. Planned as its own
milestone after M4 (the match loop), so it can be measured in real matches.

## Goal

Server-side rewind (SSR) that:

1. Works with **per-bone hitboxes**, so headshots and limb hits are lag compensated.
   Today SSR only works in Capsule mode; SkeletalHitboxes shots are always present-time.
2. Scales to **26-32 players and bots** on a dedicated server at 60 Hz.
3. Has **one owner** for "where was everything at time T", instead of history
   spread across per-character components and queries that scan the world.

The reference model is the Source engine's lag compensation manager (TF2,
CS:GO): the server records every player's hitbox history centrally and, per shot,
rewinds only the players who could be hit.

## Why now (drivers)

| Driver | Consequence for the design |
|---|---|
| Headshots are expected in competitive shooters | Bone history and a hitbox narrowphase |
| Bots will likely be needed at launch | Many server-local targets; cheap registration |
| Possible 26-32 player servers | Broadphase per shot; a recording budget |
| Every shot already asks a world-wide question | A world-level owner fits the query |

## What exists today

| Piece | Role | Fate in v2 |
|---|---|---|
| `UEnclaveTargetHistoryComponent` (424 lines) | Per-character capsule ring buffer: ~20 frames at 60 Hz, segment breaks, death boundary, interpolation within segments | Logic kept, moved into a plain struct owned by the manager; component retired |
| `UEnclaveHitTargetComponent` | Applies the hit detection mode to the character; publishes SSR timing stamps | Becomes the character's registration point with the manager and forwards its events |
| `UEnclaveCombatPolicyComponent` | Match rules: hit detection mode, SSR settings, rewind cap | Unchanged |
| `FEnclaveHistoricalHitQuery` | Per shot: loops every `AEnclaveCharacter` in the world, two `FindComponentByClass` lookups each | Replaced by registry iteration plus broadphase |
| `FEnclaveServerHistoricalEvaluator`, `FEnclaveHistoricalCapsuleIntersection`, `FEnclaveSSRHistoricalEligibility` | Capsule rewind evaluation, ray/capsule math, death eligibility | Kept; capsule test becomes the middle stage |
| `UEnclaveHitRegistrationSubsystem` (570 lines) | Validates the shot, chooses historical vs present-time, runs traces, debug diagnostics | Keeps shot validation and selection; asks the manager for rewound geometry |
| `EnclaveSSRTimingPolicy` | Chooses the rewind time (ServerAnchor default: RTT - 25 ms) | Unchanged |
| Tests | 8 SSR unit tests; layout, abuse, matrix and edge-case suites | All carried forward; new cases added |

Known limits of v1:

- Capsule only. Limbs stick out of the movement capsule, and the capsule has no head.
- On the dedicated server the third-person mesh uses `AlwaysTickPose`: animation
  ticks, but bone transforms are not refreshed. SkeletalHitboxes mode switches to
  `AlwaysTickPoseAndRefreshBones` while it is active.
- Queries scale with every character in the world, regardless of the shot.
- The query still reads the lifecycle component directly for "dead now".

## Architecture

```
UEnclaveLagCompensationManager   (UWorldSubsystem, server only)
  registry of targets  ── one FEnclaveLagCompTarget per registered actor
     FEnclaveTargetHistoryBuffer   capsule frames (today's logic, as a plain struct)
     FEnclaveHitboxHistoryBuffer   per-frame hitbox transforms
  Record()   one pass per server tick, after animation and physics
  Query()    broadphase -> capsule -> hitboxes, at a rewind time

UEnclaveHitRegistrationSubsystem  (existing)
  validate shot -> resolve rewind time -> Manager.Query() -> world occlusion -> result

UEnclaveHitTargetComponent        (existing, on each character)
  registers / unregisters with the manager
  forwards death, revive, possession, teleport and mode changes
  still applies the hit detection mode and publishes stamps
```

Why a new subsystem rather than growing `UEnclaveHitRegistrationSubsystem`:
the manager answers "what was where, when"; the registration subsystem answers
"is this shot valid, and what did it hit". Each is easier to test alone, and the
registration subsystem is already the largest file in Combat.

The manager exists only on the server (`ShouldCreateSubsystem` returns false for
clients), so clients carry no empty instance.

### Plain structs instead of components

The history logic becomes `FEnclaveTargetHistoryBuffer`, a plain C++ struct with
no actor or world dependency. Its rules (segment breaks, death boundary,
interpolation, trimming) can then be unit-tested directly with synthetic frames,
which today requires a component. The manager owns one buffer per target.

### Hitboxes

Each frame stores one transform per hitbox body (position and rotation, about
28 bytes). Shapes are fixed per mesh and stored once.

| Size estimate | Value |
|---|---|
| Hitbox bodies per character | about 20 (the template `PA_Mannequin`; confirm in P3) |
| Per frame | about 560 bytes |
| Per target (~20 frames at 60 Hz) | about 11 KB |
| 32 targets | about 360 KB |

Memory is not the constraint. Keeping bone poses current on the server is (see
Recording).

**Hitbox source (decision 1).** Two options:

- Use the Physics Asset bodies (what SkeletalHitboxes mode uses today). No new
  authoring, but hitboxes are coupled to ragdoll tuning.
- A dedicated hitbox set: a data asset listing bone, shape and zone (head, torso,
  arm, leg), seeded from the Physics Asset. This is what most competitive
  shooters do: designers control head size and zones without touching ragdolls.

Recommendation: a dedicated hitbox set (`UEnclaveHitboxSet`). It also carries the
hit zone that damage multipliers need.

### Recording

One manager pass per server tick, ordered after animation and physics
(`TG_PostUpdateWork` or a dedicated tick function), records each target's capsule
and hitbox transforms with the server time. This replaces one ticking component
per character.

Bone transforms on a dedicated server must be refreshed for the recorded pose to
mean anything (decision 2):

- Always refresh bones for every character on the server
  (`AlwaysTickPoseAndRefreshBones`). Simplest and correct; costs animation
  evaluation per character per tick.
- Refresh at a reduced rate and interpolate. Cheaper; adds pose error.

Recommendation: always refresh, then measure at 8, 16 and 32 targets before
considering anything cheaper.

### Query (per shot)

1. **Broadphase.** For each registered target, test the ray against a bound of
   its recorded motion around the rewind time, inflated to cover hitboxes that
   extend past the capsule (arms). Most targets drop out here.
2. **Capsule stage.** The existing rewound-capsule intersection on the
   candidates, with an inflated capsule so limbs are never culled.
3. **Hitbox stage.** Interpolate the hitbox transforms at the rewind time within
   a segment (never across a break), and intersect the ray with each hitbox. The
   nearest hit gives the bone and zone.
4. **World occlusion.** As today: a world trace up to the entry point.

The result is a normal `FHitResult` with `BoneName`, plus the hit zone.

Present-time skeletal shots (listen host, bots, SSR disabled) can use the same
code with the newest recorded frame instead of live Physics Asset traces, so
there is one hit path rather than two (decision 3).

### Timing stamps

ServerAnchor (the default) needs no client evidence and is unchanged. Hybrid and
EchoedStamp stamp verification moves into the manager. M3 found no accuracy
benefit from echoed stamps once frame pacing was controlled, and they cost
bandwidth and a ~14 ms backtracking window. v2 is a natural point to retire them
(decision 4), removing the replicated stamp from `UEnclaveHitTargetComponent`.

### Bots

Bots are server-local AI controllers. As targets they register like players. As
shooters they stay present-time, as authority-local shots do today: they have no
network latency to compensate.

### Performance budget

A `STATGROUP_EnclaveLagComp` stat group with cycle counters for recording and for
each query stage. Proposed budgets, to be confirmed by measurement:

| Work | Proposed budget at 32 targets, 60 Hz |
|---|---|
| Recording (all targets) | 0.3 ms per tick, excluding bone refresh |
| Bone refresh (all targets) | measured in P3; this is the unknown |
| One shot query | 0.05 ms |

## Risks

- **Server pose vs client pose.** Bone rewind is only as good as the match between
  the server's animation and what each client rendered. Aim offset (pitch),
  locomotion blend, montages and IK must produce the same pose on both. P3 adds a
  diagnostic, like the M3 capsule diagnostics, in which the shooter's client
  reports the proxy's bone positions at fire time for comparison with the
  server's recorded pose.
- **Bone refresh cost** at 32 characters on a dedicated server is unmeasured.
- **Harness capacity.** The session harness runs up to 12 headless clients.
  32-target measurements need server-spawned moving dummy targets, or bots.

## Migration plan

Each phase keeps every existing suite green and is its own commit series.

| Phase | Work | Proof |
|---|---|---|
| P0 | Extract `FEnclaveTargetHistoryBuffer` inside the existing component; no behavior change | New buffer unit tests; layout and abuse suites unchanged |
| P1 | Manager and registry; `UEnclaveHitTargetComponent` registers and forwards events; query iterates the registry; history component retired. Capsule only | A/B against the previous commit on all suites |
| P2 | Broadphase; moving dummy targets for scale tests | Query cost at 8 / 16 / 32 targets; accuracy unchanged |
| P3 | `UEnclaveHitboxSet`; record hitbox transforms (not yet queried); server bone refresh | Recording and bone-refresh cost; pose-match diagnostic |
| P4 | Hitbox stage in the query; zone in the hit result | New zone-aware layouts: hit rates by zone at mid and high RTT |
| P5 | Damage by zone (headshot multiplier) in the damage pipeline | Gameplay tests |
| P6 | Cleanup: retire stamp paths if decided; update the SSR docs | Bandwidth A/B |

## Testing

- **Carried forward:** the 8 SSR unit tests; the layout, abuse, matrix and edge-case suites.
- **New unit tests:** history buffer rules, broadphase bounds, ray vs hitbox
  shapes, transform interpolation within and across segments.
- **New suite cases:** zone-aware aiming (the shooter aims at the head or the body
  and scoring is per zone), and scale runs with dummy targets.

## Decisions to make

| # | Decision | Recommendation |
|---|---|---|
| 1 | Hitbox source: Physics Asset or a dedicated hitbox set | Dedicated set, seeded from the Physics Asset |
| 2 | Server bone refresh: every tick or reduced rate | Every tick; measure first |
| 3 | Present-time skeletal shots through the manager | Yes, one hit path |
| 4 | Keep Hybrid / EchoedStamp | Retire in P6 unless new measurements show a benefit |
| 5 | Rewind cap for skeletal rewind | Same 200 ms cap |
| 6 | Who writes which phase | To discuss (see below) |
| 7 | Carry the shot's fire time (the rewound time) into `FEnclaveDamageRequest`. Today the match ends on the first kill the server *receives* at the score limit (M4 U3), so a lower-ping team can win a last-kill race the other team fired first. With the fire time, the match could keep accepting kills for a grace window (the rewind cap) once the limit is reached, then rank them by fire time | Yes, with P5 (the damage pipeline). The match's grace window is then its own small item on top. Fire time is a client claim, bounded by the rewind checks |

## Work split (to discuss)

Candidates for you, as self-contained pieces that teach a concept:

- `FEnclaveTargetHistoryBuffer` extraction and its unit tests (P0): learning the
  history rules by moving them.
- The broadphase as a pure, unit-tested function (P2).
- `UEnclaveHitboxSet` and the zone damage table (P3, P5): data-driven gameplay.

Candidates for Claude: the manager and migration (P1), recording and the bone
refresh measurements (P3), the hitbox query stage and the zone-aware test harness (P4).

## Out of scope

- Projectile weapons. They need a different approach (simulated on the server
  with their own timing) and get their own design when a projectile weapon exists.
- Client-side hit authority ("favor the shooter" by trusting the client).
- Vehicles and non-character targets, beyond keeping registration generic enough to add them.
- Replays.
