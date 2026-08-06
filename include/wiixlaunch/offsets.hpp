#pragma once

#include "platform.hpp"

// ---------------------------------------------------------
// Dual-Platform Offset Selection Macro
// ---------------------------------------------------------
#if WIIXL_SWITCH
    #define WIIXL_OFFSET(switch_offset, wiiu_offset) (switch_offset)
#else
    #define WIIXL_OFFSET(switch_offset, wiiu_offset) (wiiu_offset)
#endif
