#!/bin/bash
# Title: UI Kit App Demo
# Author: Jenx
# Description: How to structure a REAL app with the UI Kit - one main loop that's
#   a state machine over screens. A stopwatch keeps running as you move between
#   screens (it's derived from a start timestamp, so it never pauses or resets),
#   and the battery is shown on every screen. Menu = command API (dynamic,
#   selectable); the sub-screens = view files (fixed, rich layouts).
# Version: 1.0

# ---- find the UI kit next to this payload (works from a /tmp copy too) --------
for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/App_Demo"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { command -v LOG >/dev/null && LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"

BATT=/sys/class/power_supply/bq27546-0/capacity
API=/tmp/api.sock

# ---- the live menu, drawn with the command API (selection moves, data is live).
# A command-drawn screen re-renders every tick, so the stopwatch + battery in the
# header keep updating even while you just sit here.
items=("Stopwatch" "System" "About")
draw_menu() {
    UI_CLEAR; UI_HEADER "APP DEMO"
    UI_TEXT 300 9 "SW $sw" AMBER 1            # live stopwatch, top-right
    UI_TEXT 440 9 "$batt%" "$bcol" 1          # live battery
    local i y
    for i in 0 1 2; do
        y=$(( 52 + i*36 ))
        if [ "$i" -eq "$sel" ]; then
            UI_RECT 20 $((y-4)) 440 30 "$UI_SELECT"; UI_TEXT 36 "$y" "${items[$i]}" WHITE 2
        else
            UI_TEXT 36 "$y" "${items[$i]}" TEXT 2
        fi
    done
    UI_FOOTER "UP/DN  GREEN OPEN" "RED  EXIT"; UI_SHOW
}

UI_BEGIN
screen=menu; sel=0
start=$EPOCHSECONDS          # the stopwatch origin - set ONCE, here, forever
aps=0; up="0h 0m"

while true; do
    # ---- recompute the live values every tick (fork-free: builtins only) ----
    elapsed=$(( EPOCHSECONDS - start ))
    printf -v sw '%02d:%02d' $(( elapsed/60 )) $(( elapsed%60 ))
    batt=$(<"$BATT"); [ -n "$batt" ] || batt=0
    bcol=green; [ "$batt" -le 50 ] && bcol=amber; [ "$batt" -le 20 ] && bcol=red

    case "$screen" in
      menu)
        draw_menu
        case "$(UI_KEY 1)" in
          UP)   sel=$(( sel>0 ? sel-1 : 2 )) ;;
          DOWN) sel=$(( sel<2 ? sel+1 : 0 )) ;;
          A)    case "$sel" in 0) screen=stopwatch ;; 1) screen=system ;; 2) screen=about ;; esac ;;
          B)    break ;;
        esac ;;

      stopwatch)
        # a view file - \(sw) is the SAME running timer. Leave and come back:
        # it never reset, because we only ever read EPOCHSECONDS-start.
        UI_RENDER stopwatch.view
        case "$(UI_KEY 1)" in
          A) start=$EPOCHSECONDS ;;   # Green = reset (proves you control it)
          B) screen=menu ;;
        esac ;;

      system)
        # another live screen: battery meter + a real recon number + uptime
        aps=$(curl -s -m2 --unix-socket "$API" "http://localhost/api/recon/wifi/status" \
              | sed -n 's/.*"aps":\([0-9]*\).*/\1/p'); [ -n "$aps" ] || aps=0
        read -r u _ < /proc/uptime; u=${u%.*}
        printf -v up '%dh %dm' $(( u/3600 )) $(( (u%3600)/60 ))
        UI_RENDER system.view
        case "$(UI_KEY 1)" in B) screen=menu ;; esac ;;

      about)
        UI_RENDER about.view
        case "$(UI_KEY 1)" in B) screen=menu ;; esac ;;
    esac
done
UI_END
