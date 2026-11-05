#!/bin/bash
# Title: WiFi Foxhunt
# Author: Jenx
# Description: Pick a nearby access point, then "hunt" it - a proximity meter and
#   signal-history graph fill up as you walk closer (stronger RSSI). Built entirely
#   with the Pager UI Kit (ui.sh + the prebuilt renderer); all UI is bash.
# Version: 1.0

# ---- find the UI kit next to this payload (works from a /tmp copy too) --------
for d in "${_PAYLOAD_HOME:-}" "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)" \
         "/root/payloads/user/general/WiFi_Foxhunt" "/mmc/ui-kit"; do
    [ -n "$d" ] && [ -f "$d/ui.sh" ] && { KIT="$d"; break; }
done
[ -z "$KIT" ] && { LOG red "ui.sh not found"; exit 1; }
. "$KIT/ui.sh"

API=/tmp/api.sock
DEVS="http://localhost/api/recon/wifi/devices?pagesize=80&sort=signal"

# ---- data: parse access points out of the recon API (no python on-device) -----
# One record per device, split on "mac":" . For each access point we pull the
# signal, channel and (first) SSID. Output: "signal|mac|channel|ssid", strongest
# signal first.
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

# live signal (dBm) of one MAC, or empty if it's gone quiet
signal_of() {
    curl -s -m3 --unix-socket "$API" "$DEVS" | awk -v m="$1" '
        BEGIN { RS="\"mac\":\"" }
        NR>1 && substr($0,1,17)==m {
            if (match($0,/"signal":-?[0-9]+/)) print substr($0,RSTART+9,RLENGTH-9); exit
        }'
}

# RSSI (-90..-40 dBm) -> proximity percent (0..100)
prox() { local s=$1 p=$(( (s + 90) * 100 / 50 )); [ $p -lt 0 ] && p=0; [ $p -gt 100 ] && p=100; echo $p; }
# a label for a signal bar's length in the list
barw() { local p; p=$(prox "$1"); echo $(( p * 150 / 100 )); }

UI_BEGIN

# ============================ screen 1: the AP list ============================
pick_ap() {
    local sel=0 lines=() n i y p name sig mac ch ss tc bc l
    while true; do
        lines=(); while IFS= read -r l; do lines+=("$l"); done < <(scan_aps)
        n=${#lines[@]}
        [ "$sel" -ge "$n" ] && sel=$(( n>0 ? n-1 : 0 ))

        UI_CLEAR
        UI_HEADER "WIFI FOXHUNT"
        UI_TEXT $((480-96)) 8 "$n NEARBY" DIM 1
        if [ "$n" -eq 0 ]; then
            UI_TEXT_CENTER 240 110 "SCANNING FOR APS..." DIM 2
        fi
        for ((i=0; i<n; i++)); do
            y=$(( 34 + i*23 ))
            IFS='|' read -r sig mac ch ss <<< "${lines[$i]}"
            name="${ss:-${mac: -8}}"
            [ "$i" -eq "$sel" ] && UI_RECT 6 $((y-2)) 468 22 "$UI_SELECT"
            local tc=TEXT; [ "$i" -eq "$sel" ] && tc=WHITE
            UI_TEXT 14 $((y+3)) "$name" "$tc" 2
            # signal bar + dBm on the right
            p=$(prox "$sig")
            local bc=RED; [ "$p" -gt 33 ] && bc=AMBER; [ "$p" -gt 66 ] && bc=GREEN
            UI_RECT 300 $((y+2)) $(( p*120/100 )) 12 "$bc"
            UI_TEXT 430 $((y+3)) "$sig" "$tc" 1
        done
        UI_FOOTER "UP/DN  GREEN HUNT" "RED EXIT"
        UI_SHOW

        case "$(UI_KEY 2)" in           # auto-rescan every 2s, or react to a key
            UP)    [ "$n" -gt 0 ] && sel=$(( sel>0 ? sel-1 : n-1 )) ;;
            DOWN)  [ "$n" -gt 0 ] && sel=$(( sel<n-1 ? sel+1 : 0 )) ;;
            A)     [ "$n" -gt 0 ] && { IFS='|' read -r sig mac ch ss <<< "${lines[$sel]}"; hunt "$mac" "${ss:-${mac: -8}}" "$ch"; } ;;
            B)     return ;;
        esac
    done
}

# ============================ screen 2: the hunt ==============================
# This screen is a VIEW FILE (wifi_foxhunt.view) - a SwiftUI-style layout. We
# only compute the numbers and colors; the engine lays out the cards, meter and
# graph. The colors are interpolated too, so the whole screen "greens up" as you
# close in. (The AP list above stays on the command API because it's a dynamic,
# variable-length, selectable list - views describe a fixed layout.)
hunt() {
    local name="$2" ch="$3" history="" last=-1
    local sig p trend col tcol      # <- these feed \(...) in the view
    while true; do
        sig="$(signal_of "$1")"
        if [ -z "$sig" ]; then
            UI_CLEAR
            UI_HEADER "HUNTING  $name"
            UI_TEXT_CENTER 240 100 "SIGNAL LOST" AMBER 3
            UI_TEXT_CENTER 240 140 "KEEP MOVING..." DIM 1
            UI_FOOTER "" "RED  BACK"
            UI_SHOW
            case "$(UI_KEY 1)" in B) return ;; esac
            continue
        fi
        p=$(prox "$sig")
        history="$history $p"; set -- $history; while [ $# -gt 44 ]; do shift; done; history="$*"

        # warmer / colder vs the last reading
        if   [ "$last" -lt 0 ];      then trend="LOCKED ON";  tcol=dim
        elif [ "$p" -gt "$last" ];   then trend="WARMER  >>"; tcol=green
        elif [ "$p" -lt "$last" ];   then trend="<<  COLDER"; tcol=red
        else                              trend="STEADY";     tcol=amber; fi
        last=$p

        col=red; [ "$p" -gt 33 ] && col=amber; [ "$p" -gt 66 ] && col=green

        UI_RENDER wifi_foxhunt.view      # fills in \(name) \(sig) \(p) \(col) ...

        case "$(UI_KEY 1)" in B) return ;; esac
    done
}

pick_ap
UI_END
