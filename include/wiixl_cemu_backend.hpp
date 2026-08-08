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

    // A plain `b`/`ba` only has a 24-bit word-granular field: +-32MB reach for
    // a relative branch, or the low/high 32MB absolute windows for `ba`. The
    // codecave's address is chosen by Cemu/the graphic pack at runtime and can
    // land arbitrarily far from a given hook target - Branch()'s naive
    // `delta & 0x03FFFFFC` masking silently truncates an out-of-range delta
    // into a garbage target instead of failing loudly, which crashed Cemu's
    // recompiler the first time a hook target happened to be far enough away
    // (~40MB) to hit this. WriteLongJump has unlimited reach in both
    // directions since it loads the full 32-bit destination into r12 and
    // branches through CTR rather than encoding a relative/absolute offset.
    inline void WriteLongJump(uintptr_t addr, uintptr_t dest) {
        uint32_t hi = static_cast<uint32_t>(dest) >> 16;
        uint32_t lo = static_cast<uint32_t>(dest) & 0xFFFF;
        volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(addr);
        p[0] = 0x3D800000 | hi;  // lis  r12, hi
        p[1] = 0x618C0000 | lo;  // ori  r12, r12, lo
        p[2] = 0x7D8903A6;       // mtctr r12
        p[3] = 0x4E800420;       // bctr
        FlushCache(addr, 16);
    }

    template <typename Callback, typename Original>
    inline void InstallHook(uintptr_t target, Callback callback, Original* originalOut) {
        // Trampoline layout: 4 replayed original instructions, followed by a
        // 4-instruction long jump back to target+16 (past what we overwrote).
        // 4 instructions (not 1) are relocated so the overwritten region at
        // the hook site is big enough to hold our own 4-instruction long
        // jump - a real function's first 4 prologue instructions are never a
        // valid mid-function branch target, so relocating all 4 is safe.
        uint32_t* tramp = reinterpret_cast<uint32_t*>(AllocateTrampoline(8 * 4));
        if (!tramp) return;

        for (int i = 0; i < 4; i++) {
            tramp[i] = *(volatile uint32_t*)(target + i * 4);
        }
        FlushCache(reinterpret_cast<uintptr_t>(tramp), 16);

        WriteLongJump(reinterpret_cast<uintptr_t>(&tramp[4]), target + 16);

        *originalOut = reinterpret_cast<Original>(tramp);

        uintptr_t cbAddr = reinterpret_cast<uintptr_t>(callback);
        if (cbAddr < 0x01000000) {
            cbAddr += g_CodeCaveBase;
        }
        WriteLongJump(target, cbAddr);
    }

} // namespace Backend

} // namespace WiiXLaunch
