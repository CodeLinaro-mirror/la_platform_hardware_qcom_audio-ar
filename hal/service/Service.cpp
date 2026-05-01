/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AHAL_Service_QTI"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_ibinder_platform.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <dlfcn.h>
#include <pthread.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ConfigManager.h"
#include "FdTracker.h"

/**
 * adb shell setprop persist.vendor.audio.hal.stubmode <mode>
 */
enum class StubMode {
    DISABLED = 0,
    /**< Normal HAL load. Device may block at boot animation if sound card is not registered. */

    ENABLED = 1,
    /**< Force boot with stub HAL by default, regardless of sound card registration status. */

    FALL_BACK_TO_STUB = 2,
    /**< Try normal HAL first. On timeout, escalate to FAIL_SAFE_STUB for the next boot and
       crash to reboot. */

    FAIL_SAFE_STUB = 3,
    /**< Load stub HAL this boot as fail-safe. Reset to FALL_BACK_TO_STUB for next boot
         so normal HAL is retried on the subsequent reboot. */
};

static std::string toString(StubMode mode) {
    switch (mode) {
        case StubMode::DISABLED:
            return "DISABLED";
        case StubMode::ENABLED:
            return "ENABLED";
        case StubMode::FALL_BACK_TO_STUB:
            return "FALL_BACK_TO_STUB";
        case StubMode::FAIL_SAFE_STUB:
            return "FAIL_SAFE_STUB";
        default:
            return "UNKNOWN";
    }
}

static std::string getCurrentAudioStatus() {
    std::ostringstream status;
    std::vector<std::string> dumpPaths = {"/d/asoc/components", "/proc/asound/cards"};

    char dumpString[1024];

    for (const auto& path : dumpPaths) {
        FILE* fp = fopen(path.c_str(), "r");
        if (fp == nullptr) {
            status << "Failed to open " << path << " | ";
            continue;
        }

        std::string dumpInfo;
        while (fgets(dumpString, sizeof(dumpString), fp) != nullptr) {
            dumpInfo += std::string(dumpString) + " ; ";
        }

        status << path << " : " << dumpInfo << " | ";
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
            status << interface << " NOT registered | ";
        } else {
            status << interface << " registered | ";
        }
    }

    return status.str();
}

static bool isStubEnabled(StubMode stubMode) {
    return stubMode == StubMode::ENABLED || stubMode == StubMode::FAIL_SAFE_STUB;
}

static bool registerServiceImplementation(const Interface& interface) {
    auto libraryName = interface.libraryName;
    auto interfaceMethod = interface.method;
    void* handle = dlopen(libraryName.c_str(), RTLD_LAZY);
    if (handle == nullptr) {
        const char* error = dlerror();
        LOG(ERROR) << "Failed to dlopen " << libraryName << ": "
                   << (error != nullptr ? error : "unknown error");
        return false;
    }
    auto instantiate =
            reinterpret_cast<binder_status_t (*)()>(dlsym(handle, interfaceMethod.c_str()));
    if (instantiate == nullptr) {
        const char* error = dlerror();
        LOG(ERROR) << "Factory function " << interfaceMethod << " not found in libName "
                   << libraryName << ": " << (error != nullptr ? error : "unknown error");
        dlclose(handle);
        return false;
    }
    return (instantiate() == STATUS_OK);
}

void registerInterfaces(const Interfaces& interfaces) {
    constexpr int kRegisterRetryCount = 10;
    constexpr auto kSleepTimeSeconds = std::chrono::seconds(1);

    for (const auto& interface : interfaces) {
        if (registerServiceImplementation(interface)) {
            LOG(INFO) << "successfully registered " << interface.toString();
        } else if (interface.mandatory) {
            int32_t retryCount = 0;
            bool isRegistered = false;
            while (retryCount < kRegisterRetryCount) {
                LOG(INFO) << "failed to register service: " << interface.toString()
                          << ", retry count: " << (retryCount + 1);
                isRegistered = registerServiceImplementation(interface);
                if (isRegistered) {
                    LOG(INFO) << "successfully registered " << interface.toString();
                    break;
                } else {
                    // the service may failed to register due to resource busy, sleep and try again
                    std::this_thread::sleep_for(kSleepTimeSeconds);
                }
                ++retryCount;
            }
            CHECK(isRegistered) << "failed to register " << interface.toString();
        } else {
            LOG(WARNING) << "failed to register optional " << interface.toString();
        }
    }
}

bool registerFromConfigs() {
    auto interfaces = parseInterfaces();
    if (interfaces.empty()) {
        LOG(ERROR) << __func__ << " no valid interface found, validate configuration!";
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
    constexpr int kDefaultStubMode = static_cast<int>(StubMode::DISABLED);
    StubMode stubmode = static_cast<StubMode>(
            ::android::base::GetIntProperty("persist.vendor.audio.hal.stubmode", kDefaultStubMode));

    LOG(INFO) << "Initial StubMode read: " << toString(stubmode);

    if (stubmode == StubMode::FAIL_SAFE_STUB) {
        // Reset property so next boot retries normal HAL initialization
        android::base::SetProperty("persist.vendor.audio.hal.stubmode",
                                   std::to_string(static_cast<int>(StubMode::FALL_BACK_TO_STUB)));
        LOG(INFO) << __func__
                  << ": FAIL_SAFE_STUB active. Resetting property to FALL_BACK_TO_STUB for next "
                     "boot.";
    }

    if (isStubEnabled(stubmode) || !registerFromConfigs()) {
        LOG(INFO) << "registerDefaultInterfaces stub mode " << toString(stubmode);
        registerDefaultInterfaces();
    }
}

void setLogSeverity() {
    // by default use DEBUG logging enabled
    auto logLevel = ::android::base::GetIntProperty<int8_t>("vendor.audio.hal.loglevel", 1);
    // system/libbase/include/android-base/logging.h, check LogSeverity for types
    android::base::SetMinimumLogSeverity(static_cast<::android::base::LogSeverity>(logLevel));
}

class HALHealthMonitor final {
  public:
    HALHealthMonitor() {
        constexpr int kDefaultTimeOutInSeconds = 30;
        mTimeoutSeconds = ::android::base::GetIntProperty(
                "vendor.audio.hal.health.monitor.timeout.seconds", kDefaultTimeOutInSeconds);
        mThread = std::thread(&HALHealthMonitor::monitorLoop, this);
    }

    void reportHealthyAndJoin() {
        {
            std::lock_guard<std::mutex> lck(mMutex);
            mIsHalInit = true;
        }
        mCv.notify_one();
        if (mThread.joinable()) {
            mThread.join();
        }
    }

  private:
    void monitorLoop() {
        pthread_setname_np(pthread_self(), "healthchecker");
        std::unique_lock<std::mutex> lck(mMutex);
        bool signaled = mCv.wait_for(lck, std::chrono::seconds(mTimeoutSeconds),
                                     [this] { return mIsHalInit; });

        if (!signaled) {
            std::string audioStatus = getCurrentAudioStatus();
            LOG(ERROR) << "Audio Status at timeout: " << audioStatus;

            constexpr int kDefaultStubMode = static_cast<int>(StubMode::DISABLED);
            StubMode stubmode = static_cast<StubMode>(
                    ::android::base::GetIntProperty("persist.vendor.audio.hal.stubmode", kDefaultStubMode));
            std::ostringstream msg;
            msg << "AHAL init timed out after " << mTimeoutSeconds << " Seconds. "
                << "Current StubMode: " << toString(stubmode) << ". ";

            if (stubmode == StubMode::FALL_BACK_TO_STUB) {
                android::base::SetProperty(
                        "persist.vendor.audio.hal.stubmode",
                        std::to_string(static_cast<int>(StubMode::FAIL_SAFE_STUB)));
                msg << "StubMode escalated to " << toString(StubMode::FAIL_SAFE_STUB)
                    << ". Next boot will load Stub HAL as fail-safe. ";
            } else if (stubmode == StubMode::FAIL_SAFE_STUB) {
                msg << "Fail-safe stub HAL boot timed out! Need to debug. ";
            } else if (stubmode == StubMode::DISABLED) {
                msg << "Will retry normal HAL boot on next reboot. ";
            } else if (stubmode == StubMode::ENABLED) {
                msg << "Even Stub HAL boot is failing. Need to debug. ";
            } else {
                msg << "Unknown StubMode. Next boot StubMode unchanged. ";
            }

            LOG(FATAL) << msg.str();
        }
    }

    std::mutex mMutex;
    std::condition_variable mCv;
    bool mIsHalInit = false;
    std::thread mThread;
    int mTimeoutSeconds;
};

int main() {
    auto startTime = std::chrono::steady_clock::now();
    // Random values are used in the implementation.
    std::srand(std::time(nullptr));

    HALHealthMonitor healthMonitor;

    setLogSeverity();

    ABinderProcess_setThreadPoolMaxThreadCount(16);
    ABinderProcess_startThreadPool();

    registerAvailableInterfaces();

    healthMonitor.reportHealthyAndJoin();

    auto endTime = std::chrono::steady_clock::now();
    float timeTaken =
            std::chrono::duration_cast<std::chrono::duration<float>>(endTime - startTime).count();

    LOG(INFO) << "registration took " << std::fixed << std::setprecision(2) << timeTaken
              << " seconds";

    FdTracker::getInstance();
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
