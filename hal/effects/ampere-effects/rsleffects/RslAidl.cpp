 /*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Effect_RslAidl"
#include <Utils.h>
#include <algorithm>
#include <unordered_set>


#include <android-base/logging.h>
#include <fmq/AidlMessageQueue.h>
#include <hardware/audio_effect.h>
#include "aidl/android/hardware/audio/effect/DefaultExtension.h"

#include "RslAidl.h"

using aidl::android::hardware::audio::effect::IEffect;
using aidl::android::hardware::audio::effect::Descriptor;
using aidl::ampere::effects::RslAidl;
using aidl::qti::effects::kAmbianceUUID;
using aidl::qti::effects::kSdvcUUID;
using aidl::ampere::effects::kSdvcDescriptor;
using aidl::qti::effects::kSteadyVolumeUUID;
using aidl::ampere::effects::kSteadyVolumeDescriptor;
using aidl::ampere::effects::kBMTDescriptor;
using aidl::qti::effects::kEqualizerBundleImplUUID;
using aidl::qti::effects::kBassBoostBundleImplUUID;
using aidl::ampere::effects::kBassBoostDescriptor;

using aidl::android::hardware::audio::effect::State;
using aidl::android::media::audio::common::AudioUuid;
using aidl::android::hardware::audio::effect::Parameter;
using aidl::android::hardware::audio::effect::VendorExtension;
using aidl::android::hardware::audio::effect::DefaultExtension;

bool isUuidSupported(const AudioUuid* uuid) {
    LOG(DEBUG) << "Enter " << __func__ << " uuid:" << aidl::qti::effects::toString(*uuid);
    return (*uuid == kAmbianceUUID || *uuid == kSdvcUUID || *uuid == kSteadyVolumeUUID
             || *uuid == kEqualizerBundleImplUUID || *uuid == kBassBoostBundleImplUUID);
}

extern "C" binder_exception_t createEffect(
        const AudioUuid* uuid,
        std::shared_ptr<aidl::android::hardware::audio::effect::IEffect>* instanceSpp) {
    LOG(DEBUG) << "Enter " << __func__ << " uuid:" << aidl::qti::effects::toString(*uuid);
    if (uuid == nullptr || !isUuidSupported(uuid)) {
        LOG(ERROR) << __func__ << "uuid not supported " << aidl::qti::effects::toString(*uuid);
        return EX_ILLEGAL_ARGUMENT;
    }
    if (instanceSpp) {
        *instanceSpp = ndk::SharedRefBase::make<RslAidl>(*uuid);
        LOG(DEBUG) << __func__ << " instance " << instanceSpp->get() << " created";
        return EX_NONE;
    } else {
        LOG(ERROR) << __func__ << " invalid input parameter!";
        return EX_ILLEGAL_ARGUMENT;
    }
    LOG(DEBUG) << "Exit " <<__func__;
}

extern "C" void startEffect(int ioHandle, uint64_t* palHandle) {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle: " << ioHandle << " palHandle: " << palHandle;
    aidl::ampere::effects::GlobalRslSession::getGlobalSession().startEffect(ioHandle, palHandle);
}

extern "C" void stopEffect(int ioHandle) {
    LOG(DEBUG) << "Enter " << __func__ << " ioHandle: " << ioHandle;
    aidl::ampere::effects::GlobalRslSession::getGlobalSession().stopEffect(ioHandle);
}

extern "C" binder_exception_t queryEffect(
        const AudioUuid* in_impl_uuid,
        aidl::android::hardware::audio::effect::Descriptor* _aidl_return) {
    LOG(DEBUG) << "Enter " << __func__ << " uuid: " << in_impl_uuid;
    if (!in_impl_uuid || !isUuidSupported(in_impl_uuid)) {
        LOG(ERROR) << __func__ << "uuid not supported "
                   << aidl::qti::effects::toString(*in_impl_uuid);
        return EX_ILLEGAL_ARGUMENT;
    }
    if (*in_impl_uuid == kAmbianceUUID) {
        *_aidl_return = kAmbianceDescriptor;
    } else if (*in_impl_uuid == kSdvcUUID) {
        *_aidl_return = kSdvcDescriptor;
    } else if (*in_impl_uuid == kSteadyVolumeUUID) {
        *_aidl_return = kSteadyVolumeDescriptor;
    } else if (*in_impl_uuid == kEqualizerBundleImplUUID) {
        *_aidl_return = kBMTDescriptor;
    } else if (*in_impl_uuid == kBassBoostBundleImplUUID) {
        *_aidl_return = kBassBoostDescriptor;
    } else {
        LOG(ERROR) << __func__ << in_impl_uuid << " not supported!";
    }
    return EX_NONE;
}

namespace aidl::ampere::effects {
    RslAidl::RslAidl(const AudioUuid& uuid) {
        LOG(DEBUG) << "Enter " << __func__ << " uuid: " << aidl::qti::effects::toString(uuid);
        if (uuid == kAmbianceUUID) {
            mType = RslEffectType::AMBIANCE;
            mDescriptor = &kAmbianceDescriptor;
            mEffectName = &kAmbianceEffectName;
        } else if (uuid == kSdvcUUID) {
            mType = RslEffectType::SDVC;
            mDescriptor = &kSdvcDescriptor;
            mEffectName = &kSdvcEffectName;
        } else if (uuid == kSteadyVolumeUUID) {
            mType = RslEffectType::STEADY_VOLUME;
            mDescriptor = &kSteadyVolumeDescriptor;
            mEffectName = &kSteadyVolumeEffectName;
        } else if (uuid == kEqualizerBundleImplUUID) {
            mType = RslEffectType::BMT;
            mDescriptor = &kBMTDescriptor;
            mEffectName = &kBMTEffectName;
        } else if (uuid == kBassBoostBundleImplUUID) {
            mType = RslEffectType::BASS_BOOST;
            mDescriptor = &kBassBoostDescriptor;
            mEffectName = &kBassBoostEffectName;
        } else {
            LOG(ERROR) << __func__ << aidl::qti::effects::toString(uuid) << " not supported!";
        }
    }

    RslAidl::~RslAidl() {
        LOG(DEBUG) << __func__ << mType;
        cleanUp();
    }

    ndk::ScopedAStatus RslAidl::getDescriptor(
            aidl::android::hardware::audio::effect::Descriptor* _aidl_return) {
        LOG(DEBUG) << "Enter " << __func__;
        RETURN_IF(!_aidl_return, EX_ILLEGAL_ARGUMENT, "Parameter:nullptr");
        LOG(INFO) <<" getDescriptor  "<< _aidl_return->toString();
        *_aidl_return = *mDescriptor;
        return ndk::ScopedAStatus::ok();
    }

    void RslAidl::stopEffectIfNeeded(const Parameter::Common& common) {
        LOG(DEBUG) << "Enter " << __func__ ;
        int ioHandle = common.ioHandle;
        int previousHandle = mContext->getIoHandle();
        if (ioHandle != previousHandle) {
            LOG(DEBUG) << getEffectName() << __func__ << " stop on previous handle " << previousHandle
                   << " new handle " << ioHandle;
            aidl::ampere::effects::GlobalRslSession::getGlobalSession().stopEffect(previousHandle);
        }
    }

    ndk::ScopedAStatus RslAidl::setParameterCommon(const Parameter& param) {
        LOG(DEBUG) << "Enter " << __func__;
        RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");

        const auto& tag = param.getTag();

        LOG(DEBUG) << mType << " " << __func__ << param.toString();
        if (tag == Parameter::common) {
            stopEffectIfNeeded(param.get<Parameter::common>());
            RETURN_IF(mContext->setCommon(param.get<Parameter::common>()) != RetCode::SUCCESS,
                  EX_ILLEGAL_ARGUMENT, "setCommFailed");
        } else {
            // for rest of params use base class.
            return EffectImpl::setParameterCommon(param);
        }
        LOG(DEBUG) << "Exit " << __func__;
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus RslAidl::setParameterSpecific(const Parameter::Specific& specific) {
        LOG(DEBUG) << "Enter " << __func__ << " specific " << specific.toString();
        RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");

        auto tag = specific.getTag();
        switch (tag) {
            case Parameter::Specific::equalizer:
                return setParameterBMT(specific);
            case Parameter::Specific::bassBoost:
                return setParameterBassBoost(specific);
            case Parameter::Specific::vendorEffect:
                return setParameterVendorEffect(specific);
            default:
                LOG(ERROR) << __func__ << " unsupported tag " << toString(tag);
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "specificParamNotSupported");
        }
        LOG(DEBUG) << "Exit " << __func__ ;
    }

    ndk::ScopedAStatus RslAidl::getParameterSpecific(const Parameter::Id& id,
                                                           Parameter::Specific* specific) {
        LOG(DEBUG) << "Enter " << __func__ << " tag: " << toString(id.getTag());
        RETURN_IF(!specific, EX_NULL_POINTER, "nullPtr");
        auto tag = id.getTag();

        switch (tag) {
            case Parameter::Id::equalizerTag:
                return getParameterBMT(id.get<Parameter::Id::equalizerTag>(), specific);
            case Parameter::Id::bassBoostTag:
                return getParameterBassBoost(id.get<Parameter::Id::bassBoostTag>(), specific);
            case Parameter::Id::vendorEffectTag:
                return getParameterVendorEffect(id, specific);
            default:
                LOG(ERROR) << __func__ << " unsupported tag: " << toString(tag);
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "wrongIdTag");
        }
        LOG(DEBUG) << "Exit " << __func__ ;
    }

    std::shared_ptr<EffectContext> RslAidl::createContext(const Parameter::Common& common,
                                                                bool processData) {
        LOG(DEBUG) << "Enter " << __func__ << " processData: " << processData;
        if (mContext) {
            LOG(DEBUG) << __func__ << " context already exist";
        } else {
            // GlobalSession is a singleton
            mContext =
                GlobalRslSession::getGlobalSession().createSession(mType, common, processData);
        }
        LOG(DEBUG) << "Exit " << __func__;
        return mContext;
    }

    RetCode RslAidl::releaseContext() {
        LOG(DEBUG) << "Enter " << __func__;
        if (mContext) {
            GlobalRslSession::getGlobalSession().releaseSession(mType, mContext->getSessionId());
            mContext.reset();
        }
        LOG(DEBUG) << "Exit " << __func__;
        return RetCode::SUCCESS;
    }

    ndk::ScopedAStatus RslAidl::commandImpl(
        aidl::android::hardware::audio::effect::CommandId command) {
        LOG(DEBUG) << "Enter " << __func__;
        RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");
        switch (command) {
            case aidl::android::hardware::audio::effect::CommandId::START:
                mContext->start();
                break;
            case aidl::android::hardware::audio::effect::CommandId::STOP:
                mContext->stop();
                break;
            case aidl::android::hardware::audio::effect::CommandId::RESET:
                mContext->resetBuffer();
                break;
            default:
                LOG(ERROR) << __func__ << " commandId " << toString(command) << " not supported";
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "commandIdNotSupported");
        }
        LOG(DEBUG) << "Exit " << __func__;
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus RslAidl::getParameterVendorEffect(const Parameter::Id& id,
                                                            Parameter::Specific* specific) {
        LOG(DEBUG) << "Enter " << __func__;
        auto& param = id.get<Parameter::Id::vendorEffectTag>();
        std::optional<DefaultExtension> queryExtension;

        param.extension.getParcelable(&queryExtension);

        RETURN_IF(!queryExtension.has_value(), EX_ILLEGAL_ARGUMENT, "parcelableIdNull");

        DefaultExtension replyExtension;
        VendorExtension vendor_extension;


        RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");
        LOG(DEBUG) << "mContext->getEffectType() " << mContext->getEffectType();
        replyExtension.bytes = mContext->getParameter(queryExtension->bytes);
        vendor_extension.extension.setParcelable(replyExtension);
        RETURN_IF(!specific, EX_NULL_POINTER, "nullPtr");
        specific->set<Parameter::Specific::vendorEffect>(vendor_extension);
        LOG(DEBUG) << "Exit: " << __func__;
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus RslAidl::setParameterVendorEffect(const Parameter::Specific& specific) {
        LOG(DEBUG) << "Enter " << __func__;
        uint32_t cmd;
        int32_t value;
        auto tag = specific.getTag();
        auto& param = specific.get<Parameter::Specific::vendorEffect>();
        std::optional<aidl::android::hardware::audio::effect::DefaultExtension> defaultExt;

        RETURN_IF(STATUS_OK != param.extension.getParcelable(&defaultExt), EX_ILLEGAL_ARGUMENT,
              "getParcelableFailed");

        RETURN_IF(!defaultExt.has_value(), EX_ILLEGAL_ARGUMENT, "parcelableNull");

        if (mContext->setParameter(defaultExt->bytes) != RetCode::SUCCESS)
        {
            return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                "ErrorSetSpecificParam");
        }
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus RslAidl::setParameterBMT(const Parameter::Specific& specific) {
        LOG(DEBUG) << "Enter " << __func__;
        auto& eq = specific.get<Parameter::Specific::equalizer>();
        auto eqTag = eq.getTag();
        switch (eqTag) {
           case Equalizer::bandLevels:
               RETURN_IF(mContext->setBMTBandLevels(eq.get<Equalizer::bandLevels>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setBandLevelsFailed");
               return ndk::ScopedAStatus::ok();
           default:
               LOG(ERROR) << __func__ << " unsupported parameter " << specific.toString();
               return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "eqTagNotSupported");
        }
        LOG(DEBUG) << "Exit " << __func__;
    }

    ndk::ScopedAStatus RslAidl::getParameterBMT(const Equalizer::Id& id,
                                                            Parameter::Specific* specific) {
        LOG(DEBUG) << "Enter " << __func__;
        RETURN_IF(id.getTag() != Equalizer::Id::commonTag, EX_ILLEGAL_ARGUMENT,
              "EqualizerTagNotSupported");
        RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");
        Equalizer eqParam;

        auto tag = id.get<Equalizer::Id::commonTag>();
        switch (tag) {
            case Equalizer::bandLevels: {
                eqParam.set<Equalizer::bandLevels>(mContext->getBMTBandLevels());
                break;
            }
            default: {
                LOG(ERROR) << __func__ << " not handled tag: " << toString(tag);
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "unsupportedTag");
            }
        }

        specific->set<Parameter::Specific::equalizer>(eqParam);

        LOG(DEBUG) << "Exit " << __func__;
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus RslAidl::setParameterBassBoost(const Parameter::Specific& specific) {
        LOG(DEBUG) << "Enter " << __func__;
        auto& bb = specific.get<Parameter::Specific::bassBoost>();
        auto bbTag = bb.getTag();
        switch (bbTag) {
            case BassBoost::strengthPm: {
                RETURN_IF(mContext->setBassBoost(bb.get<BassBoost::strengthPm>()) !=
                              RetCode::SUCCESS,
                      EX_ILLEGAL_ARGUMENT, "setStrengthFailed");
                return ndk::ScopedAStatus::ok();
            }
            default:
                LOG(ERROR) << __func__ << " unsupported parameter " << specific.toString();
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "bbTagNotSupported");
        }
        LOG(DEBUG) << "Exit " << __func__;
    }

    ndk::ScopedAStatus RslAidl::getParameterBassBoost(const BassBoost::Id& id,
                                                            Parameter::Specific* specific) {
        LOG(DEBUG) << "Enter " << __func__;
        RETURN_IF(id.getTag() != BassBoost::Id::commonTag, EX_ILLEGAL_ARGUMENT,
                     "BassBoostTagNotSupported");
        RETURN_IF(!mContext, EX_NULL_POINTER, "nullContext");
        BassBoost bbParam;

        auto tag = id.get<BassBoost::Id::commonTag>();
        switch (tag) {
            case BassBoost::strengthPm: {
                bbParam.set<BassBoost::strengthPm>(mContext->getBassBoost());
                break;
            }
            default: {
                LOG(ERROR) << __func__ << " not handled tag: " << toString(tag);
                return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                    "BassBoostTagNotSupported");
            }
        }

        specific->set<Parameter::Specific::bassBoost>(bbParam);

        LOG(DEBUG) << "Exit " << __func__;
        return ndk::ScopedAStatus::ok();
    }

}
