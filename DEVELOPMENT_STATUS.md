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
- CommonLib exposes `RE::ID::AIProcess::SetupSpecialIdle`, but patching only direct calls to that target missed the tested Papyrus `PlayIdle` route even though 27 call sites were found.
- Weapons that share an animation folder can recycle a clip generator and binding. Retiring runtime clones without clearing per-clip active-submod, suffix/path, and variant state lets the first weapon's replacement remain locked onto later weapons.

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
- Replaced direct-call discovery with a MinHook function-entry detour on CommonLib's runtime-selected `AIProcess::SetupSpecialIdle` relocation. This covers direct and indirect/Papyrus callers, preserves a callable relocated original, validates executable target memory, and fails open to vanilla behavior if hook creation or enablement fails.
- Added entry and fallback-reason logging for every intercepted `SetupSpecialIdle` request so runtime tests distinguish missed eligibility from a successful standalone start.
- The interceptor preserves TESIdle conditions, ordinary path versus Leaf Matching semantics, OAR conditions, cached donor validation, and donor binding validation before suppressing the native start.
- Added standalone per-actor track-filter state with independent one-shot timing. One active non-additive graph output samples the donor per actor-update generation; later clips use the existing filtered-pose cache.
- Standalone playback supports ordinary playback-following and fixed-frame pose donors, the existing blend-in/end behavior, model-space anchoring, custom start/end events, and stale-state cleanup.
- Source and donor graph annotations are intentionally absent in filter-only mode, preventing behavior-authored source triggers such as the observed EquipFast end trigger.
- Renamed the existing GUI option to `Apply IdleStop Fix After Special Idle (?)`. When armed, OAR now calls `UpdateAnimation(1000.0f)` and then invokes the original IdleStop sink, matching the reference plugin's essential behavior.
- Removed the accidental `.xmake` cache produced by an incorrect build-tool probe. No user files were removed.
- Same-folder equipped-weapon changes now clear clip runtime state after retiring clones, matching folder-change cleanup without restoring potentially freed animation pointers.
- The latest 10mm-to-shared-path test proved condition evaluation was correct (`IsEquipped [Fallout4.esm:0x4822] -> FAIL`, winner none), but the stale replacement remained. The log had no `[OAR-WeaponChange]` entries and the activation scrub repeatedly reported `bindingSet null/unreadable`.
- Activation-time player ownership and binding cleanup now use `PlayerGraphIndexForClip` plus the real `manager->graph[index]->character`. This replaces the invalid `hkbContext::character` dummy for both weapon fingerprint invalidation and stale shared-binding restoration.
- The UI now treats Windows DPI and the text-size percentage as independent multipliers. ImGui font DPI, style geometry, default window dimensions, live window resizing, and the standalone Settings panel all follow the combined effective scale; monitor DPI changes are detected per rendered frame.
- The text-size slider and persisted-setting clamp now allow 50% through 200%, while retaining 125% as the default.

## Build, deployment, and runtime state

- Source implementation: complete for static testing
- Build: succeeded with MSVC Release after the DPI and activation-owner corrections; warnings are existing warning classes and no errors were emitted
- DLL: `E:\Fallout 4 Modding\F4SE\OpenAnimationReplacer\Compile\F4SE\Plugins\OpenAnimationReplacer.dll`
- DLL SHA-256: `252ED49B4C942FFA643F953099A60B2C370C66F4AA6B791320CD7901F8263F51`
- Deployment: the DPI and real-player-binding build is deployed to LoreOut and Magnum Opus. The later 50% slider-bound rebuild is staged and packaged but not deployed; each modlist's customized INI remains untouched.
- GitHub release: the replacement `OpenAnimationReplacer-v1.1.8.zip` retains the MO2-ready four-entry layout and has SHA-256 `7F2F79A61FEBF671EB9456FECBA70DD9843555955D17C4A6E230CDBE33013A25`. The release description documents DPI-aware scaling and the 50% through 200% text-scale range.
- Runtime validation: pending for the new function-entry hook and same-folder cleanup. The prior direct-call build failed because no standalone-start marker appeared and five `1stgo` plays used the ordinary full-body source-clip path.

## Risks and runtime-only unknowns

- Some special-idle callers may depend on engine-maintained current-idle state even when the call reports success.
- Another plugin may already detour `SetupSpecialIdle`; MinHook creation/enablement failure is logged and must leave vanilla behavior intact.
- Independent sampling must choose a non-additive graph output once per frame so the filtered pose is not based on another additive clip.
- Static build success cannot prove that the PlayIdle caller, pose layering, end timing, or IdleStop behavior is correct in game.
- The new entry detour is statically built but still needs runtime proof that the Papyrus `PlayerRef.PlayIdle()` route reaches it and starts the standalone layer.
- The `LArm_Collarbone` descendant expansion also includes `WeaponIKTargetR` and `WeaponIKTargetRMirror`. Those helper tracks can move the right hand even though the explicit `RArm_Collarbone` subtree is excluded.

## Remaining work

1. Retest `CC Anims Additive` and require a `SetupSpecialIdle entry` followed by `[OAR-TrackFilter-Standalone] Started`; if it falls back, use the new reason in the same log entry.
2. Confirm the right arm and normal graph continue to animate, the selected left-side tracks use the donor, and `wpnequipfast` does not activate at the donor end.
3. If right-hand motion remains after standalone playback starts, explicitly exclude `WeaponIKTargetR` and `WeaponIKTargetRMirror`, or prevent cross-chain IK helpers from entering left-arm descendant expansion.
4. In third person, equip the 10mm, then at least two other pistols that share its animation folder. Require a new `[OAR-WeaponChange]` fingerprint transition, an activation scrub against the real binding set when a clone is present, a false Heel Chambering condition, and the vanilla animation on the non-10mm weapon.
5. At a known Windows display scale, open OAR and verify `[OAR-UI]` reports DPI, text, and their product (for example, 150% DPI times 125% text = 1.875 effective). Move the game window between differently scaled monitors if available and verify text, style geometry, and window dimensions update together.
