# Open Animation Replacer — Quick Start Guide

## Installation

1. Copy `OpenAnimationReplacer.dll` and `OpenAnimationReplacer.ini` into  
   `Fallout 4/Data/F4SE/Plugins/`.
2. Launch the game through F4SE.

---

## Creating a Replacement Mod

### The one rule

**Mirror the path after `Animations\`.** Open the Animation Log, play the anim,
then put that same relative path under your SubMod.

| Game plays | Your file goes here |
|------------|---------------------|
| `...\Animations\SCAR\WPNReload.hkx` | `<SubMod>\SCAR\WPNReload.hkx` |
| `...\Animations\MT_Idle.hkx` | `<SubMod>\MT_Idle.hkx` |

Weapon anims usually live in a weapon folder (`SCAR\`, `44Pistol\`, …). Idle-style
anims often sit directly under `Animations\` (no subfolder).

### Folder layout

Put an `OpenAnimationReplacer` folder inside the animation tree you target
(1st person: `...\Character\_1stPerson\Animations\`, 3rd: `...\Character\Animations\`).

```
Data/Meshes/Actors/Character/Animations/
└── OpenAnimationReplacer/
    └── MyAnimPack/                 ← Replacer Mod (any name)
        ├── config.json             ← optional pack metadata
        └── Combat Idle/            ← SubMod (any name)
            ├── config.json         ← conditions + priority
            └── mt_idle.hkx         ← mirrors ...\Animations\mt_idle.hkx
```

- Priority lives in the SubMod’s `config.json`, not the folder name.
- Conditions decide *when*; the path decides *which* animation.

### Matching (short version)

1. **Default:** exact path match after `Animations\` (`bDirectPathMatching=1`).
2. **Fallback:** if OAR can’t resolve the real path, match by filename only
   (`wpnreload`). Conditions + priority break ties.
3. **Legacy:** `bDirectPathMatching=0` uses filename matching for everything.

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
| `suppressAnnotations` | bool or array | — | Mute annotations from the replacement file: `true` = all, or a list of names, e.g. `["WeaponFire"]` (case-insensitive). Great for dry-fire / silent animations whose source `.hkx` still carries annotations |
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

Press **F2** (default) to toggle the overlay. Rebind it in **Settings → Activation Key**, or set `iToggleKey` / `bRequireShift` in the INI.

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
; Match replacements against the clip's resolved on-disk animation path
; (default). Set to false to restore legacy leaf-name matching everywhere.
bDirectPathMatching = true

[UI]
iToggleKey = 0x3C    ; F2 (DIK scan code). Rebind in-game via Settings.
bRequireShift = false

[AnimationLog]
bLogReplace = true
iMaxLogEntries = 100

[Debug]
bVerboseLogging = false
```

---

## Example: 1st Person Idle (ADS vs hipfire)

Paths: 1st person uses `_1stPerson` (with underscore).  
`mt_idle.hkx` in the SubMod root replaces `...\Animations\mt_idle.hkx`.

```
...\Character\_1stPerson\Animations\OpenAnimationReplacer\
└── ADS Idle Pack/
    ├── config.json
    ├── ADS Idle/
    │   ├── config.json          ← priority 2000, IsADS
    │   └── mt_idle.hkx
    └── Hipfire Idle/
        ├── config.json          ← priority 1000, IsADS negated
        └── mt_idle.hkx
```

**ADS Idle** `config.json`:

```json
{
  "name": "ADS Idle",
  "priority": 2000,
  "interruptible": true,
  "replaceOnLoop": true,
  "conditions": [
    { "condition": "IsForm", "Form": { "pluginName": "", "formID": "0x14" } },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsADS" }
  ]
}
```

**Hipfire Idle** — same, but `"priority": 1000` and `"IsADS"` with `"negated": true`.

ADS (2000) wins while aiming; hipfire takes over when sights drop.

## Example: 3rd Person Pose (out of combat)

Subfolders under `Animations\` are preserved: `Player\PoseA_Idle1.hkx` →
`<SubMod>\Player\PoseA_Idle1.hkx`.

```
...\Character\Animations\OpenAnimationReplacer\
└── Relaxed Poses/
    └── Out of Combat/
        ├── config.json
        ├── PoseA_Idle1.hkx
        └── Player/
            └── PoseA_Idle1.hkx
```

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

## Example: 1st Person Reload (layered priorities)

Game path example: `...\Animations\SCAR\WPNReload.hkx` → each SubMod has
`SCAR\WPNReload.hkx`. Highest priority that passes wins.

```
...\Character\_1stPerson\Animations\OpenAnimationReplacer\
└── Custom Reload Pack/
    ├── Sprinting Reload/     ← priority 4000, IsSprinting
    │   └── SCAR/WPNReload.hkx
    ├── Combat Quick Reload/  ← priority 3000, IsInCombat
    │   └── SCAR/WPNReload.hkx
    └── Standard Reload/      ← priority 2000, not in combat
        └── SCAR/WPNReload.hkx
```

Example SubMod (`Sprinting Reload/config.json`):

```json
{
  "name": "Sprinting Reload",
  "priority": 4000,
  "interruptible": false,
  "replaceOnLoop": false,
  "replaceOnEcho": true,
  "conditions": [
    { "condition": "IsForm", "Form": { "pluginName": "Fallout4.esm", "formID": "0x14" } },
    { "condition": "IsWeaponDrawn" },
    { "condition": "IsSprinting" }
  ]
}
```

Combat Quick Reload: priority `3000` + `IsInCombat`.  
Standard Reload: priority `2000` + `IsInCombat` negated.  
Keep `interruptible: false` on one-shot reloads.

---

## Tips

- Higher **priority** wins when several SubMods match. Put the most specific rules highest.
- `interruptible: true` for stance/sprint-style swaps; `false` for one-shots (reload, draw).
- Use the **Animation Log** to find the real path and confirm which SubMod won.
- Replacement not applying? Compare your SubMod folders to the log path, or try
  `bDirectPathMatching=0` while debugging.
- Conditions marked *(stub)* always evaluate false until their FO4 APIs are finished.
