/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace qti::audio::core {

#ifdef __LP64__
#define OFFLOAD_EFFECTS_BUNDLE_LIBRARY_PATH "/vendor/lib64/soundfx/libqcompostprocbundle.so"
#define VISUALIZER_LIBRARY_PATH "/vendor/lib64/soundfx/libqcomvisualizer.so"
#else
#define OFFLOAD_EFFECTS_BUNDLE_LIBRARY_PATH "/vendor/lib/soundfx/libqcompostprocbundle.so"
#define VISUALIZER_LIBRARY_PATH "/vendor/lib/soundfx/libqcomvisualizer.so"
#endif

/*
* All libraries implementing post processing effect should expose
* startEffect(int, pal_stream_handle_t*)
* stopEffect(int)
*/
using pal_stream_handle_t = uint64_t;

using StartEffectFptr = void (*)(int, uint64_t*);
using StopEffectFptr = void (*)(int);

struct OffloadEffectLibIntf {
    StartEffectFptr mStartEffect;
    StopEffectFptr mStopEffect;
};

/* Pair of libhandle, and function pointers*/
using EffectLibInfo = std::pair<std::unique_ptr<void, std::function<void(void*)>>,
                                std::unique_ptr<struct OffloadEffectLibIntf>>;

class HalOffloadEffects {
  private:
#ifdef __LP64__
    const std::string kOffloadVisualizerPath = "/vendor/lib64/soundfx/libqcomvisualizer.so";
    const std::string kOffloadPostProcBundlePath = "/vendor/lib64/soundfx/libqcompostprocbundle.so";
#else
    const std::string kOffloadVisualizerPath = "/vendor/lib/soundfx/libqcomvisualizer.so";
    const std::string kOffloadPostProcBundlePath = "/vendor/lib/soundfx/libqcompostprocbundle.so";
#endif
    std::vector<EffectLibInfo> mEffects;

    std::atomic<bool> mLoaded{false};
    std::mutex mLoadMutex;

    HalOffloadEffects();
    void loadLibrary(std::string path);
    bool ensureLoaded();

  public:
    static HalOffloadEffects& getInstance() {
        static HalOffloadEffects halEffects;
        return halEffects;
    }

    void startEffect(int ioHandle, pal_stream_handle_t* palHandle);
    void stopEffect(int ioHandle);
};

} // qti::audio::core