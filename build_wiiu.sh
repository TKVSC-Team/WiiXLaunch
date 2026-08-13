#!/bin/bash
# Linux equivalent of build_wiiu.bat.
#
# Requires wut + wut-tools from dkp-pacman, plus WUPS, libfunctionpatcher and
# libnotifications installed into $DEVKITPRO - run scripts/setup_wiiu_deps.sh
# once to build those from the vendor/ submodules.
set -e
cd "$(dirname "$0")"
source scripts/devkitpro_env.sh

echo "Generating config..."
python3 scripts/generate_config.py

# devkitPro's make rules cannot handle spaces in paths, so stage the build in
# a temp dir (space-free) instead of building in-place. See scripts/wiiu/Makefile.
STAGE="${TMPDIR:-/tmp}/wiixlaunch-wiiu"
echo "Preparing Wii U build environment in $STAGE..."
rm -rf "$STAGE"
mkdir -p "$STAGE" "$STAGE/generated"
cp -r src "$STAGE/src"
cp -r include "$STAGE/include"
cp -r build/generated/include "$STAGE/generated/include"

# Optional WiiXLaunch modules (e.g. vendor/wiixlaunch-botw) - not part of
# base WiiXLaunch, staged here if this mod added one as a submodule
# (git submodule add <url> vendor/wiixlaunch-<name>). scripts/wiiu/Makefile
# picks up modules/*/include automatically.
for d in vendor/wiixlaunch-*/; do
  name="$(basename "$d")"
  [ -d "${d}include" ] && mkdir -p "$STAGE/modules/$name" && cp -r "${d}include" "$STAGE/modules/$name/include"
done

cp scripts/wiiu/Makefile "$STAGE/Makefile"

echo "Building for Wii U (PowerPC)..."
make -C "$STAGE"

mkdir -p build/wiiu
cp "$STAGE/BotW_SampleMod.wps" build/wiiu/BotW_SampleMod.wps

python3 scripts/deploy.py
echo "Wii U build complete!"
