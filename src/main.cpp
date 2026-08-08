#include <wiixlaunch.hpp>
#include <cmath>

#if WIIXL_WIIU
// On-console diagnostics: Aroma toasts are the only user-visible output
// channel on real hardware (no console/log without extra setup).
#include <notifications/notifications.h>
#endif

static bool g_FreecamActive = false;
static bool g_CamInitialized = false;

static uint32_t g_ButtonsHold = 0;
static float g_LeftStickX = 0.0f;
static float g_LeftStickY = 0.0f;
static float g_RightStickX = 0.0f;
static float g_RightStickY = 0.0f;

#if WIIXL_CEMU
// Incremented once per VPADReadWrapperHook call so log entries from different
// hooks within the same frame can be correlated by the host-side reader.
// Cemu-only: see debug_log.hpp for why the logger itself is scoped there.
static uint32_t g_LogFrame = 0;
#endif

// Unified Button Masks
#if WIIXL_SWITCH
    constexpr uint32_t BTN_B     = 0x0001;
    constexpr uint32_t BTN_X     = 0x0008;
    constexpr uint32_t BTN_ZL    = 0x0080;
    constexpr uint32_t BTN_DDOWN = 0x0002;
#else
    // Wii U VPAD buttons (also used as the canonical/target bitspace for freecam logic)
    constexpr uint32_t BTN_B     = 0x4000;
    constexpr uint32_t BTN_X     = 0x2000;
    constexpr uint32_t BTN_ZL    = 0x0080;
    constexpr uint32_t BTN_DDOWN = 0x0100;

    // WPAD Pro Controller buttons (KPADStatus::pro.hold bitspace - different from VPAD!)
    constexpr uint32_t WPAD_PRO_B     = 0x0040;
    constexpr uint32_t WPAD_PRO_X     = 0x0008;
    constexpr uint32_t WPAD_PRO_ZL    = 0x0080;
    constexpr uint32_t WPAD_PRO_DDOWN = 0x4000;

    // Core WPAD (bare Wiimote) buttons (outer KPADStatus::hold bitspace - no ZL/ZR exist here)
    constexpr uint32_t WPAD_CORE_B     = 0x0400; // "B" trigger button
    constexpr uint32_t WPAD_CORE_A     = 0x0800; // used in place of X (no analog-adjacent button on core Wiimote)
    constexpr uint32_t WPAD_CORE_DDOWN = 0x0004;
#endif

// Translates a raw hold value from a given controller's native bitspace into
// the canonical VPAD-style bitspace that ProcessWiiUInput/freecam logic expects.
enum class PadSource { VPAD, WPAD_PRO, WPAD_CORE };

// TranslateToCanonical is only used for Wii U / Cemu builds. Exclude on Switch
#if !WIIXL_SWITCH
static uint32_t TranslateToCanonical(uint32_t rawHold, PadSource src) {
    uint32_t out = 0;
    switch (src) {
        case PadSource::VPAD:
            return rawHold; // already canonical
        case PadSource::WPAD_PRO:
            if (rawHold & WPAD_PRO_B)     out |= BTN_B;
            if (rawHold & WPAD_PRO_X)     out |= BTN_X;
            if (rawHold & WPAD_PRO_ZL)    out |= BTN_ZL;
            if (rawHold & WPAD_PRO_DDOWN) out |= BTN_DDOWN;
            return out;
        case PadSource::WPAD_CORE:
            // Core Wiimote has no ZL/ZR/analog trigger - no reliable combo modifier exists here.
            // Mapping B->BTN_B and A->BTN_X as a stand-in; combo toggle (ZL+DDown) is
            // effectively unreachable on a bare Wiimote and will need its own input path
            // if you want freecam toggle to work with one connected.
            if (rawHold & WPAD_CORE_B)     out |= BTN_B;
            if (rawHold & WPAD_CORE_A)     out |= BTN_X;
            if (rawHold & WPAD_CORE_DDOWN) out |= BTN_DDOWN;
            return out;
    }
    return rawHold;
}
#endif

// ----------------------------------------------------------------------------
// 1. Switch Input Hooks (nn::hid::GetNpadStates)
// ----------------------------------------------------------------------------
#if WIIXL_SWITCH
struct NpadState {
    int64_t updateCount;
    uint64_t Buttons;
    int32_t LStickX;
    int32_t LStickY;
    int32_t RStickX;
    int32_t RStickY;
    uint32_t Flags;
    uint32_t Reserved;
};

static void ProcessNpadState(NpadState* state) {
    if (!state) return;
    g_ButtonsHold = static_cast<uint32_t>(state->Buttons);

    float lx = static_cast<float>(state->LStickX) / 32767.0f;
    float ly = static_cast<float>(state->LStickY) / 32767.0f;
    float rx = static_cast<float>(state->RStickX) / 32767.0f;
    float ry = static_cast<float>(state->RStickY) / 32767.0f;

    g_LeftStickX  = (std::abs(lx) > 0.1f) ? lx : 0.0f;
    g_LeftStickY  = (std::abs(ly) > 0.1f) ? ly : 0.0f;
    g_RightStickX = (std::abs(rx) > 0.1f) ? rx : 0.0f;
    g_RightStickY = (std::abs(ry) > 0.1f) ? ry : 0.0f;

    static bool s_LastCombo = false;
    bool combo = ((state->Buttons & BTN_ZL) != 0) && ((state->Buttons & BTN_DDOWN) != 0);
    if (combo && !s_LastCombo) {
        g_FreecamActive = !g_FreecamActive;
        g_CamInitialized = false;
    }
    s_LastCombo = combo;
}

WIIXL_HOOK_DEFINE_TRAMPOLINE(GetNpadStatesHandheldHook) {
    static int Callback(void* stateArray, int count, const uint32_t& npadId) {
        int res = Orig(stateArray, count, npadId);
        if (res > 0 && npadId == 0x20) ProcessNpadState(static_cast<NpadState*>(stateArray));
        return res;
    }
};
WIIXL_HOOK_DEFINE_TRAMPOLINE(GetNpadStatesJoyDualHook) {
    static int Callback(void* stateArray, int count, const uint32_t& npadId) {
        int res = Orig(stateArray, count, npadId);
        if (res > 0 && npadId == 0x0) ProcessNpadState(static_cast<NpadState*>(stateArray));
        return res;
    }
};
WIIXL_HOOK_DEFINE_TRAMPOLINE(GetNpadStatesFullKeyHook) {
    static int Callback(void* stateArray, int count, const uint32_t& npadId) {
        int res = Orig(stateArray, count, npadId);
        if (res > 0 && npadId == 0x0) ProcessNpadState(static_cast<NpadState*>(stateArray));
        return res;
    }
};
#endif

#if WIIXL_WIIU || WIIXL_CEMU

extern "C" void Cemu_Log(const char* msg) {
}

extern "C" void Cemu_Log1(const char* fmt, uint32_t val) {
}

static void ProcessWiiUInput(uint32_t hold, float lx, float ly, float rx, float ry) {
    g_ButtonsHold = hold;

    rx = -rx;

    g_LeftStickX  = (std::abs(lx) > 0.1f) ? lx : 0.0f;
    g_LeftStickY  = (std::abs(ly) > 0.1f) ? ly : 0.0f;
    g_RightStickX = (std::abs(rx) > 0.1f) ? rx : 0.0f;
    g_RightStickY = (std::abs(ry) > 0.1f) ? ry : 0.0f;

    static bool s_LastCombo = false;
    bool combo = ((g_ButtonsHold & BTN_ZL) != 0) && ((g_ButtonsHold & BTN_DDOWN) != 0);
    if (combo && !s_LastCombo) {
        g_FreecamActive = !g_FreecamActive;
        g_CamInitialized = false;
#if WIIXL_WIIU
        WiiXLaunch::Backend::g_FreecamToggleCount++;
        NotificationModule_AddInfoNotification(g_FreecamActive ? "WiiXLaunch: Freecam ON" : "WiiXLaunch: Freecam OFF");
#endif
    }
    s_LastCombo = combo;
}

WIIXL_HOOK_DEFINE_TRAMPOLINE(VPADReadWrapperHook) {
    static void Callback(void* obj) {
        Orig(obj);
#if WIIXL_WIIU
        WiiXLaunch::Backend::g_InputHookFireCount++;
        static bool s_Announced = false;
        if (!s_Announced) {
            s_Announced = true;
            NotificationModule_AddInfoNotification("WiiXLaunch: input hook alive");
        }
#endif
#if WIIXL_CEMU
        g_LogFrame++;

        // Heartbeat: re-emit a PING every ~5s (assuming ~60fps) so the ring
        // buffer always has a fresh, easy-to-spot entry near the write cursor
        // regardless of when the host-side reader happens to attach - a
        // one-shot PING at init would almost always be evicted from the
        // 128-slot ring long before a human manually starts the reader tool.
        if (g_LogFrame % 300 == 0) {
            WIIXL_LOG(LOG_TAG_PING, g_LogFrame, 0, 0, 0, 0, 0xDEADBEEF);
        }
#endif

        uint8_t* vpad = static_cast<uint8_t*>(obj) + 0x14;
        uint32_t hold = *reinterpret_cast<uint32_t*>(vpad + 0x00);
        float lx = *reinterpret_cast<float*>(vpad + 0x0C);
        float ly = *reinterpret_cast<float*>(vpad + 0x10);
        float rx = *reinterpret_cast<float*>(vpad + 0x14);
        float ry = *reinterpret_cast<float*>(vpad + 0x18);
        ProcessWiiUInput(hold, lx, ly, rx, ry);
    }
};

WIIXL_HOOK_DEFINE_TRAMPOLINE(KPADReadExWrapperHook) {
    static void Callback(void* obj) {
        Orig(obj);
        for (int i = 0; i < 4; i++) {
            uint8_t* kpad = static_cast<uint8_t*>(obj) + 0x1118 + (i * 0xF08);
            uint32_t wii_hold = *reinterpret_cast<uint32_t*>(kpad + 0x00);
            // KPADStatus::pro union starts at 0x60 (hold@0x60, leftStick@0x6C, rightStick@0x74),
            // confirmed against wut's WUT_CHECK_OFFSET asserts in padscore/kpad.h.
            // 0x48/0x4C/0x50/0x54 are actually dist/distDiff/distDiffMagnitude/down - wrong fields entirely.
            uint32_t pro_hold = *reinterpret_cast<uint32_t*>(kpad + 0x60);
            
            if (pro_hold != 0) {
                float lx = *reinterpret_cast<float*>(kpad + 0x6C);
                float ly = *reinterpret_cast<float*>(kpad + 0x70);
                float rx = *reinterpret_cast<float*>(kpad + 0x74);
                float ry = *reinterpret_cast<float*>(kpad + 0x78);
                uint32_t canonicalHold = TranslateToCanonical(pro_hold, PadSource::WPAD_PRO);
                ProcessWiiUInput(canonicalHold, lx, ly, rx, ry);
                break;
            } else if (wii_hold != 0) {
                uint32_t canonicalHold = TranslateToCanonical(wii_hold, PadSource::WPAD_CORE);
                ProcessWiiUInput(canonicalHold, 0.0f, 0.0f, 0.0f, 0.0f);
                break;
            }
        }
    }
};
#endif

// ----------------------------------------------------------------------------
// 2. Camera Matrix Hook (`sead::LookAtCamera::doUpdateMatrix` / `FUN_0227d53c`)
// ----------------------------------------------------------------------------
#if WIIXL_SWITCH
WIIXL_HOOK_DEFINE_TRAMPOLINE(LookAtCameraHook) {
    static void Callback(void* camera, float* matrix) {
        if (!g_FreecamActive || !camera) {
            Orig(camera, matrix);
            return;
        }

        uint8_t* ptr = static_cast<uint8_t*>(camera);
        float* pos = reinterpret_cast<float*>(ptr + 0x38);
        float* at  = reinterpret_cast<float*>(ptr + 0x44);
        float* up  = reinterpret_cast<float*>(ptr + 0x50);

        static float s_Pos[3];
        static float s_At[3];
        static float s_Yaw = 0.0f;
        static float s_Pitch = 0.0f;

        if (!g_CamInitialized) {
            float dx = at[0] - pos[0];
            float dy = at[1] - pos[1];
            float dz = at[2] - pos[2];
            float hDist = std::sqrt(dx*dx + dz*dz);

            if (hDist < 0.1f || (pos[0] == 0.0f && pos[1] == 0.0f && pos[2] == 0.0f)) {
                Orig(camera, matrix);
                return;
            }

            s_Pos[0] = pos[0]; s_Pos[1] = pos[1]; s_Pos[2] = pos[2];
            s_At[0]  = at[0];  s_At[1]  = at[1];  s_At[2]  = at[2];
            s_Yaw = std::atan2(dx, dz);
            s_Pitch = std::atan2(dy, hDist);

            g_CamInitialized = true;
        }

        const float SENSITIVITY_LOOK = 0.0125f;
        const float SENSITIVITY_MOVE = 1.0f;

        s_Yaw   += g_RightStickX * SENSITIVITY_LOOK;
        s_Pitch += g_RightStickY * SENSITIVITY_LOOK;

        if (s_Pitch >  1.57f) s_Pitch =  1.57f;
        if (s_Pitch < -1.57f) s_Pitch = -1.57f;

        float forward[3] = {
            std::sin(s_Yaw) * std::cos(s_Pitch),
            std::sin(s_Pitch),
            std::cos(s_Yaw) * std::cos(s_Pitch)
        };

        float right[3] = {
            std::sin(s_Yaw - 1.570796f),
            0.0f,
            std::cos(s_Yaw - 1.570796f)
        };

        s_Pos[0] += (forward[0] * g_LeftStickY + right[0] * g_LeftStickX) * SENSITIVITY_MOVE;
        s_Pos[1] += (forward[1] * g_LeftStickY + right[1] * g_LeftStickX) * SENSITIVITY_MOVE;
        s_Pos[2] += (forward[2] * g_LeftStickY + right[2] * g_LeftStickX) * SENSITIVITY_MOVE;
        
        if (g_ButtonsHold & BTN_B) s_Pos[1] -= SENSITIVITY_MOVE;
        if (g_ButtonsHold & BTN_X) s_Pos[1] += SENSITIVITY_MOVE;

        s_At[0] = s_Pos[0] + forward[0];
        s_At[1] = s_Pos[1] + forward[1];
        s_At[2] = s_Pos[2] + forward[2];

        pos[0] = s_Pos[0]; pos[1] = s_Pos[1]; pos[2] = s_Pos[2];
        at[0]  = s_At[0];  at[1]  = s_At[1];  at[2]  = s_At[2];
        
        up[0] = 0.0f;
        up[1] = 1.0f;
        up[2] = 0.0f;

        Orig(camera, matrix);
    }
};
#else
WIIXL_HOOK_DEFINE_TRAMPOLINE(LookAtCameraHook) {

    static void Callback(void* cameraController, float* outMatrix) {
#if WIIXL_WIIU
        WiiXLaunch::Backend::g_CamHookFireCount++;
        static bool s_Announced = false;
        if (!s_Announced) {
            s_Announced = true;
            NotificationModule_AddInfoNotification("WiiXLaunch: camera hook alive");
        }
#endif
        WIIXL_LOG(LOG_TAG_CAM_ENTRY, g_LogFrame, 0, 0, 0, 0,
                  static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cameraController)));

        static void* s_KnownCameraPtr = nullptr;

        uintptr_t ccAddr = reinterpret_cast<uintptr_t>(cameraController);
        uintptr_t omAddr = reinterpret_cast<uintptr_t>(outMatrix);
        bool looksValid = ccAddr >= 0x10000000 && ccAddr < 0xC0000000 &&
                           omAddr >= 0x10000000 && omAddr < 0xC0000000;

        // if (!cameraController || !looksValid) {
        //     WIIXL_LOG(LOG_TAG_CAM_SKIP_ORIG, g_LogFrame, 0, 0, 0, 0, cameraController ? 2 : 1);
        //     return;
        // }

        if (!g_FreecamActive) {
            WIIXL_LOG(LOG_TAG_CAM_SKIP_ORIG, g_LogFrame, 0, 0, 0, 0, 0);
            Orig(cameraController, outMatrix);
            return;
        }

        // if (s_KnownCameraPtr && cameraController != s_KnownCameraPtr) {
        //     WIIXL_LOG(LOG_TAG_CAM_SKIP_ORIG, g_LogFrame, 0, 0, 0, 0, 3);
        //     Orig(cameraController, outMatrix);
        //     return;
        // }

        uint8_t* ptr = reinterpret_cast<uint8_t*>(cameraController);
        float* pos = reinterpret_cast<float*>(ptr + 0x34); // mPos
        float* at  = reinterpret_cast<float*>(ptr + 0x40); // mAt
        float* up  = reinterpret_cast<float*>(ptr + 0x4C); // mUp

        WIIXL_LOG(LOG_TAG_CAM_DEREF, g_LogFrame, pos[0], pos[1], pos[2], 0, 0);
        WIIXL_LOG(LOG_TAG_CAM_DEREF, g_LogFrame, at[0],  at[1],  at[2],  0, 1);
        WIIXL_LOG(LOG_TAG_CAM_DEREF, g_LogFrame, up[0],  up[1],  up[2],  0, 2);

        float posMag = std::sqrt(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]);
        float dx0 = at[0] - pos[0], dy0 = at[1] - pos[1], dz0 = at[2] - pos[2];
        float lookDist = std::sqrt(dx0*dx0 + dy0*dy0 + dz0*dz0);
        float upLenSq = up[0]*up[0] + up[1]*up[1] + up[2]*up[2];
        bool geometryLooksReal =
            std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
            std::isfinite(at[0])  && std::isfinite(at[1])  && std::isfinite(at[2])  &&
            posMag > 20.0f &&
            std::isfinite(up[0])  && std::isfinite(up[1])  && std::isfinite(up[2])  &&
            lookDist > 0.05f && lookDist < 500.0f &&
            upLenSq > 0.5f && upLenSq < 2.0f;


        // if (!geometryLooksReal) {
        //     WIIXL_LOG(LOG_TAG_CAM_SKIP_ORIG, g_LogFrame, lookDist, 0, 0, 0, 4);
        //     return;
        // }

        if (!s_KnownCameraPtr) {
            s_KnownCameraPtr = cameraController; // lock on - this one's real
        }

        static float s_Pos[3];
        static float s_At[3];
        static float s_Yaw = 0.0f;
        static float s_Pitch = 0.0f;

        if (!g_CamInitialized) {
            float dx = at[0] - pos[0];
            float dy = at[1] - pos[1];
            float dz = at[2] - pos[2];
            float hDist = std::sqrt(dx*dx + dz*dz);

            if (hDist < 0.1f || (pos[0] == 0.0f && pos[1] == 0.0f && pos[2] == 0.0f)) {
                Orig(cameraController, outMatrix);
                return;
            }

            s_Pos[0] = pos[0]; s_Pos[1] = pos[1]; s_Pos[2] = pos[2];
            s_At[0]  = at[0];  s_At[1]  = at[1];  s_At[2]  = at[2];
            s_Yaw = std::atan2(dx, dz);
            s_Pitch = std::atan2(dy, hDist);

            g_CamInitialized = true;
            WIIXL_LOG(LOG_TAG_CAM_INIT, g_LogFrame, hDist, s_Yaw, s_Pitch, 0, 0);
        }

        const float SENSITIVITY_LOOK = 0.003125;
        const float SENSITIVITY_MOVE = 0.25f;

        s_Yaw   += g_RightStickX * SENSITIVITY_LOOK;
        s_Pitch += g_RightStickY * SENSITIVITY_LOOK;

        if (s_Pitch >  1.57f) s_Pitch =  1.57f;
        if (s_Pitch < -1.57f) s_Pitch = -1.57f;

        float forward[3] = {
            std::sin(s_Yaw) * std::cos(s_Pitch),
            std::sin(s_Pitch),
            std::cos(s_Yaw) * std::cos(s_Pitch)
        };

        float right[3] = {
            std::sin(s_Yaw - 1.570796f),
            0.0f,
            std::cos(s_Yaw - 1.570796f)
        };

        s_Pos[0] += (forward[0] * g_LeftStickY + right[0] * g_LeftStickX) * SENSITIVITY_MOVE;
        s_Pos[1] += (forward[1] * g_LeftStickY + right[1] * g_LeftStickX) * SENSITIVITY_MOVE;
        s_Pos[2] += (forward[2] * g_LeftStickY + right[2] * g_LeftStickX) * SENSITIVITY_MOVE;

        if (g_ButtonsHold & BTN_B) s_Pos[1] -= SENSITIVITY_MOVE;
        if (g_ButtonsHold & BTN_X) s_Pos[1] += SENSITIVITY_MOVE;

        s_At[0] = s_Pos[0] + forward[0];
        s_At[1] = s_Pos[1] + forward[1];
        s_At[2] = s_Pos[2] + forward[2];

        WIIXL_LOG(LOG_TAG_CAM_PRE_WRITE, g_LogFrame, s_Pos[0], s_Pos[1], s_Pos[2], 0, 0);
        WIIXL_LOG(LOG_TAG_CAM_PRE_WRITE, g_LogFrame, s_At[0],  s_At[1],  s_At[2],  0, 1);

        pos[0] = s_Pos[0]; pos[1] = s_Pos[1]; pos[2] = s_Pos[2];
        at[0]  = s_At[0];  at[1]  = s_At[1];  at[2]  = s_At[2];

        up[0] = 0.0f;
        up[1] = 1.0f;
        up[2] = 0.0f;

        Orig(cameraController, outMatrix);

        // Read back post-Orig: this now runs the REAL doUpdateMatrix, so
        // pos/at/up themselves aren't touched by it (it only reads them to
        // build outMatrix) - this confirms nothing else clobbers them first.
        WIIXL_LOG(LOG_TAG_CAM_POST_ORIG, g_LogFrame, pos[0], pos[1], pos[2], 0, 0);
        WIIXL_LOG(LOG_TAG_CAM_POST_ORIG, g_LogFrame, at[0],  at[1],  at[2],  0, 1);
        WIIXL_LOG(LOG_TAG_CAM_POST_ORIG, g_LogFrame, up[0],  up[1],  up[2],  0, 2);
    }
};
#endif

extern "C" void WiiXLaunch_Init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

#if WIIXL_WIIU
    if (!WiiXLaunch::Backend::InitWiiUBackend()) return;
#elif WIIXL_CEMU
    if (!WiiXLaunch::Backend::InitCemuBackend()) return;
#endif

#if WIIXL_WIIU || WIIXL_CEMU
    VPADReadWrapperHook::Install(0, 0x030d9f24);
    KPADReadExWrapperHook::Install(0, 0x030da168);
    // FUN_030bf280 = the real doUpdateMatrix equivalent, confirmed via Ghidra:
    // matches Switch's sead::LookAtCamera::doUpdateMatrix algorithmic shape
    // exactly (2 normalize passes, 2 cross products, 12-float matrix output),
    // reached via real per-frame virtual dispatch (vtable 0x1028BFC0 slot
    // +0x24, called from FUN_02c4ac1c -> FUN_0318ff5c each frame).
    LookAtCameraHook::Install(0, 0x030bf280);

    // Trivial test entry - if the host-side reader tool sees this, the ring
    // buffer pipeline works end-to-end before we rely on it for anything else.
    WIIXL_LOG(LOG_TAG_PING, 0, 1.0f, 2.0f, 3.0f, 4.0f, 0xDEADBEEF);
#endif

#if WIIXL_SWITCH
    GetNpadStatesHandheldHook::Install(0x01800dd0, 0);
    GetNpadStatesJoyDualHook::Install(0x01800de0, 0);
    GetNpadStatesFullKeyHook::Install(0x01800df0, 0);
    LookAtCameraHook::Install(0x00b1be7c, 0);
#endif
}

extern "C" uintptr_t g_CodeCaveBase;
uintptr_t g_CodeCaveBase = 0;

#if WIIXL_CEMU
asm(
    ".section .text.WiiXLaunch_Cemu_Init\n"
    ".global WiiXLaunch_Cemu_Init\n"
    "WiiXLaunch_Cemu_Init:\n"
    "stwu 1, -0x80(1)\n"
    "mflr 0\n"
    "stw 0, 0x84(1)\n"
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
    "bl 1f\n"
    "1: mflr 3\n"
    "lis 4, 1b@h\n"
    "ori 4, 4, 1b@l\n"
    "subf 3, 4, 3\n"
    "lis 4, g_CodeCaveBase@h\n"
    "ori 4, 4, g_CodeCaveBase@l\n"
    "add 4, 4, 3\n"
    "stw 3, 0(4)\n"
    "bl WiiXLaunch_Init\n"
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
    "mfspr 0, 8\n"
    "lis 12, 0x0309\n"
    "ori 12, 12, 0x892C\n"
    "mtctr 12\n"
    "bctr\n"
);
#endif
