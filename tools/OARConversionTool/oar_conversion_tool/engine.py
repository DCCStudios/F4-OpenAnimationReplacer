"""High-level conversion orchestration used by the GUI and smoke tests."""

from __future__ import annotations

import re
import shutil
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from .ba2_io import Ba2Error, extract_meshes_actors
from .convert_idle import plan_idle_empty
from .convert_tr import ConversionPlan, execute_plan, plan_tactical_reload
from .esp_io import EspInfo, parse_esp, patch_esp_remove_tr
from .paths import TACTICAL_RESERVE_NAMES, AnimRoot, scan_anim_roots
from .subgraph import SubgraphData, find_subgraph_files_near, load_subgraph_file


LogFn = Callable[[str], None]


@dataclass
class JobOptions:
    """User-facing options for a conversion run."""

    source_dirs: list[Path] = field(default_factory=list)
    subgraph_paths: list[Path] = field(default_factory=list)
    esp_path: Path | None = None
    destination: Path | None = None

    # When set, this job's source came from a BA2 archive (in lieu of a plain source
    # folder): ba2_extract_dir already holds the extracted Meshes\Actors\... tree and is
    # also listed in source_dirs. run_job cleans it up afterward unless keep_ba2_extracted.
    ba2_path: Path | None = None
    ba2_extract_dir: Path | None = None
    keep_ba2_extracted: bool = False

    do_tactical_reload: bool = True
    do_idle_empty: bool = False
    include_1st: bool = True
    include_3rd: bool = False

    selected_roots: list[AnimRoot] = field(default_factory=list)

    pack_name: str = "OAR Conversion"
    tr_submod_name: str = "Tactical Reload"
    idle_submod_name: str = "Idle Empty"
    author: str = ""
    description: str = ""
    priority: int = 3000
    idle_bones: list[str] = field(default_factory=lambda: ["WeaponBolt"])
    deactivation_delay: float = 0.1

    equipped_form_id: str | None = None  # e.g. "0x2E1F"
    equipped_plugin: str | None = None

    patch_esp: bool = False
    esp_output: Path | None = None
    remove_tr_keyword: bool = True
    remove_tr_master: bool = True
    overwrite: bool = False


@dataclass
class PreviewResult:
    plans: list[ConversionPlan] = field(default_factory=list)
    esp_info: EspInfo | None = None
    subgraphs: list[SubgraphData] = field(default_factory=list)
    messages: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)


def extract_ba2_source(ba2_path: Path, extract_root: Path, *, log: LogFn | None = None) -> Path:
    """Extract meshes\\actors from ba2_path into a fresh folder under extract_root.

    Returns the created extraction directory. Writing entries to
    ``extract_dir / entry.normalized()`` (done in ba2_io.extract_entries) reproduces the
    real ``Meshes\\Actors\\...`` tree, so the returned directory can be used directly as
    a JobOptions.source_dirs entry with no other code changes.
    """
    extract_root.mkdir(parents=True, exist_ok=True)
    # Keep the folder name recognizable but filesystem-safe; unique suffix avoids
    # collisions if the same archive is picked for more than one queued job.
    safe_stem = re.sub(r"[^A-Za-z0-9_.-]+", "_", ba2_path.stem) or "BA2"
    extract_dir = extract_root / f"{safe_stem}_{uuid.uuid4().hex[:8]}"
    extract_dir.mkdir(parents=True, exist_ok=False)
    try:
        written = extract_meshes_actors(ba2_path, extract_dir, log=log)
    except Ba2Error:
        shutil.rmtree(extract_dir, ignore_errors=True)
        raise
    if log:
        log(f"Extracted {len(written)} file(s) from {ba2_path.name} -> {extract_dir}")
    return extract_dir


def cleanup_ba2_extraction(opts: JobOptions, *, had_errors: bool, log: LogFn | None = None) -> None:
    """Delete or keep a job's BA2 extraction folder once the job has finished running."""
    if opts.ba2_extract_dir is None or not opts.ba2_extract_dir.exists():
        return

    def _log(msg: str) -> None:
        if log:
            log(msg)

    if opts.keep_ba2_extracted:
        _log(f"Keeping extracted BA2 files (user preference): {opts.ba2_extract_dir}")
        return
    if had_errors:
        _log(f"Job had errors; keeping extracted BA2 files for inspection: {opts.ba2_extract_dir}")
        return
    try:
        shutil.rmtree(opts.ba2_extract_dir)
        _log(f"Deleted extracted BA2 files: {opts.ba2_extract_dir}")
    except OSError as exc:
        _log(f"Warning: failed to delete extracted BA2 files ({opts.ba2_extract_dir}): {exc}")


def validate_inputs(opts: JobOptions) -> list[str]:
    errors: list[str] = []
    has_esp = opts.esp_path is not None and opts.esp_path.is_file()
    has_sub = any(p.is_file() for p in opts.subgraph_paths)
    has_src = any(p.exists() for p in opts.source_dirs)
    if not has_esp and not has_sub:
        errors.append("Provide at least one ESP or subgraph text file.")
    if not has_src:
        # ESP-only: infer the mod folder from the ESP path so anim scans still work.
        if has_esp:
            parent = opts.esp_path.parent  # type: ignore[union-attr]
            if parent not in opts.source_dirs:
                opts.source_dirs.append(parent)
        else:
            errors.append("Provide at least one source directory containing Meshes/animations.")
    if opts.destination is None:
        errors.append("Choose a destination folder.")
    if not opts.do_tactical_reload and not opts.do_idle_empty and not opts.patch_esp:
        errors.append("Select at least one operation (Tactical Reload, Idle Empty, or ESP cleanup).")
    return errors


def discover_subgraphs(opts: JobOptions) -> list[SubgraphData]:
    paths = list(opts.subgraph_paths)
    for src in opts.source_dirs:
        for found in find_subgraph_files_near(src):
            if found not in paths:
                paths.append(found)
    if opts.esp_path:
        for found in find_subgraph_files_near(opts.esp_path.parent):
            if found not in paths:
                paths.append(found)
    out: list[SubgraphData] = []
    seen: set[str] = set()
    for p in paths:
        if not p.is_file():
            continue
        key = str(p.resolve()).lower()
        if key in seen:
            continue
        seen.add(key)
        out.append(load_subgraph_file(p))
    return out


def discover_roots(opts: JobOptions) -> list[AnimRoot]:
    require: str | tuple[str, ...] = TACTICAL_RESERVE_NAMES if opts.do_tactical_reload else "WPNReload.hkx"
    # If both ops, prefer roots that have reserve (TR); idle can still use those roots' WPNReload
    if opts.do_tactical_reload and opts.do_idle_empty:
        roots = scan_anim_roots(
            opts.source_dirs,
            include_1st=opts.include_1st,
            include_3rd=opts.include_3rd,
            require_file=TACTICAL_RESERVE_NAMES,
        )
        # Also include roots that only have WPNReload for idle-only coverage
        extra = scan_anim_roots(
            opts.source_dirs,
            include_1st=opts.include_1st,
            include_3rd=opts.include_3rd,
            require_file="WPNReload.hkx",
        )
        seen = {f"{r.source_mod}::{r.perspective}::{r.name}".lower() for r in roots}
        for r in extra:
            key = f"{r.source_mod}::{r.perspective}::{r.name}".lower()
            if key not in seen:
                roots.append(r)
        return roots
    return scan_anim_roots(
        opts.source_dirs,
        include_1st=opts.include_1st,
        include_3rd=opts.include_3rd,
        require_file=require,
    )


def build_preview(opts: JobOptions) -> PreviewResult:
    result = PreviewResult()
    result.errors = validate_inputs(opts)
    if result.errors:
        return result

    if opts.ba2_path is not None:
        result.messages.append(
            f"Source: extracted from {opts.ba2_path.name} -> {opts.ba2_extract_dir}"
        )

    if opts.esp_path and opts.esp_path.is_file():
        try:
            result.esp_info = parse_esp(opts.esp_path)
            result.messages.append(
                f"ESP: {opts.esp_path.name}: {len(result.esp_info.weapons)} WEAP, "
                f"masters={result.esp_info.masters}"
            )
        except Exception as exc:  # noqa: BLE001
            result.errors.append(f"Failed to parse ESP: {exc}")

    result.subgraphs = discover_subgraphs(opts)
    for sg in result.subgraphs:
        result.messages.append(
            f"Subgraph: {sg.path.name if sg.path else '?'}: {len(sg.entries)} rows"
        )

    equipped = None
    if opts.equipped_form_id and opts.equipped_plugin:
        equipped = {"formID": opts.equipped_form_id, "pluginName": opts.equipped_plugin}

    roots = opts.selected_roots or []
    if not roots and (opts.do_tactical_reload or opts.do_idle_empty):
        result.messages.append("No anim roots selected yet; run Scan and choose roots.")
        return result

    assert opts.destination is not None

    if opts.do_tactical_reload:
        tr_roots = [r for r in roots]
        plan = plan_tactical_reload(
            tr_roots,
            opts.destination,
            pack_name=opts.pack_name,
            submod_name=opts.tr_submod_name,
            description=opts.description,
            priority=opts.priority,
            equipped=equipped,
            author=opts.author,
            overwrite=opts.overwrite,
        )
        result.plans.append(plan)
        result.messages.append(f"TR plan: {len(plan.files)} file(s) -> {plan.submod_dir}")

    if opts.do_idle_empty:
        plan = plan_idle_empty(
            roots,
            opts.destination,
            pack_name=opts.pack_name,
            submod_name=opts.idle_submod_name,
            description=opts.description,
            priority=opts.priority,
            bones=opts.idle_bones,
            equipped=equipped,
            author=opts.author,
            deactivation_delay=opts.deactivation_delay,
        )
        result.plans.append(plan)
        result.messages.append(f"Idle plan: {len(plan.files)} file(s) -> {plan.submod_dir}")

    return result


def run_job(opts: JobOptions, log: LogFn | None = None) -> PreviewResult:
    """Execute a prepared job (plans from current options)."""
    def _log(msg: str) -> None:
        if log:
            log(msg)

    preview = build_preview(opts)
    try:
        if preview.errors:
            for e in preview.errors:
                _log(f"ERROR: {e}")
            return preview

        for plan in preview.plans:
            _log(f"Running: {plan.label}")
            try:
                for line in execute_plan(plan, overwrite=opts.overwrite):
                    _log(line)
            except Exception as exc:  # noqa: BLE001
                preview.errors.append(str(exc))
                _log(f"ERROR: {exc}")
                return preview

        if opts.patch_esp and opts.esp_path:
            out = opts.esp_output or opts.esp_path.with_name(
                opts.esp_path.stem + "_OAR" + opts.esp_path.suffix
            )
            _log(f"Patching ESP -> {out}")
            try:
                report = patch_esp_remove_tr(
                    opts.esp_path,
                    out,
                    remove_keyword=opts.remove_tr_keyword,
                    remove_tr_master_if_unused=opts.remove_tr_master,
                )
                for k, v in report.items():
                    _log(f"  ESP {k}: {v}")
            except Exception as exc:  # noqa: BLE001
                preview.errors.append(str(exc))
                _log(f"ERROR: {exc}")

        _log("Done.")
        return preview
    finally:
        # Always resolve the BA2 extraction folder's fate, even on early-return errors,
        # so a failed job never silently leaks (or silently deletes) extracted files.
        cleanup_ba2_extraction(opts, had_errors=bool(preview.errors), log=log)
