# OAR performance plan

Goal: OAR should cost as close to zero frame time as possible when installed, and only pay for features that are actually in use. This plan comes from the 2026-09-02 whole-plugin audit (three parallel code reads, top claims spot-verified), followed by an adversarial review of the plan itself against the source (2026-09-02). That review found that several phases, as first drafted, would have broken existing behaviour. The corrected designs below carry the invariants that review established; a phase that cannot keep its invariant is not implemented.

The numbers in "Baseline" get filled in from the instrumented build; everything before that is operation counts, not measurements.

## How to read the instrumentation

The measurement build logs one `[OAR-Perf]` block every 10 seconds (`[Debug] bPerfInstrumentation`, default on in this build, will default off afterwards). Each row is summed CPU time across all threads per frame, because Havok updates clips from several job threads at once. Rows indented with two spaces are sub-scopes that run inside the row above them and are listed for attribution only; `TOTAL` sums the top-level rows.

| Row | What it measures |
|---|---|
| `Update` | OAR's own work in `hkbClipGenerator_Update`, after the engine's Update returns. Every clip on every actor, every frame. |
| `  Update.noMatch` | The subset of Update calls that ended at "no replacement registered for this clip". This is the pure tax. |
| `  Cache.getOrBuild` | `AnimationCache::GetOrBuildRuntimeAnim`, inside Update (and Activate). Includes the global exclusive lock. |
| `Generate` | OAR's own work in `hkbClipGenerator_Generate`, after the engine's Generate. |
| `  TrackFilter` | The track-filter block inside Generate. Only non-zero while a filter plays. |
| `Activate` | The whole Activate hook, including the engine's Activate. Per activation, not per frame. |
| `EventFeed` | The animation-event sink, per event, all actors (footsteps, sync, jiggle, sounds). |
| `HealSkeleton` | `HealSkeletonRootNaN`, both call sites, twice per frame. |
| `PollPlayerGraph` | `PollPlayerGraphClips`, once per frame. |

What to capture: at least one 10 s window in each of a quiet interior, a busy exterior with many NPCs (Diamond City market is ideal), a stretch of combat, and one vault so `TrackFilter` shows up. Paste the blocks; the `max` column matters as much as the average because a stall on one thread is what a player feels.

## Baseline

To be filled from the instrumented build.

| Scene | Update ms | noMatch ms (calls) | Generate ms | EventFeed ms | Heal ms | Poll ms | TOTAL ms | fps |
|---|---|---|---|---|---|---|---|---|
| quiet interior | | | | | | | | |
| busy exterior | | | | | | | | |
| combat | | | | | | | | |
| vault (filter active) | | | | | | | | |

Pre-measurement estimate for the busy case: 1 to 2 ms per frame, with `Update.noMatch` the largest share.

## What must not change (verified against the code)

These are the behaviours the review found the first draft would have broken. Every phase below is written to keep them.

1. `CaptureGameVtable` ([Hooks.cpp ~8152](../src/Hooks.cpp)) must keep running for every enabled clip, replaced or not. It is the only place OAR learns the game vtable for each `hkaAnimation::type`; a type only ever seen on unreplaced clips would otherwise never be learned, and a later replacement of that type would stay unpatched forever.
2. The vanilla-annotation contract in Update (the backup driver ~8215-8308 and the per-play integrity check ~8310-8390) must stay reachable for unreplaced clips. The code calls it non-negotiable: engine trigger arrays built wrong at Activate are repaired here so authored sounds and events still fire at their times. The Deactivate flush only covers plays that end near the animation's own duration.
3. A clip's suffix is not authoritative at Activate. It starts as a leaf or authored-name guess and can be rewritten later in the same activation by the direct-path defer gate in Update (~8477-8502) or by `PollPlayerGraphClips` via `EnsureDirectSuffixForClip` (~5471). A "no replacement" decision taken from the guess must not survive that rewrite.
4. Config reload rebuilds `s_suffixToInfos` (~4732) but does not clear `s_clipSuffixCache` or any per-clip cache. Any new per-clip decision must be invalidated by the reload path explicitly. Submod enable/disable and priority edits only re-sort the tables in place, so they do not change the "has a registration" fact.
5. `GetOrBuildRuntimeAnim` re-validates a found clone against the live original's duration and track count on every call (AnimationCache.cpp ~625-644). The engine frees originals and reuses their addresses; this check is the only defence, and no cache-side generation counter can observe an engine-side address reuse. Any fast path must keep performing this comparison.
6. `HealSkeletonRootNaN`'s own adaptive-learning window trusts storms up to 2.5 s after an IdleStop delivery (~15704-15717). Any gate on the heal must be at least that generous, with margin.
7. The NaN scrub in Generate must keep scrubbing motion tracks 0 and 1 (worldFromModel, extractedMotion) whenever it runs. The documented storm lands there, not on the pose bones the filter writes (~10816-10827).
8. `PollPlayerGraphClips` validates active-node entries individually because the engine reuses entries in place without changing the container (~7212 comment); container-level validation is not a superset of per-entry validation.
9. `resolvedByChar` is fully cleared on every fresh play because `hkbCharacter*` values are recycled across weapon re-equips for different skeletons (~6908-6912). Any cache keyed by character or skeleton identity inherits that purge requirement.
10. `MathStatementCondition::expression` is mutated by the live condition editor (`DrawEditWidgets` ~3705-3715), not only by `InitializeImpl`; a parsed cache must be rebuilt from every mutation site.

Safe as originally drafted: nothing outside the two log windows reads `AnimationLog` entries (grep-verified), so gating the log is purely a UI concern. `s_soundSuppressUntilByActor` has exactly one arm site (~301) and one erase site (~313), so an exact counter is feasible.

## Phase 1: the no-op path (largest expected win)

Target: an unreplaced clip's Update does the vtable capture and the annotation contract, then reaches "no replacement" with no string hashing, no string copy and no diagnostic lookups.

Design (revised):

1. Store the decision with the suffix, not beside it. `s_clipSuffixCache` becomes `unordered_map<hkbClipGenerator*, ClipSuffixEntry{ std::string suffix; Decision decision; uint32_t lookupGeneration; }>`. Every site that writes the suffix (Activate ~7291, Update ~8174, `EnsureDirectSuffixForClip` ~5471) resets `decision` to `kUnknown`, so invariant 3 holds by construction. A global `s_lookupGeneration` is bumped where `s_suffixToInfos` is rebuilt (~4732) and the decision is only trusted when its generation matches, so invariant 4 holds.
2. Consult the decision AFTER `CaptureGameVtable`, after the vanilla-annotation blocks, and after the direct-path defer gate, in place of the string-keyed `s_suffixToInfos` / leaf-table lookups (~8419 and ~8565) and the final `find` (~8842). On `kNoRegistration` return there. The negative path keeps every side effect it has today except the string hashing and the copies.
3. Delete the "log unique suffixes" diagnostic block (~8524-8538).
4. Hoist `Settings::GetSingleton()` to one call per Update.
5. Replace the unconditional `std::string resolvedSuffix = suffix` copy with a pointer that only rebinds when multi-match resolution produces a different string; the suffix copy out of the cache (~8157) becomes a pointer held under the map's shared lock for the duration of the read, or a copy only when the string is actually needed downstream.

Verify: `Update.noMatch` average drops well under 0.5 µs per call; replacements still apply after weapon switches; add a submod for a previously-unreplaced clip, reload configs without re-equipping, confirm it plays; the pending-activate log still resolves real paths.

## Phase 2: always-on diagnostics and event work

1. `AnimationLog::enabled` defaults to false; `UIAnimationLog` and `UIAnimationEventLog` set it true when opened. Nothing functional reads the log.
2. `InSoundSuppressWindow` gets an atomic counter in front of it: incremented only when `ArmSoundSuppressWindow` inserts a NEW key (re-arming a live actor does not increment), decremented at the single erase site. Because expired windows are only erased lazily on the next event from that actor, the counter can drift upward over a long session; add a cheap periodic sweep of expired entries (in the existing per-frame deferred-IdleStop service) so the fast path actually stays fast.
3. `RecordLastActivatedClip` and `EventSourceAnimFor` run only while the log is enabled.

Verify: `EventFeed` average under 0.1 µs per call with the windows closed; the Animation Event Log fills when opened; IdleStop sound suppression still mutes the fast-forward burst.

## Phase 3: the replaced-clip path

1. `GetOrBuildRuntimeAnim`: shared-lock fast path only. Take `shared_lock`, run `SelectEntry` and the clone scan including the duration/track-count re-validation (invariant 5), return the existing clone; drop to `unique_lock` only to build or retire, re-running the scan after re-acquisition. The "per-clip cached clone pointer with a generation counter" alternative is withdrawn: it cannot see engine-side address reuse.
2. `ValidatedActiveSubMod`: `shared_lock` on the read path; on the stale-binding branch, release, take `unique_lock`, and re-`find` before erasing (no reuse of iterators across the lock change).
3. `ActiveReplacementTracker::Update`: hash the composite key only after the 1 Hz throttle passes, or key by clip pointer.

Verify: `Cache.getOrBuild` average drops to tens of nanoseconds; weapon switching, save load and config reload still rebuild clones; no `[OAR-BuildFail]` lines.

## Phase 4: player-frame hooks and the NaN guards

1. `HealSkeletonRootNaN`: keep the cheap camera-chain walk unconditional. Gate the flattened-tree scan on a window that stays open while `s_trackFilterActiveCount > 0`, for 10 s after any deferred IdleStop delivery, and for 30 s after any heal actually fired. This is four times the 2.5 s the code's own learning heuristic trusts (invariant 6). The heal-fired extension keeps the containment live if a storm arrives from a source we have not identified.
2. NaN scrub in Generate: keep the motion-track scrub (tracks 0 and 1) whenever the block runs (invariant 7). Narrowing the actor scope from "any filter active anywhere" to "this character has an active filter" is deferred until an in-game pass with NPCs present during a vault shows the bystander scrub was never needed; until then only the pose-bone loop may be narrowed to the filter's bones, and only for characters other than the filtering one.
3. `PollPlayerGraphClips`: keep per-entry validation (invariant 8); make it cheaper by probing a narrower range and skipping the container-level re-probe when the fingerprint is unchanged.

Verify: `HealSkeleton` near zero outside vaults; several cryolator vaults with NPCs nearby, no white-screen return, no `[OAR-SkelHeal]` heals missed (heal count before and after equal for the same storm).

## Phase 5: track filter per-frame waste

1. Cache the bone-to-track reverse map per (clip, donor) and rebuild it on activation and on every donor swap (variant change, condition flip, `ReplaceActiveAnimation`, leaf-matching host change).
2. Replace the per-frame `unordered_set` of filtered bone indices with a fixed bitset built at the same points.
3. Cache each anchored chain root's ancestor index list per (clip, skeleton), purged on exactly the trigger `resolvedByChar` uses (fresh play / weapon re-equip), never keyed by a bare `hkbCharacter*` value (invariant 9).

Before implementation this phase needs its own trace of every donor-swap path in the Generate track-filter block; the review did not cover that and the plan does not pretend it did.

Not achievable: sampling only the needed tracks. FO4's Havok 2014 `sampleTracks` has no partial-track entry point; partial sampling would mean hand-decoding spline blocks, which is a project of its own.

Verify: the vault set, the P890 and pipe-gun frozen-arm cases, a leaf-matching donor under two different weapons.

## Phase 6: load time and memory (not frame time)

1. Chunk the deferred BA2 donor loads across several main-thread tasks instead of draining every archive item in one task.
2. Parallelise config discovery and JSON parsing per mod directory, as the .hkx byte loads already are.
3. Add a byte cap to the retired-clone list alongside the 256-entry cap.
4. Prune `s_liveCamCarrierSeenFrame` and `s_origAnnotByActor` in the existing periodic sweep.
5. `MathStatementCondition`: parse once, but rebuild the parsed form from every site that mutates `expression` or `variables`, including `DrawEditWidgets` (invariant 10).
6. `InventoryCountCondition` early break: only if the engine guarantees one `InventoryEntryData` per form in `inventoryList->data`. Unverified; if it cannot be established, leave the full walk (an early break would silently undercount).

## Order and gating

Phase 2 first (smallest, safest, and it makes Phase 1's pending-log removal moot), then Phase 1, then Phase 3. Re-run the instrumented build after each phase and update the Baseline table; a phase that does not move its row is reverted, not kept on faith. Phases 4 and 5 touch the vault containment and the track filter, so each gets its own in-game pass on the known-bad weapons with NPCs present before it is committed. Phase 6 is independent and can be interleaved.

Every phase is a separate commit so any one of them can be reverted alone.
