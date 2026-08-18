"""Smoke tests for F4 OAR Conversion Tool.

Run:  python smoke_test.py
Exit code 0 means all checks passed.
"""

from __future__ import annotations

import json
import struct
import sys
import tempfile
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from oar_conversion_tool.ba2_io import Ba2Error, entries_under, extract_entries, parse_ba2
from oar_conversion_tool.convert_idle import plan_idle_empty
from oar_conversion_tool.convert_tr import execute_plan, plan_tactical_reload
from oar_conversion_tool.engine import JobOptions, build_preview, discover_roots, extract_ba2_source, run_job
from oar_conversion_tool.esp_io import parse_esp, patch_esp_remove_tr
from oar_conversion_tool.paths import resolve_oar_base, scan_anim_roots
from oar_conversion_tool.subgraph import load_subgraph_file

TEST_SCAR = ROOT / "TestAssets" / "SCAR-H"
TEST_SIG = ROOT / "TestAssets" / "Sig Sauer Pack"
SCAR_SUBGRAPH = TEST_SCAR / "Meshes" / "actors" / "SubGraphData_HumanRaceSubGraphDataSCAR.txt"
SCAR_ESP = TEST_SCAR / "SCAR-H.esp"


class Failures(list):
    def check(self, cond: bool, msg: str) -> None:
        if cond:
            print(f"  PASS  {msg}")
        else:
            print(f"  FAIL  {msg}")
            self.append(msg)


def test_subgraph_parse(f: Failures) -> None:
    print("\n[subgraph parse]")
    f.check(SCAR_SUBGRAPH.is_file(), f"SCAR subgraph exists: {SCAR_SUBGRAPH}")
    data = load_subgraph_file(SCAR_SUBGRAPH)
    f.check(len(data.entries) > 10, f"parsed {len(data.entries)} entries")
    first = data.weapon_entries(include_1st=True, include_3rd=False)
    f.check(len(first) > 0, f"1st-person weapon rows: {len(first)}")
    kws = data.target_keywords(include_1st=True)
    f.check(any("scar" in k.lower() for k in kws), f"target keywords include AnimsSCAR: {kws[:5]}")


def test_scan_roots(f: Failures) -> None:
    print("\n[scan anim roots]")
    roots = scan_anim_roots([TEST_SCAR], include_1st=True, include_3rd=False, require_file="WPNReloadReserve.hkx")
    names = {r.name.lower() for r in roots}
    f.check("scar" in names, f"found SCAR root among {sorted(names)}")
    sig = scan_anim_roots([TEST_SIG], include_1st=True, include_3rd=False, require_file="WPNReloadReserve.hkx")
    sig_names = {r.name.lower() for r in sig}
    f.check("p226" in sig_names, f"found P226 among {sorted(sig_names)}")


def test_dest_resolution(f: Failures) -> None:
    print("\n[destination resolution]")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td)
        oar = resolve_oar_base(dest, "1st")
        f.check(
            oar.as_posix().endswith("OpenAnimationReplacer"),
            f"creates full OAR path: {oar}",
        )
        oar.mkdir(parents=True)
        nested = resolve_oar_base(oar, "1st")
        f.check(nested == oar.resolve(), f"reuses existing OAR dir: {nested}")
        pack = oar / "MyPack"
        pack.mkdir()
        nested2 = resolve_oar_base(pack, "1st")
        f.check(nested2 == oar.resolve(), f"walks up from pack to OAR: {nested2}")


def test_tr_conversion(f: Failures) -> None:
    print("\n[tactical reload conversion]")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td) / "out"
        roots = scan_anim_roots([TEST_SCAR], include_1st=True, require_file="WPNReloadReserve.hkx")
        scar = [r for r in roots if r.name.lower() == "scar"]
        f.check(len(scar) == 1, "selected SCAR root")
        plan = plan_tactical_reload(
            scar,
            dest,
            pack_name="SCAR Test",
            submod_name="SCAR Tactical Reload",
            equipped={"formID": "0x2E1F", "pluginName": "SCAR-H.esp"},
        )
        f.check(len(plan.files) > 0, f"planned {len(plan.files)} WPNReload copies")
        logs = execute_plan(plan, overwrite=False)
        f.check(len(logs) > 0, "execute produced logs")
        # Spot-check a few outputs
        base_reload = plan.submod_dir / "SCAR" / "WPNReload.hkx"
        f.check(base_reload.is_file(), f"wrote {base_reload}")
        nested = plan.submod_dir / "SCAR" / "30rd" / "WPNReload.hkx"
        f.check(nested.is_file(), f"wrote nested {nested}")
        cfg_path = plan.submod_dir / "config.json"
        f.check(cfg_path.is_file(), "wrote SubMod config.json")
        cfg = json.loads(cfg_path.read_text(encoding="utf-8"))
        f.check(cfg.get("name") == "SCAR Tactical Reload", "config name")
        f.check("eventsOnEnd" not in cfg, "no eventsOnEnd in TR config")
        ammo = next(c for c in cfg["conditions"] if c.get("condition") == "CurrentMagazineAmmo")
        f.check(ammo.get("negated") is True, "ammo != 0 (negated equality)")
        f.check(any(c.get("condition") == "IsEquipped" for c in cfg["conditions"]), "IsEquipped present")
        # No leftover Reserve names
        leftovers = list(plan.submod_dir.rglob("*ReloadReserve*"))
        f.check(len(leftovers) == 0, "no WPNReloadReserve filenames in output")


def test_tr_tactical_alias(f: Failures) -> None:
    """WPNReloadTactical.hkx must be discovered and converted exactly like WPNReloadReserve.hkx.

    Uses a synthetic weapon folder (no real TestAssets ship this name) so this is an
    independent check of the alias, not a re-run of test_tr_conversion against fixtures.
    """
    print("\n[tactical reload: WPNReloadTactical.hkx alias]")
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "src"
        anims = src / "Meshes" / "Actors" / "Character" / "_1stPerson" / "Animations"
        # AliasGun only ships WPNReloadTactical.hkx (no WPNReloadReserve.hkx anywhere).
        alias_root = anims / "AliasGun"
        alias_root.mkdir(parents=True)
        (alias_root / "WPNReloadTactical.hkx").write_bytes(b"tactical-bytes")
        nested = alias_root / "30rd"
        nested.mkdir()
        (nested / "WPNReloadTactical.hkx").write_bytes(b"tactical-bytes-30rd")
        # MixedGun ships both names in the same folder; Reserve must win the naming tie-break
        # (find_files_ci sorts "Reserve" before "Tactical" alphabetically) rather than crash.
        mixed_root = anims / "MixedGun"
        mixed_root.mkdir(parents=True)
        (mixed_root / "WPNReloadReserve.hkx").write_bytes(b"reserve-bytes")
        (mixed_root / "WPNReloadTactical.hkx").write_bytes(b"tactical-bytes-mixed")

        roots = scan_anim_roots([src], include_1st=True, include_3rd=False)
        names = {r.name.lower() for r in roots}
        f.check("aliasgun" in names, f"WPNReloadTactical-only folder discovered as a root: {sorted(names)}")
        f.check("mixedgun" in names, f"mixed-name folder discovered as a root: {sorted(names)}")

        alias = [r for r in roots if r.name.lower() == "aliasgun"]
        dest = Path(td) / "out"
        plan = plan_tactical_reload(
            alias,
            dest,
            pack_name="Alias Test",
            submod_name="Alias Tactical Reload",
        )
        execute_plan(plan, overwrite=False)
        base_out = plan.submod_dir / "AliasGun" / "WPNReload.hkx"
        nested_out = plan.submod_dir / "AliasGun" / "30rd" / "WPNReload.hkx"
        f.check(base_out.is_file(), f"WPNReloadTactical copied+renamed to {base_out}")
        f.check(base_out.read_bytes() == b"tactical-bytes", "copied content matches source (not just renamed empty)")
        f.check(nested_out.is_file(), f"nested WPNReloadTactical copied+renamed to {nested_out}")
        leftovers = list(plan.submod_dir.rglob("*Tactical*"))
        f.check(len(leftovers) == 0, "no leftover WPNReloadTactical filenames in output")

        mixed = [r for r in roots if r.name.lower() == "mixedgun"]
        mixed_plan = plan_tactical_reload(
            mixed,
            dest,
            pack_name="Alias Test",
            submod_name="Mixed Tactical Reload",
        )
        f.check(len(mixed_plan.files) == 1, f"mixed folder dedupes to one WPNReload.hkx plan: {len(mixed_plan.files)}")
        execute_plan(mixed_plan, overwrite=False)
        mixed_out = mixed_plan.submod_dir / "MixedGun" / "WPNReload.hkx"
        f.check(
            mixed_out.read_bytes() == b"reserve-bytes",
            "when both names exist in one folder, Reserve wins the tie-break (alphabetical), not Tactical",
        )


def test_idle_empty(f: Failures) -> None:
    print("\n[idle empty conversion]")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td) / "out"
        roots = scan_anim_roots([TEST_SIG], include_1st=True, require_file="WPNReload.hkx")
        p226 = [r for r in roots if r.name.lower() == "p226"]
        f.check(len(p226) == 1, "selected P226 root")
        plan = plan_idle_empty(
            p226,
            dest,
            pack_name="Sig Test",
            submod_name="Sig Idle Empty",
        )
        execute_plan(plan, overwrite=False)
        idle = plan.submod_dir / "P226" / "WPNIdleReady.hkx"
        f.check(idle.is_file(), f"wrote {idle}")
        for suffix in ("A", "B", "C", "D"):
            p = plan.submod_dir / "P226" / f"WPNIdleReady{suffix}.hkx"
            f.check(p.is_file(), f"wrote {p.name}")
        equip = plan.submod_dir / "P226" / "WPNEquip.hkx"
        equip_fast = plan.submod_dir / "P226" / "WPNEquipFast.hkx"
        f.check(equip.is_file(), "copied WPNEquip where source had it")
        f.check(equip_fast.is_file(), "copied WPNEquipFast where source had it")
        sighted = plan.submod_dir / "P226" / "WPNIdleSighted.hkx"
        f.check(sighted.is_file(), "copied WPNIdleSighted where source had it")
        f.check(
            sighted.read_bytes() == (TEST_SIG / "Meshes/Actors/Character/_1stPerson/Animations/P226/WPNReload.hkx").read_bytes(),
            "WPNIdleSighted content comes from WPNReload (empty pose), not copied verbatim",
        )
        # Sibling variant not requested/covered: WPNIdleSightedWobble.hkx must not be touched.
        wobble = plan.submod_dir / "P226" / "WPNIdleSightedWobble.hkx"
        f.check(not wobble.exists(), "WPNIdleSightedWobble untouched (different animation, not requested)")
        # Nested EXT has WPNReload but no IdleReady in source → do not invent idle files
        ext_idle = plan.submod_dir / "P226" / "EXT" / "WPNIdleReady.hkx"
        f.check(not ext_idle.exists(), f"did not invent EXT idle (source has none): {ext_idle}")
        cfg = json.loads((plan.submod_dir / "config.json").read_text(encoding="utf-8"))
        tf = cfg["trackFilter"]
        f.check(tf.get("enabled") is True, "trackFilter enabled")
        f.check(tf.get("sampleFrame") == 0.0, "sampleFrame 0")
        f.check(tf.get("bones") == ["WeaponBolt"], "default WeaponBolt bone")
        f.check(abs(float(cfg.get("deactivationDelay", -1)) - 0.2) < 1e-6, "deactivationDelay 0.2")


def test_batch_multi_source(f: Failures) -> None:
    print("\n[batch multi-source]")
    with tempfile.TemporaryDirectory() as td:
        dest = Path(td) / "out"
        opts = JobOptions(
            source_dirs=[TEST_SCAR, TEST_SIG],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=dest,
            do_tactical_reload=True,
            do_idle_empty=False,
            pack_name="Batch Pack",
            tr_submod_name="Batch TR",
            overwrite=True,
        )
        roots = discover_roots(opts)
        # Pick one from each
        chosen = []
        for name in ("scar", "p226"):
            hit = next((r for r in roots if r.name.lower() == name), None)
            f.check(hit is not None, f"batch scan includes {name}")
            if hit:
                chosen.append(hit)
        opts.selected_roots = chosen
        result = run_job(opts)
        f.check(not result.errors, f"batch run errors: {result.errors}")
        f.check((dest / "Meshes").exists() or any(dest.rglob("WPNReload.hkx")), "batch wrote outputs")


def test_esp_parse(f: Failures) -> None:
    print("\n[esp parse]")
    f.check(SCAR_ESP.is_file(), "SCAR-H.esp present")
    info = parse_esp(SCAR_ESP)
    f.check(len(info.masters) >= 1, f"masters: {info.masters}")
    f.check(
        any("tacticalreload" in m.lower().replace(" ", "") for m in info.masters),
        f"SCAR-H.esp lists TacticalReload.esm: {info.masters}",
    )
    f.check(info.tr_master_index is not None, f"tr_master_index={info.tr_master_index}")
    f.check(len(info.anims_reload_reserve_fids) >= 1, f"TR keyword fids={[hex(x) for x in info.anims_reload_reserve_fids]}")
    f.check(len(info.weapons) >= 1, f"weapons: {len(info.weapons)}")
    weap_with_tr = [
        w for w in info.weapons
        if any(k in info.anims_reload_reserve_fids for k in w["keywords"])
    ]
    f.check(len(weap_with_tr) >= 1, f"WEAP with AnimsReloadReserve: {[w['edid'] for w in weap_with_tr]}")


def test_esp_patch_scar_real(f: Failures) -> None:
    """Patch the real SCAR-H.esp (has TacticalReload.esm + AnimsReloadReserve on WEAP)."""
    print("\n[esp patch SCAR-H.esp real]")
    f.check(SCAR_ESP.is_file(), "SCAR-H.esp present")
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "SCAR-H.esp"
        dst = Path(td) / "SCAR-H_OAR.esp"
        src.write_bytes(SCAR_ESP.read_bytes())
        before = parse_esp(src)
        f.check("TacticalReload.esm" in before.masters, f"before masters={before.masters}")
        f.check(0x01001734 in before.anims_reload_reserve_fids, "before has 0x01001734")

        report = patch_esp_remove_tr(
            src, dst, remove_keyword=True, remove_tr_master_if_unused=True
        )
        f.check(dst.is_file(), "wrote patched SCAR-H_OAR.esp")
        f.check(report["keywords_removed"] >= 2, f"keywords_removed={report['keywords_removed']} (expect 2 WEAP)")
        f.check(
            report["tr_master_removed"] is True,
            f"tr_master_removed={report['tr_master_removed']} reason={report.get('tr_master_kept_reason')}",
        )

        after = parse_esp(dst)
        f.check("TacticalReload.esm" not in after.masters, f"after masters={after.masters}")
        f.check(after.masters[0] == "Fallout4.esm", "Fallout4.esm still first master")
        # Self-index forms remapped 0x02xxxxxx -> 0x01xxxxxx after master drop
        scar = next((w for w in after.weapons if w["edid"] == "SCAR"), None)
        f.check(scar is not None, "SCAR WEAP still present")
        if scar:
            f.check(
                (scar["form_id"] >> 24) == 1,
                f"SCAR form high-byte remapped to self index 1: 0x{scar['form_id']:08X}",
            )
            f.check(
                all(k != 0x01001734 for k in scar["keywords"]),
                "AnimsReloadReserve stripped from SCAR KWDA",
            )
            # Old TR keyword was 0x01001734; after remap that high-byte is now self — ensure gone
            f.check(
                "AnimsSCAR" in scar["keyword_edids"] or any(
                    (k & 0xFFFFFF) == 0x003D58 for k in scar["keywords"]
                ),
                "AnimsSCAR keyword retained",
            )


def _make_minimal_tr_esp(path: Path) -> None:
    """Create a tiny synthetic ESP with Fallout4.esm + TacticalReload.esm and one WEAP KWDA."""
    # TES4 with two masters
    subs = []
    subs.append((b"HEDR", struct.pack("<fiI", 0.95, 1, 0)))
    subs.append((b"CNAM", b"OARSmoke\x00"))
    for m in ("Fallout4.esm", "TacticalReload.esm"):
        subs.append((b"MAST", (m + "\x00").encode("latin-1")))
        subs.append((b"DATA", struct.pack("<2I", 0, 0)))
    tes4_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in subs)
    tes4 = (
        b"TES4"
        + struct.pack("<I", len(tes4_body))
        + struct.pack("<I", 0)
        + struct.pack("<I", 0)
        + struct.pack("<I", 0x0000012C)  # versioning placeholder in 16:24
        + tes4_body
    )
    # Fix header: bytes 16-23 should be 8 bytes
    tes4 = (
        b"TES4"
        + struct.pack("<I", len(tes4_body))
        + struct.pack("<I", 0)
        + struct.pack("<I", 0)
        + b"\x00\x00\x00\x00\x83\x00\x00\x00"
        + tes4_body
    )

    # WEAP with EDID + KSIZ + KWDA pointing at master index 1 (TR)
    weap_subs = []
    weap_subs.append((b"EDID", b"SmokeWeapon\x00"))
    weap_subs.append((b"KSIZ", struct.pack("<I", 1)))
    tr_kw = (1 << 24) | 0x000801  # master 1, object id
    weap_subs.append((b"KWDA", struct.pack("<I", tr_kw)))
    weap_body = b"".join(st + struct.pack("<H", len(sd)) + sd for st, sd in weap_subs)
    weap_fid = (2 << 24) | 0x000800  # self index = 2 masters
    weap = (
        b"WEAP"
        + struct.pack("<I", len(weap_body))
        + struct.pack("<I", 0)
        + struct.pack("<I", weap_fid)
        + b"\x00\x00\x00\x00\x83\x00\x00\x00"
        + weap_body
    )

    # Top-level WEAP GRUP
    grup_inner = weap
    grup = (
        b"GRUP"
        + struct.pack("<I", 24 + len(grup_inner))
        + b"WEAP"
        + struct.pack("<I", 0)
        + b"\x00\x00\x00\x00\x00\x00\x00\x00"
        + grup_inner
    )
    path.write_bytes(tes4 + grup)


def test_esp_patch(f: Failures) -> None:
    print("\n[esp patch / clean masters]")
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "SmokeTR.esp"
        dst = Path(td) / "SmokeTR_OAR.esp"
        _make_minimal_tr_esp(src)
        info = parse_esp(src)
        f.check(info.tr_master_index == 1, f"TR master index={info.tr_master_index}")
        f.check(len(info.anims_reload_reserve_fids) >= 1, "detected TR keyword FormIDs")
        report = patch_esp_remove_tr(src, dst, remove_keyword=True, remove_tr_master_if_unused=True)
        f.check(dst.is_file(), "wrote patched ESP")
        f.check(report["keywords_removed"] >= 1, f"keywords_removed={report['keywords_removed']}")
        f.check(report["tr_master_removed"] is True, f"tr_master_removed={report['tr_master_removed']} reason={report.get('tr_master_kept_reason')}")
        out_info = parse_esp(dst)
        f.check("TacticalReload.esm" not in out_info.masters, f"masters after: {out_info.masters}")
        f.check(out_info.masters == ["Fallout4.esm"], f"only Fallout4.esm left: {out_info.masters}")


def test_engine_validation(f: Failures) -> None:
    print("\n[engine validation]")
    opts = JobOptions(destination=Path("."), do_tactical_reload=True)
    preview = build_preview(opts)
    f.check(len(preview.errors) > 0, f"rejects empty inputs: {preview.errors}")


def _make_test_ba2(path: Path, entries: dict[str, bytes], *, compress: set[str] = frozenset()) -> None:
    """Write a minimal, real-format-verified GNRL BA2 (version 1) for testing.

    Layout re-derived this session from PluginTemplate/BSA_Browser-master and
    confirmed against real Fallout 4 archives (see ba2_io module docstring):
    24-byte header, 36-byte-per-file table, then data, then a length-prefixed name
    table at name_table_offset.
    """
    import struct
    import zlib

    names = list(entries.keys())
    header_size = 24
    entry_size = 36
    offset = header_size + len(names) * entry_size

    blobs: list[bytes] = []
    meta: list[tuple[int, int, int]] = []  # (offset, size, real_size)
    for name in names:
        raw = entries[name]
        if name in compress:
            blob = zlib.compress(raw)
            size = len(blob)
        else:
            blob = raw
            size = 0
        meta.append((offset, size, len(raw)))
        blobs.append(blob)
        offset += len(blob)
    name_table_offset = offset

    with path.open("wb") as fh:
        fh.write(struct.pack("<4sI4sIQ", b"BTDX", 1, b"GNRL", len(names), name_table_offset))
        for i, _name in enumerate(names):
            off, size, real_size = meta[i]
            fh.write(struct.pack("<I4sIIQIII", i, b"tst\x00", 0, 0, off, size, real_size, 0))
        for blob in blobs:
            fh.write(blob)
        for name in names:
            name_bytes = name.encode("cp1252")
            fh.write(struct.pack("<H", len(name_bytes)))
            fh.write(name_bytes)


def test_ba2_extract(f: Failures) -> None:
    print("\n[ba2 archive parse + extract]")
    with tempfile.TemporaryDirectory() as td:
        ba2_path = Path(td) / "TestGun - Main.ba2"
        reserve_bytes = b"FAKE_RESERVE_HKX_DATA" * 20
        mag_bytes = b"FAKE_MAG_RESERVE_HKX" * 15
        decoy_dds = b"FAKE_DDS_DATA" * 10
        decoy_nif = b"FAKE_NIF_DATA" * 10
        _make_test_ba2(
            ba2_path,
            {
                "Meshes\\Actors\\Character\\_1stPerson\\Animations\\TestGun\\WPNReloadReserve.hkx": reserve_bytes,
                "Meshes\\Actors\\Character\\_1stPerson\\Animations\\TestGun\\30rd\\WPNReloadReserve.hkx": mag_bytes,
                "Textures\\decoy.dds": decoy_dds,
                "Meshes\\Weapons\\decoy.nif": decoy_nif,
            },
            compress={"Meshes\\Actors\\Character\\_1stPerson\\Animations\\TestGun\\WPNReloadReserve.hkx"},
        )

        info = parse_ba2(ba2_path)
        f.check(info.version == 1, f"parsed version {info.version}")
        f.check(len(info.entries) == 4, f"4 entries total: {len(info.entries)}")

        matches = entries_under(info, "meshes\\actors")
        f.check(len(matches) == 2, f"2 entries under meshes\\actors: {len(matches)}")

        out_dir = Path(td) / "extracted"
        written = extract_entries(ba2_path, matches, out_dir)
        f.check(len(written) == 2, f"extracted 2 files: {len(written)}")

        base_hkx = out_dir / "Meshes" / "Actors" / "Character" / "_1stPerson" / "Animations" / "TestGun" / "WPNReloadReserve.hkx"
        mag_hkx = base_hkx.parent / "30rd" / "WPNReloadReserve.hkx"
        f.check(base_hkx.is_file(), f"compressed entry extracted: {base_hkx}")
        f.check(base_hkx.read_bytes() == reserve_bytes, "compressed entry decompressed correctly")
        f.check(mag_hkx.is_file(), f"uncompressed entry extracted: {mag_hkx}")
        f.check(mag_hkx.read_bytes() == mag_bytes, "uncompressed entry bytes match")
        f.check(not (out_dir / "Textures").exists(), "decoy Textures entry not extracted")
        f.check(not (out_dir / "Meshes" / "Weapons").exists(), "decoy Meshes\\Weapons entry not extracted")

    # Bad magic / unsupported type are rejected with a clear error, not silently misread.
    with tempfile.TemporaryDirectory() as td:
        import struct

        bad_magic = Path(td) / "bad.ba2"
        bad_magic.write_bytes(struct.pack("<4sI4sIQ", b"XXXX", 1, b"GNRL", 0, 0))
        try:
            parse_ba2(bad_magic)
            f.check(False, "bad magic should raise Ba2Error")
        except Ba2Error:
            f.check(True, "bad magic raises Ba2Error")

        dx10 = Path(td) / "textures.ba2"
        dx10.write_bytes(struct.pack("<4sI4sIQ", b"BTDX", 1, b"DX10", 0, 0))
        try:
            parse_ba2(dx10)
            f.check(False, "DX10 archive should raise Ba2Error")
        except Ba2Error:
            f.check(True, "DX10 (texture) archive rejected with clear error")


def test_ba2_engine_integration(f: Failures) -> None:
    print("\n[ba2 as source folder: extraction -> conversion -> cleanup]")
    with tempfile.TemporaryDirectory() as td:
        ba2_path = Path(td) / "TestGun - Main.ba2"
        reserve_bytes = b"FAKE_RESERVE_HKX_DATA" * 20
        _make_test_ba2(
            ba2_path,
            {
                "Meshes\\Actors\\Character\\_1stPerson\\Animations\\TestGun\\WPNReloadReserve.hkx": reserve_bytes,
            },
            compress={"Meshes\\Actors\\Character\\_1stPerson\\Animations\\TestGun\\WPNReloadReserve.hkx"},
        )
        extract_root = Path(td) / "BA2_Extracted"

        # --- run 1: keep_ba2_extracted=False -> extraction folder deleted after success
        extract_dir = extract_ba2_source(ba2_path, extract_root)
        f.check(extract_dir.is_dir(), f"extraction folder created: {extract_dir}")
        dest = Path(td) / "out1"
        opts = JobOptions(
            source_dirs=[extract_dir],
            subgraph_paths=[SCAR_SUBGRAPH],  # only needed to satisfy input validation
            destination=dest,
            ba2_path=ba2_path,
            ba2_extract_dir=extract_dir,
            keep_ba2_extracted=False,
            do_tactical_reload=True,
            pack_name="TestGun",
            tr_submod_name="TestGun Tactical Reload",
            overwrite=True,
        )
        opts.selected_roots = [r for r in discover_roots(opts) if r.name.lower() == "testgun"]
        f.check(len(opts.selected_roots) == 1, f"BA2-extracted folder scans as a weapon root: {opts.selected_roots}")
        result = run_job(opts)
        f.check(not result.errors, f"run errors: {result.errors}")
        out_file = dest / "Meshes/Actors/Character/_1stPerson/Animations/OpenAnimationReplacer/TestGun/TestGun Tactical Reload/TestGun/WPNReload.hkx"
        f.check(out_file.is_file(), f"conversion output written: {out_file}")
        f.check(out_file.read_bytes() == reserve_bytes, "conversion output bytes match extracted BA2 content")
        f.check(not extract_dir.exists(), "extraction folder auto-deleted after successful run (keep=False)")

        # --- run 2: keep_ba2_extracted=True -> extraction folder survives
        extract_dir2 = extract_ba2_source(ba2_path, extract_root)
        dest2 = Path(td) / "out2"
        opts2 = JobOptions(
            source_dirs=[extract_dir2],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=dest2,
            ba2_path=ba2_path,
            ba2_extract_dir=extract_dir2,
            keep_ba2_extracted=True,
            do_tactical_reload=True,
            pack_name="TestGun",
            tr_submod_name="TestGun Tactical Reload",
            overwrite=True,
        )
        opts2.selected_roots = [r for r in discover_roots(opts2) if r.name.lower() == "testgun"]
        result2 = run_job(opts2)
        f.check(not result2.errors, f"run 2 errors: {result2.errors}")
        f.check(extract_dir2.exists(), "extraction folder kept after successful run (keep=True)")

        # --- run 3: force an error (no destination) -> extraction folder kept for inspection
        extract_dir3 = extract_ba2_source(ba2_path, extract_root)
        opts3 = JobOptions(
            source_dirs=[extract_dir3],
            subgraph_paths=[SCAR_SUBGRAPH],
            destination=None,  # triggers validate_inputs error
            ba2_path=ba2_path,
            ba2_extract_dir=extract_dir3,
            keep_ba2_extracted=False,
            do_tactical_reload=True,
        )
        result3 = run_job(opts3)
        f.check(bool(result3.errors), "run 3 intentionally errors (no destination)")
        f.check(extract_dir3.exists(), "extraction folder kept on error even with keep=False")


def main() -> int:
    f = Failures()
    tests = [
        test_subgraph_parse,
        test_scan_roots,
        test_dest_resolution,
        test_tr_conversion,
        test_tr_tactical_alias,
        test_idle_empty,
        test_batch_multi_source,
        test_esp_parse,
        test_esp_patch,
        test_esp_patch_scar_real,
        test_engine_validation,
        test_ba2_extract,
        test_ba2_engine_integration,
    ]
    for t in tests:
        try:
            t(f)
        except Exception:  # noqa: BLE001
            msg = f"EXCEPTION in {t.__name__}: {traceback.format_exc()}"
            print(f"  FAIL  {msg}")
            f.append(msg)

    print("\n==========")
    if f:
        print(f"{len(f)} failure(s)")
        for item in f:
            print(f" - {item}")
        return 1
    print("All smoke tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
