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

std::string  AudioCalibManager::getAudioCalibPath(std::string stringId)
{
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
            return calibPath;
        }
        #else
            LOG(DEBUG) << __func__ <<"Calib value returned is " << it->default_path;
            return it->default_path;
        #endif
     }
     else
     {
        LOG(DEBUG) << __func__ <<"Requested SCD id  not found " << stringId;
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
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

void AudioCalibManager::printXMLData() {
     for (const auto& data : mCalibDataList) {
        LOG(DEBUG) << __func__ << " ID: " << data.id  << ", name : " << data.stringId<< ", Key: " << data.key << ", Default Path: " << data.default_path<< std::endl;
     }
}
}// qti::audio::oem::calib