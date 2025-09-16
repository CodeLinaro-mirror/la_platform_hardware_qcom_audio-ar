/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_FD_TRACKER_QTI"

#include "FdTracker.h"
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <dirent.h>
#include <dlfcn.h>
#include <chrono>
#include <string>

namespace {
// Enable tracker when property is set, by default false
const char kFdTrackerEnabled[] = "vendor.audio.enable.fdtrack";
// minimum fd count where fdleaktracker activates
const char kFdLeakmEnableThresholdProperty[] = "vendor.audio.fdtrack_enable_threshold";
// abort ahal when it fds cross the abort threshold
const char kFdLeakmAbortThresholdProperty[] = "vendor.audio.fdtrack_abort_threshold";
// interval at which fds are checked.
const char kFdLeakmCheckIntervalProperty[] = "vendor.audio.fdtrack_interval";
// enabled only debug builds.
const char kIsDebugBuild[] = "ro.debuggable";
}  // namespace

FdTracker& FdTracker::getInstance() {
    static FdTracker instance;
    return instance;
}

FdTracker::~FdTracker() {
    LOG(INFO) << __func__ << ": destroyed";
}

FdTracker::FdTracker() {
    int isDebugBuild = android::base::GetIntProperty(kIsDebugBuild, 0);
    int enableTracker = android::base::GetIntProperty(kFdTrackerEnabled, 0);
    if (isDebugBuild && enableTracker) {
        mEnableThreshold = android::base::GetIntProperty(kFdLeakmEnableThresholdProperty, 128);
        mAbortThreshold = android::base::GetIntProperty(kFdLeakmAbortThresholdProperty, 4096);
        mCheckInterval = android::base::GetIntProperty(kFdLeakmCheckIntervalProperty, 120);

        if (!mThreadStarted) {
            mThreadStarted = true;
            mMonitorThread = std::thread(&FdTracker::monitorLoop, this);
            mMonitorThread.detach();
        }
    }
}

int FdTracker::openFdCount(pid_t pid) {
    std::string fdPath = "/proc/" + std::to_string(pid) + "/fd";
    DIR* dir = opendir(fdPath.c_str());
    if (!dir) {
        LOG(ERROR) << __func__ << ": Failed to open " << fdPath;
        return -1;
    }

    int count = 0;
    while (readdir(dir)) {
        count++;
    }

    closedir(dir);
    return count - 2;
}

void FdTracker::monitorLoop() {
    pthread_setname_np(pthread_self(), "hal-fdtracker");
    using DumpFdTrack = void (*)();
    static DumpFdTrack fdtrackFptr = nullptr;
    bool loaded = false;
    int lastCount = mEnableThreshold;

    LOG(INFO) << __func__ << ": started";
    while (true) {
        int openFds = openFdCount(getpid());
        if (openFds < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(mCheckInterval));
            continue;
        }

        if (openFds > mEnableThreshold) {
            if (!loaded) {
                loaded = true;
                LOG(INFO) << __func__ << ": FD count " << openFds << " above threshold "
                          << mEnableThreshold << " loading fdtrack";
                void* libfdtrack = dlopen("libfdtrack.so", RTLD_NOW);
                if (!libfdtrack) {
                    LOG(ERROR) << __func__ << ": Failed to load libfdtrack.so";
                    return;
                }

                fdtrackFptr = reinterpret_cast<DumpFdTrack>(dlsym(libfdtrack, "fdtrack_dump"));
                if (!fdtrackFptr) {
                    LOG(ERROR) << __func__ << "Could not find fdtrack_dump" << dlerror();
                    dlclose(libfdtrack);
                    return;
                }
            }

            bool dump = false;
            if (openFds > 2 * lastCount) {
                dump = true;
                lastCount = openFds;
            }

            // to debug inbetween before crash to get info
            if (fdtrackFptr && dump) {
                LOG(ERROR) << __func__ << "fd doubled compared with last check fd count "
                           << openFds;
                fdtrackFptr();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        }

        if (openFds > mAbortThreshold) {
            if (fdtrackFptr) {
                LOG(ERROR) << __func__ << "FD count " << openFds << " above abort threshold "
                           << mAbortThreshold << "dumping and aborting";
                fdtrackFptr();
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
            LOG(FATAL) << __func__ << "Aborting due to possible FD leak total open fds " << openFds;
        }

        std::this_thread::sleep_for(std::chrono::seconds(mCheckInterval));
    }
}
