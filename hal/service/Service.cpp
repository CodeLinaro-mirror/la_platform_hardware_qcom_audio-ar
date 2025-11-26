/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AHAL_Service_QTI"

#include <dlfcn.h>
#include <cstdlib>
#include <ctime>

#include <algorithm>

#include <chrono>
#include <string>
#include <vector>

#include <mutex>
#include <thread>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_ibinder_platform.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <log/log.h>
#include "ConfigManager.h"
#include "FdTracker.h"

#define REGISTER_RETRY_COUNT 10
#define SLEEP_TIME_SECONDS 1

#define AHAL_INIT_TIMEOUT 30

enum class StubMode {
    STUB_DISABLED = 0,
    /**< device can block at boot animation if sound card is not registered */

    STUB_ENFORCED = 1 << 0,
    /**< boot with stub mode by default with/without sound card registered */

    STUB_AUTOMATED = 1 << 2,
    /**< auto boot into stub mode after 30S if sound card is not registered */
};

static void dumpAudioStatus() {
    std::vector<std::string> dumpPaths = {"/d/asoc/components", "/proc/asound/cards"};

    char dumpString[1024];

    for (const auto& path : dumpPaths) {
        FILE* fp = fopen(path.c_str(), "r");
        if (fp == nullptr) {
            ALOGE("Failed to open path: %s", path.c_str());
            continue;
        }

        std::string dumpInfo;
        while (fgets(dumpString, sizeof(dumpString), fp) != nullptr) {
            dumpInfo += std::string(dumpString) + " ; ";
        }

        ALOGI("%s : %s", path.c_str(), dumpInfo.c_str());
        fclose(fp);
    }

    std::vector<std::string> interfaces = {
            "vendor.qti.hardware.agm.IAGM/default",
            "vendor.qti.hardware.pal.IPAL/default",
            "android.hardware.audio.core.IConfig/default",
            "android.hardware.audio.core.IModule/default",
    };

    for (const auto& interface : interfaces) {
        AIBinder* binder = AServiceManager_checkService(interface.c_str());
        if (binder == nullptr) {
            ALOGE("%s interface %s not registered", __func__, interface.c_str());
        }
    }
}

static bool isStubEnforced(StubMode stubMode)
{
    return stubMode == StubMode::STUB_ENFORCED;
}

static bool registerServiceImplementation(const Interface& interface) {
    auto libraryName = interface.libraryName;
    auto interfaceMethod = interface.method;
    void* handle = dlopen(libraryName.c_str(), RTLD_LAZY);
    if (handle == nullptr) {
        const char* error = dlerror();
        ALOGE("Failed to dlopen %s: %s", libraryName.c_str(),
              error != nullptr ? error : "unknown error");
        return false;
    }
    auto instantiate =
            reinterpret_cast<binder_status_t (*)()>(dlsym(handle, interfaceMethod.c_str()));
    if (instantiate == nullptr) {
        const char* error = dlerror();
        ALOGE("Factory function %s not found in libName %s: %s", interfaceMethod.c_str(),
              libraryName.c_str(), error != nullptr ? error : "unknown error");
        dlclose(handle);
        return false;
    }
    return (instantiate() == STATUS_OK);
}

void registerInterfaces(const Interfaces& interfaces) {
    for (const auto& interface : interfaces) {
        if (registerServiceImplementation(interface)) {
            ALOGI("successfully registered %s", interface.toString().c_str());
        } else if (interface.mandatory) {
            int32_t retryCount = 0;
            bool isRegistered = false;
            while (retryCount < REGISTER_RETRY_COUNT) {
                ALOGI("failed to register service: %s, retry count: %d",
                        interface.toString().c_str(), retryCount + 1);
                isRegistered = registerServiceImplementation(interface);
                if (isRegistered) {
                    ALOGI("successfully registered %s", interface.toString().c_str());
                    break;
                } else {
                    //the service may failed to register due to resource busy, sleep and try again
                    sleep(SLEEP_TIME_SECONDS);
                }
                ++retryCount;
            }
            LOG_ALWAYS_FATAL_IF(!isRegistered, "failed to register %s ",
                                interface.toString().c_str());
        } else {
            ALOGW("failed to register optional %s ", interface.toString().c_str());
        }
    }
}

bool registerFromConfigs() {
    auto interfaces = parseInterfaces();
    if (interfaces.empty()) {
        ALOGE("%s no valid interface found, validate configuration!", __func__);
        return false;
    }
    registerInterfaces(interfaces);
    return true;
}

/*
* Don't modify default entries unless the library is a must for stub mode bootup.
* These interfaces will be loaded when vendor.audio.hal.stubmode is 1
*/
void registerDefaultInterfaces() {
    Interfaces defaultInterfaces = {
            {.name = "audiohal-default",
             .libraryName = "libaudiocorehal.default.so",
             .method = "registerServices",
             .mandatory = true},
            {.name = "audioeffecthal",
             .libraryName = "libaudioeffecthal.qti.so",
             .method = "registerStubService",
             .mandatory = true},
            {.name = "soundtriggerhal",
             .libraryName = "libsoundtriggerhal.qti.so",
             .method = "createStubISoundTriggerFactory",
             .mandatory = true},
            {.name = "bthal",
             .libraryName = "android.hardware.bluetooth.audio_sw.so",
             .method = "registerIModuleBluetoothSWQti",
             .mandatory = false},
    };

    registerInterfaces(defaultInterfaces);
}

void registerAvailableInterfaces() {
    StubMode stubmode = (StubMode)::android::base::GetIntProperty("vendor.audio.hal.stubmode", 0);
    if (isStubEnforced(stubmode) || !registerFromConfigs()) {
        ALOGI("registerDefaultInterfaces stub mode %d", stubmode);
        registerDefaultInterfaces();
    }
}

void setLogSeverity() {
    // by default use DEBUG logging enabled
    auto logLevel = ::android::base::GetIntProperty<int8_t>("vendor.audio.hal.loglevel", 1);
    // system/libbase/include/android-base/logging.h, check LogSeverity for types
    android::base::SetMinimumLogSeverity(static_cast<::android::base::LogSeverity>(logLevel));
}

static std::mutex halInitMutex;
static std::condition_variable halInitCv;
static bool isHalInit = false;

void halMonitorThread() {
    std::unique_lock<std::mutex> lck(halInitMutex);
    while (!isHalInit) {
        std::cv_status status =
             halInitCv.wait_for(lck,std::chrono::seconds(AHAL_INIT_TIMEOUT));
        if (status == std::cv_status::timeout) {
            dumpAudioStatus();
            StubMode stubmode = (StubMode)
                ::android::base::GetIntProperty("vendor.audio.hal.stubmode", 0);
            if (stubmode == StubMode::STUB_AUTOMATED) {
                android::base::SetProperty("vendor.audio.hal.stubmode",
                    std::to_string((int)StubMode::STUB_ENFORCED));
                LOG(ERROR) << __func__ << ": Enable Stub Audio";
            }
            LOG_ALWAYS_FATAL("AHAL init took more than %d S, rebooting...", AHAL_INIT_TIMEOUT);
        }
    }
}

int main() {
    auto startTime = std::chrono::steady_clock::now();
    // Random values are used in the implementation.
    std::srand(std::time(nullptr));
    std::thread monitorThread(halMonitorThread);
    setLogSeverity();

    ABinderProcess_setThreadPoolMaxThreadCount(16);
    ABinderProcess_startThreadPool();

    registerAvailableInterfaces();

    halInitMutex.lock();
    isHalInit = true;
    halInitMutex.unlock();
    halInitCv.notify_one();
    monitorThread.join();

    auto endTime = std::chrono::steady_clock::now();
    float timeTaken =
            std::chrono::duration_cast<std::chrono::duration<float>>(endTime - startTime).count();

    ALOGI("registration took %.2f seconds ", timeTaken);

    FdTracker::getInstance();
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
