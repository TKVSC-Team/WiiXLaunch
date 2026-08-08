// WUPS plugin glue - Wii U (Aroma) only. This is what makes the built .wps a
// loadable plugin: Aroma reads the WUPS_PLUGIN_* metadata sections and calls
// INITIALIZE_PLUGIN() at plugin load, which is our one entry into
// WiiXLaunch_Init(). Compiles to nothing on Switch/Cemu (their entrypoints are
// exl_main and WiiXLaunch_Cemu_Init respectively).
//
// Also hosts the on-console diagnostics: hook/patch state is shown both as
// Aroma toasts (best effort - the notification module may not be present) and
// as stub items in this plugin's config menu (L + D-Down + Minus), which works
// through the plugin loader itself and needs no extra modules.
#include <wiixlaunch/platform.hpp>

#if WIIXL_WIIU

#include <wups.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <notifications/notifications.h>
#include <wiixlaunch/generated_wiiu_config.hpp>
#include <wiixlaunch/wiiu/wiiu_backend.hpp>
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
    static char lines[6][96];
    snprintf(lines[0], sizeof(lines[0]), "FunctionPatcher init: %s",
             B::g_BackendInitOk ? "OK" : "FAILED");
    snprintf(lines[1], sizeof(lines[1]), "Hook patches: %lu ok / %lu failed%s%s",
             (unsigned long)B::g_PatchOkCount, (unsigned long)B::g_PatchFailCount,
             B::g_PatchFailCount ? " - " : "",
             B::g_PatchFailCount ? FunctionPatcher_GetStatusStr(B::g_LastPatchStatus) : "");
    snprintf(lines[2], sizeof(lines[2]), "Input hook fires: %lu",
             (unsigned long)B::g_InputHookFireCount);
    snprintf(lines[3], sizeof(lines[3]), "Camera hook fires: %lu",
             (unsigned long)B::g_CamHookFireCount);
    snprintf(lines[4], sizeof(lines[4]), "Freecam toggles: %lu",
             (unsigned long)B::g_FreecamToggleCount);
    snprintf(lines[5], sizeof(lines[5]), "Notification lib status: %d",
             (int)s_NotifyInitStatus);

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

// Fires on every title launch, in the launched title's process - the most
// reliable place for a visible toast (and re-inits the notification lib in
// case INITIALIZE_PLUGIN ran in a context where the module wasn't reachable).
ON_APPLICATION_START() {
    s_NotifyInitStatus = NotificationModule_InitLibrary();

    namespace B = WiiXLaunch::Backend;
    char msg[128];
    snprintf(msg, sizeof(msg), "WiiXLaunch: active (%lu hooks, init %s)",
             (unsigned long)B::g_PatchOkCount, B::g_BackendInitOk ? "ok" : "FAILED");
    NotificationModule_AddInfoNotification(msg);
}

#endif // WIIXL_WIIU
