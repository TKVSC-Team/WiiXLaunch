#pragma once

#include <cstddef>

#define EXL_MODULE_NAME "BotW_SampleMod"
#define EXL_DEBUG
#define EXL_USE_FAKEHEAP

namespace WiiXLaunch::Setting {
    constexpr size_t HeapSize       = 0x50000;
    constexpr size_t JitSize        = 0x10000;
    constexpr size_t InlinePoolSize = 0x10000;
    constexpr size_t LogBufferSize  = 512;
}

namespace exl::setting {
    constexpr size_t HeapSize       = 0x50000;
    constexpr size_t JitSize        = 0x10000;
    constexpr size_t InlinePoolSize = 0x10000;
    constexpr size_t LogBufferSize  = 512;
}
