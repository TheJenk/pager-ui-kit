#!/bin/bash
# Build the pguid renderer for the Pager (FPU-less mipsel, freestanding, no libc).
# Toolchain: brew install messense/macos-cross-toolchains/mipsel-unknown-linux-gnu
set -e
cd "$(dirname "$0")"
CC="${CC:-mipsel-unknown-linux-gnu-gcc}"
"$CC" -O2 -fno-pic -mno-abicalls -G0 -nostdlib -ffreestanding -static -s \
    -Wl,-e,_start -o pguid pguid.c
file pguid
ls -lh pguid | awk '{print "size:", $5}'
shasum -a 256 pguid
