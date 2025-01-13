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


#include <aidl/android/media/audio/common/AudioUsage.h>
using ::aidl::android::media::audio::common::AudioUsage;

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
    bool populateAudioFocusConfig(std::unordered_map<AudioUsage,
                                        std::unordered_map<AudioUsage, float>> &configMap);
private:
    static void startTagHandler(void* userData, const char* tagName, const char** attr);
    static void endTagHandler(void* userData, const char* tagName);

    std::vector<Property> all_priorities; // Stores all parsed properties
    Property temp_property;               // Temporary property to hold current tag data
};
