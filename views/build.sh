#!/bin/bash
# Host preview of the view engine: render a .view to a PNG (needs ffmpeg).
#   ./build.sh hunt.view        -> hunt.png
set -e; cd "$(dirname "$0")"
cc -O2 -o pgview pgview.c
v="${1:-hunt.view}"; out="${v%.view}.png"
./pgview "$v" /tmp/_v.raw
ffmpeg -loglevel error -y -f rawvideo -pixel_format rgb565le -video_size 480x222 -i /tmp/_v.raw -vf scale=960:444:flags=neighbor "$out"
echo "wrote $out"
