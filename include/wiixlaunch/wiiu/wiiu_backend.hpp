#pragma once

#include "../platform.hpp"

#if WIIXL_WIIU

#include <wups.h>
#include <function_patcher/function_patching.h>
#include <coreinit/debug.h>

namespace WiiXLaunch::Backend {

    inline bool InitWiiUBackend() {
        FunctionPatcherStatus status = FunctionPatcher_InitLibrary();
        return (status == FUNCTION_PATCHER_RESULT_SUCCESS);
    }

    inline bool AddPPCExecutablePatch(
        void* replacementFn,
        void** originalFnPtr,
        uint32_t textOffset,
        const uint64_t* targetTitleIds,
        uint32_t targetTitleIdsCount,
        const char* rpxName = "main.rpx"
    ) {
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
                .textOffset          = textOffset,
                .functionName        = nullptr
            }
        };

        PatchedFunctionHandle handle;
        bool hasBeenPatched = false;
        FunctionPatcherStatus res = FunctionPatcher_AddFunctionPatch(&patchData, &handle, &hasBeenPatched);
        return (res == FUNCTION_PATCHER_RESULT_SUCCESS);
    }

} // namespace WiiXLaunch::Backend

#endif // WIIXL_WIIU
