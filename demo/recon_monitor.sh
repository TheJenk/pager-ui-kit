#!/bin/bash
# Title: Recon Monitor
# Author: Jenx
# Description: A live Wi-Fi recon dashboard built with ui.sh - APs, clients, a
#   scrolling packets/sec graph and a battery meter, refreshing once a second.
# Version: 1.0

for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/Recon_Monitor" "/mmc/ui-kit"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"

API=/tmp/api.sock
jget() { echo "$1" | grep -o "\"$2\":[0-9]*" | head -1 | cut -d: -f2; }

UI_BEGIN

history=""        # rolling packets/sec values for the graph
HMAX=46           # how many bars fit

while true; do
    # --- pull live data ---
    s=$(curl -s -m2 --unix-socket "$API" http://localhost/api/recon/wifi/status)
    pps=$(jget "$s" pps);  aps=$(jget "$s" aps);  devs=$(jget "$s" devs)
    : "${pps:=0}" "${aps:=0}" "${devs:=0}"
    batt=$(cat /sys/class/power_supply/bq27546-0/capacity 2>/dev/null); : "${batt:=0}"
    history="$history $pps"
    set -- $history; while [ $# -gt $HMAX ]; do shift; done; history="$*"

    # --- draw the screen ---
    UI_CLEAR
    UI_HEADER "RECON MONITOR"
    UI_TEXT $((480-70)) 8 "BATT $batt%" DIM 1

    UI_CARD 10 34 148 60;  UI_TEXT 22 44 "ACCESS PTS" DIM 1; UI_TEXT 22 62 "$aps"  BLUE  3
    UI_CARD 166 34 148 60; UI_TEXT 178 44 "CLIENTS"   DIM 1; UI_TEXT 178 62 "$devs" GREEN 3
    UI_CARD 322 34 148 60; UI_TEXT 334 44 "PKT/SEC"   DIM 1; UI_TEXT 334 62 "$pps"  AMBER 3

    UI_CARD 10 102 460 76 "PACKETS / SEC"
    UI_GRAPH 20 126 440 46 "$history" ACCENT

    # battery level meter in the footer area
    UI_METER 10 186 120 10 "$batt" "$([ "$batt" -lt 20 ] && echo RED || echo GREEN)"
    UI_TEXT 420 186 "RED EXIT" DIM 1
    UI_SHOW

    # refresh every ~1s, but react instantly to a button
    case "$(UI_KEY 1)" in
        B) UI_END; exit 0 ;;
        A) history="" ;;        # Green clears the graph
    esac
done
