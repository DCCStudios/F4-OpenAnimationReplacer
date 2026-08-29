# Generic BA2 Animation Support

This file records the review-cleanup scope for the generic BA2 support change. It describes OAR behavior only; it contains no TAEF-specific rules or paths.

## 2026-08-27 review cleanup

- `src/BA2Archive.cpp`: scan and index HKX names without calling Fallout's resource manager from OAR's background parser. Resource resolution is deferred until an indexed path is loaded as an actual replacement, avoiding a startup heap race while still honoring the game's active archive resolution. Sort the resulting index and expose prefix ranges for bounded submod scans.
- `src/Parsing.cpp`: consume the sorted prefix range instead of scanning the full archive index for every submod.
- `src/OpenAnimationReplacer.h` and `src/AnimationCache.*`: keep the Data-relative `resourcePath` as the archive identity used by resource loading and skeleton/perspective checks. Rebind unchanged loose or archive entries from metadata before reading the animation payload.
- `src/Hooks.cpp`: make player-graph polling generation-safe and use the resource identity when deciding whether a replacement is valid for first- or third-person skeletons.
- Remove the unused archive `uncompressedSize` field from the runtime index API.

The changes are generic to OAR replacement discovery, cache reuse, archive resource loading, and lifecycle synchronization. They do not add actor, weapon, posture, or mod-specific conditions.

## UI and localization follow-up

### Complete condition localization with extension packs

- Purpose: translate built-in condition names, descriptions, stub reasons, and
  editor labels without changing condition IDs or serialized JSON.
- Behavior: OAR loads the base locale and optional
  `locales/<language>.d/<unique-name>.json` extension packs in deterministic
  filename order. Missing third-party entries fall back to the original text;
  later packs can intentionally override an earlier translation.
- UI detail: labels containing ImGui `##` IDs translate only the visible prefix,
  preserving stable widget IDs.
- Genericity: no external condition is compiled into OAR. Add-on translation
  packs use the existing Conditions API strings and require no API or ABI change.

### Separate user settings from the mod-provided INI

- Purpose: prevent the Settings page from rewriting the mod-provided
  `OpenAnimationReplacer.ini` when users change language, activation key, or
  other global options.
- Behavior: OAR reads the shipped INI as defaults, overlays
  `OpenAnimationReplacer.user.ini`, and saves Settings-page changes only to the
  user file. Unspecified values continue to inherit the shipped defaults.
- Genericity: this is a generic settings-layer change and does not read or
  write any TAEF configuration.

### Finish reload state and separate visual overlays from input ownership

- Purpose: ensure `Reload All Configs` cannot leave its progress overlay or
  cursor active after the reload/editor closes.
- Behavior: reload state is finalized through an RAII guard, including failure
  paths. UI rendering and input ownership are tracked separately, so a
  visual-only progress overlay does not keep the game in menu lock mode.

### Leave menu mode when the main window is closed by its title bar

- Purpose: make the ImGui title-bar close button follow the same lifecycle as
  the activation-key and Escape paths.
- Behavior: each frame reconciles the manager state with the main window's
  actual open flag and invokes the normal menu-close path when the title-bar
  button is used.
- Genericity: this applies to all OAR UI configurations and does not depend on
  TAEF, player state, NPC state, or animation selection.

## Verification

- The source-only cleanup branch is based on upstream `1.1.10` (`origin/master` commit `330f0db`).
- The build must pass both compile and link checks before this branch is used
  for runtime testing. The UI/localization follow-up is kept on its own branch
  so it can be reviewed independently from the BA2 implementation.
