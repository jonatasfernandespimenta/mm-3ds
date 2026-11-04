# 2 Ship 2 Harkinian — Nintendo 3DS

An experimental port of [2 Ship 2 Harkinian](https://github.com/HarbourMasters/2ship2harkinian) (2S2H, the
Majora's Mask PC port) to the **New Nintendo 3DS**. It runs the 2S2H 5.0.1 code on the PICA200 renderer and
3DS audio stack from [soh-3ds](https://github.com/999sian/soh-3ds), and turns the bottom screen into a
touch-driven pause menu.

This is a work in progress, not a finished release. Expect slowdowns in busy areas and the occasional crash.

## Features

- Full game on the top screen, with N64-style rendering through Citro3D.
- Audio on a dedicated core (core 2 on New 3DS), so slow frames don't cut the sound.
- **Bottom screen**, always visible while you play:
  - **Items / Map / Quest Status / Masks** pages, the same as the pause menu. Use the arrows to switch pages.
  - Drag an item (or tap the item, then the target) onto the **C buttons** or the four **D-pad slots** to
    equip it. Tap a D-pad slot to use that item.
  - **Map**: the dungeon map for the current dungeon.
  - **Quest Status**: tap a song to hear it played.
  - **Save**: quick save and quick load, with a separate file for each game slot. Loading does not delete the
    quick save (unlike the owl statue save). The page shows whether the save or load worked.
  - **Fast-forward**: during dialogue and cutscenes, hold the button on the bottom screen to speed them up.
    It advances finished text boxes for you, but never answers a choice.

## Requirements

- A **New** Nintendo 3DS, New 3DS XL or New 2DS XL. The CIA requests the 178 MB memory mode, which the
  original 3DS/2DS doesn't have.
- Custom firmware (Luma3DS) with FBI. See [3ds.hacks.guide](https://3ds.hacks.guide).
- For sound: a DSP dump at `sdmc:/3ds/dspfirm.cdc`. Run the **DSP1** homebrew once to create it.
- Your own Majora's Mask ROM, to generate the game data on a PC (see below). **No ROM or game data is
  included in this repository.**

## Game data

The 3DS doesn't extract the ROM. Generate the archives on a PC first:

1. Install desktop [2 Ship 2 Harkinian **5.0.1**](https://github.com/HarbourMasters/2ship2harkinian/releases)
   and start it once with your Majora's Mask ROM. It creates `mm.o2r` (next to the executable), and the release
   ships `2ship.o2r` alongside it.
2. Copy **both** files to the SD card at `sdmc:/3ds/2ship/`:

   ```
   sdmc:/3ds/2ship/mm.o2r
   sdmc:/3ds/2ship/2ship.o2r
   ```

The archive versions must match the 2S2H version this port is built from (5.0.1). If a file is missing, the
game says so on the top screen. Copy the files, then press A to try again.

## Install

Before installing, make sure you have everything from [Requirements](#requirements): the DSP dump for sound,
and `mm.o2r` + `2ship.o2r` in `sdmc:/3ds/2ship/` (see [Game data](#game-data)). The CIA contains only the
port's code. No ROM or game data is included.

### Option A: QR code (no PC needed)

<img src=".github/qr-install.png" alt="QR code for the latest mm_3ds.cia" width="200">

1. Connect the 3DS to Wi-Fi.
2. Open **FBI** and choose **Remote Install → Scan QR Code**.
3. Point the outer camera at the QR code above, then confirm the install.
4. Start **2 Ship 2 Harkinian** from the HOME menu.

The QR code always points to the latest release:
`https://github.com/jonatasfernandespimenta/mm-3ds/releases/latest/download/mm_3ds.cia`.
To update later, scan it again.

### Option B: download the CIA manually

1. Download `mm_3ds.cia` from the [latest release](https://github.com/jonatasfernandespimenta/mm-3ds/releases/latest).
2. Copy it to the SD card, for example to `sdmc:/cias/`.
3. Open **FBI** and choose **SD → cias → mm_3ds.cia → Install CIA**.
4. Start it from the HOME menu. You can then delete the `.cia` file from the SD card.

It has its own title ID, so it installs alongside Ship of Harkinian 3DS without replacing it. Updating keeps
your save files, because they live in `sdmc:/3ds/2ship/`.

### Option C: build it yourself

See [Building](#building). The build writes `build-3ds-mk/mm_3ds.cia`. Install it the same way as Option B.

**Over Wi-Fi.** Start ftpd on the 3DS, then run:

```sh
env 3DS_IP=<3ds-ip> 3DS_PORT=5000 python3 -I scripts/push-cia-ftp.py build-3ds-mk/mm_3ds.cia
```

The CIA lands in `/cias`. Install it with FBI.

## Controls

The Circle Pad moves Link and the C-stick controls the camera. The button layout follows the 2S2H controller
defaults and can be remapped in the 2S2H menu. The bottom screen handles item equipping, maps, songs, quick
save and fast-forward (see [Features](#features)).

## Troubleshooting

- **No sound:** `sdmc:/3ds/dspfirm.cdc` is missing. Run DSP1 once.
- **"Copy mm.o2r and 2ship.o2r" screen:** a file is missing from `sdmc:/3ds/2ship/`, or it has the wrong
  name.
- **Crash:** with logging enabled, the game writes `lastfatal.txt` and `bootinfo.txt` to `sdmc:/3ds/2ship/`.
  Attach them when you report a bug.
- **The HOME menu icon didn't change after an update:** the HOME menu caches icons by title version.
  Uninstall the old CIA and install the new one.

## Building

Requires devkitPro (devkitARM, libctru, citro3d, makerom), CMake 3.20 or newer, and Python 3. On macOS, run the
scripts with devkitPro's bash (`/opt/devkitpro/pacman/bin/bash`).

```sh
export DEVKITPRO=/opt/devkitpro
./scripts/build-3ds-deps.sh   # once: builds SDL2 and other portlibs into devkitPro
./scripts/build-mm3ds.sh      # libultraship (3DS) + mm_3ds → build-3ds-mk/mm_3ds.3dsx and mm_3ds.cia
```

Set `MM_BUILD_JOBS` to limit parallel jobs.

The HOME menu icon and banner come from `platform/3ds/cia/`. If `platform/3ds/cia/mm/icon.icn` and
`banner.bnr` exist, they are used instead. That folder is git-ignored, because MM artwork belongs to
Nintendo and isn't redistributed.

**Host tests** (portable logic only, no 3DS needed):

```sh
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```

## Source layout

| Path | Purpose |
| --- | --- |
| `cmake/MM3DS.cmake` | The `mm_3ds` target: MM decomp + 2S2H layer for 3DS, CIA packaging |
| `src/setup/mm_prebuilt_3ds.cpp` | Checks for `mm.o2r` / `2ship.o2r` on the SD card |
| `third_party/2ship/` | 2S2H source snapshot, with the 3DS changes (`mm/2s2h/Enhancements/DualScreen3DS/` is the bottom screen) |
| `third_party/libultraship/` | libultraship with the 3DS backend |
| `platform/3ds/` | PICA200 renderer, audio, input and platform support (from soh-3ds) |
| `src/pica/`, `src/compat3ds/` | Combiner logic and 3DS compatibility code |
| `scripts/` | Build, packaging and FTP tools |
| `tests/` | Host regression tests (inherited from soh-3ds) |
| `patches/` | Port patches and technical notes |

## Credits

- [HarbourMasters](https://github.com/HarbourMasters) for 2 Ship 2 Harkinian and libultraship, and the
  [zeldaret/mm](https://github.com/zeldaret/mm) decompilation team.
- [999sian/soh-3ds](https://github.com/999sian/soh-3ds) for the 3DS renderer, audio and platform layer this port
  builds on.
- 3DS port of 2S2H: Jonatas Pimenta.

Licenses and provenance of each dependency stay with their sources (see `third_party/` and
`platform/3ds/PROVENANCE.md`). The Legend of Zelda: Majora's Mask is © Nintendo. This project isn't affiliated
with Nintendo.
