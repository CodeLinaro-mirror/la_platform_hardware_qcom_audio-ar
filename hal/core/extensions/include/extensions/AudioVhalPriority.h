/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <PalApi.h>
#include <android-base/logging.h>
#include <include/extensions/AudioHalFocusManager.h>
#include <dlfcn.h>

namespace {

#ifdef __cplusplus
extern "C" {
#endif

void handler_radioMute(int32_t radio_mute_byAAM_value);

class FocusHandler {
    private:
        static void* mHandle;
        typedef int (*RequestFocusType)(const qti::audio::core::FocusInfo, int64_t*);
        typedef int (*AbandonFocusType)(int64_t);
        RequestFocusType requestFocusFunc;
        AbandonFocusType abandonFocusFunc;

    public:
        static std::map<std::string, std::vector<int64_t>> focusIdMap;
        FocusHandler(const std::string& libName);
        ~FocusHandler();
        bool isValid();
        int requestFocus(const qti::audio::core::FocusInfo& focusInfo, int64_t* focusId);
        int abandonFocus(int64_t focusId);
};

#ifdef __cplusplus
}
#endif
}
