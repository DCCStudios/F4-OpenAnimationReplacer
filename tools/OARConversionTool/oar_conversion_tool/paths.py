"""Destination resolution and animation-root discovery."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

OAR_DIR_NAME = "OpenAnimationReplacer"
ANIMATIONS_MARKERS = ("animations",)

# Tactical Reload's "reserve" reload animation, i.e. the one played when a mag still has
# a round chambered and gets swapped/tossed rather than run empty. Most TR weapon packs name
# it WPNReloadReserve.hkx, but some name the same role WPNReloadTactical.hkx instead - both
# are the file OAR conversion copies to WPNReload.hkx, so every consumer treats them as
# interchangeable aliases of the same source animation, never as two different things.
TACTICAL_RESERVE_NAMES: tuple[str, ...] = ("WPNReloadReserve.hkx", "WPNReloadTactical.hkx")


@dataclass(frozen=True)
class AnimRoot:
    """A selectable animation folder root under _1stPerson or 3rd-person Animations."""

    name: str
    perspective: str  # "1st" | "3rd"
    absolute_path: Path
    source_mod: Path  # user-provided source directory this was found under
    relative_under_animations: str  # e.g. "SCAR" or "P226"


def normalize_game_path(p: str) -> str:
    return p.replace("/", "\\").strip().strip("\\")


def find_meshes_root(start: Path) -> Path | None:
    """Walk up / into a path to find a Meshes folder."""
    start = start.resolve()
    candidates = [start, *start.parents]
    for c in candidates:
        if c.name.lower() == "meshes":
            return c
        m = c / "Meshes"
        if m.is_dir():
            return m
        m = c / "meshes"
        if m.is_dir():
            return m
    # Also look one level down from start
    for child in start.iterdir() if start.is_dir() else []:
        if child.is_dir() and child.name.lower() == "meshes":
            return child
    return None


def animations_dir_for_perspective(meshes: Path, perspective: str) -> Path | None:
    """Return .../Actors/Character/_1stPerson/Animations or 3rd-person equivalent."""
    # Case-insensitive walk
    actors = _find_child(meshes, "Actors") or _find_child(meshes, "actors")
    if not actors:
        return None
    character = _find_child(actors, "Character") or _find_child(actors, "character")
    if not character:
        return None
    if perspective == "1st":
        person = _find_child(character, "_1stPerson") or _find_child(character, "_1stperson")
    else:
        # 3rd person weapon anims often live under Animations\Weapon\...
        # OAR examples for weapons are 1st; for 3rd we still use Character\Animations.
        person = character
    if not person:
        return None
    anims = _find_child(person, "Animations") or _find_child(person, "animations")
    return anims


def _find_child(parent: Path, name: str) -> Path | None:
    target = name.lower()
    if not parent.is_dir():
        return None
    for child in parent.iterdir():
        if child.is_dir() and child.name.lower() == target:
            return child
    return None


def _is_game_oar_dir(path: Path) -> bool:
    """True only for in-game OAR folders: .../Animations/OpenAnimationReplacer.

    A bare folder named OpenAnimationReplacer (e.g. this git repo) must NOT match,
    or destinations under the tools tree would write into the repo root.
    """
    return (
        path.name.lower() == OAR_DIR_NAME.lower()
        and path.parent.name.lower() == "animations"
    )


def resolve_oar_base(destination: Path, perspective: str = "1st") -> Path:
    """Resolve where PackName/SubModName should be created.

    If destination is already under (or is) an in-game
    ``Animations\\OpenAnimationReplacer`` folder, nest under that. Otherwise create
    the full Meshes\\Actors\\Character\\{_1stPerson|...}\\Animations\\OpenAnimationReplacer
    tree under destination.
    """
    destination = destination.resolve()
    parts = list(destination.parts)

    if _is_game_oar_dir(destination):
        return destination

    # Truncate to .../Animations/OpenAnimationReplacer when present in the path
    for i, part in enumerate(parts):
        if (
            part.lower() == OAR_DIR_NAME.lower()
            and i > 0
            and parts[i - 1].lower() == "animations"
        ):
            return Path(*parts[: i + 1])

    # Direct child named OpenAnimationReplacer only counts if parent is Animations
    # (i.e. user selected the Animations folder).
    direct = destination / OAR_DIR_NAME
    if direct.is_dir() and _is_game_oar_dir(direct):
        return direct

    # Search downward for an in-game OAR tree
    if destination.is_dir():
        for child in destination.rglob(OAR_DIR_NAME):
            if child.is_dir() and _is_game_oar_dir(child):
                return child

    if perspective == "1st":
        return (
            destination
            / "Meshes"
            / "Actors"
            / "Character"
            / "_1stPerson"
            / "Animations"
            / OAR_DIR_NAME
        )
    return (
        destination
        / "Meshes"
        / "Actors"
        / "Character"
        / "Animations"
        / OAR_DIR_NAME
    )


def scan_anim_roots(
    source_dirs: list[Path],
    *,
    include_1st: bool = True,
    include_3rd: bool = False,
    require_file: str | tuple[str, ...] | None = TACTICAL_RESERVE_NAMES,
) -> list[AnimRoot]:
    """Find selectable anim roots that contain require_file (case-insensitive)."""
    roots: list[AnimRoot] = []
    seen: set[str] = set()

    perspectives: list[str] = []
    if include_1st:
        perspectives.append("1st")
    if include_3rd:
        perspectives.append("3rd")

    for src in source_dirs:
        src = src.resolve()
        if not src.exists():
            continue
        meshes = find_meshes_root(src) or src
        for persp in perspectives:
            anims = animations_dir_for_perspective(meshes, persp)
            if anims is None or not anims.is_dir():
                # Fallback: search for _1stPerson/Animations under src
                if persp == "1st":
                    for candidate in src.rglob("_1stPerson"):
                        a = _find_child(candidate, "Animations") or _find_child(candidate, "animations")
                        if a:
                            anims = a
                            break
                if anims is None:
                    continue

            # Immediate children of Animations are candidate roots (SCAR, P226, ...)
            for child in sorted(anims.iterdir(), key=lambda p: p.name.lower()):
                if not child.is_dir():
                    continue
                if child.name.lower() == OAR_DIR_NAME.lower():
                    continue
                if require_file:
                    if not _tree_has_file(child, require_file):
                        continue
                key = f"{src}::{persp}::{child.name}".lower()
                if key in seen:
                    continue
                seen.add(key)
                roots.append(
                    AnimRoot(
                        name=child.name,
                        perspective=persp,
                        absolute_path=child,
                        source_mod=src,
                        relative_under_animations=child.name,
                    )
                )
    return roots


def _tree_has_file(root: Path, filename: str | tuple[str, ...]) -> bool:
    targets = {filename.lower()} if isinstance(filename, str) else {f.lower() for f in filename}
    for p in root.rglob("*"):
        if p.is_file() and p.name.lower() in targets:
            return True
    return False


def find_files_ci(root: Path, filename: str | tuple[str, ...]) -> list[Path]:
    """All files named filename under root (case-insensitive), sorted.

    filename may be a single name or a tuple of interchangeable aliases (e.g.
    TACTICAL_RESERVE_NAMES); any file matching one of the names is included.
    """
    targets = {filename.lower()} if isinstance(filename, str) else {f.lower() for f in filename}
    hits = [p for p in root.rglob("*") if p.is_file() and p.name.lower() in targets]
    hits.sort(key=lambda p: str(p).lower())
    return hits


def relative_under_root(path: Path, root: Path) -> Path:
    return path.resolve().relative_to(root.resolve())
