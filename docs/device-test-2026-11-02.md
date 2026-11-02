# Random playthroughs on the reader — 2026-11-02

Discovery only: random and guided play on the PocketBook Touch Lux 5 (PB628,
firmware U628.6.9.988, build `Nov 1 2026 23:01:53`), driven over Wi-Fi with
pbtk while the player watched. No fixes were made; this file lists what was
found. The previous pass is `device-test-2026-11-01.md`.

## How it was played

- The player's saves were copied off first (`~/games/xeen/backup-2026-11-02`)
  and `mmiv/savedir` pointed at `probe-saves`.
- **Random scenarios**: `tools/random-scenarios.py SEED COUNT STEPS OUT` writes
  pbtk scripts that open a probe save and play weighted random actions
  (walk, turn, strafe, action buttons, party screens, Cast/New, rest, bursts,
  long presses, taps outside the frame). Seeds 1102 (5 × 20 steps) and 1103
  (6 × 14 steps). A shot after every action; afterwards the shots were diffed
  and laid out as contact sheets.
- **Guided play**: Light in the dark dungeons, a fight in Castle Burlock, the
  Bat Queen in Nightshadow, gates, the ScummVM menu (Load, Options, Return to
  Launcher), loading probe saves through the Restore list instead of restarts.
- A small explorer (pbtk Python API) walked forward until blocked, turned, and
  stopped when the logo box changed (dialog, shop or combat).

## Findings

**P1: blocks play or loses progress. P2: next. P3: when convenient.**

### Reader and pbtk

1. **P1 (pbtk): the reader powers off during remote sessions.** Three times the
   firmware sent `EVT_BACKGROUND`, then `EVT_EXIT`; mmIV saved (when it could)
   and exited cleanly, and the reader was off. Auto power-off is set to
   10 minutes, and the outages came 9.6, 14.7 and 10.7 minutes after the
   player last touched the reader. pbagent's keep-awake only calls
   `BanSleep(60)` while a client is connected, but pbtk opens one short
   connection per command, so any pause over a minute between commands lets
   the ban lapse and the overdue power-off fires at once. Holding one
   connection open (a ping every 15 s) kept the reader on for the rest of the
   session. Fix in pbagent: keep the ban for some minutes after the last
   request, not just while a socket is open (or pbtk keeps a connection).
   - Not the cause, though it looked like it: the first outage came 1 s after
     a swipe on the title screen, and the first two came after exactly 7
     in-place restarts each. The third had neither.
2. **P2 (mmIV): no battery level in the log.** Nothing calls
   `GetBatteryPower()`, so an outage cannot be told apart from an empty
   battery afterwards, and the planned Phase 4 battery check has no data. Log
   it at start, with the `panel 60s:` lines, and at exit.
3. **P3 (pbtk): an apostrophe in a script line stops the run** with a raw
   `shlex` traceback ("No closing quotation"), e.g. `note … (Xeen's Castle 1)`.
   Report it as a line error, or read `note` text without shlex.
4. **P3 (pbtk): `pbtk key` has no hold option.** Holding MENU works only in a
   script (`key menu 1200`). The agent also refuses `/proc` paths, so the app's
   open files cannot be checked remotely.

### mmIV

5. **P1: Return to Launcher strands the player.** Hold MENU → Return to
   Launcher shows ScummVM's launcher with an **empty game list** (the game is
   started from arguments and is not in `scummvm.ini`): Start, Game Opts and
   Remove are disabled. Back there quits mmIV at once, without asking. Hide
   the button (or make it quit to the library after a save).
6. **P2: Run in combat and Dismiss share a spot.** The combat Run button
   (845,394) is Dismiss while exploring. Tapping Run until it works ends the
   fight and the next tap opens "Dismiss whom?"; a portrait tap then would
   dismiss a character. With e-ink's slow refresh repeated taps are likely.
   Ignore taps on that spot for a second after combat ends, or ask to confirm.
7. **P2: sound-only feedback is lost.** Failing to open a grate (tap it, pick a
   character) shows nothing at all; on the PC there is a sound. A failed Run
   attempt also looks exactly like an ignored tap. Candidates for the e-ink
   cues from PR #7 (a short text line in the message area).
8. **P2: after dragging a list's scrollbar thumb the row text is clipped** at
   a fixed x (~262 px): "Vertic", "Nights", "Winter". The next redraw (a
   ▼ tap) draws them whole. Seen in the canvas, so it is drawn that way, not
   an e-ink leftover. Swiping the list body draws them correctly.
9. **P3: hover highlight stays after a tap.** In ScummVM's dialogs the button
   under the last touch stays highlighted when a dialog closes: after Cancel
   in Options, "Quit" in the menu looks selected; after Cancel in Restore,
   "Return to Launcher". On touch, drop the hover on finger up.
10. **P3: prompts name keys the reader does not have.** "Open Grate — Who will
    try? F1 – F6" (tapping a portrait works). Same for any F1–F6 prompt.
11. **P3: Options shows only Audio and Keymaps.** The reader has no sound
    (`/proc/asound/cards: (none)`), so the volume sliders do nothing; there is
    no place yet for the planned contrast/e-ink settings.
12. **P3: modal layers feel like a freeze.** Cast → New → Back closes the
    spell list but leaves the Cast panel; until a second Back every movement
    key is ignored. That is Xeen's behaviour, but without an Esc label it
    reads as a hang.
13. **P3: log noise.** New: `initWithU32String: Fribidi not available` when the
    launcher opens. Still there: the 18 bitmap-header warnings, now also each
    time the ScummVM menu loads its theme.
14. **P3: the title screen flashes.** Toggling the title between the menu and
    the "World of Xeen" art with Back gave 62–64 full refreshes per minute
    and partial updates 40–65× slower than usual (43–65 ms per call).

### Tests and probe saves

15. **P1 (tests): the walking checks do not prove the party moved.** Probe
    saves 20 (Vertigo), 31 (Olympus) and 37 (Great Pyramid) put the party at
    8,8 inside walls: 45–50 explorer moves never left the square. Scenarios
    `01-town-walk` and `07-landscape-flip` still pass, because `expect-change`
    after a step is satisfied by idle animation. Check the position instead
    (automap X/Y box or `expect-differs` on the view with a high threshold),
    and move the town spots to a street.
16. **P2 (probe saves): five places kill or trap the level-1 party.** 26 Witch
    Tower 1 (falls to its death on the first step), 29 Xeen's Castle 1 (dead
    on the first step), 32 Darkside Area A1 (dead after two turns), 33 Skyroad
    A1 (falls within two steps), 37 Great Pyramid 1 (walled in).
    `make-probe-saves.py` only rejects spots where combat starts; it should
    also take a step in each direction and reject spots where the party dies
    or cannot move. `05-outdoors` steps only twice on 33 and does not notice.

### Works (checked this time)

- **Dragging** (finding 1 of 2026-11-01 is fixed on the reader): the Restore
  list's scrollbar thumb, a finger swipe on the list body (7 rows up), and
  the Options volume slider all follow the finger.
- Loading probe saves through hold MENU → Load → Restore, with no restart.
- Light (Cast → Cast) in Lost Souls Dungeon, Darkstone Tower and Great
  Pyramid; walking in the light redraws in about a second. Rest ends Light
  (as in the game).
- Combat: Attack, Use (Back closes the Items screen), QuickFight options,
  Run (succeeds after a few tries), the character sheet during a fight,
  dead characters' "RIP" portraits.
- Tapping an object in the 3D view interacts with it (the grate); tapping a
  portrait answers a "who will try" prompt and picks the caster in Cast.
- Party death opens the Restore list; Cancel goes to the title.
- Long press on a portrait opens the character sheet; taps outside the game
  frame do nothing; MENU short press while exploring does nothing.
- Every party screen opened and closed back to a pixel-identical view
  (character sheet, Quick Reference, Game Info, Quests, automap, Cast).

## Next (discovery)

- Portrait layout with the keypad, and the 3x landscape flip, with the same
  random generator (needs a coordinate table per layout).
- Shops, inns and NPC dialogs once the town probe spots are on a street.
- Text entry (naming a save) and the firmware keyboard in portrait.
