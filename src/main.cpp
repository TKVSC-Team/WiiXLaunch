#include <wiixlaunch.hpp>
#include <wiixlaunch/botw/botw.hpp>

using namespace WiiXLaunch::BotW;

// Entry point called once at plugin/module load. Install your hooks here.
extern "C" void WiiXLaunch_Init() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

#if WIIXL_WIIU
    if (!WiiXLaunch::Backend::InitWiiUBackend()) return;
#elif WIIXL_CEMU
    if (!WiiXLaunch::Backend::InitCemuBackend()) return;
#endif

    // Confirms the debug log pipeline works end-to-end on whichever platform this is.
    WIIXL_LOG("WiiXLaunch: init OK");

#if WIIXL_SWITCH
    NVN::Init();
    NVN::RegisterDrawCallback(OnRender);
    NVN::OnInitialized([]() {
        g_LogoTexture = NVN::CreateTexture(g_TestPicTextureBytes, kTestPicTextureSize);
        WIIXL_LOG("WiiXLaunch: NVN logo texture initialized: %p", reinterpret_cast<void*>(g_LogoTexture));
    });
#elif WIIXL_CEMU
    GX2::Init();
    GX2::RegisterDrawCallback(OnRender);
    GX2::OnInitialized([]() {
        g_LogoTexture = GX2::LoadTexture("WiiXLaunch/logo.bin");
    });
// Would probably work on WUH, but I really can't be bothered unless someone finds a use for it. All you probably have to do is give it a proper texture path.
// #elif WIIXL_WIIU
//     GX2::Init();
//     GX2::RegisterDrawCallback(OnRender);
//     GX2::OnInitialized([]() {
//         g_LogoTexture = GX2::CreateTexture(g_TestpicTextureBytes, kTestpicTextureSize, kTestpicTextureWidth, kTestpicTextureHeight);
//         WIIXL_LOG("WiiXLaunch: GX2 logo texture initialized: %p", reinterpret_cast<void*>(g_LogoTexture));
//     });
#endif
}

// Cemu code caves are position-independent, so the trampoline pool needs its
// own runtime base address. g_CodeCaveBase holds that offset once computed below.
extern "C" uintptr_t g_CodeCaveBase;
uintptr_t g_CodeCaveBase = 0;

#if WIIXL_CEMU
// Cemu's code cave entry point. Computes g_CodeCaveBase from the current
// instruction pointer, then calls WiiXLaunch_Init with all registers preserved.
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
