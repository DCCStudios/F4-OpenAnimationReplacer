# OpenAnimationReplacer Development Status

## Objective

Add an opt-in submod setting for special idles that prevents the source idle from entering the full-body graph. OAR should instead advance the selected replacement independently and apply only its track-filtered transforms over the normal animation graph. Align the existing IdleStop option with the behavior used by `PluginTemplate/fallout4-idlestopfix-main`.

## Current baseline

- Source tree: `E:\Fallout 4 Modding\F4SE\OpenAnimationReplacer`
- Runtime target under investigation: Fallout 4 1.10.163 (OG)
- Relevant test submod: `CC Anims Additive`
- The existing track filter samples a replacement from a live source clip and overwrites only selected output-pose tracks. It does not prevent the original special idle from playing in the behavior graph.
- The existing IdleStop option consumes the actor's `IdleStop` event before the original event sink sees it.
- The reference IdleStopFix instead calls `UpdateAnimation(1000.0f)` and then delivers the original `IdleStop` event.

## Verified findings

- The test donor animation itself has only weapon-cull and foley annotations.
- A behavior-authored end trigger from the source special idle coincides with `wpnequipfast`; filtering pose tracks cannot suppress that trigger while the source idle remains active.
- The old raw call-site offsets from the standalone IdleStopFix do not point to calls in the currently loaded OG executable, so they must not be reused blindly.
- CommonLib exposes `RE::ID::AIProcess::SetupSpecialIdle`; exact direct calls to this target can be discovered in executable code and patched with validation.

## Planned implementation

- Add a default-off `triggerOnlySpecialIdle` option inside each submod's track-filter settings.
- When a matching `SetupSpecialIdle` request is intercepted, skip the engine special-idle start only after a replacement and cached animation are validated.
- Create a standalone track-filter state whose clock starts at the intercepted request.
- Sample that replacement once per game frame and stamp only the selected tracks over ordinary graph output.
- Do not replay source or donor graph annotations in this isolated mode. Existing custom start/end events remain available.
- Change armed IdleStop handling to fast-forward the actor animation by 1000 seconds and then invoke the original event sink.

## Implemented in the current working tree

- Added the default-off `trackFilter.triggerOnlySpecialIdle` JSON setting.
- Added `Play Special Idle as Filter-Only Layer (?)` to the editable track-filter GUI with a detailed fallback and annotation warning.
- Added owner-aware cached-animation selection so the independently sampled donor is the file owned by the winning submod.
- Added validated runtime discovery of direct calls to CommonLib's `AIProcess::SetupSpecialIdle` relocation. Only exact call targets are patched; no legacy raw offsets are used.
- The interceptor preserves TESIdle conditions, ordinary path versus Leaf Matching semantics, OAR conditions, cached donor validation, and donor binding validation before suppressing the native start.
- Added standalone per-actor track-filter state with independent one-shot timing. One active non-additive graph output samples the donor per actor-update generation; later clips use the existing filtered-pose cache.
- Standalone playback supports ordinary playback-following and fixed-frame pose donors, the existing blend-in/end behavior, model-space anchoring, custom start/end events, and stale-state cleanup.
- Source and donor graph annotations are intentionally absent in filter-only mode, preventing behavior-authored source triggers such as the observed EquipFast end trigger.
- Renamed the existing GUI option to `Apply IdleStop Fix After Special Idle (?)`. When armed, OAR now calls `UpdateAnimation(1000.0f)` and then invokes the original IdleStop sink, matching the reference plugin's essential behavior.
- Removed the accidental `.xmake` cache produced by an incorrect build-tool probe. No user files were removed.

## Build, deployment, and runtime state

- Source implementation: complete for static testing
- Build: succeeded with MSVC Release; warnings are existing warning classes and no errors were emitted
- DLL: `E:\Fallout 4 Modding\F4SE\OpenAnimationReplacer\Compile\F4SE\Plugins\OpenAnimationReplacer.dll`
- DLL SHA-256: `CDC781E19CBD675F3AAB45BA22462EF5DE2844A097BAAF70CF7E79CAC9FE52A1`
- Deployment: complete for LoreOut and Magnum Opus. DLL and PDB hashes match the staged artifacts; both installed INIs retained their pre-deployment hashes.
- Runtime validation: failed for the first `CC Anims Additive` test. The hook installed across 27 validated direct call sites, but no standalone-start marker appeared. Five `1stgo` plays used the ordinary source-clip path, so the native full-body idle remained active.

## Risks and runtime-only unknowns

- Some special-idle callers may depend on engine-maintained current-idle state even when the call reports success.
- Direct-call discovery must find and validate the actual runtime call sites; failure must fall back to vanilla behavior.
- Independent sampling must choose a non-additive graph output once per frame so the filtered pose is not based on another additive clip.
- Static build success cannot prove that the PlayIdle caller, pose layering, end timing, or IdleStop behavior is correct in game.
- The Papyrus `PlayerRef.PlayIdle()` route either bypasses the 27 patched direct callers or reaches the detour and fails a silent eligibility gate. The current log cannot distinguish those cases because hook entry and rejection reasons are not logged.
- The `LArm_Collarbone` descendant expansion also includes `WeaponIKTargetR` and `WeaponIKTargetRMirror`. Those helper tracks can move the right hand even though the explicit `RArm_Collarbone` subtree is excluded.

## Remaining work

1. Add bounded entry and rejection-reason telemetry to the special-idle detour, or hook the actual Papyrus PlayIdle boundary, to distinguish a missed caller from a match rejection.
2. Explicitly exclude `WeaponIKTargetR` and `WeaponIKTargetRMirror`, or prevent cross-chain IK helpers from entering left-arm descendant expansion.
3. Retest `CC Anims Additive` and require a `[OAR-TrackFilter-Standalone] Started` line before evaluating its pose behavior.
4. Confirm the right arm and normal graph continue to animate, the selected left-side tracks use the donor, and `wpnequipfast` does not activate at the donor end.
