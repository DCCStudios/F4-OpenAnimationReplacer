# Handoff: Split multi-weapon root matches into per-weapon SubMods

## Goal (outcome, not mechanism)

When one animation root folder matches several WEAP forms (common on EFT Pack: one `EFTPackDeagle` folder used by five Deagle variants), the conversion tool must emit **one OAR SubMod per weapon form**, each containing that root's animation tree and gated by a **single** `IsEquipped` for that form.

Today the GUI correctly *detects* those multi-weapon links (e.g. `EFTPackDeagle -> WPNMK19+WPNDeagleL6+...`) but the engine still keeps them as **one group**, writes **one SubMod**, and uses an **OR** of `IsEquipped` conditions. The user wants those split up.

Success looks like:

- Scan checkbox for `EFTPackDeagle` still shows it links to multiple weapons (display can stay compact).
- Preview / Run produces separate SubMod folders, one per weapon EDID (or a short safe label derived from it), each with its own `config.json` `IsEquipped`.
- Same animation files are copied into each of those SubMods (duplication is expected and correct for OAR).
- Large packs (EFT) must remain openable in Mod Organizer 2: no SubMod name may push full paths past Windows MAX_PATH (~260) for tools that do not use `\\?\`.

## Why this matters for OAR

OAR SubMod `conditions` are ANDed as a whole. A single SubMod that holds one root and an OR of five `IsEquipped` conditions *can* work for shared anims, but:

1. The user explicitly wants per-weapon SubMods (folder structure / clarity / load-order friendliness).
2. Mixing many weapons into one SubMod name or OR list was already the failure mode that blew path length on EFT OAR and broke MO2 (`recursive_directory_iterator::operator++: The system cannot find the path specified`).
3. Per-weapon SubMods match how Ozzy M4 Platform was fixed earlier (one SubMod per distinct weapon).

## What the user showed (current UI evidence)

Weapon folder checkboxes after Scan on EFT Pack look like:

- `[1st] EFTPackAUG -> WPN_AUG_A1+WPN_AUG_A3` (one root, two weapons: **not split**)
- `[1st] EFTPackDeagle -> WPNMK19+WPNDeagleL6WTS+WPNDeagleL6+WPNDeagleL5+WPNDeagleL5_357` (**not split**)
- `[1st] EFTPackGlock -> WPN_Glock17+WPN_Glock18+WPN_Glock19` (**not split**)
- `[1st] KrissVector -> WPN_Vector9+WPN_Vector45` (**not split**)
- `[1st] EFTPackDVL -> WPN_DVL-10 (0x1F5A78)` (one root, one weapon: already fine)
- `[1st] APCAnims` / `[1st] EFTPackUSP` / `[1st] SVD` with **no** arrow: unmatched (no keyword hit); already emit one SubMod per unmatched root after the MAX_PATH fix

The `+` in the checkbox text is the GUI rendering of `group.weapons` with `len > 1`. That is the smoking gun that `match_roots_to_weapons` returned one `RootWeaponGroup` with multiple `WeaponMatch` entries instead of one group per weapon.

## Current architecture (verified in source)

### Tool location

`OpenAnimationReplacer/tools/OARConversionTool/`

Key modules:

| File | Role |
|------|------|
| `oar_conversion_tool/weapon_match.py` | Match roots to WEAP forms; produce `RootWeaponGroup`s |
| `oar_conversion_tool/engine.py` | `build_preview` / `run_job`: one plan per group |
| `oar_conversion_tool/config_json.py` | `_equipped_condition`: single `IsEquipped` or OR list |
| `oar_conversion_tool/convert_tr.py` / `convert_idle.py` | Plan file copies into `Pack/SubMod/...` |
| `gui.py` | Scan UI labels via `_build_root_labels`; queue confirm dialogs |

### How matching works today

`match_roots_to_weapons` (weapon_match.py):

1. **Subgraph walk**: animation path leaf == root name → collect that row's Target Keywords → find WEAPs carrying those keywords.
2. **Keyword-suffix heuristic**: WEAP keyword `AnimsX` / `Anims_X` → token `x`; root name normalized to `x` matches. Also strips trailing `Anims`/`Anim` from the folder (`AK400Anims` ↔ `AnimsAK400`).
3. Groups by `frozenset(matched weapon form IDs)`:
   - Roots that share the **exact same set** of weapons merge into one `RootWeaponGroup`.
   - Unmatched roots: if any matched groups exist, **each unmatched root is its own group**; if nothing matched, one shared unmatched group (classic single SubMod).

### How plans are built today

`engine.build_preview`:

- Calls `match_roots_to_weapons` once.
- For each group, calls `plan_tactical_reload` / `plan_idle_empty` with:
  - `group.roots` (may be multiple folders)
  - `equipped=_equipped_for(group)` → if multiple weapons, a **list** → OR in config.json
  - `submod_name = f"{base} - {group.label}"` when `len(groups) > 1`

`RootWeaponGroup.label` today:

- 1 root → root name (so Deagle SubMod is named after the **folder**, not each weapon)
- else if weapons → weapon EDID (or first EDID + `+Nmore`)
- else → `Unmatched`
- Clamped to `_MAX_LABEL_CHARS = 48`

### Output layout (desired end state for one root → N weapons)

Destination mod root (example: `F:\Modlists\LoreOut\mods\EFT OAR`):

```
Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/
  EFTPack/                          # pack_name
    config.json
    EFTPack - Tactical Reload - WPNDeagleL5/
      config.json                   # IsEquipped only WPNDeagleL5
      EFTPackDeagle/...hkx          # copy of the shared root tree
    EFTPack - Tactical Reload - WPNDeagleL6/
      config.json
      EFTPackDeagle/...
    ... one SubMod per weapon ...
    EFTPack - Idle Empty - WPNDeagleL5/
      ...
```

Same pattern for Idle Empty when that op is enabled.

### Path length constraint (non-negotiable)

Already bitten once on this modlist:

- Bad name (now deleted from disk):  
  `EFTPack - Tactical Reload - AK400Anims+APCAnims+EFTPack416HK+...+SVD` (131 char folder)
- Full file paths reached ~303 chars → MO2 cannot open the mod.
- After deleting those mega folders, max path in EFT OAR was ~227.

Budget rule of thumb for LoreOut-style destinations:

- Prefix to pack dir ≈ 100–140 chars
- Nested attachment path under root ≈ up to ~80 chars
- **SubMod folder name (including `Pack - Tactical Reload - Label`) should stay short; label alone is capped at 48 today. Prefer weapon EDID labels, never join many EDIDs with `+`.**

## Required behavior change

### Split rule (recommended default)

After matching root → set of weapons, **expand** so each `(root, weapon)` or more precisely each **weapon** that has a non-empty set of roots becomes its own `RootWeaponGroup`:

**Preferred expansion (per weapon form):**

For each distinct matched weapon W:

- `roots` = all AnimRoots that matched a set containing W (usually one root for EFT)
- `weapons` = `[W]` only
- `label` = short form of W.edid (clamped)

Then unmatched roots stay as today (one group per unmatched root when any matches exist).

Effect on EFT Deagle:

- Before: 1 group, roots=`[EFTPackDeagle]`, weapons=`[five Deagles]`, OR IsEquipped, one SubMod
- After: 5 groups, each roots=`[EFTPackDeagle]`, weapons=`[one Deagle]`, single IsEquipped, five SubMods (five copies of the tree)

Effect on Ozzy M4 (already one root per weapon): unchanged count of SubMods.

Effect on "two roots share one weapon": one SubMod still, both roots inside, one IsEquipped. Good.

Effect on "two roots share the exact same two weapons" (rare): today one group with OR; after per-weapon expand → two SubMods, each containing **both** roots, each gated to one weapon. Good and still path-safe.

### What NOT to do

- Do not name SubMods by joining weapon EDIDs with `+` (`WPN_Glock17+WPN_Glock18+...`). That recreates the MO2 path bomb.
- Do not keep OR-`IsEquipped` as the primary multi-weapon strategy once per-weapon SubMods exist (OR can remain as a fallback helper in `config_json.py` for edge cases, but engine should pass a single form dict).
- Do not put multiple different weapon trees into one SubMod without per-weapon gating (the original multi-root bug).

## Design decisions the implementing agent must make

These are real trade-offs; pick deliberately and document in code comments.

### 1. Label source for SubMod suffix

Options:

- **A. Weapon EDID** (recommended for split-by-weapon): `EFTPack - Tactical Reload - WPNDeagleL5`
- **B. Root folder name** (current when one root): collapses all Deagle variants into one folder name again if you forget to split
- **C. Shortened EDID** (strip `WPN_` / `WPN` prefix, clamp to 48)

Recommendation: **A or C**, always derived from the **single** weapon in the group after expand. Keep `_clamp_label`. Sanitize characters illegal on Windows (`<>:"/\|?*`). Apostrophes in names like `Fiddler's Armaments` are fine on NTFS.

### 2. Duplicate disk cost

Per-weapon SubMods duplicate `.hkx` bytes. For EFT Deagle that is 5× the tree. Acceptable for correctness; mention in preview log (`N SubMods, shared root copied N times`). Do not invent junctions/symlinks for MO2/game compatibility reasons unless the user asks.

### 3. GUI checkbox display

Today one checkbox per **root**, with `-> edid+edid+...` when multi-weapon.

After the engine splits:

- Keep one checkbox per root (selection is still "include this anim folder").
- Update `_build_root_labels` / scan log so multi-weapon roots say something like `-> 5 weapons (split into 5 SubMods)` or list EDIDs without implying one folder: e.g. `-> WPNDeagleL5, WPNDeagleL6, ...` (comma, not path-like `+`).
- Fix the misleading scan summary that says `will split into {n_matched} SubMod(s)` where `n_matched` is **group** count, not **weapon** count (`gui.py` `_scan`).

### 4. When `len(groups) <= 1` (manual IsEquipped override)

`_equipped_for` still prefers the manual FormID when there is only one group. After expand, EFT will almost always have many groups, so auto IsEquipped wins. Keep that. For a true single-weapon mod, behavior stays: one group, manual override still works.

### 5. Idle Empty + Tactical Reload

Both ops iterate groups independently today. After expand, each op emits one SubMod per weapon. Same roots copied twice (TR + Idle) × N weapons. Expected.

### 6. Unmatched roots with no IsEquipped

Still no auto `IsEquipped`. They activate on ammo/drawn conditions only. That can overlap with matched SubMods for the same anim paths if a root is unmatched incorrectly; improving match quality is separate. The `Anims` trailing-strip heuristic already landed; further fuzzy matching is optional, not required for this handoff.

## Files to change

1. **`oar_conversion_tool/weapon_match.py`**
   - Add an expand step (either inside `match_roots_to_weapons` or a sibling `expand_groups_per_weapon`) that turns multi-weapon groups into one group per weapon.
   - Update module docstring: grouping key is no longer "same frozenset of weapons" for emission; matching still discovers the set, emission splits by weapon.
   - Keep unmatched splitting and `_MAX_LABEL_CHARS`.
   - Update `label` so a one-weapon group prefers **weapon EDID** (or shortened EDID) over root name when you want SubMod folders named after weapons; if you keep root name for single-weapon groups, multi-weapon splits **must** use EDID or they will collide on disk (`... - EFTPackDeagle` written five times).

   **Collision warning:** If label stays as root name while you emit five groups that each contain `EFTPackDeagle`, `_submod_name` becomes identical five times and later plans overwrite each other. **After per-weapon expand, label must be unique per group** → use weapon EDID (unique FormID/edid).

2. **`oar_conversion_tool/engine.py`**
   - `_equipped_for`: after expand, `group.weapons` should almost always be length 0 or 1; pass a single dict, not a list.
   - Log lines: print one match line per weapon SubMod.
   - Preview message counts should reflect SubMod count after expand.

3. **`gui.py`**
   - `_build_root_labels`: stop using `"+".join(w.edid)`; show multi-weapon intent without looking like one folder name.
   - `_scan` SubMod count text: count post-expand groups / weapons, not pre-expand matched group count.
   - Confirm dialog already lists plans from `build_preview`; it will pick up new plans automatically if engine is fixed.

4. **`oar_conversion_tool/config_json.py`**
   - OR helper can stay for compatibility; engine should stop needing it for the EFT case.

5. **Tests**
   - `verification_test.py`: extend multi-weapon coverage with **one root → many weapons** (synthetic ESP: one `AnimsDeagle` keyword on five WEAPs, one fake root `EFTPackDeagle`). Assert:
     - `len(plans) == 5` for TR-only
     - five distinct `submod_dir` names
     - each `config.json` has exactly one `IsEquipped` (no OR)
     - each SubMod contains the shared root folder
     - no SubMod directory name contains `+`
     - each label / full `submod_dir` name length stays under a safe bound (e.g. label ≤ 48)
   - Keep existing test 15 (SCAR + P226 two roots) and test 16 (unmatched short labels / AK400Anims heuristic).
   - Run `python smoke_test.py` and `python verification_test.py` before calling done.

## Real-world fixtures for manual check

- Modlist destination that broke MO2: `F:\Modlists\LoreOut\mods\EFT OAR`  
  Mega `+` folders were **already deleted**; remaining SubMods are the short per-root ones from the previous run. Re-convert EFT Pack into this (or a fresh) destination after the fix.
- Source: `F:\Modlists\LoreOut\mods\EFTPack2.18` (and/or its BA2 / ESP; TR patch mod may sit beside it).
- Smaller multi-weapon reference that already worked with one-root-per-weapon: Ozzy M4 Platform BA2 + ESP under LoreOut (`Ozzys_M4_Platform`).

## Workspace rules the agent must follow

- Read `.cursor/rules/` (especially operating-manual, f4se-plugin-reference is less relevant here; this is a Python utility).
- Docs in this workspace: no em dashes / en dashes / spaced hyphen used as dash punctuation (this file follows that).
- Do not commit unless the user asks.
- Untested code is a guess: run smoke + verification; ideally one real Preview against EFT Pack showing Deagle → 5 SubMod paths.
- Prefer fixing the inferred outcome (per-weapon SubMods + MO2-safe names) over preserving OR-based "technically works" behavior.

## Suggested implementation sketch

```text
match_roots_to_weapons(...) -> groups_by_frozenset

expand_groups_per_weapon(groups):
  out = []
  for g in groups:
    if not g.weapons:
      out.append(g)   # unmatched already shaped correctly
      continue
    for w in g.weapons:
      out.append(RootWeaponGroup(roots=list(g.roots), weapons=[w]))
  return out

# engine:
groups = expand_groups_per_weapon(match_roots_to_weapons(...))
# label uniqueness comes from single weapon edid per group
```

Optionally fold `expand_groups_per_weapon` into the end of `match_roots_to_weapons` so GUI `_match_groups` and engine share one code path (GUI currently calls `match_roots_to_weapons` directly). **If expand stays only in engine, Scan checkboxes will still show `edid+edid` while Run splits; better to expand in one shared place both call.**

## Testing methodology

Follow cheapest checks that can kill the approach first. Do not call the work done on “the logic looks right.” Untested code is a guess.

Working directory for all commands:

`OpenAnimationReplacer/tools/OARConversionTool`

### A. Automated suite (required before any real mod convert)

1. `python smoke_test.py`  
   Must end with `All smoke tests passed.`  
   This is the regression gate for TR rename, Idle Empty selective copy, ESP patch, BA2 extract/cleanup, and basic engine validation. If this fails, stop; the split change broke something unrelated.

2. `python verification_test.py`  
   Must end with all groups passed (currently 16; your new case adds at least one).  
   Existing cases that must keep passing after the expand:
   - **[15] multi-weapon ESP: per-weapon SubMod split** (`SCAR` + `P226`, two roots → two SubMods, distinct `IsEquipped`).
   - **[16] unmatched roots stay short; AK400Anims ↔ AnimsAK400** (no `+`-joined unmatched labels; path-safe names).

3. **New automated case you must add** (synthetic, no LoreOut dependency):
   - Build a minimal ESP with **one** keyword `AnimsDeagle` shared by **five** WEAP records (reuse `_write_multi_weapon_esp` pattern, or extend it so multiple weapons can share one suffix).
   - One fake `AnimRoot(name="EFTPackDeagle", ...)`.
   - Run `build_preview` or `run_job` with TR only into a temp destination.
   - Assert, independently:
     - plan count == 5
     - five distinct `submod_dir` paths (no overwrite collisions)
     - no `+` in any `submod_dir.name`
     - each `config.json` has exactly one condition with `"condition": "IsEquipped"` and **no** `"condition": "OR"` wrapping them
     - each SubMod contains an `EFTPackDeagle` tree (or planned file under that relative root)
     - `len(group.label) <= 48` (or whatever `_MAX_LABEL_CHARS` is) for every emitted group
   - Also assert the **two-root / one-weapon-each** path (test 15) still produces two plans, so expand does not double-count wrongly.

How we verified related fixes before this handoff (reuse the same style of evidence):

- Unit-style calls to `match_roots_to_weapons` / `_build_root_labels` with synthetic roots (no GUI).
- Driven Tkinter smoke only when GUI text matters (optional here: Scan label string).
- Path length: `max(len(str(p)) for p in Path(dest).rglob("*"))` after a real or temp write; fail if `>= 260`.

### B. Preview dry-run against real EFT inputs (required for this feature)

Use the live LoreOut assets; do **not** treat temp-only tests as enough for EFT naming.

Suggested inputs (confirm paths still exist):

- Source / BA2: under `F:\Modlists\LoreOut\mods\EFTPack2.18` (loose or `- Main.ba2` as the tool supports)
- ESP: the EFT Pack plugin next to that mod (not the `_OAR` output if one exists; prefer the non-`_OAR` autodetect)
- Destination for this check: a **throwaway** folder, e.g. `F:\Modlists\LoreOut\mods\EFT OAR Split Test`, **not** the live `EFT OAR` mod until Preview looks right

Procedure:

1. Launch via `python gui.py` (or rebuilt exe only if you intentionally packaged; source run is enough to validate logic).
2. Select BA2 or source folder + confirm ESP autodetect.
3. Enable Tactical Reload (and Idle Empty if you want parity); Rescan.
4. **Observe Scan list:** a multi-weapon root such as `EFTPackDeagle` must no longer imply one combined folder. Acceptable UI: `-> 5 weapons (will split)` or a comma-separated EDID list. Reject: display that matches the old `edid+edid+edid` “one SubMod” story without fan-out language.
5. Set destination to the throwaway folder. Click **Preview Current**.
6. In the log / confirm dialog, find Deagle (and Glock / AUG / Vector as spot checks). **Required observations:**
   - One plan line (or confirm card section) **per weapon**, not one plan for the whole `+` set
   - Paths look like `...\EFTPack - Tactical Reload - <WeaponEdid>\EFTPackDeagle\...`
   - No plan `submod_dir` name contains `+`
7. Optionally Run into the throwaway folder with Overwrite on.

### C. Filesystem / MO2 safety check (required before touching live `EFT OAR`)

After a throwaway Run (or after copying Preview-resolved paths into a small scripted convert):

```powershell
$dest = "F:\Modlists\LoreOut\mods\EFT OAR Split Test"  # your throwaway
python -c "from pathlib import Path; root=Path(r'$dest'); lens=sorted((len(str(p)),str(p)) for p in root.rglob('*')); print('count',len(lens)); print('max',lens[-1] if lens else None); print('ge260',sum(1 for L,_ in lens if L>=260))"
```

- **Pass:** `ge260 == 0` and max path preferably under ~240 for margin.
- **Fail:** any path `>= 260` → shorten labels further; do not install into MO2.

Also list SubMods under the pack and confirm Deagle weapons are **sibling folders**, not one folder:

```powershell
Get-ChildItem "...\OpenAnimationReplacer\EFTPack" -Directory |
  Where-Object { $_.Name -match 'Deagle|Glock|AUG|Vector' } |
  Select-Object Name
```

Open the throwaway folder as a mod in Mod Organizer 2 (or “Open in Explorer” from MO2). **Pass:** no `recursive_directory_iterator` error. That was the live failure mode on `EFT OAR` when mega `+` names existed.

### D. Config spot-check (required)

Pick two of the new Deagle SubMods and one single-weapon SubMod (e.g. `EFTPackDVL`):

```powershell
Get-Content "...\EFTPack - Tactical Reload - WPNDeagleL5\config.json"
```

- **Pass:** exactly one `"condition": "IsEquipped"` with that weapon’s `formID` / plugin name; no `"condition": "OR"` for that SubMod.
- Compare formIDs across the five Deagle SubMods: all different.
- Single-weapon SubMod still has one `IsEquipped` (regression).

### E. Live `EFT OAR` (only after A–D pass)

`F:\Modlists\LoreOut\mods\EFT OAR` is a modlist destination users open in MO2. The previous mega `+` folders were already removed from disk; leftover content is from the older one-SubMod-per-root run.

- Prefer converting into a clean destination, then replacing, **or** Run with Overwrite only after Preview shows correct fan-out.
- After install: open `EFT OAR` in MO2 again; confirm no directory-iterator error.
- Re-run the max-path Python one-liner against the live mod root.

### F. What we already proved historically (do not re-break)

| Claim | How it was checked |
|-------|--------------------|
| Ozzy M4: 5 weapons → 5 TR (+ 5 Idle) SubMods | Fresh Python `run_job` / Preview against Ozzy BA2+ESP; folder listing + per-SubMod `IsEquipped` |
| Unmatched `+` mega-label broke MO2 | Measured paths under `EFT OAR` up to ~303 chars; 697 paths `>= 240`; Win32/MO2 fail mode; deleted the two `*+*` SubMod dirs; max path fell to ~227 |
| `AK400Anims` ↔ `AnimsAK400` | verification_test case 16 |
| GUI weapon annotations | Instantiated `OARConversionApp`, `_scan`, read checkbox `cget("text")` |

### G. Done criteria (methodology, not hope)

You may only report done when:

1. Smoke + verification (including new one-root-many-weapons case) pass on your machine.
2. Preview (or Run) against real EFT Pack shows Deagle (and at least one other multi-weapon root) as **N distinct SubMod paths**.
3. Max path under the output tree is under 260 characters (scripted count).
4. Sample `config.json` files show single `IsEquipped`, no OR, distinct formIDs.
5. MO2 can open the output mod folder without `recursive_directory_iterator::operator++`.

If any step fails, fix and re-run from A; do not skip to live `EFT OAR`.

## Acceptance checklist

- [ ] One root matching N weapons → N TR SubMods (and N Idle SubMods if Idle enabled)
- [ ] Each SubMod `config.json` has a single `IsEquipped` (no OR for that case)
- [ ] SubMod folder names are unique and do not contain `+`-joined weapon lists
- [ ] Labels clamped; spot-check longest output path under EFT destination &lt; 260
- [ ] Ozzy-style one-root-per-weapon still one SubMod per weapon
- [ ] Unmatched roots still short per-root SubMods
- [ ] `verification_test.py` case for one-root-many-weapons passes
- [ ] `smoke_test.py` + full `verification_test.py` pass
- [ ] GUI Scan/Preview language matches actual SubMod fan-out
- [ ] Throwaway EFT Preview/Run reviewed in log + on disk
- [ ] MO2 opens the output mod without directory-iterator error

## Out of scope (unless user asks)

- Rebuilding / releasing the PyInstaller exe
- Re-converting the entire EFT Pack into LoreOut for the user
- Changing Idle Empty selective file rules, BA2 extraction, or ESP TR stripping
- Fuzzy matching every remaining unmatched EFT root (`APCAnims`, `EFTPackUSP`, etc.) beyond the existing heuristics
