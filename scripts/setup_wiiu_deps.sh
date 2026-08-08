#!/bin/bash
# One-time setup for the Wii U build on Linux: builds WUPS, libfunctionpatcher
# and libnotifications from the vendor/ submodules and installs them into
# $DEVKITPRO (wups -> $DEVKITPRO/wups, the other two -> $DEVKITPRO/wums).
#
# These are not available via dkp-pacman - only wut and wut-tools are; install
# those first: sudo dkp-pacman -S wut wut-tools
#
# If $DEVKITPRO is root-owned (the default pacman install), run this with sudo:
#   sudo -E scripts/setup_wiiu_deps.sh
set -e
cd "$(dirname "$0")/.."
source scripts/devkitpro_env.sh

for lib in wups libfunctionpatcher libnotifications; do
    if [ ! -f "vendor/$lib/Makefile" ]; then
        echo "[WiiXLaunch] vendor/$lib is missing - run: git submodule update --init" >&2
        exit 1
    fi
    # Stage to a space-free temp dir: devkitPro make rules break on spaces,
    # and the repo may live in a path that contains them.
    STAGE="${TMPDIR:-/tmp}/wiixlaunch-dep-$lib"
    echo "=== Building $lib ==="
    rm -rf "$STAGE"
    cp -r "vendor/$lib" "$STAGE"
    rm -rf "$STAGE/.git"
    make -C "$STAGE"
    make -C "$STAGE" install
    rm -rf "$STAGE"
done

echo "Wii U dependencies installed into $DEVKITPRO."
