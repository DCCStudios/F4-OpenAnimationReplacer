"""Extended design-verification suite for F4 OAR Conversion Tool.

Covers the locked design: flexible ESP/subgraph inputs, destination nesting,
TR rename+config, Idle Empty layout+config, ESP TR cleanup, overwrite, batch,
byte-identical copies, and one-pass dual ops.

Run:  python verification_test.py
"""

from __future__ import annotations

import filecmp
import json
import struct
import sys
import tempfile
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from oar_conversion_tool.convert_idle import plan_idle_empty
from oar_conversion_tool.convert_tr import execute_plan, plan_tactical_reload
from oar_conversion_tool.engine import JobOptions, build_preview, discover_roots, discover_subgraphs, run_job, validate_inputs
from oar_conversion_tool.esp_io import find_used_master_indices, parse_esp, patch_esp_remove_tr
from oar_conversion_tool.paths import AnimRoot, find_files_ci, resolve_oar_base, scan_anim_roots
from oar_conversion_tool.subgraph import find_subgraph_files_near, load_subgraph_file

TEST_SCAR = ROOT / "TestAssets" / "SCAR-H"
TEST_SIG = ROOT / "TestAssets" / "Sig Sauer Pack"
SCAR_SUBGRAPH = TEST_SCAR / "Meshes" / "actors" / "SubGraphData_HumanRaceSubGraphDataSCAR.txt"
SCAR_ESP = TEST_SCAR / "SCAR-H.esp"


class Failures(list):
    def check(self, cond: bool, msg: str) -> None:
        # Avoid Windows cp1252 crashes on fancy punctuation in console output.
        safe = msg.encode("ascii", errors="replace").decode("ascii")
        if cond:
            print(f"  PASS  {safe}")
        else:
            print(f"  FAIL  {safe}")
            self.append(msg)


def _scar_root():
    roots = scan_anim_roots([TEST_SCAR], include_1st=True, require_file="WPNReloadReserve.hkx")
    return next(r for r in roots if r.name.lower() == "scar")


def _p226_root():
    roots = scan_anim_roots([TEST_SIG], include_1st=True, require_file="WPNReload.hkx")
    return next(r for r in roots if r.name.lower() == "p226")


# ---------------------------------------------------------------------------
# 1. Input matrix
# ---------------------------------------------------------------------------

def test_input_matrix(f: Failures) -> None:
    print("\n[1] input matrix (ESP and/or subgraph, >=1)")

    # neither
    errs = validate_inputs(JobOptions(destination=Path("x"), do_tactical_reload=True))
    f.check(any("ESP or subgraph" in e for e in errs), f"reject neither: {errs}")

    # subgraph only + source
    opts = JobOptions(
        subgraph_paths=[SCAR_SUBGRAPH],
        source_dirs=[TEST_SCAR],
        destination=Path("x"),
        do_tactical_reload=True,
    )
    f.check(validate_inputs(opts) == [], f"accept subgraph+source: {validate_inputs(opts)}")

    # ESP only (auto-adds parent as source)
    opts = JobOptions(esp_path=SCAR_ESP, destination=Path("x"), do_tactical_reload=True)
    f.check(validate_inputs(opts) == [], f"accept ESP-only: {validate_inputs(opts)}")
    f.check(TEST_SCAR.resolve() in [p.resolve() for p in opts.source_dirs], "ESP-only injects parent source dir")
    # ESP-only must be enough to discover anim roots under the ESP's mod folder
    roots = discover_roots(opts)
    f.check(any(r.name.lower() == "scar" for r in roots), f"ESP-only discovers SCAR root: {[r.name for r in roots]}")

    # both
    opts = JobOptions(
        esp_path=SCAR_ESP,
        subgraph_paths=[SCAR_SUBGRAPH],
        source_dirs=[TEST_SCAR],
        destination=Path("x"),
        do_tactical_reload=True,
    )
    f.check(validate_inputs(opts) == [], "accept ESP+subgraph+source")

    # subgraph without source and without ESP
    opts = JobOptions(subgraph_paths=[SCAR_SUBGRAPH], destination=Path("x"), do_tactical_reload=True)
    errs = validate_inputs(opts)
    f.check(any("source directory" in e for e in errs), f"subgraph alone needs source anims: {errs}")

    # no operations
    opts = JobOptions(
        esp_path=SCAR_ESP,
        destination=Path("x"),
        do_tactical_reload=False,
        do_idle_empty=False,
        patch_esp=False,
    )
    errs = validate_inputs(opts)
    f.check(any("at least one operation" in e for e in errs), f"reject no ops: {errs}")


# ---------------------------------------------------------------------------
# 2. Destination nesting
# ---------------------------------------------------------------------------

def test_destination_nesting(f: Failures) -> None:
    print("\n[2] destination nesting rules")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td)

        # bare root → full Meshes/.../OpenAnimationReplacer
        oar = resolve_oar_base(dest, "1st")
        expected_suffix = Path("Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer")
        f.check(
            oar == dest / expected_suffix,
            f"bare root builds full path: {oar}",
        )

        # existing OAR folder as dest
        oar.mkdir(parents=True)
        f.check(resolve_oar_base(oar, "1st") == oar.resolve(), "dest=OAR reuses it")

        # child pack under OAR
        pack = oar / "ExistingPack"
        pack.mkdir()
        f.check(resolve_oar_base(pack, "1st") == oar.resolve(), "dest=pack walks up to OAR")

        # submod under pack
        sub = pack / "ExistingSub"
        sub.mkdir()
        f.check(resolve_oar_base(sub, "1st") == oar.resolve(), "dest=submod walks up to OAR")

        # destination that contains a proper Animations/OpenAnimationReplacer child
        other = dest / "MyMod"
        nested_oar = (
            other / "Meshes" / "Actors" / "Character" / "_1stPerson" / "Animations" / "OpenAnimationReplacer"
        )
        nested_oar.mkdir(parents=True)
        f.check(
            resolve_oar_base(other, "1st") == nested_oar.resolve(),
            "dest containing game OAR child uses it",
        )

        # CRITICAL: path under a folder merely named OpenAnimationReplacer (this repo)
        # must NOT resolve to that repo folder.
        fake_repo = dest / "OpenAnimationReplacer" / "tools" / "OARConversionTool" / "out"
        fake_repo.mkdir(parents=True)
        resolved = resolve_oar_base(fake_repo, "1st")
        f.check(
            resolved == fake_repo / expected_suffix,
            f"repo-named ancestor does not hijack dest: {resolved}",
        )
        f.check(
            resolved != (dest / "OpenAnimationReplacer").resolve(),
            "must not write into bare OpenAnimationReplacer repo folder",
        )


# ---------------------------------------------------------------------------
# 3. TR: complete path coverage + byte identity + naming
# ---------------------------------------------------------------------------

def test_tr_complete_coverage(f: Failures) -> None:
    print("\n[3] TR complete coverage + byte-identical rename")
    scar = _scar_root()
    sources = find_files_ci(scar.absolute_path, "WPNReloadReserve.hkx")
    f.check(len(sources) == 20, f"SCAR has 20 Reserve files (got {len(sources)})")

    with tempfile.TemporaryDirectory() as td:
        dest = Path(td)
        plan = plan_tactical_reload(
            [scar], dest, pack_name="SCAR Pack", submod_name="SCAR Tactical Reload"
        )
        f.check(len(plan.files) == len(sources), f"plan file count matches sources ({len(plan.files)})")
        execute_plan(plan, overwrite=False)

        # Every source maps to a dest WPNReload.hkx with identical bytes
        for src in sources:
            rel_dir = src.parent.relative_to(scar.absolute_path.parent)
            dest_file = plan.submod_dir / rel_dir / "WPNReload.hkx"
            f.check(dest_file.is_file(), f"exists {rel_dir}/WPNReload.hkx")
            if dest_file.is_file():
                f.check(
                    filecmp.cmp(src, dest_file, shallow=False),
                    f"bytes match Reserve->Reload for {rel_dir}",
                )
            f.check(dest_file.name == "WPNReload.hkx", "dest name is exactly WPNReload.hkx")

        # No Reserve names left; no idle files accidentally created
        f.check(not list(plan.submod_dir.rglob("*Reserve*")), "no Reserve names in TR output")
        f.check(not list(plan.submod_dir.rglob("*IdleReady*")), "TR op did not create Idle files")

        # Pack + submod configs
        f.check((plan.pack_dir / "config.json").is_file(), "pack config.json written")
        pack_cfg = json.loads((plan.pack_dir / "config.json").read_text(encoding="utf-8"))
        f.check(pack_cfg.get("name") == "SCAR Pack", "pack name")


# ---------------------------------------------------------------------------
# 4. TR config schema (P890-style, no eventsOnEnd)
# ---------------------------------------------------------------------------

def test_tr_config_schema(f: Failures) -> None:
    print("\n[4] TR config schema")
    scar = _scar_root()
    with tempfile.TemporaryDirectory() as td:
        # without IsEquipped
        plan = plan_tactical_reload([scar], Path(td) / "a", pack_name="P", submod_name="TR")
        execute_plan(plan, overwrite=True)
        cfg = json.loads(plan.config_path.read_text(encoding="utf-8"))
        conds = {c["condition"] for c in cfg["conditions"]}
        f.check(conds == {"IsForm", "IsWeaponDrawn", "CurrentMagazineAmmo"}, f"base conditions: {conds}")
        f.check("IsEquipped" not in conds, "no IsEquipped when not provided")
        f.check("eventsOnEnd" not in cfg, "no eventsOnEnd")
        f.check(cfg.get("playOnceFullBody") is True, "playOnceFullBody")
        f.check(cfg.get("priority") == 3000, "default priority 3000")
        f.check(cfg["trackFilter"].get("enabled") is False, "trackFilter disabled for TR")
        ammo = next(c for c in cfg["conditions"] if c["condition"] == "CurrentMagazineAmmo")
        f.check(ammo.get("negated") is True, "ammo != 0")
        f.check(ammo["numericValue"]["value"] == 0.0, "ammo compare value 0")

        # with IsEquipped
        plan2 = plan_tactical_reload(
            [scar],
            Path(td) / "b",
            pack_name="P2",
            submod_name="TR2",
            equipped={"formID": "0x2E1F", "pluginName": "SCAR-H.esp"},
        )
        execute_plan(plan2, overwrite=True)
        cfg2 = json.loads(plan2.config_path.read_text(encoding="utf-8"))
        eq = next(c for c in cfg2["conditions"] if c["condition"] == "IsEquipped")
        f.check(eq["Form"]["formID"] == "0x2E1F", "IsEquipped formID")
        f.check(eq["Form"]["pluginName"] == "SCAR-H.esp", "IsEquipped plugin")


# ---------------------------------------------------------------------------
# 5. Idle Empty layout + content + equip gating
# ---------------------------------------------------------------------------

def test_idle_empty_design(f: Failures) -> None:
    print("\n[5] Idle Empty layout, bytes, equip gating")
    p226 = _p226_root()
    with tempfile.TemporaryDirectory() as td:
        plan = plan_idle_empty([p226], Path(td), pack_name="Sig", submod_name="Idle Empty")
        execute_plan(plan, overwrite=False)

        base = plan.submod_dir / "P226"
        reload_src = next(p for p in p226.absolute_path.iterdir() if p.name.lower() == "wpnreload.hkx")
        for name in ("WPNIdleReady.hkx", "WPNIdleReadyA.hkx", "WPNIdleReadyB.hkx",
                     "WPNIdleReadyC.hkx", "WPNIdleReadyD.hkx"):
            dest = base / name
            f.check(dest.is_file(), f"base has {name}")
            f.check(filecmp.cmp(reload_src, dest, shallow=False), f"{name} bytes == WPNReload")

        f.check((base / "WPNEquip.hkx").is_file(), "base WPNEquip present (source had it)")
        f.check((base / "WPNEquipFast.hkx").is_file(), "base WPNEquipFast present")
        f.check(filecmp.cmp(reload_src, base / "WPNEquip.hkx", shallow=False), "Equip bytes from Reload")

        f.check((base / "WPNIdleSighted.hkx").is_file(), "base WPNIdleSighted present (source had it)")
        f.check(
            filecmp.cmp(reload_src, base / "WPNIdleSighted.hkx", shallow=False),
            "WPNIdleSighted bytes from Reload",
        )
        f.check(
            not (base / "WPNIdleSightedWobble.hkx").exists(),
            "WPNIdleSightedWobble untouched (different animation, not requested)",
        )

        # EXT has WPNReload but no IdleReady/Equip in source → invent nothing there.
        ext = plan.submod_dir / "P226" / "EXT"
        f.check(not (ext / "WPNIdleReady.hkx").exists(), "did not invent EXT idle (source has none)")
        src_ext_equip = list(find_files_ci(p226.absolute_path / "EXT", "WPNEquip.hkx"))
        f.check(len(src_ext_equip) == 0, "source EXT has no WPNEquip")
        f.check(not (ext / "WPNEquip.hkx").exists(), "did not invent WPNEquip in EXT")

        cfg = json.loads(plan.config_path.read_text(encoding="utf-8"))
        f.check(cfg["trackFilter"]["sampleFrame"] == 0.0, "sampleFrame 0")
        f.check(cfg["trackFilter"]["bones"] == ["WeaponBolt"], "default bone")
        f.check(abs(cfg["deactivationDelay"] - 0.1) < 1e-6, "deactivationDelay 0.1")
        f.check(cfg["trackFilter"]["enabled"] is True, "trackFilter on")
        f.check(cfg["trackFilter"]["mode"] == "override", "override mode")
        conds = {c["condition"] for c in cfg["conditions"]}
        f.check("IsReloading" in conds, "IsReloading negated present")
        reload_c = next(c for c in cfg["conditions"] if c["condition"] == "IsReloading")
        f.check(reload_c.get("negated") is True, "IsReloading negated")
        ammo = next(c for c in cfg["conditions"] if c["condition"] == "CurrentMagazineAmmo")
        f.check(ammo.get("negated") is False, "idle ammo == 0 (not negated)")

        # custom bones
        plan2 = plan_idle_empty(
            [p226], Path(td) / "b", pack_name="Sig2", submod_name="Idle2",
            bones=["WeaponBolt", "WeaponExtra1"],
        )
        execute_plan(plan2, overwrite=True)
        cfg2 = json.loads(plan2.config_path.read_text(encoding="utf-8"))
        f.check(cfg2["trackFilter"]["bones"] == ["WeaponBolt", "WeaponExtra1"], "custom bones")


# ---------------------------------------------------------------------------
# 6. Dual ops one pass + overwrite guard
# ---------------------------------------------------------------------------

def test_dual_ops_and_overwrite(f: Failures) -> None:
    print("\n[6] dual ops one pass + overwrite guard")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td)
        opts = JobOptions(
            # No esp_path here on purpose: SCAR-H.esp's SCAR root is shared by two
            # weapons (base + "_Unique" variant), which the per-weapon expand (see
            # weapon_match.expand_groups_per_weapon, tests 15/17) now splits into two
            # SubMods - out of scope for this test, which only cares about dual TR+Idle
            # output and the overwrite guard, so it sticks to the manual equipped override.
            source_dirs=[TEST_SCAR],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=dest,
            do_tactical_reload=True,
            do_idle_empty=True,
            selected_roots=[_scar_root()],
            pack_name="SCAR Dual",
            tr_submod_name="SCAR Tactical Reload",
            idle_submod_name="SCAR Idle Empty",
            equipped_form_id="0x2E1F",
            equipped_plugin="SCAR-H.esp",
            overwrite=False,
        )
        result = run_job(opts)
        f.check(not result.errors, f"dual run errors: {result.errors}")
        f.check(len(result.plans) == 2, f"two plans: {len(result.plans)}")

        tr_dir = dest / "Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/SCAR Dual/SCAR Tactical Reload"
        idle_dir = dest / "Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/SCAR Dual/SCAR Idle Empty"
        f.check((tr_dir / "SCAR" / "WPNReload.hkx").is_file(), "dual: TR output")
        f.check((idle_dir / "SCAR" / "WPNIdleReady.hkx").is_file(), "dual: Idle output")
        f.check((idle_dir / "SCAR" / "WPNEquip.hkx").is_file(), "dual: Idle equip from SCAR base")

        # overwrite guard
        opts2 = JobOptions(
            source_dirs=[TEST_SCAR],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=dest,
            do_tactical_reload=True,
            selected_roots=[_scar_root()],
            pack_name="SCAR Dual",
            tr_submod_name="SCAR Tactical Reload",
            overwrite=False,
        )
        result2 = run_job(opts2)
        f.check(len(result2.errors) > 0, f"refuse overwrite without flag: {result2.errors}")

        opts2.overwrite = True
        result3 = run_job(opts2)
        f.check(not result3.errors, f"overwrite=True succeeds: {result3.errors}")


# ---------------------------------------------------------------------------
# 7. Batch multi-source / multi-root
# ---------------------------------------------------------------------------

def test_batch_multi_root(f: Failures) -> None:
    print("\n[7] batch multi-source multi-root")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td)
        scar = _scar_root()
        p226 = _p226_root()
        # Same pack, one TR submod containing both roots' mirrored trees
        plan = plan_tactical_reload(
            [scar, p226], dest, pack_name="Batch", submod_name="Batch TR"
        )
        execute_plan(plan, overwrite=True)
        f.check((plan.submod_dir / "SCAR" / "WPNReload.hkx").is_file(), "batch has SCAR")
        f.check((plan.submod_dir / "P226" / "WPNReload.hkx").is_file(), "batch has P226")
        f.check((plan.submod_dir / "P226" / "EXT" / "WPNReload.hkx").is_file(), "batch has P226/EXT")


# ---------------------------------------------------------------------------
# 8. Subgraph discovery + 1st-only default
# ---------------------------------------------------------------------------

def test_subgraph_and_perspective(f: Failures) -> None:
    print("\n[8] subgraph discovery + 1st-only default")
    found = find_subgraph_files_near(TEST_SCAR)
    f.check(any(p.name.lower().endswith("scar.txt") or "scar" in p.name.lower() for p in found),
            f"auto-find SCAR subgraph near mod: {[p.name for p in found]}")

    opts = JobOptions(source_dirs=[TEST_SCAR], subgraph_paths=[], esp_path=SCAR_ESP, destination=Path("x"))
    sgs = discover_subgraphs(opts)
    f.check(len(sgs) >= 1, f"engine discovers subgraph via ESP parent: {len(sgs)}")

    data = load_subgraph_file(SCAR_SUBGRAPH)
    first = data.weapon_entries(include_1st=True, include_3rd=False)
    third = data.weapon_entries(include_1st=False, include_3rd=True)
    f.check(len(first) > 0 and len(third) > 0, f"subgraph has 1st={len(first)} and 3rd={len(third)}")

    roots_1st = scan_anim_roots([TEST_SCAR], include_1st=True, include_3rd=False, require_file="WPNReloadReserve.hkx")
    f.check(all(r.perspective == "1st" for r in roots_1st), "default scan is 1st only")

    # 3rd-person weapon anims for SCAR live under Animations/Weapon/SCAR — our scanner
    # looks at Character/Animations children for 3rd. Document current behavior.
    roots_3rd = scan_anim_roots([TEST_SCAR], include_1st=False, include_3rd=True, require_file="WPNReload.hkx")
    f.check(isinstance(roots_3rd, list), f"3rd scan returns list (len={len(roots_3rd)})")


# ---------------------------------------------------------------------------
# 9. ESP cleanup on real SCAR — deep integrity
# ---------------------------------------------------------------------------

def test_esp_scar_deep(f: Failures) -> None:
    print("\n[9] ESP SCAR-H deep integrity")
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "SCAR-H.esp"
        dst = Path(td) / "SCAR-H_OAR.esp"
        src.write_bytes(SCAR_ESP.read_bytes())
        before = parse_esp(src)

        report = patch_esp_remove_tr(src, dst, remove_keyword=True, remove_tr_master_if_unused=True)
        after = parse_esp(dst)

        f.check(report["tr_master_removed"] is True, "TR master removed")
        f.check(report["keywords_removed"] == 2, f"exactly 2 KWDA removals: {report['keywords_removed']}")
        f.check(after.masters == ["Fallout4.esm"], f"masters={after.masters}")
        f.check(len(after.weapons) == len(before.weapons), "weapon count preserved")

        # No weapon still references old TR keyword form (any high-byte)
        for w in after.weapons:
            f.check(
                all((k & 0xFFFFFF) != 0x001734 for k in w["keywords"]),
                f"{w['edid']}: no *1734 keyword residue",
            )
            # AnimsSCAR object id 0x3D58 retained (high byte remapped)
            f.check(
                any((k & 0xFFFFFF) == 0x003D58 for k in w["keywords"]),
                f"{w['edid']}: AnimsSCAR retained",
            )

        # Usage detector must NOT claim TR still used after strip
        # Simulate: stripped file before master drop — write intermediate by patching keyword only on copy
        # After final patch, tr index gone so N/A. Check pre-drop usage on a keyword-stripped-only file:
        tmp = Path(td) / "stripped_only.esp"
        patch_esp_remove_tr(src, tmp, remove_keyword=True, remove_tr_master_if_unused=False)
        stripped_info = parse_esp(tmp)
        f.check(stripped_info.tr_master_index == 1, "strip-only still has TR master listed")
        used = find_used_master_indices(tmp, len(stripped_info.masters))
        f.check(1 not in used, f"after KWDA strip, master 1 unused (used={sorted(used)})")

        # False-positive guard: raw scan of MHDT would mark master 1; our detector must not
        f.check(0 in used, "Fallout4.esm still marked used")


# ---------------------------------------------------------------------------
# 10. ESP: keep TR master when a real FormID field still references it
# ---------------------------------------------------------------------------

def test_esp_keep_master_when_referenced(f: Failures) -> None:
    print("\n[10] ESP keeps TR master if a real FormID field still refs it")
    with tempfile.TemporaryDirectory() as td:
        # Minimal ESP: Fallout4 + TR masters, WEAP with KWDA TR keyword AND NAME pointing at TR form
        path = Path(td) / "KeepTR.esp"
        _write_esp_with_tr_name_ref(path)
        info = parse_esp(path)
        f.check(info.tr_master_index == 1, "fixture has TR master")

        dst = Path(td) / "KeepTR_out.esp"
        report = patch_esp_remove_tr(path, dst, remove_keyword=True, remove_tr_master_if_unused=True)
        f.check(report["keywords_removed"] >= 1, "keyword stripped")
        f.check(report["tr_master_removed"] is False, f"master kept: {report}")
        after = parse_esp(dst)
        f.check("TacticalReload.esm" in after.masters, f"TR still in masters: {after.masters}")


def _write_esp_with_tr_name_ref(path: Path) -> None:
    """TES4 + WEAP GRUP: KWDA has TR kw; NAME also has a TR FormID so master must stay."""
    subs = [
        (b"HEDR", struct.pack("<fiI", 0.95, 1, 0)),
        (b"CNAM", b"KeepTR\x00"),
    ]
    for m in ("Fallout4.esm", "TacticalReload.esm"):
        subs.append((b"MAST", (m + "\x00").encode("latin-1")))
        subs.append((b"DATA", struct.pack("<2I", 0, 0)))
    tes4_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in subs)
    tes4 = b"TES4" + struct.pack("<I", len(tes4_body)) + struct.pack("<I", 0) + struct.pack("<I", 0) + b"\x00\x00\x00\x00\x83\x00\x00\x00" + tes4_body

    tr_kw = (1 << 24) | 0x000801
    tr_name = (1 << 24) | 0x000900
    weap_subs = [
        (b"EDID", b"KeepWeapon\x00"),
        (b"KSIZ", struct.pack("<I", 1)),
        (b"KWDA", struct.pack("<I", tr_kw)),
        (b"NAME", struct.pack("<I", tr_name)),  # real FormID field → keeps master
    ]
    weap_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in weap_subs)
    weap_fid = (2 << 24) | 0x000800
    weap = b"WEAP" + struct.pack("<I", len(weap_body)) + struct.pack("<I", 0) + struct.pack("<I", weap_fid) + b"\x00\x00\x00\x00\x83\x00\x00\x00" + weap_body
    grup = b"GRUP" + struct.pack("<I", 24 + len(weap)) + b"WEAP" + struct.pack("<I", 0) + b"\x00\x00\x00\x00\x00\x00\x00\x00" + weap
    path.write_bytes(tes4 + grup)


# ---------------------------------------------------------------------------
# 11. Case-insensitive Reserve pickup (lowercase filenames in SCAR 10rd etc.)
# ---------------------------------------------------------------------------

def test_case_insensitive_reserve(f: Failures) -> None:
    print("\n[11] case-insensitive WPNReloadReserve pickup")
    scar = _scar_root()
    lower = [p for p in find_files_ci(scar.absolute_path, "WPNReloadReserve.hkx") if p.name != "WPNReloadReserve.hkx"]
    f.check(len(lower) > 0, f"fixture has lowercase reserve names: {[p.name for p in lower[:3]]}")
    with tempfile.TemporaryDirectory() as td:
        plan = plan_tactical_reload([scar], Path(td), pack_name="P", submod_name="TR")
        execute_plan(plan, overwrite=True)
        # 10rd uses lowercase source
        dest = plan.submod_dir / "SCAR" / "10rd" / "WPNReload.hkx"
        f.check(dest.is_file(), "lowercase reserve still converted to WPNReload.hkx")


# ---------------------------------------------------------------------------
# 12. Preview does not write files
# ---------------------------------------------------------------------------

def test_preview_is_dry(f: Failures) -> None:
    print("\n[12] preview is dry-run")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td)
        opts = JobOptions(
            source_dirs=[TEST_SCAR],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=dest,
            do_tactical_reload=True,
            selected_roots=[_scar_root()],
            pack_name="Dry",
            tr_submod_name="TR",
        )
        preview = build_preview(opts)
        f.check(len(preview.plans) == 1, "preview has plan")
        f.check(len(list(dest.rglob("*.hkx"))) == 0, "preview wrote no hkx")
        f.check(len(list(dest.rglob("config.json"))) == 0, "preview wrote no config")


# ---------------------------------------------------------------------------
# 13. Engine end-to-end with ESP patch alongside anims
# ---------------------------------------------------------------------------

def test_job_queue_different_options(f: Failures) -> None:
    """Two queued jobs with different dest/pack/submod names must not clobber each other."""
    print("\n[14] job queue with per-job options")
    with tempfile.TemporaryDirectory() as td:
        dest_a = Path(td) / "outA"
        dest_b = Path(td) / "outB"
        scar = _scar_root()
        p226 = _p226_root()
        job_a = JobOptions(
            source_dirs=[TEST_SCAR],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=dest_a,
            do_tactical_reload=True,
            selected_roots=[scar],
            pack_name="SCAR Pack",
            tr_submod_name="SCAR Tactical Reload",
            overwrite=True,
        )
        job_b = JobOptions(
            source_dirs=[TEST_SIG],
            subgraph_paths=[SCAR_SUBGRAPH],  # any subgraph satisfies >=1 input with source
            destination=dest_b,
            do_tactical_reload=True,
            selected_roots=[p226],
            pack_name="Sig Pack",
            tr_submod_name="P226 Tactical Reload",
            overwrite=True,
        )
        # Sig needs its own subgraph or ESP; provide a nearby subgraph discovery via source only:
        # validate requires ESP or subgraph file. Use a copy of SCAR subgraph path is fine for validation,
        # anim roots come from selected_roots.
        ra = run_job(job_a)
        rb = run_job(job_b)
        f.check(not ra.errors, f"job A errors: {ra.errors}")
        f.check(not rb.errors, f"job B errors: {rb.errors}")
        a_file = dest_a / "Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/SCAR Pack/SCAR Tactical Reload/SCAR/WPNReload.hkx"
        b_file = dest_b / "Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/Sig Pack/P226 Tactical Reload/P226/WPNReload.hkx"
        f.check(a_file.is_file(), f"job A wrote its own dest: {a_file}")
        f.check(b_file.is_file(), f"job B wrote its own dest: {b_file}")
        f.check(not (dest_a / "Meshes").joinpath(
            "Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/Sig Pack"
        ).exists(), "job A dest has no Sig Pack")
        f.check(not (dest_b / "Meshes").joinpath(
            "Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/SCAR Pack"
        ).exists(), "job B dest has no SCAR Pack")


def test_engine_anims_plus_esp(f: Failures) -> None:
    print("\n[13] engine anims + ESP patch together")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td) / "out"
        esp_out = Path(td) / "SCAR-H_OAR.esp"
        opts = JobOptions(
            source_dirs=[TEST_SCAR],
            subgraph_paths=[SCAR_SUBGRAPH],
            esp_path=SCAR_ESP,
            destination=dest,
            do_tactical_reload=True,
            do_idle_empty=False,
            selected_roots=[_scar_root()],
            pack_name="SCAR",
            tr_submod_name="Tactical Reload",
            patch_esp=True,
            esp_output=esp_out,
            remove_tr_keyword=True,
            remove_tr_master=True,
            overwrite=True,
            equipped_form_id="0x2E1F",
            equipped_plugin="SCAR-H.esp",
        )
        result = run_job(opts)
        f.check(not result.errors, f"errors: {result.errors}")
        f.check(esp_out.is_file(), "ESP output written")
        info = parse_esp(esp_out)
        f.check("TacticalReload.esm" not in info.masters, f"ESP cleaned: {info.masters}")
        # SCAR-H.esp's SCAR root is shared by two weapons (base "SCAR" + its "SCAR_Unique"
        # leveled variant, both carrying AnimsSCAR) - the per-weapon expand means this run
        # produces two SubMods, not one, so the auto-derived IsEquipped wins over the
        # manually entered equipped_form_id/plugin above. Anims must land in both.
        f.check(len(result.plans) == 2, f"one SubMod per SCAR weapon: {len(result.plans)}")
        for plan in result.plans:
            f.check(
                (plan.submod_dir / "SCAR" / "WPNReload.hkx").is_file(),
                f"anims written alongside ESP patch: {plan.submod_dir}",
            )
        f.check(
            {p.submod_dir.name for p in result.plans}
            == {"Tactical Reload - SCAR", "Tactical Reload - SCAR_Unique"},
            f"SubMod names split per weapon: {[p.submod_dir.name for p in result.plans]}",
        )


# ---------------------------------------------------------------------------
# 15. Multi-weapon ESP: one root per weapon must not share one IsEquipped
# ---------------------------------------------------------------------------

def _write_multi_weapon_esp(path: Path, weapons: list[tuple[str, str]]) -> None:
    """Minimal 1-master ESP: one KYWD 'Anims<Suffix>' + one WEAP per (edid, suffix)."""
    subs = [
        (b"HEDR", struct.pack("<fiI", 0.95, 1, 0)),
        (b"CNAM", b"MultiWeapon\x00"),
    ]
    subs.append((b"MAST", b"Fallout4.esm\x00"))
    subs.append((b"DATA", struct.pack("<2I", 0, 0)))
    tes4_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in subs)
    tes4 = (
        b"TES4"
        + struct.pack("<I", len(tes4_body))
        + struct.pack("<I", 0)
        + struct.pack("<I", 0)
        + b"\x00\x00\x00\x00\x83\x00\x00\x00"
        + tes4_body
    )

    self_index = 1  # one master (Fallout4.esm) -> this plugin's own records start at index 1
    kywd_records = b""
    weap_records = b""
    next_low = 0x000800
    for i, (edid, suffix) in enumerate(weapons):
        kw_fid = (self_index << 24) | (next_low + i * 2)
        weap_fid = (self_index << 24) | (next_low + i * 2 + 1)

        kw_subs = [(b"EDID", f"Anims{suffix}\x00".encode("latin-1"))]
        kw_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in kw_subs)
        kywd_records += (
            b"KYWD" + struct.pack("<I", len(kw_body)) + struct.pack("<I", 0)
            + struct.pack("<I", kw_fid) + b"\x00\x00\x00\x00\x83\x00\x00\x00" + kw_body
        )

        weap_subs = [
            (b"EDID", f"{edid}\x00".encode("latin-1")),
            (b"KSIZ", struct.pack("<I", 1)),
            (b"KWDA", struct.pack("<I", kw_fid)),
        ]
        weap_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in weap_subs)
        weap_records += (
            b"WEAP" + struct.pack("<I", len(weap_body)) + struct.pack("<I", 0)
            + struct.pack("<I", weap_fid) + b"\x00\x00\x00\x00\x83\x00\x00\x00" + weap_body
        )

    kywd_grup = (
        b"GRUP" + struct.pack("<I", 24 + len(kywd_records)) + b"KYWD"
        + struct.pack("<I", 0) + b"\x00\x00\x00\x00\x00\x00\x00\x00" + kywd_records
    )
    weap_grup = (
        b"GRUP" + struct.pack("<I", 24 + len(weap_records)) + b"WEAP"
        + struct.pack("<I", 0) + b"\x00\x00\x00\x00\x00\x00\x00\x00" + weap_records
    )
    path.write_bytes(tes4 + kywd_grup + weap_grup)


def test_multi_weapon_esp_grouping(f: Failures) -> None:
    """A 2-weapon ESP (SCAR root has a subgraph dump, P226 root has none) must produce
    two separate SubMods, each gated by its own weapon's IsEquipped - never one shared
    SubMod whose single condition would silently disable the other weapon's reload."""
    print("\n[15] multi-weapon ESP: per-weapon SubMod split")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td) / "out"
        esp_path = Path(td) / "MultiWeapon.esp"
        # "SCAR" is matched via the real shipped subgraph dump (Target Keyword AnimsSCAR).
        # "P226" has no subgraph at all in TestAssets/Sig Sauer Pack - matched purely via
        # the Anims<RootName> keyword-suffix heuristic. Covers both matching paths at once.
        _write_multi_weapon_esp(esp_path, [("SCARWeapon", "SCAR"), ("P226Weapon", "P226")])

        scar = _scar_root()
        p226 = _p226_root()
        opts = JobOptions(
            source_dirs=[TEST_SCAR, TEST_SIG],
            subgraph_paths=[SCAR_SUBGRAPH],
            esp_path=esp_path,
            destination=dest,
            do_tactical_reload=True,
            selected_roots=[scar, p226],
            pack_name="Multi Pack",
            tr_submod_name="Tactical Reload",
            overwrite=True,
        )
        result = run_job(opts)
        f.check(not result.errors, f"multi-weapon run errors: {result.errors}")
        f.check(len(result.plans) == 2, f"one SubMod per weapon: {len(result.plans)} plan(s)")

        by_root: dict[str, Path] = {}
        for plan in result.plans:
            for root_name in ("SCAR", "P226"):
                if (plan.submod_dir / root_name).is_dir():
                    by_root[root_name] = plan.submod_dir
        f.check(set(by_root) == {"SCAR", "P226"}, f"both weapons got their own SubMod: {sorted(by_root)}")

        scar_dir = by_root.get("SCAR")
        p226_dir = by_root.get("P226")
        f.check(scar_dir != p226_dir, "SCAR and P226 SubMods are different folders")
        if scar_dir and p226_dir:
            f.check(not (scar_dir / "P226").exists(), "SCAR SubMod does not also contain P226's tree")
            f.check(not (p226_dir / "SCAR").exists(), "P226 SubMod does not also contain SCAR's tree")

            scar_cfg = json.loads((scar_dir / "config.json").read_text(encoding="utf-8"))
            p226_cfg = json.loads((p226_dir / "config.json").read_text(encoding="utf-8"))
            scar_eq = next((c for c in scar_cfg["conditions"] if c["condition"] == "IsEquipped"), None)
            p226_eq = next((c for c in p226_cfg["conditions"] if c["condition"] == "IsEquipped"), None)
            f.check(scar_eq is not None, "SCAR SubMod has its own IsEquipped")
            f.check(p226_eq is not None, "P226 SubMod has its own IsEquipped")
            if scar_eq and p226_eq:
                f.check(scar_eq["Form"]["pluginName"] == "MultiWeapon.esp", "SCAR IsEquipped plugin")
                f.check(p226_eq["Form"]["pluginName"] == "MultiWeapon.esp", "P226 IsEquipped plugin")
                f.check(
                    scar_eq["Form"]["formID"] != p226_eq["Form"]["formID"],
                    f"SCAR and P226 IsEquipped reference different forms: "
                    f"{scar_eq['Form']['formID']} vs {p226_eq['Form']['formID']}",
                )

            info = parse_esp(esp_path)
            scar_weap = next(w for w in info.weapons if w["edid"] == "SCARWeapon")
            p226_weap = next(w for w in info.weapons if w["edid"] == "P226Weapon")
            f.check(scar_eq is not None and scar_eq["Form"]["formID"] == scar_weap["form_id_hex"],
                    "SCAR SubMod IsEquipped matches SCARWeapon's own FormID")
            f.check(p226_eq is not None and p226_eq["Form"]["formID"] == p226_weap["form_id_hex"],
                    "P226 SubMod IsEquipped matches P226Weapon's own FormID")


def test_unmatched_and_anims_suffix_labels(f: Failures) -> None:
    """Large packs must never emit a SubMod folder named RootA+RootB+RootC+...

    That "+"-joined unmatched-group label is what pushed EFT OAR past Windows MAX_PATH
    and made Mod Organizer 2 throw recursive_directory_iterator::operator++. Also covers
    the EFT-style ``AK400Anims`` folder <-> ``AnimsAK400`` keyword heuristic.
    """
    print("\n[16] unmatched roots stay short; AK400Anims <-> AnimsAK400")
    from oar_conversion_tool.weapon_match import (
        _MAX_LABEL_CHARS,
        match_roots_to_weapons,
    )

    with tempfile.TemporaryDirectory() as td:
        esp_path = Path(td) / "EFTStyle.esp"
        # One normal match (SCAR) plus one reversed-order Anims match (AK400).
        _write_multi_weapon_esp(
            esp_path,
            [("SCARWeapon", "SCAR"), ("AK400Weapon", "AK400")],
        )
        info = parse_esp(esp_path)

        def _fake_root(name: str) -> AnimRoot:
            return AnimRoot(
                name=name,
                perspective="1st",
                absolute_path=Path(td) / name,
                source_mod=Path(td),
                relative_under_animations=name,
            )

        # Many leftovers that share no keyword with the ESP (the EFT failure mode),
        # plus AK400Anims which must match AnimsAK400, plus the normal SCAR root.
        leftovers = [
            _fake_root(n)
            for n in (
                "APCAnims",
                "EFTPack416HK",
                "EFTPackSA58DSA",
                "EFTPackUSP",
                "Fiddler's Armaments",
                "MK47Anims",
                "MP7A2Anims",
                "SVDExtra",
            )
        ]
        roots = [_fake_root("SCAR"), _fake_root("AK400Anims"), *leftovers]
        groups = match_roots_to_weapons(roots, info, [], include_1st=True, include_3rd=False)

        matched = [g for g in groups if g.weapons]
        unmatched = [g for g in groups if not g.weapons]
        f.check(len(matched) == 2, f"SCAR + AK400Anims matched: {[g.label for g in matched]}")
        ak = next((g for g in matched if any(w.edid == "AK400Weapon" for w in g.weapons)), None)
        f.check(ak is not None, "AK400Anims matched via AnimsAK400 heuristic")
        if ak:
            f.check(
                [r.name for r in ak.roots] == ["AK400Anims"],
                f"AK400 group roots: {[r.name for r in ak.roots]}",
            )

        # Critical: unmatched leftovers must each be their own group with a short label,
        # never one mega group whose label is RootA+RootB+...
        f.check(
            len(unmatched) == len(leftovers),
            f"one unmatched group per leftover root: {len(unmatched)} vs {len(leftovers)}",
        )
        for g in unmatched:
            f.check(len(g.roots) == 1, f"unmatched group is single-root: {[r.name for r in g.roots]}")
            f.check(
                "+" not in g.label,
                f"label must not '+'-join roots (MO2 MAX_PATH bomb): {g.label!r}",
            )
            f.check(
                len(g.label) <= _MAX_LABEL_CHARS,
                f"label within {_MAX_LABEL_CHARS} chars: {g.label!r} ({len(g.label)})",
            )
            f.check(g.label == g.roots[0].name, f"label is the root name: {g.label!r}")


# ---------------------------------------------------------------------------
# 17. One root, many weapons: per-weapon expand must fan out to N SubMods
# ---------------------------------------------------------------------------

def test_one_root_many_weapons_expand(f: Failures) -> None:
    """One AnimRoot shared by five WEAPs (EFT-style: one Deagle folder used by five
    Deagle variant WEAPs) must expand into five per-weapon SubMods, each with its own
    single IsEquipped, never one SubMod with an OR of five IsEquipped conditions.
    See HANDOFF_per_weapon_submod_split.md."""
    print("\n[17] one root -> five weapons: per-weapon SubMod expand")
    from oar_conversion_tool.weapon_match import match_roots_to_weapons

    with tempfile.TemporaryDirectory() as td:
        tdp = Path(td)
        esp_path = tdp / "EFTStyle.esp"
        # All five WEAPs carry the same "AnimsDeagle" keyword suffix, matching the
        # single shared "Deagle" root purely via the keyword-suffix heuristic (no
        # subgraph dump needed) - the common case for packs like EFT.
        deagle_weapons = [
            ("WPNDeagleL5", "Deagle"),
            ("WPNDeagleL6", "Deagle"),
            ("WPNDeagleL6WTS", "Deagle"),
            ("WPNDeagleL5_357", "Deagle"),
            ("WPNMK19", "Deagle"),
        ]
        _write_multi_weapon_esp(esp_path, deagle_weapons)
        info = parse_esp(esp_path)

        root_dir = tdp / "Deagle"
        root_dir.mkdir()
        (root_dir / "WPNReloadReserve.hkx").write_bytes(b"fake-hkx-bytes")
        deagle_root = AnimRoot(
            name="Deagle",
            perspective="1st",
            absolute_path=root_dir,
            source_mod=tdp,
            relative_under_animations="Deagle",
        )

        groups = match_roots_to_weapons([deagle_root], info, [], include_1st=True, include_3rd=False)
        matched = [g for g in groups if g.weapons]
        f.check(len(matched) == 5, f"one group per weapon: {len(matched)} group(s)")
        f.check(all(len(g.weapons) == 1 for g in matched), "each group has exactly one weapon")
        f.check(
            all([r.name for r in g.roots] == ["Deagle"] for g in matched),
            "each group still carries the shared Deagle root",
        )
        labels = [g.label for g in matched]
        f.check(len(set(labels)) == 5, f"labels are unique per weapon: {labels}")
        f.check(all("+" not in lbl for lbl in labels), f"no label '+'-joins weapons: {labels}")
        f.check(all(len(lbl) <= 48 for lbl in labels), f"labels stay within the SubMod suffix budget: {labels}")

        dest = tdp / "out"
        opts = JobOptions(
            source_dirs=[tdp],
            esp_path=esp_path,
            destination=dest,
            do_tactical_reload=True,
            do_idle_empty=False,
            selected_roots=[deagle_root],
            pack_name="EFTPack",
            tr_submod_name="Tactical Reload",
            overwrite=True,
        )
        result = run_job(opts)
        f.check(not result.errors, f"run errors: {result.errors}")
        f.check(len(result.plans) == 5, f"five TR SubMod plans: {len(result.plans)}")

        submod_dirs = {plan.submod_dir for plan in result.plans}
        f.check(len(submod_dirs) == 5, f"five distinct SubMod folders: {submod_dirs}")
        f.check(
            all("+" not in p.submod_dir.name for p in result.plans),
            f"no SubMod folder name '+'-joins weapons: {[p.submod_dir.name for p in result.plans]}",
        )

        eq_form_ids: set[str] = set()
        for plan in result.plans:
            f.check(
                (plan.submod_dir / "Deagle").is_dir(),
                f"{plan.submod_dir} contains a copy of the shared Deagle root",
            )
            cfg = json.loads((plan.submod_dir / "config.json").read_text(encoding="utf-8"))
            eq_conds = [c for c in cfg["conditions"] if c["condition"] == "IsEquipped"]
            or_conds = [c for c in cfg["conditions"] if c["condition"] == "OR"]
            f.check(
                len(eq_conds) == 1,
                f"{plan.submod_dir.name}: exactly one IsEquipped, no OR: {cfg['conditions']}",
            )
            f.check(not or_conds, f"{plan.submod_dir.name}: no OR condition present")
            if eq_conds:
                eq_form_ids.add(eq_conds[0]["Form"]["formID"])
        f.check(len(eq_form_ids) == 5, f"each SubMod gated to a distinct weapon FormID: {eq_form_ids}")


def main() -> int:
    f = Failures()
    tests = [
        test_input_matrix,
        test_destination_nesting,
        test_tr_complete_coverage,
        test_tr_config_schema,
        test_idle_empty_design,
        test_dual_ops_and_overwrite,
        test_batch_multi_root,
        test_subgraph_and_perspective,
        test_esp_scar_deep,
        test_esp_keep_master_when_referenced,
        test_case_insensitive_reserve,
        test_preview_is_dry,
        test_job_queue_different_options,
        test_engine_anims_plus_esp,
        test_multi_weapon_esp_grouping,
        test_unmatched_and_anims_suffix_labels,
        test_one_root_many_weapons_expand,
    ]
    for t in tests:
        try:
            t(f)
        except Exception:  # noqa: BLE001
            msg = f"EXCEPTION in {t.__name__}:\n{traceback.format_exc()}"
            print(f"  FAIL  {msg}")
            f.append(msg)

    print("\n==========")
    if f:
        print(f"{len(f)} failure(s)")
        for item in f:
            print(f" - {item}")
        return 1
    print(f"All {len(tests)} verification groups passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
