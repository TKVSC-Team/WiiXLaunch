#pragma once

#include "platform.hpp"
#include <cstdint>
#include <cstdarg>
#include <cstdio>

#if WIIXL_SWITCH
#include <lib.hpp>
#include <string_view>
#elif WIIXL_WIIU
#include <notifications/notifications.h>
#endif

// One log call, WIIXL_LOG(fmt, ...), routed to whatever's actually available
// on each target. None of these platforms have a normal console:
//   Switch: exlaunch's SvcLogger, via svcOutputDebugString (a debugger or
//           emulator can pick it up).
//   Wii U (Aroma): an on-screen notification toast - there's no log/console
//           access without extra setup.
//   Cemu: the CEMU codecave has no OS log access at all, so entries go into
//           a ring buffer that a host-side tool (tools/ring_log_reader) polls
//           out of process memory. Entries are free-form text framed by
//           kEntryStart/kEntryEnd cookies, so the reader never needs to know
//           your message shape or be recompiled for it.

namespace WiiXLaunch::Debug {

constexpr uint32_t kMaxLogTextLen = 200; // per-call cap, shared by all platforms

#if WIIXL_CEMU

constexpr uint64_t kBufferMagic = 0x5749495847313031ULL; // "WIIXG101"
constexpr uint8_t kEntryStart[4] = { 'W', 'X', '[', '[' };
constexpr uint8_t kEntryEnd[4]   = { 'W', 'X', ']', ']' };
constexpr uint32_t kBufferCapacity = 4096;

struct LogRingBuffer {
    uint64_t magic;

    volatile uint32_t writeIndex; // monotonically increasing byte offset; position = writeIndex % capacity
    uint32_t capacity;
    volatile uint8_t data[kBufferCapacity];
};

inline LogRingBuffer g_DebugLog = { kBufferMagic, 0, kBufferCapacity, {} };

inline void WriteRingEntry(const char* text, int len) {
    uint32_t idx = g_DebugLog.writeIndex;
    auto put = [&](uint8_t b) {
        g_DebugLog.data[idx % kBufferCapacity] = b;
        idx++;
    };

    for (uint8_t b : kEntryStart) put(b);
    for (int i = 0; i < len; i++) put(static_cast<uint8_t>(text[i]));
    for (uint8_t b : kEntryEnd) put(b);

    g_DebugLog.writeIndex = idx;
}

#endif // WIIXL_CEMU

inline void DebugPrint(const char* fmt, ...) {
    char text[kMaxLogTextLen];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    if (len <= 0) return;
    if (len > static_cast<int>(sizeof(text)) - 1) len = sizeof(text) - 1;

#if WIIXL_SWITCH
    exl::log::Logging.Log(std::string_view{ text, static_cast<size_t>(len) });
#elif WIIXL_WIIU
    NotificationModule_AddInfoNotification(text);
#elif WIIXL_CEMU
    WriteRingEntry(text, len);
#endif
}

}

#define WIIXL_LOG(...) ::WiiXLaunch::Debug::DebugPrint(__VA_ARGS__)
