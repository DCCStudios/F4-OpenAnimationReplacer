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
- Retired clone and backing-file buffers are retained for the process
  lifetime. There is no heuristic FIFO eviction because the inspected clip
  bindings expose no reliable liveness signal; an age/count cap could free a
  buffer still referenced by Havok.

## Non-Goals

This change does not alter BA2 enumeration, TrackFilter behavior, annotation
dispatch, or TAEF. Those changes remain separate so this PR can be reviewed
and reverted independently.
