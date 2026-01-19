/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Calib_QTI"

#include <include/extensions/AudioCalib.h>
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
#include <expat.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <future>
#include <mutex>


#define LOG_NDDEBUG 0
#define MAX_CHAR_STRING 255

#ifdef ENABLE_CONFIGHUB
#include <vendor/alliance/hardware/automotive/confighub/2.0/IConfigHub.h>
#include <vendor/alliance/hardware/automotive/confighub/2.0/types.h>
#endif

namespace qti::audio::oem::calib {
// Initialise Static Members
bool AudioCalibManager::sgAudioCalibInitialized = false;
AudioCalibData AudioCalibManager::mCurrentData = {};
std::string AudioCalibManager::mCurrentElement = "";
std::vector<AudioCalibData> AudioCalibManager::mCalibDataList;
std::map<std::string, std::string> AudioCalibManager::mCalibPathCache;
std::mutex AudioCalibManager::mCacheMutex;


AudioCalibManager::AudioCalibManager() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    if (sgAudioCalibInitialized == false) {
        Init();
        sgAudioCalibInitialized = true;
    }
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

AudioCalibManager::~AudioCalibManager() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    sgAudioCalibInitialized = false;
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

std::string AudioCalibManager::getAudioCalibPath(std::string stringId)
{
    // First check if we have this path cached
    std::string cachedPath = getCachedCalibPath(stringId);
    if (!cachedPath.empty()) {
        LOG(DEBUG) << __func__ << " Using cached path for " << stringId << ": " << cachedPath;
        return cachedPath;
    }

    std::string retString;

    auto it = std::find_if(mCalibDataList.begin(), mCalibDataList.end(), [&stringId](const AudioCalibData& configingId){return configingId.stringId == stringId;});

    if (it != mCalibDataList.end())
    {
        #ifdef ENABLE_CONFIGHUB
        LOG(DEBUG) << __func__ <<"ConfigHUB is enabled";
        vendor::alliance::hardware::automotive::confighub::V2_0::CalibrationData configData;
        configData.lid = it->key;
        std::string calibPath;

        auto outReadValue = [&](const android::hardware::hidl_string outVal)
        {
            LOG(DEBUG) << __func__ <<"Calib Path Value recieved is" <<outVal;
            calibPath = outVal;
        };
        auto configHub = vendor::alliance::hardware::automotive::confighub::V2_0::IConfigHub::getService();
        if (configHub == nullptr)
        {
            LOG(DEBUG) << __func__ <<"Calib value returned is  " <<retString;
            return retString;
        }
        else
        {
            configHub->getCalibrationFilePath(configData,outReadValue);
            LOG(DEBUG) << __func__ <<"Calib value returned is " <<calibPath;

            // Cache the result for future use
            if (!calibPath.empty()) {
                std::lock_guard<std::mutex> lock(mCacheMutex);
                mCalibPathCache[stringId] = calibPath;
            }

            return calibPath;
        }
        #else
            LOG(DEBUG) << __func__ <<"Calib value returned is " << it->default_path;

            // Cache the default path
            std::lock_guard<std::mutex> lock(mCacheMutex);
            mCalibPathCache[stringId] = it->default_path;

            return it->default_path;
        #endif
    }
    else
    {
        LOG(DEBUG) << __func__ <<"Requested SCD id not found " << stringId;
    }

    LOG(DEBUG) << __func__ <<"Calib value returned is  " <<retString;
    return retString;
}

void AudioCalibManager::end(void *userData, const char *name) {

     if (std::string(name) == "Calibration") {
        mCalibDataList.push_back(mCurrentData);
        mCurrentData = AudioCalibData(); // Reset for next element
     }
     mCurrentElement = "";
}

void AudioCalibManager::start(void *userData, const char *name, const char **attr) {
    mCurrentElement=name;
}

void AudioCalibManager::value(void *userData, const char *val, int len) {

    std::string data(val, len);

    if (mCurrentElement == "ID") {
        mCurrentData.id = std::stoi(data);
    } else if (mCurrentElement == "key") {
         LOG(VERBOSE) << __func__ << "key:" << data;
        mCurrentData.key = data;
    } else if (mCurrentElement == "defaultPath") {
        LOG(VERBOSE) << __func__ << "defaultPath:" << val;
        mCurrentData.default_path = data;
    }else if (mCurrentElement == "name") {
        LOG(VERBOSE) << __func__ << "name:" << val;
        mCurrentData.stringId = data;
    }
}


bool AudioCalibManager::readDefaultXMLConfig() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    XML_Parser parser = XML_ParserCreate(NULL);

    XML_SetElementHandler(parser, start, end);
    XML_SetCharacterDataHandler(parser, value);

    std::string filePath(CALIB_FILE_PATH);
     std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG(DEBUG) << __func__ <<"FOPEN file for path : " <<CALIB_FILE_PATH <<std::endl;
        XML_ParserFree(parser);
        return false;
    }

     std::stringstream buffer;
     buffer << file.rdbuf();
     if (file.fail() && !file.eof()) {
        LOG(DEBUG) << __func__ <<"FOPEN file for path : " <<CALIB_FILE_PATH <<std::endl;
        XML_ParserFree(parser);
        return false;
     }

     if (XML_Parse(parser, buffer.str().c_str(), buffer.str().size(), XML_FALSE) == XML_STATUS_ERROR) {
        XML_ParserFree(parser);
        file.close();
        return false;
    }
    buffer.clear();
    file.close();
    XML_ParserFree(parser);
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
    return true;
}

void AudioCalibManager::Init() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    if (readDefaultXMLConfig() == false) {
        LOG(ERROR) << __func__ <<"Reading XML failed"<<std::endl;
    }
    printXMLData();

    // Preload all calibration paths from ConfigHub
    if (preloadAllCalibrationPaths()) {
        LOG(DEBUG) << __func__ << " Successfully preloaded all calibration paths";
    } else {
        LOG(WARNING) << __func__ << " Some calibration paths could not be loaded from ConfigHub";
    }

    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

void AudioCalibManager::printXMLData() {
     for (const auto& data : mCalibDataList) {
        LOG(DEBUG) << __func__ << " ID: " << data.id  << ", name : " << data.stringId<< ", Key: " << data.key << ", Default Path: " << data.default_path << std::endl;
     }
}

bool AudioCalibManager::preloadAllCalibrationPaths() {
    LOG(DEBUG) << __func__ << " Entry";
    bool success = true;

#ifdef ENABLE_CONFIGHUB
    auto configHub = vendor::alliance::hardware::automotive::confighub::V2_0::IConfigHub::getService();
    if (configHub == nullptr) {
        LOG(ERROR) << __func__ << " Failed to get ConfigHub service";
        return false;
    }

    std::vector<std::future<void>> futures;
    std::mutex cacheMutex;

    // Process each calibration data entry
    for (const auto& calibData : mCalibDataList) {
        futures.push_back(std::async(std::launch::async, [&, calibData]() {
            vendor::alliance::hardware::automotive::confighub::V2_0::CalibrationData configData;
            configData.lid = calibData.key;

            std::string calibPath;
            auto outReadValue = [&calibPath](const android::hardware::hidl_string outVal) {
                calibPath = outVal;
            };

            // Call the HIDL service to get the calibration file path
            configHub->getCalibrationFilePath(configData, outReadValue);

            // If we got a valid path, store it in the cache
            if (!calibPath.empty()) {
                std::lock_guard<std::mutex> lock(cacheMutex);
                mCalibPathCache[calibData.stringId] = calibPath;
                LOG(DEBUG) << __func__ << " Updated path for " << calibData.stringId << ": " << calibPath;
            } else {
                std::lock_guard<std::mutex> lock(cacheMutex);
                // Fall back to default path if ConfigHub didn't provide one
                mCalibPathCache[calibData.stringId] = calibData.default_path;
                LOG(DEBUG) << __func__ << " Using default path for " << calibData.stringId << ": " << calibData.default_path;
                success = false;
            }
        }));
    }
    // Wait for all async operations to complete
    for (auto& future : futures) {
        future.wait();
    }
#else
    // If ConfigHub is not enabled, just use default paths
    for (const auto& calibData : mCalibDataList) {
        mCalibPathCache[calibData.stringId] = calibData.default_path;
        LOG(DEBUG) << __func__ << " Using default path for " << calibData.stringId << ": " << calibData.default_path;
    }
#endif

    LOG(DEBUG) << __func__ << " Exit, loaded " << mCalibPathCache.size() << " calibration paths";
    return success;
}

std::string AudioCalibManager::getCachedCalibPath(const std::string& stringId) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    auto it = mCalibPathCache.find(stringId);
    if (it != mCalibPathCache.end()) {
        return it->second;
    }
    return "";
}
}// qti::audio::oem::calib