/*
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_CoreService_QTI"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_ibinder_platform.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <extensions/AudioExtension.h>

#include <qti-audio-core/Module.h>
#include <qti-audio-core/ModulePrimary.h>
#include <cstdlib>
#include <ctime>

std::shared_ptr<::qti::audio::core::ModulePrimary> gModuleDefaultQti;
using namespace ::qti::audio::core;

auto registerBinderAsService = [](auto &&binder, const std::string &serviceName) {
    AIBinder_setMinSchedulerPolicy(binder.get(), SCHED_NORMAL, ANDROID_PRIORITY_AUDIO);
    binder_exception_t status = AServiceManager_addService(binder.get(), serviceName.c_str());
    if (status != EX_NONE) {
        LOG(ERROR) << __func__ << " failed to register " << serviceName << " ret:" << status;
    } else {
        LOG(INFO) << __func__ << " successfully registered " << serviceName << " ret:" << status;
    }
};

void registerIModuleDefaultQti() {
    gModuleDefaultQti = ndk::SharedRefBase::make<::qti::audio::core::ModulePrimary>();
    //start of power policy registration
    AudioExtension::power_policy_feature_init(property_get_bool("vendor.audio.feature.arpowerpolicy.enable", false));
    const std::string kServiceName =
            std::string(gModuleDefaultQti->descriptor).append("/").append("default");
    registerBinderAsService(gModuleDefaultQti->asBinder(), kServiceName);
}

extern "C" __attribute__((visibility("default"))) int32_t registerServices() {
    registerIModuleDefaultQti();
    return STATUS_OK;
}
