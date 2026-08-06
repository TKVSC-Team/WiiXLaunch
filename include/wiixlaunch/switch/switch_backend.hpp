#pragma once

#include "../platform.hpp"

#if WIIXL_SWITCH

#include <exlaunch.hpp>

namespace WiiXLaunch::Backend {

    inline bool InitSwitchBackend() {
        // ExLaunch handles its own initialization via Atmosphere subsdk entrypoint
        return true;
    }

    template<typename Ret, typename... Args>
    inline void InstallHook(uptr offset, Ret(*callback)(Args...), Ret(**outOriginal)(Args...)) {
        uptr targetAddr = exl::util::modules::GetTargetStart() + offset;
        exl::hook::hook(targetAddr, reinterpret_cast<void*>(callback), reinterpret_cast<void**>(outOriginal));
    }

} // namespace WiiXLaunch::Backend

#endif // WIIXL_SWITCH
