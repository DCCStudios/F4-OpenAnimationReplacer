# Generic BA2 Animation Support

This file records the review-cleanup scope for the generic BA2 support change. It describes OAR behavior only; it contains no TAEF-specific rules or paths.

## 2026-08-27 review cleanup

- `src/BA2Archive.cpp`: scan and index HKX names without calling Fallout's resource manager from OAR's background parser. Resource resolution is deferred until an indexed path is loaded as an actual replacement, avoiding a startup heap race while still honoring the game's active archive resolution. Sort the resulting index and expose prefix ranges for bounded submod scans.
- `src/Parsing.cpp`: consume the sorted prefix range instead of scanning the full archive index for every submod.
- `src/OpenAnimationReplacer.h` and `src/AnimationCache.*`: keep the Data-relative `resourcePath` as the archive identity used by resource loading and skeleton/perspective checks. Rebind unchanged loose or archive entries from metadata before reading the animation payload.
- `src/Hooks.cpp`: make player-graph polling generation-safe and use the resource identity when deciding whether a replacement is valid for first- or third-person skeletons.
- Remove the unused archive `uncompressedSize` field from the runtime index API.

The changes are generic to OAR replacement discovery, cache reuse, archive resource loading, and lifecycle synchronization. They do not add actor, weapon, posture, or mod-specific conditions.

## Verification

- The source-only cleanup branch is based on upstream `1.1.10` (`origin/master` commit `330f0db`).
- The build must pass both compile and link checks before this branch is used for runtime testing.
