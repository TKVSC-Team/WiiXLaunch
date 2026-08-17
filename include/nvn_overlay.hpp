#pragma once

#include <wiixlaunch.hpp>

// First real implementation attempt for the "always-on text overlay" plan in
// docs/switch-nvn-findings.md. UNTESTED - written entirely from static
// analysis, not yet run on real hardware or an emulator. Expect bugs,
// especially around the sead::GameFrameworkNx struct offsets (+0x198 queue,
// +0x1d8 recorded handle), which were read from decompiler pseudocode rather
// than xref-anchored the way most of the NVN function offsets below were -
// see the findings doc for the full confidence breakdown per address.
//
// Switch-only for now; nothing here has a Wii U/GX2 equivalent yet.

#if WIIXL_SWITCH

#include <cstdint>
#include <cstddef>
#include <rainbow_bnsh.hpp>
#include <rainbow_sead_bin.hpp>
#include <rainbowselftest_sead_bin.hpp>
#include <plasma_sead_bin.hpp>
#include <texturequad_sead_bin.hpp>
#include <testpic_texture_bytes.hpp>
#include <wiixl_quad_bnsh_bytes.hpp>
#include <texture_shader_bnsh_bytes.hpp>
#include <nvn_font_shader_bytes.hpp>
#include <nvn_font_texture_bytes.hpp>
#include <lib/util/sys/mem_layout.hpp>

extern "C" void armDCacheFlush(void* addr, size_t size);

namespace NvnOverlay {

// -----------------------------------------------------------------------
// Reading BotW's own already-resolved nvn* function pointers directly,
// instead of bootstrapping our own nvnDeviceGetProcAddress chain - see
// "The core mechanism" in the findings doc. Every nvn* function is resolved
// once at boot into a table, reachable as a DOUBLE dereference from a fixed
// address: *(void**)(*(uintptr_t*)tableSlotAddr). Same shape used for the
// GraphicsNvn/DebugFontMgrNvn singletons below.
// -----------------------------------------------------------------------
template <typename Fn>
inline Fn ReadIndirect(uintptr_t tableSlotAddr) {
    uintptr_t cell = *reinterpret_cast<uintptr_t*>(tableSlotAddr);
    return reinterpret_cast<Fn>(*reinterpret_cast<void**>(cell));
}

// Ghidra addresses (0x7100000000-based) from docs/switch-nvn-findings.md,
// minus that base, since these aren't hook targets (WIIXL_HOOK_DEFINE_*
// already handles that subtraction for actual hooks) - these are read at
// runtime via WiiXLaunch::ResolveTarget-style base-relative addressing, so
// keep them as raw absolute-in-Ghidra addresses and resolve through
// WiiXLaunch::ResolveTarget ourselves, matching how GetTargetFunction does it
// internally for offset pairs.
namespace Offset {
    // xref-anchored (see findings doc)
    constexpr uintptr_t kMemoryPoolBuilderSetDevice   = 0x2596a88;
    constexpr uintptr_t kMemoryPoolBuilderSetDefaults = 0x2596a90;
    constexpr uintptr_t kMemoryPoolBuilderSetStorage  = 0x2596a98;
    constexpr uintptr_t kMemoryPoolBuilderSetFlags    = 0x2596aa0;
    constexpr uintptr_t kMemoryPoolInitialize         = 0x2596ac0;
    constexpr uintptr_t kCommandBufferInitialize      = 0x2597258;
    constexpr uintptr_t kCommandBufferRealInit        = 0x2597258;
    constexpr uintptr_t kCommandBufferSetMemoryCallback = 0x2597270;
    constexpr uintptr_t kCommandBufferSetMemoryCallbackData = 0x2597278;
    constexpr uintptr_t kCommandBufferAddCommandMemory = 0x2597280;
    constexpr uintptr_t kCommandBufferAddControlMemory = 0x2597288;
    constexpr uintptr_t kCommandBufferBeginRecording  = 0x25972c0;
    constexpr uintptr_t kCommandBufferEndRecording    = 0x25972c8;
    constexpr uintptr_t kCommandBufferBindUniformBuffer = 0x2597338;
    constexpr uintptr_t kCommandBufferDrawArrays      = 0x25973d0;
    constexpr uintptr_t kQueueSubmitCommands          = 0x25969b8;

    // GraphicsNvn singleton storage cell (double-indirected, see findings doc)
    constexpr uintptr_t kGraphicsNvnInstanceSlot = 0x2594c10;
    // DebugFontMgrNvn singleton storage cell (double-indirected)
    constexpr uintptr_t kDebugFontMgrNvnInstanceSlot = 0x2591a98;
    // sead::DebugFontMgrNvn::createInstance(Heap*) / ::initializeFromBinary(
    // Heap*, void* shaderBytes, size_t shaderSize, void* textureBytes,
    // size_t textureSize, uint32_t uniformBufSize) - real, decompiled
    // addresses (search_functions_by_name against the real "main" project,
    // not inferred). The game itself apparently never calls these in this
    // build (see findings doc "DebugFontMgrNvn never constructs") - called
    // directly ourselves instead, with a REAL BotW asset
    // (nvn_font_shader.bin/nvn_font.ntx), bypassing whatever gates the
    // game's own construction.
    constexpr uintptr_t kDebugFontMgrCreateInstance = 0xaff290;
    constexpr uintptr_t kDebugFontMgrInitializeFromBinary = 0xaff42c;

    // Raw-quad pipeline offsets, added for DrawQuad below. All independently
    // xref-anchored via the strict method (find the function-name string,
    // find its unique xref inside nvnLoadCProcs, trace the *preceding*
    // "adrp x8; ldr x8,[x8,#SLOT]" pair that the following "str x0,[x8]"
    // actually uses - not just the nearest one, since nvnLoadCProcs
    // interleaves "resolve name N, then prep name N+1's string, then store
    // name N's result" so the naive nearest-neighbor reading is off by one
    // step). See docs/switch-nvn-findings.md for the full trace.
    constexpr uintptr_t kProgramInitialize            = 0x2596a68;
    // Found directly in agl::ShaderProgram::forceValidate_'s real decompile
    // (the actual game code that calls nvnProgramSetShaders for BNSH-sourced
    // programs) as PTR_pfnc_nvnProgramFinalize_7102596a70 - not independently
    // xref-anchored via the strict method, but sitting exactly between the
    // already-confirmed Initialize (0x...a68) and SetShaders (0x...a80)
    // slots, which is the expected table layout.
    constexpr uintptr_t kProgramFinalize               = 0x2596a70;
    constexpr uintptr_t kProgramSetShaders             = 0x2596a80;
    constexpr uintptr_t kBufferBuilderSetDevice        = 0x2596b88;
    constexpr uintptr_t kBufferBuilderSetDefaults      = 0x2596b90;
    constexpr uintptr_t kBufferBuilderSetStorage       = 0x2596b98;
    constexpr uintptr_t kBufferInitialize              = 0x2596bb8;
    constexpr uintptr_t kBufferGetAddress              = 0x2596bd8;
    constexpr uintptr_t kBufferMap                     = 0x2596bd0;
    constexpr uintptr_t kBlendStateSetDefaults         = 0x2596fd8;
    constexpr uintptr_t kBlendStateSetBlendTarget      = 0x2596fe0;
    constexpr uintptr_t kBlendStateSetBlendFunc        = 0x2596fe8;
    constexpr uintptr_t kBlendStateSetBlendEquation    = 0x2596ff0;
    constexpr uintptr_t kColorStateSetDefaults         = 0x2597050;
    constexpr uintptr_t kColorStateSetBlendEnable      = 0x2597058;
    constexpr uintptr_t kColorStateSetLogicOp          = 0x2597060;
    constexpr uintptr_t kColorStateSetAlphaTest        = 0x2597068;
    constexpr uintptr_t kChannelMaskStateSetDefaults   = 0x2597088;
    constexpr uintptr_t kChannelMaskStateSetChannelMask= 0x2597090;
    constexpr uintptr_t kPolygonStateSetDefaults       = 0x2597158;
    constexpr uintptr_t kPolygonStateSetCullFace       = 0x2597160;
    constexpr uintptr_t kDepthStencilStateSetDefaults  = 0x25971a0;
    constexpr uintptr_t kDepthStencilStateSetDepthTestEnable  = 0x25971a8;
    constexpr uintptr_t kDepthStencilStateSetDepthWriteEnable = 0x25971b0;
    constexpr uintptr_t kVertexAttribStateSetDefaults    = 0x2597208;
    constexpr uintptr_t kVertexAttribStateSetFormat      = 0x2597210;
    constexpr uintptr_t kVertexAttribStateSetStreamIndex = 0x2597218;
    constexpr uintptr_t kVertexStreamStateSetDefaults    = 0x2597230;
    constexpr uintptr_t kVertexStreamStateSetStride      = 0x2597238;
    constexpr uintptr_t kCommandBufferBindBlendState        = 0x25972e0;
    constexpr uintptr_t kCommandBufferBindChannelMaskState  = 0x25972e8;
    constexpr uintptr_t kCommandBufferBindColorState        = 0x25972f0;
    constexpr uintptr_t kCommandBufferBindPolygonState      = 0x2597300;
    constexpr uintptr_t kCommandBufferBindDepthStencilState = 0x2597308;
    constexpr uintptr_t kCommandBufferBindVertexAttribState = 0x2597310;
    constexpr uintptr_t kCommandBufferBindVertexStreamState = 0x2597318;
    constexpr uintptr_t kCommandBufferBindProgram           = 0x2597320;
    constexpr uintptr_t kCommandBufferBindVertexBuffer      = 0x2597328;
    constexpr uintptr_t kCommandBufferSetViewport            = 0x2597448;
    constexpr uintptr_t kCommandBufferSetScissor             = 0x2597460;
    constexpr uintptr_t kCommandBufferSetRenderTargets       = 0x2597598;
    constexpr uintptr_t kCommandBufferSetTexturePool        = 0x25975e8;
    constexpr uintptr_t kCommandBufferSetSamplerPool        = 0x25975f0;
    constexpr uintptr_t kCommandBufferSetShaderScratchMemory = 0x25975f8;

    // Texture builder / texture pipeline slots - xref-anchored, all read
    // directly from sead::DebugFontMgrNvn::initializeFromBinary's own real
    // decompile (0x7100aff42c, PTR_pfnc_nvn*_* variable names Ghidra itself
    // attached) rather than resolved by name at runtime. Switched to this
    // after nvnTextureBuilderSetPackagedTextureData's by-name resolution
    // (ResolveNvnFn) silently returned a non-null but WRONG function -
    // resolved fine, called fine, but never actually set the builder's
    // packaged-data field, so nvnTextureInitialize kept taking the same
    // plain-storage vtable-crashing path as every earlier round. See
    // docs/switch-nvn-findings.md, "Texture pipeline, round 7".
    constexpr uintptr_t kTextureBuilderSetDevice     = 0x2596c10;
    constexpr uintptr_t kTextureBuilderSetDefaults   = 0x2596c18;
    constexpr uintptr_t kTextureBuilderSetTarget     = 0x2596c28;
    constexpr uintptr_t kTextureBuilderSetSize2D     = 0x2596c50;
    constexpr uintptr_t kTextureBuilderSetFormat     = 0x2596c68;
    constexpr uintptr_t kTextureBuilderSetStorage    = 0x2596c98;
    constexpr uintptr_t kTextureBuilderSetPackagedTextureData = 0x2596ca0;
    constexpr uintptr_t kTextureInitialize           = 0x2596db0;
    constexpr uintptr_t kTexturePoolRegisterTexture  = 0x2596b20;
    constexpr uintptr_t kDeviceGetTextureHandle      = 0x2596898;

    // Real-archive pivot: rather than hand-parse wiixl_quad.bnsh's control/
    // code layout by hex-archaeology (two independent, well-reasoned attempts
    // failed completely - see findings doc), construct a real
    // agl::ShaderProgramArchive and let the game's own, proven-correct code
    // parse our .bnsh file, the same way it parses its own. None of these
    // are independently xref-anchored the strict way (decompiler-sourced
    // addresses from FUN_7100b6de44 and friends) - flagged as such.
    constexpr uintptr_t kGetCurrentHeap                    = 0x11f3360; // ksys::util::getCurrentHeap() - unreliable, see kHeapMgrFindContainHeap
    // getCurrentHeap() returned null on the Presentation Thread (no TLS
    // "current heap" scope active at present_ time, and HeapMgr's own
    // fallback root-heap field was also empty there) - findContainHeap
    // instead searches the game's actual globally-registered heap lists for
    // whichever real heap owns a KNOWN address (graphicsNvn's pointer, which
    // we already reliably obtain every frame), sidestepping the empty-TLS
    // problem entirely.
    constexpr uintptr_t kHeapMgrInstancePtr                = 0x2584918; // holds ptr to the real sead::HeapMgr singleton
    constexpr uintptr_t kHeapMgrFindContainHeap             = 0xb09478;
    // sead::HeapMgr::getCurrentHeap() (decompiled: 0xb097a0) reads a TLS slot
    // and, if that slot is unset (true for our injected Presentation Thread
    // hook - it was never set up as a "sead thread"), falls back to reading
    // *(HeapMgr+8) instead. setAllocFromNotSeadThreadHeap (decompiled:
    // 0xb09670) is a one-line function that writes exactly that fallback
    // field - a real, intentional engine escape hatch for allocating from
    // threads sead doesn't own. Explains why our archive loader's internal
    // allocation call was returning a clean null from two different, both
    // independently-valid heaps: we were passing an explicit Heap* to the
    // allocator (which does skip its OWN getCurrentHeap() check when given
    // one), but something deeper inside that heap's virtual alloc() most
    // likely still calls getCurrentHeap() itself for lock/ownership
    // bookkeeping, which would fail from an empty TLS slot regardless of
    // which heap we asked for.
    constexpr uintptr_t kHeapMgrSetAllocFromNotSeadThreadHeap = 0xb09670;
    constexpr uintptr_t kShaderProgramArchiveCtor           = 0xb6d900;
    constexpr uintptr_t kShaderProgramArchiveLoadFromBinary = 0xb6de44; // FUN_7100b6de44
    constexpr uintptr_t kSearchShaderProgramIndex           = 0xb6ec54;
    constexpr uintptr_t kShaderProgramSetUpAllVariation     = 0xb38888;
    constexpr uintptr_t kSafeStringVtable                   = 0x2578db0;
    // Diagnostic only (see findings doc "byte-swap dispatch" investigation):
    // data slot holding a pointer to agl::ModifyEndianU32's "needs swap" flag
    // (int, dereferenced once more). Ghidra's static image shows this as 1
    // (swap-selecting) at link time, but that's link-time-only - real boot
    // code may set it correctly before any real archive load ever touches
    // it. Reading it live, right before our own archive load, settles
    // whether our loader crash is a byte-swap corruption or something else.
    constexpr uintptr_t kEndianSwapFlagPtrSlot               = 0x2596740;
}

// -----------------------------------------------------------------------
// Opaque NVN struct sizes, sourced from BotW's own vendored nvn.h via
// code.botw.link (the zeldamods decompilation project) - see findings doc.
// Not exposed as real typed structs since we don't have the real header,
// just correctly-sized opaque byte blobs, matching how the real SDK also
// treats these as opaque outside of nvn.cpp itself.
// -----------------------------------------------------------------------
struct NVNcommandBuffer   { alignas(8) uint8_t reserved[0xA0]; };
struct NVNmemoryPool      { alignas(8) uint8_t reserved[0x100]; };
struct NVNmemoryPoolBuilder { alignas(8) uint8_t reserved[0x40]; };
struct NVNbuffer          { alignas(8) uint8_t reserved[0x30]; };
struct NVNbufferBuilder   { alignas(8) uint8_t reserved[0x40]; };
// Real sizes (4 / 8 bytes, nvn.h:274-281) - previously double their real size
// (0x8/0x10), which silently broke every multi-element bind. Ghidra-confirmed
// against real game code: sead::PrimitiveDrawMgrNvn::prepareFromBinaryImpl
// (0x7100b01310) lays out three NVNvertexAttribStates exactly 4 bytes apart
// (param_1+0x218/+0x21c/+0x220) and its one NVNvertexStreamState spans
// exactly 8 bytes (+0x228 to +0x230, where the next distinct object starts).
// This is almost certainly THE bug behind the rainbow quad rendering solid
// black: nvnCommandBufferBindVertexAttribState(cmdBuf, 2, g_RainbowVertexAttribStates)
// walks the array using the REAL 4-byte stride, so with our old 8-byte
// struct, "element 1" (location 1 / a_Color) was read from byte offset 4 -
// squarely inside element 0's padding - never touching the real color-attrib
// data written to g_RainbowVertexAttribStates[1]. The driver treats that
// slot as disabled, so a_Color always fetches zero, v_Color is always
// (0,0,0,0), and the fragment shader dutifully outputs solid black.
struct NVNvertexAttribState  { alignas(4) uint8_t reserved[0x4]; };
struct NVNvertexStreamState  { alignas(8) uint8_t reserved[0x8]; };
// Real size (128 bytes), confirmed against the actual nvn.h from a local NVN
// SDK copy (NvnHeader/nvn/nvn.h:284-286) - see findings doc "BREAKTHROUGH".
struct NVNprogram         { alignas(8) uint8_t reserved[0x80]; };
// Real size (64 bytes, nvn.h:364-366).
struct NVNsync            { alignas(8) uint8_t reserved[0x40]; };

// Struct sizes padded safely (NVNtexture is at least 0xb0/176 bytes as confirmed by
// nvnTextureInitialize's memset(texture, 0, 0xb0)).
struct NVNtexturePool     { alignas(8) uint8_t reserved[0x80]; };
struct NVNsamplerPool     { alignas(8) uint8_t reserved[0x80]; };
struct NVNtextureBuilder  { alignas(8) uint8_t reserved[0x100]; };
struct NVNtexture         { alignas(8) uint8_t reserved[0x100]; };
struct NVNsamplerBuilder  { alignas(8) uint8_t reserved[0x80]; };
struct NVNsampler         { alignas(8) uint8_t reserved[0x80]; };
using NVNtextureHandle = uint64_t;

using NVNcommandHandle = uint64_t;

// -----------------------------------------------------------------------
// Real agl:: types for the real-archive pivot. Sizes are decompiler-derived
// upper bounds (highest offset touched in FUN_7100b6de44/forceValidate_,
// generously padded), not from a real header - agl:: isn't in the public
// NVN SDK the way nvn.h is.
// -----------------------------------------------------------------------
struct SeadSafeString { const void* vtable; const char* cstr; };
// Generously padded WAY beyond the highest offset our own reading of
// FUN_7100b6de44 touches (~0xb8) - a previous 0x400 guess is suspected of
// being the real cause of a null-pointer crash inside ShaderProgram::
// initialize (see findings doc): if the real constructor's true object size
// exceeds our guess, its writes spill into whatever's declared next in
// memory, silently corrupting it. This is cheap insurance, not a measured
// size - we don't have the real header for agl:: types.
struct AglShaderProgramArchive { alignas(8) uint8_t reserved[0x4000]; };
// ShaderProgramArchive+0x40 is a POINTER FIELD (holds the address of a
// separately heap-allocated agl::ShaderProgram array - see
// FUN_7100b6de44's `*(ulong**)(param_1+0x40) = puVar11+1`), not the array
// inlined at that offset - needs one more dereference wherever it's used.
constexpr ptrdiff_t kShaderProgramArrayPtrOffset = 0x40;
constexpr size_t kShaderProgramStride = 0x428;         // per-entry stride, confirmed via searchShaderProgramIndex's decompile
constexpr ptrdiff_t kShaderProgramNvnProgramOffset = 0x358; // ShaderProgram+0x358: embedded NVNprogram (see forceValidate_)
constexpr uint32_t kShaderProgramIndexNotFound = 0xffffffff;

using FnGetCurrentHeap                    = void* (*)();
using FnHeapMgrFindContainHeap            = void* (*)(void* heapMgrInstance, const void* addr);
using FnHeapMgrSetAllocFromNotSeadThreadHeap = void (*)(void* heapMgrInstance, void* heap);
using FnShaderProgramArchiveCtor          = void (*)(AglShaderProgramArchive*);
using FnShaderProgramArchiveLoadFromBinary = void (*)(AglShaderProgramArchive*, void* rawBytes, void* heap);
using FnSearchShaderProgramIndex          = uint32_t (*)(const AglShaderProgramArchive*, const SeadSafeString*);
using FnShaderProgramSetUpAllVariation    = void (*)(void* shaderProgram, bool);

using FnMemoryPoolBuilderSetDevice   = void (*)(NVNmemoryPoolBuilder*, void* device);
using FnMemoryPoolBuilderSetDefaults = void (*)(NVNmemoryPoolBuilder*);
using FnMemoryPoolBuilderSetStorage  = void (*)(NVNmemoryPoolBuilder*, void* memory, ptrdiff_t size);
using FnMemoryPoolBuilderSetFlags    = void (*)(NVNmemoryPoolBuilder*, int flags);
using FnMemoryPoolInitialize         = int  (*)(NVNmemoryPool*, const NVNmemoryPoolBuilder*);
using FnCommandBufferRealInit        = void (*)(NVNcommandBuffer*, void* device); // see kCommandBufferRealInit - void, not NVNboolean; real code never checks a result
using FnCommandBufferAddCommandMemory = void (*)(NVNcommandBuffer*, const NVNmemoryPool*, ptrdiff_t offset, size_t size);
// AddControlMemory takes 3 arguments: (cmdBuf, void* memory, size_t size)
// Confirmed directly from procDraw_ and initializeGraphicsSystem disassembly:
// x0=cmdBuf, x1=memory_pointer, x2=size.
using FnCommandBufferAddControlMemory = void (*)(NVNcommandBuffer*, void* memory, size_t size);
using FnCommandBufferBeginRecording  = void (*)(NVNcommandBuffer*);
using FnCommandBufferEndRecording    = NVNcommandHandle (*)(NVNcommandBuffer*);
using FnQueueSubmitCommands          = void (*)(void* queue, int count, const NVNcommandHandle*);
// Signature confirmed from DebugFontMgrNvn::print()'s real decompile
// (findings doc): (cmdBuf, stage, index, gpuAddr, size).
using FnCommandBufferBindUniformBuffer = void (*)(NVNcommandBuffer*, int stage, int index, uint64_t gpuAddr, size_t size);
using FnCommandBufferSetTexturePool    = void (*)(NVNcommandBuffer*, const void* texturePool);
using FnCommandBufferSetSamplerPool    = void (*)(NVNcommandBuffer*, const void* samplerPool);
using FnCommandBufferSetShaderScratchMemory = void (*)(NVNcommandBuffer*, const NVNmemoryPool* pool, ptrdiff_t offset, size_t size);

// Texture/sampler pipeline (nvn.h signatures - resolved by name at runtime
// via ResolveNvnFn, not hand-curated Offset:: slots, since none of these
// were ever xref-hunted in the GOT and by-name resolution is the strictly
// better technique this session already established for exactly this case).
using FnTextureBuilderSetDevice   = void (*)(NVNtextureBuilder*, void* device);
using FnTextureBuilderSetDefaults = void (*)(NVNtextureBuilder*);
using FnTextureBuilderSetTarget   = void (*)(NVNtextureBuilder*, int32_t target);
using FnTextureBuilderSetSize2D   = void (*)(NVNtextureBuilder*, int width, int height);
using FnTextureBuilderSetFormat   = void (*)(NVNtextureBuilder*, int32_t format);
using FnTextureBuilderSetFlags    = void (*)(NVNtextureBuilder*, uint32_t flags);
using FnTextureBuilderSetStride   = void (*)(NVNtextureBuilder*, int64_t stride);
using FnTextureBuilderGetStorageSize      = size_t (*)(const NVNtextureBuilder*);
using FnTextureBuilderGetStorageAlignment = size_t (*)(const NVNtextureBuilder*);
using FnTextureBuilderSetStorage  = void (*)(NVNtextureBuilder*, NVNmemoryPool*, ptrdiff_t offset);
// Real signature confirmed from sead::DebugFontMgrNvn::initializeFromBinary's
// decompile (0x7100aff42c): takes a raw CPU pointer into a real "packaged
// texture data" asset (nn::util::BinaryFileHeader/BinaryBlockHeader
// container - magic "DFvN"/"HBvN") - see docs/switch-nvn-findings.md,
// "Texture pipeline, round 6", and scripts/pack_texture.py.
using FnTextureBuilderSetPackagedTextureData = void (*)(NVNtextureBuilder*, const void*);
using FnTextureInitialize         = uint8_t (*)(NVNtexture*, const NVNtextureBuilder*);

using FnTexturePoolInitialize     = uint8_t (*)(NVNtexturePool*, const NVNmemoryPool*, ptrdiff_t offset, int numDescriptors);
using FnTexturePoolRegisterTexture = void (*)(const NVNtexturePool*, int id, const NVNtexture*, const void* view);

using FnSamplerBuilderSetDevice   = void (*)(NVNsamplerBuilder*, void* device);
using FnSamplerBuilderSetDefaults = void (*)(NVNsamplerBuilder*);
using FnSamplerBuilderSetMinMagFilter = void (*)(NVNsamplerBuilder*, int32_t min, int32_t mag);
using FnSamplerBuilderSetWrapMode = void (*)(NVNsamplerBuilder*, int32_t s, int32_t t, int32_t r);
using FnSamplerInitialize         = uint8_t (*)(NVNsampler*, const NVNsamplerBuilder*);

using FnSamplerPoolInitialize     = uint8_t (*)(NVNsamplerPool*, const NVNmemoryPool*, ptrdiff_t offset, int numDescriptors);
using FnSamplerPoolRegisterSampler = void (*)(const NVNsamplerPool*, int id, const NVNsampler*);

using FnDeviceGetTextureHandle    = NVNtextureHandle (*)(const void* device, int textureID, int samplerID);
using FnCommandBufferBindTexture  = void (*)(NVNcommandBuffer*, int32_t stage, int index, NVNtextureHandle handle);

// Real, plain POD layout (nvn.h:4464-4484) - not opaque, so declared with
// real fields rather than a reserved-byte blob like the driver-opaque types
// above.
struct NVNcopyRegion {
    int32_t xoffset, yoffset, zoffset;
    int32_t width, height, depth;
};
using FnCommandBufferCopyBufferToTexture = void (*)(NVNcommandBuffer*, uint64_t src, const NVNtexture* dstTexture, const void* dstView, const NVNcopyRegion* dstRegion, int flags);

namespace NvnTexture {
    constexpr int32_t kTarget2D          = 0x1;  // NVN_TEXTURE_TARGET_2D
    constexpr int32_t kFormatRGBA8       = 0x25; // NVN_FORMAT_RGBA8
    constexpr int32_t kFormatR8          = 0x1;  // NVN_FORMAT_R8 - diagnostic only, see docs/switch-nvn-findings.md
    constexpr uint32_t kFlagsLinearBit   = 0x10; // NVN_TEXTURE_FLAGS_LINEAR_BIT
    constexpr int32_t kMinFilterNearest  = 0x0;  // NVN_MIN_FILTER_NEAREST (Point / no filter)
    constexpr int32_t kMinFilterLinear   = 0x1;  // NVN_MIN_FILTER_LINEAR (Bilinear)
    constexpr int32_t kMagFilterNearest  = 0x0;  // NVN_MAG_FILTER_NEAREST (Point / no filter)
    constexpr int32_t kMagFilterLinear   = 0x1;  // NVN_MAG_FILTER_LINEAR (Bilinear)
    constexpr int32_t kWrapModeClampEdge = 0x7;  // NVN_WRAP_MODE_CLAMP_TO_EDGE
    constexpr int32_t kShaderStageFragment = 0x1; // NVN_SHADER_STAGE_FRAGMENT
    // Fixed for this NVN SDK/hardware generation per nvn.h's own doc comments
    // (stated as concrete values there, not as "query this" placeholders,
    // same confidence level as every other hand-authored constant in this
    // file taken directly from nvn.h): reserved descriptor count and
    // descriptor byte size for both texture and sampler pools.
    constexpr int kReservedDescriptors   = 256;
    constexpr int kDescriptorSize        = 32;
}

// Raw-quad pipeline: program/buffer/vertex-state setup and draw-time binds.
// NVNshaderData - real NVN struct shape confirmed twice independently this
// session (sead::PrimitiveDrawMgrNvn::prepareFromBinaryImpl and
// sead::DebugFontMgrNvn::initializeFromBinary both build this exact
// {data, control} pair before calling nvnProgramSetShaders) - see findings
// doc's "Building the raw pipeline" section.
struct NVNshaderData { uint64_t data; uint64_t control; };

// Real sizes (nvn.h:241-271) - previously 64 bytes each (uint64_t[8]), 8-16x
// their real size. Harmless today only because every current call site binds
// a single instance (bind*State takes one pointer, no count/array like
// BindVertexAttribState) - but same defect class as the vertex-attrib-state
// bug above, so fixed for correctness before anything ever arrays these.
struct NVNblendState { alignas(8) uint8_t reserved[0x8]; };
struct NVNcolorState { alignas(4) uint8_t reserved[0x4]; };
struct NVNchannelMaskState { alignas(4) uint8_t reserved[0x4]; };
struct NVNpolygonState { alignas(4) uint8_t reserved[0x4]; };
struct NVNdepthStencilState { alignas(8) uint8_t reserved[0x8]; };

using FnBlendStateSetDefaults = void (*)(NVNblendState*);
using FnBlendStateSetBlendTarget = void (*)(NVNblendState*, int target);
using FnBlendStateSetBlendFunc = void (*)(NVNblendState*, int srcFunc, int dstFunc, int srcAlphaFunc, int dstAlphaFunc);
using FnBlendStateSetBlendEquation = void (*)(NVNblendState*, int modeRGB, int modeAlpha);
using FnColorStateSetDefaults = void (*)(NVNcolorState*);
using FnColorStateSetBlendEnable = void (*)(NVNcolorState*, int target, uint8_t enable);
using FnColorStateSetLogicOp = void (*)(NVNcolorState*, int logicOp);
using FnColorStateSetAlphaTest = void (*)(NVNcolorState*, int alphaTest);
using FnChannelMaskStateSetDefaults = void (*)(NVNchannelMaskState*);
using FnChannelMaskStateSetChannelMask = void (*)(NVNchannelMaskState*, int target, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
using FnPolygonStateSetDefaults = void (*)(NVNpolygonState*);
using FnPolygonStateSetCullFace = void (*)(NVNpolygonState*, int cullFace);
using FnDepthStencilStateSetDefaults = void (*)(NVNdepthStencilState*);
using FnDepthStencilStateSetDepthTestEnable = void (*)(NVNdepthStencilState*, uint8_t enable);
using FnDepthStencilStateSetDepthWriteEnable = void (*)(NVNdepthStencilState*, uint8_t enable);

using FnCommandBufferBindBlendState = void (*)(NVNcommandBuffer*, const NVNblendState*);
using FnCommandBufferBindColorState = void (*)(NVNcommandBuffer*, const NVNcolorState*);
using FnCommandBufferBindChannelMaskState = void (*)(NVNcommandBuffer*, const NVNchannelMaskState*);
using FnCommandBufferBindPolygonState = void (*)(NVNcommandBuffer*, const NVNpolygonState*);
using FnCommandBufferBindDepthStencilState = void (*)(NVNcommandBuffer*, const NVNdepthStencilState*);

using FnProgramInitialize          = uint8_t (*)(NVNprogram*, void* device); // real nvn.h: returns NVNboolean, not void - previously discarded
using FnProgramFinalize            = void (*)(NVNprogram*);
using FnProgramSetShaders          = uint8_t (*)(NVNprogram*, int count, const NVNshaderData*);
using FnBufferBuilderSetDevice     = void (*)(NVNbufferBuilder*, void* device);
using FnBufferBuilderSetDefaults   = void (*)(NVNbufferBuilder*);
using FnBufferBuilderSetStorage    = void (*)(NVNbufferBuilder*, const NVNmemoryPool*, ptrdiff_t offset, size_t size);
using FnBufferInitialize           = uint8_t (*)(NVNbuffer*, const NVNbufferBuilder*);
using FnBufferGetAddress           = uint64_t (*)(const NVNbuffer*);
using FnBufferMap                  = void* (*)(const NVNbuffer*);
using FnVertexAttribStateSetDefaults = void (*)(NVNvertexAttribState*);
using FnVertexAttribStateSetFormat   = void (*)(NVNvertexAttribState*, int format, ptrdiff_t offset);
using FnVertexAttribStateSetStreamIndex = void (*)(NVNvertexAttribState*, int streamIndex);
using FnVertexStreamStateSetDefaults = void (*)(NVNvertexStreamState*);
using FnVertexStreamStateSetStride   = void (*)(NVNvertexStreamState*, ptrdiff_t stride);
using FnCommandBufferBindProgram           = void (*)(NVNcommandBuffer*, const NVNprogram*, uint32_t stageMask);
using FnCommandBufferBindVertexAttribState = void (*)(NVNcommandBuffer*, int count, const NVNvertexAttribState*);
using FnCommandBufferBindVertexStreamState = void (*)(NVNcommandBuffer*, int count, const NVNvertexStreamState*);
using FnCommandBufferBindVertexBuffer      = void (*)(NVNcommandBuffer*, int index, uint64_t gpuAddr, size_t size);
using FnCommandBufferSetViewport           = void (*)(NVNcommandBuffer*, int x, int y, int w, int h);
using FnCommandBufferSetScissor            = void (*)(NVNcommandBuffer*, int x, int y, int w, int h);
using FnCommandBufferSetRenderTargets      = void (*)(NVNcommandBuffer*, int numColors, const void* const* colorTargets, const void* colorViews, const void* depthStencilTarget, const void* depthStencilView);
using FnCommandBufferDrawArrays            = void (*)(NVNcommandBuffer*, int mode, int first, int count);

// sead::GameFrameworkNx member offsets - read from decompiler pseudocode
// (procDraw_/present_), NOT individually xref-anchored. Flagged as the
// least-trusted addresses in this file; verify against real behavior before
// trusting further.
namespace GameFramework {
    constexpr uintptr_t kQueueOffset = 0x198;
    // xref-anchored via raw disassembly of procDraw_ (see findings doc) -
    // the live sead::FrameBuffer* the game itself binds its own DrawContext
    // to every frame. Our own fresh DrawContext never got this call, which
    // is almost certainly why nothing rendered on the first attempt: no
    // bound render target means the draw commands have nowhere to write.
    constexpr uintptr_t kFrameBufferOffset = 0x100;
}

// GraphicsNvn instance layout (from GraphicsNvn::initializeImpl, xref-anchored)
constexpr uintptr_t kGraphicsNvnDeviceOffset = 0x30;

// -----------------------------------------------------------------------
// Our own, small, statically-allocated GPU resources - deliberately side-
// stepping sead::Heap entirely (see findings doc's "memory allocation
// strategy" note): a short debug string doesn't need real engine-managed
// allocation, a fixed static buffer is simpler and has no dependency on
// engine allocator internals.
// -----------------------------------------------------------------------
namespace {
    // Single pool backs command memory, control memory, AND the out-of-
    // memory-callback reserve regions below (see
    // QuadCommandBufferMemoryCallback - AddCommandMemory/AddControlMemory
    // just need a pool-backed offset+size, so the reserve doesn't need its
    // own separate NVNmemoryPool).
    //
    // Real root cause of the abort DrawQuad hit here ("CommandBuffer has run
    // out of control memory with no out-of-memory callback" - decompiled the
    // actual assert site, not guessed): the driver's internal command/
    // control bookkeeping aborts unconditionally the moment it needs to grow
    // past its initial capacity IF no callback is registered, independent of
    // how large the raw byte pool handed to Add*Memory was - real engine
    // code always registers a callback (see QuadCommandBufferMemoryCallback)
    // for exactly this. The initial regions below are still sized somewhat
    // generously (cheap, static memory) so the callback ideally never even
    // needs to fire for a single small quad draw, but the reserve exists so
    // it's handled correctly if it does.
    constexpr size_t kCommandPoolSize = 0x2000000;      // 32MB GPU memory pool for command memory
    alignas(4096) uint8_t g_CommandPoolMemory[kCommandPoolSize]; // GPU-visible backing memory (flag 0x22)
    NVNmemoryPool     g_MemoryPool;
    NVNcommandBuffer  g_CommandBuffer;
    bool g_Initialized = false;

    // Control memory is plain CPU host memory, passed directly to AddControlMemory (3 args)
    constexpr size_t kControlMemorySize = 0x1000000;    // 16MB initial CPU control memory
    constexpr size_t kControlReserveSize = 0x4000000;   // 64MB reserve CPU control memory
    alignas(4096) uint8_t g_ControlMemory[kControlMemorySize];
    alignas(4096) uint8_t g_ControlReserve[kControlReserveSize];
    size_t g_ControlReserveCursor = 0;

    // Vertex position + uniform color data - ordinary CPU-writable memory,
    // flag 0x22, same as the command/control pool above.
    //   [0,   64): vertex position data (4x vec4, one quad)
    //   [256, 272): uniform color data (1x vec4)
    constexpr size_t kQuadDataPoolSize = 4096;
    constexpr ptrdiff_t kQuadVertexOffset = 0;
    constexpr ptrdiff_t kQuadUniformOffset = 256;
    alignas(4096) uint8_t g_QuadDataPoolMemory[kQuadDataPoolSize];
    NVNmemoryPool g_QuadDataMemoryPool;
    NVNbuffer     g_QuadDataBuffer;

    // Full compiled .bnsh file, uploaded to GPU-visible memory (flag 0x62,
    // same as GraphicsNvn's own shader-data pool) so the "data" side of
    // NVNshaderData can address the real GPU code sections directly by
    // their real file offsets - see QuadShader's control/data offset
    // constants. A writable copy (not the constexpr embedded array
    // directly) since NVNbufferBuilder needs real, non-const backing
    // storage.
    constexpr size_t kQuadShaderPoolSize = 0x4000;
    alignas(4096) uint8_t g_QuadShaderPoolMemory[kQuadShaderPoolSize];
    NVNmemoryPool g_QuadShaderMemoryPool;
    NVNbuffer     g_QuadShaderBuffer;
    NVNprogram    g_QuadProgram;
    void* g_QuadNvnProgram = nullptr;

    // Hand-built "control" blobs for nvnProgramSetShaders, read directly
    // from the real nnSdk driver's own decompiled validation logic (not
    // hunted for in ShaderConverter.exe's output - confirmed, by testing a
    // clean solo compile, that raw compiler output never contains this
    // shape at all; the driver builds its own internal wrapper object
    // around whatever raw pointer we hand it as "control", so we only need
    // to supply a well-formed blob, not a wrapper). Mostly zeroed - every
    // dangerous memcpy/loop inside the driver's parse path is gated behind
    // a length/count field that defaults to 0 when zeroed, making the
    // zeroed remainder safe by construction. Sized generously past every
    // offset the decompiled accessors are known to touch (highest seen:
    // control+0x7c0).
    constexpr size_t kQuadControlBlobSize = 0x800;
    alignas(8) uint8_t g_QuadVertexControlBlob[kQuadControlBlobSize];
    alignas(8) uint8_t g_QuadFragmentControlBlob[kQuadControlBlobSize];
    NVNvertexAttribState  g_QuadVertexAttribState;
    NVNvertexStreamState  g_QuadVertexStreamState;
    bool g_QuadInitialized = false;

    // Reserve memory for the command-buffer out-of-memory callback (see
    // QuadCommandBufferMemoryCallback below) - carved out of the SAME
    // g_MemoryPool/g_PoolMemory as the initial command/control regions
    // (AddCommandMemory/AddControlMemory just need a pool-backed offset+size,
    // not a separate pool), tracked with simple bump cursors since the
    // callback can in principle fire more than once.


    // Real-archive pipeline-validation test (see docs/switch-nvn-findings.md
    // "Real-archive pivot" / "Chasing the archive loader through three more
    // real bugs" / "Abandoning the archive loader"). That earlier attempt
    // was shelved because ShaderConverter.exe's bare single-shader CLI
    // output never populates the "variation" reflection records
    // agl::ShaderProgram::forceValidate_ needs - not a bug in the loader or
    // the driver. TextureShaderFile is a REAL, shipped Nintendo shader
    // (pulled from an actual game dump), so it should have real, complete
    // variation data - a genuine test of whether the archive loader (fully
    // debugged earlier this session: grsc-base fix, source-table-offset fix,
    // endian-fixup fix, not-a-sead-thread heap fix) can now produce a
    // properly-populated NVNprogram for us, sidestepping the entire
    // hand-built-control-blob guessing problem entirely.
    alignas(8) uint8_t g_TextureShaderFileBuffer[TextureShaderFile::kSize];
    AglShaderProgramArchive g_TextureShaderArchive;
    void* g_TextureShaderProgram0 = nullptr;
    void* g_TextureShaderNvnProgram = nullptr;
    bool g_TextureShaderInitialized = false;
}

inline void* GetGraphicsNvnInstance() {
    uintptr_t base = WiiXLaunch::ResolveTarget(Offset::kGraphicsNvnInstanceSlot);
    uintptr_t cell = *reinterpret_cast<uintptr_t*>(base);
    static bool loggedOnce = false;
    if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: GraphicsNvn slot base=%p cell=%p", reinterpret_cast<void*>(base), reinterpret_cast<void*>(cell)); }
    if (!cell) return nullptr;
    return *reinterpret_cast<void**>(cell);
}

inline void* GetDebugFontMgrNvnInstance() {
    uintptr_t base = WiiXLaunch::ResolveTarget(Offset::kDebugFontMgrNvnInstanceSlot);
    uintptr_t cell = *reinterpret_cast<uintptr_t*>(base);
    if (!cell) return nullptr;
    return *reinterpret_cast<void**>(cell);
}

template <typename Fn>
inline Fn NvnFn(uintptr_t offset) {
    return ReadIndirect<Fn>(WiiXLaunch::ResolveTarget(offset));
}

// -----------------------------------------------------------------------
// Resolve any real nvn* function by name directly, instead of hand-curating
// an Offset:: slot for it (the approach every other function in this file
// uses - fragile, one xref hunt per function). This calls into nnSdk's OWN
// internal resolver, decompiled directly this session while chasing the
// nvnProgramSetShaders control-blob validation: nvnBootstrapLoaderInternal
// (the thing that resolves "nvnFoo" -> real function pointer for every
// nvn* call BotW itself ever makes) is a thin wrapper around
// FUN_71002e0534(0, "nvnFoo") - a binary search over a compressed,
// alphabetically-sorted name table baked into nnSdk, stripping the leading
// "nvn" itself. Since nnSdk has already been through its own lazy-init path
// by the time our present_ hook first fires (BotW's own boot resolved
// hundreds of these already), calling this ourselves, well after boot, is
// just a plain lookup - no side effects beyond a redundant no-op re-init
// check. 0x2e0534 is FUN_71002e0534's file offset within the nnSdk NSO
// (Ghidra address 0x71002e0534 minus this session's established
// 0x7100000000 base convention for nnSdk), applied to nnSdk's real, live,
// ASLR'd base via exl::util::GetModuleInfo(ModuleIndex::Sdk) rather than
// WiiXLaunch::ResolveTarget (which only knows about the main module).
inline void* ResolveNvnFunctionByName(const char* nvnPrefixedName) {
    uintptr_t sdkBase = exl::util::GetModuleInfo(exl::util::ModuleIndex::Sdk).m_Total.m_Start;
    using FnResolveNvnByName = void* (*)(long, const char*);
    auto resolve = reinterpret_cast<FnResolveNvnByName>(sdkBase + 0x2e0534);
    return resolve(0, nvnPrefixedName);
}

template <typename Fn>
inline Fn ResolveNvnFn(const char* nvnPrefixedName) {
    return reinterpret_cast<Fn>(ResolveNvnFunctionByName(nvnPrefixedName));
}

// -----------------------------------------------------------------------
// Command-buffer out-of-memory callback. Real, documented NVN API
// (nvn.h:3304-3322, 4735-4738) - not something we're papering over a driver
// bug with. Decompiled the real abort site that motivated this (see
// g_CommandReserveCursor's comment and kCommandMemorySize's comment): the
// driver aborts immediately and unconditionally the moment its own internal
// command/control bookkeeping needs to grow past its initial capacity IF no
// callback is registered ("has run out of ... memory with no out-of-memory
// callback"), regardless of how large the raw byte pool we handed to
// AddCommandMemory/AddControlMemory was - real engine code always
// registers one of these, we just hadn't yet. Bumping the initial pool size
// earlier reduced how OFTEN this would need to fire, but never could have
// fixed it outright on its own.
using FnCommandBufferSetMemoryCallback     = void (*)(NVNcommandBuffer*, void* callback);
using FnCommandBufferSetMemoryCallbackData = void (*)(NVNcommandBuffer*, void* data);

// Matches real NVNcommandBufferMemoryEvent (nvn.h:3305-3322) by value, not
// by including nvn.h's full enum (we don't have the real header wired into
// this translation unit, same as everywhere else in this file).
constexpr int32_t kEventOutOfCommandMemory = 0;
constexpr int32_t kEventOutOfControlMemory = 1;

// First attempt at this callback advanced the reserve cursor AFTER calling
// AddCommandMemory/AddControlMemory - which recursed ~150 deep into a guest
// stack overflow, because Add*Memory itself re-enters this same callback
// synchronously (registering a new chunk apparently needs a little of its
// own bookkeeping room), and the reentrant call saw the SAME stale cursor
// value, so it kept handing back the identical region forever instead of
// ever converging or reporting real exhaustion. Fixed by advancing the
// cursor BEFORE the Add call, so any reentrant call sees fresh state.
//
// Second attempt handed out the ENTIRE remaining reserve in one grant
// (reasoning: nvn.h warns minimal-sized grants cause frequent re-callbacks)
// - which backfired: the very next, unrelated growth request (minSize=256,
// trivially small) saw remaining=0 and aborted, even with a 256KB reserve,
// because the driver apparently tracks each AddControlMemory call as its
// own distinct block record it may need MORE of later - not just a running
// byte total. Handing away the whole budget on the first call starves every
// later call regardless of how much real memory was actually available.
// Fixed by granting a modest, reusable, FIXED-size chunk per call instead
// (still much larger than nvn.h's minimum-size warning is about) so the
// reserve survives many separate grants over the frame rather than one.
//
// Third attempt (real, in-game, complex captured program - see
// DrawWithCapturedProgram): 16KB chunks converged correctly (no more
// infinite loop) but needed ~130 recursive re-entries to satisfy one real
// draw call, and 130 native call-stack frames is enough on its own to
// overflow the guest thread's stack - a crash with NO "reserve exhausted"
// log and a fault address sitting right at the stack pointer, not the
// clean abort-with-message seen every other time. Bumping the TOTAL
// reserve size only makes this worse (more total grants needed = more
// recursion depth = guaranteed overflow before ever reaching "out of
// memory") - each grant is a recursive call into this same callback, not a
// simple allocation, so the real constraint is CALL-STACK DEPTH, not
// total capacity. Fixed by granting far bigger chunks (32x) so far fewer
// recursive re-entries are needed for the same real total, keeping
// recursion depth shallow regardless of how much a real complex shader
// ultimately needs.
//
// Fourth attempt: 512KB grants against an 8MB reserve converged cleanly
// (recursion depth stayed a safe, exact 16 levels - no stack overflow) but
// the real total need is bigger than 8MB. Scaled the reserve up to 128MB
// (see kControlReserveSize's comment) and the grant size up right along
// with it (16x, matching the reserve's 16x increase) so recursion depth
// stays pinned at the same ~16 levels that already proved safe, no matter
// how large the real total turns out to be - depth is a function of
// (reserve / grant), not of either number alone.
//
// Fifth attempt: added per-call diagnostic logging (raw cursor/reserveSize/
// minSize, not just derived "remaining") to a real captured-program draw.
// Result was conclusive and surprising: exactly 17 calls, ALL in the same
// millisecond, ALL with the identical minSize=256 - cursor advancing by
// precisely one kMemoryGrantChunkSize (8MB) each time until the full 128MB
// reserve was gone. Depth-until-failure exactly equals reserve/chunk (here
// 128MB/8MB=16; the earlier stack-overflow attempt was ~2MB/16KB=~130) in
// BOTH tests - proof the recursion never naturally terminates on its own,
// regardless of total reserve size; it only stops when the reserve runs
// out. Since minSize never grows across all 17 calls, this isn't "the real
// program needs more control memory than we gave it" - it's each DISTINCT
// AddControlMemory call itself costing exactly one more recursive re-entry
// (matching the "first attempt" finding that Add*Memory needs a little of
// its own bookkeeping room), independent of how large that call's chunk
// was. Fixing this means minimizing the NUMBER of Add calls, not their
// size - so reviving the "second attempt" strategy (grant the ENTIRE
// remaining reserve in one shot) but this time against a real 128MB
// budget instead of the tiny 256KB that caused it to be abandoned back
// then. One draw's legitimate real need is nowhere near 128MB, so a single
// big grant should leave so much slack that no second call - and thus no
// further recursive tax - is ever needed, unlike 16 separate 8MB calls
constexpr size_t kMemoryGrantChunkSize = 0x800000; // 8MB

inline void QuadCommandBufferMemoryCallback(NVNcommandBuffer* cmdBuf, int32_t event, size_t minSize, void* /*callbackData*/) {
    static uint32_t s_CallbackCount = 0;
    uint32_t callId = ++s_CallbackCount;

    if (event == kEventOutOfCommandMemory) {
        WIIXL_LOG("NvnOverlay: cmd cb #%u entry, minSize=%u", callId, static_cast<unsigned int>(minSize));
    } else {
        size_t remaining = kControlReserveSize - g_ControlReserveCursor;
        size_t give = (minSize > kMemoryGrantChunkSize ? minSize : kMemoryGrantChunkSize);
        if (give > remaining) give = remaining;
        WIIXL_LOG("NvnOverlay: ctrl cb #%u entry, cursor=%u reserveSize=%u minSize=%u give=%u remaining=%u",
            callId, static_cast<unsigned int>(g_ControlReserveCursor), static_cast<unsigned int>(kControlReserveSize),
            static_cast<unsigned int>(minSize), static_cast<unsigned int>(give), static_cast<unsigned int>(remaining));
        if (give == 0 || give < minSize) { WIIXL_LOG("NvnOverlay: control memory reserve exhausted (minSize=%u, remaining=%u)", static_cast<unsigned int>(minSize), static_cast<unsigned int>(remaining)); return; }
        void* ptr = g_ControlReserve + g_ControlReserveCursor;
        g_ControlReserveCursor += give;
        NvnFn<FnCommandBufferAddControlMemory>(Offset::kCommandBufferAddControlMemory)(cmdBuf, ptr, give);
        WIIXL_LOG("NvnOverlay: memory callback added %u bytes control memory at ptr=%p", static_cast<unsigned int>(give), ptr);
    }
}

// One-time setup: build our own small command buffer + backing memory pool.
// Must run after the game's own graphics system is initialized (i.e. not
// before the first frame) - called lazily from the present_ hook below.
inline void EnsureInitialized() {
    if (g_Initialized) return;

    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: GraphicsNvn instance null"); }
        return; // graphics system not up yet, try again next frame
    }
    void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + kGraphicsNvnDeviceOffset);
    if (!device) {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: device null, graphicsNvn=%p", graphicsNvn); }
        return;
    }

    WIIXL_LOG("NvnOverlay: step A, device=%p", device);

    NVNmemoryPoolBuilder poolBuilder{};
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&poolBuilder);
    WIIXL_LOG("NvnOverlay: step B (SetDefaults)");
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&poolBuilder, device);
    WIIXL_LOG("NvnOverlay: step C (SetDevice)");
    // 0x22: the exact flag value GraphicsNvn::initializeImpl itself uses for
    // its own general-purpose pool - reused rather than guessed.
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&poolBuilder, 0x22);
    WIIXL_LOG("NvnOverlay: step D (SetFlags)");
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&poolBuilder, g_CommandPoolMemory, sizeof(g_CommandPoolMemory));
    WIIXL_LOG("NvnOverlay: step E (SetStorage), g_CommandPoolMemory=%p sz=%u", g_CommandPoolMemory, static_cast<unsigned int>(sizeof(g_CommandPoolMemory)));

    int poolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(&g_MemoryPool, &poolBuilder);
    WIIXL_LOG("NvnOverlay: step F (PoolInitialize), result=%d", poolResult);
    if (!poolResult) {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: MemoryPoolInitialize failed, retrying next frame"); }
        return;
    }

    NvnFn<FnCommandBufferRealInit>(Offset::kCommandBufferRealInit)(&g_CommandBuffer, device);
    WIIXL_LOG("NvnOverlay: step G (CommandBufferRealInit)");

    auto setCb = NvnFn<FnCommandBufferSetMemoryCallback>(Offset::kCommandBufferSetMemoryCallback);
    if (setCb) {
        setCb(&g_CommandBuffer, reinterpret_cast<void*>(&QuadCommandBufferMemoryCallback));
        WIIXL_LOG("NvnOverlay: step G2 (SetMemoryCallback registered)");
    }
    using FnSetCbData = void (*)(NVNcommandBuffer*, void*);
    auto setCbData = NvnFn<FnSetCbData>(Offset::kCommandBufferSetMemoryCallbackData);
    if (setCbData) {
        setCbData(&g_CommandBuffer, &g_CommandBuffer);
        WIIXL_LOG("NvnOverlay: step G3 (SetMemoryCallbackData registered)");
    }

    NvnFn<FnCommandBufferAddCommandMemory>(Offset::kCommandBufferAddCommandMemory)(&g_CommandBuffer, &g_MemoryPool, 0, sizeof(g_CommandPoolMemory));
    WIIXL_LOG("NvnOverlay: step H (AddCommandMemory)");
    NvnFn<FnCommandBufferAddControlMemory>(Offset::kCommandBufferAddControlMemory)(&g_CommandBuffer, g_ControlMemory, sizeof(g_ControlMemory));
    WIIXL_LOG("NvnOverlay: step I (AddControlMemory), g_ControlMemory=%p sz=%u", g_ControlMemory, static_cast<unsigned int>(sizeof(g_ControlMemory)));

    g_Initialized = true;
    WIIXL_LOG("NvnOverlay: EnsureInitialized OK, graphicsNvn=%p device=%p", graphicsNvn, device);
}

// -----------------------------------------------------------------------
// Minimal sead types we construct ourselves. Real classes (sead::DrawContext
// /LookAtCamera/OrthoProjection all have real, callable constructors we
// invoke via GetTargetFunction below) - these are just correctly-sized
// opaque storage for them, since we don't have sead's real headers either.
// Sizes are generous upper bounds, not measured exactly (unlike the NVN
// structs, which came from a real header) - safe to be oversized, not safe
// to be undersized.
// -----------------------------------------------------------------------
struct SeadDrawContext    { alignas(8) uint8_t reserved[0x120]; };
struct SeadLookAtCamera   { alignas(8) uint8_t reserved[0x60]; };
struct SeadOrthoProjection{ alignas(8) uint8_t reserved[0x100]; };
struct Vector3f { float x, y, z; };
struct Matrix34f { float m[3][4]; };
struct Color4f { float r, g, b, a; };

using FnDrawContextCtor = void (*)(SeadDrawContext*);
using FnDrawContextDtor = void (*)(SeadDrawContext*);
using FnLookAtCameraCtor = void (*)(SeadLookAtCamera*, const Vector3f*, const Vector3f*, const Vector3f*);
using FnOrthoProjectionCtor = void (*)(SeadOrthoProjection*, float near_, float far_, float top, float bottom, float left, float right);
using FnDebugFontBegin = void (*)(void* mgr, SeadDrawContext*);
using FnDebugFontEnd   = void (*)(void* mgr, SeadDrawContext*);
using FnDebugFontPrint = void (*)(void* mgr, SeadDrawContext*, const SeadOrthoProjection*, const SeadLookAtCamera*, const Matrix34f*, const Color4f*, const uint16_t* text, int count);
using FnFrameBufferBind = void (*)(void* frameBuffer, SeadDrawContext*);

using FnDebugFontMgrCreateInstance = void* (*)(void* heap);
using FnDebugFontMgrInitializeFromBinary = void (*)(void* self, void* heap, const void* shaderBytes, size_t shaderSize, const void* textureBytes, size_t textureSize, uint32_t uniformBufSize);

namespace {
    void* g_ManualFontMgrInstance = nullptr;
    bool g_FontMgrInitAttempted = false;
    // Real object size, read directly from createInstance's own decompiled
    // operator new[] call (`_ZnamPN4sead4HeapEi(0x530, heap, 8)`) - see
    // ManuallyConstructDebugFontMgrInstance's comment for why we build this
    // ourselves in static memory instead of calling createInstance.
    constexpr size_t kDebugFontMgrNvnSize = 0x530;
    alignas(8) uint8_t g_FontMgrStorage[kDebugFontMgrNvnSize];
}

// createInstance()'s own operator new[](0x530, heap, 8) call reliably
// returns null - confirmed via register dump, not guessed (X0=8 at the
// crash matches (long*)nullptr + 1 exactly, the "this" pointer createInstance
// passes to IDisposer's constructor after a failed allocation). Tried two
// different real heaps found via findContainHeap (probed with graphicsNvn,
// then gameFramework) - both reject this specific ~1.3KB request, even
// though a same-order-of-magnitude allocation succeeded via the same
// technique during the earlier archive-loader test. Rather than keep
// guessing at heap selection, sidestep the allocator entirely - consistent
// with how every other NVN/game object in this whole file already works
// (g_CommandBuffer, g_QuadProgram, etc. are all static storage, never
// heap-allocated).
//
// Replicated directly from createInstance's real decompile (not guessed):
// its very first lines check the singleton slot and only allocate+
// initialize if it's still null, so pre-populating that slot with our own
// object (after doing the initialization ourselves) makes any future real
// call to createInstance() - by us or anything else - just harmlessly
// return our pointer.
//
// Skips calling the real sead::IDisposer::IDisposer(Heap*, HeapNullOption)
// constructor (decompiled: 0x7100b043c0) that createInstance itself calls -
// its only two field writes are (a) a vtable pointer at offset+8, which
// createInstance's own next line immediately overwrites anyway, and (b) the
// heap pointer at offset+16, replicated directly below. The real ctor's
// only OTHER effect is registering with the heap's disposer linked list
// (sead::Heap::appendDisposer_) for automatic cleanup when the heap is
// destroyed - meaningless for our statically-allocated object, which is
// never freed, and avoids one more heap-touching call after the allocator
// itself already proved unreliable from this thread/heap combination.
//
// PTR_DAT_xxxx symbols used bare (not `*(T*)PTR_DAT_xxxx`) in Ghidra's
// decompile of createInstance represent the GLOBAL'S OWN ADDRESS used
// directly as a value (the same idiom as the `PTR__ZTV...+0x10` vtable
// pattern already used elsewhere in this file) - resolved live via
// WiiXLaunch::ResolveTarget, not hard-coded from Ghidra's static image.
inline void* ManuallyConstructDebugFontMgrInstance(void* heap) {
    uint8_t* base = g_FontMgrStorage;
    __builtin_memset(base, 0, kDebugFontMgrNvnSize);

    void* vtbl0 = reinterpret_cast<void*>(WiiXLaunch::ResolveTarget(0x2597e28) + 0x10); // primary vtable, offset+0
    void* vtbl1 = reinterpret_cast<void*>(WiiXLaunch::ResolveTarget(0x2597e18) + 0x10); // secondary (IDisposer-slot) vtable, offset+8

    *reinterpret_cast<void**>(base) = vtbl0;
    *reinterpret_cast<void**>(base + 8) = vtbl1;
    *reinterpret_cast<void**>(base + 16) = heap; // IDisposer's own heap-pointer field

    // Global "last constructed" bookkeeping slot createInstance itself
    // writes to (*(long**)PTR_DAT_7102597e20 = plVar3 + 1, i.e. the
    // IDisposer sub-object's address, offset+8 into our buffer).
    *reinterpret_cast<uint8_t**>(WiiXLaunch::ResolveTarget(0x2597e20)) = base + 8;

    // Pre-populate DebugFontMgrNvn's own singleton slot - single-indirect
    // (matches createInstance's real `*(long**)PTR_DAT_7102591a98` read/
    // write, NOT the double-indirect pattern GetDebugFontMgrNvnInstance()
    // currently (incorrectly) uses - separate, pre-existing bug, harmless
    // here since we don't rely on that read path).
    *reinterpret_cast<uint8_t**>(WiiXLaunch::ResolveTarget(Offset::kDebugFontMgrNvnInstanceSlot)) = base;

    return base;
}

// Manually constructs+initializes a sead::DebugFontMgrNvn instance using a
// REAL, unmodified BotW asset (nvn_font_shader.bin/nvn_font.ntx, pulled
// directly from a real game dump - see docs/switch-nvn-findings.md).
// createInstance/initializeFromBinary are real, decompiled BotW functions,
// called directly rather than waiting on the game's own construction (which
// never happens in this build - the original DrawTextOverlay test found
// GetDebugFontMgrNvnInstance() staying null indefinitely, and that
// conclusion held up even after fixing an earlier, unrelated hook-address
// typo that had made the *diagnostic* untrustworthy).
//
// initializeFromBinary's real signature (decompiled directly, not guessed):
// (self, heap, shaderBytes, shaderSize, textureBytes, textureSize,
// uniformBufSize) - the "flags"-looking last param is actually stored and
// later used as a uniform-buffer size (rounded up to a page), not a bitmask;
// uniformBufSize is deliberately generous (4096) since real callers'
// exact value isn't known and this only affects a small ring-buffer size.
inline void EnsureFontManagerInitialized(void* gameFramework) {
    if (g_FontMgrInitAttempted) return;
    g_FontMgrInitAttempted = true;

    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;

    // Probing findContainHeap with graphicsNvn's own address (as used
    // elsewhere in this file) found a real heap, but its own vtable+0x30
    // alloc() rejected a plain 0x530-byte createInstance() allocation
    // outright (confirmed via register dump: operator new[] genuinely
    // returned null, not a crash inside the allocator itself) - the
    // graphics-subsystem heap graphicsNvn lives in is apparently too
    // narrow/full for this. Probing with gameFramework's address instead,
    // since that's a much bigger, more central game object more likely to
    // live in a general-purpose heap.
    void* heapMgrInstance = *reinterpret_cast<void**>(WiiXLaunch::ResolveTarget(Offset::kHeapMgrInstancePtr));
    auto findContainHeap = WiiXLaunch::GetTargetFunction<FnHeapMgrFindContainHeap>(Offset::kHeapMgrFindContainHeap, 0);
    void* heap = findContainHeap(heapMgrInstance, gameFramework);
    WIIXL_LOG("NvnOverlay: fontmgr step A (heap), heap=%p", heap);
    if (!heap) return;
    auto setAllocFromNotSeadThreadHeap = WiiXLaunch::GetTargetFunction<FnHeapMgrSetAllocFromNotSeadThreadHeap>(Offset::kHeapMgrSetAllocFromNotSeadThreadHeap, 0);
    setAllocFromNotSeadThreadHeap(heapMgrInstance, heap);

    void* instance = ManuallyConstructDebugFontMgrInstance(heap);
    WIIXL_LOG("NvnOverlay: fontmgr step B (manual construct), instance=%p", instance);

    auto initializeFromBinary = WiiXLaunch::GetTargetFunction<FnDebugFontMgrInitializeFromBinary>(Offset::kDebugFontMgrInitializeFromBinary, 0);
    initializeFromBinary(instance, heap, NvnFontShaderFile::kBytes, NvnFontShaderFile::kSize, NvnFontTextureFile::kBytes, NvnFontTextureFile::kSize, 4096);
    WIIXL_LOG("NvnOverlay: fontmgr step C (initializeFromBinary returned)");

    g_ManualFontMgrInstance = instance;
    WIIXL_LOG("NvnOverlay: EnsureFontManagerInitialized OK, instance=%p", instance);
}

// Draws one line of ASCII text as an always-on overlay. Called from the
// present_ hook, after Orig() - see findings doc for why this ordering is
// safe (a second, independent nvnQueueSubmitCommands to the same queue,
// executed after the game's own submission, needs no shared state).
//
// UNTESTED. Screen-space bounds (kScreenTop/Bottom/Left/Right below) are a
// 720p guess, not measured - expect to need visual iteration once this
// actually runs. Same for the text's position (currently just world-space
// origin via an identity Matrix34 - see findings doc on why the Camera
// parameter doesn't actually matter for placement, only this matrix does).
inline void DrawTextOverlay(void* gameFramework, const char* asciiText) {
    EnsureInitialized();
    if (!g_Initialized) return;

    void* queue = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kQueueOffset);
    if (!queue) {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: queue null, gameFramework=%p", gameFramework); }
        return;
    }

    EnsureFontManagerInitialized(gameFramework);
    void* fontMgr = g_ManualFontMgrInstance ? g_ManualFontMgrInstance : GetDebugFontMgrNvnInstance();
    {
        // Edge-triggered rather than log-once, so we can actually tell
        // whether this ever recovers on a later frame instead of just
        // failing silently forever after the first log line - that
        // ambiguity already cost a full test round once today.
        static bool first = true;
        static bool wasNull = false;
        static uint32_t attempts = 0;
        attempts++;
        bool isNull = (fontMgr == nullptr);
        if (first || isNull != wasNull) {
            first = false;
            wasNull = isNull;
            WIIXL_LOG("NvnOverlay: fontMgr %s after %u attempts, fontMgr=%p", isNull ? "null" : "valid", attempts, fontMgr);
        }
    }
    if (!fontMgr) {
        return;
    }

    void* frameBufferCheck = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kFrameBufferOffset);
    {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: queue=%p fontMgr=%p frameBuffer=%p", queue, fontMgr, frameBufferCheck); }
    }

    // ASCII -> UTF-16, matching print()'s confirmed 2-bytes-per-char read
    // pattern (see findings doc) - only printable ASCII (0x20-0x7E) actually
    // renders as itself, per the glyph-atlas range confirmed there.
    constexpr int kMaxChars = 127; // print() itself clamps to 128
    uint16_t utf16[kMaxChars + 1];
    int len = 0;
    for (; asciiText[len] != '\0' && len < kMaxChars; len++) {
        utf16[len] = static_cast<uint16_t>(static_cast<unsigned char>(asciiText[len]));
    }

    auto drawContextCtor = WiiXLaunch::GetTargetFunction<FnDrawContextCtor>(0xb02c5c, 0);
    auto drawContextDtor = WiiXLaunch::GetTargetFunction<FnDrawContextDtor>(0xb02cc8, 0);
    auto lookAtCameraCtor = WiiXLaunch::GetTargetFunction<FnLookAtCameraCtor>(0xb1bd88, 0);
    auto orthoProjectionCtor = WiiXLaunch::GetTargetFunction<FnOrthoProjectionCtor>(0xb1e154, 0);
    auto fontBegin = WiiXLaunch::GetTargetFunction<FnDebugFontBegin>(0xaff91c, 0);
    auto fontPrint = WiiXLaunch::GetTargetFunction<FnDebugFontPrint>(0xaff94c, 0);
    auto fontEnd   = WiiXLaunch::GetTargetFunction<FnDebugFontEnd>(0xaff948, 0);

    SeadDrawContext drawContext{};
    drawContextCtor(&drawContext);

    // Bind to the game's own live render target - without this the draw
    // commands below have nowhere to write to. See kFrameBufferOffset.
    void* frameBuffer = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kFrameBufferOffset);
    if (frameBuffer) {
        auto frameBufferBind = WiiXLaunch::GetTargetFunction<FnFrameBufferBind>(0xb1cca8, 0);
        frameBufferBind(frameBuffer, &drawContext);
    }

    // Arbitrary, valid camera - print() doesn't actually dereference it (see
    // findings doc), only needs to exist. Values themselves are throwaway.
    Vector3f camPos{0.0f, 0.0f, 1.0f};
    Vector3f camAt{0.0f, 0.0f, 0.0f};
    Vector3f camUp{0.0f, 1.0f, 0.0f};
    SeadLookAtCamera camera{};
    lookAtCameraCtor(&camera, &camPos, &camAt, &camUp);

    // 720p screen-space guess - unverified, needs visual iteration.
    constexpr float kScreenTop = 0.0f, kScreenBottom = 720.0f;
    constexpr float kScreenLeft = 0.0f, kScreenRight = 1280.0f;
    SeadOrthoProjection projection{};
    orthoProjectionCtor(&projection, -1.0f, 1.0f, kScreenTop, kScreenBottom, kScreenLeft, kScreenRight);

    // Identity - text lands at world/screen origin. Real positioning is a
    // follow-up once we can see where that actually is on screen.
    Matrix34f transform{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    }};
    Color4f white{1.0f, 1.0f, 1.0f, 1.0f};

    auto beginRecording = NvnFn<FnCommandBufferBeginRecording>(Offset::kCommandBufferBeginRecording);
    auto endRecording    = NvnFn<FnCommandBufferEndRecording>(Offset::kCommandBufferEndRecording);
    auto submitCommands  = NvnFn<FnQueueSubmitCommands>(Offset::kQueueSubmitCommands);

    beginRecording(&g_CommandBuffer);
    fontBegin(fontMgr, &drawContext);
    fontPrint(fontMgr, &drawContext, &projection, &camera, &transform, &white, utf16, len);
    fontEnd(fontMgr, &drawContext);
    NVNcommandHandle handle = endRecording(&g_CommandBuffer);

    drawContextDtor(&drawContext);

    submitCommands(queue, 1, &handle);

    {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: submitted, handle=%p len=%d", reinterpret_cast<void*>(handle), len); }
    }
}

// -----------------------------------------------------------------------
// Raw quad pipeline: our own compiled shader, our own vertex data, no
// game helper classes involved (see docs/switch-nvn-findings.md - both
// DebugFontMgrNvn and PrimitiveDrawMgrNvn are confirmed to never construct
// at runtime, and even if they did, both are locked to font-glyph or
// fixed-primitive semantics; this bypasses that entirely).
//
// The shader binary bytes below are extracted from a real .bnsh file
// compiled with Nintendo's own official ShaderConverter.exe (from a real,
// locally-available NVN SDK copy - see findings doc "BREAKTHROUGH" section),
// not reverse-engineered or hand-assembled. Source GLSL:
//   vertex:   gl_Position = a_Position;                    (location 0, vec4)
//   fragment: o_Color = u_Color;   (uniform block "wiixl_material", binding 1)
//
// Byte layout: two 256-byte blocks back to back (vertex, then fragment),
// extracted from wiixl_quad.bnsh offsets [8288, 8800) - confirmed by hex
// inspection to contain real Maxwell SASS (see findings doc). Each block is
// [control header][compiled code], but the EXACT control/code split point
// is NOT known from any authoritative source - a 32-byte guess (visually,
// where recognizable instruction patterns started) made nvnProgramSetShaders
// fail (result=0, no crash). Traced the real call site
// (agl::ShaderProgram::forceValidate_) far enough to confirm the game
// doesn't use a fixed split either - it reads pre-computed offset/pointer
// fields from a wrapper object built during archive parsing, which we don't
// replicate. Rather than reverse-engineer that whole pipeline,
// EnsureQuadInitialized below asks the real driver directly: try a range of
// split points at runtime and keep whichever one nvnProgramSetShaders
// actually accepts.
namespace QuadShader {
    // Real control-section addresses within the full wiixl_quad.bnsh file,
    // found empirically - not guessed, not brute-forced. Every prior attempt
    // (two exhaustive split-point brute-force rounds against the raw
    // per-program 256-byte code blocks, then a full real-archive-loader
    // pivot that got deep into agl::ShaderProgramArchive before hitting a
    // genuine tooling gap - see findings doc for the full history of both)
    // was working from the wrong mental model: nvn.h documents
    // NVNshaderData::control as literally "Control section from the offline
    // compiler" - a real, self-identifying section GLSLC writes into the
    // output, not a raw byte-range we need to guess the boundary of.
    // nvnTool_GlslcInterface.h defines real magic numbers for exactly this:
    // GLSLC_GPU_CODE_SECTION_CONTROL_MAGIC_NUMBER (0x98761234) marks the
    // start of a control section, GLSLC_GPU_CODE_SECTION_DATA_MAGIC_NUMBER
    // (0x12345678) marks a GPU code data section. Searching our compiled
    // file's raw bytes for these exact 4-byte patterns found exactly two of
    // each - matching vertex+fragment - at these fixed offsets (confirmed
    // via Python against the real compiled output, not assumed to be
    // portable to other shaders/compiles).
    // Two attempts using the 0x98761234-tagged blobs as "control" (at the
    // magic's own address, then +0x10 past a plausible header) both
    // returned result=0. Those two blobs are also byte-identical between
    // vertex and fragment for every field sampled - a red flag that was
    // never resolved and suggests they aren't real per-stage control
    // sections the driver wants at all (possibly generic/shared metadata
    // for something else - GLSLC_GPU_CODE_SECTION_CONTROL_MAGIC_NUMBER
    // marks *a* control section, not necessarily *the* one this call
    // needs). New hypothesis, abandoning the magic-number search entirely:
    // "control" is a CPU-readable pointer directly to the SPH itself - the
    // exact same SPH that "data"'s GPU address also starts covering, just
    // accessed as host memory instead of a GPU address, matching how
    // minimal/homebrew NVN shader loaders (not going through the full
    // archive/reflection system) typically work, and matching nvn.h's
    // plain "CPU pointer" description with no separate section to locate.
    // SPH starts at dataOffset+0x30 (see kVertexDataOffset below) within
    // the SAME uploaded pool, read via its CPU-mapped host address
    // (g_QuadShaderPoolMemory) rather than the GPU address used for data.
    constexpr ptrdiff_t kVertexControlOffset   = 0x2000 + 0x30;
    constexpr ptrdiff_t kFragmentControlOffset = 0x2100 + 0x30;
    // Two earlier data-offset guesses (the DATA magic's own address
    // directly, then +0x60 to the extracted code block's start) both got a
    // clean result=0. Resolved by finding real, independent prior art:
    // DCNick3/shader-compiler-rs (a real NVN-shader-to-GLSL tool built on
    // the Yuzu/Hades shader recompiler) documents that a raw NVN "data"
    // blob - literally the thing passed as NVNshaderData.data - is laid out
    // as [0x30-byte NVN-specific header][SPH][real SASS code], and its CLI
    // explicitly does `&shader[0x30..]` to skip that header before handing
    // the rest (SPH + code) to the translator. NVIDIA's real Shader Program
    // Header (SPH) - the header immediately after that 0x30-byte prefix -
    // is a well-documented, fixed 0x50-byte (80-byte) structure. So real
    // code should start exactly 0x30+0x50=0x80 bytes after the true "data"
    // blob start. The DATA magic locations found earlier (0x2000, 0x2100)
    // plus 0x80 land EXACTLY on the independently-verified real SASS start
    // (0x2080, 0x2180, confirmed via direct byte search for a distinctive
    // instruction pattern) - confirming the DATA magic's own address really
    // is the correct "data" pointer after all; the two later adjustments
    // were wrong turns.
    constexpr ptrdiff_t kVertexDataOffset      = 0x2000;
    constexpr ptrdiff_t kFragmentDataOffset    = 0x2100;
}

// NVN vertex attribute format constants (from NVNformat, real SDK enum -
// only these two values are needed here, not the whole enum).
namespace NvnFormat {
    constexpr int kR32G32B32A32Float = 0x2e; // NVN_FORMAT_RGBA32F (4 x 32-bit float) - confirmed from PrimitiveDrawMgrNvn
    constexpr int kR32G32B32Float    = 0x22; // NVN_FORMAT_RGB32F (3 x 32-bit float)
    constexpr int kR32G32Float       = 0x16; // NVN_FORMAT_RG32F (2 x 32-bit float)
}

// One-time setup for the raw-quad pipeline: uploads our compiled shader,
// builds the vertex/uniform buffer, sets up program + vertex state. Mirrors
// EnsureInitialized's shape (same command-buffer/queue machinery, already
// proven not to crash) but is otherwise independent of it.
inline void EnsureQuadInitialized(void* gameFramework) {
    if (g_QuadInitialized) return;

    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return; // logged already by EnsureInitialized's own check
    void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + kGraphicsNvnDeviceOffset);
    if (!device) return;

    // Driving nvnProgramSetShaders directly, using the real control-section
    // addresses found via magic-number search (see QuadShader's comment for
    // the full history - the archive-loading approach got as far as a real,
    // genuine tooling gap in ShaderConverter.exe's bare output before this
    // much simpler fix was found).
    __builtin_memcpy(g_QuadShaderPoolMemory, QuadShaderFile::kBytes, QuadShaderFile::kSize);

    NVNmemoryPoolBuilder shaderPoolBuilder{};
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&shaderPoolBuilder);
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&shaderPoolBuilder, device);
    // 0x62: shader CODE pool flag (matches GraphicsNvn's own shader-data
    // pool, confirmed real bug fix earlier this session - not the 0x22
    // general-purpose flag used for the vertex/uniform data pool below).
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&shaderPoolBuilder, 0x62);
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&shaderPoolBuilder, g_QuadShaderPoolMemory, sizeof(g_QuadShaderPoolMemory));
    int shaderPoolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(&g_QuadShaderMemoryPool, &shaderPoolBuilder);
    WIIXL_LOG("NvnOverlay: quad step A (shader PoolInitialize), result=%d", shaderPoolResult);
    if (!shaderPoolResult) return;

    NVNbufferBuilder shaderBufBuilder{};
    NvnFn<FnBufferBuilderSetDevice>(Offset::kBufferBuilderSetDevice)(&shaderBufBuilder, device);
    NvnFn<FnBufferBuilderSetDefaults>(Offset::kBufferBuilderSetDefaults)(&shaderBufBuilder);
    NvnFn<FnBufferBuilderSetStorage>(Offset::kBufferBuilderSetStorage)(&shaderBufBuilder, &g_QuadShaderMemoryPool, 0, sizeof(g_QuadShaderPoolMemory));
    uint8_t shaderBufResult = NvnFn<FnBufferInitialize>(Offset::kBufferInitialize)(&g_QuadShaderBuffer, &shaderBufBuilder);
    WIIXL_LOG("NvnOverlay: quad step B (shader BufferInitialize), result=%d", shaderBufResult);
    if (!shaderBufResult) return;

    uint64_t shaderGpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_QuadShaderBuffer);
    WIIXL_LOG("NvnOverlay: quad step C (shader GetAddress), gpuBase=%p", reinterpret_cast<void*>(shaderGpuBase));

    // Hand-built control blobs (see g_QuadVertexControlBlob's comment for
    // the full derivation). +4=1 is the driver's own "is this real" check;
    // +8 is a stage code the driver's real switch statement accepts in
    // [5,0xE] (5/6=one wrapper shape, 7=another, 8/9/10=another, 0xB/0xC/
    // 0xD/0xE=the rest, each just selecting a differently-sized internal
    // wrapper the driver builds itself - the exact code doesn't need to
    // "mean" vertex/fragment to us, it just needs to be valid and distinct
    // per stage); +0x714 is a 0-3 bucket index used to key an internal
    // 4-slot array - must be unique per simultaneous stage or one silently
    // replaces the other.
    //
    // +0xc/+0x10 are new: decompiled the real FUN_71002e3494 tail (the
    // consistency-check block after the per-entry parse loop) and found the
    // actual reason a crash-free, correctly-shaped blob was still getting
    // result=0. It's not a "does this look tampered with" anti-cheat style
    // check - it's a genuine device/shader ISA-version match: the driver
    // reads two ints back out of our control blob via vtable+0x20/+0x28
    // (disassembled directly: `ldr w0,[x8,#0xc]` / `ldr w0,[x8,#0x10]` off
    // the raw blob pointer) and requires BOTH to equal the real device's own
    // *(int*)(device+0x2c) / *(int*)(device+0x30) (GPU arch/ISA version
    // fields) before it will accept the shader. We were leaving both at 0,
    // which never matches the real device - so instead of patching out the
    // check (would touch driver code shared with the game's own real
    // shaders), just populate the two fields correctly from the live device
    // we already have a pointer to.
    int32_t deviceIsaA = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x2c);
    int32_t deviceIsaB = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x30);
    WIIXL_LOG("NvnOverlay: quad step C3 (device ISA), a=%d b=%d", deviceIsaA, deviceIsaB);

    __builtin_memset(g_QuadVertexControlBlob, 0, kQuadControlBlobSize);
    *reinterpret_cast<int32_t*>(g_QuadVertexControlBlob + 4) = 1;
    *reinterpret_cast<int32_t*>(g_QuadVertexControlBlob + 8) = 5;
    *reinterpret_cast<int32_t*>(g_QuadVertexControlBlob + 0xc) = deviceIsaA;
    *reinterpret_cast<int32_t*>(g_QuadVertexControlBlob + 0x10) = deviceIsaB;
    *reinterpret_cast<int32_t*>(g_QuadVertexControlBlob + 0x714) = 0;

    __builtin_memset(g_QuadFragmentControlBlob, 0, kQuadControlBlobSize);
    *reinterpret_cast<int32_t*>(g_QuadFragmentControlBlob + 4) = 1;
    *reinterpret_cast<int32_t*>(g_QuadFragmentControlBlob + 8) = 0xe;
    *reinterpret_cast<int32_t*>(g_QuadFragmentControlBlob + 0xc) = deviceIsaA;
    *reinterpret_cast<int32_t*>(g_QuadFragmentControlBlob + 0x10) = deviceIsaB;
    *reinterpret_cast<int32_t*>(g_QuadFragmentControlBlob + 0x714) = 1;

    // data unchanged from the last independently-verified addresses (real
    // Maxwell SASS confirmed via recognizable repeating instruction
    // patterns, back at the very start of this whole investigation). Back to
    // both stages now that the count=1 diagnostic did its job (proved the
    // failure lives in the single-entry parse path, not cross-entry
    // consistency - which led straight to the ISA-version fix above).
    NVNshaderData stages[2] = {
        { shaderGpuBase + QuadShader::kVertexDataOffset, reinterpret_cast<uint64_t>(g_QuadVertexControlBlob) },
        { shaderGpuBase + QuadShader::kFragmentDataOffset, reinterpret_cast<uint64_t>(g_QuadFragmentControlBlob) },
    };

    auto programInitialize = NvnFn<FnProgramInitialize>(Offset::kProgramInitialize);
    auto programSetShaders = NvnFn<FnProgramSetShaders>(Offset::kProgramSetShaders);
    uint8_t initResult = programInitialize(&g_QuadProgram, device);
    WIIXL_LOG("NvnOverlay: quad step C2 (ProgramInitialize), result=%d", initResult);
    uint8_t setShadersResult = programSetShaders(&g_QuadProgram, 2, stages);
    WIIXL_LOG("NvnOverlay: quad step D (SetShaders), result=%d", setShadersResult);
    if (!setShadersResult) return;
    g_QuadNvnProgram = &g_QuadProgram;

    // Ordinary CPU-writable data pool for vertex/uniform data, flag 0x22.
    NVNmemoryPoolBuilder dataPoolBuilder{};
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&dataPoolBuilder);
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&dataPoolBuilder, device);
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&dataPoolBuilder, 0x22);
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&dataPoolBuilder, g_QuadDataPoolMemory, sizeof(g_QuadDataPoolMemory));
    int dataPoolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(&g_QuadDataMemoryPool, &dataPoolBuilder);
    WIIXL_LOG("NvnOverlay: quad step B2 (data PoolInitialize), result=%d", dataPoolResult);
    if (!dataPoolResult) return;

    NVNbufferBuilder dataBufBuilder{};
    NvnFn<FnBufferBuilderSetDevice>(Offset::kBufferBuilderSetDevice)(&dataBufBuilder, device);
    NvnFn<FnBufferBuilderSetDefaults>(Offset::kBufferBuilderSetDefaults)(&dataBufBuilder);
    NvnFn<FnBufferBuilderSetStorage>(Offset::kBufferBuilderSetStorage)(&dataBufBuilder, &g_QuadDataMemoryPool, 0, sizeof(g_QuadDataPoolMemory));
    uint8_t dataBufResult = NvnFn<FnBufferInitialize>(Offset::kBufferInitialize)(&g_QuadDataBuffer, &dataBufBuilder);
    WIIXL_LOG("NvnOverlay: quad step C2 (data BufferInitialize), result=%d", dataBufResult);
    if (!dataBufResult) return;

    // Vertex format: one attribute, vec4 float position, tightly packed.
    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_QuadVertexAttribState);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_QuadVertexAttribState, NvnFormat::kR32G32B32A32Float, 0);
    NvnFn<FnVertexStreamStateSetDefaults>(Offset::kVertexStreamStateSetDefaults)(&g_QuadVertexStreamState);
    NvnFn<FnVertexStreamStateSetStride>(Offset::kVertexStreamStateSetStride)(&g_QuadVertexStreamState, sizeof(float) * 4);
    WIIXL_LOG("NvnOverlay: quad step F (vertex state)");

    g_QuadInitialized = true;
    WIIXL_LOG("NvnOverlay: EnsureQuadInitialized OK");
}

// Draws one screen-space quad as an always-on overlay, using our own
// compiled shader and our own vertex data - no game helper classes. Called
// from the present_ hook, after Orig(), same reasoning as DrawTextOverlay
// (independent second command buffer submitted to the same queue).
//
// UNTESTED. Clip-space quad coordinates below are a first guess (centered,
// roughly a quarter of the screen) - since the vertex shader does no
// transform at all (gl_Position = a_Position directly), these ARE final
// clip-space coordinates, no camera/projection involved.
inline void DrawQuad(void* gameFramework, float r, float g, float b, float a) {
    EnsureInitialized();
    if (!g_Initialized) return;
    EnsureQuadInitialized(gameFramework);
    if (!g_QuadInitialized) return;

    void* queue = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kQueueOffset);
    if (!queue) return;

    // Write vertex + uniform data fresh each call, via the mapped CPU
    // pointer (matching print()'s established pattern) rather than writing
    // g_QuadDataPoolMemory directly, in case the pool's CPU/GPU views aren't
    // trivially coherent without going through the official Map API.
    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_QuadDataBuffer);
    {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: quad mapped=%p", mapped); }
    }
    if (!mapped) return;

    float* vertexDst = reinterpret_cast<float*>(static_cast<uint8_t*>(mapped) + kQuadVertexOffset);
    // Triangle strip, clip-space: bottom-left, bottom-right, top-left, top-right.
    const float verts[4][4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
    };
    __builtin_memcpy(vertexDst, verts, sizeof(verts));

    float* uniformDst = reinterpret_cast<float*>(static_cast<uint8_t*>(mapped) + kQuadUniformOffset);
    uniformDst[0] = r; uniformDst[1] = g; uniformDst[2] = b; uniformDst[3] = a;

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_QuadDataBuffer);
    uint64_t vertexGpuAddr = gpuBase + kQuadVertexOffset;
    uint64_t uniformGpuAddr = gpuBase + kQuadUniformOffset;

    auto beginRecording = NvnFn<FnCommandBufferBeginRecording>(Offset::kCommandBufferBeginRecording);
    auto endRecording   = NvnFn<FnCommandBufferEndRecording>(Offset::kCommandBufferEndRecording);
    auto submitCommands = NvnFn<FnQueueSubmitCommands>(Offset::kQueueSubmitCommands);
    auto bindProgram          = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto bindUniformBuffer     = NvnFn<FnCommandBufferBindUniformBuffer>(Offset::kCommandBufferBindUniformBuffer);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto drawArrays             = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    // Stage mask bits: 1=vertex, 2=fragment (real NVN convention) - bind both.
    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangleStrip = 5; // matches OpenGL's GL_TRIANGLE_STRIP enum value, which NVN's NVNdrawPrimitive mirrors

    beginRecording(&g_CommandBuffer);
    setViewport(&g_CommandBuffer, 0, 0, 1280, 720);
    setScissor(&g_CommandBuffer, 0, 0, 1280, 720);
    bindProgram(&g_CommandBuffer, reinterpret_cast<const NVNprogram*>(g_QuadNvnProgram), kStageMaskVertexFragment);
    bindVertexAttribState(&g_CommandBuffer, 1, &g_QuadVertexAttribState);
    bindVertexStreamState(&g_CommandBuffer, 1, &g_QuadVertexStreamState);
    bindVertexBuffer(&g_CommandBuffer, 0, vertexGpuAddr, sizeof(verts));
    bindUniformBuffer(&g_CommandBuffer, 1, 0, uniformGpuAddr, 16);
    drawArrays(&g_CommandBuffer, kPrimitiveTriangleStrip, 0, 4);
    NVNcommandHandle handle = endRecording(&g_CommandBuffer);

    submitCommands(queue, 1, &handle);

    {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: quad submitted, handle=%p", reinterpret_cast<void*>(handle)); }
    }
}

// -----------------------------------------------------------------------
// Real-archive pipeline-validation test (see g_TextureShaderArchive's
// comment above). Separate, self-contained storage/state from the
// hand-built quad path above - doesn't touch g_QuadProgram/
// g_QuadVertexControlBlob/etc, so the hand-built path is still there to
// come back to if this doesn't pan out.
// -----------------------------------------------------------------------
namespace {
    constexpr size_t kTextureShaderDataPoolSize = 4096;
    alignas(4096) uint8_t g_TextureShaderDataPoolMemory[kTextureShaderDataPoolSize];
    NVNmemoryPool g_TextureShaderDataMemoryPool;
    NVNbuffer     g_TextureShaderDataBuffer;
    NVNvertexAttribState  g_TextureShaderVertexAttribState;
    NVNvertexStreamState  g_TextureShaderVertexStreamState;
    constexpr ptrdiff_t kTextureShaderVertexOffset = 0;
    constexpr ptrdiff_t kTextureShaderUniformOffset = 256;
}

// grsc sub-block always starts at a fixed offset within a real BNSH file
// (confirmed against both our own ShaderConverter.exe output AND this real
// shipped file - see findings doc "grsc-base breakthrough").
constexpr ptrdiff_t kGrscOffset = 0x60;

inline void EnsureTextureShaderInitialized(void* gameFramework) {
    if (g_TextureShaderInitialized) return;

    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;
    void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + kGraphicsNvnDeviceOffset);
    if (!device) return;

    // Not-a-sead-thread heap fix (see findings doc "three more real bugs",
    // #3): sead::HeapMgr::getCurrentHeap() falls back to a TLS slot that's
    // never set up for our injected Presentation Thread hook, so even a
    // heap passed explicitly to the allocator still fails deep inside
    // alloc() unless this fallback field is set first.
    void* heapMgrInstance = *reinterpret_cast<void**>(WiiXLaunch::ResolveTarget(Offset::kHeapMgrInstancePtr));
    auto findContainHeap = WiiXLaunch::GetTargetFunction<FnHeapMgrFindContainHeap>(Offset::kHeapMgrFindContainHeap, 0);
    void* heap = findContainHeap(heapMgrInstance, graphicsNvn);
    WIIXL_LOG("NvnOverlay: texshader step A (heap), heapMgr=%p heap=%p", heapMgrInstance, heap);
    if (!heap) return;
    auto setAllocFromNotSeadThreadHeap = WiiXLaunch::GetTargetFunction<FnHeapMgrSetAllocFromNotSeadThreadHeap>(Offset::kHeapMgrSetAllocFromNotSeadThreadHeap, 0);
    setAllocFromNotSeadThreadHeap(heapMgrInstance, heap);
    WIIXL_LOG("NvnOverlay: texshader step B (setAllocFromNotSeadThreadHeap)");

    // Fresh writable copy - the loader's endian-fixup pass writes in place,
    // and unlike our own broken ShaderConverter.exe CLI output, this real
    // file's grsc+0x18 source-table offset is already sane (verified by
    // direct inspection). The grsc+0xc endian-fixup flag, however, DOES
    // need the same patch our own file needed, despite looking clear
    // (meaning "needs swap") in the raw bytes: decompiled
    // agl::ResShaderArchive::setUp() directly against a live crash here and
    // confirmed empirically, not assumed - its very first ModifyEndianU32
    // call swaps grsc's own +0x10 "table offset" field before anything
    // reads it, and this file's raw (pre-swap) value there is 4 - already
    // the correct, sane, native-order value. Byte-swapping an already-
    // correct 4 (0x00000004 -> 0x04000000 = 67108864) is exactly what
    // crashed the loader: the next line computes grscBase + thatOffset and
    // dereferences it, landing 64MB past our 12672-byte buffer. So this
    // real file's on-disk bytes are ALSO already native-order (like our own
    // freshly-compiled file), contradicting the earlier notes' "every real
    // archive load byte-swaps as a matter of course" (drawn from a
    // different, evidently differently-authored reference archive) - set
    // the same bit to skip the fixup pass entirely, exactly as our own file
    // needed.
    __builtin_memcpy(g_TextureShaderFileBuffer, TextureShaderFile::kBytes, TextureShaderFile::kSize);
    uint8_t* grscBase = g_TextureShaderFileBuffer + kGrscOffset;
    grscBase[0xc] |= 1;

    auto archiveCtor = WiiXLaunch::GetTargetFunction<FnShaderProgramArchiveCtor>(Offset::kShaderProgramArchiveCtor, 0);
    archiveCtor(&g_TextureShaderArchive);
    WIIXL_LOG("NvnOverlay: texshader step C (archive ctor)");

    auto loadFromBinary = WiiXLaunch::GetTargetFunction<FnShaderProgramArchiveLoadFromBinary>(Offset::kShaderProgramArchiveLoadFromBinary, 0);
    loadFromBinary(&g_TextureShaderArchive, grscBase, heap);
    WIIXL_LOG("NvnOverlay: texshader step D (loadFromBinary returned)");

    // programCount = *(int32_t*)(grscBase + *(uint32_t*)(grscBase+0x10) + 0x18)
    // - the real formula FUN_7100b6de44 itself uses (see findings doc); read
    // it back ourselves too as a live sanity check rather than trusting the
    // offline Python computation blindly.
    uint32_t nameOff = *reinterpret_cast<uint32_t*>(grscBase + 0x10);
    int32_t programCount = *reinterpret_cast<int32_t*>(grscBase + nameOff + 0x18);
    WIIXL_LOG("NvnOverlay: texshader step E (programCount), count=%d", programCount);
    if (programCount < 1) return;

    // ShaderProgramArchive+0x40 is a pointer to the separately heap-allocated
    // agl::ShaderProgram array - program index 0 is just that pointer itself.
    void* programArrayBase = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(&g_TextureShaderArchive) + kShaderProgramArrayPtrOffset);
    WIIXL_LOG("NvnOverlay: texshader step F (programArrayBase), ptr=%p", programArrayBase);
    if (!programArrayBase) return;
    g_TextureShaderProgram0 = programArrayBase;

    // Lets the real, proven-correct engine code build NVNshaderData/control
    // from this program's (real, complete) reflection data and call
    // nvnProgramSetShaders itself - the entire point of this test.
    auto setUpAllVariation = WiiXLaunch::GetTargetFunction<FnShaderProgramSetUpAllVariation>(Offset::kShaderProgramSetUpAllVariation, 0);
    setUpAllVariation(g_TextureShaderProgram0, false);
    WIIXL_LOG("NvnOverlay: texshader step G (setUpAllVariation returned)");

    g_TextureShaderNvnProgram = reinterpret_cast<uint8_t*>(g_TextureShaderProgram0) + kShaderProgramNvnProgramOffset;

    // Minimal vertex/uniform data pool, same shape as the quad path's, so we
    // have *something* to bind and draw with (this shader almost certainly
    // wants a texture we don't have - the point here isn't a correct visual
    // result, it's whether the pipeline survives a real, correctly-shaped
    // program through bind+draw without the resource-exhaustion cascade the
    // hand-built empty-metadata blob triggered).
    NVNmemoryPoolBuilder dataPoolBuilder{};
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&dataPoolBuilder);
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&dataPoolBuilder, device);
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&dataPoolBuilder, 0x22);
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&dataPoolBuilder, g_TextureShaderDataPoolMemory, sizeof(g_TextureShaderDataPoolMemory));
    int dataPoolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(&g_TextureShaderDataMemoryPool, &dataPoolBuilder);
    WIIXL_LOG("NvnOverlay: texshader step H (data PoolInitialize), result=%d", dataPoolResult);
    if (!dataPoolResult) return;

    NVNbufferBuilder dataBufBuilder{};
    NvnFn<FnBufferBuilderSetDevice>(Offset::kBufferBuilderSetDevice)(&dataBufBuilder, device);
    NvnFn<FnBufferBuilderSetDefaults>(Offset::kBufferBuilderSetDefaults)(&dataBufBuilder);
    NvnFn<FnBufferBuilderSetStorage>(Offset::kBufferBuilderSetStorage)(&dataBufBuilder, &g_TextureShaderDataMemoryPool, 0, sizeof(g_TextureShaderDataPoolMemory));
    uint8_t dataBufResult = NvnFn<FnBufferInitialize>(Offset::kBufferInitialize)(&g_TextureShaderDataBuffer, &dataBufBuilder);
    WIIXL_LOG("NvnOverlay: texshader step I (data BufferInitialize), result=%d", dataBufResult);
    if (!dataBufResult) return;

    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_TextureShaderVertexAttribState);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_TextureShaderVertexAttribState, NvnFormat::kR32G32B32A32Float, 0);
    NvnFn<FnVertexStreamStateSetDefaults>(Offset::kVertexStreamStateSetDefaults)(&g_TextureShaderVertexStreamState);
    NvnFn<FnVertexStreamStateSetStride>(Offset::kVertexStreamStateSetStride)(&g_TextureShaderVertexStreamState, sizeof(float) * 4);

    g_TextureShaderInitialized = true;
    WIIXL_LOG("NvnOverlay: EnsureTextureShaderInitialized OK, nvnProgram=%p", g_TextureShaderNvnProgram);
}

inline void DrawTextureShaderTest(void* gameFramework, float r, float g, float b, float a) {
    EnsureInitialized();
    if (!g_Initialized) return;
    EnsureTextureShaderInitialized(gameFramework);
    if (!g_TextureShaderInitialized) return;

    void* queue = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kQueueOffset);
    if (!queue) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_TextureShaderDataBuffer);
    if (!mapped) return;

    float* vertexDst = reinterpret_cast<float*>(static_cast<uint8_t*>(mapped) + kTextureShaderVertexOffset);
    const float verts[4][4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
    };
    __builtin_memcpy(vertexDst, verts, sizeof(verts));

    float* uniformDst = reinterpret_cast<float*>(static_cast<uint8_t*>(mapped) + kTextureShaderUniformOffset);
    uniformDst[0] = r; uniformDst[1] = g; uniformDst[2] = b; uniformDst[3] = a;

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_TextureShaderDataBuffer);
    uint64_t vertexGpuAddr = gpuBase + kTextureShaderVertexOffset;
    uint64_t uniformGpuAddr = gpuBase + kTextureShaderUniformOffset;

    auto beginRecording = NvnFn<FnCommandBufferBeginRecording>(Offset::kCommandBufferBeginRecording);
    auto endRecording   = NvnFn<FnCommandBufferEndRecording>(Offset::kCommandBufferEndRecording);
    auto submitCommands = NvnFn<FnQueueSubmitCommands>(Offset::kQueueSubmitCommands);
    auto bindProgram          = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto bindUniformBuffer     = NvnFn<FnCommandBufferBindUniformBuffer>(Offset::kCommandBufferBindUniformBuffer);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto drawArrays             = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangleStrip = 5;

    beginRecording(&g_CommandBuffer);
    setViewport(&g_CommandBuffer, 0, 0, 1280, 720);
    setScissor(&g_CommandBuffer, 0, 0, 1280, 720);
    bindProgram(&g_CommandBuffer, reinterpret_cast<const NVNprogram*>(g_TextureShaderNvnProgram), kStageMaskVertexFragment);
    bindVertexAttribState(&g_CommandBuffer, 1, &g_TextureShaderVertexAttribState);
    bindVertexStreamState(&g_CommandBuffer, 1, &g_TextureShaderVertexStreamState);
    bindVertexBuffer(&g_CommandBuffer, 0, vertexGpuAddr, sizeof(verts));
    bindUniformBuffer(&g_CommandBuffer, 1, 0, uniformGpuAddr, 16);
    drawArrays(&g_CommandBuffer, kPrimitiveTriangleStrip, 0, 4);
    NVNcommandHandle handle = endRecording(&g_CommandBuffer);

    submitCommands(queue, 1, &handle);

    {
        static bool loggedOnce = false;
        if (!loggedOnce) { loggedOnce = true; WIIXL_LOG("NvnOverlay: texshader submitted, handle=%p", reinterpret_cast<void*>(handle)); }
    }
}

// -----------------------------------------------------------------------
// Bind-program hook: instead of constructing a shader program ourselves
// (hand-built control blob, real-archive loader, or hand-built
// DebugFontMgrNvn - all tried this session, each hitting real, different
// walls), snoop a genuinely valid NVNprogram* the game itself is already
// binding successfully every frame. nvnCommandBufferBindProgram is reached
// through a resolved function-pointer table cell (same double-indirect
// slot NvnFn/ReadIndirect already read from for every other nvn* call in
// this file) - overwriting the pointer STORED at that cell with our own
// function's address is a standard, low-risk hook technique (a data-
// pointer swap in a location that's already meant to be called
// indirectly), a very different risk profile from hand-constructing a real
// engine object's internal vtable/field layout (tried and abandoned this
// session after it produced a "Pure virtual function called!" hang).
// -----------------------------------------------------------------------
namespace {
    void* g_CapturedNvnProgram = nullptr;
    void* g_OriginalBindProgram = nullptr;
    bool g_BindProgramHookInstalled = false;
}

using FnCommandBufferBindProgramRaw = void (*)(NVNcommandBuffer*, const NVNprogram*, uint32_t);

inline void HookedBindProgram(NVNcommandBuffer* cmdBuf, const NVNprogram* program, uint32_t stageMask) {
    // Freezing on the FIRST real program ever seen turned out to be a bad
    // target: that's whatever the engine happens to draw first the moment
    // gameplay starts rendering (in testing, some real, complex, resource-
    // heavy world/character shader) - arbitrary, and (see
    // kControlReserveSize's comment) apparently needs an UNBOUNDED amount
    // of control memory to bind+draw without its real expected textures/
    // uniform data, which we never provide. 256KB, 8MB, even 128MB all
    // exhausted identically - proof this was never a sizing problem.
    //
    // Continuously updating instead (no freeze) means that by the time
    // DrawWithCapturedProgram reads g_CapturedNvnProgram - right after
    // Orig() returns from a full real frame, in PresentHook - it naturally
    // holds whatever the game bound MOST RECENTLY that frame. Virtually
    // every game (BotW included) renders 3D world geometry first and 2D
    // UI/HUD overlays last, every single frame, in a fixed order - so this
    // should consistently land on a simple UI-style shader (few or no
    // textures, modest uniform data) instead of an arbitrary complex one,
    // and do so the same way every run, since the draw order itself is
    // deterministic.
    if (program) {
        g_CapturedNvnProgram = const_cast<NVNprogram*>(program);
    }
    reinterpret_cast<FnCommandBufferBindProgramRaw>(g_OriginalBindProgram)(cmdBuf, program, stageMask);
}

inline void InstallBindProgramHookIfNeeded() {
    if (g_BindProgramHookInstalled) return;
    g_BindProgramHookInstalled = true;

    // Same double-indirect shape as NvnFn/ReadIndirect: ResolveTarget(offset)
    // is the table SLOT address; the cell it holds is where the REAL
    // function pointer actually lives (one more dereference). We save that
    // real pointer, then overwrite the cell itself with our own function's
    // address so every future call through this slot (by us or by the
    // game) routes through HookedBindProgram first.
    uintptr_t tableSlotAddr = WiiXLaunch::ResolveTarget(Offset::kCommandBufferBindProgram);
    uintptr_t cell = *reinterpret_cast<uintptr_t*>(tableSlotAddr);
    g_OriginalBindProgram = *reinterpret_cast<void**>(cell);
    *reinterpret_cast<void**>(cell) = reinterpret_cast<void*>(&HookedBindProgram);
    WIIXL_LOG("NvnOverlay: bind-program hook installed, original=%p", g_OriginalBindProgram);
}

// -----------------------------------------------------------------------
// GPU/CPU sync: real root cause of the control-memory reserve exhausting
// identically no matter the total size (256KB through 128MB) or which real
// program was bound (proven by the captured pointer's low bits - be9128 -
// being byte-identical across separate process launches with different
// ASLR bases, meaning "switch which program we capture" changed nothing).
//
// We record into and submit the SAME g_CommandBuffer every single frame,
// forever, with no synchronization at all - nothing ever tells the driver
// the PREVIOUS frame's submission has actually finished executing on the
// GPU. nvn.h has no "reset command buffer" API (checked directly) - the
// real model is that a command buffer's already-added memory becomes safe
// to reuse once its prior submission's fence has signaled, via
// nvnQueueFenceSync + nvnSyncWait (or, for better real-world performance,
// round-robin between multiple command buffers so one is never being
// re-recorded while its predecessor is still in flight - not needed here,
// this is a debug overlay, not a performance-sensitive renderer). Without
// that, the driver can never be sure it's safe to reclaim anything, so it
// just keeps growing its internal bookkeeping forever, independent of
// shader complexity - exactly matching every symptom observed.




namespace {
    struct RainbowVertex {
        float x, y, z, w;
        float r, g, b, a;
    };
    constexpr size_t kCapturedDrawDataPoolSize = 4096;
    alignas(4096) uint8_t g_CapturedDrawDataPoolMemory[kCapturedDrawDataPoolSize];
    NVNmemoryPool g_CapturedDrawDataMemoryPool;
    NVNbuffer     g_CapturedDrawDataBuffer;
    NVNvertexAttribState g_CapturedDrawVertexAttribState;
    NVNvertexStreamState g_CapturedDrawVertexStreamState;
    NVNvertexAttribState g_RainbowVertexAttribStates[2];
    NVNvertexStreamState g_RainbowVertexStreamState;
    NVNblendState        g_RainbowBlendState;
    NVNdepthStencilState g_RainbowDepthStencilState;
    NVNpolygonState      g_RainbowPolygonState;
    NVNcolorState        g_RainbowColorState;
    NVNchannelMaskState  g_RainbowChannelMaskState;
    bool g_CapturedDrawInitialized = false;
}

inline void EnsureCapturedDrawInitialized(void* gameFramework) {
    if (g_CapturedDrawInitialized) return;
    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;
    void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + kGraphicsNvnDeviceOffset);
    if (!device) return;

    NVNmemoryPoolBuilder poolBuilder;
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&poolBuilder);
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&poolBuilder, device);
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&poolBuilder, 0x22);
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&poolBuilder, g_CapturedDrawDataPoolMemory, sizeof(g_CapturedDrawDataPoolMemory));

    uint8_t poolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(&g_CapturedDrawDataMemoryPool, &poolBuilder);
    WIIXL_LOG("NvnOverlay: captured-draw step A (data PoolInitialize), result=%d", poolResult);
    if (!poolResult) return;

    NVNbufferBuilder bufBuilder;
    NvnFn<FnBufferBuilderSetDefaults>(Offset::kBufferBuilderSetDefaults)(&bufBuilder);
    NvnFn<FnBufferBuilderSetDevice>(Offset::kBufferBuilderSetDevice)(&bufBuilder, device);
    NvnFn<FnBufferBuilderSetStorage>(Offset::kBufferBuilderSetStorage)(&bufBuilder, &g_CapturedDrawDataMemoryPool, 0, sizeof(g_CapturedDrawDataPoolMemory));

    uint8_t bufResult = NvnFn<FnBufferInitialize>(Offset::kBufferInitialize)(&g_CapturedDrawDataBuffer, &bufBuilder);
    WIIXL_LOG("NvnOverlay: captured-draw step B (data BufferInitialize), result=%d", bufResult);
    if (!bufResult) return;

    // Single-attrib setup for captured draw
    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_CapturedDrawVertexAttribState);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_CapturedDrawVertexAttribState, NvnFormat::kR32G32B32A32Float, 0);
    NvnFn<FnVertexStreamStateSetDefaults>(Offset::kVertexStreamStateSetDefaults)(&g_CapturedDrawVertexStreamState);
    NvnFn<FnVertexStreamStateSetStride>(Offset::kVertexStreamStateSetStride)(&g_CapturedDrawVertexStreamState, sizeof(float) * 4);

    // Two-attrib setup for rainbow quad (Position + Color)
    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_RainbowVertexAttribStates[0]);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_RainbowVertexAttribStates[0], NvnFormat::kR32G32B32A32Float, 0);
    NvnFn<FnVertexAttribStateSetStreamIndex>(Offset::kVertexAttribStateSetStreamIndex)(&g_RainbowVertexAttribStates[0], 0);

    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_RainbowVertexAttribStates[1]);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_RainbowVertexAttribStates[1], NvnFormat::kR32G32B32A32Float, 16);
    NvnFn<FnVertexAttribStateSetStreamIndex>(Offset::kVertexAttribStateSetStreamIndex)(&g_RainbowVertexAttribStates[1], 0);

    NvnFn<FnVertexStreamStateSetDefaults>(Offset::kVertexStreamStateSetDefaults)(&g_RainbowVertexStreamState);
    NvnFn<FnVertexStreamStateSetStride>(Offset::kVertexStreamStateSetStride)(&g_RainbowVertexStreamState, sizeof(RainbowVertex));

    // Pipeline State Objects (disable depth test/write, disable cull, overwrite color, full RGBA write mask)
    auto blendSetDefaults = NvnFn<FnBlendStateSetDefaults>(Offset::kBlendStateSetDefaults);
    if (blendSetDefaults) blendSetDefaults(&g_RainbowBlendState);

    auto dsSetDefaults    = NvnFn<FnDepthStencilStateSetDefaults>(Offset::kDepthStencilStateSetDefaults);
    auto dsSetDepthTest   = NvnFn<FnDepthStencilStateSetDepthTestEnable>(Offset::kDepthStencilStateSetDepthTestEnable);
    auto dsSetDepthWrite  = NvnFn<FnDepthStencilStateSetDepthWriteEnable>(Offset::kDepthStencilStateSetDepthWriteEnable);
    if (dsSetDefaults) dsSetDefaults(&g_RainbowDepthStencilState);
    if (dsSetDepthTest) dsSetDepthTest(&g_RainbowDepthStencilState, 0);
    if (dsSetDepthWrite) dsSetDepthWrite(&g_RainbowDepthStencilState, 0);

    auto polySetDefaults  = NvnFn<FnPolygonStateSetDefaults>(Offset::kPolygonStateSetDefaults);
    auto polySetCull      = NvnFn<FnPolygonStateSetCullFace>(Offset::kPolygonStateSetCullFace);
    if (polySetDefaults) polySetDefaults(&g_RainbowPolygonState);
    if (polySetCull) polySetCull(&g_RainbowPolygonState, 0);

    auto colSetDefaults   = NvnFn<FnColorStateSetDefaults>(Offset::kColorStateSetDefaults);
    auto colSetBlend      = NvnFn<FnColorStateSetBlendEnable>(Offset::kColorStateSetBlendEnable);
    if (colSetDefaults) colSetDefaults(&g_RainbowColorState);
    // Only disable blend — do NOT call SetLogicOp or SetAlphaTest.
    // Calling SetLogicOp enables logic-op mode which can clobber fragment output.
    // Calling SetAlphaTest may enable alpha-test with an undefined reference value.
    // Defaults already give us normal (copy) color output.
    if (colSetBlend) colSetBlend(&g_RainbowColorState, 0, 0);

    auto chanSetDefaults  = NvnFn<FnChannelMaskStateSetDefaults>(Offset::kChannelMaskStateSetDefaults);
    auto chanSetMask      = NvnFn<FnChannelMaskStateSetChannelMask>(Offset::kChannelMaskStateSetChannelMask);
    if (chanSetDefaults) chanSetDefaults(&g_RainbowChannelMaskState);
    if (chanSetMask) chanSetMask(&g_RainbowChannelMaskState, 0, 1, 1, 1, 1);

    g_CapturedDrawInitialized = true;
    WIIXL_LOG("NvnOverlay: EnsureCapturedDrawInitialized OK");
}

// Loads a sead-binary-format compiled shader (see pack_shader.py / rainbow's
// GetBnshProgram history in docs/switch-nvn-findings.md) into caller-owned
// static storage and returns the ready-to-bind NVNprogram*, or nullptr on
// failure. Factored out of the original rainbow-only GetBnshProgram once a
// second shader (plasma) needed the exact same load sequence - each caller
// still needs its own dedicated static storage (code pool memory/NVNmemoryPool/
// NVNbuffer/NVNprogram) since these are live GPU resources, not something that
// can be shared/overwritten between two simultaneously-loaded programs.
inline NVNprogram* LoadSeadBinaryProgram(
        void* graphicsNvn, const uint8_t* seadBytes, size_t seadSize,
        uint8_t* codePoolMemory, size_t codePoolMemorySize,
        NVNmemoryPool* codeMemoryPool, NVNbuffer* codeBuffer,
        NVNprogram* programStorage, const char* label) {
    if (!graphicsNvn) return nullptr;
    void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + kGraphicsNvnDeviceOffset);
    if (!device) return nullptr;

    if (seadSize > codePoolMemorySize) {
        WIIXL_LOG("NvnOverlay: %s sead binary (%u bytes) exceeds pool memory (%u bytes)",
            label, static_cast<unsigned int>(seadSize), static_cast<unsigned int>(codePoolMemorySize));
        return nullptr;
    }

    // 1. Copy sead-formatted binary into code pool memory FIRST
    __builtin_memcpy(codePoolMemory, seadBytes, seadSize);

    // 2. Initialize Code Memory Pool (flag 0x62)
    NVNmemoryPoolBuilder codePoolBuilder{};
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&codePoolBuilder);
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&codePoolBuilder, device);
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&codePoolBuilder, 0x62);
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&codePoolBuilder, codePoolMemory, codePoolMemorySize);
    int poolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(codeMemoryPool, &codePoolBuilder);
    WIIXL_LOG("NvnOverlay: %s CodePoolInitialize result=%d", label, poolResult);
    if (!poolResult) return nullptr;

    // 3. Initialize Buffer on the Code Memory Pool
    NVNbufferBuilder codeBufBuilder{};
    NvnFn<FnBufferBuilderSetDevice>(Offset::kBufferBuilderSetDevice)(&codeBufBuilder, device);
    NvnFn<FnBufferBuilderSetDefaults>(Offset::kBufferBuilderSetDefaults)(&codeBufBuilder);
    NvnFn<FnBufferBuilderSetStorage>(Offset::kBufferBuilderSetStorage)(&codeBufBuilder, codeMemoryPool, 0, codePoolMemorySize);
    uint8_t bufResult = NvnFn<FnBufferInitialize>(Offset::kBufferInitialize)(codeBuffer, &codeBufBuilder);
    WIIXL_LOG("NvnOverlay: %s CodeBufferInitialize result=%d", label, bufResult);
    if (!bufResult) return nullptr;

    uint64_t codeGpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(codeBuffer);
    WIIXL_LOG("NvnOverlay: %s Code GPU Base=%p", label, reinterpret_cast<void*>(codeGpuBase));

    // 4. Match device ISA version fields in the control headers inside the pool
    int32_t deviceIsaA = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x2c);
    int32_t deviceIsaB = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x30);
    WIIXL_LOG("NvnOverlay: Device ISA version A=%d, B=%d", deviceIsaA, deviceIsaB);

    uint32_t* hdr = reinterpret_cast<uint32_t*>(codePoolMemory);
    uint32_t offVertCtrl = hdr[0];
    uint32_t offFragCtrl = hdr[1];
    uint32_t offVertCode = hdr[2];
    uint32_t offFragCode = hdr[3];

    uint8_t* vertControl = codePoolMemory + offVertCtrl;
    uint8_t* fragControl = codePoolMemory + offFragCtrl;

    uint32_t* vWords = reinterpret_cast<uint32_t*>(vertControl);
    uint32_t* fWords = reinterpret_cast<uint32_t*>(fragControl);

    vWords[2] = 0x0e; // Match BotW NVN 4.4.0 driver control header version
    fWords[2] = 0x0e;

    vWords[3] = deviceIsaA;
    vWords[4] = deviceIsaB;
    fWords[3] = deviceIsaA;
    fWords[4] = deviceIsaB;

    WIIXL_LOG("NvnOverlay: %s vert control header: 0x%x 0x%x 0x%x 0x%x 0x%x",
        label, vWords[0], vWords[1], vWords[2], vWords[3], vWords[4]);
    WIIXL_LOG("NvnOverlay: %s frag control header: 0x%x 0x%x 0x%x 0x%x 0x%x",
        label, fWords[0], fWords[1], fWords[2], fWords[3], fWords[4]);

    NVNshaderData stages[2] = {
        { codeGpuBase + offVertCode, reinterpret_cast<uint64_t>(vertControl) },
        { codeGpuBase + offFragCode, reinterpret_cast<uint64_t>(fragControl) },
    };

    auto programInitialize = NvnFn<FnProgramInitialize>(Offset::kProgramInitialize);
    auto programSetShaders = NvnFn<FnProgramSetShaders>(Offset::kProgramSetShaders);

    uint8_t initResult = programInitialize(programStorage, device);
    WIIXL_LOG("NvnOverlay: %s ProgramInitialize result=%d", label, initResult);

    uint8_t setShadersResult = programSetShaders(programStorage, 2, stages);
    WIIXL_LOG("NvnOverlay: %s ProgramSetShaders result=%d", label, setShadersResult);
    if (setShadersResult) {
        WIIXL_LOG("NvnOverlay: %s NVNprogram ready at %p!", label, programStorage);
        return programStorage;
    }

    return nullptr;
}

namespace {
    alignas(4096) uint8_t g_BnshCodePoolMemory[65536];
    NVNmemoryPool g_BnshCodeMemoryPool;
    NVNbuffer     g_BnshCodeBuffer;
    NVNprogram    g_BnshProgramStorage{};
    NVNprogram*   g_LoadedBnshProgram = nullptr;
    bool          g_BnshProgramLoaded = false;

    alignas(4096) uint8_t g_PlasmaCodePoolMemory[65536];
    NVNmemoryPool g_PlasmaCodeMemoryPool;
    NVNbuffer     g_PlasmaCodeBuffer;
    NVNprogram    g_PlasmaProgramStorage{};
    NVNprogram*   g_LoadedPlasmaProgram = nullptr;
    bool          g_PlasmaProgramLoaded = false;

    // Diagnostic only (see docs/switch-nvn-findings.md, "plasma bind failure"
    // investigation) - pack_shader.py's own repack of the SAME rainbow.vert/
    // rainbow.frag source that g_RainbowSeadBin was hand-built from. Never
    // drawn; loading it (and only it, alongside the known-good hand-built
    // rainbow and the failing plasma) isolates whether pack_shader.py's
    // overall file layout is sound, independent of plasma's larger code.
    alignas(4096) uint8_t g_RainbowSelftestCodePoolMemory[65536];
    NVNmemoryPool g_RainbowSelftestCodeMemoryPool;
    NVNbuffer     g_RainbowSelftestCodeBuffer;
    NVNprogram    g_RainbowSelftestProgramStorage{};
    NVNprogram*   g_LoadedRainbowSelftestProgram = nullptr;
    bool          g_RainbowSelftestProgramLoaded = false;
}

inline NVNprogram* GetBnshProgram(void* graphicsNvn) {
    if (g_BnshProgramLoaded) return g_LoadedBnshProgram;
    g_BnshProgramLoaded = true;
    g_LoadedBnshProgram = LoadSeadBinaryProgram(
        graphicsNvn, g_RainbowSeadBin, kRainbowSeadBinSize,
        g_BnshCodePoolMemory, sizeof(g_BnshCodePoolMemory),
        &g_BnshCodeMemoryPool, &g_BnshCodeBuffer, &g_BnshProgramStorage, "Rainbow");
    return g_LoadedBnshProgram;
}

// Custom-compiled demo shader (shaders/plasma.vert + shaders/plasma.frag,
// packed via scripts/pack_shader.py) - an animated, screen-space plasma
// effect. Uses the exact same vertex plumbing already proven working for the
// rainbow quad (position + a repurposed vertex-color channel), so it needs
// zero new NVN binding code - just a different compiled program and a
// per-frame value written into the existing color attribute (see
// DrawPlasmaQuadDirect).
inline NVNprogram* GetPlasmaProgram(void* graphicsNvn) {
    if (g_PlasmaProgramLoaded) return g_LoadedPlasmaProgram;
    g_PlasmaProgramLoaded = true;
    g_LoadedPlasmaProgram = LoadSeadBinaryProgram(
        graphicsNvn, g_PlasmaSeadBin, kPlasmaSeadBinSize,
        g_PlasmaCodePoolMemory, sizeof(g_PlasmaCodePoolMemory),
        &g_PlasmaCodeMemoryPool, &g_PlasmaCodeBuffer, &g_PlasmaProgramStorage, "Plasma");
    return g_LoadedPlasmaProgram;
}

// Diagnostic only - see the g_RainbowSelftest* storage comment above.
inline NVNprogram* GetRainbowSelftestProgram(void* graphicsNvn) {
    if (g_RainbowSelftestProgramLoaded) return g_LoadedRainbowSelftestProgram;
    g_RainbowSelftestProgramLoaded = true;
    g_LoadedRainbowSelftestProgram = LoadSeadBinaryProgram(
        graphicsNvn, g_RainbowSelftestSeadBin, kRainbowSelftestSeadBinSize,
        g_RainbowSelftestCodePoolMemory, sizeof(g_RainbowSelftestCodePoolMemory),
        &g_RainbowSelftestCodeMemoryPool, &g_RainbowSelftestCodeBuffer,
        &g_RainbowSelftestProgramStorage, "RainbowSelftest");
    return g_LoadedRainbowSelftestProgram;
}

// -----------------------------------------------------------------------
// Texture pipeline: a real image sampled by a real texture, bound through a
// private texture/sampler pool (not the game's, at graphicsNvn+0x58/+0x78 -
// registered texture/sampler IDs must be >=256 per nvn.h, and a private pool
// sidesteps any question of which IDs the game's own live pool already
// uses). Image comes from scripts/pack_texture.py -> include/testpic_texture_bytes.hpp.
//
// First attempt used a LINEAR (non-swizzled) texture layout specifically to
// avoid implementing Tegra X1 block-linear/GOB swizzling from scratch -
// crashed inside nvnTextureInitialize every time. Second attempt switched to
// a normal (default, block-linear) texture populated via
// nvnCommandBufferCopyBufferToTexture from a CPU-writable staging buffer -
// STILL crashed, same PC, across 5 rounds of flag/format/target changes.
//
// Ghidra-decompiled the real internal implementation this time (nnSdk file
// offset 0x2e9468, "Texture pipeline, round 6" in the findings doc): BOTH of
// nvnTextureInitialize's internal code paths (block-linear vtable+0x10 AND
// the LINEAR-flag vtable+0x38 path found earlier) unconditionally
// dereference the supplied memory pool as a vtable-having C++ object -
// `(**(pool->vtable + N))(pool, texture)` - and crash reading address 0x0
// there. Nothing on the builder side (flags/format/target) can route around
// this; it's not a setup mistake, it's a hard requirement of both plain-pool
// code paths. The one real, working reference in this codebase
// (sead::DebugFontMgrNvn::initializeFromBinary, 0x7100aff42c) instead calls
// nvnTextureBuilderSetPackagedTextureData, which takes an entirely different
// internal path (NvRmGpuMappingCreate) that never touches that vtable at
// all - see scripts/pack_texture.py for the real container format this
// requires and how its fields were confirmed.
//
// Packaged texture data is used as-is with no runtime copy: the pool is
// built directly over the packed asset bytes (same pattern
// DebugFontMgrNvn's real code uses for its own font atlas), already
// block-linear swizzled at pack time - no staging buffer or per-draw
// nvnCommandBufferCopyBufferToTexture needed anymore.
// -----------------------------------------------------------------------
namespace {
    NVNmemoryPool g_TexturePixelMemoryPool;
    NVNtexture    g_TestPicTexture;

    alignas(4096) uint8_t g_TextureDescriptorPoolMemory[16384];
    NVNmemoryPool g_TextureDescriptorMemoryPool;
    NVNtexturePool g_TextureDescriptorPool;

    alignas(4096) uint8_t g_SamplerDescriptorPoolMemory[16384];
    NVNmemoryPool g_SamplerDescriptorMemoryPool;
    NVNsamplerPool g_SamplerDescriptorPool;
    NVNsampler    g_TestPicSampler;

    NVNtextureHandle g_TestPicTextureHandle = 0;
    bool g_TexturePipelineInitialized = false;

    // General-purpose "sprite/UI" vertex format: position + UV + a tint
    // color that also carries alpha - see shaders/texture.vert/.frag.
    struct TextureVertex {
        float x, y, z, w;
        float u, v;
        float r, g, b, a;
    };
    NVNvertexAttribState g_TextureQuadVertexAttribStates[3];
    NVNvertexStreamState g_TextureQuadVertexStreamState;

    constexpr size_t kTextureQuadDataPoolSize = 4096;
    alignas(4096) uint8_t g_TextureQuadDataPoolMemory[kTextureQuadDataPoolSize];
    NVNmemoryPool g_TextureQuadDataMemoryPool;
    NVNbuffer     g_TextureQuadDataBuffer;

    alignas(4096) uint8_t g_TextureQuadCodePoolMemory[65536];
    NVNmemoryPool g_TextureQuadCodeMemoryPool;
    NVNbuffer     g_TextureQuadCodeBuffer;
    NVNprogram    g_TextureQuadProgramStorage{};
    NVNprogram*   g_LoadedTextureQuadProgram = nullptr;
    bool          g_TextureQuadProgramLoaded = false;
}

inline void EnsureTexturePipelineInitialized(void* graphicsNvn) {
    if (g_TexturePipelineInitialized) return;
    if (!graphicsNvn) return;
    void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + kGraphicsNvnDeviceOffset);
    if (!device) return;

    // Resolve all NVN functions by name through nnSdk's bootstrap table.
    auto poolBuilderSetDefaults = ResolveNvnFn<FnMemoryPoolBuilderSetDefaults>("nvnMemoryPoolBuilderSetDefaults");
    auto poolBuilderSetDevice   = ResolveNvnFn<FnMemoryPoolBuilderSetDevice>("nvnMemoryPoolBuilderSetDevice");
    auto poolBuilderSetFlags    = ResolveNvnFn<FnMemoryPoolBuilderSetFlags>("nvnMemoryPoolBuilderSetFlags");
    auto poolBuilderSetStorage  = ResolveNvnFn<FnMemoryPoolBuilderSetStorage>("nvnMemoryPoolBuilderSetStorage");
    auto poolInitialize         = ResolveNvnFn<FnMemoryPoolInitialize>("nvnMemoryPoolInitialize");

    auto texBuilderSetDevice   = ResolveNvnFn<FnTextureBuilderSetDevice>("nvnTextureBuilderSetDevice");
    auto texBuilderSetDefaults = ResolveNvnFn<FnTextureBuilderSetDefaults>("nvnTextureBuilderSetDefaults");
    auto texBuilderSetFlags    = ResolveNvnFn<FnTextureBuilderSetFlags>("nvnTextureBuilderSetFlags");
    auto texBuilderSetTarget   = ResolveNvnFn<FnTextureBuilderSetTarget>("nvnTextureBuilderSetTarget");
    auto texBuilderSetSize2D   = ResolveNvnFn<FnTextureBuilderSetSize2D>("nvnTextureBuilderSetSize2D");
    auto texBuilderSetFormat   = ResolveNvnFn<FnTextureBuilderSetFormat>("nvnTextureBuilderSetFormat");
    auto texBuilderSetStorage  = ResolveNvnFn<FnTextureBuilderSetStorage>("nvnTextureBuilderSetStorage");
    auto texBuilderSetPackagedTextureData = ResolveNvnFn<FnTextureBuilderSetPackagedTextureData>("nvnTextureBuilderSetPackagedTextureData");
    auto texInitialize         = ResolveNvnFn<FnTextureInitialize>("nvnTextureInitialize");
    auto texPoolRegisterTexture = ResolveNvnFn<FnTexturePoolRegisterTexture>("nvnTexturePoolRegisterTexture");
    auto deviceGetTextureHandle = ResolveNvnFn<FnDeviceGetTextureHandle>("nvnDeviceGetTextureHandle");

    auto texBuilderGetStorageSize = ResolveNvnFn<FnTextureBuilderGetStorageSize>("nvnTextureBuilderGetStorageSize");
    auto texBuilderGetStorageAlignment = ResolveNvnFn<FnTextureBuilderGetStorageAlignment>("nvnTextureBuilderGetStorageAlignment");
    auto texPoolInitialize      = ResolveNvnFn<FnTexturePoolInitialize>("nvnTexturePoolInitialize");

    auto smpBuilderSetDevice   = ResolveNvnFn<FnSamplerBuilderSetDevice>("nvnSamplerBuilderSetDevice");
    auto smpBuilderSetDefaults = ResolveNvnFn<FnSamplerBuilderSetDefaults>("nvnSamplerBuilderSetDefaults");
    auto smpBuilderSetMinMagFilter = ResolveNvnFn<FnSamplerBuilderSetMinMagFilter>("nvnSamplerBuilderSetMinMagFilter");
    auto smpBuilderSetWrapMode = ResolveNvnFn<FnSamplerBuilderSetWrapMode>("nvnSamplerBuilderSetWrapMode");
    auto smpInitialize         = ResolveNvnFn<FnSamplerInitialize>("nvnSamplerInitialize");

    auto smpPoolInitialize      = ResolveNvnFn<FnSamplerPoolInitialize>("nvnSamplerPoolInitialize");
    auto smpPoolRegisterSampler = ResolveNvnFn<FnSamplerPoolRegisterSampler>("nvnSamplerPoolRegisterSampler");

    if (!poolBuilderSetDefaults || !poolBuilderSetDevice || !poolBuilderSetFlags || !poolBuilderSetStorage || !poolInitialize ||
        !texBuilderSetDevice || !texBuilderSetDefaults || !texBuilderSetFlags || !texBuilderSetTarget || !texBuilderSetSize2D ||
        !texBuilderSetFormat || !texBuilderSetStorage || !texBuilderSetPackagedTextureData ||
        !texInitialize || !texPoolInitialize || !texPoolRegisterTexture ||
        !smpBuilderSetDevice || !smpBuilderSetDefaults || !smpBuilderSetMinMagFilter || !smpBuilderSetWrapMode ||
        !smpInitialize || !smpPoolInitialize || !smpPoolRegisterSampler || !deviceGetTextureHandle) {
        WIIXL_LOG("NvnOverlay: texture pipeline function resolution failed (one or more null)");
        return;
    }

    // 1. Memory pool built DIRECTLY over the packed asset bytes (header +
    // already-swizzled pixel data) - matches DebugFontMgrNvn's real pattern
    // exactly: the pool backs the SAME bytes the packaged-data pointer
    // reads, rather than a separate scratch allocation. Flag 0x21
    // (CPU_NO_ACCESS_BIT | GPU_CACHED_BIT) matches the real reference too;
    // safe here since we never write into this pool after pack time (it's
    // baked into the binary as a static const-shaped array).
    NVNmemoryPoolBuilder pixelPoolBuilder{};
    poolBuilderSetDefaults(&pixelPoolBuilder);
    poolBuilderSetDevice(&pixelPoolBuilder, device);
    poolBuilderSetFlags(&pixelPoolBuilder, 0x21);
    // kTestPicTextureSize is already page-aligned (pack_texture.py pads the
    // emitted array itself to a 4096 boundary) so the pool never claims
    // bytes past the array's real allocation.
    poolBuilderSetStorage(&pixelPoolBuilder, g_TestPicTextureBytes, kTestPicTextureSize);
    int pixelPoolResult = poolInitialize(&g_TexturePixelMemoryPool, &pixelPoolBuilder);
    WIIXL_LOG("NvnOverlay: texture pixel pool init result=%d (vtable=%p, gpuAddr=%p)",
        pixelPoolResult,
        *reinterpret_cast<void**>(&g_TexturePixelMemoryPool),
        reinterpret_cast<void**>(&g_TexturePixelMemoryPool)[8]);
    if (!pixelPoolResult) return;

    // 2. Texture object - packaged-data path (see namespace comment above).
    // SetDefaults before SetDevice - matches every other builder in this
    // file (NVNmemoryPoolBuilder, NVNbufferBuilder).
    NVNtextureBuilder texBuilder{};
    texBuilderSetDefaults(&texBuilder);
    texBuilderSetDevice(&texBuilder, device);
    texBuilderSetTarget(&texBuilder, NvnTexture::kTarget2D);
    texBuilderSetFormat(&texBuilder, NvnTexture::kFormatRGBA8);
    texBuilderSetSize2D(&texBuilder, kTestPicTextureWidth, kTestPicTextureHeight);
    texBuilderSetPackagedTextureData(&texBuilder, g_TestPicTextureBytes + kTestPicTextureHeaderSize);
    texBuilderSetStorage(&texBuilder, &g_TexturePixelMemoryPool, kTestPicTextureHeaderSize);
    {
        const uint64_t* w = reinterpret_cast<const uint64_t*>(&texBuilder);
        WIIXL_LOG("NvnOverlay: texBuilder: [0x0]=%p [0x8]=%p [0x10]=%p [0x50]=%p [0x58]=%p",
            reinterpret_cast<void*>(w[0]), reinterpret_cast<void*>(w[1]),
            reinterpret_cast<void*>(w[2]),
            reinterpret_cast<void*>(w[10]), reinterpret_cast<void*>(w[11]));
    }

    size_t storageSize = texBuilderGetStorageSize ? texBuilderGetStorageSize(&texBuilder) : kTestPicTextureDataSize;
    size_t storageAlign = texBuilderGetStorageAlignment ? texBuilderGetStorageAlignment(&texBuilder) : 512;
    WIIXL_LOG("NvnOverlay: texture storage size=%u align=%u (packed data size=%u)",
        static_cast<unsigned int>(storageSize), static_cast<unsigned int>(storageAlign),
        static_cast<unsigned int>(kTestPicTextureDataSize));

    uint8_t texInitResult = texInitialize(&g_TestPicTexture, &texBuilder);
    WIIXL_LOG("NvnOverlay: texture initialize result=%d", texInitResult);
    if (!texInitResult) return;

    // 3. Private descriptor pools + registration (id must be >=256, the
    // reserved-descriptor count - see NvnTexture::kReservedDescriptors).
    NVNmemoryPoolBuilder descPoolBuilder{};
    poolBuilderSetDefaults(&descPoolBuilder);
    poolBuilderSetDevice(&descPoolBuilder, device);
    poolBuilderSetFlags(&descPoolBuilder, 0x22);
    poolBuilderSetStorage(&descPoolBuilder, g_TextureDescriptorPoolMemory, sizeof(g_TextureDescriptorPoolMemory));
    int texDescPoolResult = poolInitialize(&g_TextureDescriptorMemoryPool, &descPoolBuilder);
    WIIXL_LOG("NvnOverlay: texture descriptor pool init result=%d", texDescPoolResult);
    if (!texDescPoolResult) return;

    constexpr int kNumDescriptors = 320; // > 256 reserved + our one real slot, comfortable margin
    uint8_t texPoolResult = texPoolInitialize(&g_TextureDescriptorPool, &g_TextureDescriptorMemoryPool, 0, kNumDescriptors);
    WIIXL_LOG("NvnOverlay: texture pool init result=%d", texPoolResult);
    if (!texPoolResult) return;

    constexpr int kTextureId = NvnTexture::kReservedDescriptors; // 256, first non-reserved slot
    texPoolRegisterTexture(&g_TextureDescriptorPool, kTextureId, &g_TestPicTexture, nullptr);

    // 4. Sampler.
    NVNsamplerBuilder smpBuilder{};
    smpBuilderSetDefaults(&smpBuilder);
    smpBuilderSetDevice(&smpBuilder, device);
    smpBuilderSetMinMagFilter(&smpBuilder, NvnTexture::kMinFilterLinear, NvnTexture::kMagFilterLinear);
    smpBuilderSetWrapMode(&smpBuilder, NvnTexture::kWrapModeClampEdge, NvnTexture::kWrapModeClampEdge, NvnTexture::kWrapModeClampEdge);
    uint8_t smpInitResult = smpInitialize(&g_TestPicSampler, &smpBuilder);
    WIIXL_LOG("NvnOverlay: sampler initialize result=%d", smpInitResult);
    if (!smpInitResult) return;

    NVNmemoryPoolBuilder smpDescPoolBuilder{};
    poolBuilderSetDefaults(&smpDescPoolBuilder);
    poolBuilderSetDevice(&smpDescPoolBuilder, device);
    poolBuilderSetFlags(&smpDescPoolBuilder, 0x22);
    poolBuilderSetStorage(&smpDescPoolBuilder, g_SamplerDescriptorPoolMemory, sizeof(g_SamplerDescriptorPoolMemory));
    int smpDescPoolResult = poolInitialize(&g_SamplerDescriptorMemoryPool, &smpDescPoolBuilder);
    WIIXL_LOG("NvnOverlay: sampler descriptor pool init result=%d", smpDescPoolResult);
    if (!smpDescPoolResult) return;

    uint8_t smpPoolResult = smpPoolInitialize(&g_SamplerDescriptorPool, &g_SamplerDescriptorMemoryPool, 0, kNumDescriptors);
    WIIXL_LOG("NvnOverlay: sampler pool init result=%d", smpPoolResult);
    if (!smpPoolResult) return;

    constexpr int kSamplerId = NvnTexture::kReservedDescriptors; // 256
    smpPoolRegisterSampler(&g_SamplerDescriptorPool, kSamplerId, &g_TestPicSampler);

    // 5. Combined handle, bound in shaders via nvnCommandBufferBindTexture.
    g_TestPicTextureHandle = deviceGetTextureHandle(device, kTextureId, kSamplerId);
    WIIXL_LOG("NvnOverlay: texture handle=%p", reinterpret_cast<void*>(g_TestPicTextureHandle));

    // 6. Vertex layout: position(vec4,loc0) + uv(vec2,loc1) + tint(vec4,loc2)
    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_TextureQuadVertexAttribStates[0]);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_TextureQuadVertexAttribStates[0], NvnFormat::kR32G32B32A32Float, 0);
    NvnFn<FnVertexAttribStateSetStreamIndex>(Offset::kVertexAttribStateSetStreamIndex)(&g_TextureQuadVertexAttribStates[0], 0);

    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_TextureQuadVertexAttribStates[1]);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_TextureQuadVertexAttribStates[1], NvnFormat::kR32G32Float, 16);
    NvnFn<FnVertexAttribStateSetStreamIndex>(Offset::kVertexAttribStateSetStreamIndex)(&g_TextureQuadVertexAttribStates[1], 0);

    NvnFn<FnVertexAttribStateSetDefaults>(Offset::kVertexAttribStateSetDefaults)(&g_TextureQuadVertexAttribStates[2]);
    NvnFn<FnVertexAttribStateSetFormat>(Offset::kVertexAttribStateSetFormat)(&g_TextureQuadVertexAttribStates[2], NvnFormat::kR32G32B32A32Float, 24);
    NvnFn<FnVertexAttribStateSetStreamIndex>(Offset::kVertexAttribStateSetStreamIndex)(&g_TextureQuadVertexAttribStates[2], 0);

    NvnFn<FnVertexStreamStateSetDefaults>(Offset::kVertexStreamStateSetDefaults)(&g_TextureQuadVertexStreamState);
    NvnFn<FnVertexStreamStateSetStride>(Offset::kVertexStreamStateSetStride)(&g_TextureQuadVertexStreamState, sizeof(TextureVertex));

    // 7. Vertex data buffer (own storage, same pattern as every other demo).
    NVNmemoryPoolBuilder dataPoolBuilder{};
    NvnFn<FnMemoryPoolBuilderSetDefaults>(Offset::kMemoryPoolBuilderSetDefaults)(&dataPoolBuilder);
    NvnFn<FnMemoryPoolBuilderSetDevice>(Offset::kMemoryPoolBuilderSetDevice)(&dataPoolBuilder, device);
    NvnFn<FnMemoryPoolBuilderSetFlags>(Offset::kMemoryPoolBuilderSetFlags)(&dataPoolBuilder, 0x22);
    NvnFn<FnMemoryPoolBuilderSetStorage>(Offset::kMemoryPoolBuilderSetStorage)(&dataPoolBuilder, g_TextureQuadDataPoolMemory, sizeof(g_TextureQuadDataPoolMemory));
    uint8_t dataPoolResult = NvnFn<FnMemoryPoolInitialize>(Offset::kMemoryPoolInitialize)(&g_TextureQuadDataMemoryPool, &dataPoolBuilder);
    WIIXL_LOG("NvnOverlay: texture quad data pool init result=%d", dataPoolResult);
    if (!dataPoolResult) return;

    NVNbufferBuilder dataBufBuilder{};
    NvnFn<FnBufferBuilderSetDefaults>(Offset::kBufferBuilderSetDefaults)(&dataBufBuilder);
    NvnFn<FnBufferBuilderSetDevice>(Offset::kBufferBuilderSetDevice)(&dataBufBuilder, device);
    NvnFn<FnBufferBuilderSetStorage>(Offset::kBufferBuilderSetStorage)(&dataBufBuilder, &g_TextureQuadDataMemoryPool, 0, sizeof(g_TextureQuadDataPoolMemory));
    uint8_t dataBufResult = NvnFn<FnBufferInitialize>(Offset::kBufferInitialize)(&g_TextureQuadDataBuffer, &dataBufBuilder);
    WIIXL_LOG("NvnOverlay: texture quad data buffer init result=%d", dataBufResult);
    if (!dataBufResult) return;

    g_TexturePipelineInitialized = true;
    WIIXL_LOG("NvnOverlay: EnsureTexturePipelineInitialized OK");
}

inline NVNprogram* GetTextureQuadProgram(void* graphicsNvn) {
    if (g_TextureQuadProgramLoaded) return g_LoadedTextureQuadProgram;
    g_TextureQuadProgramLoaded = true;
    g_LoadedTextureQuadProgram = LoadSeadBinaryProgram(
        graphicsNvn, g_TextureQuadSeadBin, kTextureQuadSeadBinSize,
        g_TextureQuadCodePoolMemory, sizeof(g_TextureQuadCodePoolMemory),
        &g_TextureQuadCodeMemoryPool, &g_TextureQuadCodeBuffer,
        &g_TextureQuadProgramStorage, "TextureQuad");
    return g_LoadedTextureQuadProgram;
}

inline void DrawTextureQuadDirect(NVNcommandBuffer* cmdBuf, void* dstTexture) {
    if (!g_TexturePipelineInitialized) return;
    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;

    NVNprogram* program = GetTextureQuadProgram(graphicsNvn);
    if (!program) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_TextureQuadDataBuffer);
    if (!mapped) return;

    // Top-left 1:1 aspect ratio quad on 16:9 display (X scaled by 9/16 to maintain square).
    constexpr float kX0 = -0.92f;
    constexpr float kX1 = -0.92f + 0.40f * (9.0f / 16.0f); // -0.695f
    constexpr float kY0 =  0.50f;
    constexpr float kY1 =  0.90f;

    const TextureVertex verts[6] = {
        {kX0, kY1, 0.0f, 1.0f,   0.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f},
        {kX0, kY0, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f},
        {kX1, kY0, 0.0f, 1.0f,   1.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f},
        {kX0, kY1, 0.0f, 1.0f,   0.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f},
        {kX1, kY0, 0.0f, 1.0f,   1.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f},
        {kX1, kY1, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f},
    };
    __builtin_memcpy(mapped, verts, sizeof(verts));
    armDCacheFlush(mapped, sizeof(verts));

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_TextureQuadDataBuffer);

    auto setRenderTargets      = NvnFn<FnCommandBufferSetRenderTargets>(Offset::kCommandBufferSetRenderTargets);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto bindProgram           = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto drawArrays            = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);
    auto setTexturePool        = NvnFn<FnCommandBufferSetTexturePool>(Offset::kCommandBufferSetTexturePool);
    auto setSamplerPool        = NvnFn<FnCommandBufferSetSamplerPool>(Offset::kCommandBufferSetSamplerPool);
    auto bindTexture           = ResolveNvnFn<FnCommandBufferBindTexture>("nvnCommandBufferBindTexture");

    // No per-draw copy needed anymore: the texture's storage pool is built
    // directly over the packed (already block-linear swizzled) asset bytes
    // at init time - see EnsureTexturePipelineInitialized and
    // scripts/pack_texture.py.

    const void* colorTargets[1] = { dstTexture };
    if (setRenderTargets) setRenderTargets(cmdBuf, 1, colorTargets, nullptr, nullptr, nullptr);

    int width = 1280;
    int height = 720;
    void* displayBuffer = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + 0x50);
    if (displayBuffer) {
        float fW = *reinterpret_cast<float*>(static_cast<uint8_t*>(displayBuffer) + 8);
        float fH = *reinterpret_cast<float*>(static_cast<uint8_t*>(displayBuffer) + 12);
        if (fW > 0.0f && fH > 0.0f) {
            width = static_cast<int>(fW);
            height = static_cast<int>(fH);
        }
    }

    if (setViewport) setViewport(cmdBuf, 0, 0, width, height);
    if (setScissor) setScissor(cmdBuf, 0, 0, width, height);

    // Our own private texture/sampler pools, not the game's.
    if (setTexturePool) setTexturePool(cmdBuf, &g_TextureDescriptorPool);
    if (setSamplerPool) setSamplerPool(cmdBuf, &g_SamplerDescriptorPool);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangles = 4;

    bindProgram(cmdBuf, reinterpret_cast<const NVNprogram*>(program), kStageMaskVertexFragment);
    bindVertexAttribState(cmdBuf, 3, g_TextureQuadVertexAttribStates);
    bindVertexStreamState(cmdBuf, 1, &g_TextureQuadVertexStreamState);
    bindVertexBuffer(cmdBuf, 0, gpuBase, sizeof(verts));
    if (bindTexture) bindTexture(cmdBuf, NvnTexture::kShaderStageFragment, 0, g_TestPicTextureHandle);
    drawArrays(cmdBuf, kPrimitiveTriangles, 0, 6);

    static uint32_t s_TextureDrawCount = 0;
    if ((s_TextureDrawCount++ % 120) == 0) {
        WIIXL_LOG("NvnOverlay: Texture quad drawn on swapchain texture %p (%dx%d, frame #%u)", dstTexture, width, height, s_TextureDrawCount);
    }
}

inline void DrawBnshQuad(void* gameFramework, float r, float g, float b, float a);

inline void DrawWithCapturedProgram(void* gameFramework, float r, float g, float b, float a) {
    EnsureInitialized();
    if (!g_Initialized) return;
    InstallBindProgramHookIfNeeded();
    EnsureCapturedDrawInitialized(gameFramework);
    if (!g_CapturedDrawInitialized) return;

    {
        static void* lastLogged = nullptr;
        if (g_CapturedNvnProgram != lastLogged) {
            lastLogged = g_CapturedNvnProgram;
            WIIXL_LOG("NvnOverlay: captured program changed, ptr=%p", g_CapturedNvnProgram);
        }
    }
    if (!g_CapturedNvnProgram) return;

    void* queue = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kQueueOffset);
    if (!queue) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_CapturedDrawDataBuffer);
    if (!mapped) return;
    const float verts[4][4] = {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f},
    };
    __builtin_memcpy(mapped, verts, sizeof(verts));

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_CapturedDrawDataBuffer);

    auto beginRecording = NvnFn<FnCommandBufferBeginRecording>(Offset::kCommandBufferBeginRecording);
    auto endRecording   = NvnFn<FnCommandBufferEndRecording>(Offset::kCommandBufferEndRecording);
    auto submitCommands = NvnFn<FnQueueSubmitCommands>(Offset::kQueueSubmitCommands);
    auto bindProgram           = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram); // now routed through HookedBindProgram
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto drawArrays            = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangleStrip = 5;

    WIIXL_LOG("NvnOverlay: calling beginRecording");
    beginRecording(&g_CommandBuffer);
    WIIXL_LOG("NvnOverlay: beginRecording returned");

    // Bind texture pool, sampler pool, and shader scratch memory
    void* graphicsNvn = GetGraphicsNvnInstance();
    if (graphicsNvn) {
        auto setTexturePool = NvnFn<FnCommandBufferSetTexturePool>(Offset::kCommandBufferSetTexturePool);
        auto setSamplerPool = NvnFn<FnCommandBufferSetSamplerPool>(Offset::kCommandBufferSetSamplerPool);
        if (setTexturePool) setTexturePool(&g_CommandBuffer, reinterpret_cast<uint8_t*>(graphicsNvn) + 0x58);
        if (setSamplerPool) setSamplerPool(&g_CommandBuffer, reinterpret_cast<uint8_t*>(graphicsNvn) + 0x78);
        WIIXL_LOG("NvnOverlay: texture and sampler pools bound");
    }
    if (gameFramework) {
        auto setShaderScratchMemory = NvnFn<FnCommandBufferSetShaderScratchMemory>(Offset::kCommandBufferSetShaderScratchMemory);
        void* scratchPool = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gameFramework) + 0x170);
        uint32_t scratchSize = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gameFramework) + 0x178);
        if (setShaderScratchMemory && scratchPool && scratchSize > 0) {
            setShaderScratchMemory(&g_CommandBuffer, reinterpret_cast<const NVNmemoryPool*>(scratchPool), 0, scratchSize);
        }
    }

    setViewport(&g_CommandBuffer, 0, 0, 1280, 720);
    setScissor(&g_CommandBuffer, 0, 0, 1280, 720);

    bindProgram(&g_CommandBuffer, reinterpret_cast<const NVNprogram*>(g_CapturedNvnProgram), kStageMaskVertexFragment);
    bindVertexAttribState(&g_CommandBuffer, 1, &g_CapturedDrawVertexAttribState);
    bindVertexStreamState(&g_CommandBuffer, 1, &g_CapturedDrawVertexStreamState);
    bindVertexBuffer(&g_CommandBuffer, 0, gpuBase, sizeof(verts));
    drawArrays(&g_CommandBuffer, kPrimitiveTriangleStrip, 0, 4);

    NVNcommandHandle handle = endRecording(&g_CommandBuffer);
    submitCommands(queue, 1, &handle);

    {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            loggedOnce = true;
            WIIXL_LOG("NvnOverlay: captured-program draw loop running smoothly, handle=%p", reinterpret_cast<void*>(handle));
        }
    }
}

inline void DrawBnshQuad(void* gameFramework, float r, float g, float b, float a) {
    EnsureInitialized();
    if (!g_Initialized) return;
    EnsureCapturedDrawInitialized(gameFramework);
    if (!g_CapturedDrawInitialized) return;

    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;

    NVNprogram* program = GetBnshProgram(graphicsNvn);
    if (!program) {
        DrawWithCapturedProgram(gameFramework, r, g, b, a);
        return;
    }

    void* queue = *reinterpret_cast<void**>(static_cast<uint8_t*>(gameFramework) + GameFramework::kQueueOffset);
    if (!queue) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_CapturedDrawDataBuffer);
    if (!mapped) return;
    const RainbowVertex verts[12] = {
        // Triangle 1 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f}, // Bottom-left (Green)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)

        // Triangle 2 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
        { 0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f}, // Top-right (Yellow)

        // Triangle 1 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f}, // Bottom-left (Green)

        // Triangle 2 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f}, // Top-right (Yellow)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
    };
    __builtin_memcpy(mapped, verts, sizeof(verts));

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_CapturedDrawDataBuffer);

    auto beginRecording = NvnFn<FnCommandBufferBeginRecording>(Offset::kCommandBufferBeginRecording);
    auto endRecording   = NvnFn<FnCommandBufferEndRecording>(Offset::kCommandBufferEndRecording);
    auto submitCommands = NvnFn<FnQueueSubmitCommands>(Offset::kQueueSubmitCommands);
    auto bindProgram           = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto drawArrays            = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangles = 4;

    beginRecording(&g_CommandBuffer);

    {
        auto setTexturePool = NvnFn<FnCommandBufferSetTexturePool>(Offset::kCommandBufferSetTexturePool);
        auto setSamplerPool = NvnFn<FnCommandBufferSetSamplerPool>(Offset::kCommandBufferSetSamplerPool);
        if (setTexturePool) setTexturePool(&g_CommandBuffer, reinterpret_cast<uint8_t*>(graphicsNvn) + 0x58);
        if (setSamplerPool) setSamplerPool(&g_CommandBuffer, reinterpret_cast<uint8_t*>(graphicsNvn) + 0x78);
    }
    if (gameFramework) {
        auto setShaderScratchMemory = NvnFn<FnCommandBufferSetShaderScratchMemory>(Offset::kCommandBufferSetShaderScratchMemory);
        void* scratchPool = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gameFramework) + 0x170);
        uint32_t scratchSize = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gameFramework) + 0x178);
        if (setShaderScratchMemory && scratchPool && scratchSize > 0) {
            setShaderScratchMemory(&g_CommandBuffer, reinterpret_cast<const NVNmemoryPool*>(scratchPool), 0, scratchSize);
        }
    }

    setViewport(&g_CommandBuffer, 0, 0, 1280, 720);
    setScissor(&g_CommandBuffer, 0, 0, 1280, 720);

    bindProgram(&g_CommandBuffer, reinterpret_cast<const NVNprogram*>(program), kStageMaskVertexFragment);
    bindVertexAttribState(&g_CommandBuffer, 2, g_RainbowVertexAttribStates);
    bindVertexStreamState(&g_CommandBuffer, 1, &g_RainbowVertexStreamState);
    bindVertexBuffer(&g_CommandBuffer, 0, gpuBase, sizeof(verts));
    drawArrays(&g_CommandBuffer, kPrimitiveTriangles, 0, 12);

    NVNcommandHandle handle = endRecording(&g_CommandBuffer);
    submitCommands(queue, 1, &handle);

    {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            loggedOnce = true;
            WIIXL_LOG("NvnOverlay: Rainbow BNSH quad draw submitted successfully, handle=%p", reinterpret_cast<void*>(handle));
        }
    }
}

inline void DrawBnshQuadOnCmdBuf(NVNcommandBuffer* cmdBuf, void* gameFramework) {
    if (!g_CapturedDrawInitialized) {
        EnsureCapturedDrawInitialized(gameFramework);
    }
    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;

    NVNprogram* program = GetBnshProgram(graphicsNvn);
    if (!program) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_CapturedDrawDataBuffer);
    if (!mapped) return;
    const RainbowVertex verts[12] = {
        // Triangle 1 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f}, // Bottom-left (Green)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)

        // Triangle 2 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
        { 0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f}, // Top-right (Yellow)

        // Triangle 1 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f}, // Bottom-left (Green)

        // Triangle 2 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f}, // Top-right (Yellow)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
    };
    __builtin_memcpy(mapped, verts, sizeof(verts));

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_CapturedDrawDataBuffer);

    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto bindProgram           = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto drawArrays            = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    static NVNdepthStencilState s_DepthStencilState{};
    static NVNpolygonState      s_PolygonState{};
    static NVNcolorState        s_ColorState{};
    static NVNchannelMaskState  s_ChannelMaskState{};
    static bool s_StatesInitialized = false;

    if (!s_StatesInitialized) {
        s_StatesInitialized = true;

        auto dsSetDefaults    = NvnFn<FnDepthStencilStateSetDefaults>(Offset::kDepthStencilStateSetDefaults);
        auto dsSetDepthTest   = NvnFn<FnDepthStencilStateSetDepthTestEnable>(Offset::kDepthStencilStateSetDepthTestEnable);
        auto dsSetDepthWrite  = NvnFn<FnDepthStencilStateSetDepthWriteEnable>(Offset::kDepthStencilStateSetDepthWriteEnable);
        if (dsSetDefaults) dsSetDefaults(&s_DepthStencilState);
        if (dsSetDepthTest) dsSetDepthTest(&s_DepthStencilState, 0);
        if (dsSetDepthWrite) dsSetDepthWrite(&s_DepthStencilState, 0);

        auto polySetDefaults  = NvnFn<FnPolygonStateSetDefaults>(Offset::kPolygonStateSetDefaults);
        auto polySetCull      = NvnFn<FnPolygonStateSetCullFace>(Offset::kPolygonStateSetCullFace);
        if (polySetDefaults) polySetDefaults(&s_PolygonState);
        if (polySetCull) polySetCull(&s_PolygonState, 0);

        auto colSetDefaults   = NvnFn<FnColorStateSetDefaults>(Offset::kColorStateSetDefaults);
        auto colSetBlend      = NvnFn<FnColorStateSetBlendEnable>(Offset::kColorStateSetBlendEnable);
        if (colSetDefaults) colSetDefaults(&s_ColorState);
        if (colSetBlend) colSetBlend(&s_ColorState, 0, 0);

        auto chanSetDefaults  = NvnFn<FnChannelMaskStateSetDefaults>(Offset::kChannelMaskStateSetDefaults);
        auto chanSetMask      = NvnFn<FnChannelMaskStateSetChannelMask>(Offset::kChannelMaskStateSetChannelMask);
        if (chanSetDefaults) chanSetDefaults(&s_ChannelMaskState);
        if (chanSetMask) chanSetMask(&s_ChannelMaskState, 0, 1, 1, 1, 1);
    }

    auto bindDepthStencil = NvnFn<FnCommandBufferBindDepthStencilState>(Offset::kCommandBufferBindDepthStencilState);
    auto bindPolygon      = NvnFn<FnCommandBufferBindPolygonState>(Offset::kCommandBufferBindPolygonState);
    auto bindColor        = NvnFn<FnCommandBufferBindColorState>(Offset::kCommandBufferBindColorState);
    auto bindChannelMask  = NvnFn<FnCommandBufferBindChannelMaskState>(Offset::kCommandBufferBindChannelMaskState);

    if (bindDepthStencil) bindDepthStencil(cmdBuf, &g_RainbowDepthStencilState);
    if (bindPolygon) bindPolygon(cmdBuf, &g_RainbowPolygonState);
    if (bindColor) bindColor(cmdBuf, &g_RainbowColorState);
    if (bindChannelMask) bindChannelMask(cmdBuf, &g_RainbowChannelMaskState);

    if (setViewport) setViewport(cmdBuf, 0, 0, 1280, 720);
    if (setScissor) setScissor(cmdBuf, 0, 0, 1280, 720);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangles = 4;

    bindProgram(cmdBuf, reinterpret_cast<const NVNprogram*>(program), kStageMaskVertexFragment);
    bindVertexAttribState(cmdBuf, 2, g_RainbowVertexAttribStates);
    bindVertexStreamState(cmdBuf, 1, &g_RainbowVertexStreamState);
    bindVertexBuffer(cmdBuf, 0, gpuBase, sizeof(verts));
    drawArrays(cmdBuf, kPrimitiveTriangles, 0, 12);

    static uint32_t s_DrawFrameCount = 0;
    if ((s_DrawFrameCount++ % 120) == 0) {
        WIIXL_LOG("NvnOverlay: Rainbow quad drawn on cmdBuf %p (frame #%u)", cmdBuf, s_DrawFrameCount);
    }
}

inline void DrawBnshQuadDirect(NVNcommandBuffer* cmdBuf, void* dstTexture) {
    if (!g_CapturedDrawInitialized) return;
    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;

    NVNprogram* program = GetBnshProgram(graphicsNvn);
    if (!program) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_CapturedDrawDataBuffer);
    if (!mapped) return;
    const RainbowVertex verts[12] = {
        // Triangle 1 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f}, // Bottom-left (Green)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)

        // Triangle 2 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
        { 0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f}, // Top-right (Yellow)

        // Triangle 1 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f}, // Bottom-left (Green)

        // Triangle 2 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f}, // Top-left (Red)
        { 0.7f,  0.7f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f}, // Top-right (Yellow)
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f}, // Bottom-right (Blue)
    };
    __builtin_memcpy(mapped, verts, sizeof(verts));
    armDCacheFlush(mapped, sizeof(verts));

    // Diagnostic: confirm what we wrote — log first vertex color (should be 1,0,0,1 = Red)
    static bool s_VertLogDone = false;
    if (!s_VertLogDone) {
        s_VertLogDone = true;
        const float* f = reinterpret_cast<const float*>(mapped);
        // f[0..3]=pos, f[4..7]=color
        WIIXL_LOG("NvnOverlay: vert[0] pos=(%.2f,%.2f,%.2f,%.2f) color=(%.2f,%.2f,%.2f,%.2f)",
            f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
    }

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_CapturedDrawDataBuffer);

    auto setRenderTargets      = NvnFn<FnCommandBufferSetRenderTargets>(Offset::kCommandBufferSetRenderTargets);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto bindProgram           = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto drawArrays            = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    const void* colorTargets[1] = { dstTexture };
    if (setRenderTargets) setRenderTargets(cmdBuf, 1, colorTargets, nullptr, nullptr, nullptr);

    // Strip ALL state object binds for now.
    // We previously got yellow (wrong state offsets), now solid black.
    // Each state we bind might be killing the output. Inherit the game's
    // last-bound state for EVERYTHING (color, blend, channelmask, depth, polygon).
    // The game's state allows color writes (BotW renders correctly).

    int width = 1280;
    int height = 720;
    void* displayBuffer = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + 0x50);
    if (displayBuffer) {
        float fW = *reinterpret_cast<float*>(static_cast<uint8_t*>(displayBuffer) + 8);
        float fH = *reinterpret_cast<float*>(static_cast<uint8_t*>(displayBuffer) + 12);
        if (fW > 0.0f && fH > 0.0f) {
            width = static_cast<int>(fW);
            height = static_cast<int>(fH);
        }
    }

    if (setViewport) setViewport(cmdBuf, 0, 0, width, height);
    if (setScissor) setScissor(cmdBuf, 0, 0, width, height);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangles = 4;

    bindProgram(cmdBuf, reinterpret_cast<const NVNprogram*>(program), kStageMaskVertexFragment);
    bindVertexAttribState(cmdBuf, 2, g_RainbowVertexAttribStates);
    bindVertexStreamState(cmdBuf, 1, &g_RainbowVertexStreamState);
    bindVertexBuffer(cmdBuf, 0, gpuBase, sizeof(verts));
    drawArrays(cmdBuf, kPrimitiveTriangles, 0, 12);

    static uint32_t s_DirectDrawCount = 0;
    if ((s_DirectDrawCount++ % 120) == 0) {
        WIIXL_LOG("NvnOverlay: Direct rainbow quad drawn on swapchain texture %p (%dx%d, frame #%u)", dstTexture, width, height, s_DirectDrawCount);
    }
}

// Custom-shader demo: identical to DrawBnshQuadDirect above (same command
// buffer, same render target, same vertex attribute layout, same proven state
// handling) except it binds GetPlasmaProgram() instead of GetBnshProgram(),
// and repurposes each vertex's .a color channel to carry a per-frame "time"
// value instead of a fixed 1.0 alpha - shaders/plasma.frag reads it back out
// of v_Color.a to animate. No new NVN binding (no UBO, no texture) was needed
// to get a fully custom, animated fragment effect running - proof the whole
// pipeline generalizes past the original flat-interpolated-color rainbow.
inline void DrawPlasmaQuadDirect(NVNcommandBuffer* cmdBuf, void* dstTexture) {
    if (!g_CapturedDrawInitialized) return;
    void* graphicsNvn = GetGraphicsNvnInstance();
    if (!graphicsNvn) return;

    NVNprogram* program = GetPlasmaProgram(graphicsNvn);
    if (!program) return;

    void* mapped = NvnFn<FnBufferMap>(Offset::kBufferMap)(&g_CapturedDrawDataBuffer);
    if (!mapped) return;

    static uint32_t s_FrameCount = 0;
    float t = static_cast<float>(s_FrameCount++) * 0.033f; // ~2 radians/sec at 60fps

    RainbowVertex verts[12] = {
        // Triangle 1 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},

        // Triangle 2 (CCW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        { 0.7f,  0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},

        // Triangle 1 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        {-0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},

        // Triangle 2 (CW)
        {-0.7f,  0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        { 0.7f,  0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
        { 0.7f, -0.7f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, t},
    };
    __builtin_memcpy(mapped, verts, sizeof(verts));
    armDCacheFlush(mapped, sizeof(verts));

    uint64_t gpuBase = NvnFn<FnBufferGetAddress>(Offset::kBufferGetAddress)(&g_CapturedDrawDataBuffer);

    auto setRenderTargets      = NvnFn<FnCommandBufferSetRenderTargets>(Offset::kCommandBufferSetRenderTargets);
    auto setViewport           = NvnFn<FnCommandBufferSetViewport>(Offset::kCommandBufferSetViewport);
    auto setScissor            = NvnFn<FnCommandBufferSetScissor>(Offset::kCommandBufferSetScissor);
    auto bindProgram           = NvnFn<FnCommandBufferBindProgram>(Offset::kCommandBufferBindProgram);
    auto bindVertexAttribState = NvnFn<FnCommandBufferBindVertexAttribState>(Offset::kCommandBufferBindVertexAttribState);
    auto bindVertexStreamState = NvnFn<FnCommandBufferBindVertexStreamState>(Offset::kCommandBufferBindVertexStreamState);
    auto bindVertexBuffer      = NvnFn<FnCommandBufferBindVertexBuffer>(Offset::kCommandBufferBindVertexBuffer);
    auto drawArrays            = NvnFn<FnCommandBufferDrawArrays>(Offset::kCommandBufferDrawArrays);

    const void* colorTargets[1] = { dstTexture };
    if (setRenderTargets) setRenderTargets(cmdBuf, 1, colorTargets, nullptr, nullptr, nullptr);

    int width = 1280;
    int height = 720;
    void* displayBuffer = *reinterpret_cast<void**>(static_cast<uint8_t*>(graphicsNvn) + 0x50);
    if (displayBuffer) {
        float fW = *reinterpret_cast<float*>(static_cast<uint8_t*>(displayBuffer) + 8);
        float fH = *reinterpret_cast<float*>(static_cast<uint8_t*>(displayBuffer) + 12);
        if (fW > 0.0f && fH > 0.0f) {
            width = static_cast<int>(fW);
            height = static_cast<int>(fH);
        }
    }

    if (setViewport) setViewport(cmdBuf, 0, 0, width, height);
    if (setScissor) setScissor(cmdBuf, 0, 0, width, height);

    constexpr uint32_t kStageMaskVertexFragment = 0x1 | 0x2;
    constexpr int kPrimitiveTriangles = 4;

    bindProgram(cmdBuf, reinterpret_cast<const NVNprogram*>(program), kStageMaskVertexFragment);
    bindVertexAttribState(cmdBuf, 2, g_RainbowVertexAttribStates);
    bindVertexStreamState(cmdBuf, 1, &g_RainbowVertexStreamState);
    bindVertexBuffer(cmdBuf, 0, gpuBase, sizeof(verts));
    drawArrays(cmdBuf, kPrimitiveTriangles, 0, 12);

    if ((s_FrameCount % 120) == 0) {
        WIIXL_LOG("NvnOverlay: Plasma quad drawn on swapchain texture %p (%dx%d, t=%d)", dstTexture, width, height, static_cast<int>(t * 1000));
    }
}

inline void* g_MainGameFramework = nullptr;
inline FnCommandBufferEndRecording g_OrigCommandBufferEndRecording = nullptr;
inline bool g_EndRecordingHookInstalled = false;

inline NVNcommandHandle HookedCommandBufferEndRecording(NVNcommandBuffer* cmdBuf) {
    if (g_CapturedDrawInitialized && g_MainGameFramework) {
        void* mainCmdBuf = *reinterpret_cast<void**>(
            static_cast<uint8_t*>(g_MainGameFramework) + 0x158);
        if (cmdBuf == mainCmdBuf) {
            void* graphicsNvn = GetGraphicsNvnInstance();
            if (graphicsNvn) {
                void* displayBuffer = *reinterpret_cast<void**>(
                    static_cast<uint8_t*>(graphicsNvn) + 0x50);
                if (displayBuffer) {
                    uint32_t activeIdx = *reinterpret_cast<uint32_t*>(
                        static_cast<uint8_t*>(displayBuffer) + 0x20);
                    if (activeIdx >= 3) activeIdx = 0;
                    void* dstTexture = *reinterpret_cast<void**>(
                        static_cast<uint8_t*>(displayBuffer) + 0x28 + activeIdx * 8);
                    if (dstTexture) {
                        DrawTextureQuadDirect(cmdBuf, dstTexture);
                    }
                }
            }
        }
    }
    return g_OrigCommandBufferEndRecording(cmdBuf);
}

inline void EnsureEndRecordingHookInstalled() {
    if (g_EndRecordingHookInstalled) return;
    uintptr_t targetStart = exl::util::modules::GetTargetStart();
    uintptr_t slotAddr = targetStart + Offset::kCommandBufferEndRecording;
    uintptr_t cell = *reinterpret_cast<uintptr_t*>(slotAddr);
    void** fnPtrLoc = reinterpret_cast<void**>(cell);
    if (fnPtrLoc && *fnPtrLoc) {
        g_OrigCommandBufferEndRecording = reinterpret_cast<FnCommandBufferEndRecording>(*fnPtrLoc);
        *fnPtrLoc = reinterpret_cast<void*>(&HookedCommandBufferEndRecording);
        g_EndRecordingHookInstalled = true;
        WIIXL_LOG("NvnOverlay: EndRecording hook installed (orig=%p, cell=%p)", g_OrigCommandBufferEndRecording, fnPtrLoc);
    }
}

} // namespace NvnOverlay

#endif // WIIXL_SWITCH
