/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#pragma once
#include <android-base/logging.h>
#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <thread>
#include <expat.h>
#include <cstring>
#include <sstream>
#include <fstream>


#define DUCKED_VOLUME -8100

#include <aidl/android/media/audio/common/AudioUsage.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/AudioControlVendorParameterExt.h>
#include <aidl/ampere/hardware/interfaces/automotive/audioparameterparser/RadioVendorParameterExt.h>

using ::aidl::android::media::audio::common::AudioUsage;
using UseCase = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt::AudioFocusRequest::UseCase;
using RadioAudioSource = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::RadioVendorParameterExt::AudioSource;
using Type = ::aidl::ampere::hardware::interfaces::automotive::audioparameterparser::AudioControlVendorParameterExt::MasterMuteRequest::Type;
using StreamType = std::variant<AudioUsage, UseCase, RadioAudioSource, Type, std::string>;


struct FocusAttr {
    AudioUsage usage;
    float gain;
};

// Struct to hold the parsed XML data
struct Property {
    std::string in_src;
    std::string running_src;
    bool duck;
    int gain;
};


class BusDuckConfigParser {
public:
    BusDuckConfigParser();
    ~BusDuckConfigParser();

    bool parseConfig(const std::string& xmlFilePath);
    const std::vector<Property>& getProperties() const;
    bool populateAudioFocusConfig(std::unordered_map<StreamType,
                                                        std::unordered_map<StreamType, float> > &configMap);
private:
    static void startTagHandler(void* userData, const char* tagName, const char** attr);
    static void endTagHandler(void* userData, const char* tagName);
    std::vector<Property> all_priorities; // Stores all parsed properties
    Property temp_property;               // Temporary property to hold current tag data
};


enum {
    STATE_ON,
    STATE_OFF
};
