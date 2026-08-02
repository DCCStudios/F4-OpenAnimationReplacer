"""Match discovered animation roots to the specific WEAP form(s) that use them.

An ESP with several weapons (e.g. a rifle platform mod shipping M4/M16/FTAC/FSS/Icarus
variants) has one animation root folder per weapon, but a single OAR SubMod's
``conditions`` list is ANDed as a whole: if one shared SubMod held all five roots and
carried a single ``IsEquipped`` condition for, say, the M4, then drawing the M16 would
fail that condition and silently disable *all five* weapons' reload overrides, not just
mismatch its own. The fix is to know which WEAP form(s) each root actually belongs to, so
the engine can emit one SubMod per weapon (or per group of weapons that share a root),
each gated by its own ``IsEquipped``.

Two independent sources of evidence are combined (a root only needs one to be matched):

1. **Subgraph walk** - Tactical Reload's exported ``SubGraphData_*.txt`` dumps list, per
   row, the "Target Keyword(s)" (the ``AnimsXxx`` keyword(s) a WEAP must carry for that
   row's animation search path to apply) alongside the "Animation Path(s)" search order.
   The weapon's own root folder always appears in that search order with no trailing
   subfolder (e.g. ``...\\_1stPerson\\Animations\\SCAR``, as opposed to
   ``...\\SCAR\\AngledSight`` for an attachment variant of the *same* weapon), which is
   exactly the folder name ``scan_anim_roots()`` uses as an ``AnimRoot.name``. Matching a
   row's animation path leaf to a root name, then cross-referencing that row's target
   keywords against each WEAP's KWDA-resolved keyword editor IDs, tells us which weapon(s)
   a root belongs to. Precise, but only available when a mod ships a subgraph dump (many
   don't - it's a CK debug export, not something every TR weapon pack includes).

2. **Keyword-suffix heuristic** - Tactical Reload's own convention names each weapon's
   per-weapon keyword ``Anims<WeaponName>`` (sometimes ``Anims_<WeaponName>``), and that
   ``<WeaponName>`` suffix is, in practice, the same string as the animation root folder
   name (``AnimsSCAR`` <-> folder ``SCAR``, ``Anims_M4`` <-> folder ``M4``). This works
   even with zero subgraph coverage, which is the common case, and is what lets a mod like
   a multi-weapon rifle platform (no shipped subgraph dump at all) still get correct
   per-weapon SubMods.

Both sources feed the same output shape (root name -> candidate keyword strings), so a
root is considered matched if *either* source points at a WEAP; evidence is unioned, never
required from both, keeping the common single-subgraph-dump and zero-subgraph-dump cases
both fully covered.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .esp_io import EspInfo
from .paths import AnimRoot
from .subgraph import SubgraphData

# Tactical Reload's own generic "a reload just happened, played the reserve/tactical
# animation" keyword. It is not weapon-specific (every TR weapon in a pack carries it),
# so it must never be treated as a per-weapon suffix token - see _keyword_weapon_suffix.
_GENERIC_TR_KEYWORD_SUFFIXES = {"reloadreserve", "reloadtactical", "reload"}


@dataclass(frozen=True)
class WeaponMatch:
    """One WEAP form identified as using a particular animation root."""

    edid: str
    form_id_hex: str  # already masked to the low 24 bits by esp_io.parse_esp


@dataclass
class RootWeaponGroup:
    """One or more AnimRoots that all belong to the same WEAP form(s).

    ``weapons`` is empty for the trailing "unmatched" group (no ESP, no subgraph
    coverage, or no keyword overlap found) - callers should treat that group like the
    tool's original, pre-matching behavior: one shared SubMod, no auto-derived
    ``IsEquipped``.
    """

    roots: list[AnimRoot] = field(default_factory=list)
    weapons: list[WeaponMatch] = field(default_factory=list)

    @property
    def label(self) -> str:
        """Short, human/filesystem-friendly name for this group's roots."""
        return "+".join(r.name for r in self.roots) or "Unmatched"


def _normalize_token(s: str) -> str:
    """Lowercase, alnum-only form used to compare keyword suffixes against root names."""
    return "".join(ch for ch in s.lower() if ch.isalnum())


def _keyword_weapon_suffix(keyword_edid: str) -> str | None:
    """Strip a leading 'anims' from a keyword EDID, e.g. 'Anims_M4' -> 'm4'.

    Returns None for keywords that aren't the Anims<Weapon> pattern at all, or whose
    suffix is one of TR's generic (non-weapon-specific) reload keywords.
    """
    norm = _normalize_token(keyword_edid)
    if not norm.startswith("anims"):
        return None
    suffix = norm[len("anims"):]
    if not suffix or suffix in _GENERIC_TR_KEYWORD_SUFFIXES:
        return None
    return suffix


def _subgraph_root_keywords(
    subgraphs: list[SubgraphData],
    root_names: set[str],
    *,
    include_1st: bool = True,
    include_3rd: bool = False,
) -> dict[str, set[str]]:
    """root name (lowercased) -> set of Target Keyword(s) whose search path selects it."""
    mapping: dict[str, set[str]] = {name: set() for name in root_names}
    for sg in subgraphs:
        for entry in sg.weapon_entries(include_1st=include_1st, include_3rd=include_3rd):
            for raw_path in entry.animation_paths:
                segments = [s for s in raw_path.replace("/", "\\").split("\\") if s]
                if not segments:
                    continue
                leaf = segments[-1].lower()
                if leaf in mapping:
                    mapping[leaf].update(entry.target_keywords)
    return mapping


def match_roots_to_weapons(
    roots: list[AnimRoot],
    esp_info: EspInfo | None,
    subgraphs: list[SubgraphData],
    *,
    include_1st: bool = True,
    include_3rd: bool = False,
) -> list[RootWeaponGroup]:
    """Group roots by the WEAP form(s) that use them.

    Returns one group per distinct set of matched weapons (roots that resolve to the
    exact same weapon(s) are merged into one group), plus a single trailing group with
    an empty ``weapons`` list for any roots that couldn't be matched. When there is no
    ESP to match against at all, every root lands in that trailing unmatched group, i.e.
    the caller's original single-shared-SubMod behavior.
    """
    if not roots:
        return []
    if esp_info is None or not esp_info.weapons:
        return [RootWeaponGroup(roots=list(roots), weapons=[])]

    root_names = {r.name.lower() for r in roots}
    subgraph_map = _subgraph_root_keywords(
        subgraphs, root_names, include_1st=include_1st, include_3rd=include_3rd
    )

    # keyword edid (lowercased) -> WeaponMatch(es) that carry it
    weapon_by_keyword: dict[str, list[WeaponMatch]] = {}
    # normalized Anims<suffix> token -> WeaponMatch(es), for the heuristic fallback
    weapon_by_suffix: dict[str, list[WeaponMatch]] = {}
    for weap in esp_info.weapons:
        wm = WeaponMatch(edid=weap["edid"], form_id_hex=weap["form_id_hex"])
        for kw_edid in weap.get("keyword_edids", []):
            weapon_by_keyword.setdefault(kw_edid.lower(), []).append(wm)
            suffix = _keyword_weapon_suffix(kw_edid)
            if suffix:
                weapon_by_suffix.setdefault(suffix, []).append(wm)

    groups: dict[frozenset, RootWeaponGroup] = {}
    unmatched: list[AnimRoot] = []
    for root in roots:
        matched: dict[str, WeaponMatch] = {}

        # 1) Subgraph walk: root's own keyword set (if the root appears in any loaded
        #    subgraph's animation-path search order) resolved against the ESP's WEAP
        #    keywords.
        for kw in subgraph_map.get(root.name.lower(), ()):
            for wm in weapon_by_keyword.get(kw.lower(), []):
                matched[wm.form_id_hex] = wm

        # 2) Keyword-suffix heuristic: Anims<RootName> directly, independent of any
        #    subgraph dump. Runs unconditionally so a root with partial/no subgraph
        #    coverage still benefits from whatever this heuristic can find.
        for wm in weapon_by_suffix.get(_normalize_token(root.name), []):
            matched[wm.form_id_hex] = wm

        if not matched:
            unmatched.append(root)
            continue
        key = frozenset(matched.keys())
        group = groups.setdefault(key, RootWeaponGroup(weapons=list(matched.values())))
        group.roots.append(root)

    result = list(groups.values())
    if unmatched:
        result.append(RootWeaponGroup(roots=unmatched, weapons=[]))
    return result
