#pragma once

#include <cstdint>
#include <cstring>

namespace WiiXLaunch {
namespace Backend {

    constexpr size_t TRAMPOLINE_POOL_SIZE = 0x1000;
    static uint8_t g_TrampolinePool[TRAMPOLINE_POOL_SIZE];
    static size_t g_TrampolineAllocated = 0;

    extern "C" uintptr_t g_CodeCaveBase;
    
    inline void* AllocateTrampoline(size_t size) {
        if (g_TrampolineAllocated + size > TRAMPOLINE_POOL_SIZE) {
            return nullptr;
        }
        void* ptr = &g_TrampolinePool[g_TrampolineAllocated];
        g_TrampolineAllocated += size;
        
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        if (p < 0x01000000) {
            p += g_CodeCaveBase;
        }
        return reinterpret_cast<void*>(p);
    }

    inline bool InitCemuBackend() {
        return true;
    }

    inline void Nop(uintptr_t addr) {
        *(volatile uint32_t*)addr = 0x60000000;
    }

    inline void FlushCache(uintptr_t addr, size_t size = 4) {
        uintptr_t p = addr & ~31;
        uintptr_t end = addr + size;
        for (; p < end; p += 32) {
            asm volatile("dcbst 0, %0" : : "r"(p));
            asm volatile("sync");
            asm volatile("icbi 0, %0" : : "r"(p));
        }
        asm volatile("isync");
    }

    inline void Branch(uintptr_t addr, uintptr_t dest, bool link = false) {
        uint32_t delta = dest - addr;
        uint32_t insn = 0x48000000 | (delta & 0x03FFFFFC);
        if (link) {
            insn |= 1;
        }
        *(volatile uint32_t*)addr = insn;
        FlushCache(addr, 4);
    }

    template <typename Callback, typename Original>
    inline void InstallHook(uintptr_t target, Callback callback, Original* originalOut) {
        uint32_t* tramp = reinterpret_cast<uint32_t*>(AllocateTrampoline(8));
        if (!tramp) return;

        uint32_t origInsn = *(volatile uint32_t*)target;
        tramp[0] = origInsn;

        uint32_t returnAddr = target + 4;
        uint32_t trampAddr = reinterpret_cast<uintptr_t>(&tramp[1]);
        uint32_t delta = returnAddr - trampAddr;
        tramp[1] = 0x48000000 | (delta & 0x03FFFFFC);
        FlushCache(reinterpret_cast<uintptr_t>(tramp), 8);

        *originalOut = reinterpret_cast<Original>(tramp);

        uintptr_t cbAddr = reinterpret_cast<uintptr_t>(callback);
        if (cbAddr < 0x01000000) {
            cbAddr += g_CodeCaveBase;
        }
        Branch(target, cbAddr, false);
    }

} // namespace Backend

} // namespace WiiXLaunch
