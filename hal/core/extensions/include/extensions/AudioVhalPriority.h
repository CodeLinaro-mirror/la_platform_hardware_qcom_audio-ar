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

void handler_vhal();
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

// VHAL Night Mode and Door properties struct
struct vhal_data{
    int32_t nightModeValue;
    int32_t driverDoor;
    int32_t frontPassengerDoor;
    int32_t rearLeftDoor;
    int32_t rearRightDoor;
};

//VHAL properties data handling
class Vhal_Data{
private:
    static struct vhal_data* vhal_data;

public:
    static void init() {
        if (vhal_data == nullptr) {
            vhal_data = (struct vhal_data*)malloc(sizeof(struct vhal_data));
            vhal_data->nightModeValue = 0;
            vhal_data->driverDoor = 0;
            vhal_data->frontPassengerDoor = 0;
            vhal_data->rearLeftDoor = 0;
            vhal_data->rearRightDoor = 0;
        }
    }

    static struct vhal_data* vhal_get_params(){
        return vhal_data;
    }
    static void vhal_set_params(struct vhal_data* curr_vhal_data){
        if (curr_vhal_data != nullptr) {
            vhal_data = curr_vhal_data;
        }
    }
    static void cleanup() {
        free(vhal_data);
        vhal_data = nullptr; // Avoid dangling pointer
    }
};

#ifdef __cplusplus
}
#endif
}
