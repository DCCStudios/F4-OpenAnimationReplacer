# Open Animation Replacer — Quick Start Guide

## Installation

1. Copy `OpenAnimationReplacer.dll` and `OpenAnimationReplacer.ini` into  
   `Fallout 4/Data/F4SE/Plugins/`.
2. Launch the game through F4SE.

---

## Creating a Replacement Mod

### Folder Structure

Place your mod under any animation directory inside `Data/Meshes/`.  
The plugin scans recursively for folders named `OpenAnimationReplacer`.

```
Data/Meshes/
└── Actors/
    └── Character/
        └── Animations/
            └── OpenAnimationReplacer/
                └── MyAnimPack/              ← Replacer Mod folder
                    ├── config.json          ← (optional) mod metadata
                    └── Combat Idle/         ← SubMod folder
                        ├── config.json      ← conditions, priority & settings
                        └── mt_idle.hkx      ← replacement .hkx file
```

- **Replacer Mod folder** — any name. Groups related submods together.
- **SubMod folder** — use a descriptive name (e.g. `Combat Idle`, `Sprint Override`, `FemaleOnly`). The priority is defined inside the submod's `config.json`, not by the folder name.
- **`.hkx` files** — must mirror the relative path of the animation they replace. `mt_idle.hkx` in the submod root replaces `Actors/Character/Animations/mt_idle.hkx`.

### Mod-Level `config.json` (Optional)

```json
{
  "name": "My Animation Pack",
  "author": "YourName",
  "description": "Replaces idle animations when conditions are met."
}
```

### SubMod `config.json`

This is where all the behavior lives.

```json
{
  "name": "Combat Idle Override",
  "description": "Plays a custom idle when in combat with weapon drawn.",
  "priority": 1000,
  "disabled": false,
  "interruptible": true,
  "replaceOnLoop": true,
  "replaceOnEcho": true,
  "conditions": [
    { "condition": "IsInCombat" },
    { "condition": "IsWeaponDrawn" }
  ]
}
```

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | string | folder name | Display name in the GUI |
| `priority` | int | 0 | Higher priority submods are evaluated first |
| `disabled` | bool | false | Skip this submod entirely |
| `interruptible` | bool | false | Re-evaluate conditions every frame (allows mid-animation switching) |
| `replaceOnLoop` | bool | true | Re-evaluate when the clip loops |
| `replaceOnEcho` | bool | true | Re-evaluate on echo (transition blend) |
| `conditions` | array | [] | All must pass (AND logic). Empty = always matches |

---

## Conditions Reference

### Simple Conditions (no extra fields)

| Name | Description |
|---|---|
| `IsWeaponDrawn` | Actor has weapon or magic drawn |
| `IsInCombat` | Actor is in combat |
| `IsSprinting` | Actor is sprinting |
| `IsInAir` | Actor is airborne |
| `IsFemale` | Actor is female *(stub)* |
| `IsInPowerArmor` | Actor is in power armor *(stub)* |
| `IsSneaking` | Actor is sneaking |
| `IsADS` | Actor is aiming down sights |

### Form Conditions

```json
{
  "condition": "IsForm",
  "Form": {
    "pluginName": "Fallout4.esm",
    "formID": "0x14"
  }
}
```

Works with: `IsForm`, `IsActorBase`, `IsRace` *(stub)*, `CurrentWeather` *(stub)*.

### Keyword Condition

By editor ID (preferred):

```json
{ "condition": "HasKeyword", "editorID": "ActorTypeNPC" }
```

Or by form reference:

```json
{
  "condition": "HasKeyword",
  "Form": { "pluginName": "Fallout4.esm", "formID": "0x13794" }
}
```

### Faction Condition

```json
{
  "condition": "IsInFaction",
  "Form": { "pluginName": "Fallout4.esm", "formID": "0x1C21C" }
}
```

### Random Condition

```json
{ "condition": "Random", "threshold": 0.3 }
```

30% chance to pass on each evaluation.

### Comparison Conditions

```json
{
  "condition": "Level",
  "comparison": 2,
  "numericValue": { "type": "Static", "value": 20.0 }
}
```

Comparison operators: `0` = Equal, `1` = Not Equal, `2` = Greater, `3` = Greater/Equal, `4` = Less, `5` = Less/Equal.

The `numericValue` can also reference a global variable:

```json
{
  "numericValue": {
    "type": "GlobalVariable",
    "Form": { "pluginName": "Fallout4.esm", "formID": "0x1234" }
  }
}
```

### Logical Grouping

**OR** — any child must pass:

```json
{
  "condition": "OR",
  "conditions": [
    { "condition": "IsSprinting" },
    { "condition": "IsInCombat" }
  ]
}
```

**AND** — all children must pass (useful inside OR blocks):

```json
{
  "condition": "AND",
  "conditions": [
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsInCombat" }
  ]
}
```

### Negation & Disabling

Any condition supports `negated` and `disabled`:

```json
{ "condition": "IsInCombat", "negated": true, "disabled": false }
```

---

## User Overrides

End users can override submod settings without editing the author's `config.json`.  
Create a `user.json` in the same submod folder — it takes precedence for any field it contains.

```json
{
  "priority": 500,
  "disabled": true
}
```

The in-game GUI saves user changes to `user.json` automatically.

---

## In-Game GUI

Press **Shift + O** (scan code `0x18`) to toggle the overlay.

- **Inspect mode** — read-only view of all loaded mods, conditions, and their live evaluation results.
- **User mode** — toggle submods, change priorities, save to `user.json`.
- **Author mode** — full editing: add/remove/negate conditions, save to `config.json`.

The **Animation Log** (View menu) shows replacements as they happen in real time.

---

## INI Settings

`Data/F4SE/Plugins/OpenAnimationReplacer.ini`:

```ini
[General]
bEnabled = true
bEnableUI = true

[UI]
iToggleKey = 0x18    ; O key scan code
bRequireShift = true

[AnimationLog]
bLogReplace = true
iMaxLogEntries = 100

[Debug]
bVerboseLogging = false
```

---

## Example: 1st Person Idle Replacement

This example replaces a first-person idle animation with a custom one when the player has a weapon drawn and is aiming down sights.

FO4's vanilla animation tree uses these paths:

- **1st person**: `Meshes/Actors/Character/_1stPerson/Animations/`
- **3rd person**: `Meshes/Actors/Character/Animations/`

Note the underscore — it's `_1stPerson`, not `1stPerson`.

### Folder Layout

```
Data/Meshes/
└── Actors/
    └── Character/
        └── _1stPerson/
            └── Animations/
                └── OpenAnimationReplacer/
                    └── ADS Idle Pack/
                        ├── config.json
                        ├── ADS Idle/
                        │   ├── config.json
                        │   └── mt_idle.hkx
                        └── Hipfire Idle/
                            ├── config.json
                            └── mt_idle.hkx
```

The `OpenAnimationReplacer` folder goes inside the animations directory you want to target. The replacement `.hkx` mirrors the original's relative path — so `mt_idle.hkx` in the submod root replaces `Actors/Character/_1stPerson/Animations/mt_idle.hkx`.

### Mod `config.json`

```json
{
  "name": "ADS Idle Pack",
  "author": "YourName",
  "description": "Custom 1st-person idle while aiming."
}
```

### SubMod `config.json` (`ADS Idle/config.json`)

```json
{
  "name": "ADS Idle",
  "description": "Replaces 1st-person idle when aiming down sights with weapon drawn.",
  "priority": 2000,
  "interruptible": true,
  "replaceOnLoop": true,
  "conditions": [
    {
      "condition": "IsForm",
      "Form": { "pluginName": "", "formID": "0x14" }
    },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsADS" }
  ]
}
```

**What this does:**

- `IsForm` with formID `0x14` (empty plugin = absolute ID) targets the player character only.
- `IsWeaponDrawn` ensures a weapon is out.
- `IsADS` checks for aiming down sights.
- `interruptible: true` means the replacement swaps in/out immediately as the player enters or exits ADS, rather than waiting for the animation to loop.

### SubMod `config.json` (`Hipfire Idle/config.json`)

```json
{
  "name": "Hipfire Idle",
  "description": "Replaces 1st-person idle when weapon is drawn but not aiming.",
  "priority": 1000,
  "interruptible": true,
  "replaceOnLoop": true,
  "conditions": [
    {
      "condition": "IsForm",
      "Form": { "pluginName": "", "formID": "0x14" }
    },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsADS", "negated": true }
  ]
}
```

The `ADS Idle` submod has higher priority (2000) than `Hipfire Idle` (1000), so when both `IsWeaponDrawn` and `IsADS` are true, the ADS animation wins. When the player lowers their sights, the hipfire animation takes over. This layered approach lets you define multiple submods in the same mod folder with different priorities and conditions.

## Example: 3rd Person Pose Replacement

This example replaces a 3rd-person idle pose when the player is not in combat.

### Folder Layout

```
Data/Meshes/
└── Actors/
    └── Character/
        └── Animations/
            └── OpenAnimationReplacer/
                └── Relaxed Poses/
                    ├── config.json
                    └── Out of Combat/
                        ├── config.json
                        ├── PoseA_Idle1.hkx
                        ├── PoseA_IdleFlavor1.hkx
                        ├── PoseA_IdleFlavor2.hkx
                        └── Player/
                            └── PoseA_Idle1.hkx
```

3rd-person animations are directly under `Actors/Character/Animations/`. Subdirectories like `Player/` are preserved — `Player/PoseA_Idle1.hkx` in the submod replaces `Actors/Character/Animations/Player/PoseA_Idle1.hkx`.

### SubMod `config.json` (`Out of Combat/config.json`)

```json
{
  "name": "Relaxed Out-of-Combat Poses",
  "priority": 500,
  "interruptible": true,
  "conditions": [
    { "condition": "IsInCombat", "negated": true }
  ]
}
```

`negated: true` on `IsInCombat` means this only plays when the actor is **not** in combat. As soon as combat starts, the original animation takes over because `interruptible: true` allows mid-animation switching.

## Example: 1st Person Reload Replacement

This example shows a multi-submod pack that replaces reload animations with different variants depending on game state. Higher-priority submods are evaluated first, so more specific conditions (sprinting) override more general ones (standard reload).

### Folder Layout

```
Data/Meshes/
└── Actors/
    └── Character/
        └── _1stPerson/
            └── Animations/
                └── OpenAnimationReplacer/
                    └── Custom Reload Pack/
                        ├── config.json
                        ├── Sprinting Reload/
                        │   ├── config.json
                        │   └── ReloadA.hkx
                        ├── Combat Quick Reload/
                        │   ├── config.json
                        │   └── ReloadA.hkx
                        └── Standard Reload/
                            ├── config.json
                            └── ReloadA.hkx
```

### Mod `config.json`

```json
{
  "name": "Custom Reload Pack",
  "author": "YourName",
  "description": "Replaces 1st-person reload animations with custom variants."
}
```

### SubMod `config.json` (`Sprinting Reload/config.json`)

```json
{
  "name": "Sprinting Reload",
  "description": "Stylized one-handed reload while sprinting.",
  "priority": 4000,
  "interruptible": false,
  "replaceOnLoop": false,
  "replaceOnEcho": true,
  "conditions": [
    {
      "condition": "IsForm",
      "Form": { "pluginName": "Fallout4.esm", "formID": "0x14" }
    },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsSprinting" }
  ]
}
```

### SubMod `config.json` (`Combat Quick Reload/config.json`)

```json
{
  "name": "Combat Quick Reload",
  "description": "Faster, snappier reload when in combat.",
  "priority": 3000,
  "interruptible": false,
  "replaceOnLoop": false,
  "replaceOnEcho": true,
  "conditions": [
    {
      "condition": "IsForm",
      "Form": { "pluginName": "Fallout4.esm", "formID": "0x14" }
    },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsInCombat" }
  ]
}
```

### SubMod `config.json` (`Standard Reload/config.json`)

```json
{
  "name": "Standard Reload",
  "description": "General-purpose reload replacement when not in combat.",
  "priority": 2000,
  "interruptible": false,
  "replaceOnLoop": false,
  "replaceOnEcho": true,
  "conditions": [
    {
      "condition": "IsForm",
      "Form": { "pluginName": "Fallout4.esm", "formID": "0x14" }
    },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsInCombat", "negated": true }
  ]
}
```

**Priority layering:** Sprinting Reload (4000) is evaluated first — if the player is sprinting, it wins regardless of combat state. Combat Quick Reload (3000) takes over during combat. Standard Reload (2000) is the fallback when neither of the above match. All three use `interruptible: false` because reloads are one-shot animations that shouldn't be cut short mid-clip.

---

## Tips

- **Priority matters.** The first submod whose conditions pass wins. Use higher numbers for more specific overrides.
- **Use `interruptible = true`** for state-dependent animations (combat stance, sprinting) so they switch immediately when conditions change.
- **Leave `interruptible = false`** for one-shot animations (draw weapon, jump) to avoid mid-animation pop.
- **Test with the Animation Log** open — it shows exactly which submod triggered each replacement.
- Conditions marked *(stub)* parse and serialize correctly but always evaluate to `false` until a future update completes their FO4 API integration.
