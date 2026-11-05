#!/bin/bash
# mkimg.sh - turn an image into a .pgi the Pager renderer can blit.
#
#   tools/mkimg.sh logo.png logo.pgi [WIDTH] [HEIGHT]
#
# A .pgi is dead simple: a 4-byte header (uint16 width, uint16 height, both
# little-endian) followed by width*height RGB565 little-endian pixels - exactly
# the Pager's pixel format, so pguid just reads and blits it, no decoding.
#
# Give a WIDTH/HEIGHT to resize (keep images small - they're uncompressed:
# 64x64 = 8 KB, a full 480x222 = ~208 KB). With only a WIDTH, height scales to
# keep the aspect ratio. With neither, the image's own size is used.
#
# Needs ffmpeg:  brew install ffmpeg
set -e
src="$1"; out="$2"; w="$3"; h="$4"
[ -n "$src" ] && [ -n "$out" ] || { echo "usage: mkimg.sh in.png out.pgi [W] [H]" >&2; exit 2; }
[ -f "$src" ] || { echo "no such file: $src" >&2; exit 1; }

# source dimensions
SW="$(ffprobe -v error -select_streams v:0 -show_entries stream=width  -of csv=p=0 "$src" 2>/dev/null | tr -dc 0-9)"
SH="$(ffprobe -v error -select_streams v:0 -show_entries stream=height -of csv=p=0 "$src" 2>/dev/null | tr -dc 0-9)"
[ -n "$SW" ] && [ -n "$SH" ] || { echo "could not read image size" >&2; exit 1; }

# final dimensions
if [ -n "$w" ] && [ -n "$h" ]; then OW="$w"; OH="$h"
elif [ -n "$w" ]; then OW="$w"; OH=$(( (SH * w + SW/2) / SW ))
else OW="$SW"; OH="$SH"; fi

tmp="$(mktemp -t pgi).raw"
trap 'rm -f "$tmp"' EXIT
ffmpeg -v error -y -i "$src" -vf "scale=$OW:$OH,format=rgb565le" -f rawvideo -pix_fmt rgb565le "$tmp"

# write the 4-byte little-endian header, then the pixels
printf "$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' $((OW&255)) $((OW>>8)) $((OH&255)) $((OH>>8)))" > "$out"
cat "$tmp" >> "$out"
echo "$out: ${OW}x${OH}  $(wc -c < "$out" | tr -d ' ') bytes"
