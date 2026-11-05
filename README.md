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
| `UI_IMAGE x y path` | blit a `.pgi` image (make one with `tools/mkimg.sh`) |
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

## View files — SwiftUI-style layouts (no coordinates)

For a whole screen, describe the layout in a **`.view`** file and let the engine
place everything with stacks — no x/y anywhere:

```swift
VStack(spacing: 6) {
  Header("HUNTING \(name)")
  HStack(spacing: 8) {
    Card("SIGNAL")    { Text("\(sig)", .green, .big) }
    Card("CLOSENESS") { Text("\(p)%",  .green, .big) }
  }.h(62)
  Meter(value: p, .green).h(20)
  Graph(history, .green).h(50)
  Spacer()
  Footer("CLOSER = FULLER", "RED BACK")
}
```

The payload becomes almost pure logic — set the variables, render the view:

```sh
. ui.sh
UI_BEGIN
name="Beast Mode"; sig=-50; p=100     # your data
UI_RENDER hunt.view                   # draws it; \(name), \(sig) are filled in
UI_KEY
UI_END
```

`\(name)`, `\(sig)` are interpolated from the shell. Design views visually in the
builder below and export the `.view`. Full syntax + the layout engine are in
[`views/`](views/README.md).

## Visual builder

Open **`builder.html`** in any browser — a drag-and-drop designer for `.view`
files with a live, pixel-perfect preview:

- A **component palette** (VStack, HStack, Card, Text, Meter, Graph, Image…) and
  an **outline** of your view's structure.
- Click an element to select it, edit it in the **inspector** (text, color, size,
  fixed `.h`/`.w`), reorder or wrap it in a stack.
- The **`.view` code** sits in a plain editor at the bottom — edit it directly and
  the canvas follows, so you can paste one in or copy it out. Both stay in sync.
- **Copy .view** to drop it next to your payload.

No install, no server — it's a single self-contained file.

## Examples

Full payloads in `demo/`, all pure bash:

- **`App_Demo/`** — **how to structure a real app**: one main loop that's a state
  machine over screens (a menu plus Stopwatch / System / About sub-screens). A
  stopwatch **keeps running as you move between screens** — it's derived from a
  start timestamp, so it never pauses or resets — and the battery shows on every
  screen. The menu uses the command API (dynamic, selectable); the sub-screens
  are view files. Start here if you want to know how to do more than one screen.
- **`payload.sh`** — a dashboard with cards and a bar-graph, a menu, a confirm,
  and message screens.
- **`recon_monitor.sh`** — a **live** Wi-Fi dashboard: access-point and client
  counts, a scrolling packets/sec graph and a battery meter.
- **`wifi_foxhunt.sh`** — pick a nearby access point, then hunt it: a proximity
  meter and signal graph fill up as you get closer. Its hunt screen is a **view
  file** (`wifi_foxhunt.view`) — even the colours are interpolated, so the whole
  screen greens up as you approach — while the dynamic AP list stays on the
  command API. Full line-by-line walkthrough in
  [`docs/wifi_foxhunt_explained.md`](docs/wifi_foxhunt_explained.md).

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
| `ui.sh` | the command library + `UI_RENDER` — source this in your payload |
| `pguid` / `pguid.c` | the raw-canvas renderer (prebuilt binary + source) |
| `pgviewc` / `pgviewc.c` | compiles a `.view` into draw commands (used by `UI_RENDER`) |
| `font5x7.h` | the bitmap font |
| `build.sh` | reproducible build of both binaries |
| `builder.html` | the visual **view-file** builder |
| `tools/mkimg.sh` | turn a PNG/JPG into a `.pgi` image for `Image`/`UI_IMAGE` |
| `views/` | the view-file syntax, engine source, and samples |
| `demo/` | example payloads (`payload.sh`, `recon_monitor.sh`, `wifi_foxhunt.sh`, `View_Demo/`) |
| `docs/` | the WiFi Foxhunt code walkthrough |

Works on WiFi Pineapple Pager firmware 2.0. MIT-licensed — do what you like with it.
