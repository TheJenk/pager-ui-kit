# Pager UI Kit

Add good-looking **custom screens and menus** to your WiFi Pineapple Pager
payloads — in a few lines of bash, with no framebuffer code.

Building a custom on-screen UI for the Pager normally means writing a tiny C
program that pokes `/dev/fb0`, ships its own font, and decodes raw input. This
kit does all of that for you: a small bundled renderer owns the screen, and your
payload just calls simple commands.

```sh
. ui.sh
UI_BEGIN
choice="$(UI_MENU "Main Menu" "Start scan" "View targets" "Settings" "Exit")"
UI_MESSAGE "You picked" "$choice"
UI_END
```

That's a complete, scrollable, D-pad-navigable menu. The commands are UPPERCASE
so they sit naturally alongside the rest of a payload.

## Quick start

1. Drop `ui.sh` and the `pguid` renderer next to your `payload.sh`.
2. Source it and go:

```sh
. "$(dirname "$0")/ui.sh"
UI_BEGIN
UI_HEADER "My Tool"
UI_TEXT 20 70 "Hello, Pager" GREEN 3
UI_FOOTER "GREEN  OK" "RED  QUIT"
UI_SHOW
UI_KEY                 # waits for a button: UP/DOWN/LEFT/RIGHT/A/B
UI_END
```

## Commands

**Session**
| | |
|---|---|
| `UI_BEGIN` | take over the screen and start drawing |
| `UI_END` | hand the screen back (also runs automatically on exit) |

**One-line screens** — the easy way to get a lot done
| | |
|---|---|
| `UI_MENU "Title" item1 item2 ...` | scrollable menu; **echoes the chosen item**, or exits non-zero if the user backs out |
| `UI_MESSAGE "Title" "line1" "line2" ...` | centered message; waits for any key |
| `UI_CONFIRM "Question?"` | yes/no; exit 0 = Green/yes, 1 = Red/no |

**Build your own screen**
| | |
|---|---|
| `UI_CLEAR [color]` | fill the screen |
| `UI_HEADER "Title"` | title bar across the top |
| `UI_FOOTER "left" "right"` | hint line along the bottom |
| `UI_CARD x y w h ["Title"]` | a panel with an optional heading |
| `UI_TEXT x y "text" [color] [size]` | text (size 1–4, default 2) |
| `UI_RECT x y w h [color]` | filled rectangle |
| `UI_BOX x y w h [color]` | outline |
| `UI_BAR x y w h [color]` | filled bar |
| `UI_METER x y w h percent [color]` | a 0–100 level/progress bar |
| `UI_GRAPH x y w h "v1 v2 v3 ..." [color]` | auto-scaled bar graph of a series |
| `UI_SHOW` | render everything you've drawn |
| `UI_KEY [timeout]` | wait for a button → `UP DOWN LEFT RIGHT A B POWER` (A = Green, B = Red) |

**Colors** can be names — `WHITE GREY RED GREEN BLUE CYAN YELLOW AMBER ORANGE
PURPLE PINK ACCENT DIM` — or any `RRGGBB` hex. Coordinates are the upright
480×222 screen, `(0,0)` top-left.

### Re-skin it

Set any palette variable before `UI_BEGIN`:

```sh
UI_BG=0b0d12; UI_ACCENT=2fae6a; UI_SELECT=1f6f4a
UI_BEGIN
```

## Visual builder

Open **`builder.html`** in any browser for a drag-and-drop designer with a live
preview (pixel-perfect — it uses the same font) and the generated code below:

- Add elements with buttons, **drag to move, drag a corner to resize**.
- **Edit the code directly** and the canvas follows — so you can paste a payload
  in and see it, or copy snippets from anywhere.
- It reads the `UI_*` commands and **ignores the rest of the payload**.
- Payloads with **several screens** (more than one `UI_CLEAR…UI_SHOW`) show up as
  thumbnails along the top; pick one to edit it, and its lines light up in the
  code so you can see where it lives.

No install, no server — it's a single self-contained file.

## Examples

Three full payloads in `demo/`, all pure bash:

- **`payload.sh`** — a dashboard with cards and a bar-graph, a menu, a confirm,
  and message screens.
- **`recon_monitor.sh`** — a **live** Wi-Fi dashboard: access-point and client
  counts, a scrolling packets/sec graph and a battery meter.
- **`wifi_foxhunt.sh`** — pick a nearby access point, then hunt it: a proximity
  meter and signal graph fill up as you get closer. There's a full line-by-line
  walkthrough in [`docs/wifi_foxhunt_explained.md`](docs/wifi_foxhunt_explained.md).

```sh
scp pguid ui.sh demo/payload.sh root@172.16.52.1:/root/payloads/user/general/UI_Kit_Demo/
# then: Payloads > user > general > UI Kit Demo
```

A live screen that refreshes on a timer but reacts instantly to a button looks
like this:

```sh
while true; do
    pps=$(... pull a number ...)
    history="$history $pps"; set -- $history; while [ $# -gt 46 ]; do shift; done; history="$*"
    UI_CLEAR; UI_HEADER "LIVE"
    UI_GRAPH 20 40 440 120 "$history" ACCENT
    UI_SHOW
    case "$(UI_KEY 1)" in B) break ;; esac   # 1s refresh, or a button
done
```

## How it works

`ui.sh` talks to **`pguid`**, a ~7 KB renderer that owns `/dev/fb0` (handling the
Pager's rotated RGB565 layout), the font, and the buttons. Your payload sends it
plain text draw commands and reads button presses back. Nothing in your payload
touches the framebuffer, so it works from bash — or any language that can write
to a pipe.

It's **easy on the battery**: the renderer blocks until a command or a button
arrives, so it uses **no CPU at all while your UI is just sitting there**, and
draws only when you call `UI_SHOW`. A written and freestanding C core keeps it
tiny (~7 KB, no runtime) and instant to start.

## Building the renderer

`pguid` is prebuilt (and reproducible). To rebuild:

```sh
# brew install messense/macos-cross-toolchains/mipsel-unknown-linux-gnu
bash build.sh
```

## Files

| File | |
|---|---|
| `ui.sh` | the command library — source this in your payload |
| `pguid` / `pguid.c` | the renderer (prebuilt binary + source) |
| `font5x7.h` | the bitmap font |
| `build.sh` | reproducible build of `pguid` |
| `builder.html` | the visual drag-and-drop UI builder |
| `demo/` | example payloads (`payload.sh`, `recon_monitor.sh`, `wifi_foxhunt.sh`) |
| `docs/` | the WiFi Foxhunt code walkthrough |

Works on WiFi Pineapple Pager firmware 2.0. MIT-licensed — do what you like with it.
