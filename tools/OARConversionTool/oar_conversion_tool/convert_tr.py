"""Tactical Reload → OAR conversion (WPNReloadReserve.hkx → WPNReload.hkx)."""

from __future__ import annotations

import shutil
from dataclasses import dataclass, field
from pathlib import Path

from . import config_json
from .paths import (
    AnimRoot,
    TACTICAL_RESERVE_NAMES,
    find_files_ci,
    relative_under_root,
    resolve_oar_base,
)


@dataclass
class FilePlan:
    """One planned copy/rename operation."""

    source: Path
    dest: Path
    action: str  # "copy"


@dataclass
class ConversionPlan:
    """Planned outputs for one SubMod."""

    submod_dir: Path
    pack_dir: Path
    files: list[FilePlan] = field(default_factory=list)
    config_path: Path | None = None
    config_payload: dict | None = None
    pack_config_path: Path | None = None
    pack_config_payload: dict | None = None
    label: str = ""


def plan_tactical_reload(
    anim_roots: list[AnimRoot],
    destination: Path,
    *,
    pack_name: str,
    submod_name: str,
    description: str = "",
    priority: int = 3000,
    equipped: dict[str, str] | list[dict[str, str]] | None = None,
    author: str = "",
    overwrite: bool = False,
) -> ConversionPlan:
    """Build a plan: copy every WPNReloadReserve/WPNReloadTactical under selected roots as WPNReload.hkx."""
    # Destination base uses 1st-person tree when any root is 1st; create both if mixed.
    has_1st = any(r.perspective == "1st" for r in anim_roots)
    has_3rd = any(r.perspective == "3rd" for r in anim_roots)

    plans_files: list[FilePlan] = []
    pack_dir_1st: Path | None = None
    submod_1st: Path | None = None

    if has_1st:
        oar_1st = resolve_oar_base(destination, "1st")
        pack_dir_1st = oar_1st / pack_name
        submod_1st = pack_dir_1st / submod_name
        for root in anim_roots:
            if root.perspective != "1st":
                continue
            for src in find_files_ci(root.absolute_path, TACTICAL_RESERVE_NAMES):
                rel = relative_under_root(src.parent, root.absolute_path.parent)
                # rel is like SCAR\30rd — keep under SubMod
                dest = submod_1st / rel / "WPNReload.hkx"
                plans_files.append(FilePlan(src, dest, "copy"))

    if has_3rd:
        oar_3rd = resolve_oar_base(destination, "3rd")
        pack_dir_3rd = oar_3rd / pack_name
        submod_3rd = pack_dir_3rd / submod_name
        for root in anim_roots:
            if root.perspective != "3rd":
                continue
            for src in find_files_ci(root.absolute_path, TACTICAL_RESERVE_NAMES):
                rel = relative_under_root(src.parent, root.absolute_path.parent)
                dest = submod_3rd / rel / "WPNReload.hkx"
                plans_files.append(FilePlan(src, dest, "copy"))

    # Primary SubMod for config is 1st if present else 3rd
    primary_submod = submod_1st
    primary_pack = pack_dir_1st
    if primary_submod is None and has_3rd:
        oar_3rd = resolve_oar_base(destination, "3rd")
        primary_pack = oar_3rd / pack_name
        primary_submod = primary_pack / submod_name

    assert primary_submod is not None and primary_pack is not None

    cfg = config_json.tactical_reload_config(
        name=submod_name,
        description=description or f"Tactical reload (OAR) for {pack_name}.",
        priority=priority,
        equipped=equipped,
    )
    pack_cfg = config_json.pack_config(pack_name, author=author, description=description)

    # Deduplicate dest paths (keep first source)
    seen_dest: set[str] = set()
    unique: list[FilePlan] = []
    for fp in plans_files:
        key = str(fp.dest).lower()
        if key in seen_dest:
            continue
        seen_dest.add(key)
        unique.append(fp)

    return ConversionPlan(
        submod_dir=primary_submod,
        pack_dir=primary_pack,
        files=unique,
        config_path=primary_submod / "config.json",
        config_payload=cfg,
        pack_config_path=primary_pack / "config.json",
        pack_config_payload=pack_cfg,
        label=f"Tactical Reload -> {submod_name}",
    )


def execute_plan(plan: ConversionPlan, *, overwrite: bool = False) -> list[str]:
    """Execute copy + config writes. Returns log lines."""
    logs: list[str] = []
    if plan.submod_dir.exists() and any(plan.submod_dir.iterdir()) and not overwrite:
        raise FileExistsError(
            f"SubMod folder already exists and is not empty: {plan.submod_dir}"
        )

    for fp in plan.files:
        fp.dest.parent.mkdir(parents=True, exist_ok=True)
        if fp.dest.exists() and not overwrite:
            raise FileExistsError(f"Refusing to overwrite: {fp.dest}")
        shutil.copy2(fp.source, fp.dest)
        logs.append(f"Copied {fp.source.name} -> {fp.dest}")

    if plan.config_path and plan.config_payload is not None:
        if plan.config_path.exists() and not overwrite:
            raise FileExistsError(f"Refusing to overwrite: {plan.config_path}")
        config_json.write_json(plan.config_path, plan.config_payload)
        logs.append(f"Wrote {plan.config_path}")

    if plan.pack_config_path and plan.pack_config_payload is not None:
        # Pack config: write if missing, or overwrite only when allowed
        if not plan.pack_config_path.exists() or overwrite:
            config_json.write_json(plan.pack_config_path, plan.pack_config_payload)
            logs.append(f"Wrote {plan.pack_config_path}")
        else:
            logs.append(f"Kept existing pack config {plan.pack_config_path}")

    return logs
