#pragma once

#include <utility>
#include "platform.hpp"
#include "offsets.hpp"
#include "context.hpp"

#if WIIXL_SWITCH
    #include "switch/switch_backend.hpp"
#elif WIIXL_WIIU
    #include "wiiu/wiiu_backend.hpp"
#elif WIIXL_CEMU
    #include "wiixl_cemu_backend.hpp"
#endif

#if WIIXL_SWITCH

#define WIIXL_HOOK_REPLACE(HookName, RetType, SwitchOffset, WiiUOffset, ...) \
    struct HookName { \
        static constexpr ::WiiXLaunch::uptr TargetOffset = WIIXL_OFFSET(SwitchOffset, WiiUOffset); \
        static RetType (*Original)(__VA_ARGS__); \
        static RetType Callback(__VA_ARGS__); \
        static void Install() { \
            ::WiiXLaunch::Backend::InstallHook(TargetOffset, &Callback, &Original); \
        } \
    }; \
    RetType (*HookName::Original)(__VA_ARGS__) = nullptr; \
    RetType HookName::Callback(__VA_ARGS__)

#elif WIIXL_WIIU

#define WIIXL_HOOK_REPLACE(HookName, RetType, SwitchOffset, WiiUOffset, ...) \
    struct HookName { \
        static constexpr ::WiiXLaunch::uptr TargetOffset = WIIXL_OFFSET(SwitchOffset, WiiUOffset); \
        static RetType (*Original)(__VA_ARGS__); \
        static RetType Callback(__VA_ARGS__); \
        static void Install(const uint64_t* titleIds = nullptr, uint32_t count = 0) { \
            ::WiiXLaunch::Backend::AddPPCExecutablePatch( \
                reinterpret_cast<void*>(&Callback), \
                reinterpret_cast<void**>(&Original), \
                TargetOffset, \
                titleIds, \
                count \
            ); \
        } \
    }; \
    RetType (*HookName::Original)(__VA_ARGS__) = nullptr; \
    RetType HookName::Callback(__VA_ARGS__)

#elif WIIXL_CEMU

#define WIIXL_HOOK_REPLACE(HookName, RetType, SwitchOffset, WiiUOffset, ...) \
    struct HookName { \
        static constexpr ::WiiXLaunch::uptr TargetOffset = WIIXL_OFFSET(SwitchOffset, WiiUOffset); \
        static RetType (*Original)(__VA_ARGS__); \
        static RetType Callback(__VA_ARGS__); \
        static void Install() { \
            ::WiiXLaunch::Backend::InstallHook(TargetOffset, reinterpret_cast<void*>(&Callback), reinterpret_cast<void**>(&Original)); \
        } \
    }; \
    RetType (*HookName::Original)(__VA_ARGS__) = nullptr; \
    RetType HookName::Callback(__VA_ARGS__)

#endif

#define WIIXL_HOOK_DEFINE_REPLACE(name) \
    struct name : public ::WiiXLaunch::impl::ReplaceHookBase<name>

#define WIIXL_HOOK_DEFINE_TRAMPOLINE(name) \
    struct name : public ::WiiXLaunch::impl::TrampolineHookBase<name>

namespace WiiXLaunch::impl {

    template<typename Derived>
    class ReplaceHookBase {
    public:
        template<typename T = Derived>
        using CallbackFuncPtr = decltype(&T::Callback);

        static auto& OrigRef() {
            static CallbackFuncPtr<> s_FnPtr = nullptr;
            return s_FnPtr;
        }

        template<typename... Args>
        static decltype(auto) Orig(Args&&... args) {
            return OrigRef()(std::forward<Args>(args)...);
        }

        static void Install(uptr switchOffset, uptr wiiuOffset) {
            uptr targetOffset = WIIXL_OFFSET(switchOffset, wiiuOffset);
#if WIIXL_SWITCH
            ::WiiXLaunch::Backend::InstallHook(targetOffset, &Derived::Callback, &OrigRef());
#elif WIIXL_WIIU
            ::WiiXLaunch::Backend::AddPPCExecutablePatch(
                reinterpret_cast<void*>(&Derived::Callback),
                reinterpret_cast<void**>(&OrigRef()),
                targetOffset,
                nullptr, 0
            );
#elif WIIXL_CEMU
            ::WiiXLaunch::Backend::InstallHook(targetOffset, &Derived::Callback, &OrigRef());
#endif
        }
    };

    template<typename Derived>
    class TrampolineHookBase : public ReplaceHookBase<Derived> {};

} // namespace WiiXLaunch::impl
