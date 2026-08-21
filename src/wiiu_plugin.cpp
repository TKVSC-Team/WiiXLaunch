// WUPS plugin glue - Wii U (Aroma) only. This is what makes the built .wps a
// loadable plugin: Aroma reads the WUPS_PLUGIN_* metadata sections and calls
// INITIALIZE_PLUGIN() at plugin load, which is our one entry into
// WiiXLaunch_Init().
#include <wiixlaunch/platform.hpp>

#if WIIXL_WIIU

#include <wups.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <notifications/notifications.h>
#include <wiixlaunch/generated_wiiu_config.hpp>
#include <wiixlaunch/wiiu/wiiu_backend.hpp>
#include <wiixlaunch/time.hpp>
#include <cstdio>

WUPS_PLUGIN_NAME(WUPS_PLUGIN_NAME_STR);
WUPS_PLUGIN_DESCRIPTION(WUPS_PLUGIN_DESCRIPTION_STR);
WUPS_PLUGIN_VERSION(WUPS_PLUGIN_VERSION_STR);
WUPS_PLUGIN_AUTHOR(WUPS_PLUGIN_AUTHOR_STR);
WUPS_PLUGIN_LICENSE("GPLv3");

extern "C" void WiiXLaunch_Init();

static NotificationModuleStatus s_NotifyInitStatus = NOTIFICATION_MODULE_RESULT_LIB_UNINITIALIZED;
static WUPSConfigAPIStatus s_ConfigInitStatus = WUPSCONFIG_API_RESULT_LIB_UNINITIALIZED;

static WUPSConfigAPICallbackStatus ConfigMenuOpened(WUPSConfigCategoryHandle root) {
    namespace B = WiiXLaunch::Backend;

    // Static buffers: the config item may keep the pointer until the menu
    // closes, so these must outlive this callback.
    static char lines[4][96];
    snprintf(lines[0], sizeof(lines[0]), "FunctionPatcher init: %s",
             B::g_BackendInitOk ? "OK" : "FAILED");
    snprintf(lines[1], sizeof(lines[1]), "Hook patches: %lu ok / %lu failed%s%s",
             (unsigned long)B::g_PatchOkCount, (unsigned long)B::g_PatchFailCount,
             B::g_PatchFailCount ? " - " : "",
             B::g_PatchFailCount ? FunctionPatcher_GetStatusStr(B::g_LastPatchStatus) : "");
    snprintf(lines[2], sizeof(lines[2]), "Notification lib status: %d",
             (int)s_NotifyInitStatus);
    // Re-read on every menu open, so this doubles as a liveness check on
    // the console RTC rather than a value frozen at plugin load.
    char clock[20];
    WiiXLaunch::Time::FormatNow(clock, sizeof(clock));
    snprintf(lines[3], sizeof(lines[3]), "System clock: %s", clock);

    for (auto& line : lines) {
        WUPSConfigItemStub_AddToCategory(root, line);
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void ConfigMenuClosed() {
}

INITIALIZE_PLUGIN() {
    s_NotifyInitStatus = NotificationModule_InitLibrary();

    WUPSConfigAPIOptionsV1 configOptions = {.name = WUPS_PLUGIN_NAME_STR};
    s_ConfigInitStatus = WUPSConfigAPI_Init(configOptions, ConfigMenuOpened, ConfigMenuClosed);

    WiiXLaunch_Init();

    namespace B = WiiXLaunch::Backend;
    char msg[128];
    if (!B::g_BackendInitOk) {
        NotificationModule_AddErrorNotification("WiiXLaunch: FunctionPatcher init FAILED");
    } else if (B::g_PatchFailCount > 0) {
        snprintf(msg, sizeof(msg), "WiiXLaunch: %lu hooks ok, %lu FAILED (%s)",
                 (unsigned long)B::g_PatchOkCount, (unsigned long)B::g_PatchFailCount,
                 FunctionPatcher_GetStatusStr(B::g_LastPatchStatus));
        NotificationModule_AddErrorNotification(msg);
    } else {
        snprintf(msg, sizeof(msg), "WiiXLaunch: %lu hooks registered",
                 (unsigned long)B::g_PatchOkCount);
        NotificationModule_AddInfoNotification(msg);
    }
}

ON_APPLICATION_START() {
    s_NotifyInitStatus = NotificationModule_InitLibrary();

    namespace B = WiiXLaunch::Backend;
    char msg[128];
    snprintf(msg, sizeof(msg), "WiiXLaunch: active (%lu hooks, init %s)",
             (unsigned long)B::g_PatchOkCount, B::g_BackendInitOk ? "ok" : "FAILED");
    NotificationModule_AddInfoNotification(msg);
}

#endif
