# Hooks

[« Back to overview](overview.md)

## Defining a hook

A hook is a struct that intercepts a function at a given address, runs your code, and can call through to the original:

```cpp
#include <wiixlaunch.hpp>

WIIXL_HOOK_DEFINE_TRAMPOLINE(PlayerStaminaHook) {
    static void Callback(float amount, void* player) {
        // Prevent stamina decrease by passing 0.0f instead
        Orig(0.0f, player);
    }
};

extern "C" void WiiXLaunch_Init() {
#if WIIXL_WIIU
    if (!WiiXLaunch::Backend::InitWiiUBackend()) return;
#elif WIIXL_CEMU
    if (!WiiXLaunch::Backend::InitCemuBackend()) return;
#endif

    PlayerStaminaHook::Install(0x00885bd0, 0x02d908b4);
}
```

`Callback`'s signature must exactly match the target function's (return type, argument types, calling convention). `Orig(...)` calls the original function

(call it, don't call it, or call it with different arguments, depending on what you want the hook to do.)

`Install(switchOffset, wiiuOffset)` takes both offsets and picks the right one for the platform being built. see [Finding offsets](#finding-offsets) below. On Cemu, the Wii U offset is reused obv.

`WIIXL_HOOK_DEFINE_REPLACE` is also available with the same interface, for hooks that don't need `Orig()` at all.

## Finding offsets

Offsets are addresses into the game binary. WiiXLaunch doesn't locate these for you; that's reverse-engineering work done in a disassembler (Ghidra, IDA) against the specific game version you're targeting. WiiXLaunch uses relative offsets, so taking an address from Ghidra using the SwitchLoader plugin, ensure you subtract `0x7100000000`. Using the RPX plugin for Ghidra already yields the proper offsets when reverse engineering Wii U binaries.

Reminder:

* `wiixlaunch.json`'s `switch.title_id_range_min`/`max` and `wiiu.target_title_ids` scope which game versions your hooks are expected to apply to.

## Raw memory patches

For patches that aren't a full function hook (a single instruction, a constant, a NOP): use `WiiXLaunch::CodePatch` directly:

```cpp
// Overwrite raw bytes
WiiXLaunch::CodePatch::Write(offset, data, size);

// Overwrite a typed value
WiiXLaunch::CodePatch::WriteValue<uint32_t>(offset, 0x60000000);

// NOP out an instruction
WiiXLaunch::CodePatch::Nop(offset);
```

This writes directly to the target's memory/code cave rather than installing a trampoline (use it when you don't need to run any C++ logic at that address, just change what's there.)

## Platform differences you don't have to think about

`WIIXL_HOOK_DEFINE_TRAMPOLINE` and `CodePatch` cover the same three backends as everything else in the framework:

* **Switch**: exlaunch's inline hooking engine (`vendor/exlaunch`), applied against the loaded NSO.
* **Wii U**: libfunctionpatcher, matching by title ID and RPX text offset (`AddPPCExecutablePatch` in [`wiiu_backend.hpp`](../include/wiixlaunch/wiiu/wiiu_backend.hpp)).
* **Cemu**: a hand-rolled PowerPC trampoline pool + long-jump patcher (`InstallHook` in [`wiixl_cemu_backend.hpp`](../include/wiixl_cemu_backend.hpp)), since there's no plugin API to call into on a bare code cave.

You write the hook once; `Install()` dispatches to whichever of these applies at compile time via `#if WIIXL_SWITCH` / `WIIXL_WIIU` / `WIIXL_CEMU`.

## A note on the "same" function not always being the same

Even when Switch and Wii U/Cemu are running "the same game," the two builds aren't the same machine code (different compiler, different ABI, different struct packing.) A struct that's genuinely the same game object can still end up with its fields at different byte offsets, or a function's arguments shuffled differently, on one platform versus the other. Don't assume a `Callback` written against one platform's layout is safe to reuse verbatim on the other.

When a hook's target differs enough between platforms, split it: either `#if WIIXL_SWITCH` / `#elif WIIXL_WIIU || WIIXL_CEMU` branches inside one `Callback`, or, once the platform-specific part is more than a couple of lines: pull the actual game logic out into a shared base struct, and give each platform its own thin `Callback` that extracts that platform's fields and calls into it. The hook macros expand to `struct name : public <HookBase>`, so you can add your own base with the usual multiple-inheritance syntax:

```cpp
struct MyCameraLogic {
    static void Apply(float* pos, float* at, float* up) {
        // the actual logic, written once, shared by every platform
    }
};

#if WIIXL_SWITCH
WIIXL_HOOK_DEFINE_TRAMPOLINE(MyCameraHook), public MyCameraLogic {
    static void Callback(void* camera, float* matrix) {
        auto* p = static_cast<uint8_t*>(camera);
        Apply(reinterpret_cast<float*>(p + 0x38), reinterpret_cast<float*>(p + 0x44), reinterpret_cast<float*>(p + 0x50));
        Orig(camera, matrix);
    }
};
#else
WIIXL_HOOK_DEFINE_TRAMPOLINE(MyCameraHook), public MyCameraLogic {
    static void Callback(void* camera, float* matrix) {
        auto* p = static_cast<uint8_t*>(camera);
        Apply(reinterpret_cast<float*>(p + 0x34), reinterpret_cast<float*>(p + 0x40), reinterpret_cast<float*>(p + 0x4C));
        Orig(camera, matrix);
    }
};
#endif
```

That keeps the part you actually care about (what the hook does) written once, while the part that has to differ (how you reach into each platform's version of the struct to get there), stays isolated and easy to audit per platform.

## List of Hooks and CodePatch types:

### WIIXL_HOOK_DEFINE_TRAMPOLINE(name)

Wraps a function: your `Callback` runs instead of it, and can call `Orig(...)` to run the real thing.

```cpp
WIIXL_HOOK_DEFINE_TRAMPOLINE(MyHook) {
    static RetType Callback(ArgTypes... args) {
        // your code 
        return Orig(args...); // optional: call through to the original
    }
};

MyHook::Install(switchOffset, wiiuOffset);
```

* `Orig(args...)` - static member, calls the original function. Only valid to call after `Install()` has run.
* `Install(switchOffset, wiiuOffset)` - installs the hook. On Wii U this always patches against `wiixlaunch.json`'s `wiiu.target_title_ids`.

### WIIXL_HOOK_DEFINE_REPLACE(name)

Identical mechanics to `WIIXL_HOOK_DEFINE_TRAMPOLINE` (same `Orig()` + `Install(switchOffset, wiiuOffset)` shape). It's a separate macro purely so the call site can say what it means: a hook that fully replaces a function's behavior rather than wrapping around it. Use whichever name documents your intent; there's no behavioral difference.

### WIIXL_HOOK_REPLACE(HookName, RetType, SwitchOffset, WiiUOffset, Args...)

A macro-only alternative: offsets are baked in at the macro invocation instead of chosen later at an `Install()` call, and the original function is exposed as a plain function pointer (`HookName::Original`) instead of an `Orig(...)` call.

```cpp
WIIXL_HOOK_REPLACE(MyHook, void, 0x1234, 0x5678, int a, float b) {
    // call through if you want to
    MyHook::Original(a, b);
}

// Switch / Cemu:
MyHook::Install();

// Wii U - defaults to wiixlaunch.json's target title IDs:
MyHook::Install();
// or override per-hook:
MyHook::Install(myTitleIds, myTitleIdCount);
```

This is the only hook form that lets you target different title IDs than the project defaults on a per-hook basis on Wii U.

### WIIXL_OFFSET(switchOffset, wiiuOffset)

The plain macro all three hook forms use internally to pick the right offset for the platform being compiled: `switchOffset` on Switch, `wiiuOffset` everywhere else (Wii U and Cemu share the same binary offsets obv). Usable standalone if you need a platform-correct address outside of a hook:

```cpp
constexpr auto offset = WIIXL_OFFSET(0x00885bd0, 0x02d908b4);
```

### CodePatch::Write(targetOffset, data, size)

Copies raw bytes over the target address. On Wii U this also flushes the data cache and invalidates the instruction cache at that range (`DCFlushRange`/`ICInvalidateRange`), since PowerPC doesn't keep them coherent for you.

```cpp
const uint8_t bytes[] = { 0x60, 0x00, 0x00, 0x00 };
WiiXLaunch::CodePatch::Write(offset, bytes, sizeof(bytes));
```

### CodePatch::WriteValue<T>(targetOffset, value)

`Write`, but for a single typed value instead of a raw byte buffer. Equivalent to `Write(targetOffset, &value, sizeof(T))`.

```cpp
WiiXLaunch::CodePatch::WriteValue<float>(offset, 0.0f);
WiiXLaunch::CodePatch::WriteValue<uint32_t>(offset, 0x60000000);
```

### CodePatch::Nop(targetOffset)

Overwrites the instruction at `targetOffset` with a platform-correct no-op (`0xD503201F` on Switch's AArch64, `0x60000000` on Wii U/Cemu's PowerPC).

```cpp
WiiXLaunch::CodePatch::Nop(offset);
```

### CpuContext

[`context.hpp`](../include/wiixlaunch/context.hpp) defines a `WiiXLaunch::CpuContext` struct for raw register access (`GetArg`/`SetArg` per platform). It's not wired into any hook path yet on Switch, Wii U, or Cemu (nothing constructs one or passes it to a callback.) I haven't found a reason to implement it yet, if I do, I will, or maybe you could make a PR lol.
