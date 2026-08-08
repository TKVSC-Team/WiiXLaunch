#!/bin/bash
# Linux equivalent of build_cemu.bat: one direct devkitPPC compile+link, no
# CMake. No -g: the payload ships as a raw binary, and debug sections would
# add .rela.debug_* entries the deploy-time relocator must ignore.
set -e
cd "$(dirname "$0")"
source scripts/devkitpro_env.sh

echo "Generating config..."
python3 scripts/generate_config.py

mkdir -p build

echo "Building Cemu payload (PowerPC)..."
"$DKP_PPC_GXX" \
  -std=gnu++20 -fPIE -msdata=none \
  -D__CEMU__=1 -DWIIXL_CEMU=1 \
  -I include -I build/generated/include \
  -nostartfiles -T scripts/cemu.ld -Wl,-q \
  src/main.cpp src/wiiu_plugin.cpp \
  -o build/wiixlaunch_cemu

python3 scripts/deploy.py
echo "Cemu build complete!"
