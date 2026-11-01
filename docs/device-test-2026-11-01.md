# Device test with pbtk — 2026-11-01

A full pass over mmIV on the PocketBook Touch Lux 5 (PB628, firmware
U628.6.9.988), driven over Wi-Fi with
[pbtk](https://github.com/jvanheerikhuize/pocketbook_lux_5), plus the PC test
suite. This file has the findings and the plan that follows from them;
`PLAN.md` §8 points here.

## How it was tested

- **PC**: `tools/build-host.sh pocketbook && tests/run.sh`.
- **Reader**: build `21:00:41` (main after #1), landscape 3x by default, then
  portrait. A small pbtk script did one step at a time:
  1. stop if the player had touched the reader since the last step (pbtk `events`);
  2. inject a tap, swipe or hardware key;
  3. poll `/dev/fb0` until the panel changed (time = latency);
  4. compare the app canvas with the panel after it settled (missed or stale
     refreshes show up as differences).
- Before starting, the saves, `scummvm.ini` and the log were copied off the
  reader and checked against `sum`. No new save slots were written.
- Screenshots stayed on the PC (no game imagery in the repo). The reader's log
  for the session is `device/results/2026-11-01-pbtk-device-test.log`.

Latency numbers are upper bounds: one whole-panel read over Wi-Fi takes a few
hundred ms, so they include up to ~0.4 s of polling.

## Results

### PC suite: all pass

| Part | Result |
|---|---|
| Unit tests | 864 passed |
| Launcher, icon installer | pass |
| Golden tests: smoke-from-save (portrait, landscape), touch-only landscape, keypad + suspend | every shot 100% of blocks match |
| Suspend save + resume, rolling safety save, keyboard text input, delete a save by touch, key burst | pass |

### Reader: what works

| Area | Checked | Panel latency |
|---|---|---|
| Title menu | Credits, Other Options, Load a Saved Game; Back returns | 0.5–1.2 s |
| Restore dialog | select slot, page down by tapping the scrollbar track, restore slot 99 | 1.6–2.0 s |
| Movement, landscape | on-screen turn left/right (turning back gives a pixel-identical view); Next = forward, Prev = back | 0.5–1.3 s |
| Game screens | character sheet, Quick Reference, automap, info; Back and short MENU close them | 0.5–1.1 s |
| ScummVM menu | hold MENU opens it; Save dialog; Resume | 1.0–1.8 s |
| Text input | an empty save slot opens the firmware keyboard (landscape, "Answer" field); Back cancels and nothing is saved | 1.6 s / 0.7 s |
| Portrait | `mmiv/layout` = portrait on a fresh start: 2x frame plus the 36-button keypad; Info, Esc, Turn R/L | 0.4–0.7 s |
| Canvas vs panel | identical after settling on every static screen. Only short fades (title) and idle animation differ, and they catch up | — |
| pbtk restart / Wi-Fi deploy | `deploy --wifi --restart` takes 3.0 s and the game is back on the panel after 6.2 s. Saves to slot 99 on the way out, resumes slot 99, same pid | — |
| Sleep | the reader's own sleep sends `EVT_BACKGROUND`: suspend save, then `EVT_EXIT` and a second save; the next start resumes the same spot (game clock unchanged) | — |

ScummVM's own dialogs (restore, save, menu) are consistently slower to show
(1.6–2.0 s) than the game's screens (0.5–1.3 s). They redraw most of the frame,
and the refresh policy treats that as a large partial update.

## Findings

**P1: fix soon. P2: next. P3: when convenient.**

### mmIV

1. **P1: dragging does nothing.** `handler()` in `platform-inkview.cpp` passes
   only `EVT_POINTERDOWN/UP`. `EVT_POINTERMOVE` never reaches ScummVM, so
   scrollbar thumbs, sliders and drag-to-scroll in lists do not work.
   Swiping the restore list or dragging its thumb only selects a row. Lists
   still work with the scroll arrows (one row per tap) and the track (one page,
   8 rows, per tap).
2. **P1: idle animation keeps the panel busy.** With animated monsters in
   view and nobody touching the reader, the log shows 227–241 partial updates
   a minute (~4/s, ~2% area each). With a still view it is 0. This drains the
   battery and wears the eye. It belongs with the Phase 4 battery check.
   Options: lower the partial-update rate after some seconds without input,
   or skip regions smaller than N px when idle. Measure before and after with
   the per-minute `panel 60s:` log lines.
3. **P2: slot 99 is hard to reach and easy to overwrite.** "99. Suspended"
   is the last of 100 rows in the Restore and Save dialogs. Restoring it by
   hand from the title takes ~11 page taps (about 20 s). It is also offered
   as a normal save slot, and the next suspend or safety save overwrites
   whatever the player saved there. Options: hide slot 99 from the Save list
   (keep it in Restore), name it "Resume (automatic)", and open the Restore
   list scrolled to the newest save.
4. **P2: a restart ignores `mmiv/layout`.** The launcher reads the file only
   when `MMIV_LAYOUT` is unset. It also exports the value, so a restart in
   place (pbtk `restart`, which runs `mmiv.app` again) inherits the old
   layout. Switching layout needs a fresh start from the library. Fix in
   `mmiv.app`: remember where the value came from (for example
   `MMIV_LAYOUT_SOURCE=file`) and re-read the file in that case. Add a case
   to `tests/launcher/test-launcher.sh`.
5. **P3: the app folder path grows on every restart.** `init()` takes
   `_appDir` from `argv[0]`. After restarts the launcher path becomes
   `…/mmiv/../mmiv/../mmiv.app`, and the paths passed to ScummVM grow with
   it (`--config=…/mmiv/../mmiv/../mmiv/scummvm.ini`). Harmless today. Use
   `realpath()` on `argv[0]` (or the launcher's `DIR`).
6. **P3: touches outside the game frame.** One touch at the panel edge, where
   a hand holds the reader, arrived during the test (outside the 3x frame,
   which starts at y = 79). Check that touches outside the frame are dropped
   rather than clamped onto the nearest game pixel. If they are clamped, drop
   them.
7. **P3: log noise.** Every start logs "Only Windows v1-v5 bitmaps are
   supported, unknown header" 18 times (theme loading). Find which theme file
   triggers it, then fix or silence it so real warnings stand out.
8. **P3: readability on 16 grays.** The credits (white text on dark marble)
   and the automap (explored vs unexplored floor patterns, party marker) are
   hard to read. Everything else read well: character sheet, Quick
   Reference, info, dialogs, keypad. Candidates for the planned contrast
   setting.
9. **Not a bug: page keys in landscape.** Next = forward / list up and
   Prev = back / list down, as `mapKey()` intends for landscape. Confirmed
   by the player to match the physical buttons as the reader is held.
10. **Not a bug: Back from Credits and Other Options** leaves the bare title
    art; one more tap brings the menu back. That is Xeen's own behaviour.

### pbtk (toolkit), found while testing mmIV

These belong in the toolkit repo. They are listed because they affect how
reliably mmIV can be tested.

11. **P1: injected input is reported as the player's.** On the real InkView
    `SendEvent()` queues the event, so the agent's "dispatching" flag is
    already clear when the handler sees it. pbtk's timeline and `events` then
    show Claude's taps as the player's. (The host stub calls the handler
    directly, so the tests pass.) Fix: remember the injected events and drop
    the matching observed ones.
12. **P2: the reader sleeps during remote sessions.** Injected input does not
    reset the firmware's idle timer. After a few idle minutes the reader
    sleeps, the firmware ends the app and the agent is gone. Look for a
    keep-awake call while a pbtk client is connected (or have pbtk send
    something that counts as activity).
13. **P2: `shot --whole` ignores `view`.** For mmIV the raw framebuffer is
    already upright (`--rotate raw`). Whole-screen shots should follow `view`
    like canvas shots.
14. **P3:** no `rm` in the agent (test files cannot be cleaned up over
    Wi-Fi); no process stats (memory/CPU of the app); the canvas-vs-panel
    check needs to cope with the firmware keyboard, which changes the panel
    size.

## Plan

### Next (P1)
- [ ] Pass `EVT_POINTERMOVE` through as mouse moves while a finger is down
      (with a small movement threshold so taps stay taps). Add a host golden
      for dragging the restore list's scrollbar. Re-test on the reader with
      pbtk `swipe`.
- [ ] Idle refresh budget: after ~5 s without input, limit partial updates
      (e.g. ≤1/s, or merge small regions). Compare the `panel 60s:` lines
      before and after with monsters in view.
- [ ] pbtk: fix injected-vs-player events (toolkit), then
      `tools/update-pbagent.sh`.

### Then (P2)
- [ ] Slot 99: keep it out of the Save list, label it "Resume (automatic)",
      open Restore at the newest save.
- [ ] Launcher: re-read `mmiv/layout` on a restart; launcher test.
- [ ] Battery check (Phase 4) after the idle refresh budget: an hour of play
      with the reader's battery level from the log at start and end.
- [ ] pbtk: keep-awake during sessions; whole shots follow `view`.

### Later (P3)
- [ ] `realpath()` for the app folder; drop touches outside the frame; find
      the bitmap-header warning; contrast for credits/automap via the
      planned settings screen.
- [ ] A repeatable device smoke test: the pbtk steps above as a script
      (`tools/device-smoke.py`) with latency and canvas-vs-panel checks,
      run after each `pbtk deploy --wifi --restart`. It should not write
      saves and should stop if the player touches the reader.
