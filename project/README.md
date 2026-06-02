# Jetpack Joyride — LCOM Edition

A *Jetpack Joyride*-style arcade game for **MINIX**, built on the LCOM framework.
Fly your jetpack as far as possible, dodge electric **zappers** and **missiles**, and
grab coins. Includes a live real-time clock and a **2-player race over the serial port**.

## I/O devices used

All six devices studied in the course are exercised:

| Device | Role |
| --- | --- |
| **Timer (i8254)** | 60 Hz game tick driving update + render |
| **Keyboard (KBC)** | thrust (`W`/`SPACE`), menu navigation, pause, quit |
| **Mouse (PS/2)** | menu cursor + clicks, hold-to-thrust during play |
| **Graphics (VBE 0x115)** | 800×600, 24-bit, linear frame buffer + double buffering |
| **RTC (MC146818)** | live on-screen date/time, 1 Hz update interrupt, RNG seed |
| **Serial port (UART 16550)** | interrupt-driven link for the 2-player race |

## Controls

| Input | Action |
| --- | --- |
| `W` / `SPACE` / left mouse button | fire jetpack (fly up); release to fall |
| Mouse move + click | navigate and select menu buttons |
| `↑` / `↓` + `ENTER` | navigate and select with the keyboard |
| `P` | pause / resume (solo) |
| `ESC` | quit the program |

## Architecture (MVC)

```
include/devices/   kbc, mouse, timer, video_gr, rtc, serial, utils, i8042, i8254
include/game/      config, entities, model, view, controller, net, game
src/devices/       device drivers
src/game/          main, game (glue), model, view, controller, net
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
the menu. Each player flies their own course; the link exchanges position and
score so you see the opponent as a cyan **ghost** and a `P2` score in the HUD.
When both players crash, the game-over screen shows the winner (highest score).

The link is **non-blocking**: if no peer is connected, 2-player mode still runs
as a normal solo game (the ghost simply never appears).

### Protocol

A tiny framed protocol at 115200 baud, 8N1:

```
SYNC(0xA5) TYPE B0 B1 B2 B3 CHK     CHK = TYPE ^ B0 ^ B1 ^ B2 ^ B3
  TYPE_STATE (1): B0:B1 = player Y, B2:B3 = score   (sent ~20x/s while playing)
  TYPE_DEAD  (2): B0:B1 = final score               (sent once on death)
```

## Notes

- The RNG is seeded from the RTC, so the obstacle course differs each launch.
- `core.*`, `proj` and object files are git-ignored.
