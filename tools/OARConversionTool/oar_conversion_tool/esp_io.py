"""Fallout 4 ESP/ESM/ESL reader/writer for WEAP keywords and CleanMasters-style TR removal.

Logic mirrors xEdit CleanMasters: FindUsedMasters via FormID high-byte, then remap
indices when an unused master is dropped (FO4 non-complex FileID path).
See PluginTemplate/TES5Edit-dev-4.1.6 Core/wbImplementation.pas TwbFile.CleanMasters.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path

RECORD_HDR = 24
GROUP_HDR = 24
SUB_HDR = 6

TR_MASTER_NAMES = {
    "tacticalreload.esm",
    "tactical reload.esm",
    "tacticalreload.esl",
}

TR_KEYWORD_EDIDS = {
    "animsreloadreserve",
}


@dataclass
class PluginRecord:
    rtype: bytes
    flags: int
    form_id: int
    header16_24: bytes  # bytes[16:24] of original header
    subrecords: list[tuple[bytes, bytes]]
    was_compressed: bool = False


@dataclass
class EspInfo:
    path: Path
    masters: list[str] = field(default_factory=list)
    is_light: bool = False
    weapons: list[dict] = field(default_factory=list)
    keywords: dict[int, str] = field(default_factory=dict)
    tr_master_index: int | None = None
    anims_reload_reserve_fids: set[int] = field(default_factory=set)


def _decode_zstring(data: bytes) -> str:
    return data.split(b"\x00", 1)[0].decode("latin-1", errors="replace")


def _read_subrecords(raw: bytes) -> list[tuple[bytes, bytes]]:
    out: list[tuple[bytes, bytes]] = []
    i = 0
    n = len(raw)
    while i + SUB_HDR <= n:
        st = raw[i : i + 4]
        ss = struct.unpack_from("<H", raw, i + 4)[0]
        i += SUB_HDR
        sd = raw[i : i + ss]
        i += ss
        out.append((st, sd))
    return out


def _write_subrecords(subs: list[tuple[bytes, bytes]]) -> bytes:
    parts: list[bytes] = []
    for st, sd in subs:
        parts.append(st + struct.pack("<H", len(sd)) + sd)
    return b"".join(parts)


def _decompress_record(flags: int, raw: bytes) -> bytes:
    if flags & 0x00040000:
        return zlib.decompress(raw[4:])
    return raw


def _pack_record(rec: PluginRecord) -> bytes:
    data = _write_subrecords(rec.subrecords)
    flags = rec.flags
    if rec.was_compressed:
        payload = struct.pack("<I", len(data)) + zlib.compress(data)
        flags |= 0x00040000
    else:
        payload = data
        flags &= ~0x00040000
    return (
        rec.rtype
        + struct.pack("<I", len(payload))
        + struct.pack("<I", flags)
        + struct.pack("<I", rec.form_id)
        + rec.header16_24
        + payload
    )


def master_index_of_formid(formid: int) -> int:
    return (formid >> 24) & 0xFF


def _parse_record_at(data: bytes, pos: int) -> tuple[PluginRecord, int]:
    rtype = data[pos : pos + 4]
    rsize = struct.unpack_from("<I", data, pos + 4)[0]
    rflags = struct.unpack_from("<I", data, pos + 8)[0]
    fid = struct.unpack_from("<I", data, pos + 12)[0]
    header16_24 = data[pos + 16 : pos + 24]
    raw = data[pos + RECORD_HDR : pos + RECORD_HDR + rsize]
    was_comp = bool(rflags & 0x00040000)
    body = _decompress_record(rflags, raw)
    rec = PluginRecord(rtype, rflags, fid, header16_24, _read_subrecords(body), was_comp)
    return rec, pos + RECORD_HDR + rsize


def iter_records(path: Path):
    data = path.read_bytes()
    if len(data) < RECORD_HDR:
        return
    rec, pos = _parse_record_at(data, 0)
    yield None, rec

    def walk(start: int, end: int, label: bytes):
        p = start
        while p + 4 <= end:
            if data[p : p + 4] == b"GRUP":
                gsize = struct.unpack_from("<I", data, p + 4)[0]
                glabel = data[p + 8 : p + 12]
                yield from walk(p + GROUP_HDR, p + gsize, glabel)
                p += gsize
            else:
                rec, nxt = _parse_record_at(data, p)
                yield label, rec
                p = nxt

    file_size = len(data)
    while pos + GROUP_HDR <= file_size and data[pos : pos + 4] == b"GRUP":
        gsize = struct.unpack_from("<I", data, pos + 4)[0]
        glabel = data[pos + 8 : pos + 12]
        yield from walk(pos + GROUP_HDR, pos + gsize, glabel)
        pos += gsize


def parse_esp(path: Path) -> EspInfo:
    info = EspInfo(path=path)
    data = path.read_bytes()
    if len(data) >= RECORD_HDR:
        info.is_light = bool(struct.unpack_from("<I", data, 8)[0] & 0x00000200)

    for _label, rec in iter_records(path):
        if rec.rtype == b"TES4":
            for st, sd in rec.subrecords:
                if st == b"MAST":
                    info.masters.append(_decode_zstring(sd))
            continue
        if rec.rtype == b"KYWD":
            edid = ""
            for st, sd in rec.subrecords:
                if st == b"EDID":
                    edid = _decode_zstring(sd)
            info.keywords[rec.form_id] = edid
            if edid.lower() in TR_KEYWORD_EDIDS:
                info.anims_reload_reserve_fids.add(rec.form_id)
            continue
        if rec.rtype == b"WEAP":
            edid = ""
            kwda: list[int] = []
            for st, sd in rec.subrecords:
                if st == b"EDID":
                    edid = _decode_zstring(sd)
                elif st == b"KWDA":
                    for i in range(0, len(sd) - 3, 4):
                        kwda.append(struct.unpack_from("<I", sd, i)[0])
            info.weapons.append(
                {
                    "edid": edid,
                    "form_id": rec.form_id,
                    "form_id_hex": f"0x{rec.form_id & 0xFFFFFF:X}",
                    "keywords": kwda,
                    "keyword_edids": [],
                }
            )

    for i, name in enumerate(info.masters):
        compact = name.lower().replace(" ", "")
        if name.lower() in TR_MASTER_NAMES or "tacticalreload" in compact:
            info.tr_master_index = i
            break

    if info.tr_master_index is not None:
        for weap in info.weapons:
            for fid in weap["keywords"]:
                if master_index_of_formid(fid) == info.tr_master_index:
                    info.anims_reload_reserve_fids.add(fid)

    for weap in info.weapons:
        weap["keyword_edids"] = [info.keywords.get(k, f"0x{k:08X}") for k in weap["keywords"]]
    return info


# Subrecords that are FormID / FormID-array fields for CleanMasters *usage* detection.
# Do NOT scan opaque binary blobs (MHDT, WLEV, OBTS, XCRI, DATA, DNAM, …) for usage:
# their bytes produce false master hits and would incorrectly block CleanMasters.
# Remapping after a master drop is separate: see _remap_subs (must cover OBTS/APPR).
_FORMID_SUBS = {
    b"KWDA",
    b"NAME",
    b"XLIB",
    b"XLKR",
    b"CNAM",
    b"BNAM",
    b"INAM",
    b"TNAM",
    b"UNAM",
    b"YNAM",
    b"ZNAM",
    b"KNAM",
    b"WNAM",
    b"ETYP",
    b"NNAM",
    b"PNAM",
    b"QNAM",
    b"RNAM",
    b"ANAM",
    b"LNAM",
    b"MNAM",
    b"ONAM",
    b"GNAM",
    b"HNAM",
    b"JNAM",
    b"ENAM",
    b"ATXT",
    b"ATTX",
    b"XPRI",
    b"XPWR",
    b"XOWN",
    b"XRNK",
    b"XEZN",
    b"XLRL",
    b"WAMD",
    b"WZMD",
}

# FormID arrays (length multiple of 4)
_FORMID_ARRAY_SUBS = {
    b"KWDA",
    b"APPR",
    b"XLKR",
    b"XPRI",
    b"XPWR",
}


def _iter_formids_in_subrecord(st: bytes, sd: bytes):
    """Yield FormIDs from a subrecord when the field type is FormID-bearing."""
    if st in _FORMID_ARRAY_SUBS and len(sd) >= 4 and len(sd) % 4 == 0:
        for i in range(0, len(sd), 4):
            yield struct.unpack_from("<I", sd, i)[0]
        return
    if st in _FORMID_SUBS and len(sd) == 4:
        yield struct.unpack_from("<I", sd, 0)[0]


def find_used_master_indices(path: Path, master_count: int) -> set[int]:
    """Mark masters referenced by record FormIDs or known FormID subrecords.

    Mirrors xEdit FindUsedMasters intent: only real FormID fields count. Scanning
    every uint32 in every subrecord false-positives on MHDT/WLEV/OBTS and would
    keep TacticalReload.esm forever on large weapon plugins like SCAR-H.esp.
    """
    used: set[int] = set()
    if master_count > 0:
        used.add(0)  # keep game master when present
    for _label, rec in iter_records(path):
        mi = master_index_of_formid(rec.form_id)
        if mi < master_count:
            used.add(mi)
        for st, sd in rec.subrecords:
            for fid in _iter_formids_in_subrecord(st, sd):
                if fid in (0, 0xFFFFFFFF):
                    continue
                if (fid & 0x00FFFFFF) < 0x800:
                    continue
                mi = master_index_of_formid(fid)
                if mi < master_count:
                    used.add(mi)
    return used


def _remap_formid(fid: int, old_to_new: dict[int, int], old_c: int, new_c: int) -> int:
    if fid in (0, 0xFFFFFFFF):
        return fid
    obj = fid & 0x00FFFFFF
    if obj < 0x800:
        return fid
    mi = master_index_of_formid(fid)
    if mi in old_to_new:
        return (old_to_new[mi] << 24) | obj
    if mi == old_c:
        return (new_c << 24) | obj
    return fid


def _needs_remap(fid: int, old_to_new: dict[int, int], old_c: int) -> bool:
    """True when this value's high byte is a master/self index that will move."""
    if fid in (0, 0xFFFFFFFF):
        return False
    if (fid & 0x00FFFFFF) < 0x800:
        return False
    mi = master_index_of_formid(fid)
    return mi in old_to_new or mi == old_c


def _remap_blob(sd: bytes, old_to_new: dict[int, int], old_c: int, new_c: int) -> bytes:
    """Remap FormID high-bytes inside opaque subrecord payloads (OBTS, APPR, …).

    After dropping an unused master, the file's self index shrinks (e.g. 0x02 -> 0x01).
    Allowlisted FormID fields alone are not enough: FO4 WEAP object templates (OBTS),
    attach parent slots (APPR), and similar still hold self FormIDs. Leaving those at
    the old high byte makes xEdit/game show empty broken weapon data.

    Only rewrite aligned uint32s whose high byte is a remappable master/self index,
    so float/binary noise with other high bytes is left alone.
    """
    if len(sd) < 4:
        return sd
    out = bytearray(sd)
    changed = False
    for i in range(0, len(sd) - 3, 4):
        fid = struct.unpack_from("<I", out, i)[0]
        if not _needs_remap(fid, old_to_new, old_c):
            continue
        new_fid = _remap_formid(fid, old_to_new, old_c, new_c)
        if new_fid != fid:
            struct.pack_into("<I", out, i, new_fid)
            changed = True
    return bytes(out) if changed else sd


def _remap_subs(subs: list[tuple[bytes, bytes]], old_to_new: dict[int, int], old_c: int, new_c: int):
    """Remap FormIDs in every subrecord after a master list change."""
    out: list[tuple[bytes, bytes]] = []
    for st, sd in subs:
        out.append((st, _remap_blob(sd, old_to_new, old_c, new_c)))
    return out


def _strip_kwda(subs: list[tuple[bytes, bytes]], remove_fids: set[int]) -> tuple[list[tuple[bytes, bytes]], int]:
    """Remove FormIDs from KWDA/KSIZ in place; preserve all other subrecord order."""
    if not remove_fids:
        return subs, 0

    removed = 0
    kept_kwda: list[int] | None = None
    for st, sd in subs:
        if st != b"KWDA":
            continue
        kept: list[int] = []
        for i in range(0, len(sd) - 3, 4):
            fid = struct.unpack_from("<I", sd, i)[0]
            if fid in remove_fids:
                removed += 1
            else:
                kept.append(fid)
        kept_kwda = kept

    if removed == 0 or kept_kwda is None:
        return subs, 0

    out: list[tuple[bytes, bytes]] = []
    for st, sd in subs:
        if st == b"KSIZ":
            out.append((b"KSIZ", struct.pack("<I", len(kept_kwda))))
        elif st == b"KWDA":
            if kept_kwda:
                out.append((b"KWDA", b"".join(struct.pack("<I", x) for x in kept_kwda)))
            # Drop empty KWDA entirely (KSIZ already says 0).
        else:
            out.append((st, sd))
    return out, removed


def _set_masters(tes4_subs: list[tuple[bytes, bytes]], masters: list[str]) -> list[tuple[bytes, bytes]]:
    rebuilt: list[tuple[bytes, bytes]] = []
    i = 0
    while i < len(tes4_subs):
        st, sd = tes4_subs[i]
        if st == b"MAST":
            if i + 1 < len(tes4_subs) and tes4_subs[i + 1][0] == b"DATA":
                i += 2
            else:
                i += 1
            continue
        rebuilt.append((st, sd))
        i += 1
    insert_at = 0
    for idx, (st, _sd) in enumerate(rebuilt):
        if st in {b"HEDR", b"CNAM", b"SNAM"}:
            insert_at = idx + 1
    mast_blob: list[tuple[bytes, bytes]] = []
    for m in masters:
        mast_blob.append((b"MAST", (m + "\x00").encode("latin-1")))
        mast_blob.append((b"DATA", struct.pack("<2I", 0, 0)))
    return rebuilt[:insert_at] + mast_blob + rebuilt[insert_at:]


def _rebuild_file(data: bytes, transform) -> bytes:
    """Walk TES4 + GRUPs, transforming each PluginRecord via transform(rec)->rec."""
    tes4, pos = _parse_record_at(data, 0)
    tes4 = transform(tes4, is_tes4=True)
    out = bytearray(_pack_record(tes4))
    file_size = len(data)

    def walk(start: int, end: int) -> bytes:
        parts = bytearray()
        p = start
        while p + 4 <= end:
            if data[p : p + 4] == b"GRUP":
                gsize = struct.unpack_from("<I", data, p + 4)[0]
                gheader = bytearray(data[p : p + GROUP_HDR])
                inner = walk(p + GROUP_HDR, p + gsize)
                struct.pack_into("<I", gheader, 4, GROUP_HDR + len(inner))
                parts += gheader + inner
                p += gsize
            else:
                rec, nxt = _parse_record_at(data, p)
                rec = transform(rec, is_tes4=False)
                parts += _pack_record(rec)
                p = nxt
        return bytes(parts)

    while pos + GROUP_HDR <= file_size and data[pos : pos + 4] == b"GRUP":
        gsize = struct.unpack_from("<I", data, pos + 4)[0]
        gheader = bytearray(data[pos : pos + GROUP_HDR])
        inner = walk(pos + GROUP_HDR, pos + gsize)
        struct.pack_into("<I", gheader, 4, GROUP_HDR + len(inner))
        out += gheader + inner
        pos += gsize
    return bytes(out)


def patch_esp_remove_tr(
    src: Path,
    dst: Path,
    *,
    remove_keyword: bool = True,
    remove_tr_master_if_unused: bool = True,
) -> dict:
    """Write patched ESP: strip AnimsReloadReserve from WEAP; drop TR master if unused."""
    info = parse_esp(src)
    report = {
        "source": str(src),
        "dest": str(dst),
        "weapons_patched": 0,
        "keywords_removed": 0,
        "tr_master_removed": False,
        "tr_master_kept_reason": None,
        "masters_before": list(info.masters),
        "masters_after": list(info.masters),
    }
    remove_fids = set(info.anims_reload_reserve_fids)
    data = src.read_bytes()
    old_c = len(info.masters)

    def transform_strip(rec: PluginRecord, is_tes4: bool = False) -> PluginRecord:
        if is_tes4:
            return rec
        if remove_keyword and rec.rtype == b"WEAP" and remove_fids:
            new_subs, removed = _strip_kwda(rec.subrecords, remove_fids)
            if removed:
                report["weapons_patched"] += 1
                report["keywords_removed"] += removed
                rec.subrecords = new_subs
        return rec

    stripped = _rebuild_file(data, transform_strip)

    new_masters = list(info.masters)
    old_to_new: dict[int, int] | None = None
    new_c = old_c

    if remove_tr_master_if_unused and info.tr_master_index is not None:
        tmp = dst.with_suffix(dst.suffix + ".trtmp")
        try:
            tmp.write_bytes(stripped)
            used = find_used_master_indices(tmp, old_c)
            if info.tr_master_index in used:
                report["tr_master_kept_reason"] = (
                    f"Master index {info.tr_master_index} ({info.masters[info.tr_master_index]}) "
                    "still referenced by FormIDs"
                )
            else:
                old_to_new = {}
                new_masters = []
                for i, name in enumerate(info.masters):
                    if i == info.tr_master_index:
                        continue
                    old_to_new[i] = len(new_masters)
                    new_masters.append(name)
                new_c = len(new_masters)
                report["tr_master_removed"] = True
                report["masters_after"] = list(new_masters)
        finally:
            if tmp.exists():
                tmp.unlink(missing_ok=True)

    if report["tr_master_removed"] and old_to_new is not None:
        def transform_remap(rec: PluginRecord, is_tes4: bool = False) -> PluginRecord:
            if is_tes4:
                rec.subrecords = _set_masters(rec.subrecords, new_masters)
                return rec
            rec.form_id = _remap_formid(rec.form_id, old_to_new, old_c, new_c)
            rec.subrecords = _remap_subs(rec.subrecords, old_to_new, old_c, new_c)
            return rec

        final = _rebuild_file(stripped, transform_remap)
    else:
        final = stripped

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(final)
    return report
