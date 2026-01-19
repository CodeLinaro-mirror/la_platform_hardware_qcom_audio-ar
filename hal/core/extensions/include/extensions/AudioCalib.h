/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <iostream>
#include <expat.h>
#include <cstring> // strlen
#include <map>
#include <vector>
#include <string>
#include <mutex>

#define CALIB_FILE_PATH "/vendor/etc/audio_calibration.xml"


namespace qti::audio::oem::calib {
typedef enum
{
    AUDIO_CALIB_NONE,
    AUDIO_CALIB_MAX,
}AudioConfigType;

typedef enum AudioProcessing {
     INVALID = 0,
     DNN_DNS_HFSQ_16KHZ = 1,
     DNN_DNS_HFSQ_24KHZ = 2,
     DNN_DNS_VR_16KHZ = 3,
     SSE_BT_HF_NB_DL = 4,
     SSE_BT_HF_NB_UL = 5,
     SSE_BT_HF_WB_DL = 6,
     SSE_BT_HF_WB_UL = 7,
     SSE_CP_FT_USB_DL = 8,
     SSE_CP_FT_USB_UL = 9,
     SSE_CP_FT_WIFI_DL = 10,
     SSE_CP_FT_WIFI_UL = 11,
     SSE_CP_SIRI_USB_UL = 12,
     SSE_CP_SIRI_WIFI_UL = 13,
     SSE_CP_TEL_FB_USB_DL = 14,
     SSE_CP_TEL_FB_USB_UL = 15,
     SSE_CP_TEL_FB_WIFI_DL = 16,
     SSE_CP_TEL_FB_WIFI_UL = 17,
     SSE_CP_TEL_NB_USB_DL = 18,
     SSE_CP_TEL_NB_USB_UL = 19,
     SSE_CP_TEL_NB_WIFI_DL = 20,
     SSE_CP_TEL_NB_WIFI_UL = 21,
     SSE_CP_TEL_SWB_USB_DL = 22,
     SSE_CP_TEL_SWB_USB_UL = 23,
     SSE_CP_TEL_SWB_WIFI_DL = 24,
     SSE_CP_TEL_SWB_WIFI_UL = 25,
     SSE_CP_TEL_WB_USB_DL = 26,
     SSE_CP_TEL_WB_USB_UL = 27,
     SSE_CP_TEL_WB_WIFI_DL = 28,
     SSE_CP_TEL_WB_WIFI_UL = 29,
     SSE_WUW_BI_ESIRI_ASRAUDIOTYPE = 30,
     SSE_WUW_BI_ESIRI_UL = 31,
     // Add the New Entries Above it.
     MAX
    }AudioProcessing_t;

typedef struct{
    std::string key;
    int id;
    std::string stringId;
    std::string default_path;
}AudioCalibData;

typedef struct{
    std::string key;
    int id;
    std::string default_path;
    std::string tune_path;
    std::string calib_path;
}AudioCalibData2;


class AudioCalibManager {
    public:
        static AudioCalibManager& getInstance() {
            static AudioCalibManager instance;
            return instance;
        }
        void printXMLData();

        std::string getAudioCalibPath(std::string scd_file_name);
        
        // New methods for calibration path management
        bool preloadAllCalibrationPaths();
        std::string getCachedCalibPath(const std::string& stringId);

    private:
        static std::vector<AudioCalibData> mCalibDataList;
        static AudioCalibData mCurrentData;
        static std::string mCurrentElement;
        static bool sgAudioCalibInitialized;
        static std::map<std::string, std::string> mCalibPathCache;
        static std::mutex mCacheMutex;

        AudioCalibManager();
        ~AudioCalibManager();

        AudioCalibManager(const AudioCalibManager&) = delete;
        AudioCalibManager& operator=(const AudioCalibManager&) = delete;

        static void end(void *userData, const char *name);
        static void start(void *userData, const char *name, const char **attr);
        static void value(void *userData, const char *val, int len);

        bool readDefaultXMLConfig();
        void Init();
    };

}