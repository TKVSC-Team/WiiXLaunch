#pragma once

#include "../platform.hpp"

#if WIIXL_SWITCH

#include <lib.hpp>

namespace WiiXLaunch::Backend {

    inline bool InitSwitchBackend() {
        // ExLaunch handles its own initialization via Atmosphere subsdk entrypoint
        return true;
    }

    template<typename Ret, typename... Args>
    inline void InstallHook(uptr offset, Ret(*callback)(Args...), Ret(**outOriginal)(Args...)) {
        uptr targetAddr = exl::util::modules::GetTargetStart() + offset;
        auto orig = exl::hook::Hook(reinterpret_cast<void*>(targetAddr), reinterpret_cast<void*>(callback), outOriginal != nullptr);
        if (outOriginal) {
            *outOriginal = reinterpret_cast<Ret(*)(Args...)>(orig);
        }
    }

} // namespace WiiXLaunch::Backend

#endif // WIIXL_SWITCH

#if WIIXL_SWITCH
// Export ExLaunch entrypoints
extern "C" void WiiXLaunch_Init();

extern "C" void exl_main(void* x0, void* x1) {
    // Setup ExLaunch hooking environment
    exl::hook::Initialize();
    
    // Call the unified init function
    WiiXLaunch_Init();
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Default exception handler called!");
}
#endif
