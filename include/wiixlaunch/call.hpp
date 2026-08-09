#pragma once

#include "platform.hpp"
#include "offsets.hpp"

#if WIIXL_SWITCH
#include <lib.hpp>
#endif

// Resolving an offset you found in Ghidra into an address you can actually
// call/read/write at runtime, cross-platform (the same resolution every hook)
// Install() already does internally, exposed for when you want to reach into
// the game directly instead of hooking it (e.g. calling a getter function).
//
// Switch NSOs are relocated to a random base every launch, so a Switch offset
// (a Ghidra address minus 0x7100000000) needs that base added back at runtime.
// Wii U/Cemu RPXs load at a fixed, known address (see kRpxTextBase in
// wiiu_backend.hpp), so a Wii U offset from Ghidra's RPX loader plugin is
// already the final, absolute, callable address, and
// Cemu reuses the Wii U offset for the same reason hook Install() does.

namespace WiiXLaunch {

inline uptr ResolveTarget(uptr offset) {
#if WIIXL_SWITCH
    return exl::util::modules::GetTargetStart() + offset;
#else
    return offset;
#endif
}

// Resolves a WIIXL_OFFSET-style (switchOffset, wiiuOffset) pair into a
// callable function pointer of the given type.
//
// Usage:
//   using GetUniqueNameFn = const char* (*)(void* actor);
//   auto getUniqueName = WiiXLaunch::GetTargetFunction<GetUniqueNameFn>(0x11c9bfc, 0x0);
//   const char* name = getUniqueName(actor);
template<typename FnPtr>
inline FnPtr GetTargetFunction(uptr switchOffset, uptr wiiuOffset) {
    return reinterpret_cast<FnPtr>(ResolveTarget(WIIXL_OFFSET(switchOffset, wiiuOffset)));
}

}
