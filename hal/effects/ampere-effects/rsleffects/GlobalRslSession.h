 /*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <algorithm>
#include <memory>
#include <unordered_map>

#include <android-base/logging.h>
#include <android-base/thread_annotations.h>

#include "RslContext.h"
#include "RslTypes.h"

namespace aidl::ampere::effects {

/**
 * @brief Maintain all rsl effect sessions.
 *
 */
class GlobalRslSession {
    public:
    static GlobalRslSession& getGlobalSession() {
        static GlobalRslSession instance;
        return instance;
    }

    static bool findTypeInContextList(std::vector<std::shared_ptr<RslContext>>& list,
                                      const RslEffectType& type, bool remove = false) {
        LOG(DEBUG) << "Enter " << __func__ << " type:" << type;
        auto itr = std::find_if(list.begin(), list.end(),
                                [type](const std::shared_ptr<RslContext>& effect) {
                                    return effect->getEffectType() == type;
                                });
        if (itr == list.end()) {
            return false;
        }
        if (remove) {
            LOG(ERROR) << __func__ << " performing deInit";
            (*itr)->deInit(); // call release inside of it.
            list.erase(itr);
        }
        LOG(DEBUG) << "Exit " <<__func__;
        return true;
    }

    std::shared_ptr<RslContext> createContext(const RslEffectType& type,
                                                        const Parameter::Common& common,
                                                        bool processData) {
        LOG(DEBUG) << "Enter " << __func__ << " type: " << type << " processData:" << processData;
        switch (type) {
            case RslEffectType::AMBIANCE:
                return std::make_shared<AmbianceContext>(common, type, processData);
            case RslEffectType::SDVC:
                return std::make_shared<SDVCContext>(common, type, processData);
            case RslEffectType::STEADY_VOLUME:
                return std::make_shared<SteadyVolumeContext>(common, type, processData);
        }
        LOG(DEBUG) << "Exit " <<__func__;
        return nullptr;
    }

    /**
     * Create a certain type of RslContext in shared_ptr container, each session must not have
     * more than one session for each type.
     */
    std::shared_ptr<RslContext> createSession(const RslEffectType& type,
                                                        const Parameter::Common& common,
                                                        bool processData) {
        LOG(DEBUG) << "Enter " << __func__ << " type: " << type << " processData:" << processData;
        std::lock_guard lg(mMutex);
        int ioHandle = common.ioHandle;
        int sessionId = common.session;
        LOG(DEBUG) << __func__ << " " << type << " with ioHandle " << ioHandle << " sessionId"
                   << sessionId;
        if (mSessionsMap.count(sessionId)) {
            if (findTypeInContextList(mSessionsMap[sessionId], type)) {
                LOG(ERROR) << __func__ << type << " already exist in  " << sessionId;
                return nullptr;
            }
        }

        auto& list = mSessionsMap[sessionId];
        LOG(DEBUG) << __func__ << type << " createContext ioHandle " << ioHandle << " sessionId"
                   << sessionId;
        auto context = createContext(type, common, processData);
        RETURN_VALUE_IF(!context, nullptr, "failedToCreateContext");

        list.push_back(context);

        // find ioHandle in the mActiveIoHandles
        for (const auto& pair : mActiveIoHandles) {
            if (pair.first == ioHandle) {
                LOG(DEBUG) << "IoHandle is active " << ioHandle << " session " << sessionId;
                context->start();
            }
        }
        LOG(DEBUG) << "Exit " <<__func__;
        return context;
    }

    void releaseSession(const RslEffectType& type, int sessionId) {
        std::lock_guard lg(mMutex);
        LOG(DEBUG) << "Enter " << __func__ << " type: " << type << " sessionId " << sessionId;
        if (mSessionsMap.count(sessionId)) {
            auto& list = mSessionsMap[sessionId];
            if (!findTypeInContextList(list, type, true /* remove */)) {
                LOG(ERROR) << __func__ << " can't find " << type << "in sessionId " << sessionId;
                return;
            }
            if (list.empty()) {
                mSessionsMap.erase(sessionId);
            }
        }
        LOG(DEBUG) << "Exit " << __func__ << " type: " << type << " sessionId " << sessionId << " sessions " << mSessionsMap.size();
    }

    // Used by AudioHal to link effect with output.
    void startEffect(int ioHandle, pal_stream_handle_t* palHandle) {
        std::lock_guard lg(mMutex);

        LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << ioHandle << 
                    " sessions " << mSessionsMap.size();
        // start the context having same ioHandle
        for (const auto& handles : mSessionsMap) {
            auto& list = handles.second;
            for (const auto& context : list) {
                if (context->getIoHandle() == ioHandle) {
                    context->start();
                }
            }
        mActiveIoHandles[ioHandle] = palHandle;
        LOG(DEBUG) << "Exit " << __func__;
        }
    }

    // Used by AudioHal to link effect with output.
    void stopEffect(int ioHandle) {
        std::lock_guard lg(mMutex);
        LOG(DEBUG) << "Enter " << __func__ << " ioHandle " << ioHandle << " sessions " << mSessionsMap.size()
                   << "activeHandles " << mActiveIoHandles.count(ioHandle);

        // stop the context having same ioHandle
        for (const auto& handles : mSessionsMap) {
            auto& list = handles.second;
            for (const auto& context : list) {
                if (context->getIoHandle() == ioHandle) {
                    context->stop();
                }
            }
        }

        if (mActiveIoHandles.count(ioHandle)) {
            mActiveIoHandles.erase(ioHandle);
            LOG(VERBOSE) << __func__ << " Removed ioHandle " << ioHandle << " sessions "
                         << mSessionsMap.size() << " activeHandles "
                         << mActiveIoHandles.count(ioHandle);
        }
    }

    private:
    // Lock for mSessionsMap access.
    std::mutex mMutex;

    // map between sessionId and list of effect contexts for that session
    std::unordered_map<int /* sessionId */, std::vector<std::shared_ptr<RslContext>>>
            mSessionsMap GUARDED_BY(mMutex);

    // io Handle to palHandle mapping.
    std::unordered_map<int, pal_stream_handle_t*> mActiveIoHandles;
};
} // namespace aidl::ampere::effects
