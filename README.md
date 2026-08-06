# WiiXLaunch

**WiiXLaunch** is a unified, cross-platform C++ hooking and executable modding framework for **Nintendo Switch** (AArch64 via ExLaunch) and **Nintendo Wii U** (PowerPC via WUPS / libfunctionpatcher).

Write your game mod logic **once in clean C++**, and build for both consoles seamlessly!

---

## Features

* **Single C++ Codebase**: Define hooks and memory patches once using ExLaunch-style syntax (`HOOK_DEFINE_REPLACE`, `Orig(...)`).
* **Unified Master Config (`wiixlaunch.json`)**: Manage all project settings, memory sizes (`JitSize`, `HeapSize`), Switch NPDM permissions (`system_resource_size`), and Wii U Title IDs in one place.
* **ExLaunch Feature Parity**: Full support for function replacement, trampolines, and raw memory patching (`CodePatch::Nop`).
* **Automated SD Deployer**: Built-in script packages artifacts directly into Atmosphere (`exefs`) and Aroma (`plugins`) SD card folder layouts.

---

## Example

```cpp
#include <wiixlaunch.hpp>

// Hook Player Damage in BotW: Switch 1.6.0 | Wii U v208
HOOK_DEFINE_REPLACE(PlayerDamageHook) {
    static void Callback(void* playerActor, int32_t damageAmount) {
        // Halve damage taken
        int32_t reducedDamage = damageAmount / 2;

        // Call original function on either console
        Orig(playerActor, reducedDamage);
    }
};

extern "C" void WiiXLaunch_Init() {
#if WIIXL_WIIU
    if (!WiiXLaunch::Backend::InitWiiUBackend()) return;
#endif

    // Install hook with (SwitchOffset, WiiUOffset)
    PlayerDamageHook::Install(0x01234560, 0x02123456);

    // Patch raw memory (NOP instruction on both platforms)
    WiiXLaunch::CodePatch::Nop(WIIXL_OFFSET(0x01990000, 0x02990000));
}
```

---

## Building

### Build for Switch
```bash
cmake -B build/switch -DPLATFORM=SWITCH
cmake --build build/switch
```

### Build for Wii U
```bash
cmake -B build/wiiu -DPLATFORM=WIIU
cmake --build build/wiiu
```

---

## Packaging & Deploying

To package your compiled binaries into SD card folder structures:

```bash
python scripts/deploy.py
```

This creates the output in `deploy/`:
* **Switch**: `deploy/switch/atmosphere/contents/<title_id>/exefs/` (`subsdk9` + `main.npdm`)
* **Wii U**: `deploy/wiiu/wiiu/environments/aroma/plugins/` (`<mod_name>.wps`)

---

## License

Licensed under the GNU General Public License v3.0 (GPL-3.0).

---

## Credits

WiiXLaunch builds upon and integrates the foundational work of the following open-source projects and developers:

* **[ExLaunch](https://github.com/shadowninja108/exlaunch)** - Created by [shadowninja108](https://github.com/shadowninja108) and contributors. Provides the AArch64 inline hooking engine, NSO loading, and memory patching for Nintendo Switch.
* **[WiiUPluginSystem (WUPS)](https://github.com/wiiu-env/WiiUPluginSystem)** – Maintained by [wiiu-env](https://github.com/wiiu-env) (Maschell and contributors). Provides the plugin architecture and Aroma integration for Nintendo Wii U.
* **[libfunctionpatcher](https://github.com/wiiu-env/libfunctionpatcher)** – Maintained by [wiiu-env](https://github.com/wiiu-env). Provides PowerPC function patching and memory page permission handling for Wii U.
* **[wut](https://github.com/devkitPro/wut)** – Maintained by [devkitPro](https://github.com/devkitPro) (fincs and contributors). Provides C/C++ headers and OS bindings for Nintendo Wii U homebrew.
