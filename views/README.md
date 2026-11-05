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

## Status

The layout engine works (see the samples). Still to wire up:

- fold the engine into the `pguid` renderer so a payload can `UI_RENDER file.view`
  directly on the device;
- a live view-file preview in `builder.html`.

The command-based API (`UI_CARD`, `UI_TEXT`, …) in `ui.sh` keeps working either
way — view files are the higher-level, no-coordinates option on top of it.
