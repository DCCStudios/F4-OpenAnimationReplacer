"""Fallout 4 BA2 (GNRL) archive reader/extractor.

Credit: the BA2/BSA format layout used below was re-derived by reading (not linking
against) BSA Browser by AlexxEG (Alexander Ellingsen), https://github.com/AlexxEG/BSA_Browser,
vendored for reference under PluginTemplate/BSA_Browser-master/Sharp.BSA.BA2. That project
is C#/.NET and is not used as a dependency here; this module is an independent Python
re-implementation of the same on-disk format.

Format re-derived from PluginTemplate/BSA_Browser-master/Sharp.BSA.BA2 (BA2Header.cs,
BA2FileEntry.cs, Archive.cs, CompressionUtils.cs) this session, not from memory. Only the
BTDX/GNRL ("general files") archive layout is implemented: that is what Fallout 4 packs
loose meshes/animations into ("<Mod> - Main.ba2"). BTDX/DX10 ("<Mod> - Textures.ba2") is a
different, chunked texture layout that this tool has no use for (we only need
meshes/actors/*.hkx and *.nif for the OAR conversion) and is deliberately rejected with a
clear error instead of silently mis-reading it.

On-disk layout (all little-endian):

Header (24 bytes for Fallout 4's own versions 1/7/8):
    magic         4s   "BTDX"
    version       u32  1 (original), 7/8 (2024 "next-gen" update); 2/3 are Starfield-only
    type          4s   "GNRL" | "DX10" | "GNMF"
    num_files     u32
    name_table_off u64
    (version==2 adds 2x u32; version==3 adds 3x u32 - Starfield only, never seen from
    Fallout 4, but the fields are skipped correctly if encountered so header parsing does
    not desync)

Verified this session against real Fallout 4 "next-gen" (v8) archives: the 24-byte header
and 36-byte GNRL entry layout below are byte-identical to v1; only the version number
changed. (Confirmed by manual header/entry dump: entry offsets landed exactly at
header_size + num_files*36, and consecutive offsets matched the previous entry's
real_size exactly for uncompressed .pex entries.)

GNRL file entry (36 bytes each, num_files of them immediately after the header):
    name_hash  u32
    extension  4s
    dir_hash   u32
    flags      u32
    offset     u64  absolute file offset of this entry's data
    size       u32  compressed size; 0 means the data is stored raw at real_size
    real_size  u32  uncompressed size
    align      u32

Name table (only if name_table_off > 0), one entry per file in the same order as the file
entries, at name_table_off:
    length  u16
    bytes   length bytes of the path (single-byte codepage; Fallout 4 paths are ASCII)

Compressed entry data is a standard zlib stream (RFC1950 header), decompressible with
Python's stdlib zlib.decompress directly - Fallout 4 never uses the LZ4 path (that is a
version 3 / Starfield-era feature we do not implement).
"""

from __future__ import annotations

import os
import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path

HEADER_BASE = struct.Struct("<4sI4sIQ")  # magic, version, type, num_files, name_table_off
FILE_ENTRY = struct.Struct("<I4sIIQIII")  # name_hash, ext, dir_hash, flags, offset, size, real_size, align

SUPPORTED_TYPES = {"GNRL"}


class Ba2Error(Exception):
    """Raised for BA2 files this reader cannot or will not parse."""


@dataclass
class Ba2Entry:
    """One file entry from a GNRL BA2's file table."""

    full_path: str  # as stored in the name table, backslash-separated (e.g. "meshes\\actors\\...")
    offset: int
    size: int  # compressed size; 0 = stored raw
    real_size: int  # uncompressed size

    @property
    def compressed(self) -> bool:
        return self.size != 0

    def normalized(self) -> str:
        return self.full_path.replace("/", "\\").strip("\\")


@dataclass
class Ba2Info:
    path: Path
    version: int
    archive_type: str
    entries: list[Ba2Entry] = field(default_factory=list)


def parse_ba2(path: Path) -> Ba2Info:
    """Parse a BA2's header and file table (no data reads). GNRL only."""
    with path.open("rb") as fh:
        base = fh.read(HEADER_BASE.size)
        if len(base) < HEADER_BASE.size:
            raise Ba2Error(f"{path.name}: file too small to be a BA2")
        magic, version, archive_type_b, num_files, name_table_off = HEADER_BASE.unpack(base)
        if magic != b"BTDX":
            raise Ba2Error(f"{path.name}: not a BA2 (magic={magic!r}, expected b'BTDX')")

        # Version 2/3 (Starfield only) headers insert extra u32 fields before the file
        # table; skip them so the file table read below stays correctly aligned even if
        # encountered. Fallout 4's own versions (1, 7, 8) all share the plain 24-byte
        # header with no extra fields - verified against real v8 "next-gen" archives.
        if version == 2:
            fh.read(8)
        elif version == 3:
            fh.read(12)
        elif version not in (1, 7, 8):
            raise Ba2Error(f"{path.name}: unsupported BA2 version {version}")

        archive_type = archive_type_b.decode("ascii", errors="replace").strip()
        if archive_type not in SUPPORTED_TYPES:
            raise Ba2Error(
                f"{path.name}: archive type '{archive_type}' is not supported for extraction "
                "(only GNRL 'general files' archives, i.e. the '- Main.ba2' file, are readable "
                "here; texture/DX10 archives use a different chunked layout and are not needed "
                "for meshes/actors animation files)."
            )

        entries: list[Ba2Entry] = []
        for _ in range(num_files):
            raw = fh.read(FILE_ENTRY.size)
            if len(raw) < FILE_ENTRY.size:
                raise Ba2Error(f"{path.name}: file table truncated")
            _name_hash, _ext, _dir_hash, _flags, offset, size, real_size, _align = FILE_ENTRY.unpack(raw)
            entries.append(Ba2Entry(full_path="", offset=offset, size=size, real_size=real_size))

        if name_table_off:
            fh.seek(name_table_off)
            for entry in entries:
                len_raw = fh.read(2)
                if len(len_raw) < 2:
                    raise Ba2Error(f"{path.name}: name table truncated")
                (name_len,) = struct.unpack("<H", len_raw)
                name_bytes = fh.read(name_len)
                if len(name_bytes) < name_len:
                    raise Ba2Error(f"{path.name}: name table truncated")
                entry.full_path = name_bytes.decode("cp1252", errors="replace")

    return Ba2Info(path=path, version=version, archive_type=archive_type, entries=entries)


def entries_under(info: Ba2Info, prefix: str) -> list[Ba2Entry]:
    """Entries whose normalized path starts with prefix (case-insensitive), e.g. 'meshes\\actors'."""
    target = prefix.replace("/", "\\").strip("\\").lower()
    out = []
    for e in info.entries:
        norm = e.normalized().lower()
        if norm.startswith(target):
            out.append(e)
    return out


def extract_entries(
    ba2_path: Path,
    entries: list[Ba2Entry],
    dest_dir: Path,
    *,
    log=None,
) -> list[Path]:
    """Extract entries into dest_dir, preserving the archive's own relative path layout.

    Writing to ``dest_dir / entry.normalized()`` recreates the real
    ``Meshes\\Actors\\...`` tree, so the extraction folder can be used directly as a
    conversion-tool source folder without any path rewriting.
    """
    written: list[Path] = []
    with ba2_path.open("rb") as fh:
        for entry in entries:
            rel = entry.normalized().replace("\\", os.sep)
            dest = dest_dir / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            fh.seek(entry.offset)
            if entry.compressed:
                raw = fh.read(entry.size)
                if len(raw) < entry.size:
                    raise Ba2Error(f"{ba2_path.name}: truncated compressed data for {entry.full_path}")
                try:
                    data = zlib.decompress(raw)
                except zlib.error as exc:
                    raise Ba2Error(f"{ba2_path.name}: failed to decompress {entry.full_path}: {exc}") from exc
            else:
                data = fh.read(entry.real_size)
                if len(data) < entry.real_size:
                    raise Ba2Error(f"{ba2_path.name}: truncated data for {entry.full_path}")
            dest.write_bytes(data)
            written.append(dest)
            if log:
                log(f"  Extracted {entry.normalized()}")
    return written


def extract_meshes_actors(
    ba2_path: Path,
    dest_dir: Path,
    *,
    log=None,
) -> list[Path]:
    """Parse ba2_path and extract every entry under meshes\\actors into dest_dir."""
    info = parse_ba2(ba2_path)
    matches = entries_under(info, "meshes\\actors")
    if not matches:
        raise Ba2Error(f"{ba2_path.name}: no files found under meshes\\actors")
    if log:
        log(f"BA2 {ba2_path.name}: {len(info.entries)} total entries, {len(matches)} under meshes\\actors")
    return extract_entries(ba2_path, matches, dest_dir, log=log)
