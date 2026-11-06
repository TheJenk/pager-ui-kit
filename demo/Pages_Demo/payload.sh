#!/bin/bash
# Title: UI Kit Pages
# Author: Jenx
# Description: Flip through live dashboard PAGES with Left/Right - each page is a
#   .view file, the loop just sets variables and UI_RENDERs the current one. Green
#   flashes an alert banner drawn on top of the view with the command API. This is
#   the exact pattern the Wardrive HUD uses (view pages + a live loop + an overlay).
# Version: 1.0

# find the kit next to this payload (works from a /tmp copy too)
for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/Pages_Demo"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { command -v LOG >/dev/null && LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"

API=/tmp/api.sock
BATT=/sys/class/power_supply/bq27546-0/capacity

# an alert banner, drawn ON TOP of whatever page is showing (then re-presented)
alert_banner(){
    UI_RECT 40 80 400 58 e9b900; UI_BOX 40 80 400 58 000000; UI_BOX 42 82 396 54 000000
    UI_TEXT 96 96 "ALERT OVERLAY" 000000 3
    UI_TEXT 150 124 "DRAWN ON THE VIEW" 302200 1
    UI_SHOW
}

UI_BEGIN
trap UI_END EXIT INT TERM
PAGES=(wifi system about); NP=${#PAGES[@]}; pg=0; tick=0
aps=0; devs=0; pps=0; alert_until=0

while true; do
    now=$EPOCHSECONDS
    # live Wi-Fi recon numbers (throttled - a curl every tick is wasteful)
    if [ $(( tick % 2 )) -eq 0 ]; then
        read p a d <<<"$(curl -s -m2 --unix-socket "$API" "http://localhost/api/recon/wifi/status" 2>/dev/null \
            | sed -n 's/.*"pps":\([0-9]*\),"total":[0-9]*,"aps":\([0-9]*\),"devs":\([0-9]*\).*/\1 \2 \3/p')"
        [ -n "$a" ] && { pps=$p; aps=$a; devs=$d; }
    fi
    batt=$(<"$BATT" 2>/dev/null); : "${batt:=0}"
    bcol=green; [ "$batt" -le 50 ] && bcol=amber; [ "$batt" -le 20 ] && bcol=red
    read u _ < /proc/uptime; u=${u%.*}; printf -v up '%dh %02dm' $(( u/3600 )) $(( (u%3600)/60 ))
    page="$((pg+1))/$NP"

    UI_RENDER "${PAGES[$pg]}.view"
    [ "$now" -lt "$alert_until" ] && alert_banner      # command overlay on the view

    case "$(UI_KEY 1)" in
        LEFT)  pg=$(( (pg+NP-1)%NP )) ;;
        RIGHT) pg=$(( (pg+1)%NP )) ;;
        A)     alert_until=$(( now+3 )) ;;             # Green: flash the overlay
        B)     break ;;
    esac
    tick=$((tick+1))
done
