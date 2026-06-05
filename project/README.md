# Jetpack Joyride — LCOM Edition

A *Jetpack Joyride*-style arcade game for **MINIX**, built on the LCOM framework.
Fly your jetpack as far as possible, dodge electric **zappers** and **missiles**, and
grab coins. Includes timed **power-ups**, a persistent **XP / unlockable-powers**
progression with **user accounts**, a live real-time clock, and a **2-player race
over the serial port**.

## I/O devices used

All six devices studied in the course are exercised:

| Device | Role |
| --- | --- |
| **Timer (i8254)** | 60 Hz game tick driving update + render |
| **Keyboard (KBC)** | thrust (`W`/`SPACE`), menu navigation, pause, quit |
| **Mouse (PS/2)** | menu cursor + clicks, hold-to-thrust during play |
| **Graphics (VBE 0x115)** | 800×600, 24-bit, linear frame buffer + double buffering |
| **RTC (MC146818)** | live date/time, 1 Hz update interrupt, RNG seed, high-score timestamps |
| **Serial port (UART 16550)** | interrupt-driven link for the 2-player race |

## Controls

| Input | Action |
| --- | --- |
| `W` / `SPACE` / left mouse button | fire jetpack (fly up); release to fall |
| Mouse move + click | navigate and select menu buttons |
| `↑` / `↓` + `ENTER` | navigate and select with the keyboard |
| `P` | pause / resume (solo) |
| `ESC` | back to menu, or quit from the menu |

The thrust keys can be remapped in **Settings** to one of three control schemes
(`W`/`SPACE`, `↑`/`ENTER`, or `Z`/`X`). Settings also expose difficulty, starting
speed and visual effects, and are persisted to disk.

## Architecture (MVC)

```
include/devices/   kbc, mouse, timer, video_gr, rtc, serial, utils, i8042, i8254
include/game/      config, entities, model, view, controller, net, scores,
                   settings, game (+ model_internal, view_internal)
src/devices/       device drivers
src/game/          main, game (glue), controller, net, scores, settings
src/game/model/    model, model_geometry, model_effects, model_spawn
src/game/view/     view, view_draw, view_world, view_background, view_sprites, view_ui
```

- **Model** (`model.c`, `entities.h`) — pure simulation: physics, spawning,
  collision, scoring. No drawing, no device access. All state lives in one
  `GameModel`.
- **View** (`view.c`) — all rendering (sprites, font, HUD, per-state screens).
  Reads a `const GameModel`; never mutates it.
- **Controller** (`controller.c`) — turns keyboard/mouse events into changes on
  the model. Button geometry is shared with the view through `config.h`.
- **Glue** (`game.c`, `main.c`) — `main.c` owns the interrupt loop and dispatches
  hardware interrupts; `game.c` owns the single `GameModel` and connects model,
  view, controller and the serial netplay.

All tunables (physics, colours, spawn rates, button layout) live in
`include/game/config.h`.

## Build & run (MINIX / LCOM)

```sh
cd project
make
lcom_run proj          # or: service run `pwd`/proj
```

## 2-player mode

Two MINIX machines connected by a **null-modem cable on COM1** (or two VMs with
their first serial ports cross-connected). On each machine pick **2-PLAYER** from
the menu. Both sides exchange a short **HELLO / READY handshake** in the lobby
(`WAITING`) — which also agrees a shared RNG seed so the obstacle course matches —
and then start on the same frame. Each player flies their own course; the link
exchanges position and score so you see the opponent as a blue **ghost** (`P2`)
and a `P2` score in the HUD. When both players crash, the game-over screen shows
the winner (highest score).

The link is **non-blocking**: if a peer goes silent it is dropped after a timeout
(`NET_PEER_TIMEOUT`), so a pulled cable can't hang the round.

### Protocol

A small framed protocol at 115200 baud, 8N1, with per-frame **CRC-8** (poly 0x07):

```
SYNC0(0xA5) SYNC1(0x5A) TYPE LEN B0..B(LEN-1) CRC8
  CRC8 covers TYPE, LEN and the LEN payload bytes.
  STATE (1): payload y(2) score(2) seed(2)   (sent every 3 frames while playing)
  DEAD  (2): payload score(2)                (sent once on death)
  HELLO (3): payload seed(2)                 (lobby handshake beacon)
  READY (4): payload seed(2)                 (sent after the peer's HELLO is seen)
  BYE   (5): no payload                      (one-shot: cancels a waiting/playing peer)
```

The receiver uses a sliding-window parser that re-syncs byte by byte, so a
payload byte that happens to equal a sync byte can't lock it onto a false frame.

## Power-ups

Two timed pickups float in from the right (5 s each):

- **S — Shield**: temporary invulnerability; absorbs a zapper touch and destroys
  any missile that hits you. A pulsing cyan ring shows it is active.
- **M — Magnet**: nearby coins are pulled toward the player.

## XP, powers and accounts

Coins, dodged missiles, power-ups and distance all earn **XP**, which is saved
per user. Reaching XP thresholds unlocks permanent **powers** that then apply to
every run:

- **2× Coins**, **Shield at start**, **Slower missiles**, **Extra life**.

On launch you log in with a **name + password** (new names are registered
automatically); your XP — and therefore your unlocked powers — is restored from
disk. Thresholds, rewards and power bit-flags all live in `include/game/config.h`.

## Persistent leaderboard

The best runs are saved to disk with the player **name** and their **RTC
date/time**, and shown on the menu and on the game-over screen (with the rank
earned). Paths are defined in `include/game/config.h`
(`SCORES_PATH`, `SETTINGS_PATH`, `USERS_PATH`), under `data/` by default —
point them at a writable location on your VM if needed.

## Documentation

The source is documented with **Doxygen**. To generate the browsable HTML docs:

```sh
doxygen Doxyfile        # or, on MINIX: make docs
```

Open `docs/html/index.html` in a browser. Configuration lives in `Doxyfile`;
the generated `docs/` folder is git-ignored.

## Notes

- The RNG is seeded from the RTC, so the obstacle course differs each launch
  (in 2-player mode both machines agree a shared seed during the handshake).
- The background (sky gradient + scrolling city skylines) is drawn procedurally,
  not from image assets.
- The serial driver counts line-status errors and RX-ring overflows, and the
  2-player link drops a silent peer after a timeout so a pulled cable can't hang
  the round.
- `proj`, object files (`*.o`) and the generated `docs/` are git-ignored.
