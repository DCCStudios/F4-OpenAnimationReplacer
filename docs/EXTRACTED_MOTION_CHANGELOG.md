# Extracted Motion Preservation

This document records the implementation boundary for the extracted-motion
replacement feature proposed in PR #7.

## Scope

- Preserve a replacement animation's `m_extractedMotion` only when its
  packfile identifies the pointed-to object as
  `hkaDefaultAnimatedReferenceFrame`.
- Resolve that class's runtime vtable from CommonLib's Address Library entry;
  do not infer it from whichever original animation happened to be processed
  first.
- Keep the feature opt-in through `preserveExtractedMotion`, which remains
  disabled by default for pose-only replacements.

## Review Fixes

- Weapon-only donors no longer depend on an original animation that already
  has extracted motion before the feature can work.
- Parsed entries record the exact extracted-motion virtual fixup. The clone
  builder rejects unclassified pointers and the separate
  `hkaAnimatedReferenceFrame` class instead of applying the default class's
  vtable to both types.
- Reference-frame validation checks the complete fixed-size object layout,
  finite vectors and duration, and the bounds of the serialized sample array
  before the pointer is exposed to Havok.
- A vtable becoming available after preload patches all matching serialized
  fixups and retires preserve-enabled clones built before that point.
- Changing the option during a config reload retires existing clones so the
  new policy takes effect on the next build.
- The reference-frame vtable is resolved lazily, only after a selected entry
  opts into `preserveExtractedMotion`, through the required CommonLib Address
  Library entry `587967`. A missing or mismatched Address Library database
  follows CommonLib's fatal dependency contract when the opt-in feature is
  used; pose-only replacements do not require this lookup.
- Retired clone and backing-file buffers are retained for the process lifetime
  as an intentional memory-for-lifetime tradeoff. There is no heuristic FIFO
  eviction because the inspected clip bindings expose no reliable liveness
  signal; an age/count cap could free a buffer still referenced by Havok. This
  is a known limitation of the current implementation, not an unbounded live
  cache policy that can be reclaimed safely without a future liveness API.
- Inline parsing, first-vtable retroactive repair, and deferred cache repair all
  use the same bounds-checked virtual-fixup patch helper.
- The reference-frame validator uses the Fallout 4 runtime layout: duration at
  `+0x40` and the sample array at `+0x48/+0x50`. This keeps valid weapon HKX
  motion donors from being rejected by stale field offsets.

## Non-Goals

This change does not alter BA2 enumeration, TrackFilter behavior, annotation
dispatch, or TAEF. Those changes remain separate so this PR can be reviewed
and reverted independently.
