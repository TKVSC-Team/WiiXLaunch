#pragma once

#include "platform.hpp"
#include <cstdint>

// Fixed-size ring buffer for debugging on Cemu specifically: Cemu's HLE
// intercepts the real OS log calls, and the CEMU codecave target has no
// console/log access at all. The buffer lives as a plain static global in
// the mod's own memory - no syscalls, no HLE dependency. A host-side tool
// polls it by scanning the target process for the magic cookie below and
// reading entries directly out of process memory.
//
// Fields that the host-side reader depends on are `volatile` so the compiler
// can't dead-store-eliminate or reorder writes the C++ abstract machine
// thinks nobody reads - the "reader" is an external process the compiler
// doesn't know about.
//
// Deliberately scoped to WIIXL_CEMU only - real Wii U (Aroma/WUPS) and
// Switch builds don't need this and shouldn't carry the dead weight of an
// unused ~4KB global. Call sites should go through the WIIXL_LOG(...) macro
// below rather than WiiXLaunch::Debug::LogWrite directly, so they compile to
// nothing on other platforms without needing their own #if guards.

#if WIIXL_CEMU

namespace WiiXLaunch::Debug {

constexpr uint64_t kLogMagic = 0x5749495847313031ULL; // "WIIXG101"
constexpr uint32_t kLogCapacity = 128;

enum LogTag : uint32_t {
    LOG_TAG_PING           = 1, // trivial end-to-end pipeline test entry
    LOG_TAG_VPAD            = 2, // u0=canonical hold, f0-3=lx,ly,rx,ry
    LOG_TAG_KPAD            = 3, // u0=canonical hold, f0-3=lx,ly,rx,ry
    LOG_TAG_CAM_ENTRY       = 4, // u0=camControllerObj ptr, f0=blendFactor
    // u0=reason (0=freecam inactive,1=null camera,2=implausible pointer,3=not the locked-on camera,4=geometry doesn't look real).
    // Orig() is called untouched for reasons 0-3; for reason 4 the object failed
    // even a coarse validity check, so Orig() is skipped too - see the crash note
    // at the geometryLooksReal check in main.cpp.
    LOG_TAG_CAM_SKIP_ORIG   = 5,
    // The following all use u0 as a vector discriminator: 0=pos, 1=at, 2=up
    LOG_TAG_CAM_DEREF       = 6, // raw pos/at/up read at hook entry, before we touch anything
    LOG_TAG_CAM_INIT        = 7, // one-shot snapshot on freecam (re)init: f0=hDist, f1=yaw, f2=pitch (u0 unused)
    LOG_TAG_CAM_PRE_WRITE   = 8, // our computed value about to be written into camera
    LOG_TAG_CAM_POST_ORIG   = 9, // read-back of pos/at/up AFTER Orig() returns - proves whether Orig()/game overwrote our write
};

struct LogEntry {
    uint32_t tag;
    uint32_t frame;
    float f0, f1, f2, f3;
    uint32_t u0;
    uint32_t _pad;
};
static_assert(sizeof(LogEntry) == 32, "LogEntry layout drifted - update the host-side reader too");

struct LogRingBuffer {
    uint64_t magic;
    volatile uint32_t writeIndex; // monotonically increasing; slot = writeIndex % capacity
    uint32_t capacity;
    volatile LogEntry entries[kLogCapacity];
};

inline LogRingBuffer g_DebugLog = { kLogMagic, 0, kLogCapacity, {} };

inline void LogWrite(uint32_t tag, uint32_t frame, float f0 = 0.0f, float f1 = 0.0f,
                      float f2 = 0.0f, float f3 = 0.0f, uint32_t u0 = 0) {
    uint32_t idx = g_DebugLog.writeIndex;
    volatile LogEntry& e = g_DebugLog.entries[idx % kLogCapacity];
    e.tag = tag;
    e.frame = frame;
    e.f0 = f0;
    e.f1 = f1;
    e.f2 = f2;
    e.f3 = f3;
    e.u0 = u0;
    g_DebugLog.writeIndex = idx + 1; // published last - reader treats this as the "entry ready" signal
}

} // namespace WiiXLaunch::Debug

#define WIIXL_LOG(tag, ...) ::WiiXLaunch::Debug::LogWrite(::WiiXLaunch::Debug::tag, __VA_ARGS__)

#else // !WIIXL_CEMU

#define WIIXL_LOG(tag, ...) ((void)0)

#endif // WIIXL_CEMU
