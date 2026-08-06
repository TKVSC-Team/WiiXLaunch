#pragma once

#include "platform.hpp"

#if WIIXL_SWITCH
    #include <exlaunch.hpp>
#elif WIIXL_WIIU
    #include <coreinit/cache.h>
    #include <coreinit/memory.h>
    #include <cstring>
#endif

namespace WiiXLaunch {

// ---------------------------------------------------------
// CodePatch: Raw Memory & Instruction Patching
// ---------------------------------------------------------
class CodePatch {
public:
    static void Write(uptr targetOffset, const void* data, size_t size) {
#if WIIXL_SWITCH
        uptr dest = exl::util::modules::GetTargetStart() + targetOffset;
        exl::patch::CodePatch patch(dest, data, size);
        patch.Perform();
#elif WIIXL_WIIU
        uptr dest = targetOffset;
        // On Wii U, unlock page permissions, copy memory, and flush cache
        OSSetMEM2PagePermission(reinterpret_cast<void*>(dest), size, OS_MEM_PERMISSION_RWX);
        std::memcpy(reinterpret_cast<void*>(dest), data, size);

        // Cache coherency flush
        dcbst(reinterpret_cast<void*>(dest));
        sync();
        icbi(reinterpret_cast<void*>(dest));
        isync();
#endif
    }

    template<typename T>
    static void WriteValue(uptr targetOffset, T value) {
        Write(targetOffset, &value, sizeof(T));
    }

    static void Nop(uptr targetOffset) {
#if WIIXL_SWITCH
        // ARM64 NOP instruction (0xD503201F)
        uint32_t nop = 0xD503201F;
        WriteValue<uint32_t>(targetOffset, nop);
#elif WIIXL_WIIU
        // PowerPC NOP instruction (0x60000000 - ori r0, r0, 0)
        uint32_t nop = 0x60000000;
        WriteValue<uint32_t>(targetOffset, nop);
#endif
    }
};

} // namespace WiiXLaunch
