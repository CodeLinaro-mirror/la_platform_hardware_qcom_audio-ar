/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_Config_QTI"

#include <include/extensions/AudioConfig.h>
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

#define LOG_NDDEBUG 0

namespace qti::audio::oem::config {
// Initialise Static Members
AudioConfigData AudioConfigManager::sgconfigElement = {};
AudioConfigType AudioConfigManager::sgconfigType = AUDIO_CONFIG_MAX;
std::map<AudioConfigType, AudioConfigData> AudioConfigManager::sgconfigDataMap;

AudioConfigData AudioConfigManager::sgLoadDefaultconfig[MAX_CONFIG] = {
    {EV_ESE_FEATURE_STR, TYPE_INT, FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_DISABLED},
    {R_ANC_FEATURE_STR, TYPE_INT, FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_DISABLED},
    {OUTPUT_INFO_STR, TYPE_INT, FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_DISABLED},
    {MAX_VOL_STARTUP_STR, TYPE_INT, MIN_VOLUME_LEVEL, MIN_VOLUME_LEVEL, DEFAULT_MAX_VOL_STARTUP},
    {ATTENUATION_TARGET_STR, TYPE_INT, MIN_ATTENAUATION_TARGET, MIN_ATTENAUATION_TARGET, DEFAULT_ATTENUATION_TARGET},
    {FADER_AVAILABILITY_STR, TYPE_INT, FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_ENABLED},
    {TONE_CONTROLLER_STR, TYPE_INT, MIN_TONE_CONTROLLER_BANDS, MAX_TONE_CONTROLLER_BANDS, MIN_TONE_CONTROLLER_BANDS},
    {SOUND_STAGE_STR, TYPE_INT, SOUND_STAGE_ENTRY, SOUND_STAGE_SUPER_PREM, SOUND_STAGE_PREMIUM},
    {DEFAULT_AMBIANCE_STR, TYPE_INT, AMBIANCE_LEVEL_1, AMBIANCE_LEVEL_5, AMBIANCE_LEVEL_1},
    {DEFAULT_AGC_STATE_STR, TYPE_INT, FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_ENABLED},
};


AudioConfigManager::AudioConfigManager() : sgAudioConfigInitialized(false) {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    memset(&sgconfigElement, 0, sizeof(sgconfigElement));
    sgconfigType = AUDIO_CONFIG_MAX;
    if (sgAudioConfigInitialized == false) {
        configInit();
        sgAudioConfigInitialized = true;
    }
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

AudioConfigManager::~AudioConfigManager() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    sgAudioConfigInitialized = false;
    sgconfigDataMap.empty();
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

AudioConfigType AudioConfigManager::getTypeFromName(const char* configName) {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    AudioConfigType type = AUDIO_CONFIG_MAX;
    std::string keyName(configName);

    if (configName != NULL) {
        auto itr = ConfigkeyMap.find(keyName);
        if (itr != ConfigkeyMap.end()) {
            type = itr->second;
        }
    }
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
    return type;
}

void AudioConfigManager::end(void *userData, const char *name) {
    AudioXMLData *data = (AudioXMLData *)userData;
    AudioConfigData *current_device = &sgconfigElement;
    AudioConfigType *current_type = &sgconfigType;

    if (strcmp(name, "config") == 0) {
        if (*current_type != AUDIO_CONFIG_MAX) {
            sgconfigDataMap.insert(std::make_pair(*current_type, *current_device));
        }
        else {
            LOG(ERROR) << __LINE__ << "Config Item Not Valid or not Found"<<std::endl;
        }
        memset(&sgconfigElement, 0, sizeof(AudioConfigData));
    }
    memset(data->current_element, 0, sizeof(data->current_element));
}

void AudioConfigManager::start(void *userData, const char *name, const char **attr) {
    AudioXMLData *data = (AudioXMLData *)userData;
    memset(data->temp_data, 0, sizeof(data->temp_data));
    memcpy(data->current_element, name, sizeof(data->current_element));
}

void AudioConfigManager::value(void *userData, const char *val, int len) {
    AudioXMLData *data = (AudioXMLData *)userData;
    AudioConfigData *current_device = &sgconfigElement;
    AudioConfigType *current_type = &sgconfigType;

    if (len >= sizeof(data->temp_data)) {
        len = sizeof(data->temp_data) - 1;
    }

    memcpy(data->temp_data, val, len);
    data->temp_data[len] = '\0';

    if (strcmp(data->current_element, "name") == 0) {
        memcpy(current_device->configName, data->temp_data, len);
        *current_type = getTypeFromName(current_device->configName);
    } else if (strcmp(data->current_element, "type") == 0) {
        memcpy(current_device->type_info, data->temp_data, len);
    } else if (strcmp(data->current_element, "min") == 0) {
        current_device->minValue = atoi(data->temp_data);
    } else if (strcmp(data->current_element, "max") == 0) {
        current_device->maxValue = atoi(data->temp_data);
    } else if (strcmp(data->current_element, "defaultvalue") == 0) {
        current_device->defaultValue = atoi(data->temp_data);
    }
    memset(data->temp_data, 0, sizeof(data->temp_data));
}

void AudioConfigManager::readConfigHUB() {
    // Update Default Values from ConfigHUB;
    // Read from Config HUB Later Once LGE provides the contract
}

bool AudioConfigManager::readDefaultXMLConfig() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    AudioXMLData user_data = {0};
    XML_Parser parser = XML_ParserCreate(NULL);

    XML_SetUserData(parser, &user_data);
    XML_SetElementHandler(parser, start, end);
    XML_SetCharacterDataHandler(parser, value);

    char readbuffer[BUFSIZ];
    size_t len;

    FILE *fh = fopen(CONFIG_FILE_PATH, "r");
    if (!fh) {
        XML_ParserFree(parser);
        return false;
    }

    while ((len = fread(readbuffer, 1, sizeof(readbuffer), fh)) > 0) {
        if (XML_Parse(parser, readbuffer, len, XML_FALSE) == XML_STATUS_ERROR) {
            XML_ParserFree(parser);
            fclose(fh);
            return false;
        }
    }

    if (XML_Parse(parser, NULL, 0, XML_TRUE) == XML_STATUS_ERROR) {
        fclose(fh);
        XML_ParserFree(parser);
        return false;
    }

    fclose(fh);
    XML_ParserFree(parser);
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
    return true;
}

void AudioConfigManager::configInit() {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    if (readDefaultXMLConfig() == false) {
        for (auto it = 0; it < MAX_CONFIG; it++) {
            AudioConfigType configType = getTypeFromName(sgLoadDefaultconfig[it].configName);
            sgconfigDataMap.insert(std::make_pair(configType, sgLoadDefaultconfig[it]));
        }
    }
    printXMLData();
    readConfigHUB();
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
}

void AudioConfigManager::printXMLData() {
    for (auto itr = sgconfigDataMap.begin(); itr != sgconfigDataMap.end(); itr++) {
        LOG(DEBUG) << "Current config Type: " << itr->first << " Name: " << itr->second.configName << " type_name: " << itr->second.type_info << " min: " << itr->second.minValue << " max: " << itr->second.maxValue << " def: " << itr->second.defaultValue << std::endl;
    }
}

int AudioConfigManager::getAudioConfigValue(AudioConfigType req, AudioConfigData* configType) {
    LOG(DEBUG) << __func__ <<"Entry"<<std::endl;
    int ret = -1;

    if ((configType != NULL) && (req < AUDIO_CONFIG_MAX)) {
        LOG(DEBUG) << __LINE__ << std::endl;

        for (auto it = 0; it < MAX_CONFIG; it++) {
            auto itr = sgconfigDataMap.find(req);
            if (itr == sgconfigDataMap.end()) {
                LOG(DEBUG) << __LINE__ << std::endl;
            } else {
                memcpy(configType->configName, itr->second.configName,MAX_CHAR_STRING);
                memcpy(configType->type_info, itr->second.type_info,MAX_CHAR_STRING);
                configType->maxValue = itr->second.maxValue;
                configType->minValue = itr->second.minValue;
                configType->defaultValue = itr->second.defaultValue;
                ret = 0;
                break;
            }
        }
    } else {
        LOG(DEBUG) << "Get Audio COnfig Failed for Req "<< std::endl;
        ret = -1;
    }
    LOG(DEBUG) << __func__ <<"Exit"<<std::endl;
    return ret;
}

}