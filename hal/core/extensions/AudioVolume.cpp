/*
 * ​​​​​Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Volume_QTI"

#include "include/extensions/AudioVolume.h"
#include <android-base/logging.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <errno.h>
#include <log/log.h>
#include <math.h>
#include <unistd.h>
#include <map>
#include <string>
#include <cstring>

#include "android_audio_policy_configuration.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#define LOG_NDDEBUG 0

namespace qti::audio::oem::volume {

std::map<int32_t, int32_t> AudioVolume::mMutableVolumeCurve;
std::map<int32_t, int32_t> AudioVolume::mUnmutableVolumeCurve;

    template <class T>
    constexpr void (*xmlDeleter)(T* t);
    template <>
    constexpr auto xmlDeleter<xmlDoc> = xmlFreeDoc;
    template <>
    auto xmlDeleter<xmlChar> = [](xmlChar *s) { xmlFree(s); };
    template <class T>
    constexpr auto make_xmlUnique(T *t) {
        auto deleter = [](T *t) { xmlDeleter<T>(t); };
        return std::unique_ptr<T, decltype(deleter)>{t, deleter};
    }
    std::optional<::android::audio::policy::configuration::Volumes>
                                volProfileRead(const char* configFile) {
        auto doc = make_xmlUnique(xmlParseFile(configFile));
        if (doc == nullptr) {
            return std::nullopt;
        }
        xmlNode* _child = xmlDocGetRootElement(doc.get());
        if (_child == nullptr) {
            return std::nullopt;
        }
        if (xmlXIncludeProcess(doc.get()) < 0) {
            return std::nullopt;
        }

        if (!xmlStrcmp(_child->name, reinterpret_cast<const xmlChar*>("volumes"))) {
            ::android::audio::policy::configuration::Volumes
                _value = ::android::audio::policy::configuration::Volumes::read(_child);
            return _value;
        }
        return std::nullopt;
    }

const std::map<int32_t, int32_t>  AudioVolume::processVolumePoints(const std::vector<std::string> &points)
{
        std::map<int32_t, int32_t> volumeMap;
        int32_t index, gain;
        LOG(INFO) << "Dynamic Volume Curve:";
        for (auto point: points) {
            size_t p = point.find(',');
            if (p != std::string::npos) {
                index = std::stoi(point.substr(0, p));
                gain = std::stoi(point.substr(p + 1));
                volumeMap[index] = gain;
                LOG(DEBUG) << index << " : " << gain;
            }
        }
        return volumeMap;
}

void AudioVolume::parseVolumeProfile()
{
    const std::optional<::android::audio::policy::configuration::Volumes> volumeInfo(volProfileRead(externalVolumeConfiguration));
    if (volumeInfo.has_value() && volumeInfo->hasReference()) {
        const std::vector<::android::audio::policy::configuration::Reference>
            references = volumeInfo->getReference();
        for (auto reference: references) {
            if (reference.hasName() && reference.hasPoint()) {
                const auto& points = reference.getPoint();
                if (reference.getName() == "DEFAULT_VOLUME_STEPS_CURVE") {
                    mMutableVolumeCurve = processVolumePoints(points);
                } else if (reference.getName() == "NOT_MUTABLE_VOLUME_STEPS_CURVE_5TO40") {
                    mUnmutableVolumeCurve = processVolumePoints(points);
                }
            }
        }
    }
    // Copy the default map is anyt of the volume curve is not available
    if (mMutableVolumeCurve.empty())
    {
        mMutableVolumeCurve = mDefaultVolumeCurve;
    }
    // Copy the default map is anyt of the volume curve is not available
    if (mUnmutableVolumeCurve.empty())
    {
        mUnmutableVolumeCurve = mDefaultVolumeCurve;
    }
    return ;
}


AudioVolume::AudioVolume() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    parseVolumeProfile();
    for (const auto& pair : mMutableVolumeCurve) {
        LOG(DEBUG)  << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
    }
    for (const auto& pair : mUnmutableVolumeCurve) {
        LOG(DEBUG)  << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
    }


    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

AudioVolume::~AudioVolume() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    mMutableVolumeCurve.clear();
    mUnmutableVolumeCurve.clear();
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

std::map<int32_t, int32_t>& AudioVolume::getVolumeCurve(int index)
{
    if (index == MUTABLE_VOL_CURVE)
    {
        return mMutableVolumeCurve;
    }

    return mUnmutableVolumeCurve;

}

int32_t AudioVolume::getNearestAttenuation(float value, int volumeCurveIndex)
{
    const std::map<int32_t,int32_t>& mapReference = getVolumeCurve(volumeCurveIndex);
    int minIndex = mapReference.begin()->first;
    int maxIndex = mapReference.rbegin()->first;

    // First get index (rounded on closest integer)
    int range = maxIndex - minIndex;
    int index = static_cast<int>(std::round(value * range)) + minIndex;
    LOG(DEBUG)  << __func__ << " value: " << value << ", index: " << value * range + minIndex
            << ", rounded index: " << index << ", clamped index: "
            << std::clamp(minIndex, index, maxIndex);

    // Clamp index to prevent out of bounds
    index = std::clamp(minIndex, index, maxIndex);

    // Get attenuation
    auto it = mapReference.lower_bound(index);
    if (it == mapReference.begin()) {
        return it->second;
    }
    if (it == mapReference.end()) {
        return std::prev(it)->second;
    }
    auto prevIt = std::prev(it);
    if (std::abs(index - prevIt->first) <= std::abs(index - it->first)) {
        return prevIt->second;
    }
    return it->second;
}

}
