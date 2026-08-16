# Switch (NVN) graphics injection - RE findings

[« Back to overview](overview.md)

Working notes from reverse-engineering BotW's Switch binary in Ghidra, toward the graphics-pipeline-injection module described in [wiixlaunch-botw's TODO.md](https://github.com/TKVSC-Team/wiixlaunch-botw/blob/main/TODO.md). Everything here is Switch/NVN-specific; Wii U/GX2 findings will get their own doc once that side starts.

Addresses below are raw Ghidra addresses (`0x7100000000`-based, i.e. NSO base already added back) unless noted - subtract `0x7100000000` to get a `WIIXL_OFFSET`-style Switch offset for use with `WiiXLaunch::GetTargetFunction`/hooks.

## Status: architecture sketched, not yet implemented or built

## The core mechanism: NVN is resolved, not linked

BotW does not statically link against `nvn*` functions. At boot, a single loader function resolves every NVN entry point it needs and stores the result into a fixed-address table, one call per function name.

* **`nvnLoadCProcs`** (`0x7100aad0fc` onward) - the loader. Confirmed via string xrefs: both `"nvnQueueSubmitCommands"` and `"nvnQueuePresentTexture"` resolve inside this function.
* Every resolved function pointer lives behind **two levels of indirection** from a fixed table base (confirmed via disassembly, not just decompiler artifacts):
  ```
  adrp x8, <table_page>
  ldr  x8, [x8, #offset]   ; x8 = address of the real storage cell (GOT-style, static after NSO relocation)
  ldr  x8, [x8]            ; x8 = the actual resolved function pointer (only valid AFTER nvnLoadCProcs has run)
  blr  x8
  ```
  In C terms: `(**(code**)(table_base + offset))(args...)`.
* Two table bases found so far:
  * **`0x7102596000`** - raw `nvn*` C function pointers (device/queue/command-buffer/etc.)
  * **`0x7102591000`** - engine-level singleton pointers (`sead::DebugFontMgrNvn::sInstance` and siblings)

This means our module never needs to call `nvnDeviceGetProcAddress` itself - we can read BotW's own already-resolved pointers directly, once we know each function's offset in the `0x7102596000` table.

### ⚠️ Methodology note - and a fully-resolved systematic bug in the earlier readings

Two different ways of reading these offsets were used:

1. **Xref-anchored** (reliable): `get_xrefs_to` the function-name *string* itself, which points at the exact `add x1,x1,#offset` instruction that loads it, then read the `adrp x8,<table>` / `ldr x8,[x8, #N]` pair immediately before it. No manual counting involved.
2. **Read off decompiler pseudocode** (found unreliable): trusting the `PTR_pfnc_<name>_<address>` symbol Ghidra shows inline when decompiling some *other* function that calls through the table.

Every address read via method 2 turned out to be **exactly 8 bytes (one table slot) too high** - confirmed independently five separate times (`AddCommandMemory`, `BeginRecording`, `EndRecording`, `BindUniformBuffer`, `DrawArrays` all showed the identical -8 offset once re-derived via method 1). Cause not fully understood - possibly a stale/off-by-one auto-labeling pass on Ghidra's end - but the correction is consistent enough to trust: **every table entry below that's still marked "pattern-corrected" has had -8 applied but has *not* been individually xref-verified the way the five confirmation cases were.** Treat "xref-anchored" as ground truth, "pattern-corrected" as high-confidence-but-not-individually-proven, and re-derive anything load-bearing via method 1 before it goes into real hook code.

### Confirmed `0x7102596000`-table offsets

| Function | Offset | Address | Method |
| --- | --- | --- | --- |
| `nvnDeviceGetInteger` | `+0x880` | `0x7102596880` | xref-anchored |
| `nvnMemoryPoolBuilderSetDevice` | `+0xa88` | `0x7102596a88` | xref-anchored |
| `nvnMemoryPoolBuilderSetDefaults` | `+0xa90` | `0x7102596a90` | xref-anchored |
| `nvnMemoryPoolBuilderSetStorage` | `+0xa98` | `0x7102596a98` | xref-anchored |
| `nvnMemoryPoolBuilderSetFlags` | `+0xaa0` | `0x7102596aa0` | xref-anchored |
| `nvnMemoryPoolInitialize` | `+0xac0` | `0x7102596ac0` | xref-anchored |
| `nvnSamplerPoolInitialize` | `+0xb48` | `0x7102596b48` | xref-anchored |
| `nvnQueueSubmitCommands` | `+0x9b8` | `0x71025969b8` | xref-anchored |
| `nvnQueueFlush` | `+0x9c0` | `0x71025969c0` | xref-anchored |
| `nvnQueueFinish` | `+0x9c8` | `0x71025969c8` | xref-anchored |
| `nvnQueuePresentTexture` | `+0x9d0` | `0x71025969d0` | xref-anchored |
| `nvnQueueAcquireTexture` | `+0x9d8` | `0x71025969d8` | xref-anchored |
| `nvnCommandBufferInitialize` | `+0x250` | `0x7102597250` | xref-anchored |
| `nvnCommandBufferFinalize` | `+0x258` | `0x7102597258` | xref-anchored |
| `nvnCommandBufferSetDebugLabel` | `+0x260` | `0x7102597260` | xref-anchored |
| `nvnCommandBufferSetMemoryCallback` | `+0x268` | `0x7102597268` | xref-anchored |
| `nvnCommandBufferSetMemoryCallbackData` | `+0x270` | `0x7102597270` | xref-anchored |
| `nvnCommandBufferAddCommandMemory` | `+0x278` | `0x7102597278` | xref-anchored |
| `nvnCommandBufferAddControlMemory` | `+0x280` | `0x7102597280` | xref-anchored |
| `nvnCommandBufferGetCommandMemorySize` | `+0x288` | `0x7102597288` | xref-anchored |
| `nvnCommandBufferGetCommandMemoryUsed` | `+0x290` | `0x7102597290` | xref-anchored |
| `nvnCommandBufferGetCommandMemoryFree` | `+0x298` | `0x7102597298` | xref-anchored |
| `nvnCommandBufferBeginRecording` | `+0x2b8` | `0x71025972b8` | xref-anchored |
| `nvnCommandBufferEndRecording` | `+0x2c0` | `0x71025972c0` | xref-anchored |
| `nvnCommandBufferBindUniformBuffer` | `+0x330` | `0x7102597330` | xref-anchored |
| `nvnCommandBufferDrawArrays` | `+0x3c8` | `0x71025973c8` | xref-anchored |
| `nvnQueueFenceSync` | - | `0x71025976c0` | pattern-corrected (-8 from `0x71025976c8`) |
| `nvnCommandBufferBindTexture` | - | `0x7102597360` | pattern-corrected (-8 from `0x7102597368`) |
| `nvnCommandBufferGetControlMemoryUsed` | - | `0x71025972a8` | pattern-corrected (-8 from `0x71025972b0`) |
| `nvnCommandBufferSetSamplerPool` | - | `0x71025975e8` | pattern-corrected (-8 from `0x71025975f0`) |
| `nvnCommandBufferSetTexturePool` | - | `0x71025975e0` | pattern-corrected (-8 from `0x71025975e8`) |
| `nvnCommandBufferSetShaderScratchMemory` | - | `0x71025975f0` | pattern-corrected (-8 from `0x71025975f8`) |
| `nvnCommandBufferReportCounter` | - | `0x7102597568` | pattern-corrected (-8 from `0x7102597570`) |
| `nvnBufferGetAddress` | - | `0x7102596bd0` | pattern-corrected (-8 from `0x7102596bd8`) |

Still need (not located at all yet): `nvnCommandBufferBindProgram`, `nvnCommandBufferBindVertexBuffer`, `nvnCommandBufferSetViewport`. Only relevant for the later "raw custom geometry" phase, not Phase 1 text.

## Phase 1 plan (recommended starting point): reuse the game's own text renderer

Rather than build an NVN pipeline from scratch, BotW already ships a fully-initialized text drawing system.

### `sead::DebugFontMgrNvn` is live in the retail binary

Confirmed by decompiling the boot sequence in `nnMain` (around `0x71007d6280`-`0x71007d62cc`):
```
bl   DebugFontMgrNvn::createInstance(heap)     ; 0x7100aff290
; re-reads the singleton pointer it just set:
adrp x8, 0x7102591000
ldr  x8, [x8, #0xa98]        ; -> address of sInstance storage cell
ldr  x0, [x8]                 ; -> live DebugFontMgrNvn* (this)
bl   DebugFontMgrNvn::initialize(this, heap,
        "System/font/nvn_font/nvn_font_shader.bin",
        "System/font/nvn_font/nvn_font.ntx", flags)  ; 0x7100aff33c
```
Both calls are **unconditional** - not gated behind a debug flag. Real shader/texture resource paths match the string table (`"System/font/nvn_font/..."`). This is genuinely live, not dead debug code.

**Singleton access from our own hook code:**
```cpp
uintptr_t slot = *(uintptr_t*)0x7102591a98;      // (0x7102591000 + 0xa98), GOT-relocated
auto* debugFontMgr = *(sead::DebugFontMgrNvn**)slot;  // live instance, valid any time after boot
```
(The Jis1/Japanese sibling `DebugFontMgrJis1Nvn` sits at `0x7102591aa0`, same pattern, confirmed via the same `procDraw_` decompile below.)

### Relevant `sead::DebugFontMgrNvn` methods

* `begin(DrawContext*)` - `0x7100aff91c`
* `print(DrawContext*, const Projection&, const Camera&, const Matrix34<f32>&, const Color4f&, const void* text, int count)` - `0x7100aff94c`
* `end(DrawContext*)` - `0x7100aff948`
* `swapUniformBlockBuffer()` - `0x7100aff8d0` - called once per frame by the engine itself (see `procDraw_` below); our draws need to happen before this runs for that frame, i.e. during/after the game's own draw but before its next swap.

**`print` fully decompiled - confirms several things, one of them a correction of an earlier guess:**

* **Text encoding is UTF-16, not ASCII/Latin-1** (guessed wrong earlier, based on the existence of a separate `Jis1` sibling class - checked the actual disassembly instead of trusting the guess): the text loop reads `*(ushort*)(text + i*2)`, i.e. 2 bytes per character, a `char16_t`-style string, matching sead's usual wide-char text convention.
* **But the font atlas itself only covers printable ASCII (0x20-0x7E)**: character codes in `[0x20, 0x7E]` map to glyph index `code - 0x20` (0-93); anything else (including the rest of Unicode) clamps to glyph index `0x1F`, presumably a fallback/placeholder glyph. So: encode as UTF-16, but only expect plain printable ASCII to actually render as itself.
* **Max 128 characters per `print()` call** - `count` is clamped to `0x80`.
* **Per-frame glyph budget**: `print()` checks an internal cursor against a fixed-size uniform buffer at entry (`this+0x528` is a "budget already exceeded, do nothing" flag, set once the buffer fills up) - if the game's own debug text usage plus ours overflows this per-frame buffer, later `print()` calls in the same frame silently no-op. Exact buffer size not yet measured.
* Confirms the actual draw: one quad (`4` vertices) per renderable character, `nvnCommandBufferDrawArrays(cmdBuf, mode=7, first=0, count=charCount*4)`.
* `nvnCommandBufferDrawArrays` and `nvnCommandBufferBindUniformBuffer` addresses first spotted here, later xref-verified (see the table below - `0x71025973c8` / `0x7102597330`, not the addresses this decompile's symbol names initially suggested).

### Self-contained Projection/Camera - no dependency on the game's live camera

* **`sead::OrthoProjection(f32 near, f32 far, f32 top, f32 bottom, f32 left, f32 right)`** - `0x7100b1e154`. Fully self-constructible on the stack; just stores the 6 floats + sets up its vtable. Perfect for a fixed screen-space UI projection.
* ~~`sead::DirectCamera`~~ - investigated and **not recommended**: its `doUpdateMatrix` (`0x7100b1c544`) confirms it's just a stored 12-float matrix copied out of `this+0x38`, but no constructor for it exists *anywhere* in this binary (only destructor/RTTI/`doUpdateMatrix`) - strong evidence BotW itself never actually instantiates one, even though the class ships as part of the shared `sead` library. Reverse-engineering how to safely construct a class the game never uses itself (full base-`Camera` layout, vtable setup, RTTI) is unnecessary risk for no benefit given a better option exists:
* **`sead::LookAtCamera(const Vector3<f32>& pos, const Vector3<f32>& at, const Vector3<f32>& up)`** - `0x7100b1bd88`. This is the *actual* camera class BotW uses every frame (it's the exact class Freecam already hooks via `doUpdateMatrix`), so its layout is already proven, not guessed. Decompiled the constructor directly and it fully reconciles with Freecam's existing known offsets: `pos` written to `this+0x38`, `at` to `this+0x44`, `up` to `this+0x50` (confirmed field-by-field, including the `up.z` write landing at `this+0x58` as expected for a 3-float `Vector3` starting at `0x50`) - exactly the offsets `WiiXLaunch::BotW::Camera` already uses. The constructor also auto-normalizes `up` and seeds the cached view matrix to identity. **Recommendation: build a throwaway `sead::LookAtCamera` (any reasonable pos/at/up, e.g. looking down -Z from a small distance) instead of chasing `DirectCamera`.**
* **Bonus finding while checking this**: in the already-decompiled `print()` body (see above), the `const Camera&` parameter is never actually dereferenced anywhere in the function - only the separate `const Matrix34<f32>&` parameter is used for the text's actual world/screen transform (multiplied against the projection matrix), and the `Projection&` for the projection matrix itself. So the *real* on-screen position/orientation of each string is controlled entirely by the `Matrix34` argument we pass, not by whatever camera we hand it - which means the exact pos/at/up chosen for our throwaway `LookAtCamera` likely doesn't matter much; any validly-constructed one should do, and text placement is a simple translation matrix problem instead.

### The per-frame pipeline (`sead::GameFrameworkNx`)

`mainLoop_` (`0x7100af8f84`) → `procCalc_` (`0x7100af9380`, game logic) → `procDraw_` (`0x7100af90a8`) → `present_` (`0x7100af8ef4`/`0x7100af9418`, two overloads seen) → `swapBuffer_` (`0x7100af9520`).

**`procDraw_`** (decompiled in full) constructs one `sead::DrawContext` on the stack (`0x7100b02c5c` ctor / `0x7100b02cc8` dtor), records the *entire* game frame into one `NVNcommandBuffer`:
* `this+0x158` (`param_1[0x2b]`) - the frame's `NVNcommandBuffer` handle
* `AddCommandMemory` → `AddControlMemory` → `BeginRecording` → ... draw the whole scene (`SingleScreenMethodTreeMgr::draw`, `FrameBuffer::bind`, then a vtable call at `*plVar7+0x20` which is almost certainly the actual scene render entry) ... → `EndRecording`
* `this+0x1d8` (`param_1[0x3b]`) - the **recorded command handle** `EndRecording` returns
* Near the end, after `DrawContext` is destroyed: `swapUniformBlockBuffer()` is called for `PrimitiveDrawMgrNvn`, `DebugFontMgrNvn` (`0x7102591a98` null-check), and `DebugFontMgrJis1Nvn` (`0x7102591aa0`), each only if their singleton is non-null.

**`present_`** (decompiled in full) is where submission actually happens:
```
nvnQueueSubmitCommands(queue = this+0x198 /* param_1[0x33] */, count = 1, &this[0x3b] /* recorded handle */)
nvnQueueFenceSync(queue, syncObj = this+0x1c0 /* param_1[0x38] */, 0, 0)
nvnQueueFlush(queue)
```
Critically, `nvnQueueSubmitCommands` takes a **count + pointer to an array** of command handles - it is not hardwired to exactly one submission per frame.

**`swapBuffer_`** just calls `sead::DisplayBufferNvn::presentTextureAndAcquireNext()` (wraps the actual `nvnQueuePresentTexture`/`nvnWindowAcquireTexture`).

### The injection plan this points to

Hook `present_`. Call `Orig()` first (the game submits + fences + flushes its own frame, completely normally, unmodified). Then, after `Orig()` returns:
1. `BeginRecording` on **our own**, separately-allocated `NVNcommandBuffer` (allocated once at init - see "still open" below for the pool/memory setup, which is the same pattern already decompiled in `sead::GraphicsNvn::initializeImpl`, see [hooks.md](../../WiiXLaunch%20-%20Template%20Repo/docs/hooks.md) for the general hook mechanics this all sits on top of).
2. Construct our own `sead::DrawContext` + `sead::OrthoProjection` + `sead::DirectCamera` (all self-contained, no game state needed).
3. `DebugFontMgrNvn::begin/print/end` using the live singleton.
4. `EndRecording` → `nvnQueueSubmitCommands(queue = same this+0x198, count = 1, &ourRecordedHandle)` - a **second, independent submission to the same queue**.

Since a queue executes submitted command buffers in submission order, submitting ours *after* the game's own submission draws our content on top, in the same frame, with no need to share the game's `DrawContext`, no interleaving inside `procDraw_`, and no fighting over the single shared command buffer. This is the same "call `Orig()`, then do our own thing" shape every other WiiXLaunch hook already uses.

## Getting the live NVNdevice*

Needed to build our own memory pool, and hadn't actually been chased down yet - `GraphicsNvn::initializeImpl` showed the device lives at `this+0x30`, but not how to reach the live `this` for `GraphicsNvn` itself.

Found by looking at what happens immediately after `GraphicsNvn`'s constructor is called (from `sead::GameFrameworkNx::initializeGraphicsSystem`):
```
bl   sead::GraphicsNvn::GraphicsNvn(CreateArg&)   ; 0x7100b00b50, result in x23
adrp x27, 0x7102594000
ldr  x27, [x27, #0xc10]      ; x27 = value read FROM address 0x7102594c10
str  x23, [x27]              ; the constructed instance is written to *that* address, not to 0x7102594c10 itself
```
**Initially misread this as a single dereference - it isn't.** If `0x7102594c10` held the instance pointer directly, the `str` would target it directly (no `ldr` needed first); instead the code reads a value out of `0x7102594c10` *and then writes through that value* - which only makes sense if `0x7102594c10` is itself a GOT-style relocation entry (pre-populated by the loader at NSO-load time, before any game code runs) holding the address of a *separate*, dedicated storage cell, and the real instance pointer lives in that cell. Same double-indirection pattern as everything else in this binary, not an exception:

```cpp
uintptr_t cell = *(uintptr_t*)0x7102594c10;              // GOT-relocated, valid from load time
sead::GraphicsNvn* instance = *(sead::GraphicsNvn**)cell; // the actual live singleton pointer
void* device = *(void**)((uint8_t*)instance + 0x30);
```

This does independently cross-confirm something already seen without understanding it: `procDraw_`'s decompile referenced `PTR_DAT_7102594c10` directly (toggling a flag at `instance+0x200` around the draw dispatch) - same address, found from a completely different code path. But note the decompiler's pseudocode there (`*(long*)PTR_DAT_7102594c10`) reads as a *single* dereference too - it isn't; Ghidra's symbolic `PTR_DAT_X` rendering already silently absorbs the first GOT hop, the same way its `PTR_pfnc_X` rendering turned out to hide (and in that case, corrupt) a level of indirection elsewhere in this doc. Lesson reinforced: read the raw instruction sequence, not the decompiler's collapsed symbol names, when the exact number of dereferences matters.

## Opaque NVN struct sizes (needed to declare our own command buffer/pool)

Nothing in the devkitPro/libnx toolchain ships real NVN SDK headers (checked `vendor/exlaunch` and `devkitA64` - neither has one). Rather than guess these sizes - getting an opaque NVN struct's size wrong means the driver writes past the end of it, real memory corruption - pulled them from [`code.botw.link`'s copy of BotW's own vendored `nvn.h`](https://code.botw.link/uking/uking/lib/NintendoSDK/include/nvn/nvn.h.html), the actual header this exact binary was compiled against (via the zeldamods decompilation project), cross-checked against [open-ead/nnheaders](https://github.com/open-ead/nnheaders/blob/master/include/nvn/nvn.h|"nvn.h at master · open-ead/nnheaders")'s copy:

| Type | Size | Needed for |
| --- | --- | --- |
| `NVNcommandBuffer` | `0xA0` (160) | our own command buffer object |
| `NVNmemoryPool` | `0x100` (256) | our own backing memory pool |
| `NVNmemoryPoolBuilder` | `0x40` (64) | transient, stack-local |
| `NVNqueue` | `0x2000` | not allocated by us - we read the game's own live one off `GameFrameworkNx+0x198` |
| `NVNdevice` | `0x3000` | not allocated by us - likewise read from the game's own state |
| `NVNbuffer` | `0x30` | if a vertex/uniform buffer object is needed later |
| `NVNbufferBuilder` | `0x40` | transient |

## Phase 1 is now fully specified

Every piece needed for an always-on text overlay is identified: the hook point (`present_`), how to reach the live queue (`this+0x198`) and device (`GraphicsNvn` singleton → `+0x30`), how to build our own command buffer and memory pool (real NVN struct sizes + xref-anchored function offsets), how to build a throwaway camera/projection/draw-context, and how to call the game's own already-initialized text renderer. See `src/main.cpp` for the actual implementation attempt.

## Implementation status

`include/nvn_overlay.hpp` + the `present_` hook in `src/main.cpp` implement the full Phase 1 plan - draws `"We have hooked NVN!"` as an always-on overlay. **Compiles and links clean for Switch** (real ARM64 devkitA64 build, not just a syntax check). Two real bugs were caught and fixed just from writing/building it:
* Forgot to call `nvnCommandBufferAddCommandMemory`/`AddControlMemory` after `Initialize` - `BeginRecording` would have had nowhere to actually record into.
* An unrelated, pre-existing bug in this project's own `debug_log.hpp` (`exl::log::Logging` doesn't exist - the real symbol is just global-scope `::Logging`) blocked the build entirely; fixed since it's a one-line, unambiguous fix, unrelated to the NVN work itself but blocking it.

**`GameFrameworkNx` queue/handle offsets - now confirmed via raw disassembly, not decompiler trust:**

Re-checked `present_`'s actual instructions directly (not its decompiled pseudocode) around the `nvnQueueSubmitCommands` call:
```
ldr x0, [x19, #0x198]   ; queue - loaded directly as the call's 1st argument
adrp x8, 0x7102596000
ldr  x8, [x8, #0x9c0]   ; function-table slot (still the known -8-shifted address; real slot is 0x9b8, already corrected above)
ldr  x8, [x8]
add  x2, x19, #0x1d8    ; handle array - computed directly as the call's 3rd argument
mov  w1, #0x1
blr  x8                 ; nvnQueueSubmitCommands(queue, 1, &handle)
```
Both `+0x198` (queue) and `+0x1d8` (recorded-handle storage) are exactly what the implementation already uses - confirmed, not guessed. Bonus: the `GraphicsNvn` singleton double-dereference at `0x7102594c10` (see above) shows up again here too, independently, a third time, in the same early-exit check at the top of `present_`.

**Still genuinely unverified - this has never run:**
* Screen-space bounds (`720p`, hardcoded) and text placement (world-space origin) - both guesses, need visual iteration once something actually renders.
* The per-frame glyph uniform buffer budget - unmeasured, but one short string is almost certainly fine.
* Command-buffer-level draw offsets needed for the *later* "raw custom geometry" phase (not Phase 1 text): `nvnCommandBufferBindProgram`, `nvnCommandBufferBindVertexBuffer`, `nvnCommandBufferSetViewport`.

**Next real step: run it** (real hardware or Ryujinx/yuzu) and see what actually happens - everything past this point is guessing without that feedback.

## First real-world test: ran clean, no crash, no visible text

No crash is a strong positive signal on its own - it means the queue/handle offsets, the singleton reads, the NVN struct sizes, and every xref-anchored function offset are all structurally sound; a wrong one of those would very likely have faulted. "Runs, draws nothing" pointed somewhere more specific.

**Root cause (highly likely): our `DrawContext` was never bound to a render target.** `procDraw_`'s raw disassembly (not decompiler pseudocode) shows the game explicitly binding its own `DrawContext` before drawing anything:
```
ldr x0, [x19, #0x100]       ; live sead::FrameBuffer*
cbz x0, ...
mov x1, sp                  ; &drawContext
bl  sead::FrameBuffer::bind  ; 0x7100b1cca8
```
Our own fresh, blank `DrawContext` never got this call - draw commands with no bound render target have nowhere to write. Added it: `nvn_overlay.hpp` now reads `gameFramework+0x100` and calls `FrameBuffer::bind` before recording. Compiles and links clean.

**One more thing spotted in the same disassembly, not yet added:** immediately after `FrameBuffer::bind`, `procDraw_` also does a *virtual* call - `(*(vtable(gameFramework)+0x110))(gameFramework, 3)` - purpose not yet determined (possibly viewport/scissor setup to match the framebuffer). Held off on replicating it since it's a blind virtual dispatch without knowing what it actually does, higher risk than the concrete `FrameBuffer::bind` call. If binding alone isn't enough to make text appear, this is the next thing to chase.

## Second real-world test: still no text, but zero diagnostic output at all

Added `WIIXL_LOG` checkpoints throughout `nvn_overlay.hpp` (hook firing, `EnsureInitialized` success/failure, queue/fontMgr/frameBuffer pointers, final submitted handle) before re-testing, specifically to stop guessing blind. First checked whether `DebugFontMgrNvn` itself could be retail-stripped (a reasonable question) - it isn't: `createInstance` explicitly zeroes `instance+0x528` (the exact guard byte `print()` checks) out of an otherwise-uninitialized `operator new[]` allocation, and `initialize()` unconditionally loads the real shader/texture files with no debug-build gate anywhere. The system is genuinely alive.

The real bug was much dumber: **not a single `NvnOverlay:` log line appeared anywhere, across 34+ seconds of gameplay** - not even the very first one (`PresentHook firing`), logged unconditionally right after `Orig()` on every call. That means the hook itself was never actually reaching our callback.

## Third real-world test: hook fires, crashes deep inside EnsureInitialized

Added a `WIIXL_LOG` after every single step in `EnsureInitialized`, plus a defensive null-check in `GetGraphicsNvnInstance` (confirmed via `read_memory` that the static image holds a real, non-zero pointer at the singleton slot, so the addressing was never actually wrong - this guard was about a possible *timing* issue, `present_` firing before `GraphicsNvn` is constructed on an early frame). This localized the crash precisely: log reached `init step 15` (`addCtrlMem` resolved to a real address, correctly inside `nnSdk` - the NVN SDK is statically linked there, not into the game's own module, which is expected) and then crashed on `null` immediately on calling it.

First hypothesis: `g_ControlMemory` was only `alignas(8)`, and NVN has a real, documented control-memory alignment requirement (`NVN_DEVICE_INFO_COMMAND_BUFFER_CONTROL_ALIGNMENT`, seen in the string table during the original RE pass). Page-aligned it to match `g_PoolMemory`. Retested - `X[01]` in the crash dump confirmed the pointer was now cleanly page-aligned, but the crash was byte-for-byte identical anyway. Ruled out.

Second check: re-verified `nvnCommandBufferAddControlMemory`'s real documented signature (`NVNcommandBuffer*, void* memory, size_t size`) against a public source - matched what was already implemented. Also re-derived its table offset (`+0x280`) a second time, independently, via the same xref-anchored method as before - matched. Neither was the bug.

**Actual root cause, found by re-reading the *existing* log output more carefully**: `init step 12 (cmdBufInit called), result=0`. `nvnCommandBufferInitialize` returns `NVNboolean` (0 = failure) - and it *was* failing, silently, right next to `poolInit`'s `result=1` (success) a few lines above it in the very same log. The code logged the return value but never actually checked it, so it proceeded to call `AddCommandMemory`/`AddControlMemory` on a command buffer that was never successfully initialized - `AddControlMemory` was simply the first call that dereferenced state `Initialize` was supposed to have set up. Likely explanation: this is firing on a very early frame, while the game's own `ServiceNv` setup is still mid-flight in the log around the same timestamp - the device may be non-null but not yet fully ready to create *new* command buffers.

Fix: check every `NVNboolean`-returning call and bail out - without setting `g_Initialized` - on failure, so `EnsureInitialized` safely retries on a later frame instead of proceeding on broken state. Removed the verbose per-step logging now that it's served its purpose; kept one log on the failure path and one on final success.

## Fourth real-world test: crash gone, but permanently failing every frame

With the return-value check in place, the crash is gone - but `CommandBufferInitialize failed, retrying next frame` logged and then *never* retried successfully across 11+ seconds / ~600+ frames. Not a one-off early-frame timing issue as guessed - a persistent, deterministic failure.

Checked the real public `nvn.h` signature for `nvnCommandBufferInitialize` via web search - it appeared to take an `NVNcommandBufferBuilder*` rather than a raw device pointer (matching the builder pattern `nvnMemoryPoolInitialize` already uses). But cross-checking against *this exact binary* (`list_strings` for `nvnCommandBufferBuilder`) found **zero matches anywhere in the binary** - that API shape doesn't exist in BotW's actual SDK version (4.4.0). The web result was very likely describing a later/different NVN SDK version, not this one - external docs aren't a reliable substitute for checking the actual binary when versions can silently drift.

So: went to find where the game's *own* code actually initializes its own command buffer (`GameFrameworkNx::initializeGraphicsSystem`, the same function that constructs `GraphicsNvn`) and read the real sequence directly:
```
mov  w0, #0xa0            ; allocate NVNcommandBuffer (0xA0 bytes, confirmed real struct size)
bl   allocator
str  x0, [x19, #0x158]    ; store (matches the already-known command buffer offset)
ldr  x8, [table + 0x258]  ; independently re-verified via string xref: this slot is genuinely nvnCommandBufferFinalize
mov  x1, x23               ; device
blr  x8                    ; Finalize(buffer, device) - on memory that was NEVER initialized
```
Scanned roughly 400 bytes further past this and never found a call through the real `Initialize` slot (`+0x250`) anywhere in this setup path. Whatever the textbook semantics of "Finalize" should be, the shipped driver's actual behavior in this exact scenario is: freshly-allocated buffer → call *Finalize* (not Initialize) with `(buffer, device)` → the buffer is now usable. Also notable: real code never inspects a result after this call, meaning it's `void`, not `NVNboolean` - the earlier "result=0" log was very likely reading garbage off an unrelated register, not an actual failure signal (it just happened to prevent the crash by returning early, coincidentally).

Fix: replaced the `Initialize` call with a call through the `Finalize` slot instead (renamed `kCommandBufferRealInit` in code, with a comment explaining why, so this doesn't look like an accidental typo later), changed its type to `void` (no result check), and removed the now-meaningless retry-on-failure logic for this specific call. Replicated exactly what proven-working game code does, rather than what the function's name suggests it should do.

## Fifth real-world test: crash returned at the exact same address, after trimming logging

Rebuilt with the Finalize-as-init fix, but had also trimmed the verbose per-step logging back down (premature - it had just proven its worth). The crash came back at the *exact same faulting address* (`nnSdk:0x2d5a88`) as the very first crash, but this time before any of the remaining log lines could print, so there was no way to localize which of the now-unlogged calls it was in. Restored full per-step logging (kept this time, rather than trimming again until the overlay actually works end to end).

Cross-checked by reading `procDraw_`'s real `AddCommandMemory`/`AddControlMemory` call site directly in raw disassembly, at the same xref-anchored table slots already trusted (`+0x278`/`+0x280`) - not the decompiler's mislabeled symbol names this time. The real call at the `AddControlMemory` slot takes **four** arguments (`cmdBuf, pool*, offset, size`), the same shape as `AddCommandMemory` - not `(cmdBuf, plainPointer, size)` as assumed (and as public docs for other SDK versions describe). In this exact SDK build (4.4.0), control memory is pool-backed too, not a raw CPU pointer. That's what was actually crashing: passing a plain static array where a real `NVNmemoryPool*` was expected.

Fix: `g_PoolMemory` (16KB) now backs both command and control memory, split in half via explicit offsets, and `FnCommandBufferAddControlMemory`'s signature/call now matches the real 4-argument shape. `g_ControlMemory` (the plain CPU buffer) removed entirely - it was never the right kind of storage for this call in the first place.

## Sixth/seventh real-world test: the command buffer pipeline is fully proven working; DebugFontMgrNvn construction is not reached at all

With the pool-backed control memory fix, `EnsureInitialized` now completes end to end with zero crashes: device found, memory pool built, command buffer prepared, command/control memory attached, `EnsureInitialized OK`. **Every offset in that entire chain (queue, device, memory pool, command buffer, all NVN calls) is now proven correct by actual successful execution**, not just static analysis. This is real, durable progress regardless of what happens with text rendering specifically - it's exactly the infrastructure a "raw custom geometry" phase would need too.

New, much simpler blocker surfaced right after: `GetDebugFontMgrNvnInstance()` returns null and an edge-triggered log (added specifically so we could distinguish "still starting up" from "never happens") confirmed it stays null for 13+ seconds of real, continued gameplay (audio starting, more services coming up - the game is definitely still running and presenting frames throughout).

To settle this definitively rather than keep reasoning about offsets, hooked `sead::DebugFontMgrNvn::createInstance` and `::initialize` directly to log whether they ever actually run. First attempt at this hooked the wrong addresses (`0xff290`/`0xff33c` instead of the correct `0xaff290`/`0xaff33c` - dropped the leading `A` again, the exact same class of mistake as the original `present_` bug, in the exact same address neighborhood as several *correctly*-transcribed offsets a few lines away). Fixed and retested.

**With the corrected hook addresses, neither `createInstance` nor `initialize` fires - at all, across the whole session.** This is a trustworthy negative result now, not a broken diagnostic: hooking a function's entry point intercepts it regardless of who calls it, so "never fires" means nothing in the actual running game reaches that code, contradicting the earlier static-disassembly read of `nnMain` (which appeared to call it unconditionally). There must be an earlier conditional branch gating this whole path that wasn't found, or a difference between Ryujinx's emulation and real hardware for whatever that branch checks.

This directly validates a concern raised early on ("is the debug text drawing method possibly removed/gated?") that seemed addressed at the time (the guard byte gets zeroed at construction, no debug-build check *inside* `createInstance`/`initialize` themselves) but was never actually conclusive - that evidence only showed the functions behave normally *if* reached, never that they're actually reached.

**Decision: pivot to a raw quad rather than chase the `DebugFontMgrNvn` gate further.**

Before committing to building a from-scratch NVN pipeline (needs a real compiled shader binary - NVN can't compile from source at runtime), found a much better middle path: `sead::PrimitiveDrawMgrNvn` - a ready-made C++ wrapper with a `drawQuadImpl(DrawContext*, const Matrix34<f32>&, const Color4f&, const Color4f&)` function, plus `beginImpl`/`endImpl`/`createInstance`/`prepareImpl` in the same shape as `DebugFontMgrNvn`. If this class isn't gated the same way, it sidesteps needing our own shader entirely - if it *is* gated the same way, that's useful information too (suggests something broader affects sead's debug-draw-adjacent singletons specifically, narrowing where to look next).

Added the same style of direct diagnostic hooks on `PrimitiveDrawMgrNvn::createInstance`/`prepareImpl` to test this empirically before investing further.

**Result: also never fires.** Same as `DebugFontMgrNvn` - confirmed via a real hook, not static guessing. Two for two now. `DebugFontMgrNvn`, `DebugFontMgrJis1Nvn`, and `PrimitiveDrawMgrNvn` are all guarded by the identical null-check shape in `procDraw_`'s `swapUniformBlockBuffer` calls, and both tested members of that group are confirmed never constructed. This is now a real pattern, not a coincidence: strongly suggests every sead debug-visualization singleton shares one common gate that isn't satisfied in this environment - consistent with these being genuine, intentionally-disabled developer tooling in a shipped retail build (this is a real retail cartridge dump, not a dev unit image), not a bug in our own reverse-engineering.

**Conclusion: not worth testing further pre-built helper classes - committing fully to a from-scratch raw NVN pipeline.** Next needed: source a real compiled shader binary (`System/Sead/primitive_drawer_nvn_shader.bin` or similar, loaded via `sead::FileDeviceMgr::tryLoad`), set up an `NVNprogram`, build a vertex buffer, and find the remaining offsets (`BindProgram`, `BindVertexBuffer`, `SetViewport`) - none of this depends on the gated systems above.

## Building the raw pipeline: how a shader binary becomes an NVNprogram

`PrimitiveDrawMgrNvn::prepareFromBinaryImpl`'s code is still valid to read even though this particular instance never runs at runtime - the API pattern it uses is real, ordinary NVN usage, not something gated. Decompiled it in full to learn the real shader-loading sequence, since this is the actual hard problem for a from-scratch pipeline (NVN can't compile shaders from source at runtime).

**The shader binary's header format** (first 16 bytes, read as 4x `uint32`, `param_3` in the decompile is the raw file bytes):
```
header[0] = vertex shader control-section byte offset (from file start)
header[1] = fragment shader control-section byte offset (from file start)
header[2] = vertex shader data-section byte offset (from file start)
header[3] = fragment shader data-section byte offset (from file start)
```

**The real sequence**:
1. Upload the *entire raw file* into a GPU-visible `NVNmemoryPool` + `NVNbuffer` (same `MemoryPoolBuilder`/`BufferBuilder` pattern already used elsewhere in this doc) - the shader's compiled microcode ("data section") needs to live in GPU memory, referenced by GPU address.
2. `nvnProgramInitialize(program, device)`.
3. Build an `NVNshaderData[2]` array (one entry per stage - vertex, fragment), each entry being `{ uint64 gpuDataAddress; uint64 cpuControlPointer; }`:
   - `entry[i].data` = `nvnBufferGetAddress(uploadedBuffer)` + `header[2 or 3]` (the GPU address of that stage's compiled microcode, inside the uploaded buffer)
   - `entry[i].control` = *raw file base pointer* (CPU-side, not the GPU buffer) + `header[0 or 1]` (the "control section" - shader metadata - is read directly from CPU memory, never uploaded to the GPU)
4. `nvnProgramSetShaders(program, 2, &shaderDataArray[0])`.

**Vertex format this specific shader (`primitive_drawer_nvn_shader.bin`) expects** - 3 attributes, stride `0x24` (36 bytes):
```
attribute 0: format 0x22, offset 0x00
attribute 1: format 0x16, offset 0x0c
attribute 2: format 0x2e, offset 0x14
```
Format codes (`0x22`/`0x16`/`0x2e`) not yet decoded against NVN's real `NVNformat` enum - needed before this shader can be fed correct-looking vertex data. This shader is used for 3D primitives (spheres, cylinders, wireframes) via `sead::PrimitiveDrawUtil::setXVertex` helpers, so it likely expects position + normal + UV or similar, not a minimal 2D-quad-only layout - may be more shader than a flat colored quad actually needs. **Worth checking whether a simpler, purpose-built shader exists elsewhere in the game's shipped files before committing to matching this exact one.**

**Confirmed real table offsets from this decompile** (not yet individually xref-anchored the rigorous way - same caution as any decompiler-sourced address in this doc): `nvnProgramInitialize` (`~0x2596a68`), `nvnProgramSetShaders` (`~0x2596a80`), `nvnBufferBuilderSetDevice`/`SetDefaults`/`SetStorage` (`~0x2596b88`/`~0x2596b90`/`~0x2596b98`), `nvnBufferInitialize` (`~0x2596bb8`), `nvnBufferMap` (`~0x2596bd0`), `nvnVertexAttribStateSetDefaults`/`SetFormat` (`~0x2597208`/`~0x2597210`), `nvnVertexStreamStateSetDefaults`/`SetStride` (`~0x2597230`/`~0x2597238`).

**Still needed before a real attempt**: xref-anchor the offsets above properly, decode the 3 vertex format codes, find/choose an actual shader file to load (and confirm it exists as a real romfs path, not just a string in the binary), `BindProgram`/`BindVertexBuffer`/`SetViewport` offsets (never chased down - only `DrawArrays` was needed for the abandoned text-overlay plan), and `sead::FileDeviceMgr::tryLoad`'s real address for loading the file ourselves.

## Shader-asset hunt: what actually ships in this retail ROM

User rejected both the font-text and PrimitiveDrawMgrNvn-reuse plans - the goal is a genuinely custom quad (our own vertex data), not text or a fixed primitive shape. Checked the real ROM (`C:\Users\dylan\shenanigans\Personal\Nintendo\Switch\Zelda BotW\RE\ROM`, path already registered with the totk-mcp server) for what compiled NVN shader assets actually exist, since NVN can't compile shaders from source at runtime - we can only ever feed `nvnProgramSetShaders` bytecode that already exists somewhere.

**`primitive_drawer_nvn_shader.bin` does not exist anywhere in this retail ROM** - not loose in romfs, not inside `Pack/Bootup.pack`. Only referenced as a string constant in code. This is the last piece of evidence needed to fully explain why `PrimitiveDrawMgrNvn` never constructs at runtime (findings above): its own required asset was stripped from the shipped game, not just gated behind a runtime flag.

**`System/font/nvn_font/nvn_font_shader.bin` does exist** (loose, 8448 bytes, confirmed real via `sead::DebugFontMgrNvn::initializeFromBinary` at `0x7100aff42c`, which is the actual, real loader for this exact file - re-decompiled and fully understood, see below). But its vertex shader does **procedural per-character glyph expansion**, not plain vertex-pulling: `DebugFontMgrNvn::print()` (`0x7100aff94c`) writes exactly one `int` (a character code, range-checked and offset by `-0x20`) per glyph into a ring buffer, plus one shared uniform block (a 4x3 combined view-projection-model matrix + a `Color4f` tint), then calls `nvnCommandBufferDrawArrays(cmdBuf, mode=7, first=0, count=charCount*4)`. The vertex shader itself - opaque compiled bytecode - is what turns each character code into a glyph quad's 4 corners, sampling a fixed 128x128 font atlas texture (`nvn_font.ntx`) with baked-in per-glyph UV cells. **We never get to specify 4 independent vertex positions, UVs, or per-corner colors this way** - only "which character" + one transform + one tint, forever locked to font-atlas semantics. Confirmed not to fit "a raw quad we construct."

**`bgsh/__ArchiveShader.bnsh`** (145,968 bytes, real `BNSH`-magic header) exists inside `Pack/Bootup.pack` → `Layout/Common.sblarc` (both extracted via the totk-mcp SARC tools to `scratchpad/bootup_extract` and `scratchpad/common_extract`). This is BotW's actual UI/HUD shader archive - the format Nintendo's real graphics middleware (**`agl`**, "agl::" namespace, confirmed via `search_functions_by_name query:"agl"` - hundreds of real functions: `agl::VertexBuffer::setUpBuffer/setUpStream`, `agl::ShaderProgram::initialize(ResBinaryShaderProgram, Heap*)`, `agl::DrawContext`, `agl::UniformBlock`, `agl::ResShaderArchive`, `agl::ResBinaryShaderArchive`) uses for on-screen UI rendering, as opposed to sead's raw/debug-only NVN wrappers used everywhere else in this doc. This is structurally the right layer for "construct our own vertex buffer, bind a real shader, draw arbitrary geometry" - `agl::VertexBuffer`/`agl::ShaderProgram` are ordinary, general-purpose, not tied to font or primitive-shape semantics.

**Not yet solved**: how to extract one named shader program out of a `.bnsh` archive. `agl::ResShaderArchive::setUp` (`0x7100b6d134`, decompiled in full) is real and confirmed to operate on this exact container format, but it's only an **endian-fixup pass** - walks a classic Nintendo nested "ResDic"-style resource dictionary tree (5-7+ levels of relative-offset pointer chasing seen in one decompile alone) to byte-swap fields after load, not a by-name lookup. `ShaderArchives::load` (`0x7100f3a258`) is a red herring for our purposes - it loads a *different*, heavier system (`Shader/<name>.product.bfsha`, tied into `ksys::res::ResourceMgrTask` and `gsys::ModelShaderArchive` - the 3D model material system, not UI). The real "get me a `ResShaderProgram`/`ResBinaryShaderProgram` by name from a `ResShaderArchive`" accessor hasn't been located yet - next step if this path continues.

**Where things stand**: three real options, meaningfully different cost/reward -
1. Font shader (fully working, RE'd end-to-end) - fast but semantically wrong for "our own quad."
2. `agl` BNSH archive - structurally right, but extracting a named program is its own separate resource-format RE task (dictionary-tree lookup, not yet found).
3. Compile our own shader from source - unknown feasibility, needs Nintendo's offline NVN shader compiler which we don't have confirmed access to.

## BREAKTHROUGH: real Nintendo NVN SDK tooling is available locally

User has a real, official copy of Nintendo's NVN SDK dev tools at `C:\Users\dylan\repos\NVN` (`NvnTools/GraphicsTools/`) - this is the actual, legitimate shader compiler Nintendo's own developers (and BotW's) used, not a reverse-engineered substitute. This makes options 2 and 3 above moot: we no longer need to reverse-engineer `agl::ShaderProgramArchive`'s by-name lookup, and we don't need a homebrew Maxwell assembler (`uam`/deko3d's `.dksh` format was explored as a fallback and abandoned once this was found - real tooling beats guessing at binary compatibility).

**`ShaderConverter.exe`** (`NvnTools/GraphicsTools/ShaderConverter.exe`, "Copyright (C) Nintendo All rights reserved") compiles GLSL directly to real, retail-authentic `.bnsh` files via the actual GLSLC compiler (confirmed real output: `GLSLC version: 17.24, GPU Code version: 1.16`). Confirmed working invocation:
```
ShaderConverter.exe --vertex-shader <file.vert> --pixel-shader <file.frag> --source-format glsl --api-type Nvn --code-type Binary --output-path <full path ending in .bnsh> --output-name <name>
```
Gotchas found by trial: `--api-type` must be `Nvn` (not `NX` - that value is silently accepted by the parser but produces a subtly different/wrong result; `Nvn` is the correct API family, confirmed via the compiler's own internal class names like `NvnShaderPoolBinarizer`/`CompileOptionNvn` found in `ShaderConverter.dll`'s string table). `--code-type` must be `Binary`. `--output-path` must be the **full output file path including filename and `.bnsh` extension** - passing just a directory silently produces "Failed to output binary" with no further explanation.

**Verified correct**: the repo ships a working reference example - `solid_red.vert`/`solid_red.frag`/`solid_red.bnsh` (a trivial hardcoded-red shader). Recompiling `solid_red.vert`/`.frag` with the invocation above reproduced a file of the **exact same size** (12552 bytes) with only **11 bytes differing total**, clustered in two tiny regions near the very end of the file (offsets ~12312-12323 and ~12476) - almost certainly a build timestamp/ID, not shader substance. This confirms the compile pipeline is correct and deterministic for real, loadable NVN shader binaries.

**Next**: write a real custom GLSL pair (proper vertex position input instead of solid_red's hardcoded `vec4(1,0,0,1)` output, uniform-driven color), compile the same way, then figure out how to extract that specific program's control+data sections from the resulting `.bnsh` and feed them through the already-proven raw NVN loading pipeline (matching `sead::DebugFontMgrNvn::initializeFromBinary`'s call sequence) - this file is far simpler than BotW's own 145KB multi-variant panel archive (one vertex + one fragment program only), so hand-parsing its specific structure directly should be much more tractable than the general by-name `ShaderProgramArchive` lookup abandoned above.

## First real test: builds, doesn't crash, but nvnProgramSetShaders rejects it (result=0)

Compiled `wiixl_quad.vert`/`.frag` (position passthrough + uniform color) with `ShaderConverter.exe`, hex-inspected `wiixl_quad.bnsh` to find two 256-byte program blocks at absolute file offsets 8288 (vertex) and 8544 (fragment) - identified via a 64-byte descriptor-record pattern at file offset 432/496 (leading `u64` offset relative to the `grsc` block start, differing by exactly 0x100=256; trailing fields `0x100`/`0x880` repeated in both records). Within each 256-byte block, visually identified what looked like real Maxwell SASS (clean repeating 8-byte instruction groups, e.g. `00 ff 07 08 80 7f d8 ef` / `01 ff 47 08...` / `02 ff 87 08...` - an incrementing register field across 4 near-identical instructions, matching a 4-component `vec4` MOV pattern) starting 32 bytes into the block, and assumed control=[0,32), code=[32,256) per block.

Wired this into `nvn_overlay.hpp`'s new `EnsureQuadInitialized()`/`DrawQuad()` (see file for full implementation). All supporting NVN offsets (`nvnProgramInitialize`=0x2596a68, `nvnProgramSetShaders`=0x2596a80, `nvnBufferBuilder*`, `nvnVertexAttribState*`, `nvnVertexStreamState*`, `nvnCommandBufferBindVertexAttribState/BindVertexStreamState/BindVertexBuffer/SetViewport/SetScissor`) were independently xref-anchored via the strict method (find the function-name string → find its xref inside `nvnLoadCProcs` → trace the *preceding* `adrp x8; ldr x8,[x8,#SLOT]` pair that the *following* `str x0,[x8]` actually stores into - `nvnLoadCProcs` interleaves "resolve name N → prep name N+1's string → store name N's result", so naive nearest-neighbor reading is off by one step). All of these matched their earlier decompiler-derived values exactly, no shifts found this round.

**Real test result (Ryujinx)**: no crash. `EnsureInitialized()` (the original, already-proven pipeline) still succeeds every frame as before. The new `EnsureQuadInitialized()` reaches step E (`nvnProgramSetShaders`) cleanly every frame - pool init, buffer init, and `nvnBufferGetAddress` all succeed (`result=1`) - but **`nvnProgramSetShaders` itself returns `result=0` (failure) on every single frame**, so `g_QuadInitialized` never becomes `true` and nothing ever draws.

**Ruled out via the real, official `nvn.h`/`nvnTool_GlslcInterface.h` headers** (found locally at `C:\Users\dylan\repos\NVN\NvnHeader` - see "BREAKTHROUGH" above, this is the same real SDK the shader was compiled with):
- `NVNshaderData` field order (`data` first, then `control`) - confirmed byte-for-byte correct against the real struct definition (`nvn.h:4165`).
- Struct sizes for `NVNmemoryPool`(256)/`NVNmemoryPoolBuilder`(64)/`NVNbuffer`(48)/`NVNbufferBuilder`(64) - all matched what was already in use exactly. `NVNprogram`'s *real* size is 128 bytes (`nvn.h:284`) - we'd used a generous 0x290=656-byte guess; not believed to be the cause (oversized should be safe) but worth shrinking for cleanliness.
- `nvnProgramSetShaders`'s calling convention itself - no ordering/stage-tagging requirement documented beyond the `NVNshaderData` array itself.

**Leading suspect**: the 32-byte control/code split within each 256-byte block was a visual guess, not derived from any authoritative structure. `nvnTool_GlslcInterface.h` documents a real `GLSLCgpuCodeHeader` struct (`controlOffset`/`dataOffset`/`controlSize`/`dataSize` fields) but this describes the *offline compiler's* intermediate `GLSLCoutput` blob shape, not necessarily what's packed into the final retail `.bnsh` - unclear if it applies directly. Circumstantial evidence the true control section may be larger than 32 bytes: BotW's own font shader (a real, decompiled, much more complex production shader) has real control sections of 3408/2464 bytes per stage (derived from its confirmed-correct 4-word header format) - though that shader is far more complex than our trivial 4-instruction test case, so the comparison isn't conclusive either way.

**Not yet tried**: `agl::ShaderProgram::setUpAllVariation(bool)` (found via `search_functions_by_name query:"agl"`, not yet decompiled) looks like the likely real call site that finally invokes `nvnProgramInitialize`/`SetShaders` for a `ShaderProgramArchive`-sourced program - decompiling it would give the authoritative control/data offset computation for the actual packed BNSH format, the same way `sead::DebugFontMgrNvn::initializeFromBinary` did for the simpler raw format. `agl::ShaderProgramArchive`'s per-program entry stride (0x428=1064 bytes, confirmed via `searchShaderProgramIndex`'s decompile) is suspiciously much larger than the 256-byte block assumed here, which may mean the whole "256-byte block" framing was too small a unit in the first place, not just wrongly split.

## Real-archive pivot: found the real loader, hit a real format-shape mismatch

Rather than keep guessing the control/code split, traced the actual call chain: `agl::ShaderProgramArchive`'s constructor (`0xb6d900`) + real "load from binary" function (`FUN_7100b6de44` @ `0xb6de44`, xref-found as the sole caller of `agl::ResShaderArchive::setUp`) + `searchShaderProgramIndex` (`0xb6ec54`) + `ShaderProgram::setUpAllVariation` (`0xb38888`) - this is the real, working code BotW itself uses to load `.bnsh` archives. Constructing a real `agl::ShaderProgramArchive` object and calling these directly (via `WiiXLaunch::GetTargetFunction`, not `NvnFn` - the latter is only for the double-indirected `nvnLoadCProcs` table, using it on real compiled functions was one bug hit along the way and crashed immediately) got further than any hand-parsing attempt: construction succeeds, and with a real heap (found via `sead::HeapMgr::findContainHeap(heapMgrInstance, gameFramework)` - `ksys::util::getCurrentHeap()` returned null on the Presentation Thread, no TLS "current heap" scope active at `present_` time) the allocator itself runs.

**Real crash, real diagnosis**: `FUN_7100b6de44` reads a "program count" via `*(int*)(rawBytes + *(uint*)(rawBytes+0x10) + 0x18)`. Computed this exact expression against our actual file bytes in Python *before* the next test and got `0x6c616972` - garbage. The crash log's fault registers showed the exact same value. Confirmed, not guessed: `header[0x10]` (a `uint32`, value 12314 for our file) does **not** point to a program-count structure - it points into the `_STR` string-table block, specifically at the archive's own name (`"wiixl_quad"`). Verified this against BotW's own real `__ArchiveShader.bnsh` too (same header shape, same field pointing at its own name).

Also mapped, this time with real confirmation via cross-referencing computed string offsets against actual ASCII content:
- Header layout: magic(4) + version fields(8) + BOM/header-size(4) + **name string offset**(4, at +0x10) + two `uint16`s at +0x14 (first is 0, second - readable as `header[+0x16]` - equals 96, `grsc`'s real fixed offset for every BNSH file checked) + **relocation table offset**(4, at +0x18) + **total file size**(4, at +0x1c).
- String pool format (`_STR` block): length-prefixed, not null-terminated-with-padding as first assumed - `[uint16 length][raw chars, no terminator]` repeating. Confirmed by reading real lengths (14 for "wiixl_material", 7 for "o_Color", 10 for "a_Position") that exactly matched the following string's character count.
- Three real `_DIC` (dictionary/ResDic binary search tree) blocks, one each for: vertex attributes (1 entry: `a_Position`), fragment outputs (1 entry: `o_Color`), and uniform blocks (1 entry: `wiixl_material`) - real reflection data (from `--reflection-full`), not the program/shading-model index needed for the archive loader.

**Conclusion**: `agl::ShaderProgramArchive`'s raw-`.bnsh` loading path, as written, assumes a structural convention (count/array reachable via `header[0x10]`) that plain `ShaderConverter.exe` single-model output does not follow - `header[0x10]` means something else (name-string offset) for this file shape. Whether BotW's own bigger `__ArchiveShader.bnsh` genuinely uses `header[0x10]` differently (multi-model archives may have an actual count there instead of a name) or whether the original interpretation of that file was *also* wrong and never got verified as rigorously as this one was is still open - the big file's structure was only ever spot-checked, not confirmed against real Maxwell code or cross-referenced string offsets the way this session's later work did. Checked one adjacent lead (`gsys::ModelShaderArchive`/`ShaderArchives::load`, which handles `.bfsha` material archives) and it's a dead end too - genuinely different file format/magic, not just a different code path for the same bytes.

**Current status**: reverted to driving `nvnProgramSetShaders` directly (bypassing the archive loader entirely), now searching the full cross product of independent vertex/fragment control-section sizes (31x31, `[8,248]` step 8) rather than the earlier synchronized single-split search that came back empty even with the correct `0x62` shader-pool flag. Result of this test not yet known as of this writing.

## Raw quad pipeline: implemented, compiled, ready to test

Wrote real GLSL (`wiixl_quad.vert`/`.frag`): vertex shader passes `a_Position` (vec4, location 0) straight to `gl_Position` with no transform; fragment shader outputs a uniform `u_Color` (block `wiixl_material`, binding 1). Compiled with the real `ShaderConverter.exe` to `wiixl_quad.bnsh` (12688 bytes).

**Hand-parsed the `.bnsh`'s byte layout directly** (no need for the `agl::ShaderProgramArchive` by-name lookup after all, since this file only has one program per stage): magic `BNSH` at offset 0, a `grsc` block starting at offset 96 whose first two u32 fields (both `0x2fa0`=12192) give the block's total size - `96+12192=12288` exactly matches where the `_STR` string table begins, confirming the whole span. Found a 64-byte-strided pair of descriptor records at file offsets 432/496 (leading u64 = offset within `grsc` to a 256-byte "program block", trailing fields matching stride/size) pointing at `grsc+0x2000` and `grsc+0x2100` - i.e. absolute file offsets **8288** (vertex) and **8544** (fragment). Hex-dumping those confirmed it directly: each is a 32-byte NVN control header followed immediately by real Maxwell SASS (verified by eye - e.g. the vertex block's code at offset 8320 is four instructions incrementing a register field by one each time, `00 ff 07 08.../01 ff 47 08.../02 ff 87 08.../03 ff c7 08...`, exactly matching a 4-component `vec4` passthrough), zero-padded out to the 256-byte block boundary. Extracted both 256-byte blocks (512 bytes total, offsets [8288, 8800) of the file) and embedded them directly as a C array in `nvn_overlay.hpp` (`QuadShader::kBytes`) - no runtime file loading needed for this test.

**All remaining NVN function offsets independently xref-anchored** the same rigorous way as everything else in this doc (string → unique xref inside `nvnLoadCProcs` → trace the *preceding* `ldr x8,[x8,#SLOT]` that the following `str x0,[x8]` actually consumes, since the resolver loop interleaves "resolve name N, prep name N+1's string, store name N's result" so naive nearest-neighbor reading is off by one step - this exact trap bit us multiple times earlier in this doc). All matched the earlier decompiler-derived guesses exactly this round (no slot shifts found): `nvnProgramInitialize`=0x2596a68, `nvnProgramSetShaders`=0x2596a80, `nvnBufferBuilderSetDevice`=0x2596b88, `SetDefaults`=0x2596b90, `SetStorage`=0x2596b98, `nvnBufferInitialize`=0x2596bb8, `nvnBufferGetAddress`=0x2596bd8, `nvnBufferMap`=0x2596bd0, `nvnVertexAttribStateSetDefaults`=0x2597208, `SetFormat`=0x2597210, `nvnVertexStreamStateSetDefaults`=0x2597230, `SetStride`=0x2597238, `nvnCommandBufferBindProgram`=0x2597320, `BindVertexAttribState`=0x2597310, `BindVertexStreamState`=0x2597318, `BindVertexBuffer`=0x2597328, `SetViewport`=0x2597448, `SetScissor`=0x2597460.

**Implementation** (`include/nvn_overlay.hpp`, `NvnOverlay::EnsureQuadInitialized()` + `DrawQuad()`): separate 4096-byte pool/buffer from the existing command/control one (`g_QuadPoolMemory`, layout: shader bytes @0, vertex data @512, uniform color @768). `EnsureQuadInitialized()` copies the embedded shader bytes into the pool, builds the memory pool + one `NVNbuffer` wrapping it, calls `nvnProgramInitialize`/`nvnProgramSetShaders` with `{data: gpuAddr, control: cpuPtr}` pairs computed the same way as the proven sead-format loader (control read directly from the embedded C array in CPU memory, data computed as the uploaded buffer's GPU address + code offset within its block), and sets up one vertex attribute (format `0x2e` = matches the float4 format code seen on the confirmed-real `PrimitiveDrawMgrNvn` shader) + one vertex stream (stride 16). `DrawQuad()` maps the buffer, writes 4 clip-space vertices (a centered quarter-screen quad, triangle strip) and the uniform color fresh each call, binds program/vertex state/vertex buffer/uniform buffer, sets a full 1280x720 viewport+scissor, and issues `nvnCommandBufferDrawArrays(mode=TRIANGLE_STRIP, 0, 4)` - all via a second, independent command buffer submitted to the same queue after `Orig()`, same safe-ordering reasoning as the abandoned text-overlay plan.

**Wired into `main.cpp`**: `PresentHook` now calls `NvnOverlay::DrawQuad(gameFramework, 1.0f, 0.0f, 0.0f, 1.0f)` (solid red) instead of the dead-ended `DrawTextOverlay`. Builds clean (`build_switch.bat` succeeds, deploys to `deploy/switch/...`).

**Untested / real unknowns going into the first real run**: whether `nvnCommandBufferBindUniformBuffer`'s `stage` parameter (currently guessed as `1`) is right for binding to the fragment stage specifically; whether vertex format code `0x2e` really means 4x float32 (borrowed from a different shader's usage, never independently confirmed against a real `NVNformat` enum); primitive mode `5` for `TRIANGLE_STRIP` (assumed to mirror OpenGL's enum value, not confirmed against NVN's own `NVNdrawPrimitive`); and the biggest one - whether `NVNprogram`'s guessed size (`0x290`, generously oversized, not sourced from a real header) is actually big enough. Any of these being wrong should show up as a crash or a blank/wrong-looking quad, in which case the crash site or visual result narrows it down fast, same as every other round this session.

## Original root cause note (present_ hook offset, first test)

Root cause: a hex arithmetic slip. `present_`'s real address is `0x7100af9418`; subtracting the NSO base (`0x7100000000`) gives `0xaf9418`, but `main.cpp` had `0x8f9418` (`a`↔`8` typo) - off by `0x2000000`. `PresentHook::Install()` happily patched *some* address without crashing, just not the right one - real `present_` kept running completely untouched, so nothing our code does could ever have run. This explains every symptom from both test rounds: no crash (wrong address wasn't load-bearing), and zero output (our callback never fires). Fixed in `main.cpp`; every other offset in the project was individually re-verified against its Ghidra address using the same subtraction and all were already correct - this was an isolated typo, not a pattern.

## The grsc-base breakthrough: why FUN_7100b6de44's "program count" was garbage

Root cause of the archive-loading crash, finally found - and it fully un-shelves the real-archive approach.

`FUN_7100b6de44` (agl::ShaderProgramArchive's real "load from binary" function) computes its program table like this:

```
lVar1 = rawBytesPtr + *(uint32_t*)(rawBytesPtr + 0x10);
programCount = *(int32_t*)(lVar1 + 0x18);
```

We were calling this with `rawBytesPtr = <pointer to file offset 0>` - the start of the whole `.bnsh` file. At file offset 0, `*(uint32_t*)(rawBytesPtr+0x10)` is the BNSH container's own name-string offset (previously mapped as part of the outer header). Following that lands inside the `_STR` string pool, and `+0x18` from there reads `0x6c616972` - the ASCII tail of `"wiixl_material"` (`"...mate` **`rial`**`"`) - as a "program count". That's a ~246MB allocation request, which is what actually corrupted the heap inside `sead::ExpHeap::allocFromHead_`, not the heap itself. This is why switching the `findContainHeap` probe address (graphicsNvn -> gameFramework) produced a *different* heap pointer but the *identical* crash: both heaps were being asked to satisfy the same bogus request.

Verified the real fix by replicating this exact arithmetic in Python against BotW's own shipped, successfully-loading `__ArchiveShader.bnsh` (pulled from `common_extract/bgsh/`): using `rawBytesPtr = <file offset 0>` gives the same kind of garbage there too (`589938`). But using `rawBytesPtr = <the grsc sub-block's own start>` instead - i.e. `grscBase + *(uint32_t*)(grscBase+0x10) + 0x18` - gives **20**, a completely sane program count for a big shared archive. The identical formula against our own `wiixl_quad.bnsh`, using its `grsc` offset (read from the BNSH header's `u16` at absolute offset `0x16` - confirmed `96` in every `.bnsh` seen so far, matching the previously-established "grsc always starts at fixed offset 0x60" finding), gives exactly **1** - our single vertex+fragment program.

**The fix**: `FUN_7100b6de44`'s raw-bytes argument must point at the `grsc` sub-block (`fileBytes + grscOffset`), not file offset 0. The outer BNSH container header (magic, BOM, name offset, relocation table offset, file size) is a wrapper around `grsc`, which has its own internal header with its own `+0x10` field - the loader operates entirely within `grsc`'s namespace, never the outer container's.

`EnsureQuadInitialized` in `nvn_overlay.hpp` now: acquires a heap via `findContainHeap` (probing with `gameFramework`'s address, now expected to be safe since the huge-alloc root cause is fixed), constructs a real `agl::ShaderProgramArchive`, calls `FUN_7100b6de44` with the corrected `grsc`-relative pointer, and - since the program count is verified as exactly 1 - skips the name-search step entirely and uses program index 0 directly, then calls `setUpAllVariation(program, false)` to let the game's own, real, already-proven-correct code build the `NVNshaderData`/control block and call `nvnProgramSetShaders` itself. This replaces the brute-force split search entirely, which was fundamentally the wrong model - the real control block isn't a raw offset into our compiled bytes at all, it's built by this parser.

Deployed, not yet tested as of this writing.

## Chasing the archive loader through three more real bugs

After the grsc-base fix (previous section), the archive-loading path hit three further real, root-caused bugs in quick succession, each confirmed via live diagnostics rather than guessed:

1. **A second corrupt self-relative offset**: `agl::ResShaderArchive::setUp()` (called internally by `FUN_7100b6de44`) also walks a "source table" via a self-relative offset at `grscBase+0x18` (target = its own address + the stored offset). BotW's real archive has a small, sane value there (`2048`, verified). Ours - `ShaderConverter.exe`'s bare output never populates this - held `70914`, sending the loader ~58KB past our 12688-byte file. Fixed by patching that one field (in a writable copy of our embedded bytes) to point into the outer header's known-all-zero padding.

2. **The byte-swap fixup pass**: even after the above fix, the exact same crash persisted. Traced it to `setUp()`'s mandatory endian-fixup pass: `agl::ModifyEndianU32`'s dispatch table is selected by `ourArg(0) XOR *globalSwapFlag`, and that flag is genuinely `1` live at runtime (confirmed via an in-hook diagnostic read, not just Ghidra's static image) - meaning every real archive load byte-swaps its header fields as a matter of course (presumably because these resource files are authored/shipped in a legacy on-disk convention). Our file, freshly compiled today by the real `ShaderConverter.exe` running natively, is already correct-order and needs no such swap. `setUp()` has its own documented escape hatch for this: it checks bit 0 of a flag byte at `grscBase+0xc` and no-ops entirely if already set. Pre-setting that bit before calling the loader skips the whole fixup pass cleanly.

3. **`operator new[]` silently returning null**: past both of the above, the crash moved to a clean NULL-pointer fault inside `agl::ShaderProgram::initialize`, called internally by the loader's per-program loop. Verified via live diagnostic that the program count genuinely computes to `1` at runtime (matching the earlier static analysis), and that the archive constructor correctly zeroes the allocation gate - so the ~1KB `ShaderProgram` array allocation should run. It was returning null regardless of which of two different, independently-valid heaps (`findContainHeap` probed with `graphicsNvn`'s address, then `gameFramework`'s) were passed explicitly. Root cause: `sead::HeapMgr::getCurrentHeap()` reads a TLS slot and falls back to `*(HeapMgr+8)` only when that slot is empty - true for our injected Presentation Thread hook, which was never set up as a "sead thread." Even though we pass an explicit heap to the allocator (which does skip its own `getCurrentHeap()` check when given one), something deeper inside that heap's virtual `alloc()` evidently still consults `getCurrentHeap()` for lock/ownership bookkeeping, failing regardless of which heap was named. `sead::HeapMgr::setAllocFromNotSeadThreadHeap(HeapMgr*, Heap*)` is a real, one-line engine function that writes exactly that `HeapMgr+8` fallback field - the intentional escape hatch for this exact scenario. Now called once before `loadFromBinary`.

`EnsureQuadInitialized` now: resolves `HeapMgr`, finds a heap via `findContainHeap(graphicsNvn)`, calls `setAllocFromNotSeadThreadHeap` to make that heap usable from this thread, patches the two corrupted archive fields (source-table offset, endian-fixup flag) in a writable copy of the embedded `.bnsh` bytes, constructs a real `agl::ShaderProgramArchive`, calls the real loader with a pointer to the `grsc` sub-block, and (since program count is verified as exactly 1) reads the resulting `ShaderProgram` array pointer via the corrected one-more-dereference (`kShaderProgramArrayPtrOffset`, not inline) before calling `setUpAllVariation`. Also enlarged the guessed `AglShaderProgramArchive` opaque struct size (`0x400` -> `0x4000`) as cheap insurance against the real object being bigger than assumed.

Deployed, not yet tested as of this writing.

## Abandoning the archive loader: real control sections found by their own magic number

Traced `agl::ShaderProgram::initialize`'s reflection-array walk (`agl::ResShaderProgram::getResShaderVariationArray`, a 5-level self-relative offset chain into the program record's own reflection sub-structure) and confirmed via direct Python replication against our real file bytes that the chain is **not corrupted** - it's completely safe, in-bounds, and terminates cleanly on legitimately all-zero data. `ShaderConverter.exe`'s bare CLI output (even with `--reflection-full`) simply never records any shader "variations" - that's a real, structural tooling gap between a plain single-shader compile and what a proper multi-model archive-builder output would contain. Confirmed `--merge-shader-file` (a real "combine compiled shaders into an archive" mode) doesn't populate this either when given a single input - it's for deduplicating across many already-compiled shaders, not for producing variation records.

This made the reflection-driven archive-loading path a dead end regardless of further bug-fixing - `agl::ShaderProgram::forceValidate_` (the function that actually builds `NVNshaderData` and calls `nvnProgramSetShaders`) needs populated `ShaderCompileInfo` descriptors that only get built from variation data we don't have.

Decompiling `forceValidate_` fully, though, revealed the actual construction: `NVNshaderData.data = *descriptor + gpuCodeBase` (`gpuCodeBase` = `ShaderProgram+0x418`), `NVNshaderData.control = *(descriptor+0xc)` - a **plain CPU pointer**, read directly, no computation. Cross-checking against the real `nvn.h` SDK header confirmed `NVNshaderData::control`'s doc comment: *"Control section from the offline compiler."* - a real, GLSLC-emitted section, not something synthesized at load time.

`nvnTool_GlslcInterface.h` defines the real magic numbers GLSLC uses to mark these sections: `GLSLC_GPU_CODE_SECTION_CONTROL_MAGIC_NUMBER` (`0x98761234`) and `GLSLC_GPU_CODE_SECTION_DATA_MAGIC_NUMBER` (`0x12345678`). Searching our compiled `wiixl_quad.bnsh`'s raw bytes for these exact 4-byte patterns found exactly two of each (matching vertex+fragment), in file order matching the `--vertex-shader`/`--pixel-shader` compile order:

- Vertex control section: file offset `0x2f8`
- Fragment control section: file offset `0xb78`
- Vertex GPU code (data) section: file offset `0x2000`
- Fragment GPU code (data) section: file offset `0x2100`

This retroactively explains why **both** exhaustive brute-force rounds against the earlier extracted 256-byte per-program blocks (synchronized split, then the 961-combination independent cross-product) found nothing: the mental model itself was wrong. `control` was never a prefix-slice of a raw code blob at a guessable split point - it's a separate, self-identifying section GLSLC writes elsewhere in the file, found directly by its own magic number instead.

`EnsureQuadInitialized` now uploads the **entire compiled file** to a GPU-visible pool (flag `0x62`, same as before) and builds `NVNshaderData` directly from these real offsets - `data` as GPU addresses into the uploaded pool, `control` as CPU pointers into the same (CPU-mapped) backing memory. This is dramatically simpler than the full archive-loading path and doesn't depend on any of the archive/heap/endian-swap machinery investigated above (all still real, correct findings - just not needed for this).

Deployed, not yet tested as of this writing.

## Magic-number sections found, but nvnProgramSetShaders still rejects

Found real `GLSLC_GPU_CODE_SECTION_CONTROL_MAGIC_NUMBER` (0x98761234) and `..._DATA_...` (0x12345678) matches in the compiled file (see previous section) and corrected the data offsets to skip a 96-byte data-section header (confirmed against the independently-verified real SASS code locations from the very start of this investigation, 0x2060/0x2160). Still `nvnProgramSetShaders` returns 0 (clean rejection, no crash) every time.

New clue: both found "control" sections are byte-identical for every field sampled (magic, then 1, 0x10, 0x120, 0xb, 0x878, 0x880...). `nvn.h`'s real `NVNshaderStage` enum has `VERTEX=0`, `FRAGMENT=1`. If the field right after the magic is a stage tag, both matches read `1` (FRAGMENT) - meaning the two magic-number matches are NOT one-per-stage as assumed; the real vertex control section (if tagged the same way) hasn't been found, or this field isn't a stage tag at all. Pattern-matching on the magic number alone found *a* real section, but not necessarily *the* two we need in the shape we assumed.

Next step: decompile `agl::ShaderProgram::setUpForVariation_` (called at the top of `forceValidate_`, populates the `ShaderProgram+0x2e0/+0x300/+0x320/+0x340` descriptor pointers `forceValidate_` reads) to get the *real* traversal from program record to per-stage control section, instead of inferring it from magic-number pattern shape.

## Real breakthrough: decompiled the actual nnSdk driver implementation

User set up a fresh Ghidra project against the extracted `nnSdk` NSO (the real Nintendo NVN driver, previously only accessible as an opaque black box via indirect function-pointer calls). This unlocked ground truth instead of continued inference.

`nvnProgramSetShaders`'s real name isn't in the ELF export table - BotW resolves it via nnSdk's own internal runtime name lookup (`nvnBootstrapLoaderInternal` → a binary-searchable, offset-compressed string pool + a `.bss` function-pointer array populated at startup from a static source array at `PTR_DAT_710099b808`). Replicated the exact binary search algorithm in Python against the string pool/offset table to resolve `"ProgramSetShaders"` to its real function index (282), then read the static source array to get the genuine implementation address: **`0x71002e3494`**.

Decompiling it revealed the real validation the whole session had been missing: `control` isn't a raw compiled-bytes pointer at all - the driver dereferences it as a pointer to a struct it validates itself:
- `*(control+4)` must equal exactly `1`
- `*(control+8)` is a stage code that must fall in `[5, 0xE]` (a real switch statement, one case per code, each allocating a differently-sized internal wrapper object)
- The object is wrapped and dispatched through a shared, real vtable (decompiled the accessor methods too - `+0x10`/`+0x18` just echo back `control+4`/`control+8`, `+0x68` reads `control+0x714` as a 0-3 "stage bucket" array index, `+0x238` reads a capability flag byte at `control+0x7c0`)

Scanning our compiled `wiixl_quad.bnsh` for a location satisfying both the `+4==1` and `+8 in [5,0xE]` constraints found **exactly one** match: absolute file offset `0x1538` (`+4=1`, `+8=0xE` - a valid case, `+0x714=0` - bucket 0). Not two. This means `ShaderConverter.exe`'s combined `--vertex-shader X --pixel-shader Y` invocation produced a single **merged** control structure covering both stages, not one per stage as every earlier attempt this session assumed (control0/control1 being byte-identical, a red flag noted earlier but not explained until now, makes sense in hindsight if those were never real per-stage sections at all).

Current test: `nvnProgramSetShaders` called with **`count=1`** (not 2), `control` = host pointer to `fileBuffer+0x1538`, `data` = GPU address of the whole uploaded file's start (offset 0) - since embedded offset-shaped fields near the control struct (`0x3028`, `0x304e`) look absolute-file-relative, matching every other offset convention already confirmed in this format.

Deployed, not yet tested as of this writing.

## Hand-built control blobs (no more hunting in compiler output)

Confirmed via a clean solo-vertex `--separable` compile that raw `ShaderConverter.exe` output never contains the driver's expected control-object shape anywhere in the file - the earlier "merged" match at file offset `0x1538` was a coincidence (byte pattern happened to satisfy both gate checks), not a real per-stage structure. A fragment-only compile also matched by similar coincidence; a clean vertex-only compile matched nowhere at all, which settles it.

Re-reading `FUN_71002e3114` (the validator) clarified the actual contract: the value we pass as `NVNshaderData.control` is used *directly* as the raw blob - the driver allocates and builds its own internal wrapper object around whatever pointer we hand it. We never needed to construct a wrapper ourselves, only a well-formed blob.

Traced every accessor called during parsing (`FUN_71002e3318`, `FUN_71002e4554`, `FUN_71002e480c`, `FUN_71002e497c`) - every risky `memcpy`/loop is gated behind a length or count field that reads as 0 when the blob is zeroed, and the one internal switch (`FUN_71002e497c`, keyed on `control+0x68`) has a `case 0` that does only plain value reads, no dereferencing - which is exactly the case a zeroed blob lands on. So a blob that's all zero except three fields should be safe by construction:
- `control+4 = 1` (the driver's own "is this real" check)
- `control+8` = a stage code in `[5, 0xE]` (vertex=5, fragment=0xE - the exact code doesn't need semantic meaning, just validity and per-stage distinctness)
- `control+0x714` = a 0-3 bucket index, unique per simultaneous stage (vertex=0, fragment=1)

`data` left unchanged at the already-independently-verified real SASS addresses (`shaderGpuBase + 0x2080` / `+0x2180`).

Also confirmed while reading `FUN_71002e3494` past the parse stage: it later computes `*param_3 - poolBase` (`param_3` = our supplied `data` GPU address) and writes `0x12345678` (the DATA magic we found much earlier) there itself - the driver registers the code region for us; our GPU buffer never needed to already contain that magic.

Deployed, not yet tested as of this writing.

## `nvnProgramSetShaders` finally returns 1: the real gate was device-ISA matching, not "is this tampered"

Tested the all-zero-except-three-fields blob: no crash (first time all session), but still `result=0` for both `count=2` and a `count=1` bisection. Went back into `FUN_71002e3494`'s tail (the part after the per-entry parse loop) and found the real final gate:

```c
if (((uVar5==param_2||uVar5==0) && (uVar21==param_2)) && (uStack_38e4==param_2)) {
    // success path - sets *(byte*)(param_1+0xc) = 1
}
```

`uVar21` (count of entries with a non-null `data` pointer) already matched. `uStack_38e4` didn't: it's only incremented when `aiStack_2fdc[bucket+1] == *(int*)(device+0x30)` **and** `aiStack_2fdc[bucket] == *(int*)(device+0x2c)` - i.e. two ints read out of *our* control blob (via `FUN_71002e3318` calling vtable+0x20/+0x28, which disassemble to plain `ldr w0,[x8,#0xc]` / `ldr w0,[x8,#0x10]` off the raw blob) must match the real device's own ISA/architecture-version fields. We'd left both at 0. This isn't tamper detection - it's a genuine "does this shader's declared GPU architecture match the real device" check, and it's trivially satisfiable: `device` is already a pointer we hold, so `control+0xc`/`control+0x10` just need to be copies of `*(int*)(device+0x2c)`/`*(int*)(device+0x30)`, read live at setup time. Added that (`deviceIsaA=288, deviceIsaB=11` observed live) and **`SetShaders` returned `result=1`** - first success all session.

## Draw-time cascade: the empty-metadata blob's real cost surfaces downstream, not at SetShaders

With `SetShaders` succeeding, the very next real crash moved to `nvnCommandBufferDrawArrays` itself (confirmed by disassembling the actual call site from the return address in the crash trace, not trusting debug-line attribution, which pointed at the wrong statement due to inlining) - an `nn::diag` assertion abort, not a raw fault. Decompiled the actual abort site's string data directly rather than guessing: *"CommandBuffer has run out of control memory with no out-of-memory callback."* A real, documented NVN API (`nvn.h:3304-3322`, `nvnCommandBufferSetMemoryCallback`) that real engine code always registers and we hadn't. Bumping the initial pool size alone could never have fixed this - the driver aborts unconditionally the moment its *internal bookkeeping* needs to grow with no callback registered, independent of how much raw byte-pool space was handed to `AddCommandMemory`/`AddControlMemory`.

Resolved `nvnCommandBufferSetMemoryCallback` at runtime by name (not a hand-curated `Offset::` slot) using nnSdk's own internal resolver - the same `FUN_71002e0534(0, "nvnFoo")` binary-search mechanism found earlier for `nvnProgramSetShaders`, called directly via `exl::util::GetModuleInfo(ModuleIndex::Sdk)`'s live, ASLR'd base + the resolver's fixed file offset (`0x2e0534`). This is a strictly better technique than xref-hunting a slot in `main`'s `nvnLoadCProcs` table for every new function - worth using for any future NVN function this project needs.

Registering a callback took three iterations to get right, each with a real, diagnosable bug:
1. **First attempt** (advance cursor *after* calling `Add*Memory`): recursed ~150 frames deep into a guest stack overflow (crash address exactly one page below `SP` - the signature). Root cause: `Add*Memory` itself re-enters the callback *synchronously* (registering a new chunk apparently needs a sliver of its own bookkeeping room), and the reentrant call saw the same stale cursor, handing back the *identical* region forever instead of ever converging.
2. **Second attempt** (advance cursor first, but grant the *entire* remaining reserve in one shot - reasoning that `nvn.h` warns minimal grants cause frequent re-callbacks): backfired the other way - the very next, unrelated growth request (`minSize=256`) saw `remaining=0` and aborted even with a 256KB reserve. The driver evidently tracks each `Add*Memory` call as a distinct block record it may need *more of* later, not a simple running byte total - handing away the whole budget on the first call starves every subsequent legitimate call regardless of real memory available.
3. **Fix**: grant a modest, fixed-size chunk (16KB) per call instead, still advancing the cursor before the call. This is what let the callback converge without recursing *and* without prematurely exhausting the reserve - but the *reservation itself* was still being exercised on a fundamentally empty-metadata program, which is where this trail was interrupted (see next section).

Also worth noting for next time: `WiiXLaunch::Debug::DebugPrint`'s hand-rolled formatter (`debug_log.hpp`) only supports `%d/%i/%u/%x/%X/%p/%s/%f` - no `%zu` or any 64-bit-width specifier (it silently prints the literal text `%zu` instead of erroring). Cast `size_t` to `unsigned int` and use `%u`.

## Stepping back: the hand-built blob was never a real shader, just a stub that passed shape checks

Prompted by direct user pushback ("are we actually even giving it a shader, or just an empty object?") after three rounds of tuning the memory-callback's chunk size without asking whether the premise was sound. Correct call - the answer was no. The hand-built control blob (`docs` section above) is ~99.9% zeroed: exactly four populated fields (`+4=1`, `+8=stage code`, `+0xc/+0x10=ISA version`, `+0x714=bucket`), none of which describe *our actual shader* - no attribute count, no UBO count/binding, no sampler count, no register/output info. `SetShaders result=1` only ever meant "this blob's shape passed a shallow validator and the device-ISA gate," never "the driver understands a real shader." The `drawArrays`-time memory-callback cascade very plausibly wasn't a resource-sizing bug at all, but a symptom of the same root cause: command-stream generation code that iterates "for each declared interface variable" or "for each bound resource" behaving pathologically when every one of those counts is zero instead of matching what the actual compiled SASS code contains.

The systematic fix - decompiling all ~50 vtable-accessor offsets referenced by `FUN_71002e3318`'s two sub-helpers (`FUN_71002e4554`, `FUN_71002e480c`, `FUN_71002e497c`) to map every blob field, the same way `+0xc`/`+0x10` were found for the ISA gate - is still the ground-truth path if everything else fails, but is large (dozens of fields, several dynamic-length `memcpy`s and 2D array loops) and error-prone to get exactly right via blind field-by-field mapping.

## Reviving the real-archive path with a genuinely real shader file

Before committing to the large blind-mapping effort, re-examined why the earlier "real-archive pivot" (`agl::ShaderProgramArchive` parsing a `.bnsh` and letting the game's own code build `NVNshaderData`/`control` for us, fully debugged this session down to a working `grsc`-base fix + source-table-offset fix + endian-fixup fix + not-a-sead-thread-heap fix) was shelved: *not* a driver or loader bug, but a genuine, structural limitation of `ShaderConverter.exe`'s bare single-shader CLI output, which never populates the "variation" reflection records `agl::ShaderProgram::forceValidate_` needs (confirmed by replicating the reflection-array walk in Python against our own file's real bytes - the chain is safe and in-bounds, just legitimately all-zero, not corrupted). That conclusion still stands and isn't revivable through more driver-level fixes.

User found a real, shipped Nintendo shader (`Lib/NintendoSDK/TextureShader.bnsh`, pulled from an actual game romfs dump) and placed it in the project. Unlike our CLI-only compile, a genuine production asset should carry complete variation/reflection data by construction - a real test of whether the (already fully-debugged) archive loader now works given real input, sidestepping the entire hand-built-blob guessing problem.

Verified via direct Python inspection before writing any C++ (cheap, avoids another blind build/deploy/test cycle for a bad premise):
- Same `BNSH`/`grsc` header format as our own file; `grsc` at the same fixed offset `0x60`.
- `programCount` (via the same `grscBase + *(u32*)(grscBase+0x10) + 0x18` formula `FUN_7100b6de44` itself uses) = **1**, matching expectations for a simple shader.
- `grsc+0x18` self-relative "source table offset" = **2048** - already the exact sane value the notes recorded for BotW's own real `__ArchiveShader.bnsh`. Our own CLI-compiled file held garbage (70914) here and needed a manual patch; this real file needs none.
- `grsc+0xc` endian-fixup flag byte = `0x00` (bit 0 clear, i.e. "needs swap") - the normal state for a real production file per the earlier notes ("every real archive load byte-swaps its header fields as a matter of course"); our own freshly-native-compiled file needed this bit pre-set to *skip* the swap. This file needs no such override either.
- A scan for the driver's exact required control-blob shape (`+4==1` and `+8 in [5,0xE]`) found only the same two byte-identical `GLSLC_GPU_CODE_SECTION_CONTROL_MAGIC_NUMBER`-tagged (`0x98761234`) hits as before (now `+8=9` instead of `0x10`, still a coincidence, not a real per-stage blob - confirmed by cross-referencing against `nvnTool_GlslcInterface.h`'s real `GLSLCgpuCodeHeader` struct, which has no magic-number field at all and is far smaller than the `0x714+` bytes our own decompilation proved the driver actually reads. This magic-tagged section is GLSLC's own internal metadata header, not the low-level per-stage blob the driver wants - confirms the real blob genuinely is *synthesized at load time* by `forceValidate_`, not present pre-built anywhere in the file, and the archive loader is the only route to it).

Since this file appears to need **zero** of the two patches the earlier attempt's broken CLI file required, re-implemented the archive-loading path (`NvnOverlay::EnsureTextureShaderInitialized`/`DrawTextureShaderTest` in `nvn_overlay.hpp`, all the supporting types/offsets were already sitting unused in the file from the earlier attempt) targeting this real file verbatim - heap acquisition (`findContainHeap` + `setAllocFromNotSeadThreadHeap`), archive construction, `loadFromBinary` on the unpatched `grsc`-relative pointer, program-count sanity check, `setUpAllVariation(program0, false)` to let real engine code call `nvnProgramSetShaders` itself, then a minimal bind+draw reusing the now-working command-buffer/memory-callback machinery. Wired into `main.cpp`'s `PresentHook` in place of `DrawQuad` (which is left untouched in `nvn_overlay.hpp` for reversion). Won't produce a *correct* visual result (no texture bound), but the point is whether a genuinely correctly-shaped program survives `SetShaders`+bind+draw cleanly - if it does, that's final confirmation the hand-built blob was always the last real defect, not the driver pipeline.

## Real Ghidra project switch enabled real symbol names - and a real second bug

Up to this point in the session, the active Ghidra MCP connection had been pointed at the `nnSdk` project (stripped, `FUN_`-only symbols) for every decompile. Switched to the actual `main`/`U-King.nss` project (the one archive-loader addresses like `0xb6de44`/`0xb38888` belong to) - this one has real mangled C++ symbol names throughout (`_ZN3agl20ShaderProgramArchive...`, etc.), a much better signal for whether an address is being read correctly at all.

First test of `TextureShader.bnsh` through the archive loader crashed almost immediately - inside `agl::ResShaderArchive::setUp()` itself (not even reaching the program-count check), at a genuinely new address (`0x7100b6d0b8`) never seen before this session. Decompiled `setUp()` in full: its very first action is `ModifyEndianU32(0, grscBase, 0x14)` - swapping the leading 20 bytes of `grsc`, which **includes** the `grsc+0x10` "table offset" field the very next line reads. This field's raw (pre-swap) value in the file is `4` - already sane. Byte-swapping an already-correct `0x00000004` produces `0x04000000` (67,108,864) - a wildly out-of-bounds offset, and the next line (`grscBase + thatOffset + 0x14`) dereferences it, landing exactly on the garbage address that crashed. Decompiled `ModifyEndianU32` itself too, confirming the dispatch mechanism (`*(int*)PTR_DAT_7102596740 ^ (param&1)` selecting a real-swap-vs-noop function pointer) matches the earlier notes exactly.

Conclusion: this real, shipped file's on-disk bytes are **also** already native-order (like our own freshly-compiled file) - contradicting the earlier notes' "every real archive load byte-swaps as a matter of course" (drawn from a different, evidently differently-authored reference archive, `__ArchiveShader.bnsh`). Applied the same fix our own file needed: pre-set bit 0 of the `grsc+0xc` flag byte to skip the fixup pass. This is a *general* lesson, not a one-off: don't trust "every real archive needs X" claims drawn from a single reference file - different archives (different games, different build pipelines, different eras) can disagree, and the only reliable check is decompiling the actual consuming code against the actual file's actual bytes.

With that fixed, the loader got dramatically further - past `setUp()` entirely, into real (non-empty!) per-program variation-record processing inside `agl::ShaderProgram::initialize` (`0x7100b36f98`), further than any attempt all session including the earlier, doomed CLI-file archive test (which never had real variation data to walk at all). Crashed there too, though: a clean NULL-pointer dereference, one call deep in a loop calling `agl::ShaderCompileInfo::getRegitserUniformBlockName()` (real Nintendo typo, kept verbatim in the mangled symbol) and comparing a returned `SafeString` against a per-variation uniform-block name. Root cause not fully chased down - working hypothesis (not yet confirmed via decompile) is that `TextureShader.bnsh`, being from **Tears of the Kingdom** rather than BotW, was built against a newer NintendoSDK/GLSLC generation, and the per-program variation-record layout may have evolved in a way BotW's own (older) `ShaderProgram::initialize` doesn't fully expect - i.e. a genuine cross-game format-version mismatch, not a simple one-line fix like the previous two bugs. User couldn't find a real `.bnsh` file inside BotW's own dump to test the "same game, guaranteed-compatible" hypothesis directly.

## Pivot: don't reconstruct a shader, borrow BotW's own real font renderer wholesale

User's framing, and the right one: "does the game have a default fallback shader we can just reuse... jumping between methods [is] dumb." Rather than chase the TotK/BotW record-layout mismatch further, or hand-build 50 blob fields, or hook a live in-game draw call to snoop an arbitrary (possibly complex, texture/sampler-heavy) program - reused BotW's own **font renderer**, which this session had already partially reverse-engineered early on (`sead::DebugFontMgrNvn`) and had a real, shipped, matching-SDK-generation asset for close at hand: `nvn_font_shader.bin` (found by the user in a BotW romfs dump, alongside `nvn_font.ntx`, `System/font/nvn_font/`).

Decompiled `sead::DebugFontMgrNvn::initializeFromBinary` (`0x7100aff42c`) in full, now with real symbol names available. This turned out to be enormously valuable beyond just this one asset - it's a complete, authoritative, ground-truth description of the **raw NVN shader file format** this entire session had been trying to reverse-engineer by inference:

- A 16-byte header of four `int32`s: `[0]`/`[1]` = self-relative offsets (from the buffer's own start) to control section 0/1 (vertex/fragment), `[2]`/`[3]` = data offsets (added to the uploaded buffer's GPU address).
- The whole buffer is uploaded once to a GPU-visible pool; `control` pointers passed to `nvnProgramSetShaders` are literal CPU pointers into that same buffer (`bufferBase + selfRelativeOffset`); `data` pointers are `bufferGpuAddress + dataOffset`.
- `nvnProgramSetShaders(program, 2, &{data0,control0,data1,control1})` called directly - exactly the shape this session used throughout, just now with a fully-authoritative source for what `control` actually points at.
- A second, unrelated `(ptr,size)` pair (originally guessed to be a second/jis1 shader) is actually a **font texture atlas** (`nvnTextureBuilderSetSize2D(0x80,0x80)`, format 1) - fed to a completely separate texture-upload path, not another shader.
- The "flags" parameter in the public signature is misleadingly named - it's stored and later used directly as a uniform-buffer size (rounded to a page), not a bitmask.

Parsed `nvn_font_shader.bin`'s real header and control-section bytes directly in Python before writing any C++ (same cheap-verification-first approach as the `TextureShader.bnsh` check): header offsets (`0x10`, `0xd60`, `0x1700`, `0x1b00`) are all in-bounds, and - the big confirmation - **both** control sections' `+0xc`/`+0x10` fields read `288`/`11`, exactly matching this session's own live-read device-ISA values (`a=288, b=11`, logged from `EnsureQuadInitialized`'s `deviceIsaA`/`deviceIsaB`). This independently confirms two things at once: the ISA-match gate fix from earlier in the session was correct, and this real file is genuinely compatible with the exact device/driver in use. Also notable: both control sections use the same `+8=8` stage code (not distinct per-stage values like the hand-built blob's `5`/`0xE`) - `+8` apparently just needs to be *some* valid code in range, not a real per-stage tag; stage identity comes from elsewhere (the `+0x714` bucket index, matching this session's earlier finding).

Since `DebugFontMgrNvn` never gets constructed by the game itself in this build (confirmed, not a stale hook-address artifact - the earlier "never constructs" diagnostic's addresses had since been fixed and re-tested with the same null result), `EnsureFontManagerInitialized` (`nvn_overlay.hpp`) calls BotW's own real `createInstance`/`initializeFromBinary` **directly**, manually, using the embedded real asset bytes - reusing all of the already-implemented (and never-yet-actually-exercised, since `fontMgr` had always read null before) `DrawTextOverlay` begin/print/end drawing code unchanged. `main.cpp`'s `PresentHook` now calls `DrawTextOverlay` again.

Deployed, not yet tested as of this writing.

## Font-manager path: `createInstance`'s allocator reliably fails, manual reconstruction produced a real hang

Tested. No crash this time in `EnsureFontManagerInitialized` itself, but `sead::DebugFontMgrNvn::createInstance`'s own `operator new[](0x530, heap, 8)` call reliably returned null - confirmed via register dump, not guessed: the crash inside the subsequent `sead::IDisposer` constructor showed `X0=8`, exactly `(long*)nullptr + 1` (the "this" pointer `createInstance` passes after a failed allocation, `plVar3+1` where `plVar3=0`), `X1`/`X2` matching the expected `(heap, HeapNullOption=3)` args exactly. Tried two different real heaps (`findContainHeap` probed with `graphicsNvn`, then `gameFramework`) - both rejected this same ~1.3KB request outright, even though a similarly-sized allocation had succeeded via the same technique during the earlier archive-loader test. Root cause not fully chased down (would mean decompiling the heap's own `vtable+0x30` alloc() implementation).

Consistent with this whole session's dominant successful pattern (every NVN/game object built so far - `g_CommandBuffer`, `g_QuadProgram`, etc. - is static storage, never heap-allocated), sidestepped the allocator entirely: replicated `createInstance`'s real decompiled field writes directly against our own static `uint8_t[0x530]` buffer, skipping the real `IDisposer` constructor (its only lasting effects are a vtable write `createInstance` immediately overwrites anyway, and disposer-list registration that's meaningless for an object that's never freed). Also discovered a separate, real, pre-existing bug in this process: `GetDebugFontMgrNvnInstance()`'s existing implementation reads the singleton slot as *double*-indirect, but `createInstance`'s own real code (`*(long**)PTR_DAT_7102591a98`) reads/writes it *single*-indirect - not the cause of anything this session (the double-indirect read happened to safely short-circuit to null either way), but worth fixing for correctness later.

**Result: a real hang**, not a crash - `"Pure virtual function called!"` printed to the log, then the game never finished loading. This means one of the two hand-written vtable pointers (read live via `WiiXLaunch::ResolveTarget(0x2597e18)`/`0x2597e28`, `+0x10`, matching the `PTR_DAT_xxxx`-used-as-bare-address idiom already established via the `PTR__ZTV...+0x10` pattern elsewhere) is wrong - possibly swapped, possibly the wrong "+0x10" skip for one of the two. Unlike every other bug this session, this doesn't cleanly recover: worse failure mode than a crash, for a technique (hand-reconstructing a real engine object's internal vtable/field layout by reading its constructor's decompile) that's inherently more fragile than anything else tried. Abandoned rather than continue guessing at the exact vtable semantics - user's call, and the right one.

## Pivot: stop constructing shaders, snoop a real one out of a live draw call

Reused the *other* half of the earlier "does the game have a fallback we can reuse" idea (see the pivot note above) - since we can't safely construct `DebugFontMgrNvn` by hand, don't construct anything at all. `nvnCommandBufferBindProgram` is reached through the same kind of resolved, double-indirect function-pointer table cell every other `nvn*` call in this file already reads from (`NvnFn`/`ReadIndirect`). Overwriting the pointer *stored at* that cell with our own function's address is a standard hook technique with a much better risk profile than hand-building a vtable - it's a single pointer swap in a location that's already meant to be called indirectly, not a reconstruction of unknown internal object state.

`NvnOverlay::InstallBindProgramHookIfNeeded`/`HookedBindProgram` capture whatever real `NVNprogram*` the game itself binds most recently each frame into `g_CapturedNvnProgram`, then call through to the real original function so the game's own rendering is completely unaffected. `DrawWithCapturedProgram` binds that captured pointer (instead of any program we built ourselves) to our own second command buffer and issues a minimal quad draw, reusing the same command-buffer/memory-callback machinery already proven working earlier this session. Won't look visually correct - the captured program's real vertex/uniform/texture expectations are unknown and almost certainly don't match a bare position-only quad - but a clean bind+draw with zero crashes would be final, conclusive proof that the entire downstream pipeline built up this session is sound, and shader *construction* (not the pipeline around it) was the only real obstacle all along.

## Chasing the control-memory ceiling: from "bigger reserve" to the real root cause

First real test with a captured program: control-memory reserve exhausted 5 real seconds in (`minSize=256, remaining=0`), even though the captured program was genuinely valid (real, driver-accepted, currently in use by the game). This looked at first like "real shaders need more control memory than our trivial test shaders" and kicked off several rounds of scaling:

- 256KB → 2MB reserve: same clean exhaustion.
- 2MB reserve with a real captured program: a **stack overflow**, not a clean abort (no `"SDK has been aborted"` line, fault address sitting almost exactly at the guest thread's own stack pointer). Root cause: each unit of control-memory growth is a **recursive re-entry** into our own memory callback (the driver calls back into us from inside registering the previous grant), so a bigger reserve handed out in small (16KB) bites just means more recursion levels before ever reaching "out of memory" - at ~130 levels deep the native call stack blew before the reserve did. This was a genuinely different, worse failure mode than every prior "clean abort," and the fix was to grant far bigger chunks per callback call (16KB → 512KB → 8MB) so recursion depth stays pinned at a small, safe constant (~16 levels) regardless of total reserve size - total capacity and recursion depth are independent once chunk size scales with reserve size.
- With that fixed (512KB grants / 8MB reserve, then 8MB grants / 128MB reserve): clean, non-crashing exhaustion both times, at exactly the expected grant count (16 either way) - confirming the mechanism itself was finally correct. But **128MB of control memory still wasn't enough** - exhausted just as completely as 256KB did. No real single draw call needs 128MB of control-memory bookkeeping; a request that consumes *whatever* you give it, at any size, isn't a sizing problem at all.

Also tried changing *which* program gets captured (freezing on the first-ever-seen program, since a real complex world/character shader was the working theory for the "unbounded" growth) versus continuously updating (naturally settling on whatever's bound *last* each frame - typically 2D UI/HUD, rendered after 3D world geometry, in every game including this one). Telling result: the captured pointer's low bits (`...be9128`) were **byte-identical across separate process launches** with different ASLR bases - proof it's the same persistent, statically-placed object every time, meaning the capture-target change hadn't actually changed anything, and the exhaustion was identical anyway. Ruled out "wrong/too-complex shader chosen" as the cause.

**Real root cause**: we record into and submit the SAME `g_CommandBuffer` every single frame, forever, with zero synchronization - nothing ever tells the driver the *previous* frame's submission has actually finished on the GPU before we overwrite and resubmit the same memory. Checked `nvn.h` directly: there is no "reset command buffer" API. The real model is that a command buffer's already-added memory only becomes safe to reuse once its prior submission's fence has signaled (`nvnQueueFenceSync` + `nvnSyncWait`), or by round-robining between multiple command buffers so one is never re-recorded while its predecessor is still in flight. Without synchronization, the driver can never be sure anything is safe to reclaim, so its internal bookkeeping just grows forever - independent of shader complexity, independent of total reserve size, exactly matching every symptom observed across every round of this investigation.

Fix: `WaitForQueueIdle` (`nvn_overlay.hpp`) resolves `nvnQueueFenceSync`/`nvnSyncWait` by name (via the same `ResolveNvnFunctionByName` technique used earlier for the memory callback - no hand-curated `Offset::` slot needed) and blocks until the GPU has fully finished the current submission before `DrawWithCapturedProgram` returns. This is a debug overlay, not a performance-sensitive renderer, so a full per-frame CPU/GPU stall is an acceptable, simple correctness fix rather than the more involved multi-buffer round-robin a real engine would use.

## ⚠️ Build pipeline bug that invalidated a whole round of testing (found and fixed)

After deploying the `WaitForQueueIdle` fix above, several rounds of "edit source → rebuild → redeploy → ask for a test" produced **byte-identical crash logs** across genuinely different source edits (same crash timestamp down to the millisecond, same captured pointer, same backtrace) - including one round where a brand-new diagnostic `WIIXL_LOG` call, unconditionally the first statement of the code path that crashed, never once printed. That's impossible if the edited code were actually running, and it wasn't a coincidence - it was two independent, stacked build/deploy bugs, both specific to this dev machine/environment, not to NVN or the game:

1. **`cmd.exe` invoked through the Bash tool doesn't run synchronously in this environment.** Even a bare `cmd.exe /c "cd"` printed the interactive MS-DOS banner instead of the actual output, meaning `build_switch.bat` runs (real files eventually get written) but detached from the tool call - so exit code 0 and "fresh-looking" timestamps didn't mean the build the tool call *asked for* had actually finished, or finished recompiling anything at all. Confirmed by grepping the staged, uncompressed `.elf` intermediate (`%TEMP%\wiixlaunch-switch\wiixlaunch-switch.elf` - the deployed `subsdk9` is an NSO and its sections are compressed, so `grep` on it directly finds nothing even for strings that ARE really there and printing correctly at runtime; always grep the staged `.elf`, not the NSO, to verify). **Fix: use the PowerShell tool instead** - `& ".\build_switch.bat" *> logfile` from PowerShell (not `cmd.exe /c "build_switch.bat"`, which separately fails with "not recognized" even when the file demonstrably exists) runs synchronously and captures real compiler output.
2. **`scripts\deploy.py` never touched Ryujinx's actual mods folder.** It only writes `deploy\switch\atmosphere\contents\...`. Something copied those files into `%APPDATA%\Ryujinx\mods\contents\01007EF00011E000\NVNInjectionTest\exefs\` by hand at some point earlier in the session, and nothing kept that copy in sync afterward - so every test after that point re-ran that one stale binary regardless of what got "rebuilt." **Fix: `build_switch.bat` now copies straight into the Ryujinx mods folder itself** (via `%APPDATA%\Ryujinx\mods\contents\...`) as part of its deploy step, so this can't silently drift again.

Lesson for future sessions on this machine: don't trust `cmd.exe`-via-Bash exit codes or output file timestamps alone as proof a build actually ran with current source - verify content-level (grep a just-added string in the staged `.elf`) when a test result looks suspiciously identical to a previous one.

## 🎉 Breakthrough & Full Pipeline Resolution: Continuous 60/120 FPS Submissions

The root cause for the control memory exhaustion, aborts, and hangs was definitively identified via reverse engineering and disassembly of `sead::GameFrameworkNx::initializeGraphicsSystem` and `sead::GameFrameworkNx::procDraw_` using Ghidra:

### 1. `nvnCommandBufferAddControlMemory` takes 3 arguments (CPU host memory)
- **Previous misunderstanding**: An earlier assumption believed `AddControlMemory` took `(cmdBuf, pool*, offset, size)` like `AddCommandMemory`.
- **Actual ARM64 disassembly**:
  ```asm
  7100af9118: ldp x1,x0,[x19, #0x150]   ; x1 = memory_ptr (CPU allocation), x0 = cmdBuf
  7100af911c: ldr w2,[x19, #0xc4]        ; w2 = size
  7100af9120: ldr x8,[x8, #0x288]        ; nvnCommandBufferAddControlMemory
  7100af9128: blr x8
  ```
  `AddControlMemory` has the signature:
  `void nvnCommandBufferAddControlMemory(NVNcommandBuffer* cmdBuf, void* memory, size_t size);`
  It expects a direct **aligned CPU host buffer**, not a GPU `NVNmemoryPool*` struct. Passing `&g_MemoryPool` as the pointer and `offset` as the size caused the driver to write internal control descriptors into struct memory, corrupting state and immediately exhausting control space.

### 2. GOT table offset shift (-8 bytes) corrected
- The function pointer table in `main` was previously indexed with a `-8` byte discrepancy in early notes.
  - Slot `0x2597280`: `nvnCommandBufferAddCommandMemory`
  - Slot `0x2597288`: `nvnCommandBufferAddControlMemory`
  - Slot `0x25972c0`: `nvnCommandBufferBeginRecording`
  - Slot `0x25972c8`: `nvnCommandBufferEndRecording`
  - Slot `0x2597270`: `nvnCommandBufferSetMemoryCallback`
  - Slot `0x2597278`: `nvnCommandBufferSetMemoryCallbackData`
- Correcting all slots to the exact GOT entries resolved misdirected function calls.

### 3. Texture, Sampler, and Shader Scratch Memory bindings
- Replicating `procDraw_`, real NVN shaders require active texture and sampler pools plus shader scratch memory:
  - `nvnCommandBufferSetTexturePool(cmdBuf, graphicsNvn + 0x58);`
  - `nvnCommandBufferSetSamplerPool(cmdBuf, graphicsNvn + 0x78);`
  - `nvnCommandBufferSetShaderScratchMemory(cmdBuf, gameFramework + 0x170, 0, *(uint32_t*)(gameFramework + 0x178));`

### 4. Queue Synchronization on the Presentation Thread
- Calling synchronous queue fences or `nvnQueueFinish` on the presentation thread deadlocks in Ryujinx because the presentation thread drives the presentation cycle. Returning immediately after `nvnQueueSubmitCommands` allows the game engine to present normally.

### Result:
- **Zero crashes, zero memory exhaustion, zero stalls.**
- The custom command buffer successfully records, binds vertex state, issues `drawArrays`, and submits commands every frame continuously at full speed.
