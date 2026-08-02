"""Parse Creation Kit / xEdit subgraph text dumps (tab-separated)."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class SubgraphEntry:
    """One Weapon-role subgraph row from a SubGraphData_*.txt dump."""

    role: str
    perspective: str  # "1st" | "3rd" | other
    actor_keywords: tuple[str, ...]
    target_keywords: tuple[str, ...]
    behavior_path: str
    animation_paths: tuple[str, ...]

    @property
    def is_first_person(self) -> bool:
        return self.perspective.strip().lower() in {"1st", "first", "1"}

    @property
    def is_third_person(self) -> bool:
        return self.perspective.strip().lower() in {"3rd", "third", "3"}


@dataclass
class SubgraphData:
    """Parsed subgraph file plus helpers for anim-root discovery."""

    path: Path | None = None
    entries: list[SubgraphEntry] = field(default_factory=list)

    def weapon_entries(self, include_1st: bool = True, include_3rd: bool = False) -> list[SubgraphEntry]:
        out: list[SubgraphEntry] = []
        for e in self.entries:
            if e.role.strip().lower() != "weapon":
                continue
            if e.is_first_person and include_1st:
                out.append(e)
            elif e.is_third_person and include_3rd:
                out.append(e)
        return out

    def target_keywords(self, include_1st: bool = True, include_3rd: bool = False) -> list[str]:
        seen: set[str] = set()
        ordered: list[str] = []
        for e in self.weapon_entries(include_1st, include_3rd):
            for kw in e.target_keywords:
                key = kw.lower()
                if key and key not in seen:
                    seen.add(key)
                    ordered.append(kw)
        return ordered

    def animation_path_hints(self, include_1st: bool = True, include_3rd: bool = False) -> list[str]:
        """Unique animation path strings (normalized with backslashes)."""
        seen: set[str] = set()
        ordered: list[str] = []
        for e in self.weapon_entries(include_1st, include_3rd):
            for p in e.animation_paths:
                norm = p.replace("/", "\\").strip()
                key = norm.lower()
                if key and key not in seen:
                    seen.add(key)
                    ordered.append(norm)
        return ordered


def _split_keywords(cell: str) -> tuple[str, ...]:
    parts = [p.strip() for p in cell.split(",")]
    return tuple(p for p in parts if p)


def _split_paths(cell: str) -> tuple[str, ...]:
    parts = [p.strip() for p in cell.split(",")]
    return tuple(p for p in parts if p)


def parse_subgraph_text(text: str, source: Path | None = None) -> SubgraphData:
    """Parse a tab-separated subgraph dump.

    Expected header:
      Role  Perspective  Actor Keyword(s)  Target Keyword(s)  Behavior Path  Animation Path(s)
    """
    data = SubgraphData(path=source)
    for raw_line in text.splitlines():
        line = raw_line.strip("\ufeff").rstrip("\n\r")
        if not line.strip():
            continue
        cols = line.split("\t")
        if len(cols) < 6:
            continue
        role = cols[0].strip()
        if role.lower() == "role":
            continue
        data.entries.append(
            SubgraphEntry(
                role=role,
                perspective=cols[1].strip(),
                actor_keywords=_split_keywords(cols[2]),
                target_keywords=_split_keywords(cols[3]),
                behavior_path=cols[4].strip(),
                animation_paths=_split_paths(cols[5]),
            )
        )
    return data


def load_subgraph_file(path: Path) -> SubgraphData:
    """Load and parse a subgraph text file from disk."""
    text = path.read_text(encoding="utf-8", errors="replace")
    return parse_subgraph_text(text, source=path)


def find_subgraph_files_near(root: Path) -> list[Path]:
    """Discover SubGraphData_*.txt dumps under a mod / meshes root."""
    root = root.resolve()
    hits: list[Path] = []
    patterns = ("SubGraphData_*.txt", "*SubGraphData*.txt", "*SubgraphData*.txt")
    search_roots = [root]
    meshes = root / "Meshes"
    if meshes.is_dir():
        search_roots.append(meshes)
    actors = root / "Meshes" / "Actors"
    if not actors.is_dir():
        actors = root / "Meshes" / "actors"
    if actors.is_dir():
        search_roots.append(actors)

    seen: set[str] = set()
    for base in search_roots:
        if not base.is_dir():
            continue
        for pattern in patterns:
            for p in base.rglob(pattern):
                key = str(p.resolve()).lower()
                if key in seen:
                    continue
                # Skip AnimTextData noise; prefer Meshes/Actors dumps.
                parts_lower = {part.lower() for part in p.parts}
                if "animtextdata" in parts_lower and "subgraphdata" not in p.name.lower():
                    continue
                if p.name.lower().startswith("persistantsubgraph"):
                    continue
                seen.add(key)
                hits.append(p)
    hits.sort(key=lambda p: str(p).lower())
    return hits
