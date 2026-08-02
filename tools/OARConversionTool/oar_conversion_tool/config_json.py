"""Build OAR SubMod / pack config.json payloads."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


PLAYER_FORM = {"formID": "0x14", "pluginName": "Fallout4.esm"}


def pack_config(name: str, author: str = "", description: str = "") -> dict[str, Any]:
    return {
        "name": name,
        "author": author,
        "description": description,
    }


def _is_equipped(form: dict[str, str]) -> dict[str, Any]:
    return {
        "Form": {
            "formID": form["formID"],
            "pluginName": form["pluginName"],
        },
        "condition": "IsEquipped",
        "disabled": False,
        "negated": False,
    }


def _equipped_condition(
    equipped: dict[str, str] | list[dict[str, str]] | None,
) -> dict[str, Any] | None:
    """Build the extra condition gating a SubMod to specific weapon form(s).

    A single form (dict) produces a plain IsEquipped, exactly as before. A list of
    forms - used when several WEAP records share one animation root, e.g. a base
    weapon plus its "_Unique" leveled variant - produces an OR of IsEquipped
    conditions, so the SubMod stays active no matter which of that root's weapons is
    actually drawn.
    """
    if not equipped:
        return None
    forms = equipped if isinstance(equipped, list) else [equipped]
    if len(forms) == 1:
        return _is_equipped(forms[0])
    return {
        "condition": "OR",
        "conditions": [_is_equipped(f) for f in forms],
        "disabled": False,
        "negated": False,
    }


def tactical_reload_config(
    name: str,
    description: str = "",
    *,
    priority: int = 3000,
    equipped: dict[str, str] | list[dict[str, str]] | None = None,
) -> dict[str, Any]:
    """P890-style TR config: ammo != 0, no eventsOnEnd, optional IsEquipped."""
    conditions: list[dict[str, Any]] = [
        {
            "Form": dict(PLAYER_FORM),
            "condition": "IsForm",
            "disabled": False,
            "negated": False,
        },
        {
            "condition": "IsWeaponDrawn",
            "disabled": False,
            "negated": False,
        },
        {
            "comparison": 0,
            "condition": "CurrentMagazineAmmo",
            "disabled": False,
            "negated": True,
            "numericValue": {"type": "Static", "value": 0.0},
        },
    ]
    cond = _equipped_condition(equipped)
    if cond:
        conditions.append(cond)
    return {
        "conditions": conditions,
        "description": description,
        "disabled": False,
        "interruptible": False,
        "keepRandomResultsOnLoop": False,
        "name": name,
        "playOnceFullBody": True,
        "priority": priority,
        "replaceAnnotations": True,
        "replaceOnEcho": True,
        "replaceOnLoop": False,
        "shareRandomResults": False,
        "trackFilter": {
            "blendInTime": 0.0,
            "blendOutTime": 0.0,
            "bones": [],
            "enabled": False,
            "excludeBones": [],
            "excludeChildren": True,
            "includeChildren": True,
            "mode": "additive",
            "sampleFrame": -1.0,
            "weight": 1.0,
        },
    }


def idle_empty_config(
    name: str,
    description: str = "",
    *,
    priority: int = 3000,
    bones: list[str] | None = None,
    equipped: dict[str, str] | list[dict[str, str]] | None = None,
    deactivation_delay: float = 0.1,
) -> dict[str, Any]:
    """Sig/P890-style Idle Empty with sampleFrame 0 and deactivationDelay."""
    if bones is None:
        bones = ["WeaponBolt"]
    conditions: list[dict[str, Any]] = [
        {
            "Form": dict(PLAYER_FORM),
            "condition": "IsForm",
            "disabled": False,
            "negated": False,
        },
        {
            "condition": "IsWeaponDrawn",
            "disabled": False,
            "negated": False,
        },
        {
            "comparison": 0,
            "condition": "CurrentMagazineAmmo",
            "disabled": False,
            "negated": False,
            "numericValue": {"type": "Static", "value": 0.0},
        },
        {
            "condition": "IsReloading",
            "disabled": False,
            "negated": True,
        },
    ]
    cond = _equipped_condition(equipped)
    if cond:
        conditions.append(cond)
    return {
        "conditions": conditions,
        "customBlendTimeOnInterrupt": 0.0,
        "deactivationDelay": deactivation_delay,
        "description": description,
        "disabled": False,
        "interruptible": True,
        "keepRandomResultsOnLoop": False,
        "name": name,
        "priority": priority,
        "replaceAnnotations": False,
        "replaceOnEcho": True,
        "replaceOnLoop": False,
        "shareRandomResults": False,
        "trackFilter": {
            "blendInTime": 0.0,
            "blendOutTime": 0.0,
            "bones": list(bones),
            "enabled": True,
            "excludeBones": [],
            "excludeChildren": True,
            "includeChildren": True,
            "mode": "override",
            "sampleFrame": 0.0,
            "weight": 1.0,
        },
    }


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=4) + "\n", encoding="utf-8")
