#pragma once

#include "platform.hpp"

#if WIIXL_SWITCH
    #include <lib.hpp>
#elif WIIXL_WIIU
    #include <coreinit/cache.h>
    #include <coreinit/memory.h>
    #include <cstring>
#elif WIIXL_CEMU
    #include "wiixl_cemu_backend.hpp"
    #include <cstring>
#endif

namespace WiiXLaunch {

class CodePatch {
public:
    static void Write(uptr targetOffset, const void* data, size_t size) {
#if WIIXL_SWITCH
        exl::patch::StreamPatcher patch(targetOffset);
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for(size_t i = 0; i < size; i++) {
            patch.Write<uint8_t>(p[i]);
        }
#elif WIIXL_WIIU
        uptr dest = targetOffset;
        std::memcpy(reinterpret_cast<void*>(dest), data, size);
        DCFlushRange(reinterpret_cast<void*>(dest), size);
        ICInvalidateRange(reinterpret_cast<void*>(dest), size);
#elif WIIXL_CEMU
        uptr dest = targetOffset;
        std::memcpy(reinterpret_cast<void*>(dest), data, size);
#endif
    }

    template<typename T>
    static void WriteValue(uptr targetOffset, T value) {
        Write(targetOffset, &value, sizeof(T));
    }

    static void Nop(uptr targetOffset) {
#if WIIXL_SWITCH
        uint32_t nop = 0xD503201F;
        WriteValue<uint32_t>(targetOffset, nop);
#elif WIIXL_WIIU
        uint32_t nop = 0x60000000;
        WriteValue<uint32_t>(targetOffset, nop);
#elif WIIXL_CEMU
        WiiXLaunch::Backend::Nop(targetOffset);
#endif
    }
};

}
