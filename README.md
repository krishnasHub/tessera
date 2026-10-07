# Tessera

A small C++ framework for top-down HD-2D games in Unreal Engine 5: pixel-art sprites standing in a lit 3D world,
built at runtime from data. No Blueprints, no assets: every game-specific value (file names, map symbols, material
paths, command-line names, wording) comes from the game's data or config.

| Module | What |
|---|---|
| `TesseraCore` | `UTSData` (JSON game data, units, tile map, spawns, regions), `TSLook` (3D / HD-2D / flat 2D looks, sprite and pixel materials), `TSAssets`, `TSJson`, `TSCmd` / `TSConfig` |
| `TesseraWorld` | `ATSWorldBuilder` (base for a game's world builder: mesh helpers, height field, cutaways, flickering lights, runtime navmesh), `ATSSky` (sun, moon, fog, grade, day/night clock, night visibility) |
| `TesseraGameplay` | `ATSCharacter` (base for every character: stats, tags, poise, knockback, death, weapon kits, sprite, hooks for the game's rules), `UTSStatsComponent` (data-defined pools and formulas), `TSCombat` (damage pipeline), `UTSAbilityComponent` (12 built-in ability types + `RegisterType`), `ATSProjectile`, `ATSFX`, `UTSInventoryComponent` / `TSLoot` / `ATSPickup`, `UTSFeedback` (floating text, toasts, shake), `UTSSpriteComponent`, `UTSPoseMesh`, anim-notify hooks |

Story and dialogue live in a separate plugin, [Loom](https://github.com/krishnasHub/loom); Tessera doesn't depend on it.

## Add it to a game

```
git submodule add https://github.com/krishnasHub/tessera.git <Project>/Plugins/Tessera
```

Enable it in the `.uproject` (`{ "Name": "Tessera", "Enabled": true }`), add `"TesseraCore"` / `"TesseraWorld"` / `"TesseraGameplay"` to the
game module's dependencies, and tell Tessera about the game in `Config/DefaultGame.ini`:

```ini
[Tessera]
DataFile=Data/game.json     ; relative to Content (stage the folder as loose files when packaging)
WorldSection=world          ; the data section with the world / look / sky settings
CommandPrefix=TS            ; command-line switches: -TSLook=hd2d, -TSHour=22, -TSSun=..., -TSFog=0
```

## Use it

```cpp
// Game mode, before anything spawns:
TSLook::Init(this);
Builder = GetWorld()->SpawnActor<AMyWorldBuilder>();
Builder->Build();

// The game's builder:
void AMyWorldBuilder::Build()
{
	const UTSData& D = UTSData::Get(this);
	BeginBuild();
	// terrain: fill Heights / GridW / GridH / Step / OriginX / OriginY, build the Terrain mesh
	// buildings: Collect = &Cutaways.AddDefaulted_GetRef().Full; AddBox(...); ... (cut away while they hide the hero)
	// lamps: AddFlicker(Light, Intensity); ATSSky::AddNightLight(X, Y, Radius);
	FinishBuild(FBox(FVector(0, 0, -1500), FVector(D.MapW * D.TileSize, D.MapH * D.TileSize, 3000)));
}
```

## Data it reads

```jsonc
"<world>": {
  "assets": { "sprite", "pixelWorld", "glow", "flash", "telegraph", "hitEffect", "smoke", "coin", "kitPart" },  // paths, or material names for the game's resolver
  "text": { "block": "BLOCK", "notEnough": "Not enough {pool}", ... },   // overrides for on-screen words
  "currencyKey": "currency",                      // the loot-table key for coin ranges
  "kits": { ... }, "mounts": { ... },             // weapon kits: shapes on bones (ATSCharacter::SetWeaponKits)
  "arrows": { "heightPerDistance": 0.16, "minHeight": 30, "maxHeight": 320 },
  "unitsPerPx": 3.5, "tileSize": 300,            // data "pixels" -> Unreal units (UTSData::Px)
  "look": "hd2d",                                 // mesh3d | hd2d | flat2d
  "camera": { "topdown": { "pitch": -58, "yaw": -90 } },
  "looks2d": {
    "spriteUnits": { "hd2d": 6.5, "flat2d": 9.375 }, "hd2dCamera": { "pitch": -40 }, "pixelSize": 200,
    "textureFolder": "/Game/.../Pixel", "pixelMaterials": { "<material name>": "<texture>" }
  },
  "sun": { "pitch": -36, "yaw": 125, "intensityLux": 9 },
  "dayNight": { "enabled": true, "startHour": 14, "secondsPerHour": 30, "moonLux": 0.6,
                "nightExposure": -2.2, "exposureMinEV": 2, "exposureMaxEV": 5,
                "fog": { "enabled": true, "day": 0.012, "dusk": 0.03, "night": 0.05 },
                "nightVision": { "heroSight": 1000, "strength": 0.97, "lightReach": 1 } }
},
"tuning": { ... },                                // free-form numbers: UTSData::Tuning(key)
"stats": {                                        // pools and damage rules (UTSStatsComponent); a string names a tuning number
  "health": "hp",
  "pools": { "hp":     { "max": { "flat": "hpFlat", "per": { "vitality": 10 } }, "round": true },
             "energy": { "max": { "base": 80 }, "regen": { "base": 30 }, "regenDelay": 0.5 } },
  "crit": { "flat": "critPct" }, "critMultiplier": 1.5, "armorStat": "armor", "armorConstant": 100, "variance": 0.1,
  "scaling": { "strength": 0.06 }, "staggerTime": 0.55, "poiseRegenDelay": 2
},
"abilities": { "<id>": { "type": "projectile" | "aoe" | ... | <registered>, "<pool>": cost, "cooldown": 1, ... } },
"items": { ... }, "rarities": { ... }, "affixes": [ ... ], "lootTables": { ... },
"map": {
  "rows": [ "########", "#..P..g#", ... ],        // one character per tile
  "spawns": { "P": "player", "g": "enemy:guard" },// marker -> "kind:id"
  "floor": ".", "solid": "#",                     // a marker tile becomes the floor to its left unless that's solid
  "regions": [ { "id": "north", "maxRow": 13 }, { "id": "south", "maxRow": 999 } ]
}
```
