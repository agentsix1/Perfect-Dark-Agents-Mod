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
4. [What’s working (high level)](#whats-working-high-level)
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

- Host–client network matches (Combat Simulator)
- Scenario modes over the network (not just plain Combat)
- Challenge hosting / completion tracking improvements
- Lots of crash, desync, and “host thinks you left the hill” style fixes

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

## What’s working (high level)

### From the base PC port

- Single-player and local split-screen Combat Sim
- Mouse look, widescreen, FOV options, 60 FPS (and experimental uncapped tickrate)
- Controller support, remappable controls, basic mod loading
- Transfer Pak cheat unlock emulation (optional GBC ROM)

### Added / improved for this net mod

- Network Combat Simulator (host + join)
- Scenario modes online (King of the Hill, Hacker Central, Hold the Briefcase, Pop a Cap, Capture the Case, and more)
- Challenge matches that can be hosted online
- Better sync for pickups, objectives, timers, and lobby settings
- Many crash / freeze / “phantom item” fixes while playing online

**Still experimental:** expect bugs. Sims, some weapons, and edge cases are incomplete. See the [changelog](#changelog) for what landed in `v0.0.1`.

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

## Changelog

### Experimental v0.0.1

First Agents Mod changelog versus the early upstream `port-net` baseline. Written for players, not engineers.

#### Big picture

- Turned “barely working Combat over the net” into a broader **Combat Sim net** attempt: scenarios, challenges, lobby settings, and a pile of stability fixes.
- Still **experimental**. Matches can crash or desync. This is a milestone tag, not a release quality claim.

#### Multiplayer / online feel

- Host runs the match; clients follow the host’s world (fewer “I shot you / no you didn’t” surprises for objectives).
- Lobby settings (teams, scenario options, etc.) are shared so clients can see what the host set up — even when only the host can edit.
- Challenges can be hosted online; finishing one can count toward your challenge stars (1–4 players → 1st–4th star), and that progress is saved.
- Kill feed / match feedback and some explosion effects (like laptop blow-ups) show up for everyone more reliably.

#### Scenario modes (the fun game types)

- **King of the Hill** — hills light and claim more reliably (including maps that used to show a radar blip but no real hill). Brief network lag is less likely to reset your hill timer while you’re still standing in it.
- **Hacker Central** — terminals / uplink pickup sync better; fewer phantom uplinks, empty inventories, or host crashes when grabbing the device. Optional **Mobile Computer** (terminal can move after a successful hack). Mid-hack lag is less likely to instantly break the download.
- **Hold the Briefcase** — briefcase ownership and the hold timer behave better online; timer display is clearer (including when you’re not the carrier). Case/weapons are less likely to float on broken glass.
- **Pop a Cap** — “who is the target” is fairer and more stable for the host (less stuck / flashing host-as-cap nonsense).
- **Capture the Case** and other non-Combat setups — fewer “everything broke after challenges” style pad/desync failures.

#### Stability & “it just stopped working” fixes

- Lots of host/client crash and freeze paths cleaned up (bad prop deletes, dangling models, corrupted scenario memory, etc.).
- Leaving a match / host disconnect is less likely to leave the menu shaking forever.
- Glass breaks drop resting props properly instead of leaving gear hanging in the air.
- Team-colored menu title bars when teams are on (easier to tell who’s who in the lobby UI).

#### What we are *not* claiming yet

- A polished 8-player experience
- Perfect gun / rocket / cloak sync
- Simulants (bots) in net matches
- Support beyond **NTSC Final**
- “Ready for public matchmaking”

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

---

*Perfect Dark Agents Mod \[Full Net Support\] — Experimental v0.0.1*
