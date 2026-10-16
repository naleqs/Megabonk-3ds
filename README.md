# Megabonk 3ds
Probably like 75% vibe coded. 

An unofficial Nintendo 3DS port of Megabonk, written from scratch in C on libctru
(citro3d for 3D, citro2d for the UI). **This repo contains no game files.** It
contains the port's code and a builder that converts the assets from **your own
copy of Megabonk** (Steam) into a 3DS homebrew app.

## Building

You need Python 3.9+, an internet connection, and Megabonk installed through Steam.

- **Windows:** double-click `build.bat`
- **Linux / macOS:** `./build.sh`
- or directly: `python build_megabonk3ds.py [--game-dir "path/to/Megabonk"]`

The builder:
1. finds Megabonk through Steam (or uses `--game-dir`, the folder that contains `Megabonk_Data`)
2. creates a temporary environment with a Python venv and the devkitARM / libctru /
   citro3d toolchain (downloaded from devkitPro, about 100 MB)
3. converts the models, animations, textures, audio and game data
4. compiles `output/3ds/megabonk3ds/megabonk3ds.3dsx`
5. deletes the temporary environment (`--keep-env` keeps it, for debugging)

It takes a while (baking 21 animated characters is the slow part). Nothing is installed system-wide.

## Installing on a 3DS

Copy the `output/3ds` folder to the root of the SD card (so the app is at
`sdmc:/3ds/megabonk3ds/megabonk3ds.3dsx`) and start it from the Homebrew Launcher.
Sound needs `sdmc:/3ds/dspfirm.cdc` (run DSP1 once if you don't have it).

The `.3dsx` contains assets converted from your copy of the game: keep it for yourself
and don't upload or share it.

## Controls

| Input | Action |
|---|---|
| Circle Pad | Move |
| A / B | Jump (hold to glide with Float characters) |
| X | Interact (chests, shrines, Shady Guy, ...) |
| L / R, C-stick, D-pad left/right | Rotate camera |
| D-pad up/down | Camera pitch |
| START | Pause |
| Touch / D-pad + A | Menus |
| Y | Reroll level-up choices |

## What's in it

- Forest, Desert and Graveyard stages with their props, lighting and enemy timelines from the game data
  (a new enemy type joins every minute once its spawn time has passed; minibosses and swarms on the
  game's schedule; the Final Swarm's ghosts turn black, purple and red the longer you stay)
- All 21 characters with their animations, starting weapons and passive abilities
- All 30 weapons. Base damage, knockback, crit chance, projectile count, bounces, cooldown and the
  level-up stat lines are read from the game's own weapon data
- 22 tomes, including Shield, Thorns, Bloody, Knockback, Golden, Duration, Cursed and Chaos
- Chests, pots, the six shrine types, Shady Guy's shop, the microwave, the boss portal and 82 items
  with the game's own rarities
- Enemy special attacks: Cactus Shooter projectiles plus every miniboss and boss attack (slams, root and
  stone lines, predictive spikes, pillar circles, lasers), with timings, ranges and cooldowns from the game data
- The game's music, sound effects, font, icons and portraits

Item, tome and passive numbers that aren't stored in the game's data files (they live in compiled
code) follow the [megabonk.org database](https://megabonk.org/database/) where it lists them, and are
estimated otherwise. Not in the port yet: Wizard's Hat, Beacon and Pumpkin (they need tome level caps
or later stages), Monke's wall climb, the Scythe's charged attack, saves and unlocks.

[中文说明](README.zh-CN.md)

## Layout

```
build_megabonk3ds.py   the builder (start here)
source/                game code (C, libctru/citro3d)
shaders/               PICA200 vertex shader
tools/                 asset conversion + toolchain setup + compile step (run by the builder)
```

Megabonk is made by vedinad. This project is not affiliated with or endorsed by its developer.
