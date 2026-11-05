#!/bin/bash
# Copy the kit (ui.sh, pguid, and pgviewc where a demo uses view files) into each
# demo folder, so every demo is a self-contained, installable folder - scp it to
# the Pager and run it, nothing else needed. Run this after build.sh (build.sh
# calls it automatically) so the copies never drift from the source at the repo
# root. The source of truth is always ../pguid, ../pgviewc, ../ui.sh.
set -e
cd "$(dirname "$0")"
ROOT=..
for d in */; do
    d="${d%/}"
    [ -f "$d/payload.sh" ] || continue
    cp "$ROOT/ui.sh"   "$d/ui.sh"
    cp "$ROOT/pguid"   "$d/pguid"
    # pgviewc only matters for demos that render .view files
    if ls "$d"/*.view >/dev/null 2>&1; then cp "$ROOT/pgviewc" "$d/pgviewc"; else rm -f "$d/pgviewc"; fi
    echo "$d: $(ls "$d" | tr '\n' ' ')"
done
