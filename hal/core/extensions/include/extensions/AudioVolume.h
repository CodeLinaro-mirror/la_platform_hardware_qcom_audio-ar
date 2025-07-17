/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <iostream>
#include <cstring> // strlen

#include <map>

namespace qti::audio::oem::volume {

#define MUTABLE_VOL_CURVE 0
#define UNMUTABLE_VOL_CURVE 1

class AudioVolume {
    public:
        static AudioVolume& getInstance() {
            static AudioVolume instance;
            return instance;
        }
        std::map<int32_t, int32_t>& getVolumeCurve(int index);
        int32_t getNearestAttenuation(float value,int volumeCurveIndex) ;

    private:
    static std::map<int32_t, int32_t> mMutableVolumeCurve;
    static std::map<int32_t, int32_t> mUnmutableVolumeCurve;
    const std::map<int32_t, int32_t> mDefaultVolumeCurve = {
        {0, -9000}, {1, -8775}, {2, -8550}, {3, -8325},
        {4, -8100}, {5, -7875}, {6, -7650}, {7, -7425},
        {8, -7200}, {9, -6975}, {10, -6750}, {11, -6525},
        {12, -6300}, {13, -6075}, {14, -5850}, {15, -5625},
        {16, -5400}, {17, -5175}, {18, -4950}, {19, -4725},
        {20, -4500}, {21, -4275}, {22, -4050}, {23, -3825},
        {24, -3600}, {25, -3375}, {26, -3150}, {27, -2925},
        {28, -2700}, {29, -2475}, {30, -2250}, {31, -2025},
        {32, -1800}, {33, -1575}, {34, -1350}, {35, -1125},
        {36, -900}, {37, -675}, {38, -450}, {39, -225}, {40, 0}};

    const char* externalVolumeConfiguration = "/vendor/etc/audio_policy_engine_default_volumes.xml";
    const std::map<int32_t, int32_t> processVolumePoints(const std::vector<std::string> &points) ;

    void parseVolumeProfile();
        AudioVolume();
        ~AudioVolume();
};

}