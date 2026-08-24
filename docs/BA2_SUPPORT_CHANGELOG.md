# BA2 Support Changelog

This file records the purpose and verification status of every change made on the `codex/oar-ba2-support` development branch. Entries describe generic OAR behavior and must not depend on TAEF-specific names or conditions.

## 2026-08-25

### Replace full animation-cache reverse scans with a clone identity index

- Purpose: reduce the generic cost of replacement identity checks in Clip Generator and lifecycle paths when many animations are preloaded.
- Root cause: `IsOurReplacement`, `GetReplacementIdentity`, `GetOriginalFromReplacement`, and `GetOriginalFromRetiredReplacement` scanned every cached file, live clone, and retired clone until they found a pointer. With 1,879 preloaded replacement animations, this made an unrelated pointer query scale with the entire cache.
- Scope: add a mutex-protected `hkaAnimation*` reverse index containing the original-animation validation data and SubMod identity, update it on clone creation, retirement, retired-buffer eviction, and cache clear, and preserve the existing original duration/track validation and retired-clone keep-alive behavior.
- Genericity: no TAEF names, posture rules, weapon rules, archive assumptions, actor filters, or player-only logic were added. The optimization applies to all OAR replacement configurations and both loose/resource-backed animations.
- Verification result: the releasedbg build passed and was deployed with matching project/MO2 hashes. After a clean restart, the user measured a peak of approximately 160 FPS, improving over the previous stable-155 baseline. Replacement behavior remains under continued gameplay verification, with no reported regression in this test.

### Remove default-off diagnostic work from steady-state paths

- Purpose: reduce generic OAR CPU overhead when verbose logging is disabled.
- Root cause: the Clip Generator hot path still performed unique-suffix diagnostic locking and per-no-match/per-update diagnostic counter operations even though `bVerboseLogging=false`. Track-filter actor updates also copied an unused suffix string and recreated temporary containers each frame.
- Scope: gate those diagnostics behind the existing verbose-logging setting, avoid repeated atomic log-counter increments after the diagnostic cap, remove the unused temporary suffix copy, reuse thread-local evaluation buffers, and replace the false-condition tree with a small reusable vector. Selection, condition evaluation order, blending, replacement, annotation, and cleanup semantics are unchanged.
- Genericity: no TAEF names, posture rules, weapon rules, archive assumptions, or player-only replacement rules were added.
- Verification target: build and compare the deployed DLL hash, then clean-restart the game and compare stable FPS against the `stable-155` baseline. Runtime confirmation remains pending.

### Back off unavailable weapon-graph path discovery

- Purpose: prevent a temporary unavailable player weapon graph from creating repeated activation-time graph walks.
- Root cause: when `s_weaponAnimFolderValid` was false, every Clip Generator activation called the full weapon animation-folder discovery routine. The graph can remain unavailable during graph construction or transition, so the same failed walk was repeated as more clips activated and the cost accumulated after startup.
- Scope: keep the existing periodic refresh when a folder is valid, but limit retries after an unavailable result to one wall-clock attempt per second. Runtime path resolution, direct matching, weapon-change invalidation, and fallback sources are unchanged.
- Genericity: no TAEF names, posture rules, weapon rules, archive assumptions, or player-only replacement rules were added. This applies to any OAR setup using the generic weapon-graph path discovery fallback.
- Verification result: built and deployed DLL `8B3A44AD4B91D1FBD59919E3777AF14CE303E01A947652E2AF0AEA2B84BC3998`; workspace and MO2 hashes matched. After a clean restart, the user reported that the settled peak FPS returned to the level of the previously backed-up stable-155 source/runtime version. Animation-path behavior remained functional in the test.

### Reduce steady-state bookkeeping contention

- Purpose: reduce generic per-frame CPU work while preserving replacement selection, condition evaluation, annotation timing, and API/UI state.
- Root cause: the per-play annotation-integrity stamp used an exclusive mutex on every clip update, even after the play had already been checked. The active-replacement diagnostic tracker also copied unchanged strings and took an exclusive lock every update. Two legacy active-replacement side tables were written on every replacement update but had no readers in the current source tree.
- Scope: store annotation-integrity timestamps in per-entry atomics behind a shared map lock, limit exclusive work to first sighting or a detected local-time regression, refresh unchanged diagnostic entries at most once per second, and remove the unread legacy side-table writes. Replacement and annotation state remain authoritative in their existing maps and trigger paths.
- Genericity: no TAEF names, posture rules, weapon rules, archive assumptions, or player-only conditions were added. The optimization applies to all OAR replacement configurations.
- Verification target: build and compare the deployed DLL hash, then perform a clean in-game restart and compare peak/stable FPS plus annotation and replacement behavior against the stable 155-FPS source snapshot. Runtime confirmation remains pending.

### Reverted no-candidate Update fast path trial

- Trial purpose: reduce the per-frame cost of OAR's generic Clip Generator hook for clips without a registered replacement.
- Trial scope: added an exact/leaf candidate lookup followed by runtime-state checks before the existing annotation, direct-path, and condition-maintenance path.
- Runtime result: after deployment of the trial DLL `747204B2FC511BE20B244639031F4E62CF406CC5F376F0B81C0B0102A57CE8B9`, the user measured a lower and less stable result: approximately 140-150 FPS, versus the preceding optimized build's approximately 152 FPS stable / 155 FPS peak.
- Decision: reverted from active source and deployment. The trial is not retained because its extra shared-lock and candidate-check work was not a performance win in the real graph workload.

### Stable player-graph poll fast path

- Purpose: remove repeated per-frame work from generic direct-path matching after the player's active graph has reached a stable state.
- Root cause: `PollPlayerGraphClips()` walked every active player Clip on every actor-update frame, even when the active-node arrays and nested graph pointers had not changed. That repeated pointer validation, path-cache locking, and suffix maintenance while no new path could be discovered.
- Scope: add a lightweight per-root fingerprint of the active-node array and a dirty marker set by Clip activation, deactivation, and runtime-state invalidation. A stable frame skips the expensive path-resolution walk; graph rebuilds, in-place node changes, and lifecycle hooks force the next stable scan. Replacement selection, condition evaluation, path matching, and NPC handling are unchanged.
- Genericity: no TAEF names, conditions, archive assumptions, or player-only replacement rules were added. This applies to any OAR configuration using direct-path matching.
- Verification target: build the generic OAR target and compare Animation Log path resolution plus in-game FPS against the pre-change optimized DLL. Runtime FPS improvement remains pending in-game confirmation.

### Build optimization parity

- Purpose: restore the optimization level used by the upstream Release build for the workspace xmake target.
- Root cause: the outer xmake project did not register `mode.release` or `mode.releasedbg`; the generated MSVC commands therefore had no `/O2` optimization flag even though xmake labeled the build as Release.
- Scope: add explicit xmake mode rules and `set_optimize("fastest")` for the generic OAR target. This affects compilation only and does not change BA2 discovery, resource loading, replacement selection, or TAEF behavior.
- Verification target: the next verbose build must show `/O2` in the OAR compile commands. Runtime FPS comparison remains a separate in-game test.
- Verification result: `scripts/build-xmake-project.ps1 -Name OpenAnimationReplacer -Configuration releasedbg -Target OpenAnimationReplacer` passed after the rule change. The optimized workspace DLL is 2,668,544 bytes with SHA256 `A0477F2371F962A2C315AD08D4BF45BDBE5FF291CDD2A69DDA035FEF01DBE034`; the previous unoptimized Dev DLL was 3,956,224 bytes with SHA256 `6C7E95E9B4E4AE7F8DC89E2D33145529C0C897788F6583FCD8EE7864521B180`. The optimized DLL is now deployed to `D:\TMR AE\mods\OpenAnimationReplacer BA2 Support Dev`, and the workspace/MO2 hashes match. Runtime FPS comparison remains pending.

### Initial BA2 support baseline

- Purpose: allow OAR replacement HKX files stored in Fallout 4 General BA2 archives to participate in the same discovery and preload pipeline as loose HKX files.
- User-visible problem: OAR loaded loose `config.json` files and submods, but reported `0 replacement animations` when the replacement HKX files were present only in BA2.
- Scope: add a BA2 General archive index, connect archive entries to OAR submods, and load archive resources through Fallout's resource stream API. No TAEF condition, posture, weapon, or animation path is hard-coded.
- Compatibility target: Fallout 4 BA2 `BTDX/GNRL` versions 1, 7, and 8; texture `DX10` archives remain outside this change.
- Verification status: implementation is complete for the first test build. The workspace build passed and an independent BA2 reader counted 1,888 HKX entries in the current TAEF General v8 archive. In-game resource-stream loading and replacement playback remain the runtime test step.

### 2026-08-25 implementation details

- `src/BA2Archive.h/.cpp`: added a bounded, defensive index for Fallout 4 `BTDX/GNRL` archives. It reads the 36-byte General file table, normalizes paths, ignores `DX10`, and indexes only `Meshes\...\*.hkx`. Purpose: make archive resources discoverable without changing OAR's JSON layout or adding mod-specific rules.
- `src/Parsing.h/.cpp`: merged indexed archive entries into each existing OAR submod after loose-file enumeration. Purpose: preserve the existing submod/variant/condition model while allowing an archived HKX to be registered by its relative path. Loose files take precedence when both sources contain the same path.
- `src/OpenAnimationReplacer.h`: added the internal distinction between an absolute loose-file path and a Data-relative resource path. Purpose: let later stages choose the correct reader without exposing BA2-specific behavior to conditions or replacement selection.
- `src/AnimationCache.h/.cpp`: added `LoadAnimationResource` using `RE::BSResourceNiBinaryStream`, and moved common Havok byte parsing/cache insertion into `LoadAnimationBytes`. Purpose: archive and loose HKX use one parser, one cache identity model, the same owner/priority rebinding, and the same runtime clone handling.
- `src/Hooks.cpp`: preload now accepts either a disk path or a resource path. Purpose: avoid the previous `0 replacement animations` result when the configuration is loose but HKX files are archive-only.
- `CMakeLists.txt`: registered the new BA2 source/header for upstream-style CMake builds. `src/PCH.h`: explicitly includes `fmt/format.h`, which the current official source already uses and the workspace xmake build requires.
- `projects/OpenAnimationReplacer/xmake.lua`: fixed locale staging to copy locale contents into the runtime directory and clear stale generated contents first. Purpose: keep the new Dev overlay deterministic across repeated builds and prevent an accidental `locales\locales` tree.

### Verification record

- Build command: `scripts/build-xmake-project.ps1 -Name OpenAnimationReplacer -Configuration releasedbg -Target OpenAnimationReplacer`
- Result: passed, including `BA2Archive.cpp`, `AnimationCache.cpp`, `Hooks.cpp`, and the final DLL link.
- Offline archive check: `TacticalActionExtensionFramework - Main.ba2`, `BTDX/GNRL v8`, 1,907 total entries, 1,888 Meshes HKX entries, 19 non-HKX entries. Of the HKX files, 1,879 are under the current OAR submod prefix; 9 are legacy OAR-config-external generic action files and are intentionally not registered as OAR replacements.
- Deployment: staged to `D:\TMR AE\mods\OpenAnimationReplacer BA2 Support Dev`; project and overlay DLL SHA256 are `6C7E95E9B4E4AE7F8DC89E2D33145529C0C897788F6583FCD8EE7864521B1802`. The old `OpenAnimationReplacer Chinese Dev` directory remains absent, and the new overlay was not added to `modlist.txt`.
- Runtime status: verified after a clean game restart on Fallout 4 AE 1.11.221. The log reports `TacticalActionExtensionFramework - Main.ba2` contributing 1,888 HKX entries, `Indexed 19964 General BA2 resources from Data`, `Parsed ... 1879 replacement animations`, and `Pre-loaded 1879 animations (0 failed)`. With the optimized DLL and BA2-backed TAEF assets active, the user measured approximately 151 FPS, matching the loose-file baseline range. This confirms the prior 120 FPS result was caused by the unoptimized Dev build rather than BA2 animation loading itself.
