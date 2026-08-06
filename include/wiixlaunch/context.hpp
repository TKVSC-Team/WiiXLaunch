#pragma once

#include "platform.hpp"

namespace WiiXLaunch {

// ---------------------------------------------------------
// CPU Register Context Abstraction for Inline Hooks
// ---------------------------------------------------------
struct CpuContext {
#if WIIXL_SWITCH
    uint64_t x[31]; // x0 - x30
    uint64_t sp;
    uint64_t pc;

    // Portable argument getters
    uint64_t GetArg(size_t index) const {
        if (index < 8) return x[index]; // x0-x7
        return 0;
    }

    void SetArg(size_t index, uint64_t val) {
        if (index < 8) x[index] = val;
    }
#elif WIIXL_WIIU
    uint32_t r[32]; // r0 - r31
    uint32_t lr;
    uint32_t cr;

    // Portable argument getters (PPC SVR4 ABI uses r3-r10)
    uint32_t GetArg(size_t index) const {
        if (index < 8) return r[3 + index];
        return 0;
    }

    void SetArg(size_t index, uint32_t val) {
        if (index < 8) r[3 + index] = val;
    }
#endif
};

} // namespace WiiXLaunch
