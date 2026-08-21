#include <wiixlaunch.hpp>
#include <wiixlaunch/botw/botw.hpp>

using namespace WiiXLaunch::BotW;

#if WIIXL_SWITCH
#include <testpic_texture_bytes.hpp>
#elif WIIXL_WIIU
#include <testpic_texture_bytes_gx2.hpp>
#endif

// feel free to remove this, its just proof your toolchain works end-end (line 45-67 can also be removed)
#if WIIXL_SWITCH
static NVN::TextureHandle g_LogoTexture = 0;

void OnRender(NVN::CommandBuffer* cmdBuf, void* dstTexture, int width, int height) {
    NVN::DrawSprite(cmdBuf, dstTexture, g_LogoTexture, -0.92f, 0.50f, 0.225f, 0.40f);
}
#elif WIIXL_CEMU
static GX2::TextureHandle g_LogoTexture = 0;

void OnRender(GX2::CommandBuffer* cmdBuf, void* dstTexture, int width, int height) {
    if (g_LogoTexture) {
        GX2::DrawSprite(cmdBuf, dstTexture, g_LogoTexture, -0.92f, 0.50f, 0.225f, 0.40f);
    }
}
#elif WIIXL_WIIU
static GX2::TextureHandle g_LogoTexture = 0;

void OnRender(GX2::CommandBuffer* cmdBuf, void* dstTexture, int width, int height) {
    GX2::DrawSprite(cmdBuf, dstTexture, g_LogoTexture, -0.92f, 0.50f, 0.225f, 0.40f);
}
#endif

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

    WIIXL_LOG("WiiXLaunch: init OK");

    // Proof the system clock is reachable: console RTC on hardware,
    // host PC clock under Cemu. Reads "unavailable" on Switch.
    char clock[20];
    WiiXLaunch::Time::FormatNow(clock, sizeof(clock));
    WIIXL_LOG("WiiXLaunch: system clock %s", clock);

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

// NO TOUCHING BELOW THIS POINT. This is Cemu-specific code that handles the trampoline pool and code cave entry point. You don't need to touch this unless you know what you're doing.

// Cemu code caves are position-independent, so the trampoline pool needs its
// own runtime base address. g_CodeCaveBase holds that offset once computed below.
extern "C" uintptr_t g_CodeCaveBase;
uintptr_t g_CodeCaveBase = 0;

#if WIIXL_CEMU
// Cemu's code cave entry point (hooks 0x03098928).
// Preserves all registers, calls WiiXLaunch_Init,
// restores all registers, executes replaced instruction, and branches to 0x0309892c.
asm(
    ".section .text.WiiXLaunch_Cemu_Init\n"
    ".global WiiXLaunch_Cemu_Init\n"
    "WiiXLaunch_Cemu_Init:\n"
    "mflr 0\n"
    "stwu 1, -0x2000(1)\n"
    "stw 0, 0x2004(1)\n"
    "mfcr 0\n"
    "stw 0, 0x2008(1)\n"
    "stmw 2, 0x1F80(1)\n"
    "bl WiiXLaunch_Init\n"
    "lmw 2, 0x1F80(1)\n"
    "lwz 0, 0x2008(1)\n"
    "mtcr 0\n"
    "lwz 0, 0x2004(1)\n"
    "mtlr 0\n"
    "addi 1, 1, 0x2000\n"
    "mflr 0\n"
    "lis 12, 0x0309\n"
    "ori 12, 12, 0x892c\n"
    "mtctr 12\n"
    "bctr\n"
);
#endif
