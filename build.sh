#!/bin/bash
# Build the pguid renderer for the Pager (FPU-less mipsel, freestanding, no libc).
# Toolchain: brew install messense/macos-cross-toolchains/mipsel-unknown-linux-gnu
set -e
cd "$(dirname "$0")"
CC="${CC:-mipsel-unknown-linux-gnu-gcc}"
FLAGS="-O2 -fno-pic -mno-abicalls -G0 -nostdlib -ffreestanding -static -s -Wl,-e,_start"
$CC $FLAGS -o pguid   pguid.c      # the raw-canvas renderer
$CC $FLAGS -o pgviewc pgviewc.c    # the .view -> draw-commands compiler
for b in pguid pgviewc; do ls -lh $b | awk -v n=$b '{print n": "$5}'; shasum -a 256 $b; done
