/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <unistd.h>
#include <mutex>
#include <thread>

class FdTracker {
  public:
    static FdTracker& getInstance();

  private:
    FdTracker();
    ~FdTracker();
    FdTracker(const FdTracker&) = delete;
    FdTracker& operator=(const FdTracker&) = delete;

    void monitorLoop();
    int openFdCount(pid_t pid);

    std::thread mMonitorThread;
    bool mThreadStarted = false;

    int mEnableThreshold;
    int mAbortThreshold;
    int mCheckInterval;
};

