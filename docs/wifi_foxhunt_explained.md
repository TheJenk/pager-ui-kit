# WiFi Foxhunt — a line-by-line walkthrough

`demo/wifi_foxhunt.sh` is a complete, real payload built with the kit. You pick a
nearby access point, then "hunt" it: a proximity meter and a signal-history graph
fill up and turn green as you walk closer (stronger RSSI). Every pixel is drawn by
the bundled renderer; all the logic is plain bash.

This explains each part so you can lift the patterns into your own payloads.

---

## 1. The payload header

```sh
#!/bin/bash
# Title: WiFi Foxhunt
# Author: Jenx
# Description: ...
# Version: 1.0
```

Standard Pager payload header — the Payloads menu reads `Title`/`Description` from
these comments. Nothing kit-specific yet.

## 2. Finding the kit

```sh
for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/WiFi_Foxhunt" "/mmc/ui-kit"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"
```

When the Pager launches a payload it runs a **copy in `/tmp`**, so `$0` doesn't
point at your payload's folder. This loop finds `ui.sh` by checking, in order: the
firmware's `$_PAYLOAD_HOME`, the script's own dir, the install path, and a shared
`/mmc` location. Then `. "$KIT/ui.sh"` sources the kit so all the `UI_*` commands
exist. Copy this block into any payload that uses the kit.

## 3. The data layer — reading access points from recon

```sh
API=/tmp/api.sock
DEVS="http://localhost/api/recon/wifi/devices?pagesize=80&sort=signal"
```

The firmware's local API lists every device recon has seen, already sorted by
signal. We talk to it over its Unix socket with `curl --unix-socket`.

```sh
scan_aps() {
    curl -s -m3 --unix-socket "$API" "$DEVS" | awk '
        BEGIN { RS="\"mac\":\"" }
        NR>1 && /"type":"ap"/ {
            mac = substr($0,1,17)
            sig=""; if (match($0,/"signal":-?[0-9]+/)) sig=substr($0,RSTART+9,RLENGTH-9)
            ch="";  if (match($0,/"channel":[0-9]+/))  ch=substr($0,RSTART+10,RLENGTH-10)
            ss="";  if (match($0,/"ssid":"[^"]+"/))    ss=substr($0,RSTART+8,RLENGTH-9)
            if (sig != "") print sig "|" mac "|" ch "|" ss
        }' | sort -t'|' -k1,1nr | head -8
}
```

There's **no python on the Pager**, so we parse JSON with `awk`. The trick is
`RS="\"mac\":\""` — awk splits the response into one record per device (each starts
right after a `"mac":"`). For every record that is an access point:

- `mac = substr($0,1,17)` — the MAC is the first 17 chars of the record.
- `match(...)/substr(...)` pulls `signal`, `channel`, and the first `ssid`.
- We print `signal|mac|channel|ssid`.

`sort -t'|' -k1,1nr` sorts by the first field (signal) **numerically, reversed** —
RSSI is negative, so `-50` sorts above `-70`, i.e. strongest/closest first.
`head -8` keeps the top eight.

```sh
signal_of() {
    curl -s -m3 --unix-socket "$API" "$DEVS" | awk -v m="$1" '
        BEGIN { RS="\"mac\":\"" }
        NR>1 && substr($0,1,17)==m {
            if (match($0,/"signal":-?[0-9]+/)) print substr($0,RSTART+9,RLENGTH-9); exit
        }'
}
```

During the hunt we only care about **one** AP, so this re-queries and prints just
that MAC's current signal (or nothing if it's gone quiet). Same `RS` split, matched
to the target MAC.

## 4. Turning RSSI into something visual

```sh
prox() { local s=$1 p=$(( (s + 90) * 100 / 50 )); [ $p -lt 0 ] && p=0; [ $p -gt 100 ] && p=100; echo $p; }
```

Signal strength (`-90` far … `-40` close) mapped to a **0–100 "closeness"**. That
single number drives the meter, the graph height, and the colour. Integer math only
(the Pager has no FPU), clamped to 0–100.

## 5. Starting the UI

```sh
UI_BEGIN
```

Takes over the screen and starts the renderer. From here, draw and read buttons with
the `UI_*` commands. (At the very end, `UI_END` hands the screen back.)

## 6. Screen one — the access-point list

```sh
pick_ap() {
    local sel=0 lines=() ...
    while true; do
        lines=(); while IFS= read -r l; do lines+=("$l"); done < <(scan_aps)
        n=${#lines[@]}
```

Each pass it **re-scans** (so the list stays live) into a bash array. `sel` is the
highlighted row.

```sh
        UI_CLEAR
        UI_HEADER "WIFI FOXHUNT"
        UI_TEXT $((480-96)) 8 "$n NEARBY" DIM 1
```

Clear the frame, draw the title bar, and a small "N NEARBY" count in the top-right.

```sh
        for ((i=0; i<n; i++)); do
            y=$(( 34 + i*23 ))
            IFS='|' read -r sig mac ch ss <<< "${lines[$i]}"
            name="${ss:-${mac: -8}}"
            [ "$i" -eq "$sel" ] && UI_RECT 6 $((y-2)) 468 22 "$UI_SELECT"
            local tc=TEXT; [ "$i" -eq "$sel" ] && tc=WHITE
            UI_TEXT 14 $((y+3)) "$name" "$tc" 2
            p=$(prox "$sig")
            local bc=RED; [ "$p" -gt 33 ] && bc=AMBER; [ "$p" -gt 66 ] && bc=GREEN
            UI_RECT 300 $((y+2)) $(( p*120/100 )) 12 "$bc"
            UI_TEXT 430 $((y+3)) "$sig" "$tc" 1
        done
```

For each AP we split the `signal|mac|channel|ssid` line back apart, use the SSID (or
the last 8 chars of the MAC if it's hidden), and draw a row: a highlight bar behind
the selected one (`UI_RECT ... "$UI_SELECT"`), the name, a **signal bar whose length
and colour come from `prox`** (`UI_RECT 300 y <width> 12`), and the dBm value. This
is a custom list — you control exactly how each row looks.

```sh
        UI_FOOTER "UP/DN  GREEN HUNT" "RED EXIT"
        UI_SHOW

        case "$(UI_KEY 2)" in
            UP)    sel=$(( sel>0 ? sel-1 : n-1 )) ;;
            DOWN)  sel=$(( sel<n-1 ? sel+1 : 0 )) ;;
            A)     ... hunt "$mac" ... ;;
            B)     return ;;
        esac
    done
}
```

`UI_SHOW` paints the frame. `UI_KEY 2` waits up to **2 seconds** for a button — so
the list auto-refreshes every 2 s, but reacts instantly if you press something. Up/
Down move the selection, Green starts the hunt on the selected AP, Red exits.

## 7. Screen two — the hunt

```sh
hunt() {
    local name="$2" ch="$3" history="" last=-1
    local sig p trend col tcol      # these feed \(...) in the view
    while true; do
        sig="$(signal_of "$1")"
        if [ -z "$sig" ]; then
            UI_CLEAR
            UI_HEADER "HUNTING  $name"
            UI_TEXT_CENTER 240 100 "SIGNAL LOST" AMBER 3
            ...
            UI_SHOW
            case "$(UI_KEY 1)" in B) return ;; esac
            continue
        fi
```

Poll the target's live signal. If it's gone quiet, show a "SIGNAL LOST" screen and
keep looping until it comes back (or Red backs out).

```sh
        p=$(prox "$sig")
        history="$history $p"; set -- $history; while [ $# -gt 44 ]; do shift; done; history="$*"
```

Convert to closeness, then push it onto a **rolling history** of the last 44 values.
The `set -- $history; while [ $# -gt 44 ]; do shift; done` idiom is a neat way to keep
only the newest N items of a space-separated list in pure bash.

```sh
        if   [ "$last" -lt 0 ];      then trend="LOCKED ON";  tcol=dim
        elif [ "$p" -gt "$last" ];   then trend="WARMER  >>"; tcol=green
        elif [ "$p" -lt "$last" ];   then trend="<<  COLDER"; tcol=red
        else                              trend="STEADY";     tcol=amber; fi
        last=$p
        col=red; [ "$p" -gt 33 ] && col=amber; [ "$p" -gt 66 ] && col=green
```

Compare to the previous reading for a **warmer/colder** hint, and pick the accent
colour from closeness (red → amber → green). Note the colour names are lowercase
here — they get interpolated straight into the view as `.green` / `.amber` / `.red`.

```sh
        UI_RENDER wifi_foxhunt.view

        case "$(UI_KEY 1)" in B) return ;; esac
    done
}
```

That's it — the whole hunt screen is **a view file**, `wifi_foxhunt.view`:

```swift
VStack(spacing: 6) {
  Header("HUNTING \(name)")
  HStack(spacing: 8) {
    Card("SIGNAL")    { Text("\(sig)",   .\(col),  .big) }
    Card("CLOSENESS") { Text("\(p)%",    .\(col),  .big) }
    Card("TREND")     { Text("\(trend)", .\(tcol))       }
  }.h(62)
  Meter(value: \(p), .\(col)).h(20)
  Graph("\(history)", .\(col)).h(52)
  Spacer()
  Footer("CLOSER = FULLER + GREENER", "RED  BACK")
}
```

`UI_RENDER` fills in every `\(...)` from the shell and lets the layout engine
place the three cards, the meter and the graph — **no coordinates anywhere**. The
nice trick is that the *colours* are interpolated too: `.\(col)` becomes
`.green`/`.amber`/`.red`, so the whole screen fills and greens up as you close in.
The meter fills with closeness and the graph shows your last ~40 readings, exactly
as before — but now the payload is pure logic and the look lives in a file you can
design in `builder.html`.

The dynamic AP list in screen one stays on the command API on purpose: it's a
variable-length, selectable list, and view files describe a *fixed* layout. Use
views for rich static screens, commands for things that grow and scroll.

`UI_KEY 1` refreshes once a second, Red returns to the list. That one-second loop
is why the display feels live without you managing any timers.

## 8. Kicking it off

```sh
pick_ap
UI_END
```

Run the list screen (which calls `hunt` when you pick one); when it returns, `UI_END`
releases the screen back to the firmware.

---

## Patterns worth stealing

- **Live screens:** draw → `UI_SHOW` → `UI_KEY <seconds>` in a loop. The timeout is
  your refresh rate; a button press returns immediately.
- **One number drives the visuals:** compute a 0–100 value and feed it to a meter, a
  graph height, and a colour choice. Simple and consistent.
- **Rolling history for a graph:** append, then trim with `set --`/`shift`.
- **Parse the recon API with `awk -v RS=`** when there's no python.
- **Custom lists** (not just `UI_MENU`) when you want bars, colours and live data per
  row — you draw each row yourself.
