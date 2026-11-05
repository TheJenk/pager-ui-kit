#!/bin/bash
# Title: View Demo
# Author: Jenx
# Description: A screen built from a SwiftUI-style .view file - the payload just
#   sets variables and calls UI_RENDER. Shows a live tick counter + battery.
# Version: 1.0
for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/View_Demo"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"

UI_BEGIN
n=0
while true; do
    n=$((n+1))
    batt=$(cat /sys/class/power_supply/bq27546-0/capacity 2>/dev/null); : "${batt:=?}"
    UI_RENDER "$KIT/demo.view"          # draws the view, filling in \(n) and \(batt)
    case "$(UI_KEY 1)" in B) break ;; esac
done
UI_END
