"""Idle Empty OAR conversion from WPNReload.hkx (+ optional equip replacements)."""

from __future__ import annotations

from pathlib import Path

from . import config_json
from .convert_tr import ConversionPlan, FilePlan
from .paths import AnimRoot, find_files_ci, relative_under_root, resolve_oar_base

# Idle / equip names we may override in OAR. Only emitted when that exact file
# already exists in the matching source folder (so nested mag folders that only
# ship WPNReload do not invent a full IdleReady A-D set).
#
# WPNIdleSighted.hkx is the aim-down-sights idle pose (confirmed as a real, commonly
# shipped file alongside WPNIdleReady* in TR weapon packs, e.g. "A Bundle of Tape"'s
# Main.ba2 ships it for CrudeBlowback/M1Garand/SVT40/VarmintRifle). Deliberately not
# covering WPNIdleSightedWobble.hkx / WPNIdleSighted2.hkx here - those are separate,
# distinct animations (aim sway / alternate sighted pose) rather than an "empty" idle
# variant, and were not requested.
IDLE_NAMES = (
    "WPNIdleReady.hkx",
    "WPNIdleReadyA.hkx",
    "WPNIdleReadyB.hkx",
    "WPNIdleReadyC.hkx",
    "WPNIdleReadyD.hkx",
    "WPNIdleSighted.hkx",
)

EQUIP_NAMES = (
    "WPNEquip.hkx",
    "WPNEquipFast.hkx",
)


def _nearest_reload(folder: Path, anim_root: Path) -> Path | None:
    """Find WPNReload.hkx in folder or an ancestor up to anim_root."""
    folder = folder.resolve()
    anim_root = anim_root.resolve()
    cur = folder
    while True:
        hits = [p for p in cur.iterdir() if p.is_file() and p.name.lower() == "wpnreload.hkx"]
        if hits:
            return hits[0]
        if cur == anim_root or anim_root not in cur.parents and cur != anim_root:
            if cur == anim_root:
                break
            if anim_root not in cur.parents:
                break
        parent = cur.parent
        if parent == cur:
            break
        cur = parent
        if cur == anim_root.parent and folder != anim_root:
            continue
    hits = [p for p in anim_root.iterdir() if p.is_file() and p.name.lower() == "wpnreload.hkx"]
    return hits[0] if hits else None


def _file_exists_ci(folder: Path, name: str) -> Path | None:
    """Return the path if folder contains name (case-insensitive)."""
    target = name.lower()
    try:
        for p in folder.iterdir():
            if p.is_file() and p.name.lower() == target:
                return p
    except OSError:
        return None
    return None


def plan_idle_empty(
    anim_roots: list[AnimRoot],
    destination: Path,
    *,
    pack_name: str,
    submod_name: str,
    description: str = "",
    priority: int = 3000,
    bones: list[str] | None = None,
    equipped: dict[str, str] | list[dict[str, str]] | None = None,
    author: str = "",
    deactivation_delay: float = 0.2,
) -> ConversionPlan:
    """Plan Idle Empty outputs for selected anim roots.

    For each source folder, only place IdleReady / Equip replacements when those
    filenames already exist there. Content comes from that folder's (or nearest)
    WPNReload.hkx so OAR can override the empty pose without inventing missing files.
    """
    has_1st = any(r.perspective == "1st" for r in anim_roots)
    plans_files: list[FilePlan] = []
    pack_dir: Path | None = None
    submod: Path | None = None

    perspectives = []
    if has_1st:
        perspectives.append("1st")
    if any(r.perspective == "3rd" for r in anim_roots):
        perspectives.append("3rd")

    for persp in perspectives:
        oar = resolve_oar_base(destination, persp)
        pack = oar / pack_name
        sub = pack / submod_name
        if persp == "1st" or submod is None:
            pack_dir = pack
            submod = sub

        for root in anim_roots:
            if root.perspective != persp:
                continue

            # Visit every folder under the anim root that has at least one
            # idle/equip candidate, or has WPNReload (reload used as content source).
            folders: set[Path] = set()
            for name in IDLE_NAMES + EQUIP_NAMES:
                for hit in find_files_ci(root.absolute_path, name):
                    folders.add(hit.parent.resolve())
            for reload_hit in find_files_ci(root.absolute_path, "WPNReload.hkx"):
                folders.add(reload_hit.parent.resolve())

            for folder in sorted(folders, key=lambda p: str(p).lower()):
                src_reload = _nearest_reload(folder, root.absolute_path)
                if src_reload is None:
                    continue
                rel = relative_under_root(folder, root.absolute_path.parent)
                for name in IDLE_NAMES + EQUIP_NAMES:
                    if _file_exists_ci(folder, name) is None:
                        continue
                    dest = sub / rel / name
                    plans_files.append(FilePlan(src_reload, dest, "copy"))

    assert submod is not None and pack_dir is not None

    cfg = config_json.idle_empty_config(
        name=submod_name,
        description=description or f"Empty idle for {pack_name}.",
        priority=priority,
        bones=bones,
        equipped=equipped,
        deactivation_delay=deactivation_delay,
    )
    pack_cfg = config_json.pack_config(pack_name, author=author, description=description)

    seen: set[str] = set()
    unique: list[FilePlan] = []
    for fp in plans_files:
        key = str(fp.dest).lower()
        if key in seen:
            continue
        seen.add(key)
        unique.append(fp)

    return ConversionPlan(
        submod_dir=submod,
        pack_dir=pack_dir,
        files=unique,
        config_path=submod / "config.json",
        config_payload=cfg,
        pack_config_path=pack_dir / "config.json",
        pack_config_payload=pack_cfg,
        label=f"Idle Empty -> {submod_name}",
    )


def execute_idle_plan(plan: ConversionPlan, *, overwrite: bool = False) -> list[str]:
    from .convert_tr import execute_plan

    return execute_plan(plan, overwrite=overwrite)
