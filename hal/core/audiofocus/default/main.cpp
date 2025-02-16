/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "AudioHalFocusManager.h"
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include "BusDuckConfig.h"

using aidl::android::hardware::audio::focus::AudioFocusService;
#define XML_FILE_PATH "/vendor/etc/audio_ar/duck_configuration.xml"

void parseXML() {

    //XML parsing
    BusDuckConfigParser parser;
    FILE* file = NULL;
    file = fopen(XML_FILE_PATH, "r");
    if (!file) {
        LOG(ERROR) << __func__ << "File not present: " << XML_FILE_PATH;
        return;
    } else {
        LOG(ERROR) << __func__ << "File present" << XML_FILE_PATH;
    }
    fclose(file);

    if (parser.parseConfig(XML_FILE_PATH)) {
        const auto& properties = parser.getProperties();
        for(const auto& prop : properties) {
            LOG(INFO) << __func__ << " In_src:" << prop.in_src
                            << " running_src:" << prop.running_src;
        }
        parser.populateAudioFocusConfig(AudioFocusService::configuration);
    } else {
        LOG(ERROR) << __func__ <<  "Failed to parse configuration" ;
    }
}


int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(4);

    parseXML();
    LOG(INFO) << "XML parsed successfully ";
    std::shared_ptr<AudioFocusService> audioHalFocusService = ::ndk::SharedRefBase::make<AudioFocusService>();

    const std::string instance = std::string() + AudioFocusService::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(audioHalFocusService->asBinder().get(), instance.c_str());

    if (status != STATUS_OK) {
        LOG(ERROR) << "Service registration failed with status: " << status;
        return EXIT_FAILURE;
    }
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
