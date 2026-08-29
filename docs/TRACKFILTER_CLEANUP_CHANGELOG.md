# Track-Filter Loop and Timing Cleanup

This file records the review-cleanup scope for the generic track-filter lifecycle change. It describes OAR behavior only and contains no TAEF-specific conditions.

## 2026-08-27 review cleanup

- `src/Hooks.cpp`: keep donor mapping, loop clocks, one-shot completion, starvation, self-advance, and Camera frame-zero samples per source clip. The actor/filter state retains only the shared output blend state and standalone special-idle fallback.
- `src/Hooks.cpp`: a loop source no longer suppresses the completion handling of a different one-shot source. A filter-level fade is started only when no configured loop source remains active.
- `src/Hooks.cpp`: clear donor-specific Camera state whenever the source/donor pairing changes, and recognize `multi:` suffixes using the same leaf-prefix rule as ordinary suffixes.
- `src/Hooks.cpp`: loop-clock diagnostics are disabled unless verbose logging is enabled, so the normal Generate path does not emit periodic messages while holding the track-filter lock.
- `src/Parsing.cpp` and `src/UI/UIMain.cpp`: keep `loopSourcePrefixes` author-owned until a UI editor and a clear operation exist. Generated `user.json` data can no longer permanently shadow an author's list or fail parsing through an invalid vector element.

The changes preserve the existing replacement selection and condition model. They only separate source-local playback state and remove avoidable hot-path diagnostic work.

## Verification

- The source-only cleanup branch is based on upstream `1.1.10` (`origin/master` commit `330f0db`).
- The `RelWithDebInfo` build passed after a temporary local ImGui signature adaptation; that adaptation was restored and is not part of this change.
