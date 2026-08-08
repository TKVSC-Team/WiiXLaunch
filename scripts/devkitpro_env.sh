# Locates devkitPro for the Linux build scripts. Source this, don't execute it.
#
# Resolution order:
#   1. An already-set DEVKITPRO (the standard variable on Linux, usually
#      exported by /etc/profile.d/devkit-env.sh)
#   2. /opt/devkitpro (the dkp-pacman default)
#
# Exports DEVKITPRO/DEVKITPPC/DEVKITA64 and puts devkitPro's tools on PATH
# (elf2rpl and friends). Sets DKP_PPC_GXX for the Cemu build.

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"

if [ ! -d "$DEVKITPRO" ]; then
    echo "[WiiXLaunch] devkitPro not found at $DEVKITPRO." >&2
    echo "[WiiXLaunch] Install it via dkp-pacman (https://devkitpro.org/wiki/Getting_Started)" >&2
    echo "[WiiXLaunch] or export DEVKITPRO to your install directory and retry." >&2
    return 1 2>/dev/null || exit 1
fi

export DEVKITPPC="${DEVKITPPC:-$DEVKITPRO/devkitPPC}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/tools/bin:$PATH"

DKP_PPC_GXX="$DEVKITPPC/bin/powerpc-eabi-g++"
