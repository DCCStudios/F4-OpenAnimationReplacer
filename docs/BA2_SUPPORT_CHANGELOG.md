# BA2 Support Changelog

This file records the purpose and verification status of every change made on the `codex/oar-ba2-support` development branch. Entries describe generic OAR behavior and must not depend on TAEF-specific names or conditions.

## 2026-08-25

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
- Runtime status: not yet proven. After deploying the new overlay, the expected log evidence is an `[OAR-BA2] Indexed ...` line followed by a non-zero replacement count and successful `[OAR-Cache] Loaded` entries for resource paths. A clean game restart is required for this check.
