# Condition Reference

All conditions available in Open Animation Replacer for Fallout 4. Use these in SubMod JSON configs under `"conditions": [...]`.

---

## JSON Structure

Every condition uses this envelope:

```json
{
  "condition": "ConditionName",
  "negated": false,
  "disabled": false,
  ...additional parameters...
}
```

- `negated` — inverts the result (NOT)
- `disabled` — always passes (effectively commented out)

---

## Comparison Conditions

Conditions that compare a value use these shared parameters:

```json
{
  "comparison": 0,
  "numericValue": { "type": "Static", "value": 10.0 }
}
```

**Comparison operators:** `0`=Equal, `1`=NotEqual, `2`=Greater, `3`=GreaterEqual, `4`=Less, `5`=LessEqual

**Numeric value types:**
- `"Static"` — fixed float: `{ "type": "Static", "value": 100.0 }`
- `"GlobalVariable"` — reads a TESGlobal: `{ "type": "GlobalVariable", "Form": { "pluginName": "Fallout4.esm", "formID": "0x123" } }`
- `"ActorValue"` — reads an actor value: `{ "type": "ActorValue", "actorValue": "Health" }`

---

## Form Conditions

Conditions that reference a specific form use:

```json
{
  "Form": { "pluginName": "Fallout4.esm", "formID": "0x1A4D7" }
}
```

---

## Logic / Composite Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **OR** | Any child condition passes (OR logic) | `"conditions": [...]` |
| **AND** | All child conditions pass (AND logic) | `"conditions": [...]` |
| **XOR** | Exactly one child condition passes | `"conditions": [...]` |
| **TARGET** | Evaluates children against the actor's combat target | `"conditions": [...]` |
| **PLAYER** | Evaluates children against the player character | `"conditions": [...]` |

---

## Identity Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **IsForm** | Reference matches a specific form | Form |
| **IsActorBase** | Actor's NPC_ base record matches | Form |
| **IsRace** | Actor's race matches | Form |
| **IsFemale** | Actor is female | (none) |
| **IsChild** | Actor is a child | (none) |
| **IsUnique** | NPC has the Unique flag | (none) |
| **IsPlayerTeammate** | Actor is a companion/follower | (none) |
| **IsGuard** | NPC is flagged as a guard | (none) |
| **IsGhost** | Actor has the Ghost flag (invulnerable) | (none) |
| **IsSummoned** | Actor was summoned/conjured | (none) |

---

## State / Activity Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **IsWeaponDrawn** | Weapon is raised and ready | (none) |
| **IsInCombat** | Actively in combat | (none) |
| **IsCombatState** | Finer combat state (0=None, 1=Combat, 2=Searching) | Comparison + Numeric |
| **IsSprinting** | Actor is sprinting | (none) |
| **IsRunning** | Actor is running (not walking/sprinting) | (none) |
| **IsInAir** | Actor is airborne (jumping/falling) | (none) |
| **IsSneaking** | Actor is sneaking | (none) |
| **IsSwimming** | Actor is swimming | (none) |
| **IsInPowerArmor** | In power armor | (none) |
| **IsADS** | Aiming down sights | (none) |
| **IsAttacking** | In any attack state (melee or gun) | (none) |
| **IsReloading** | Reloading weapon (gunState == 4) | (none) |
| **IsFiring** | Firing ranged weapon (gunState 7/8) | (none) |
| **IsBlocking** | Actively blocking | (none) |
| **IsTalking** | Currently in conversation | (none) |
| **IsOverEncumbered** | Carry weight exceeds maximum | (none) |
| **IsTrespassing** | Actor is trespassing | (none) |
| **IsGreetingPlayer** | NPC is greeting the player | (none) |
| **IsDoingFavor** | Performing a command for the player | (none) |

---

## Equipment Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **IsEquipped** | Specific weapon/item form is equipped | Form |
| **IsEquippedType** | Equipped weapon matches a type keyword | Form (keyword) |
| **IsEquippedHasKeyword** | Equipped weapon/armor has keyword | Keyword (editor ID or form) |
| **IsWorn** | Specific item is equipped/worn | Form |
| **IsWornHasKeyword** | Any equipped item has keyword | Keyword |
| **CurrentMagazineAmmo** | Ammo in magazine (0=empty) | Comparison + Numeric |
| **EquippedObjectWeight** | Weight of equipped weapon | Comparison + Numeric |

---

## Stats / Value Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **Level** | Actor's current level | Comparison + Numeric |
| **CompareActorValue** | Any actor value (Health, AP, etc.) | `actorValueName` + Comparison + Numeric |
| **Scale** | Visual scale multiplier (1.0=normal) | Comparison + Numeric |
| **Height** | Actor height/bounding box size | Comparison + Numeric |
| **Weight** | Total equipped weight | Comparison + Numeric |
| **MovementSpeed** | Speed in game units/second | Comparison + Numeric |
| **InventoryCount** | Count of specific item in inventory | Form + Comparison + Numeric |
| **InventoryWeight** | Total inventory weight | Comparison + Numeric |
| **SubmergeLevel** | Water submersion (0.0 surface, 1.0 fully under) | Comparison + Numeric |
| **FactionRank** | Rank in a faction (-1=not member) | Form + Comparison + Numeric |
| **CrimeGold** | Bounty with a faction | Form + Comparison + Numeric |

---

## Location / World Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **IsInInterior** | In an interior cell | (none) |
| **IsWorldSpace** | In a specific worldspace | Form |
| **IsParentCell** | In a specific cell | Form |
| **IsInLocation** | In a specific location (includes parent hierarchy) | Form |
| **LocationHasKeyword** | Current location has keyword | Form (keyword) |
| **LocationCleared** | Location marked as cleared | (none) |
| **CurrentGameTime** | Hour of day (0.0-23.99) | Comparison + Numeric |
| **CurrentWeather** | *[STUB]* Current weather form | Form |

---

## AI / Behavior Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **LifeState** | Life state (0=Alive, 1=Dying, 2=Dead, ...) | Comparison + Numeric |
| **SitSleepState** | Sit/sleep state (0=Normal, 3=Sitting, 6=Sleeping) | Comparison + Numeric |
| **IsMovementDirection** | Direction relative to facing (0=Fwd, 1=Right, 2=Back, 3=Left) | Comparison + Numeric |
| **HasTarget** | Has a combat target | (none) |
| **CurrentTargetDistance** | Distance to combat target (game units) | Comparison + Numeric |
| **CurrentTargetAngle** | Angle to combat target (degrees, 0=facing) | Comparison + Numeric |
| **IsCurrentPackage** | Running a specific AI package | Form |
| **IsInScene** | Participating in a scene | (none) |
| **CurrentFurniture** | Using specific furniture | Form |
| **IsPlayingIdleAnimation** | Playing a specific TESIdleForm | Form |
| **IdleTime** | Seconds since last action | Comparison + Numeric |

---

## Keyword / Form Check Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **HasKeyword** | Actor/base has keyword | Keyword (editor ID or form) |
| **IsInFaction** | Actor belongs to faction | Form |
| **HasPerk** | Actor has perk | Form |
| **HasSpell** | Actor has spell/ability | Form |
| **HasMagicEffect** | Active magic effect on actor | Form |
| **HasGraphVariable** | Havok graph variable check | `variableName`, `variableType`, value |
| **IsReplacerEnabled** | Another OAR replacer/submod is active | `subModName` |
| **IsQuestStageDone** | Quest stage completed | Form + `stage` (int) |

---

## Random / Probability

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **Random** | Random chance per evaluation | `"threshold": 0.5` (0.0–1.0) |

---

## Animation Timing Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **AnimTimeRemaining** | Seconds remaining in current clip | Comparison + Numeric |
| **AnimTimeElapsed** | Seconds elapsed since clip started | Comparison + Numeric |
| **AnimProgress** | Progress ratio (0.0 start, 1.0 end) | Comparison + Numeric |
| **FallDistance** | Fall distance in game units | Comparison + Numeric |

---

## Physical / Environment Conditions (Stubs)

These are registered but return stub results — the underlying game APIs are not yet available in CommonLibF4:

| Condition | Notes |
|-----------|-------|
| **LightLevel** | Ambient light at actor position |
| **SurfaceMaterial** | Ground material type |
| **MovementSurfaceAngle** | Slope angle |
| **IsOnStairs** | Stairs detection |
| **WindSpeed** | Current wind speed |
| **WindAngleDifference** | Wind vs actor facing angle |

---

## Detection Conditions

These require stealth/detection system access. Use `DetectedBy` or `Detects` as parent conditions with child conditions for filtering.

| Condition | Description | Parameters |
|-----------|-------------|------------|
| **DetectedBy** | True if any qualifying actor detects this actor | `"conditions": [...]` (child filter) |
| **Detects** | True if this actor detects any qualifying actor | `"conditions": [...]` (child filter) |
| **DetectionDistance** | Distance between detector and target (child-only) | Comparison + Numeric |
| **DetectionRelationship** | Relationship rank between NPCs (child-only) | Comparison + Numeric |
| **DetectionAngle** | Angle from one actor to another (child-only) | `swapActors`, `limitRight`, `limitLeft` + Comparison + Numeric |

### Detection Example

```json
{
  "condition": "DetectedBy",
  "conditions": [
    { "condition": "DetectionDistance", "comparison": 4, "numericValue": { "type": "Static", "value": 500.0 } },
    { "condition": "DetectionAngle", "comparison": 4, "numericValue": { "type": "Static", "value": 90.0 }, "swapActors": false, "limitRight": false, "limitLeft": false }
  ]
}
```

---

## Dialogue Condition

Detects dialogue state phases with edge detection for transient states.

| Parameter | Type | Description |
|-----------|------|-------------|
| `dialogueActive` | bool | Menu currently open |
| `dialogueStarted` | bool | Edge: menu just opened this frame |
| `playerChoosing` | bool | Player is selecting a response |
| `playerChose` | bool | Edge: player just made a choice |
| `dialogueEnded` | bool | Edge: menu just closed this frame |

Enable (set `true`) only the sub-checks you need. Condition passes when ALL enabled checks match.

```json
{ "condition": "Dialogue", "dialogueActive": true, "dialogueStarted": false, "playerChoosing": false, "playerChose": false, "dialogueEnded": false }
```

---

## Math Statement Condition

Evaluates arbitrary math expressions with variable bindings.

| Parameter | Type | Description |
|-----------|------|-------------|
| `expression` | string | Math expression (supports `+`, `-`, `*`, `/`, `>`, `<`, `>=`, `<=`, `==`, `!=`, `and`, `or`, `not`, parentheses) |
| `variables` | array | Named variables bound to NumericComponent values |

Result is `true` when the expression evaluates to non-zero.

```json
{
  "condition": "MathStatement",
  "expression": "health > 50 and ammo < 10",
  "variables": [
    { "name": "health", "numericValue": { "type": "ActorValue", "actorValue": "Health" } },
    { "name": "ammo", "numericValue": { "type": "Static", "value": 30.0 } }
  ]
}
```
