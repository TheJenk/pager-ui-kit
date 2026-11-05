#!/bin/bash
# Title: UI Kit Demo
# Author: Jenx
# Description: Custom Pager screens and menus built in a few lines of bash with
#   ui.sh - a dashboard, a menu, a message and a confirm, no framebuffer code.
# Version: 1.0

# find ui.sh next to this payload (works when launched from a /tmp copy)
for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/UI_Kit_Demo" "/mmc/ui-kit"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"

UI_BEGIN

dashboard() {
    UI_CLEAR
    UI_HEADER "FIELD TOOLKIT"
    UI_CARD 10 40 225 70;  UI_TEXT 22 52 "UPLINK"  DIM 1;  UI_TEXT 22 68 "ONLINE" BLUE  3
    UI_CARD 245 40 225 70; UI_TEXT 257 52 "TARGETS" DIM 1; UI_TEXT 257 68 "42"     GREEN 3
    UI_CARD 10 120 460 70 "SIGNAL"
    local i h
    for i in 0 1 2 3 4 5 6 7 8 9; do
        h=$(( 8 + i*4 ))
        UI_BAR $((24 + i*44)) $((178 - h)) 30 "$h" "$([ "$i" -gt 6 ] && echo AMBER || echo BLUE)"
    done
    UI_FOOTER "GREEN  OPEN MENU" "RED  EXIT"
    UI_SHOW
}

while true; do
    dashboard
    case "$(UI_KEY)" in
        A)
            case "$(UI_MENU "Main Menu" "Start scan" "View targets" "Settings" "About" "Exit")" in
                "Start scan")   UI_CONFIRM "Begin the scan?" && UI_MESSAGE "Scanning" "Sweeping..." "(demo)" ;;
                "View targets") UI_MESSAGE "Targets" "42 devices seen" "12 access points" ;;
                "Settings")     UI_MESSAGE "Settings" "Nothing to set" "in this demo" ;;
                "About")        UI_MESSAGE "About" "Built with ui.sh" "Custom UI, pure bash" ;;
                "Exit"|"")      UI_END; exit 0 ;;
            esac ;;
        B) UI_END; exit 0 ;;
    esac
done
