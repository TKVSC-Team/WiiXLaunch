#include <wiixlaunch.hpp>

// ============================================================================
// WiiXLaunch Sample BotW Mod
// Featuring Full ExLaunch API Parity (Trampolines, Replacements & CodePatch)
// ============================================================================

// ----------------------------------------------------------------------------
// Style 1: ExLaunch Struct-based Syntax (HOOK_DEFINE_REPLACE + Orig(...))
// ----------------------------------------------------------------------------
HOOK_DEFINE_REPLACE(PlayerDamageHook) {
    static void Callback(void* playerActor, int32_t damageAmount) {
        // Reduced damage by 50%
        int32_t reducedDamage = damageAmount / 2;

        // ExLaunch-style Orig(...) call
        Orig(playerActor, reducedDamage);
    }
};

// ----------------------------------------------------------------------------
// Style 2: Functional Macro Syntax (WIIXL_HOOK_REPLACE + Original(...))
// ----------------------------------------------------------------------------
WIIXL_HOOK_REPLACE(StaminaConsumeHook, void, 0x0155AA80, 0x0288BB40, void* staminaCtrl, float consumeAmount) {
    // Infinite stamina
    Original(staminaCtrl, 0.0f);
}

// ----------------------------------------------------------------------------
// Plugin Entry Point
// ----------------------------------------------------------------------------
extern "C" void WiiXLaunch_Init() {
#if WIIXL_WIIU
    if (!WiiXLaunch::Backend::InitWiiUBackend()) {
        return;
    }
#endif

    // 1. Install Replacement & Trampoline Hooks
    PlayerDamageHook::Install(0x01234560, 0x02123456);
    StaminaConsumeHook::Install();

    // 2. Perform ExLaunch-style Code Patches (e.g. NOPing out a cheat check or fall damage)
    // NOP instruction at Switch 0x01990000 / Wii U 0x02990000
    WiiXLaunch::CodePatch::Nop(WIIXL_OFFSET(0x01990000, 0x02990000));
}
