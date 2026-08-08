#pragma once

#include "../platform.hpp"

#if WIIXL_SWITCH

#include <lib.hpp>

namespace WiiXLaunch::Backend {

    inline bool InitSwitchBackend() {
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

}

#endif

#if WIIXL_SWITCH

extern "C" void WiiXLaunch_Init();

extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();
    WiiXLaunch_Init();
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Default exception handler called!");
}
#endif
