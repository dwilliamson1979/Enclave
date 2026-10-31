# SSR Walkthrough: One Shot, From Click to Damage

A guided tour of Enclave's server-side rewind (SSR) as it exists at commit
`44fe192`. It follows one shot through the code, explains why each step exists,
and points to the milestone 3 measurements behind the numbers. File paths are
under `Source/Enclave/`.

## 1. The problem

The server owns the truth: positions, hits, damage. Every player sees other
players slightly in the past.

```
Server time ──────────────────────────────────────────────────────────►
  T0: target is at position A
        │  server → client (half the round trip)
        ▼
      client receives A, smooths it onto the screen
        │  the player sees A, aims at A, clicks
        │  client → server (the other half)
        ▼
                  server receives the shot at T0 + RTT (+ a little)
                  ...but by now the target has moved to position B
```

If the server tested the shot against B, where the target is *now*, a player with
110 ms of round-trip time who aimed perfectly would miss any moving target.
Unreal's default run speed is 600 cm/s, so in 110 ms a target moves about 66 cm:
roughly its own width.

**Server-side rewind:** the server keeps a short recording of where every
character was, and tests the shot against where the target was *when the
shooter saw it*. The server still decides everything; it just decides against
the right moment.

## 2. The pieces

| Piece | File | Role |
|---|---|---|
| Fire ability | `Private/Abilities/EnclaveGA_Fire.cpp` | Client: sends the shot. Server: receives, validates, commits, applies damage |
| Shot intent | `Public/Combat/EnclaveShotIntent.h` | What the client sends; what the server accepts |
| Hit registration | `Private/Combat/EnclaveHitRegistrationSubsystem.cpp` | Validates the shot; chooses historical or present-time; runs the traces |
| Weapon | `Private/Equipment/Weapons/EnclaveFirearmWeapon.cpp` | Builds the hitscan query (range); attaches damage to a hit |
| Combat policy | `Combat/Components/EnclaveCombatPolicyComponent` | Match rules: hit detection mode, SSR on/off, timing settings, 200 ms cap |
| Timing policy | `Private/Combat/EnclaveSSRTimingPolicy.cpp`, `EnclaveShotTiming.cpp` | Chooses the rewind time |
| Evaluator | `Private/Combat/EnclaveServerHistoricalEvaluator.cpp` | Guards, then runs the historical query |
| Historical query | `Private/Combat/EnclaveHistoricalHitQuery.cpp` | Rewinds each character's capsule and tests the ray |
| Capsule math | `Private/Combat/EnclaveHistoricalCapsuleIntersection.cpp` | Ray vs capsule |
| Death rule | `Public/Combat/EnclaveSSRHistoricalEligibility.h` | A corpse can't be hit at or after its death time |
| Target history | `Combat/Components/EnclaveTargetHistoryComponent` | The recording: each character's capsule over the last ~320 ms |
| Hit target | `Combat/Components/EnclaveHitTargetComponent` | Applies the hit detection mode to the character; publishes timing stamps |

## 3. One shot, step by step

### Step 1: the client fires (`UEnclaveGA_Fire::ActivateAbility`, non-authority branch)

On the shooter's machine, the ability:

1. Reads the camera position and direction (`GetActorEyesViewPoint`).
2. Fills an `FEnclaveShotIntent`:
   - `Origin` and `Direction`: where the player aimed.
   - `ClientServerTimeSeconds`: the client's estimate of server time.
   - `ShotSequence`: the GAS prediction key, to match the shot on the server.
   - `TimingTarget` and `TimingStampMs`: optional timing evidence (Step 6).
3. Sends it with GAS's target data RPC (`CallServerSetReplicatedTargetData`).
4. Plays the fire effect immediately, so the shot feels instant.

What the client does **not** do: decide whether it hit anything. It sends *what it
saw*, never *what it hit*. That one rule is what makes SSR safe: a cheating client
can lie about where it aimed (which the server checks), but it can't claim a hit.

### Step 2: the server receives (`ActivateAbility` on the server, then `OnShotTargetDataReceived`)

For a remote player, the server's copy of the ability starts, then waits for the
target data, with a timeout. When it arrives, `OnShotTargetDataReceived` checks
the structure: one payload of the right type, no NaNs, a unit direction, a sane
time. A malformed payload is rejected and the ammo refunded.

### Step 3: the server validates the shot (`ValidateClientShotIntent`)

The server compares the claim with what it knows:

| Check | Limit | Why |
|---|---|---|
| Sequence matches this activation | exact | The shot belongs to this ability activation |
| Shot age | at most 1.5 s old, at most 0.25 s "in the future" | Rejects replayed or forged times |
| Origin vs the server's view of the shooter's eye | within 225 cm | A client can't fire from somewhere it isn't |
| Direction vs the server's view rotation | within 110° | Catches nonsense; generous because the server's copy of view rotation lags |
| No wall between the server's eye and the claimed origin | trace | Can't shoot from behind geometry |

These limits catch impossible claims, not inaccurate aim. On success it builds an
`FEnclaveValidatedShotRay` and fills its timing context:

- `ServerReceiptTimeSeconds`: when the server got the shot.
- `ServerObservedRTTSeconds`: the shooter's round-trip time, from the
  PlayerState ping. This is the server's own measurement, not the client's word.
- `MaxRewindSeconds`: the policy's cap (200 ms).

### Step 4: commit and query (`CompleteAuthoritativeShot`, then the weapon)

The server commits the ability (spends ammo, starts the cooldown) and calls
`PerformHitDetection`, which asks the weapon. `AEnclaveFirearmWeapon::PerformFireHitDetection`
builds an `FEnclaveHitscanQuery`: start at the validated origin, end at the
weapon's range along the validated direction, plus the timing context. It calls
`RegisterHitscan`.

### Step 5: historical or present-time? (`RegisterHitscan`)

```
if SSR is active (policy says ServerSideRewind AND mode is Capsule)
   and the shot has validated client timing:
       SelectHistoricalHit      ← the rewind path
else:
       TraceCurrentGeometry     ← an ordinary line trace against "now"
```

Present-time shots are correct for: the listen-server host and bots (no network
delay to compensate), matches with SSR disabled, and SkeletalHitboxes mode (v1
records capsules only; see `LagCompensation_v2_Design.md`).

### Step 6: choosing the rewind time (`ResolveSSRTiming`)

This is the heart of SSR: **how long ago did the shooter see the target?**

With the default `ServerAnchor` source:

```
target age   = RTT + offset            (offset = −25 ms)
rewind time  = receipt time − target age
if target age > 200 ms  → unavailable (too far back to trust)
```

Why RTT: the state the shooter saw left the server half a round trip before they
saw it, and their shot took the other half to come back. So the target's age at
receipt is about one round trip.

Why −25 ms: Unreal's ping includes time each end spends waiting for its next
frame, so it slightly overstates the pure network delay. The offset is a measured
correction, not a theoretical one. Milestone 3 swept it on the same shots (mid
condition, ~112 ms RTT):

| Offset | −75 | −50 | **−25** | 0 | +25 |
|---|---|---|---|---|---|
| Combined accuracy | 84.6% | 96.7% | **98.8%** | 92.1% | 79.6% |

Being 50 ms wrong in either direction costs 15-20 points. Timing is everything.

**The other sources (for completeness).** `Hybrid` and `EchoedStamp` use the
timing evidence from Step 1: the client reports the server stamp the target it
was aiming at was displaying. The server only believes a stamp that names a frame
it actually recorded for that target (`TryResolveRecordedStampMs`), and Hybrid
only lets it move the time −40/+60 ms from the anchor. Milestone 3 found that with
frame pacing controlled, the stamp added no accuracy over the anchor, so
ServerAnchor is the default. The reason is Unreal-specific: each actor replicates
independently and simulated proxies are smoothed, so no single client frame
number describes everything on screen, unlike fixed-tick engines (Counter-Strike,
Overwatch).

### Step 7: rewinding and testing (`EvaluateCapsules`, then `FEnclaveHistoricalHitQuery`)

The evaluator first re-checks its inputs. These guards look redundant, but each
one closes a hole:

- Capsule mode only (the history has no bones).
- The requested time is within the cap. The cap is re-read from the policy, so a
  caller can't pass a larger one.
- **Clock drift:** the timing context must be fresh (server time now is within
  −0.05/+0.25 s of the receipt time). A stale context can't be reused later to
  authorize an old shot.

Then the query runs:

1. **World trace, ignoring every character.** Current character bodies must not
   block a ray aimed at *historical* positions. This trace finds walls only.
2. **For each character** (except the shooter):
   - The death rule: if the rewind time is at or after its recorded death time,
     skip it (a corpse can't be hit after death, but a trade kill on the living
     target just before can).
   - `TryGetCapsuleAtTime`: the capsule at the rewind time, interpolated between
     the two recorded frames around it, **only within one segment** (Section 4).
     No frames covering that time means *missing*.
   - Ray vs capsule (`TryIntersectSegment`). Keep the nearest hit.
3. **Occlusion:** if the wall is closer than the nearest target's entry point, the
   wall wins.
4. **Missing history anywhere makes the result unavailable.** If one character's
   history was missing, an apparent miss can't be trusted: that character might
   have been in the way. Declaring a miss anyway would let a bullet pass through
   someone.

### Step 8: the decision (`SelectHistoricalHit`)

| Outcome | What happens |
|---|---|
| Historical hit | A hit result at the rewound entry point, on the target's live capsule component |
| Historical miss | **Final.** Never retried at present time, which would let a miss become a hit. If a wall was in the way, the bullet stops at the wall |
| Unavailable | The fallback policy: a present-time trace (default) or no hit |

### Step 9: damage

1. The weapon wraps the hit with its damage amount and tags.
2. `UEnclaveGA_Fire::PerformHitDetection` builds an `FEnclaveDamageRequest`
   (amount, shooter's character as `SourceActor`, weapon as `DamageCauser`, the hit)
   and calls `UEnclaveDamageStatics::ApplyDamage`.
3. `ApplyDamage` finds the target's damage receiver (the health component), which
   applies the damage through GAS.
4. If health reaches zero, the health component broadcasts `OnHealthDepleted`; the
   lifecycle component enters the dead state. That fires
   `OnLifecycleStateChanged` (the target history marks the death time) and then
   `OnAvatarDied` (the match GameMode will credit the kill there).
5. `NotifyHitConfirmed` tells the shooter's client to show a hit marker.

## 4. The recording side

`UEnclaveTargetHistoryComponent` runs only on the server. Every tick, after physics:

- Records the capsule's center, rotation, radius and half-height with the server
  time. At 60 Hz that's about 20 frames covering the last ~320 ms (the 200 ms cap
  plus a 125 ms margin), never more than 128.
- Starts a new **segment** after a teleport, a capture gap over 150 ms, or a move
  faster than 2000 cm/s. Queries never interpolate across a segment boundary, so a
  teleport can't create a fake path between two places the character never was
  in between.
- Records crouch per frame: each frame has its own capsule size.

What resets it: possession changes and hit detection mode changes clear it. Death
keeps the pre-death frames (for trade kills) and stops recording the corpse.

`UEnclaveHitTargetComponent` applies the match's hit detection mode to the
character (capsule blocks weapon traces, or the mesh does) and, for Hybrid and
EchoedStamp, publishes the replicated timing stamp each time the history records
a frame in which the character moved.

## 5. Rules that make it hard to abuse

| Rule | Where | What it prevents |
|---|---|---|
| The client never claims hits | Step 1 | Fake hits |
| Validation limits | Step 3 | Shooting from impossible places or times |
| RTT is measured by the server | Step 3 | Lying about latency to get more rewind |
| 200 ms rewind cap | Steps 6 and 7 | Unbounded rewind for very high ping or fake lag |
| Fresh timing context only | Step 7 | Reusing old timing to authorize a late shot |
| World-only occlusion trace | Step 7 | Live bodies blocking historical shots, or shots through walls |
| Missing history means unavailable | Step 7 | Phantom pass-through misses |
| Historical misses are final | Step 8 | Turning a rewind miss into a present-time hit |
| Death boundary | Step 7 | Hitting corpses; trade kills still work |
| Stamps must name a recorded frame, within a bounded window | Step 6 | Forged or backtracked stamps (Hybrid) |

Milestone 3 tested the cheats directly (`RunSSRAbuse.ps1`): forged and future
stamps never verified, large backtracks were all rejected, and a lag switch made
the cheater *worse* (bursts inflate the server's RTT estimate, so their shots are
rewound to the wrong time).

## 6. The numbers behind it

On a dedicated server at 60 Hz, ServerAnchor, roughly 80-110 ms RTT:

- Clear hits registered 97-100%, clear misses missed 100% (open, crossing,
  backstop and crowd layouts).
- Targets behind cover not hit: 100% (cover, half cover and pillar layouts).
- No present-time fallbacks.
- The weak spots are jitter and packet loss, which cost 3-7 points.

"Clear" means the test shooter's aim was at least 10 cm inside the target's
capsule (a clear hit) or 10-40 cm outside it (a clear miss), as the shooter's
client saw it, so the label is unambiguous. Single runs
vary by up to ±5 points from noise; the offset sweep compares methods on the same
shots, which is reliable.

## 7. Questions you may be asked

**Why not let the client decide hits?** Then anyone with a modified client can
hit anything. The client reports where it aimed; the server decides.

**How does the server know how far back to rewind?** It measures the shooter's
round-trip time itself and rewinds by about that much, corrected by a measured
offset, capped at 200 ms. (Section 3, Step 6.)

**Doesn't rewind let people shoot me after I reach cover?** Yes, slightly: if the
shooter saw you before you reached cover, the hit counts. That's the inherent
trade-off of favoring the shooter. The 200 ms cap bounds it, and the alternative
(no rewind) makes everyone with real latency miss moving targets.

**What happens at very high ping?** Beyond the 200 ms cap the shot can't be
rewound. It falls back to present-time (by default), which means the high-ping
player has to lead their target. The cap protects everyone else.

**What stops someone faking lag?** Fake lag only buys rewind up to the same cap
any high-ping player gets, and a lag switch makes the server's RTT estimate
wrong, which hurt the cheater in testing.

**Why capsules and not hitboxes?** v1 records capsules. Lag-compensated hitboxes
(headshots) are the v2 design: a central manager that records bone transforms.

**Why is ServerAnchor the default instead of using the client's stamp?** Measured:
the stamp gave no accuracy benefit in Unreal's per-actor replication, while it
costs bandwidth and opens a small backtracking window.

## 8. Try it yourself

Console variables (development builds, on the server unless noted):

| Command | What it shows or does |
|---|---|
| `Enclave.Debug.SSRSelection 1` | One log line per shot: rewind time, outcome, stamp handling |
| `Enclave.Debug.SSRHistory 1` | A history summary every 2 s per character, plus segment breaks and death events |
| `Enclave.Debug.SSRHistoricalEligibility 1` | The death rule decision per candidate |
| `Enclave.Debug.SSRTimingSweep 1` | Re-evaluates each shot at several offsets (the sweep table above) |
| `Enclave.Debug.SetSSRTimingSource <Source>` | Switch ServerAnchor / Hybrid / EchoedStamp live |
| `Enclave.Debug.SetWeaponHitDetectionMode <Mode>` | Switch Capsule / SkeletalHitboxes live |
| `Enclave.Debug.ShotAlignment 1` | Client and server shot traces side by side |

Scripts (`Tools/Scripts/SSR/`): `RunSSRSession.ps1` runs one session (dedicated
server, shooter, target, network emulation); `RunSSRLayouts.ps1`,
`RunSSRAbuse.ps1`, `RunSSRMatrix.ps1` and `RunSSREdgeCases.ps1` are the suites.

A good first exercise: run one session with `-LagCompensation Disabled` and one
with `ServerSideRewind` at the same lag, and compare clear-hit rates. That's the
whole point of SSR in two numbers.

## Glossary

| Term | Meaning |
|---|---|
| RTT | Round-trip time: client → server → client |
| Simulated proxy | Another player's character as your client sees it, driven by replication |
| Present-time trace | A normal line trace against where things are now on the server |
| Rewind time | The server time the shot is evaluated at |
| Segment | A continuous run of recorded frames; never interpolated across |
| Fallback | What an unavailable rewind does: present-time trace (default) or no hit |
| Stamp | A server time attached to a recorded frame, echoed by the client (Hybrid / EchoedStamp) |
