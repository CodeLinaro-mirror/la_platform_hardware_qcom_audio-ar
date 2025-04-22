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

#ifdef __cplusplus
extern "C" {
#endif

struct ThermalCondition {
    int tempLower;
    int tempUpper;
    int gain;
};

class ThermalParser {
    public:
        ThermalParser();
        ~ThermalParser();
        bool parseConfig(const std::string& xmlFilePath);
        const std::vector<ThermalCondition>& getConditions() const;

    private:
        static void startTagHandler(void* userData, const char* tagName, const char** attr);
        static void endTagHandler(void* userData, const char* tagName);
        std::vector<ThermalCondition> all_conditions; // Stores all parsed conditions
        ThermalCondition temp_property;               // Temporary condtion to hold current tag data
};


#ifdef __cplusplus
}
#endif
