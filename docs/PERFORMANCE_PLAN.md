# OAR performance plan

Goal: OAR should cost as close to zero frame time as possible when installed, and only pay for features that are actually in use. This plan comes from the 2026-09-02 whole-plugin audit (three parallel code reads, top claims spot-verified). The numbers in "Baseline" get filled in from the instrumented build; everything before that is operation counts, not measurements.

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

## Phase 1: the no-op path (largest expected win)

Target: an unreplaced clip's Update costs a few atomic loads and one pointer-keyed lookup, no string work, no allocation.

1. Per-clip replacement decision cache. Add a small pointer-keyed map (or a slot in a consolidated per-clip record, see Phase 4) holding `kUnknown / kNoRegistration / kHasRegistration`, written the first time a clip's suffix is resolved and its `s_suffixToInfos` lookup fails or succeeds, cleared in Activate (the suffix can change when the engine reuses a generator) and Deactivate. Check it first in Update, right after the four existing atomic gates, and return on `kNoRegistration`. This removes the pending-activate-log lookup, the bypass-set lookup, the suffix-cache string copy, the annotation-integrity stamp, both `s_nameLookupMutex` acquisitions, the diagnostic suffix log and the `resolvedSuffix` copy from the negative path.
2. Delete the "log unique suffixes" diagnostic block in Update (shared lock plus string-keyed set lookup on every call, forever).
3. Hoist `Settings::GetSingleton()` to one call per Update.
4. Replace the unconditional `std::string resolvedSuffix = suffix` copy with a pointer or `string_view` that only rebinds when multi-match resolution actually produces a different string.

Risk: low. The decision cache is invalidated on the same events that already invalidate the suffix cache. Verify: `Update.noMatch` avg drops to well under 0.1 µs per call; replacements still apply after weapon switches and config reloads.

## Phase 2: always-on diagnostics and event work

1. `AnimationLog::enabled` defaults to false. `UIAnimationLog` and `UIAnimationEventLog` set it true when opened. Nothing calls `SetEnabled` today, so the event log is permanently live for every user.
2. `InSoundSuppressWindow` gets an atomic "windows armed" counter in front of it (incremented when a window is armed, decremented when one expires or is consumed) so the per-event mutex, map lookup and clock read only run while some actor actually has a window.
3. `EventSourceAnimFor` and `RecordLastActivatedClip` (the event-log attribution) only run when the log is enabled; today the Activate hook records into `s_lastActivatedClipByActor` unconditionally.

Risk: low. Verify: `EventFeed` avg drops to under 0.1 µs per call with the log windows closed; the Animation Event Log still fills when opened.

## Phase 3: the replaced-clip path

1. `GetOrBuildRuntimeAnim`: take a `shared_lock` for the lookup and clone-scan, return the existing clone under it, and only re-acquire as `unique_lock` when a clone must be built or retired (double-checked). Alternative with a bigger payoff: cache `{gameOriginal, clone, generation}` per clip in Hooks.cpp and bump a cache-wide generation counter on every retire/build, so the steady-state call is a pointer compare with no lock.
2. `ValidatedActiveSubMod`: `shared_lock` on the read path, upgrade only on the stale-binding branch.
3. `ActiveReplacementTracker::Update`: hash the composite key only after the 1 Hz throttle passes, or key by clip pointer.

Risk: medium (lock discipline around clone retirement). Verify: `Cache.getOrBuild` avg drops to tens of nanoseconds; weapon switching, save load and config reload still rebuild clones correctly; no `[OAR-BuildFail]` lines.

## Phase 4: player-frame hooks and the NaN guards

1. `HealSkeletonRootNaN`: keep the chain walk (cheap) but gate the full flattened-tree scan on recent activity: a track filter active on the player, or a deferred IdleStop delivered within the last few seconds. Both facts already exist as atomics/timestamps.
2. Generate's NaN scrub: scope to the character that actually has an active filter (`FindTrackFilterState`) instead of the global `s_trackFilterActiveCount`, and scrub only the bones the filter writes.
3. `PollPlayerGraphClips`: validate pointers per (root, entry count) once instead of `IsBadReadPtr` per entry per frame; recompute the fingerprint from plain reads.

Risk: medium for the heal gate (it is the containment for the vault NaN storm; the gate must be generous). Verify: `HealSkeleton` near zero outside vaults; run several cryolator vaults and confirm no white-screen return.

## Phase 5: track filter per-frame waste

1. Cache the bone-to-track reverse map per (clip, donor) on activation instead of rebuilding per Generate.
2. Replace the per-frame `unordered_set` of filtered bone indices with a fixed bitset built on activation.
3. Cache each anchored chain root's ancestor index list per (clip, skeleton); the topology is static.

Not achievable: sampling only the needed tracks. FO4's Havok 2014 `sampleTracks` has no partial-track entry point; partial sampling would mean hand-decoding spline blocks, which is a project of its own.

Risk: medium to high (this is the most delicate code in the plugin). Do last, verify with the vault set and the P890/pipe-gun frozen-arm cases.

## Phase 6: load time and memory (not frame time)

1. Chunk the deferred BA2 donor loads across several main-thread tasks instead of draining every archive item in one task.
2. Parallelise config discovery and JSON parsing per mod directory, as the .hkx byte loads already are.
3. Add a byte cap to the retired-clone list alongside the 256-entry cap; `backingFileData` copies whole donor files on every config reload.
4. Prune `s_liveCamCarrierSeenFrame` and `s_origAnnotByActor` in the existing periodic sweep.
5. `MathStatementCondition`: parse once at initialisation, evaluate against a slot array. `InventoryCountCondition`: break once the matching entry is summed.

## Order and gating

Phase 1 first (largest, most mechanical), then 2, then 3. Re-run the instrumented build after each phase and update the Baseline table; a phase that does not move its row is reverted, not kept on faith. Phases 4 and 5 touch the vault containment and the track filter, so each gets its own in-game pass on the known-bad weapons before it is committed. Phase 6 is independent and can be interleaved.

Every phase is a separate commit on `feature/preserve-extracted-motion` (or a dedicated perf branch if preferred) so any one of them can be reverted alone.
