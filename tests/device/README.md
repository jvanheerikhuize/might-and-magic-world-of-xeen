# Device scenarios

Gameplay scenarios that pbtk (the PocketBook toolkit, `pbtk run`) plays on a
real reader running the mmIV bundle with pbagent. Each one opens the game at a
probe save, taps the on-screen buttons and keys, and checks the screen:

| Scenario | Probe save | What it checks |
|---|---|---|
| `01-town-walk` | 20 Vertigo | steps, turns, party screens, back to exploring |
| `02-night-town` | 21 Nightshadow | page keys, the automap |
| `03-dark-dungeon` | 25 Dwarf Mine 1 | stepping in the dark (no visible change) |
| `04-combat` | 27 Castle Burlock 1 | a fight with the combat buttons |
| `05-outdoors` | 23 Clouds A1, 33 Skyroad A1 | big redraws outdoors |
| `06-portrait-keypad` | 30 Castleview | portrait layout, the touch keypad |
| `07-landscape-flip` | 31 Olympus | the turned landscape layout, page keys |

## Probe saves

The saves are made from the player's own game and party; they hold game data
and stay out of the repository.

    tools/build-host.sh pocketbook
    python3 -I tools/make-probe-saves.py ~/games/xeen/probe-saves --from 0

`--from` picks the save in `~/games/xeen/saves` to start from. Each place gets
its slot (20–37, see `PLACES` in the script); a place where combat starts on
arrival is tried at other spots and left out when every spot fights (28, the
Warzone). `tools/xeen-maps.py` lists every map id and name.

## On the reader

1. Back up the player's saves (`pbtk pull /mnt/ext1/applications/mmiv/saves …`).
2. Push the probe saves to `/mnt/ext1/applications/mmiv/probe-saves/` and
   write `probe-saves` to `mmiv/savedir`. The game then saves and loads there
   only; `mmiv/saves` is not touched.
3. Run a scenario from the repository root:

       pbtk run tests/device/scenarios/01-town-walk.txt --out OUT --stop-on-user
       pbtk --view none run tests/device/scenarios/06-portrait-keypad.txt …
       pbtk --view cw run tests/device/scenarios/07-landscape-flip.txt …

   `--stop-on-user` ends the run when someone touches the reader.
   `OUT/report.json` has every step, the panel-change latencies and the shots.
4. Afterwards write `saves` to `mmiv/savedir` and `landscape` to
   `mmiv/layout`, then restart the app.

The scenarios use `write …/mmiv/resume SLOT` and `restart`: the launcher opens
that slot once and removes the file. Coordinates are upright landscape (pbtk
view `ccw`) unless the header says otherwise.
