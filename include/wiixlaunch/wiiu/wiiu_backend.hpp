#pragma once

#include "../platform.hpp"

#if WIIXL_WIIU

#include <wups.h>
#include <function_patcher/function_patching.h>
#include <coreinit/debug.h>
#include "../generated_wiiu_config.hpp"

namespace WiiXLaunch::Backend {

    // WiiXLaunch hook offsets are absolute virtual addresses as seen in
    // Cemu/Ghidra (e.g. 0x030d9f24). FunctionPatcherModule instead wants an
    // offset relative to the executable's .text start - it resolves the patch
    // site as rpl.textAddr + textOffset at load time. The main RPX's .text is
    // linked/loaded at 0x02000000, so subtract that here rather than making
    // every call site carry a different address space than Cemu's.
    constexpr uint32_t kRpxTextBase = 0x02000000;

    // Diagnostics surfaced as an Aroma toast by wiiu_plugin.cpp - there is no
    // other user-visible failure channel on console, and a hook that fails to
    // register is otherwise indistinguishable from one that never fires.
    inline bool g_BackendInitOk = false;
    inline uint32_t g_PatchOkCount = 0;
    inline uint32_t g_PatchFailCount = 0;
    inline FunctionPatcherStatus g_LastPatchStatus = FUNCTION_PATCHER_RESULT_SUCCESS;

    // Live counters incremented from the hooks in main.cpp, read by the
    // config-menu diagnostics in wiiu_plugin.cpp.
    inline uint32_t g_InputHookFireCount = 0;
    inline uint32_t g_CamHookFireCount = 0;
    inline uint32_t g_FreecamToggleCount = 0;

    inline bool InitWiiUBackend() {
        FunctionPatcherStatus status = FunctionPatcher_InitLibrary();
        g_BackendInitOk = (status == FUNCTION_PATCHER_RESULT_SUCCESS);
        return g_BackendInitOk;
    }

    inline bool AddPPCExecutablePatch(
        void* replacementFn,
        void** originalFnPtr,
        uint32_t textOffset,
        const uint64_t* targetTitleIds,
        uint32_t targetTitleIdsCount,
        // BOTW's executable - with no targetTitleIds the patch applies to any
        // title whose RPX has this name, and only BOTW ships U-King.rpx.
        const char* rpxName = "U-King.rpx"
    ) {
        // FunctionPatcherModule's shouldBePatched() does a plain
        // titleIds.contains(currentTitle) with NO empty-list bypass - a patch
        // registered with zero title ids is accepted but never applied to any
        // title. So an explicit list is mandatory; default to the generated
        // target list from wiixlaunch.json when the caller doesn't pass one.
        if (targetTitleIds == nullptr || targetTitleIdsCount == 0) {
            targetTitleIds = WiiXLaunch::WiiUConfig::TargetTitleIds;
            targetTitleIdsCount = WiiXLaunch::WiiUConfig::TargetTitleIdsCount;
        }
        function_replacement_data_t patchData = {
            .version       = FUNCTION_REPLACEMENT_DATA_STRUCT_VERSION,
            .type          = FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS,
            .physicalAddr  = 0,
            .virtualAddr   = 0,
            .replaceAddr   = reinterpret_cast<uint32_t>(replacementFn),
            .replaceCall   = reinterpret_cast<uint32_t*>(originalFnPtr),
            .targetProcess = FP_TARGET_PROCESS_ALL,
            .ReplaceInRPX  = {
                .targetTitleIds      = targetTitleIds,
                .targetTitleIdsCount = targetTitleIdsCount,
                .versionMin          = 0,
                .versionMax          = 0xFFFF,
                .executableName      = rpxName,
                .textOffset          = textOffset - kRpxTextBase,
                .functionName        = nullptr
            }
        };

        PatchedFunctionHandle handle;
        bool hasBeenPatched = false;
        FunctionPatcherStatus res = FunctionPatcher_AddFunctionPatch(&patchData, &handle, &hasBeenPatched);
        if (res == FUNCTION_PATCHER_RESULT_SUCCESS) {
            g_PatchOkCount++;
        } else {
            g_PatchFailCount++;
            g_LastPatchStatus = res;
        }
        return (res == FUNCTION_PATCHER_RESULT_SUCCESS);
    }

} // namespace WiiXLaunch::Backend

#endif // WIIXL_WIIU
