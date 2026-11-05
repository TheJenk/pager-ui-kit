# ui.sh - draw custom screens and menus in your Pager payloads, the easy way.
#
#   . /path/to/ui.sh
#   UI_BEGIN
#   choice="$(UI_MENU "Main Menu" "Scan" "Report" "Settings" "Exit")"
#   UI_MESSAGE "Done" "You chose:" "$choice"
#   UI_END
#
# No framebuffer, no font, no C - the bundled renderer does all of that. Commands
# are UPPERCASE so they read like the rest of a payload. Colors can be names
# (RED, GREEN, BLUE, AMBER, ...) or a plain RRGGBB hex.

UI_DIR="${UI_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)}"
UI_BIN="${UI_BIN:-$UI_DIR/pguid}"
UI_VIEWC="${UI_VIEWC:-$UI_DIR/pgviewc}"
_UI_CMD=/tmp/pgui.cmd
_UI_EVT=/tmp/pgui.evt
_UI_W=480; _UI_H=222

# ---- palette (override any of these before UI_BEGIN to reskin) ----
UI_BG=0e1016; UI_BAR=1a2234; UI_LINE=2b3a52
UI_TEXT=e8ecf3; UI_DIM=7f8aa0; UI_ACCENT=5aa0dc
UI_SELECT=2f7d57; UI_SELECT_TEXT=ffffff

# resolve a color name or RRGGBB hex -> hex (default = body text color).
# Portable: no bash-4 case-conversion, works under any /bin/bash or busybox.
_col() {
    local c u
    c="${1:-}"
    u="$(printf '%s' "$c" | tr '[:lower:]' '[:upper:]')"
    case "$u" in
        WHITE) echo ffffff;; BLACK) echo 000000;;
        GREY|GRAY|DIM) echo "$UI_DIM";; TEXT) echo "$UI_TEXT";; ACCENT) echo "$UI_ACCENT";;
        RED) echo e0483f;; GREEN) echo 2fae6a;; BLUE) echo 3a86c8;; CYAN) echo 3ac8c8;;
        YELLOW) echo e0c23f;; AMBER) echo e0a53f;; ORANGE) echo e07b3f;;
        PURPLE) echo 9b7ede;; PINK) echo e06ba0;;
        "") echo "${2:-$UI_TEXT}";;
        [0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]) echo "$c";;   # RRGGBB (either case)
        *) echo "${2:-$UI_TEXT}";;
    esac
}

# ---- session ----
UI_BEGIN() {
    rm -f "$_UI_CMD" "$_UI_EVT"; mkfifo "$_UI_CMD" "$_UI_EVT"
    command -v UI_TAKEOVER >/dev/null 2>&1 && UI_TAKEOVER
    "$UI_BIN" "$_UI_CMD" "$_UI_EVT" & _UI_PID=$!
    exec 4>"$_UI_CMD"; exec 3<"$_UI_EVT"
    local r; read -r r <&3                 # wait for READY
    trap UI_END EXIT INT TERM
}
UI_END() {
    trap - EXIT INT TERM
    [ -n "$_UI_PID" ] && { echo "quit" >&4 2>/dev/null; sleep 0.1; kill "$_UI_PID" 2>/dev/null; }
    exec 4>&- 2>/dev/null; exec 3<&- 2>/dev/null
    rm -f "$_UI_CMD" "$_UI_EVT"
    command -v UI_RELEASE >/dev/null 2>&1 && UI_RELEASE
    _UI_PID=""
}

# ---- drawing primitives ----
UI_CLEAR() { echo "clear $(_col "${1:-}" "$UI_BG")" >&4; }
UI_RECT()  { echo "rect $1 $2 $3 $4 $(_col "${5:-}")" >&4; }                 # filled
UI_BOX()   { echo "frame $1 $2 $3 $4 $(_col "${5:-}" "$UI_LINE")" >&4; }     # outline
UI_BAR()   { echo "rect $1 $2 $3 $4 $(_col "${5:-}" "$UI_ACCENT")" >&4; }    # filled bar
# UI_TEXT x y "text" [color] [size 1-4]
UI_TEXT()  { echo "text $1 $2 ${5:-2} $(_col "${4:-}") $3" >&4; }
UI_SHOW()  { echo "present" >&4; }

_tw() { echo $(( ${#1} * 6 * ${2:-2} - ${2:-2} )); }   # text width in px
# UI_TEXT_CENTER cx y "text" [color] [size]
UI_TEXT_CENTER() { UI_TEXT $(( $1 - $(_tw "$3" "${5:-2}")/2 )) "$2" "$3" "${4:-}" "${5:-2}"; }

# ---- layout widgets ----
UI_HEADER() {                                   # title bar across the top
    UI_RECT 0 0 $_UI_W 26 "$UI_BAR"; UI_RECT 0 26 $_UI_W 1 "$UI_LINE"
    UI_TEXT 10 6 "$1" ACCENT 2
}
UI_FOOTER() {                                   # bottom hints: left, right
    UI_RECT 0 $((_UI_H-16)) $_UI_W 1 "$UI_LINE"
    [ -n "$1" ] && UI_TEXT 8 $((_UI_H-12)) "$1" DIM 1
    [ -n "$2" ] && UI_TEXT $(( _UI_W-8-$(_tw "$2" 1) )) $((_UI_H-12)) "$2" DIM 1
}
UI_CARD() {                                     # panel x y w h [Title]
    UI_RECT "$1" "$2" "$3" "$4" "$UI_BAR"; UI_BOX "$1" "$2" "$3" "$4" "$UI_LINE"
    [ -n "$5" ] && UI_TEXT $(( $1+10 )) $(( $2+8 )) "$5" TEXT 2
}
# UI_METER x y w h percent [color]   - a 0-100 progress/level bar
UI_METER() {
    local x=$1 y=$2 w=$3 h=$4 pct=$5 col=${6:-GREEN} fw
    UI_BOX "$x" "$y" "$w" "$h" "$UI_LINE"
    fw=$(( (w-2) * pct / 100 )); [ "$fw" -lt 0 ] && fw=0; [ "$fw" -gt $((w-2)) ] && fw=$((w-2))
    [ "$fw" -gt 0 ] && UI_BAR $((x+1)) $((y+1)) "$fw" $((h-2)) "$col"
}
# UI_GRAPH x y w h "v1 v2 v3 ..." [color]   - auto-scaled bar graph of a series
UI_GRAPH() {
    local x=$1 y=$2 w=$3 h=$4 col=${6:-ACCENT}
    set -- $5
    local n=$# max=1 v bw gap=1 i=0 bh
    [ "$n" -gt 0 ] || return
    for v in "$@"; do [ "$v" -gt "$max" ] 2>/dev/null && max=$v; done
    bw=$(( w / n )); [ "$bw" -lt 1 ] && bw=1
    [ "$bw" -gt 2 ] && bw=$((bw-gap))
    for v in "$@"; do
        bh=$(( v * (h-1) / max )); [ "$bh" -lt 1 ] && bh=1
        UI_BAR $(( x + i*(bw+gap) )) $(( y + h - bh )) "$bw" "$bh" "$col"
        i=$((i+1))
    done
}

# ---- view files (SwiftUI-style layouts) ----
# UI_RENDER file.view   - lay out and draw a .view file. \(var) placeholders in
# the view are filled in from the current shell variables. Followed by UI_SHOW
# implicitly (the view includes its own present).
UI_RENDER() {
    local vf="$1" content v
    [ -f "$vf" ] || vf="$UI_DIR/$1"
    [ -f "$vf" ] || { echo "[UI_RENDER] no such view: $1" >&2; return 1; }
    content="$(cat "$vf")"
    # fill \(name) -> $name
    while [[ "$content" =~ \\\(([a-zA-Z_][a-zA-Z0-9_]*)\) ]]; do
        v="${BASH_REMATCH[1]}"; content="${content//\\($v)/${!v}}"
    done
    # compile to draw commands and feed the renderer
    printf '%s' "$content" | "$UI_VIEWC" | while IFS= read -r l; do printf '%s\n' "$l" >&4; done
}

# ---- input ----
# UI_KEY [timeout_s]  -> UP / DOWN / LEFT / RIGHT / A / B / POWER (A=Green, B=Red)
UI_KEY() {
    local line
    if [ -n "$1" ]; then read -r -t "$1" line <&3 || { echo ""; return 1; }
    else read -r line <&3; fi
    echo "${line#KEY }"
}

# ---- one-line screens ----
# UI_MENU "Title" item1 item2 ...  -> echoes the chosen item (exit 0),
#   or exits 1 if the user backs out. Up/Down move, Green picks, Red backs out.
UI_MENU() {
    local title="$1"; shift
    local items=("$@") n=${#items[@]} sel=0 top=0 rowh=24 rows=7 ly=34 i y idx
    while true; do
        [ "$sel" -lt "$top" ] && top=$sel
        [ "$sel" -ge $((top+rows)) ] && top=$((sel-rows+1))
        UI_CLEAR; UI_HEADER "$title"
        for ((i=0; i<rows && top+i<n; i++)); do
            idx=$((top+i)); y=$((ly + i*rowh))
            if [ "$idx" -eq "$sel" ]; then
                UI_RECT 6 "$y" $((_UI_W-12)) $((rowh-4)) "$UI_SELECT"
                UI_TEXT 16 $((y+4)) "${items[$idx]}" "$UI_SELECT_TEXT" 2
            else
                UI_TEXT 16 $((y+4)) "${items[$idx]}" TEXT 2
            fi
        done
        [ "$top" -gt 0 ] && UI_TEXT $((_UI_W-16)) 32 "^" DIM 1
        [ $((top+rows)) -lt "$n" ] && UI_TEXT $((_UI_W-16)) $((_UI_H-26)) "v" DIM 1
        UI_FOOTER "GREEN  SELECT" "RED  BACK"; UI_SHOW
        case "$(UI_KEY)" in
            UP)   sel=$(( sel>0 ? sel-1 : n-1 )) ;;
            DOWN) sel=$(( sel<n-1 ? sel+1 : 0 )) ;;
            A)    echo "${items[$sel]}"; return 0 ;;
            B)    return 1 ;;
        esac
    done
}

# UI_MESSAGE "Title" "line one" "line two" ...  -> show, wait for any key
UI_MESSAGE() {
    local title="$1"; shift
    UI_CLEAR; UI_HEADER "$title"
    local y=$(( (_UI_H-26)/2 - (${#}*11)/2 + 26 )) ln
    for ln in "$@"; do UI_TEXT_CENTER $((_UI_W/2)) "$y" "$ln" TEXT 2; y=$((y+22)); done
    UI_FOOTER "" "ANY KEY"; UI_SHOW
    UI_KEY >/dev/null
}

# UI_CONFIRM "Question?"  -> exit 0 if Green (yes), 1 if Red (no)
UI_CONFIRM() {
    UI_CLEAR; UI_HEADER "CONFIRM"
    UI_TEXT_CENTER $((_UI_W/2)) 90 "$1" TEXT 2
    UI_TEXT_CENTER $((_UI_W/2)) 150 "GREEN = YES    RED = NO" DIM 1
    UI_FOOTER "" ""; UI_SHOW
    while true; do case "$(UI_KEY)" in A) return 0;; B) return 1;; esac; done
}
