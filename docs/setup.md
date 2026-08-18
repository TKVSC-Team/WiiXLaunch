# Setting Up

[« Back to overview](overview.md)

## Prerequisites

* **Python 3** - runs `scripts/generate_config.py` (turns `wiixlaunch.json` into the generated headers each target build reads) and `scripts/deploy.py` (packages build output).
* **devkitPro**, with the `devkitPPC` and `devkitA64` toolchains - required for Wii U, Cemu, and Switch builds. Install from [devkitpro.org](https://devkitpro.org/wiki/Getting_Started).
  * On Windows, if devkitPro isn't installed at the default `C:\devkitPro`, set `DEVKITPRO_WIN` to your install directory before building.
* **Some form of Visual Studio (20XX)** (Windows only) - only needed if you're also building host-side tools like `tools/ring_log_reader` (see [Debugging](debugging.md)); the console mods themselves don't need it.
* For Wii U: WUPS, libfunctionpatcher, and libnotifications installed into your devkitPro environment. All three ship as submodules under `vendor/`. On Linux, `scripts/setup_wiiu_deps.sh` builds and installs all three for you; on other platforms see their READMEs for `make install` instructions into `/opt/devkitpro`. libnotifications is what powers the on-screen toasts described in [Debugging](debugging.md).

## Getting the source

The vendored dependencies (exlaunch, wut, WUPS, libfunctionpatcher) are git submodules:

```bash
git clone --recurse-submodules <your-fork-url>
```

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

## Configuring your mod

Everything project-specific lives in [`wiixlaunch.json`](../wiixlaunch.json) at the repo root:

* `project` - name, version, author, description, and `debug` (controls `EXL_DEBUG` on the Switch build).
* `memory` - heap/JIT/inline-pool sizes and the Cemu debug log buffer size.
* `switch` - title ID, subsdk name, thread stack size/priority.
* `wiiu` - the plugin's `.wps` filename and the target title IDs it patches.
* `cemu` - the entry hook address, graphic pack path/version for your target game build, and `module_matches` (the `moduleMatches` field written into the generated `.asm` patch). usually doesn't require changing, V208 is the only launchable version from my understanding.

Running `python scripts/generate_config.py` (the build scripts do this for you) turns this into generated headers under `build/generated/include/` and platform config files under `build/generated/switch/` - these are regenerated on every build, so edit `wiixlaunch.json`, not the generated files.

## Building

Convenience scripts (Windows, there are Linux equivalents available as well):

```bash
build_switch.bat (builds switch, deploys all)
build_wiiu.bat (builds wiiu, deploys all)
build_cemu.bat (builds cemu, deploys all)
build_all.bat (builds all, deploys all)
```

Or via CMake directly (Switch only builds this way; Wii U and Cemu use devkitPPC/the WUPS Makefile flow directly, since their output isn't a normal CMake executable target (see `build_wiiu.bat` and `build_cemu.bat`):

```bash
cmake -B build/switch -DPLATFORM=SWITCH
cmake --build build/switch
```

## Deploying

```bash
python scripts/deploy.py
```

Packages whatever you've built into `deploy/`:

* **Switch**: `deploy/switch/atmosphere/contents/<title_id>/exefs/`
* **Wii U**: `deploy/wiiu/wiiu/environments/aroma/plugins/<mod_name>.wps`
* **Cemu**: `deploy/cemu/graphicPacks/<graphic_pack_name>/`

Copy the relevant folder onto your SD card, place it in your emulator as a regular exefs mod, or place the mod in Cemus graphic pack folder.
