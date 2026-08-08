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


# SubMod folder suffix budget. The in-game path already consumes ~100-140 chars before
# the SubMod name (mod root + Meshes\\...\\OpenAnimationReplacer\\Pack\\), and attachment
# nests under a root can add another ~80. Windows MAX_PATH is 260 for tools like MO2 that
# do not use the \\?\ long-path prefix; keeping the label well under this ceiling is what
# stops a multi-root "+"-joined name from making the whole mod unopenable in MO2.
_MAX_LABEL_CHARS = 48


@dataclass
class RootWeaponGroup:
    """One or more AnimRoots that all belong to the same WEAP form(s).

    ``weapons`` is empty for unmatched roots (no ESP, no subgraph coverage, or no
    keyword overlap found) - callers should treat those groups like the tool's original
    pre-matching behavior: no auto-derived ``IsEquipped``. When *every* root is
    unmatched they stay in one shared group; when some roots matched and others did
    not, each unmatched root gets its own group so the SubMod folder name stays a
    short single root name instead of ``RootA+RootB+RootC+...`` (which has blown past
    MAX_PATH on large packs like EFT and broken MO2's directory walker).

    Matched groups always carry exactly one weapon by the time ``match_roots_to_weapons``
    returns (see ``expand_groups_per_weapon``): a root that matches several WEAP forms
    (e.g. one EFT-style folder shared by five Deagle variants) is expanded into one
    group per weapon, each still listing the same shared root(s), so the engine emits
    one SubMod per weapon instead of one SubMod with an OR'd ``IsEquipped``.
    """

    roots: list[AnimRoot] = field(default_factory=list)
    weapons: list[WeaponMatch] = field(default_factory=list)

    @property
    def label(self) -> str:
        """Short, filesystem-safe name for this group (never a long "+"-joined list)."""
        if self.weapons:
            # A matched group is keyed on its own weapon, not the (possibly shared)
            # root folder name: after expand_groups_per_weapon, several groups can
            # share the same root(s), and a root-name label would collide on disk
            # (five Deagle SubMods all named "... - EFTPackDeagle") and silently
            # overwrite each other.
            if len(self.weapons) == 1:
                raw = _short_weapon_label(self.weapons[0])
            else:
                # Defensive only: match_roots_to_weapons always expands to one weapon
                # per group, so this branch should not be reachable in practice.
                raw = _short_weapon_label(self.weapons[0]) + f"etc{len(self.weapons)}"
        elif len(self.roots) == 1:
            raw = self.roots[0].name
        else:
            raw = "Unmatched"
        return _clamp_label(raw)


def _short_weapon_label(w: WeaponMatch) -> str:
    """Weapon EDID with a leading 'WPN_'/'WPN' stripped, e.g. 'WPNDeagleL5' -> 'DeagleL5'.

    Trims Tactical Reload's near-universal WPN(_) naming boilerplate so SubMod folder
    names read as the weapon name while staying unique per weapon (the EDID itself).
    """
    edid = w.edid
    for prefix in ("WPN_", "WPN"):
        if edid.lower().startswith(prefix.lower()) and len(edid) > len(prefix):
            return edid[len(prefix):]
    return edid


# Characters Windows forbids in file/folder names; EDIDs are normally alnum/underscore
# already, but sanitize defensively since a label can also fall back to a root folder
# name pulled straight off disk.
_ILLEGAL_PATH_CHARS = '<>:"/\\|?*'


def _clamp_label(raw: str) -> str:
    """Trim a SubMod suffix to _MAX_LABEL_CHARS without leaving a trailing separator."""
    cleaned = raw.strip().strip(". ")
    if not cleaned:
        return "Unmatched"
    cleaned = "".join("_" if ch in _ILLEGAL_PATH_CHARS else ch for ch in cleaned)
    if len(cleaned) <= _MAX_LABEL_CHARS:
        return cleaned
    # Keep the start (usually the distinctive weapon/root token); ellipsis marks truncation.
    return cleaned[: _MAX_LABEL_CHARS - 1].rstrip(" -_+.") + "…"


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
    """Group roots by the WEAP form(s) that use them, one group per weapon.

    Roots are first grouped by the exact set of weapons they resolve to (so a root
    shared by five weapons and a root shared by the same five weapons merge), then
    that intermediate grouping is expanded per weapon (see ``expand_groups_per_weapon``)
    so the caller always gets one group per weapon, not one group per weapon *set*.
    A single trailing entry (or one group per leftover root) carries an empty
    ``weapons`` list for anything that couldn't be matched. When there is no ESP to
    match against at all, every root lands in that unmatched case, i.e. the caller's
    original single-shared-SubMod behavior.
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
        #    Also try stripping a trailing "Anims"/"Anim" from the folder name
        #    (EFT-style ``AK400Anims`` <-> keyword ``AnimsAK400``), which is the same
        #    TR naming convention written in the opposite order.
        root_token = _normalize_token(root.name)
        suffix_tokens = {root_token}
        for trailer in ("anims", "anim"):
            if root_token.endswith(trailer) and len(root_token) > len(trailer):
                suffix_tokens.add(root_token[: -len(trailer)])
        for token in suffix_tokens:
            for wm in weapon_by_suffix.get(token, []):
                matched[wm.form_id_hex] = wm

        if not matched:
            unmatched.append(root)
            continue
        key = frozenset(matched.keys())
        group = groups.setdefault(key, RootWeaponGroup(weapons=list(matched.values())))
        group.roots.append(root)

    result = list(groups.values())
    if unmatched:
        if result:
            # Some roots matched: give each unmatched root its own SubMod so the folder
            # name is just that root's name. Bundling them into one group would produce
            # a "+"-joined label of every leftover root - on a pack the size of EFT that
            # alone pushed full paths past Windows MAX_PATH and made MO2 refuse to open
            # the destination mod.
            for root in unmatched:
                result.append(RootWeaponGroup(roots=[root], weapons=[]))
        else:
            # Nothing matched at all (no useful ESP keywords / no subgraph coverage):
            # keep the classic single shared SubMod with no IsEquipped and no name suffix
            # (engine only appends group.label when len(groups) > 1).
            result.append(RootWeaponGroup(roots=unmatched, weapons=[]))
    return expand_groups_per_weapon(result)


def expand_groups_per_weapon(groups: list[RootWeaponGroup]) -> list[RootWeaponGroup]:
    """Split each multi-weapon group into one group per weapon.

    OAR SubMod ``conditions`` are ANDed as a whole, so one SubMod covering several
    weapons behind an OR of ``IsEquipped`` conditions is still a single unit; the user
    wants a separate SubMod folder per weapon instead (the shared root's animation
    tree is duplicated into each one, matching how a one-root-per-weapon pack like
    Ozzy M4 Platform was already handled). Unmatched groups have no weapons to split
    on and pass through unchanged - they're already shaped correctly, one per leftover
    root (or a single shared fallback group when nothing matched at all).
    """
    out: list[RootWeaponGroup] = []
    for g in groups:
        if not g.weapons:
            out.append(g)
            continue
        for w in g.weapons:
            out.append(RootWeaponGroup(roots=list(g.roots), weapons=[w]))
    return out
