#include <wiixlaunch.hpp>

// ============================================================================
// WiiXLaunch - Universal Cross-Platform Mod Template
// ============================================================================
// Supported Platforms:
//   - Nintendo Switch (Atmosphère / Ryujinx / Yuzu)
//   - Nintendo Wii U (Aroma / Real Hardware)
//   - Cemu PC Emulator (Graphic Pack Bare-Metal Code Cave)
// ============================================================================

// ----------------------------------------------------------------------------
// EXAMPLE 1: Struct-based Trampoline Hook (Recommended)
// ----------------------------------------------------------------------------
// Define hooks using the WIIXL_HOOK_DEFINE_TRAMPOLINE macro.
// Inside Callback(), use Orig(...) to call the target game function.
WIIXL_HOOK_DEFINE_TRAMPOLINE(SampleTrampolineHook) {
    static int Callback(int arg1, float arg2) {
        // Perform custom logic before calling original function...
        
        // Call the original game function:
        int result = Orig(arg1, arg2);
        
        // Perform custom logic after calling original function...
        return result;
    }
};

// ----------------------------------------------------------------------------
// EXAMPLE 2: Functional Macro Hooking Syntax
// ----------------------------------------------------------------------------
// Alternative inline macro syntax:
// WIIXL_HOOK_REPLACE(HookName, ReturnType, SwitchOffset, WiiUOffset, Arguments...)
WIIXL_HOOK_REPLACE(SampleMacroHook, void, 0x00000000, 0x00000000, void* self) {
    // Custom logic here:
    SampleMacroHook::Original(self);
}

// ----------------------------------------------------------------------------
// Universal Initialization Routine
// ----------------------------------------------------------------------------
// Called automatically on plugin startup across all target platforms.
extern "C" void WiiXLaunch_Init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

#if WIIXL_WIIU
    if (!WiiXLaunch::Backend::InitWiiUBackend()) {
        return;
    }
#endif

    // 1. Install Trampoline Hooks (Specify SwitchOffset, WiiUOffset):
    // SampleTrampolineHook::Install(0x00885bd0, 0x02d908b4);

    // 2. Install Functional Macro Hooks:
    // SampleMacroHook::Install();

    // 3. Perform Direct Memory / Instruction Patches (Nop / Write):
    // WiiXLaunch::CodePatch::Nop(WIIXL_OFFSET(0x00885bd0, 0x02d908b4));
    // WiiXLaunch::CodePatch::WriteValue<uint32_t>(WIIXL_OFFSET(0x00885bd0, 0x02d908b4), 0x60000000);
}

// ----------------------------------------------------------------------------
// Cemu Code Cave Boot Hook (System Boilerplate - Do not modify)
// ----------------------------------------------------------------------------
extern "C" uintptr_t g_CodeCaveBase;
uintptr_t g_CodeCaveBase = 0;

// It works, I will not remember how, this was incredibly painful. But it works (BotW v208)
#if WIIXL_CEMU
asm(
    ".section .text.WiiXLaunch_Cemu_Init\n"
    ".global WiiXLaunch_Cemu_Init\n"
    "WiiXLaunch_Cemu_Init:\n"
    "stwu 1, -0x80(1)\n"
    "mflr 0\n"
    "stw 0, 0x84(1)\n"
    
    // Save volatile registers in local stack area
    "stw 3, 0x40(1)\n"
    "stw 4, 0x44(1)\n"
    "stw 5, 0x48(1)\n"
    "stw 6, 0x4C(1)\n"
    "stw 7, 0x50(1)\n"
    "stw 8, 0x54(1)\n"
    "stw 9, 0x58(1)\n"
    "stw 10, 0x5C(1)\n"
    "stw 11, 0x60(1)\n"
    "stw 12, 0x64(1)\n"
    
    // Dynamically calculate and store Code Cave base address
    "bl 1f\n"
    "1: mflr 3\n"
    "lis 4, 1b@h\n"
    "ori 4, 4, 1b@l\n"
    "subf 3, 4, 3\n"
    
    "lis 4, g_CodeCaveBase@h\n"
    "ori 4, 4, g_CodeCaveBase@l\n"
    "add 4, 4, 3\n"
    "stw 3, 0(4)\n"
    
    // Call main C++ initialization
    "bl WiiXLaunch_Init\n"
    
    // Restore volatile registers
    "lwz 3, 0x40(1)\n"
    "lwz 4, 0x44(1)\n"
    "lwz 5, 0x48(1)\n"
    "lwz 6, 0x4C(1)\n"
    "lwz 7, 0x50(1)\n"
    "lwz 8, 0x54(1)\n"
    "lwz 9, 0x58(1)\n"
    "lwz 10, 0x5C(1)\n"
    "lwz 11, 0x60(1)\n"
    "lwz 12, 0x64(1)\n"
    
    "lwz 0, 0x84(1)\n"
    "mtlr 0\n"
    "addi 1, 1, 0x80\n"
    
    // Execute replaced original instruction (mfspr r0, LR)
    "mfspr 0, 8\n"
    
    // Branch back to FSInit continuation (0x0309892C)
    "lis 12, 0x0309\n"
    "ori 12, 12, 0x892C\n"
    "mtctr 12\n"
    "bctr\n"
);
#endif

