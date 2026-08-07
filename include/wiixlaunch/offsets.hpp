#pragma once

#include "platform.hpp"

#if WIIXL_SWITCH
    #define WIIXL_OFFSET(switch_offset, wiiu_offset) (switch_offset)
#else
    #define WIIXL_OFFSET(switch_offset, wiiu_offset) (wiiu_offset)
#endif
