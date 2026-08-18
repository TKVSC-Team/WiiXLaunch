# Debugging

[« Back to overview](overview.md)

None of the three targets have a normal console, so WiiXLaunch gives you one call, `WIIXL_LOG`, that reaches you differently depending on what's actually available on each platform:

```cpp
WIIXL_LOG("player pos: %.2f %.2f %.2f", x, y, z);
```

| Platform | Where it goes |
|---|---|
| Switch | `svcOutputDebugString`, via exlaunch's `SvcLogger`: shows up directly in your emulators console, unsure of how it works/if it will display on Switch. |
| Wii U (Aroma) | An on-screen notification toast (WUPS `NotificationModule`). |
| Cemu | A ring buffer written into the mod's own memory, read out of process by `tools/ring_log_reader`. |

Same call site everywhere - `#if WIIXL_...` branches only live inside `WIIXL_LOG`'s implementation ([`debug_log.hpp`](../include/wiixlaunch/debug_log.hpp)), not in your hook code.

## Reading Cemu's ring log

Cemu's target process is a bare code cave with no OS log access at all, so `WIIXL_LOG` writes free-form text entries (framed by start/end cookies) into a small ring buffer living as a static global inside the mod. `tools/ring_log_reader` finds that buffer by scanning the Cemu process's memory for a magic cookie - no debugger, no symbols, no cooperation from Cemu needed - and prints new entries as they arrive.

Because entries are just text, the reader never needs to know your message format or be rebuilt when you add a new `WIIXL_LOG(...)` call site somewhere in your mod.

Build it:

```bash
# Windows
tools/ring_log_reader/build.bat

# Linux
cd tools/ring_log_reader && ./build.sh
```

Run it while Cemu is running your mod:

```bash
# Windows
tools/ring_log_reader/ring_log_reader.exe

# Linux
tools/ring_log_reader/ring_log_reader          # may need sudo, see below
```

By default it looks for a process named `Cemu.exe` (Windows) or `Cemu` (Linux); pass a different name as the first argument if yours differs.

Reading another process's memory needs elevated privileges on both platforms:

* **Windows**: run as Administrator if `OpenProcess` fails.
* **Linux**: run with `sudo` if opening `/proc/<pid>/mem` fails - normally only a process's owner (or root) can read it.

## Wii U toasts vs. plugin status

The on-screen toasts that WUPS plugins show at load ("N hooks registered", etc., in [`wiiu_plugin.cpp`](../src/wiiu_plugin.cpp)) are a separate, fixed set of messages reporting FunctionPatcher/hook-registration status - they're not `WIIXL_LOG` output. `WIIXL_LOG` calls from your own hooks show up as their own toasts alongside those.

## Limitations
Debug logs can be no longer than 200 characters. This is a limitation of the ring buffer method for Cemu. It's configurable in [`debug_log.hpp`](../include/wiixlaunch/debug_log.hpp), but I would recommend against changing it.

## `BotW::OSLog` is a different thing

If you're using the [wiixlaunch-botw](https://github.com/TKVSC-Team/wiixlaunch-botw) module, you'll also see `WiiXLaunch::BotW::OSLog(...)` calls in its Cemu-side code (`gx2.hpp`, `fs.hpp`, etc). This is not `WIIXL_LOG` - it's a separate, Cemu-only logger that calls Cemu's own `OSReport` directly, and it's what the module's internals use for their own diagnostics. It doesn't write to the ring buffer, so `tools/ring_log_reader` won't show it; look for it in Cemu's own log window instead. Your own mod code should keep using `WIIXL_LOG`.