# mmIV — Clouds of Xeen on the PocketBook Touch Lux 5

Goal: play **Might and Magic IV: Clouds of Xeen** on the PocketBook Touch Lux 5 (PB628), using the game's original DOS data files and ScummVM's existing Xeen engine. This repo does **not** recreate the game's content. It adds an e-ink platform backend so ScummVM runs on the reader.

## 1. Findings

| Question | Answer | Source |
|---|---|---|
| Does an engine already run Clouds of Xeen? | Yes. ScummVM `mm` engine, game id `mm:cloudsofxeen`, rated **Good** (completable, minor issues). It needs the original DOS data (`XEEN.CC`, …). | [ScummVM compatibility](https://www.scummvm.org/es/compatibility/DEV/mm:cloudsofxeen/) |
| Where do the data files come from? | A legally owned copy, e.g. GOG's *Might and Magic 4-5* / World of Xeen. Data files are never committed here. | — |
| Target device | PocketBook Touch Lux 5 (PB628): 6" E Ink Carta, **758×1024**, dual-core 1 GHz, **512 MB RAM**, 8 GB + microSD, Wi-Fi. No Bluetooth listed, so plan for **touch + hardware buttons only**. | `~/Repos/pockectbook_lux_5`, retailer specs |
| Firmware / ABI | `U628.6.9.988`: **soft-float** loader, **glibc 2.23** only. Hard-float (`gnueabihf`) dynamic binaries will not load. | `inkquest` `specs/tech-stack.md` (FCT-14) |
| Toolchain | `pocketbook/SDK_6.3.0` branch `6.5`, tree `SDK-B288`, `arm-obreey-linux-gnueabi-` (GCC 6), **already installed** at `~/opt/pocketbook-sdk/SDK_6.3.0-6.5/SDK-B288`. The compiler needs `LD_LIBRARY_PATH=<sdk>/usr/lib`. Flags: `-fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp`. Sysroot has freetype and libpng. | `inkquest` Makefiles, ADR-0004 |
| Display / input API | **InkView** (`inkview.h`, `-linkview`): `InkViewMain(handler)`, `FullUpdate()`, `PartialUpdate(x,y,w,h)`, `SoftUpdate()`, `IV_KEY_*` keys, touch via pointer events. Use only `IV_KEY_*`: the Linux `KEY_*`/`BTN_*` names compile but have different values. The device has no Menu key. | `inkquest` `client/platform/pocketbook/pocketbook.c`, `probe.c` |
| Install / launch | Copy a bundle into `applications/` on the device; the firmware lists `*.app` launcher entries. | `inkquest` `inkquest.app` |
| Existing ScummVM e-ink port? | **None found** for PocketBook, Kobo or Kindle. Precedent exists for games on PocketBook (Doom 2, IF interpreters). | [MobileRead](https://www.mobileread.com/forums/showthread.php?p=4547741), [Doom on PB360](https://blog.the-ebook-reader.com/2011/04/07/video-of-doom-2-running-on-pocketbook-360-plus-e-ink-ereader/) |

**Why it fits:** Xeen is turn-based and only redraws in response to input, which suits e-ink well. 320×200 at **2× in portrait = 640×400**, which leaves about 620 px under the game for a touch keypad (see §4).

## 2. Architecture

```
mmIV/
├── PLAN.md
├── scummvm/                    git submodule, pinned to a ScummVM release tag
├── backend/pocketbook/         our OSystem backend (copied/symlinked into
│   ├── pocketbook.cpp          scummvm/backends/platform/pocketbook at build time)
│   ├── graphics-inkview.cpp    8-bit surface → grayscale → InkView, refresh policy
│   ├── events-inkview.cpp      touch/keys → Common::Event, on-screen keypad
│   ├── keypad-layouts.h        Xeen command strip + movement pad
│   └── module.mk
├── packaging/
│   ├── scummvm-xeen.app        launcher script for applications/
│   └── scummvm.ini             preset: one game, no audio, auto-launch
├── tools/                      host scripts: build, deploy over USB, grab logs
└── Makefile                    host build, device build, bundle zip
```

- **Use a custom `OSystem`, not SDL.** The PocketBook SDK has no SDL port, and InkView owns the main loop (`InkViewMain`). ScummVM's own engines run their own loop and call `OSystem::pollEvent()`/`updateScreen()`. So run the engine on a **worker thread** and let InkView's handler thread feed a lock-protected event queue. This is the same split inkquest uses for jobs.
- **Build only the needed engine:** configure with all engines disabled except `mm` (Xeen subengine), and disable audio, MIDI, cloud, networking, TTS and the OpenGL renderer. This keeps the binary and RAM small.
- **Link statically** against libstdc++ and the other dependencies. Link InkView and glibc dynamically, as inkquest does, to match the device's soft-float glibc 2.23.
- **Host build:** the same backend compiled with a stub display (writes PGM frames and reads scripted input). It runs Xeen on Ubuntu without the device, the same way inkquest uses `platform/host`.

## 3. Display pipeline

1. ScummVM gives an 8-bit paletted 320×200 game surface (plus the mouse cursor).
2. **Palette → gray:** turn each of the 256 palette entries into an 8-bit luma value through a contrast curve (gamma + black/white points). A 256-entry LUT is rebuilt only when the palette changes.
3. **Optional ordered dither** (4×4 Bayer) to 16 levels. The Carta panel shows 16 grays; the first-person view gains the most from dithering. Toggle it in settings.
4. **Integer 2× scale** into the InkView framebuffer at a fixed origin (portrait), with nearest-neighbour scaling for sharp pixels.
5. **Refresh policy**, driven by dirty rectangles from `copyRectToScreen`:
   - small changes (text, stats, cursor) → `PartialUpdate` on the merged dirty box;
   - first-person view changes (turning, stepping) → `PartialUpdate` on the view window;
   - every N partial updates, or on a screen change (combat start, menus) → `FullUpdate()` to clear ghosting. Start with N = 8 and make it configurable;
   - coalesce frames: update the panel only when the engine has been idle about 50–100 ms, so animations (water, sprites) don't flash the panel continuously. Animation is cut back on purpose.
6. **Cursor:** drawn into the framebuffer only when needed. The touch UI mostly doesn't need one.

## 4. Input design (touch + buttons, no keyboard)

Portrait layout: game at the top (640×400, centred), control area below.

- **Tap on the game area** → left click at the mapped 320×200 coordinate. Xeen's UI is mostly clickable buttons.
- **Movement pad:** forward / back / turn left / turn right / strafe → arrow keys.
- **Command strip** (Xeen hotkeys): Attack, Bash, Cast, Dismiss, Info, Protect, Quick-ref, Rest, Search, View-quests, Automap, Block, Fight, Run, Use. Also party members 1–6, Esc and Enter.
- **Text input** (naming characters, riddles/passwords): a pop-up QWERTY keyboard drawn by the backend. InkView has a built-in keyboard (`OpenKeyboard`), so try that first.
- **Hardware buttons** (`IV_KEY_PREV`/`NEXT`/`HOME`/`BACK`): next/prev = move forward/back or turn, back = Esc, long-press home = ScummVM menu (save/load/quit).
- Long-press on the game area = right click, if the engine needs one.

Layouts live in data (`keypad-layouts.h`) so they can be adjusted without touching event code.

## 5. Phases

### Phase 0 — Prove it on the PC (1–2 days)
- [x] Get the game data (GOG) and keep it outside the repo (`~/games/xeen`). Done 2026-11-01 with `tools/prepare-gog-data.sh`. Findings:
  - The installer is `setup_might_and_magic_4-5_-_world_of_xeen_1.0_(28066).exe` (Inno Setup 5.6.2), unpacked with `innoextract`.
  - The game data is `__support/save/XEEN.CC` and `__support/save/DARK.CC`, plus `INTRO.CC` (intro movie, optional).
  - ScummVM detection (5000-byte MD5 + size) matches the reference **English World of Xeen** entry, which ScummVM treats as stable: `xeen.cc 0cffbab5… 13435646`, `dark.cc df194483… 11217676`. `xeen.cc` alone is detected as `cloudsofxeen`.
  - `GAME1.GOG`/`GAME2.GOG` + `.INS` are raw CD images (MODE1/2352) with cue sheets; their CD-audio music is in `MUSIC/xeenNN.ogg`. They aren't needed, because the device build has no audio.
  - ScummVM also needs its own engine data file **`mm.dat`** (`dists/engine-data/`), which must go in the device bundle.
  - Data folders: `~/games/xeen/scummvm/world-of-xeen` (recommended; Clouds is playable inside it) and `~/games/xeen/scummvm/clouds-of-xeen`.
- [ ] Add the `scummvm` submodule, pinned to a release tag; build stock ScummVM on Ubuntu with only the `mm` engine; play into the first town.
- [ ] Grayscale preview: run with a debug filter (or capture screenshots and convert them) at 16 levels, with and without dither. **Gate: the UI and automap must stay readable.** If not, tune the contrast curve before continuing.
- [ ] Measure peak RSS while playing; it must fit well under 512 MB (expected: tens of MB).

### Phase 1 — Toolchain smoke test (≤1 day)
- [ ] Cross-compile a C++ hello-world with InkView using the `SDK-B288` toolchain and run it on the device. This checks that C++ (libstdc++ static, `-std=c++11`/`c++14`) works, since inkquest only proved C.
- [ ] Find which C++ standard the pinned ScummVM tag needs and confirm GCC 6 can build it. If it can't, pin an older ScummVM tag that still has the Xeen engine, or build a newer GCC that targets the same soft-float glibc 2.23 sysroot.

### Phase 2 — Backend on the host stub (3–5 days)
- [ ] `backends/platform/pocketbook` with `OSystem` (timer, mutexes, filesystem via the POSIX FS node, saves), plus a graphics manager and event source.
- [ ] Host stub build: PGM frame dumps + scripted input. Golden-frame test: boot to the title screen, start a new game, step forward 3 times.
- [ ] The palette LUT, dither and dirty-rect merge work and are unit-tested.

### Phase 3 — On the device (1–2 weeks)
- [ ] InkView display: the game is visible, with the refresh policy from §3.
- [ ] Touch keypad + hardware buttons (§4); worker-thread engine with an event queue.
- [ ] Save/load to `/mnt/ext1/applications/scummvm-xeen/saves`; config in the same folder.
- [ ] Handle the app losing focus and sleep/wake: pause the engine on `EVT_HIDE`/`EVT_FOREGROUND` and redraw fully on return.
- [ ] Logging to a file on the device for debugging (pull it over USB).

### Phase 4 — Package & polish (≤1 week)
- [ ] `make pocketbook` → `dist/applications/scummvm-xeen/` + `.app` launcher + zip. Install = copy over USB. Optionally add an "install ScummVM-Xeen" action to the PocketBook Lux 5 Studio app.
- [ ] Settings screen: refresh interval N, dither on/off, contrast, left/right-handed keypad.
- [ ] Battery sanity check: an hour of play, compared against reading.

### Phase 5 — Optional
- [ ] Clouds + Darkside (World of Xeen) if the user owns Darkside of Xeen too; the engine supports the combined game.
- [ ] Generalise the keypad so other ScummVM games work, then offer the backend upstream to ScummVM.

## 6. Risks

| Risk | Mitigation |
|---|---|
| GCC 6 is too old for current ScummVM | Phase 1 checks this first; fall back to an older ScummVM tag or a custom GCC on the same sysroot. |
| Game is unreadable in grayscale | Phase 0 gate; contrast curve + dither; optional per-palette tweaks. |
| Ghosting / flashing gets tiring | Coalesce frames, tune N, partial updates limited to the view window. |
| InkView main loop vs ScummVM loop | Engine on a worker thread with a locked event queue (pattern proven in inkquest). |
| Typing riddle answers by touch | InkView `OpenKeyboard`, or the backend's own pop-up keyboard. |
| Firmware update changes the ABI | Static C++ runtime; re-run the Phase 1 smoke test after updates. |

## 7. Legal / licensing
- ScummVM is GPL; this repo's backend is GPL-compatible (GPLv3+) to match.
- Game data is copyrighted and comes from the player's own purchase. It is **never** committed or distributed here; `.gitignore` excludes it.

## 8. Next steps
1. Run Phase 0 with your GOG copy.
2. Phase 1 C++ smoke test on the PB628, reusing inkquest's SDK path and flags.
