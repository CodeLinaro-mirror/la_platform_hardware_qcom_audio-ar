/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <iostream>
#include <expat.h>
#include <cstring> // strlen

#include <map>

#define CONFIG_FILE_PATH "/vendor/etc/audio_config.xml"
#define MAX_CONFIG 15

#define FEATURE_DISABLED 0
#define FEATURE_ENABLED 1

#define MAX_VOLUME_LEVEL 40
#define MIN_VOLUME_LEVEL 0
#define DEFAULT_MAX_VOL_STARTUP 16

#define MAX_ATTENAUATION_TARGET 0
#define MIN_ATTENAUATION_TARGET -9000
#define DEFAULT_ATTENUATION_TARGET -4000

#define MIN_TONE_CONTROLLER_BANDS 0
#define MAX_TONE_CONTROLLER_BANDS 1

#define MAX_VOLUME 0
#define MIN_VOLUME -9000
#define DEFAULT_VOLUME -3300

#define SOUND_STAGE_ENTRY 0
#define SOUND_STAGE_MID 1
#define SOUND_STAGE_PREMIUM 2
#define SOUND_STAGE_SUPER_PREM 3


#define OUTPUT_INFO_4CH 0
#define OUTPUT_INFO_8CH 1
#define OUTPUT_INFO_12CH 2
#define OUTPUT_INFO_16CH 3

#define AMBIANCE_LEVEL_1 0
#define AMBIANCE_LEVEL_2 1
#define AMBIANCE_LEVEL_3 2
#define AMBIANCE_LEVEL_4 3
#define AMBIANCE_LEVEL_5 4

// Unit is Milli Sec
#define THERMAL_PERIODICITY_MAX 5000
#define THERMAL_PERIODICITY_MIN 500
#define THERMAL_PERIODICITY_DEFAULT 5000

// Unit is Milli Sec
#define THERMAL_ATTACK_TIME_MAX 10000
#define THERMAL_ATTACK_TIME_MIN 1000
#define THERMAL_ATTACK_TIME_DEFAULT 1000

// Unit is Milli Sec
#define THERMAL_RELEASE_TIME_MAX 10000
#define THERMAL_RELEASE_TIME_MIN 1000
#define THERMAL_RELEASE_TIME_DEFAULT 10000

// Unit is DB
#define THERMAL_DELTA_STEP_MAX 6
#define THERMAL_DELTA_STEP_MIN 1
#define THERMAL_DELTA_STEP_DEFAULT 1

#define EV_ESE_FEATURE_STR "ESE-EV_Feature"
#define R_ANC_FEATURE_STR "R-ANC_Feature"
#define OUTPUT_INFO_STR "Output_information"
#define MAX_VOL_STARTUP_STR "maxVolumeStartup"
#define ATTENUATION_TARGET_STR "AttenuationTarget"
#define FADER_AVAILABILITY_STR "Fader"
#define TONE_CONTROLLER_STR "Tone_controller_bands"
#define SOUND_STAGE_STR "SoundStage"
#define DEFAULT_AMBIANCE_STR "DefaultAmbiance"
#define DEFAULT_AGC_STATE_STR "DefaultAGCState"
#define DEFAULT_VOLUME_STR "DefaultVolume"
#define THERMAL_PERIODICITY_STR "ThermalPeriodicity"
#define THERMAL_ATTACK_TIME_STR "ThermalAttackTime"
#define THERMAL_RELEASE_TIME_STR "ThermalReleaseTime"
#define THERMAL_DELTA_STEP_STR "ThermalDeltaStep"

#define TYPE_INT "int"

#define MAX_CHAR_STRING 50
namespace qti::audio::oem::config {
typedef enum
{
    AUDIO_CONFIG_ESE_EV,
    AUDIO_CONFIG_R_ANC,
    AUDIO_CONFIG_OUTPUT_INFORMATION,
    AUDIO_CONFIG_MAX_VOL_STARTUP,
    AUDIO_CONFIG_ATTENUATION_TARGET,
    AUDIO_CONFIG_FADER_AVAILABLITY,
    AUDIO_CONFIG_TONE_CONTROLLER_BANDS,
    AUDIO_CONFIG_SOUND_STAGE,
    AUDIO_CONFIG_DEFAULT_AMBIANCE,
    AUDIO_CONFIG_DEFAULT_AGC_STATE,
    AUDIO_CONFIG_DEFAULT_VOLUME,
    AUDIO_CONFIG_THERMAL_PERIODICITY,
    AUDIO_CONFIG_THERMAL_ATTACK_TIME,
    AUDIO_CONFIG_THERMAL_RELEASE_TIME,
    AUDIO_CONFIG_THERMAL_DELTA_STEP,

    // Add new Items above this
    AUDIO_CONFIG_MAX,
}AudioConfigType;


typedef struct{
    char configName[50];
    char type_info[50];
    int minValue;
    int maxValue;
    int defaultValue;
}AudioConfigData;

// Create an map for string and Key
const std::map<std::string,AudioConfigType> ConfigkeyMap =
{
    {EV_ESE_FEATURE_STR,AUDIO_CONFIG_ESE_EV},
    {R_ANC_FEATURE_STR,AUDIO_CONFIG_R_ANC},
    {OUTPUT_INFO_STR,AUDIO_CONFIG_OUTPUT_INFORMATION},
    {MAX_VOL_STARTUP_STR,AUDIO_CONFIG_MAX_VOL_STARTUP},
    {ATTENUATION_TARGET_STR,AUDIO_CONFIG_ATTENUATION_TARGET},
    {FADER_AVAILABILITY_STR,AUDIO_CONFIG_FADER_AVAILABLITY},
    {TONE_CONTROLLER_STR,AUDIO_CONFIG_TONE_CONTROLLER_BANDS},
    {SOUND_STAGE_STR,AUDIO_CONFIG_SOUND_STAGE},
    {DEFAULT_AMBIANCE_STR,AUDIO_CONFIG_DEFAULT_AMBIANCE},
    {DEFAULT_AGC_STATE_STR,AUDIO_CONFIG_DEFAULT_AGC_STATE},
    {DEFAULT_VOLUME_STR,AUDIO_CONFIG_DEFAULT_VOLUME},
    {THERMAL_PERIODICITY_STR,AUDIO_CONFIG_THERMAL_PERIODICITY},
    {THERMAL_ATTACK_TIME_STR,AUDIO_CONFIG_THERMAL_ATTACK_TIME},
    {THERMAL_RELEASE_TIME_STR,AUDIO_CONFIG_THERMAL_RELEASE_TIME},
    {THERMAL_DELTA_STEP_STR,AUDIO_CONFIG_THERMAL_DELTA_STEP},
};

class AudioConfigManager {
    public:
        static AudioConfigManager& getInstance() {
            static AudioConfigManager instance;
            return instance;
        }

        int getAudioConfigValue(AudioConfigType req, AudioConfigData* configType);
        void printXMLData();
        static AudioConfigType getTypeFromName(const char* configName);

    private:
        struct AudioXMLData {
            int config_count;
            char current_element[MAX_CHAR_STRING];
            char temp_data[MAX_CHAR_STRING];
        };

        static AudioConfigData sgconfigElement;
        static AudioConfigType sgconfigType;
        static bool sgAudioConfigInitialized;
        static std::map<AudioConfigType, AudioConfigData> sgconfigDataMap;

        static AudioConfigData sgLoadDefaultconfig[MAX_CONFIG];

        AudioConfigManager();
        ~AudioConfigManager();
        AudioConfigManager(const AudioConfigManager&) = delete;
        AudioConfigManager& operator=(const AudioConfigManager&) = delete;

        static void end(void *userData, const char *name);
        static void start(void *userData, const char *name, const char **attr);
        static void value(void *userData, const char *val, int len);

        void readConfigHUB();
        bool readDefaultXMLConfig();
        void configInit();
    };

}