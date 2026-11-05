# View files — SwiftUI-style layouts (in progress)

Instead of placing every element by hand, describe a screen as a **view file** and
let the engine lay it out. Stacks arrange their children automatically, so there
are **no coordinates** — it's SwiftUI for the Pager.

```swift
VStack(spacing: 6) {
  Header("HUNTING \(name)")
  HStack(spacing: 8) {
    Card("SIGNAL")    { Text("\(sig)", .green, .big) }
    Card("CLOSENESS") { Text("\(p)%",  .green, .big) }
    Card("TREND")     { Text(trend,    .amber) }
  }.h(62)
  Meter(value: p, .green).h(20)
  Graph(history, .green).h(50)
  Spacer()
  Footer("CLOSER = FULLER + GREENER", "RED BACK")
}
```

A payload sets its variables and renders the view; `\(name)`, `\(sig)` etc. are
interpolated from the shell. No hand-placed pixels.

## Components

| | |
|---|---|
| `VStack(spacing: n) { … }` | stack children top-to-bottom |
| `HStack(spacing: n) { … }` | stack children left-to-right |
| `ZStack { … }` | overlay children |
| `Card("Title") { child }` | a titled panel with a border |
| `Text("str", .color, .big)` | text (`.big`/`.med`/`.small` or `.size(n)`) |
| `Header("str")` | the top title bar |
| `Footer("left", "right")` | the bottom hint line |
| `Meter(value: n, .color)` | a 0–100 level bar |
| `Graph("v1 v2 …", .color)` | an auto-scaled bar graph |
| `Bar(.color)` | a filled block |
| `Image("file.pgi")` | blit an image (make one with `tools/mkimg.sh`) |
| `Spacer()` | a flexible gap that pushes things apart |

**Modifiers:** `.h(n)` / `.w(n)` pin a size, `.pad(n)` sets padding, `.color`
and `.big`/`.med`/`.small` set text style.

**Layout:** in a stack, items with a fixed size (`Header`, `Footer`, `Text`, or
anything with `.h`/`.w`) take that size; everything else shares the remaining
space equally. `Spacer()` is the classic "push the rest away".

## Try it (host preview)

```sh
# brew install ffmpeg
./build.sh hunt.view          # -> hunt.png
./build.sh dashboard.view     # -> dashboard.png
```

`pgview.c` is the engine (parser + stack layout + draw). It renders a view to the
same 480×222 the Pager draws, so the preview matches the device.

## Using a view from a payload

Views run on the device — `UI_RENDER` resolves `\(var)` from the shell, compiles
the view with `pgviewc`, and feeds the result to the renderer:

```sh
. ui.sh
UI_BEGIN
name="Beast Mode"; sig=-50; p=100
UI_RENDER hunt.view
UI_KEY
UI_END
```

`pgviewc` (built from `pgviewc.c`) is the engine on the device — a freestanding
compiler that turns a `.view` into `pguid` draw commands, so the renderer stays a
simple canvas. `pgview.c` here is the same engine as a host tool for PNG previews.

The command-based API (`UI_CARD`, `UI_TEXT`, …) in `ui.sh` still works — view
files are the higher-level, no-coordinates option on top of it.

## Images

`Image("logo.pgi")` blits a picture. Give it a size with `.w`/`.h` (inside a
stack it otherwise fills its share). The file is a **`.pgi`** — the Pager's own
pixel format with a tiny size header, so the renderer just reads and blits it,
no decoding. Make one from any image:

```sh
tools/mkimg.sh logo.png logo.pgi 96 96    # resize to 96x96
tools/mkimg.sh icon.png icon.pgi 48       # 48 wide, height keeps the aspect
```

Keep them small — a `.pgi` is uncompressed (96×96 ≈ 18 KB, full-screen ≈ 208 KB).
Ship the `.pgi` next to your `payload.sh`; paths in a view are relative to it.

Or skip the command line entirely: **drag a PNG/JPG onto `builder.html`** and it
converts to a `.pgi` in the browser, downloads it, and adds the `Image(...)` for
you.

## Notes

- One UI command / component per line still applies to the command API; view
  files use normal nesting.
- Image paths are resolved relative to the payload directory (where `ui.sh`
  lives), so `Image("logo.pgi")` just works when the `.pgi` ships alongside.
