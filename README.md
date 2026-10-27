# Perfect Dark Agents Mod \[Full Net Support\]

> **Experimental software — not a finished game build.**  
> Crashes, freezes, desyncs, and broken matches are still expected.  
> This is **not** a “playable release” yet. Use it for testing and development only.

A community mod / netplay fork of the [Perfect Dark PC port](https://github.com/perfect-dark-pc-port/perfect_dark) (`port-net`), focused on making **Combat Simulator** work better over the network — including scenarios and challenges.

**License:** [MIT](#license)  
**ROM tested:** **NTSC Final only** (see [Supported ROM](#supported-rom))

---

## Table of contents

1. [Read this first](#read-this-first)
2. [What is this?](#what-is-this)
3. [Supported ROM](#supported-rom)
4. [What’s working (high level)](#what-s-next---goals-)
5. [Network play](#network-play)
6. [Changelog](#changelog)
7. [Running the game](#running-the-game)
8. [Controls](#controls)
9. [Building from source](#building-from-source)
10. [License](#license)
11. [Credits](#credits)

---

## Read this first

- This project is in an **experimental** phase.
- **Crashes are still expected.** Do not treat this like a stable multiplayer game.
- Online play can desync, drop players, or soft-lock a match.
- Only **NTSC Final** (`US Rev 1` / `ntsc-final`) has been tested for this mod.
- You must already own a legitimate Perfect Dark ROM. **This repo does not include ROMs or game assets.**

If you just want a stable single-player / split-screen experience, use the regular [PC port](https://github.com/perfect-dark-pc-port/perfect_dark) instead of this experimental net build.

---

## What is this?

**Perfect Dark Agents Mod \[Full Net Support\]** builds on the early `port-net` netplay work and keeps pushing Combat Sim online further:

- Full Classic Combat Simulator Support
- Fully Suppported Simulants (AI's)
- All Scenarios (Gamemodes) Support
- All Challenges Support
- Quality of life changes
- Additional Scenario Support
- HD Texture Support
- XBLA Texture Support

Under the hood it is still the Perfect Dark decompilation port. The goal of *this* README / mod branding is to make the net-focused work easier to find and understand.

---

## Supported ROM

| Region | Status for this mod |
| - | - |
| **NTSC Final** (`US V1.1` / `US Rev 1`) | **Tested — use this** |
| NTSC 1.0 | Not tested here |
| PAL Final | Not tested here |
| JPN Final | Not tested here |

**Recommended ROM**

- Name on disk: `pd.ntsc-final.z64`
- MD5: `e03b088b6ac9e0080440efed07c1e40f`
- Boot screen: `NTSC version 8.7 final`

Everyone in a net game must use the **same** ROM. Mixing regions will break things.

---

## What’s Next? (Goals)

- Add support for players to host and play with out port forwarding
- Maintain full support for Port Forwarding
- Co-op Support
- Continue QOL Changes
- Continue Bug/Crash Support
- Additional Scenario's 
- Ability to add or remove hills, terminal locations, gun locations, etc.

## Lofty Goals? (Thoughts but not something I expect to do)

- Map Editor (The ability to create our own maps)
*I have yet to check if there is something public like this. But if there isnt I think this would be cool*
- Scripting language to create scenarios/gamemodes
- Scripting language to create basic game modfications
- External Server Support

## Things I dont plan to do!

- Anti Cheat
*I have no intrest in making a anti cheat for this at thiss time. I have no knowledge in this realm and have no intrest in learning. But if others want to create them. By all means I am here to support you!*

---

## Network play

More technical detail lives in [`docs/netplay.md`](docs/netplay.md). Short version:

### Hosting

1. Forward UDP/TCP port **27100** (or change it in `pd.ini`), or use a VPN like ZeroTier / Hamachi.
2. In-game: **Network Game → Host Game**, set player cap, start.
3. Set up Combat Sim (scenario, challenge, options). Wait for friends to join.
4. Start the match when ready.
5. After the match, you’ll return to setup so people can join again.

### Joining

1. Set up your Combat Sim player profile first (you use the local player-1 style config).
2. **Network Game → Join Game**, enter the host address (and port if not `27100`).
3. Wait for the host. Chat with `` ` `` / `~` if available; `ESC` disconnects.
4. The match starts when the host starts it.

### Tips

- Same ROM filename / region for everyone.
- `` ` `` / `~` opens the console; `F9` shows network stats.
- For larger lobbies, hosts may want `Net.Server.UpdateFrames = 2` in `pd.ini` to reduce bandwidth.

---

## HD texture packs

This build can load **N64-style** HD packs compatible with [Perfect Dark Plus HD Textures](https://github.com/retro-foundry/Perfect-Dark-Plus-HD-Textures) (same layout as Dab’s Mod). Packs are **local visuals only** — each player picks their own; nothing is synced over the network.

### Install (Phase 1 — N64 packs)

The game creates a `texture-packs` folder next to the executable (portable) or under the save directory on first launch.

1. Drop a pack into `texture-packs/` as either:
   - a **`.zip`** (recommended — keeps the folder tidy), e.g. `PD.Forever.Plus.HD.v0.10.zip`, or
   - an **extracted folder**, e.g. `PD Forever Plus HD/` with `0001.png`, …
2. In-game: **Extended Options → Video → Texture Pack** and choose the pack (or **Default** for built-in ROM textures).
3. Choice is saved in `pd.ini` as `Mod.TexturePack=` (empty / missing = Default).

Zips are unpacked once into `cache/texture-packs/<name>/` on first select (or when the zip changes). That can take a minute for large packs like Forever Plus (~300MB). A Perfect Dark–themed loading screen appears while unpacking; warm starts with a valid cache skip it.

If the folder is empty, the pack is missing, or a texture has no replacement file, the game always falls back to **ROM textures** — never blank materials.

**Recommended N64 pack:** [PD Forever Plus HD](https://github.com/retro-foundry/Perfect-Dark-Plus-HD-Textures). Credits: Retro Foundry / Perfect Dark Plus.

### Xbox 360 (XBLA) package + Ultimate / XBLA Plus packs

Meshes, rooms, and HD record textures from the official XBLA release are supported (ported from [Dab’s Mod](https://github.com/DabDavis/perfect-dark-dabs-mod)).

1. Create / use **`added-content/`** next to the executable (or under the save directory). Drop the Perfect Dark XBLA **`.7z`** (or its unpacked package / STFS) there. Legacy `xbla/` is still searched.
2. First use unpacks once into **`cache/xbla/`** (heavy; shows a loading screen).
3. **Extended Options → Video:** enable **XBLA Meshes**, **XBLA Textures**, and optionally **XBLA Rooms**. F6 toggles the whole release (`Mod.XblaMeshKey`).
4. Select an **Ultimate Plus / XBLA Plus** pack under **Texture Pack**. Pack folders `pd-xbla/` / `xbla/` replace package art by record; numbered PNGs still override ROM textures.

`pd.ini` keys: `Mod.XblaMeshes`, `Mod.XblaTextures`, `Mod.XblaStages` (and related Dab keys the loaders register).

#### Netplay: host-authoritative hitboxes

Combat Sim damage is already decided on the **host**. XBLA mesh triangle hits run only **offline or on `NETMODE_SERVER`**. Clients draw their own meshes/packs but never use local XBLA geometry for shot resolution — `SVC_CHR_DAMAGE` from the host wins.

If the host stays on Default (N64) geometry and a client draws larger XBLA models (or the reverse), aim / silhouettes can disagree visually. That is intentional: hits follow the host’s loaded geometry, not each viewer’s pack.

---


## Changelog

### Experimental v0.0.2

>Still **experimental**. Matches can crash or desync. This is a milestone tag, not a release quality claim.

**Features Added**
- Added HD Texture Support
*You can download textures at https://github.com/retro-foundry/Perfect-Dark-Plus-HD-Textures/releases*

*My texture support code was refrenced from https://github.com/DabDavis/perfect-dark-dabs-mod*

**Bug Fixes**
- Hills not improperly detecting players
- Timer text wrong color when teamate owns hill


### Experimental v0.0.1

#### Network Play/Online Multiplayer Mode

>Still **experimental**. Matches can crash or desync. This is a milestone tag, not a release quality claim.

- Added ability allow other players to edit changes to the match
- Added ability for playerse to see the current settings of the match
- Added Challenges that count towards stars
- Added proper explosion support
- Added proper laptop gun support
- Added every scenario!
- Added support for props on glass should fall to the next solid object below when glass is broken
- Glass is now properly synced between all players
- Bullet Holes are now properly synced between all players
- Paintball bullets are now properly synced between all players
- Added support to play with simulants
- Added all proper animations for all players
- Added a waiting screen for when a player ends the match instead of just having the game end when a player leaves

**King of the Hill**
- Added a new hill to to "Grid" map
- Added ability to change how long it takes to claim hill
- Added timer to screen showing if the hill is being contested/is being claimed
- Added sound for when some one enters the hill contesting the hill

**Hacker Central**
- Added ability to make the terminal move after a successful hack
- Added ability to change how long it takes to hack

**Hold the Briefcase**
- Added a timer for all players that shows how long the player who is holding the case has left
- Added ability to change how long you have to hold the case before getting a point

**Pop a Cap**
- Added a time for all players that shows how long the player has left to get a point
- Added the ability to change how long the player has to be the cap to score a point

---

## Running the game

You need your own **NTSC Final** ROM.

Assuming an `x86_64` Windows/Linux build:

1. Create a `data` folder next to the executable (if it isn’t there).
2. Put the ROM in as `data/pd.ntsc-final.z64`.
3. Run `pd.x86_64` (or `pd.x86_64.exe` on Windows).

Optional: put a Perfect Dark GBC ROM named `pd.gbc` in `data` to emulate Transfer Pak cheat unlocks.

On Linux/macOS you can also keep data under the usual user data paths (`~/.local/share/perfectdark` / `~/Library/Application Support/perfectdark`).

A GPU with **OpenGL 3.0 / ES 3.0+** is required.

> Nintendo Switch builds of upstream `port-net` do not currently support this net work. Focus for Agents Mod testing is **PC + NTSC Final**.

---

## Controls

1964GEPD-style and Xbox-style bindings are implemented. Mouse controls player 1 only. Rebind in `pd.ini`.

| Action | Keyboard / mouse | Xbox pad | N64 pad |
| - | - | - | - |
| Fire / Accept | LMB / Space | RT | Z Trigger |
| Aim mode | RMB / Z | LT | R Trigger |
| Use / Cancel | E | — | B |
| Use / Accept | — | A | A |
| Half-Crouch | Shift | — | Extra |
| Full-Crouch | Control | — | Extra |
| Crouch cycle | — | L3 | Extra |
| Reload | R | X | X |
| Previous weapon | Wheel up | B | D-Left |
| Next weapon | Wheel down | Y | Y |
| Radial menu | Q | LB | D-Down |
| Alt fire mode | F | RB | L Trigger |

---

## Building from source

### Windows (MSYS2)

1. Install [MSYS2](https://www.msys2.org).
2. Open **MINGW64** (x86_64) or **MINGW32** (i686) — not the plain MSYS prompt.
3. Install deps:  
   `pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-SDL2 mingw-w64-x86_64-zlib mingw-w64-x86_64-cmake mingw-w64-x86_64-python3 mingw-w64-i686-toolchain mingw-w64-i686-SDL2 mingw-w64-i686-zlib mingw-w64-i686-cmake mingw-w64-i686-python3 make git`
4. Clone this repo (with submodules) and `cd` into it.
5. `cmake -G"Unix Makefiles" -Bbuild .`
6. `cmake --build build -j4 -- -O`
7. Binary: `build/pd.x86_64.exe`

### Linux

1. Need gcc/g++ 10+, make, cmake, git, python3, SDL2 2.0.12+, libGL, zlib.
2. `cmake -G"Unix Makefiles" -Bbuild .`
3. `cmake --build build -j4`
4. Binary: `build/pd.x86_64` (or your arch)

### macOS / Switch

Upstream port build steps still apply for those platforms, but **this mod’s net work is only being tested on PC with NTSC Final.** Prefer Windows/Linux when trying online features.

### Notes

- Do not expect Visual Studio project support.
- Alternate toolchains: pass `-DCMAKE_TOOLCHAIN_FILE=...` as usual.
- Do not commit or redistribute ROMs. See `.gitignore`.

---

## License

This project is released under the **MIT License**. See [`LICENSE`](LICENSE).

You still need your own legally obtained Perfect Dark ROM to run the game. The ROM and Nintendo’s game content are **not** covered by this license and are **not** included in this repository.

---

## Credits

- Original [Perfect Dark decompilation](https://github.com/n64decomp/perfect_dark) authors
- Ryan Dwyer and the [perfect-dark-pc-port](https://github.com/perfect-dark-pc-port/perfect_dark) team (`port` / `port-net`)
- doomhack’s earlier public porting effort
- [sm64-port](https://github.com/sm64-port/sm64-port), Ship of Harkinian / Fast3D contributors, lieff/minimp3
- Mouse Injector & 1964GEPD authors for FPS / mouselook work
- Raf (64-bit), NicNamSam (icon), and everyone who filed issues, PRs, and test reports
- Agents Mod netplay testers and contributors working on Combat Sim online
- [Retro Foundry](https://github.com/retro-foundry) / Perfect Dark Plus HD texture packs
- DabDavis / perfect-dark-dabs-mod (texture-pack loader reference)

---

*Perfect Dark Agents Mod \[Full Net Support\] — Experimental v0.0.1*
