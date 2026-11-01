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
2. **Palette → gray:** turn each of the 256 palette entries into a gray level with the **paper mapping** chosen in Phase 0. It works on CIE L*, and darkens strongly saturated colours far in hue from the parchment, so coloured text stays readable. Plain luma fails, see Phase 0. A 256-entry LUT is rebuilt only when the palette changes. The parameters (k, chroma window, L* black/white points) are settings, and the reference implementation is `tools/eink-preview.py`.
   - Follow-up: colours that carry meaning (HP gems, stat colours) need distinct grays or a pattern, from a small override table keyed by palette index. Also tune the mini-map contrast.
3. **Optional ordered dither** (4×4 Bayer) to 16 levels. The Carta panel shows 16 grays; the first-person view gains the most from dithering. Toggle it in settings.
4. **Integer 2× scale** into the InkView framebuffer at a fixed origin (portrait), with nearest-neighbour scaling for sharp pixels.
5. **Refresh policy**, driven by dirty rectangles from `copyRectToScreen`:
   - measured on the device (Phase 1): `PartialUpdate` takes about 225 ms on the game area, while `DynamicUpdate` and `FullUpdate` take about 430 ms. So Partial is the default and Dynamic isn't used;
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
- [x] Add the `scummvm` submodule, pinned to a release tag. It's shallow, at **`v2026.4.0`** (`7d2709da`); ScummVM now numbers releases by date.
- [x] Build with only the Xeen engine: `tools/build-host.sh null` (configure flag `--disable-all-engines --enable-engine=xeen`, which builds `mm` with the `xeen` subengine and leaves out `mm1`). It builds cleanly with g++ 15.
- [x] Detection: `mm:worldofxeen` ("Might and Magic: World of Xeen (DOS/English)") and `mm:cloudsofxeen` ("Might and Magic IV: Clouds of Xeen (DOS/English)").
- [x] Headless boot (null backend, 15 s): loads `mm.dat` from `--extrapath=scummvm/dists/engine-data`, sets a 320×200 mode, no crash. **Peak RSS ≈ 25 MB** (debug build), well under 512 MB.
- [x] Toolchain finding for Phase 1: this ScummVM needs only **C++11** and rejects only GCC ≤ 4.7, so the SDK's GCC 6 should be enough.
- [x] Play into the first town with the SDL build (`tools/build-host.sh sdl`, then `tools/play-host.sh`). Done 2026-11-01: created a party, walked through Vertigo, talked to the mayor and a tavern patron, fought a slime, rested, and saved and quit cleanly. 34 screenshots are in `~/games/xeen/screenshots`.
- [x] Grayscale preview with `tools/eink-preview.py` (16 levels, 4×4 Bayer dither, plus a 758×1024 portrait mock-up). **Gate: passed, but only with the "paper" mapping.**
  - **Plain luma fails.** Xeen's green/blue/yellow value text has the same luma as the tan parchment behind it: parchment `(214,154,123)` is luma 168 and green text `(107,239,82)` is luma 181. Stats, HP/SP, gold, food and "Good" disappear, and the combat slime nearly vanishes.
  - **The paper mapping works.** For each palette colour, gray = CIE L* − k · smoothstep(chroma 45…75) · (a*b* distance from the parchment), with L* 12…72 stretched to black…white and k = 0.9. Coloured text goes black on near-white parchment, the slime becomes a solid black shape, and sky, stone, portraits and dialogue keep their tone. This becomes the backend's palette LUT (§3).
  - **Known losses**, carried into Phase 2/3:
    - colour-coded meaning disappears: the party HP gems green/yellow/red, and stat colours (green normal, blue zero) all become black;
    - the mini-map is low-contrast in paper mode;
    - pink plaster walls become flat white.
- [x] Peak RSS while playing about 10 minutes with the SDL build at 3× scale: **≈ 117 MB** (`~/games/xeen/rss.log`), against 25 MB for the headless boot. The difference is mostly SDL and the GPU driver; the InkView backend should land near the headless figure. Fits in 512 MB either way.

**Phase 0 complete (2026-11-01).**

### Phase 1 — Toolchain smoke test (≤1 day)
- [x] Cross-compile a C++ test program with InkView using the `SDK-B288` toolchain (GCC 6.3.0) and run it on the device. This is `device/smoke/` (`make -C device/smoke install`). Ran 2026-11-01; log in `applications/mmiv-smoke/smoke.log`.
  - **Binary:** `-std=c++11 -static-libstdc++ -static-libgcc -pthread -linkview` with the vendor softfp flags. It uses the soft-float loader `/lib/ld-linux.so.3`, and its only shared libraries are libinkview, libm, libpthread and libc. Its highest glibc symbol is 2.17 (the device has 2.23). Size 704 KB, stripped.
  - **Runtime on the device: all PASS.** Static constructors, exceptions, RTTI, containers, 4 threads with atomics and a mutex, chrono and `sleep_for`.
  - **Worker-thread handoff works.** 40/40 frames went from the worker thread through a `SetHardTimer` poll (20 ms) on the main thread to the screen. This is the ScummVM pattern.
  - **Screen:** `ScreenWidth/Height` = 758×1024, orientation 0 (portrait) by default. Touch coordinates are in the same space, so no rotation is needed.
  - **Drawing:** 2× nearest-neighbour scaling in C plus a 1:1 `Stretch(IMAGE_GRAY8)` takes 7–13 ms per 640×400 frame.
  - **Refresh on the 640×400 game area,** including `WaitForUpdateComplete`: **Partial avg 225 ms** (worst 569), Dynamic avg 426 ms, Full avg 428 ms; Partial on the full screen took 594 ms. Dynamic gives no speed-up on this firmware, so it's dropped.
  - Tap → frame handed to the panel controller: **29–30 ms** in every mode. During interactive use `WaitForUpdateComplete` returns before the panel settles, so the real tap-to-visible time is about **0.25 s** with Partial.
  - **Lifecycle:** Home sends `EVT_BACKGROUND`, then the firmware sends `EVT_EXIT` about 10 s later without the app's own quit path running. **The backend must autosave and stop the engine thread on `EVT_BACKGROUND`/`EVT_EXIT`.**
- [x] Find which C++ standard the pinned ScummVM tag needs and confirm GCC 6 can build it. `v2026.4.0` needs **C++11** and rejects only GCC ≤ 4.7 (see Phase 0). Confirmed on the device that GCC 6.3 C++11 binaries run. Actually cross-building ScummVM is Phase 2's first task.

**Phase 1 complete (2026-11-01).**

### Phase 2 — Backend on the host stub (3–5 days)
- [x] **Cross-build ScummVM with GCC 6** (`tools/build-device.sh null`, then `pocketbook`): `v2026.4.0` with Xeen compiles with **zero warnings** using the SDK's GCC 6.3. The null-backend binary is 10.5 MB stripped. No compiler workaround needed.
- [x] `backend/pocketbook/`, linked into `scummvm/backends/platform/pocketbook` by `tools/link-backend.sh`, which also applies `patches/0001-configure-pocketbook-backend.patch` (one `configure` case). Done 2026-11-01:
  - `pocketbook.cpp`: `OSystem` with default timers, events and saves, the POSIX filesystem, the null mixer (no audio), `mm.dat` from the app folder, and `scummvm.ini` in the app folder. Touches become move + click at game (or overlay) coordinates, and keys become key events. Xeen reads raw keys, not keymapper actions.
  - `eink-graphics.cpp`: a 320×200 CLUT8 game screen plus a 640×400 RGB565 overlay (ScummVM's menus at native 2×). It composes the gray frame through the palette table and **diffs it against the last frame it sent**, so only real changes reach the panel. The cursor isn't drawn, since input is touch.
  - `gray-map.cpp`: the Phase 0 paper mapping in C++, as a 256 × 16-cell table with the Bayer dither precomputed.
  - `refresh-policy.cpp`: when and how to refresh (see below).
  - `platform-host.cpp`: the headless host stub with scripted input (`wait`, `tap`, `tapg`, `key`, `shot`, `quit`), PGM shots and a log of every frame and every panel decision.
  - `platform-inkview.cpp`: InkView owns the main thread and ScummVM runs on an engine thread, with a locked frame and input queue, as in Phase 1. Temporary keys: PREV/NEXT = forward/back, MENU/BACK = Esc.
- [x] **Refresh policy, from the host-stub run** (`refresh-policy.h`). Refreshing on every engine frame would flash constantly: loading a save fades in over ~30 near-full frames, and idle sprites change a few pixels every frame. The policy:
  - waits until the frame has been **unchanged for 80 ms**, but never more than 400 ms;
  - refreshes changes **under 2% at most once a second**;
  - does a **full refresh at ≥ 85%** of the frame (screen switches) or once partial updates add up to 400% of the frame. Turning and stepping (50–60%) stay partial, with no flash.
  - Result on the smoke script: **71 engine frames → 12–13 panel updates** (4 full).
- [x] Host-stub test `tests/host/smoke-from-save.txt`: load save slot 0, step, turn, tap the first portrait (opens the character sheet, stats readable), Esc. Shots are checked against coarse 40×25 block signatures in `tests/host/golden/` (`tools/check-golden.py`). Only the numbers are committed, never game images. Repeat runs match 98.9–100%, and a swapped-screen check fails at 30.6%, as it should. It needs the local slot-0 save in Vertigo; a new-game script that needs no save is still to do.
- [x] Unit tests (`make -C tests/unit`, **55 passing**): the paper mapping gives the same levels as the Python reference for all sampled Xeen colours; also dither levels, the palette table, RGB565, the changed-area box, and every refresh-policy rule (settle, max wait, small-change throttle, full threshold, ghost budget, forced full). `tests/run.sh` runs everything.
- [x] **Device build with our backend**: `tools/build-device.sh pocketbook` gives **11.2 MB** stripped with zero warnings. It uses the soft-float loader, links only libinkview, libpthread, libm and libc, and its highest glibc symbol is 2.4.

**Phase 2 complete (2026-11-01).** Nothing has run on the reader yet; that's Phase 3.

### Phase 3 — On the device (1–2 weeks)
- [x] Device bundle + launcher (`applications/mmiv.app` → `mmiv/scummvm`, `mm.dat`, game data, saves; `tools/make-bundle.sh --install`), home-screen icon (`tools/install-icons.py`, absolute app path in `view.json`).
- [x] InkView display with the refresh policy from §3 (`refresh-policy.h`), tuned on the panel. Landscape 3x is the default. The firmware's status panel must be off (`SetPanelType(PANEL_DISABLED)`): with it on, the picture moved ~79 px and our black never showed. The landscape layouts keep the panel in portrait and rotate frame, updates and touches themselves (`layout.h`).
- [x] Touch play on the game itself (landscape) or the 36-button keypad (portrait, `mmiv/layout`); MENU = Esc, hold MENU = ScummVM menu; text prompts open the reader's keyboard, keys paced for Xeen's 5-event queue.
- [x] Saves in `applications/mmiv/saves`; suspend save to slot 99 on `EVT_BACKGROUND`, a safety save every 2 min, the launcher resumes the newest save. Xeen patch 0004: no saves while signing in at an inn (map 0).
- [x] Logging to `applications/mmiv/scummvm.log`; session logs in `device/results/`.

**Phase 3 complete (2026-11-01).** Played on the PB628: naming characters, saving, resuming, centred landscape picture.

### Phase 4 — Package & polish (≤1 week)
- [x] Release zip without game data: `tools/make-release.sh VERSION` → `dist/mmiv-VERSION-pocketbook.zip`. Install = copy over USB. v1.0.0 published on GitHub.
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
1. Phase 4: settings, battery check.
2. Phase 5: offer the backend upstream to ScummVM.
