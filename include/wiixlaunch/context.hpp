#pragma once

#include <cstdint>
#include <cstddef>
#include "platform.hpp"

namespace WiiXLaunch {

struct CpuContext {
#if WIIXL_SWITCH
    uint64_t x[31];
    uint64_t sp;
    uint64_t pc;

    uint64_t GetArg(size_t index) const {
        if (index < 8) return x[index];
        return 0;
    }

    void SetArg(size_t index, uint64_t val) {
        if (index < 8) x[index] = val;
    }
#elif WIIXL_WIIU
    uint32_t r[32];
    uint32_t lr;
    uint32_t cr;

    uint32_t GetArg(size_t index) const {
        if (index < 8) return r[3 + index];
        return 0;
    }

    void SetArg(size_t index, uint32_t val) {
        if (index < 8) r[3 + index] = val;
    }
#endif
};

}
