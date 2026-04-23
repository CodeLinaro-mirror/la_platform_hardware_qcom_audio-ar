/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_HalOffloadEffects_QTI"

#include <android-base/logging.h>
#include <dlfcn.h>
#include <qti-audio-core/HalOffloadEffects.h>

namespace qti::audio::core {

HalOffloadEffects::HalOffloadEffects() {
    mLoaded.store(false, std::memory_order_relaxed);
}

void HalOffloadEffects::loadLibrary(std::string path) {
    // dlopen library and dlsym fptr.
    std::function<void(void *)> dlClose = [](void *handle) -> void {
        if (handle && dlclose(handle)) {
            LOG(ERROR) << "dlclose failed " << dlerror();
        }
    };

    auto libHandle =
            std::unique_ptr<void, decltype(dlClose)>{dlopen(path.c_str(), RTLD_LAZY), dlClose};
    if (!libHandle) {
        LOG(ERROR) << __func__ << ": dlopen failed for " << path << " " << dlerror();
        return;
    }

    auto effectIntf = std::make_unique<OffloadEffectLibIntf>(OffloadEffectLibIntf{nullptr, nullptr});
    effectIntf->mStartEffect = (StartEffectFptr)dlsym(libHandle.get(), "startEffect");
    if (!effectIntf->mStartEffect) {
        LOG(ERROR) << "startEffect is missing in " << path << dlerror();
        return;
    }
    effectIntf->mStopEffect = (StopEffectFptr)dlsym(libHandle.get(), "stopEffect");
    if (!effectIntf->mStopEffect) {
        LOG(ERROR) << "stopEffect is missing in " << path << dlerror();
        return;
    }
    LOG(DEBUG) << "found post proc library" << path;
    mEffects.emplace_back(std::make_pair(std::move(libHandle), std::move(effectIntf)));
}

bool HalOffloadEffects::ensureLoaded() {
    // Single-check version: always guard initialization with mutex.
    std::lock_guard<std::mutex> _l(mLoadMutex);
    if (mLoaded.load(std::memory_order_acquire)) return true;

    loadLibrary(kOffloadPostProcBundlePath);
    loadLibrary(kOffloadVisualizerPath);

    if (mEffects.empty()) {
        LOG(ERROR) << __func__ << ": no offload effect libraries loaded";
        return false;
    }

    mLoaded.store(true, std::memory_order_release);
    return true;
}

void HalOffloadEffects::startEffect(int ioHandle, pal_stream_handle_t *palHandle) {
    if (!ensureLoaded()) {
        LOG(WARNING) << __func__ << ": effects not loaded, skip start";
        return;
    }

    for (const auto &effect : mEffects) {
        effect.second->mStartEffect(ioHandle, palHandle);
    }
}

void HalOffloadEffects::stopEffect(int ioHandle) {
    std::vector<StopEffectFptr> stopFns;
    {
        std::lock_guard<std::mutex> _l(mLoadMutex);
        if (!mLoaded.load(std::memory_order_acquire) || mEffects.empty()) {
            LOG(WARNING) << __func__ << ": effects not loaded, skip stop";
            return;
        }

        stopFns.reserve(mEffects.size());
        for (const auto &effect : mEffects) {
            stopFns.push_back(effect.second->mStopEffect);
        }
    }

    for (const auto &stopFn : stopFns) {
        stopFn(ioHandle);
    }
}

} // namespace qti::audio::core