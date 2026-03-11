/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL_AudioExtension_QTI"

#include <android-base/logging.h>
#include <dlfcn.h>

#include <log/log.h>
#include "PalApi.h"
#include <extensions/hal_ecnr.h>
#include "extensions/AudioCalib.h"

const char * const scd_file_name_table[SCD_TYPE_MAX] = {
 [TEL_BT_NB_UL] = "SSE_BT_HF_NB_UL",
 [TEL_BT_NB_DL] = "SSE_BT_HF_NB_DL",
 [TEL_BT_WB_UL] = "SSE_BT_HF_WB_UL",
 [TEL_BT_WB_DL] = "SSE_BT_HF_WB_DL",
 [TEL_CP_8K_USB] = "SSE_CP_TEL_NB_USB_UL",
 [TEL_CP_8K_USB_DL] = "SSE_CP_TEL_NB_USB_DL",
 [TEL_CP_16K_USB] = "SSE_CP_TEL_WB_USB_UL",
 [TEL_CP_16K_USB_DL] = "SSE_CP_TEL_WB_USB_DL",
 [TEL_CP_32K_USB] = "SSE_CP_TEL_SWB_USB_UL",
 [TEL_CP_32K_USB_DL] = "SSE_CP_TEL_SWB_USB_DL",
 [TEL_CP_24K_USB] = "SSE_CP_FT_USB_UL",
 [TEL_CP_24K_USB_DL] = "SSE_CP_FT_USB_DL",
 [TEL_CP_48K_USB] = "SSE_CP_TEL_SWB_USB_UL",
 [TEL_CP_48K_USB_DL] = "SSE_CP_TEL_SWB_USB_DL",
 [TEL_CP_8K_WIFI] = "SSE_CP_TEL_NB_WIFI_UL",
 [TEL_CP_8K_WIFI_DL] = "SSE_CP_TEL_NB_WIFI_DL",
 [TEL_CP_16K_WIFI] = "SSE_CP_TEL_WB_WIFI_UL",
 [TEL_CP_16K_WIFI_DL] = "SSE_CP_TEL_WB_WIFI_DL",
 [TEL_CP_32K_WIFI] = "SSE_CP_TEL_SWB_WIFI_UL",
 [TEL_CP_32K_WIFI_DL] = "SSE_CP_TEL_SWB_WIFI_DL",
 [TEL_CP_24K_WIFI] = "SSE_CP_FT_WIFI_UL",
 [TEL_CP_24K_WIFI_DL] = "SSE_CP_FT_WIFI_DL",
 [TEL_CP_48K_WIFI] = "SSE_CP_TEL_SWB_WIFI_UL",
 [TEL_CP_48K_WIFI_DL] = "SSE_CP_TEL_SWB_WIFI_DL",
 [VR_16K] = "SSE_WUW_BI_ESIRI_UL",
 [LEGACY_SIRI_USB_UL] = "SSE_CP_SIRI_USB_UL",
 [LEGACY_SIRI_WIFI_UL] = "SSE_CP_SIRI_WIFI_UL",
 [FACETIME_USB] = "SSE_CP_FT_USB_UL",
 [FACETIME_USB_DL] = "SSE_CP_FT_USB_DL",
 [FACETIME_WIFI] = "SSE_CP_FT_WIFI_UL",
 [FACETIME_WIFI_DL] = "SSE_CP_FT_WIFI_DL",
};

const char * const scd_file_name_table_2nd[SCD_TYPE_MAX] = {
 [TEL_BT_NB_UL] = NULL,
 [TEL_BT_NB_DL] = NULL,
 [TEL_BT_WB_UL] = "DNN_DNS_HFSQ_16KHZ",
 [TEL_BT_WB_DL] = NULL,
 [TEL_CP_8K_USB] = NULL,
 [TEL_CP_8K_USB_DL] = NULL,
 [TEL_CP_16K_USB] = "DNN_DNS_HFSQ_24KHZ",
 [TEL_CP_16K_USB_DL] = NULL,
 [TEL_CP_32K_USB] = "DNN_DNS_HFSQ_16KHZ",
 [TEL_CP_32K_USB_DL] = NULL,
 [TEL_CP_24K_USB] = "DNN_DNS_HFSQ_24KHZ",
 [TEL_CP_24K_USB_DL] = NULL,
 [TEL_CP_48K_USB] = NULL,
 [TEL_CP_48K_USB_DL] = NULL,
 [TEL_CP_8K_WIFI] = NULL,
 [TEL_CP_8K_WIFI_DL] = NULL,
 [TEL_CP_16K_WIFI] = "DNN_DNS_HFSQ_24KHZ",
 [TEL_CP_16K_WIFI_DL] = NULL,
 [TEL_CP_32K_WIFI] = NULL,
 [TEL_CP_32K_WIFI_DL] = NULL,
 [TEL_CP_24K_WIFI] = "DNN_DNS_HFSQ_24KHZ",
 [TEL_CP_24K_WIFI_DL] = NULL,
 [TEL_CP_48K_WIFI] = "DNN_DNS_HFSQ_24KHZ",
 [TEL_CP_48K_WIFI_DL] = NULL,
 [VR_16K] = "DNN_DNS_VR_16KHZ",
 [LEGACY_SIRI_USB_UL] = NULL,
 [LEGACY_SIRI_WIFI_UL] = NULL,
 [FACETIME_USB] = "DNN_DNS_HFSQ_24KHZ",
 [FACETIME_USB_DL] = NULL,
 [FACETIME_WIFI] = "DNN_DNS_HFSQ_24KHZ",
 [FACETIME_WIFI_DL] = NULL,
};

HalECNRExtension::~HalECNRExtension() {
    if (mHandle != nullptr) {
        dlclose(mHandle);
    }
}
HalECNRExtension::HalECNRExtension() {

        mEnabled = property_get_bool(mHalECNRProperty.c_str(), false);
        LOG(INFO) << __func__ << " opening " << mLibraryName.c_str() << " enabled " << mEnabled;
        if (mEnabled) {
            mHandle = dlopen(mLibraryName.c_str(), RTLD_LAZY);
               LOG(INFO) << __func__ << " mHandle " << mHandle ;
            if (mHandle == nullptr) {
                const char *error = dlerror();
                LOG(INFO) << __func__ << " Failed to dlopen  " << mLibraryName.c_str() << " error  "
                          << error;
            }
        }
        LOG(INFO) << __func__ << " Enter";
        vocoder_sample_rate = -1;
        connect_type = -1;

        if (mHandle != nullptr) {
            if (!(ecnrCreate = (ecnrCreate_t)dlsym(
                mHandle, "sseCreate")) ||
                !(ecnrInitialize =
                (ecnrInitialize_t)dlsym(
                    mHandle, "sseInitialize")) ||
                !(ecnrDestroy =
                (ecnrDestroy_t)dlsym(
                    mHandle, "sseDestroy")) ||
                !(ecnrReset =
                (ecnrReset_t)dlsym(
                    mHandle, "sseReset")) ||
                !(ecnrProcess =
                (ecnrProcess_t)dlsym(
                    mHandle, "sseProcess")) ||
#ifdef ECNR_HAL_HIRES
                !(ecnrProcessHiRes =
                (ecnrProcessHiRes_t)dlsym(
                    mHandle, "sseProcessHiRes")) ||
#endif
                !(ecnrSetData =
                (ecnrSetData_t)dlsym(
                    mHandle, "sseSetData")) ||
                !(ecnrGetData =
                (ecnrGetData_t)dlsym(
                    mHandle, "sseGetData")) ||
                !(ecnrSetEffect =
                (ecnrSetEffect_t)dlsym(
                    mHandle, "sseSetEffect")) ||
                !(ecnrGetEffect =
                (ecnrGetEffect_t)dlsym(
                    mHandle, "sseGetEffect")) ||
                !(ecnrGetErrorMessage =
                (ecnrGetErrorMessage_t)dlsym(
                    mHandle, "sseGetErrorMessage")) ||
                !(ecnrGetVersion =
                (ecnrGetVersion_t)dlsym(
                    mHandle, "sseGetVersion")) ||
                !(ecnrSetConfigData =
                (ecnrSetConfigData_t)dlsym(
                    mHandle, "sseSetConfigData"))) {
                LOG(ERROR) << __func__ << " dlsym failed";
                goto feature_disabled;
            }
            property_get(VARIANT_PROP,hw_variant, "");
            LOG(DEBUG) << __func__ << "---- Feature HAL ECNR is Enabled ----";
            audio_extn_ecnrGetVersion();
            LOG(DEBUG) << __func__ << "HW variant postfix is : " << hw_variant;
            return;
        }

    feature_disabled:
        if (mHandle) {
            dlclose(mHandle);
            mHandle = NULL;
        }

        ecnrCreate = NULL;
        ecnrInitialize = NULL;
        ecnrDestroy = NULL;
        ecnrReset = NULL;
        ecnrProcess = NULL;
#ifdef ECNR_HAL_HIRES
        ecnrProcessHiRes = NULL;
#endif
        ecnrSetData = NULL;
        ecnrGetData = NULL;
        ecnrSetEffect = NULL;
        ecnrGetEffect = NULL;
        ecnrGetErrorMessage = NULL;
        ecnrGetVersion = NULL;
        ecnrSetConfigData = NULL;
        mEnabled = false;
        LOG(INFO) << __func__ << "----- Feature HAL ECNR is disabled ----";
    }

    bool HalECNRExtension::audio_extn_getEnablement() {
        LOG(DEBUG) << __func__ << " mEnabled ? " << mEnabled;
        return mEnabled;
    }

    int HalECNRExtension::audio_extn_ecnrCreate(tECNR_Main **Main, void* unused) {
        return ((ecnrCreate) ?
            ecnrCreate(Main,NULL) : false);
    }

    int HalECNRExtension::audio_extn_ecnrInitialize(tECNR_Main* pMain) {
        return ((ecnrInitialize) ?
            ecnrInitialize(pMain) : false);
    }

    int HalECNRExtension::audio_extn_ecnrDestroy(tECNR_Main  **pMain) {
        int ret = 0;
        char ErrorMsg[100] = "";
        if (*pMain == NULL) {
            ret = -1;
        } else if (ecnrDestroy) {
            ret = ecnrDestroy(pMain);
            if (ret) {
                audio_extn_ecnrGetErrorMessage(*pMain, ErrorMsg, sizeof(ErrorMsg) );
                LOG(ERROR) << __func__ << " failed with error  " << ret << " and error message  "
                      << ErrorMsg;
            }
        }
        return ret;
    }

    int HalECNRExtension::audio_extn_ecnrReset(tECNR_Main* pMain, const unsigned int ResetMode) {
        return ((ecnrReset) ?
            ecnrReset(pMain,ResetMode) : false);
    }

    int HalECNRExtension::audio_extn_ecnrProcess( tECNR_Main* pMain,
                    tECNR_AudioIO  AudioIO,
                    tECNR_TuneIO*  pTuneIO )
    {
        int ret = 0;
        char ErrorMsg[100] = "";
        if (ecnrProcess) {
            ret = ecnrProcess(pMain,AudioIO,pTuneIO);
            if (ret) {
                audio_extn_ecnrGetErrorMessage(pMain, ErrorMsg, sizeof(ErrorMsg) );
                LOG(ERROR) << __func__ << " failed with error  " << ret << " and error message  "
                      << ErrorMsg;
            }
           return ret;
        }
        return -1;
    }

#ifdef ECNR_HAL_HIRES
    int HalECNRExtension::audio_extn_ecnrProcessHiRes(tECNR_Main* pMain,
                          tECNR_AudioIOHiRes AudioIO,
                          tECNR_TuneIO*      pTuneIO) {
        return ((ecnrProcessHiRes) ?
            ecnrProcessHiRes(pMain,AudioIO,pTuneIO) : false);
    }

#endif
    int HalECNRExtension::audio_extn_ecnrSetData(tECNR_Main* pMain,
        const unsigned int    DataID,
        const int iChannel,
        unsigned int* puSize,
        const void* pData) {
        return ((ecnrSetData) ?
            ecnrSetData(pMain,DataID,iChannel,puSize,pData) : false);
    }

    int HalECNRExtension::audio_extn_ecnrGetData(tECNR_Main* const pMain,
        const unsigned int DataID,
        const int iChannel,
        unsigned int* puSize,
        void* pData) {
        return ((ecnrGetData) ?
            ecnrGetData(pMain,DataID,iChannel,puSize,pData) : false);
    }

    int HalECNRExtension::audio_extn_ecnrSetEffect(tECNR_Main* pMain,
        const unsigned int EffectID,
        const int Channel,
        const unsigned int InputCnt,
        int* pData) {
        return ((ecnrSetEffect) ?
            ecnrSetEffect(pMain,EffectID,Channel,InputCnt,pData) : false);
    }

    int HalECNRExtension::audio_extn_ecnrGetEffect(tECNR_Main* const pMain,
        const unsigned int EffectID,
        const int Channel,
        const unsigned int OutputCnt,
        int* pData) {
        return ((ecnrGetEffect) ?
            ecnrGetEffect(pMain,EffectID,Channel,OutputCnt,pData) : false);
    }

    int HalECNRExtension::audio_extn_ecnrGetErrorMessage( const tECNR_Main* pMain,
        char* pErrorMessage,
        const unsigned int MaxMessageLength) {
        return ((ecnrGetErrorMessage) ?
            ecnrGetErrorMessage(pMain,pErrorMessage,MaxMessageLength) : false);
    }

    void HalECNRExtension::audio_extn_ecnrGetVersion() {
        if (ecnrGetVersion) {
            unsigned int iArrayLen = 0 ;
            const int* pVersionArray = NULL; /* HAL ECNR version  array */
            const char* pVersionString = NULL; /* HAL ECNR version    string */
            const char* pVersionComment = NULL; /* HAL ECNR version  comment */
            ecnrGetVersion(&iArrayLen,&pVersionArray,&pVersionString,&pVersionComment);
            /*--- print HAL ECNR version  using the integer version array */
            LOG(INFO) << __func__ << " HAL ECNR version    numbers " << iArrayLen ;
            for (unsigned int i = 0; i < iArrayLen; i++)
            {
                LOG(INFO) << __func__ << " : " << pVersionArray[i] ;
            }
            /*--- print string based HAL ECNR version  information */
                LOG(INFO) << __func__ << " HAL ECNR version  string : " << pVersionString ;
                LOG(INFO) << __func__ << " HAL ECNR version  comment : " << pVersionComment ;
        }
        return;
    }

    int HalECNRExtension::audio_extn_ecnrSetConfigData(tECNR_Main*   pMain, const void* const pCfgData, const unsigned int uCfgDataSize) {
        return ((ecnrSetConfigData) ?
            ecnrSetConfigData(pMain,pCfgData,uCfgDataSize) : false);
    }

    int HalECNRExtension::get_vocoder_rate() const {
        return vocoder_sample_rate;
    }
    int HalECNRExtension::get_conn_type() const {
        return connect_type;
    }
    int HalECNRExtension::get_cp_type() const {
        return carplay_type;
    }
    void HalECNRExtension::set_vocoder_rate(int data) {
        vocoder_sample_rate = data;
        LOG(DEBUG) << __func__ << " vocoder_rate: " << vocoder_sample_rate;
    }
    void HalECNRExtension::set_conn_type(int data) {
        connect_type = data;
        LOG(DEBUG) << __func__ << " connection_type: " << connect_type;
    }
    void HalECNRExtension::set_cp_type(int data) {
        carplay_type = data;
        LOG(DEBUG) << __func__ << " carplay_type: " << carplay_type;
    }

    int HalECNRExtension::carplay_param_converter(char * data) {
        if (strcmp(data, "CALL") == 0)
            return CALL;
        else if (strcmp(data, "FACETIME") == 0)
            return FACETIME;
        else if (strcmp(data, "SIRI") == 0)
            return SIRI;
        else if (strcmp(data, "ESIRI_IN") == 0)
            return ESIRI_IN;
        else if (strcmp(data, "ESIRI_OUT") == 0)
            return ESIRI_OUT;
        else
            return USECASE_NONE;
    }

    void HalECNRExtension::carplay_set_parameters(struct str_parms *params) {
        int ret = 0;
        char value[32] = {0};
        if (str_parms_get_str(params, CP_SAMPLERATE, value, sizeof(value)) >= 0) {
            carplay_sample_rate = (atoi(value));
            LOG(DEBUG) << __func__ << " carplay sample rate = " << carplay_sample_rate;
        } else if (str_parms_get_str(params, CP_TYPE, value, sizeof(value)) >= 0) {
            carplay_type = carplay_param_converter(value);
            LOG(DEBUG) << __func__ << " carplay_type = " << carplay_type;
        } else if (str_parms_get_str(params, CP_VOCODER_SAMPLERATE, value, sizeof(value)) >= 0) {
            vocoder_sample_rate = (atoi(value));
            LOG(DEBUG) << __func__ << " Vocoder sample rate = " << vocoder_sample_rate;
        } else if (str_parms_get_str(params, CP_TRANSPORT, value, sizeof(value)) >= 0) {
            connect_type = (atoi(value));
            LOG(DEBUG) << __func__ << " connection_type = " << connect_type;
        } else {
           ret = -1;
           LOG(ERROR) << __func__ << " Invalid Params : " << ret;
        }
   }

    int HalECNRExtension::audio_extn_getSCDtype(uint32_t sample_rate, int vocoder_rate, uint32_t ecnr_type, int connection_type, uint32_t dir) {

        int scd_type = SCD_TYPE_INVALID;
        LOG(DEBUG) << __func__ << " vocoder_samplerate: " << vocoder_rate << " connection_type: " << connection_type << " ecnr_type: " << ecnr_type;

        if (ecnr_type == ECNR_TYPE_VR) {
            if (sample_rate == 16000) {
                return VR_16K;
            } else {
                LOG(ERROR) << __func__ << " not supported sampling rate " << sample_rate << " for ecnr_type " << ecnr_type ;
                return SCD_TYPE_INVALID;
            }
        } else if (ecnr_type == ECNR_TYPE_TEL) {
            if (connection_type == CP_CONNECTION_WIFI) {
                switch (vocoder_rate) {
                    case 48000 :
                        scd_type = TEL_CP_48K_WIFI;
                        break;
                    case 32000 :
                        scd_type = TEL_CP_48K_WIFI;
                        break;
                    case 24000 :
                        scd_type = TEL_CP_24K_WIFI;
                        break;
                    case 16000 :
                        scd_type = TEL_CP_16K_WIFI;
                        break;
                    case 8000 :
                        scd_type = TEL_CP_8K_WIFI;
                        break;
                    default :
                        LOG(ERROR) << __func__ << " not supported vocoder sampling rate " << vocoder_rate << " for ecnr_type " << ecnr_type ;
                        break;
                    }
            } else {
                switch (vocoder_rate) {
                    case 48000 :
                        scd_type = TEL_CP_32K_USB;
                        break;
                    case 32000 :
                        scd_type = TEL_CP_32K_USB;
                        break;
                    case 24000 :
                        scd_type = TEL_CP_24K_USB;
                        break;
                    case 16000 :
                        scd_type = TEL_CP_16K_USB;
                        break;
                    case 8000 :
                        scd_type = TEL_CP_8K_USB;
                        break;
                    default :
                        LOG(ERROR) << __func__ << " not supported vocoder sampling rate " << vocoder_rate << " for ecnr_type " << ecnr_type ;
                        break;
                }
            }
            if (scd_type < SCD_TYPE_MAX && scd_type > SCD_TYPE_INVALID)
                scd_type = scd_type + dir;
        } else if (ecnr_type == ECNR_TYPE_LEGACY_SIRI) {
            if (connection_type == CP_CONNECTION_WIFI)
                scd_type = LEGACY_SIRI_WIFI_UL;
            else
                scd_type = LEGACY_SIRI_USB_UL;
        } else if (ecnr_type == ECNR_TYPE_FACETIME) {
            if (connection_type == CP_CONNECTION_WIFI)
                scd_type = FACETIME_WIFI;
            else
                scd_type = FACETIME_USB;
            if (scd_type < SCD_TYPE_MAX && scd_type > SCD_TYPE_INVALID)
                scd_type = scd_type + dir;
        } else {
            LOG(ERROR) << __func__ << " invalid ecnr type " << ecnr_type ;
            return SCD_TYPE_INVALID;
        }
        LOG(ERROR) << __func__ << " return scd_type " << scd_type ;
        return scd_type;
    }
    int HalECNRExtension::find_crc(FILE *fp, char * scd_file_name, uint32_t dir, uint16_t data) {
        size_t filesize = 0;
        size_t read_num = 0;
        char * i_scd_buffer_crc = NULL;
        char crc_buffer[CRC_MAX];
        unsigned int crc;
        int ret = 0;

        if(fp == NULL)
        {
            LOG(ERROR) << __func__ << " File is not opened";
            return -1;
        }
        LOG(DEBUG) << __func__ << " SCD file name : " << scd_file_name;
        filesize = (size_t)ftell(fp);
        ret = fseek (fp,6*sizeof(uint32_t),SEEK_SET);
        if(ret) {
            LOG(ERROR) << __func__ << " fseek failed";
            return -1;
        } else {
            i_scd_buffer_crc = (char *)malloc(CRC_LEN);
            if (!i_scd_buffer_crc) {
                LOG(ERROR) << __func__ << " failed to allocate scd buffer";
                return -1;
            }

            read_num = fread(i_scd_buffer_crc,sizeof(char),CRC_LEN,fp);
            if(read_num != CRC_LEN) {
                LOG(ERROR) << __func__ << " failed to read all data from scd file file size = " << filesize << " read bytes = " << read_num;
                free(i_scd_buffer_crc);
                i_scd_buffer_crc = NULL;
                return -1;
            }
            crc = (static_cast<unsigned int>(i_scd_buffer_crc[0])) |
                        (static_cast<unsigned int>(i_scd_buffer_crc[1]) << 8) |
                        (static_cast<unsigned int>(i_scd_buffer_crc[2]) << 16) |
                        (static_cast<unsigned int>(i_scd_buffer_crc[3]) << 24);
            // Convert to hex string without "0x"
            std::snprintf(crc_buffer, sizeof(crc_buffer), "%08X", crc);

            std::string scd_crc(crc_buffer);
            std::string scd_name(scd_file_name);
            scd_name = scd_name + " + " + scd_crc;
            LOG(DEBUG) << __func__ << " scd_name + crc : " << scd_name;
            if(data == SSE) {
                if(dir)
                    property_set("vendor.audio.ecnr.scd.dl", scd_name.c_str());
                else
                    property_set("vendor.audio.ecnr.scd.ul", scd_name.c_str());
            }

            free(i_scd_buffer_crc);
            return 0;
        }
    }

    int HalECNRExtension::audio_extn_fillSCDbuffer(char * scd_file_name, uint32_t** scd_buffer, uint32_t* scd_buffer_size, uint32_t dir, uint16_t data) {
        FILE *fp_scd = NULL;
        long filesize = 0;
        long numsize = 0;
        long read_num = 0;
        int ret = 0;
        uint32_t* i_scd_buffer = NULL;
        *scd_buffer_size = 0;

        if (scd_file_name != NULL && strlen(scd_file_name)> 0) {
            fp_scd = fopen(scd_file_name, "rb");

        } else {
            LOG(ERROR) << __func__ << " not avaiable input " ;
            return -1;
        }
        if (fp_scd) {
            fseek (fp_scd,0,SEEK_END);
            filesize = ftell(fp_scd);
            fseek (fp_scd,0,SEEK_SET);
            if (filesize <=0) {
                LOG(ERROR) << __func__ << " file size error, scd_file_name = " << scd_file_name << " file size = "<< filesize;
                fclose(fp_scd);
                return -1;
            }
            numsize = filesize/(sizeof(int32_t));
            i_scd_buffer = (uint32_t *)calloc(numsize, sizeof(int32_t));
            if (!i_scd_buffer) {
                LOG(ERROR) << __func__ << " failed to allocate scd buffer";
                return -1;
            }
            read_num = fread(i_scd_buffer,sizeof(int32_t),numsize,fp_scd);
            if (read_num != numsize) {
                LOG(ERROR) << __func__ << " failed to read all data from scd file file size = " << numsize << " read bytes = " << read_num;
                free(i_scd_buffer);
                i_scd_buffer=NULL;
                return -1;
            }

            ret = find_crc(fp_scd, scd_file_name, dir, data);
            if(ret)
                LOG(ERROR) << __func__ << " find_crc failed, ret = " << ret;

            *scd_buffer_size = filesize;
            *scd_buffer = i_scd_buffer;
            fclose(fp_scd);
            LOG(DEBUG) << __func__ << " scd_file_name = " << scd_file_name <<" scd_buffer_size = " << filesize;
        } else {
            LOG(ERROR) << __func__ << " error in opening file scd_file_name = " << scd_file_name;
            return -1;
        }
        return 0;
    }
    int HalECNRExtension::audio_extn_getSCDdata(tECNR_ProcessData* pECNR_ProcessData, uint32_t dir, int *current_file_path) {

        long filesize = 0;
        long numsize = 0;
        long read_num = 0;
        int32_t* i_scd_buffer = NULL;
        int ret = 0, rc = 0;
        int i_scd_type = SCD_TYPE_INVALID;
        char scd_file_name[128];
        char scd_file_path[256];
        int32_t tuning_mode = property_get_int32(TUNE_MODE_PROP, 0);
        if (NULL == pECNR_ProcessData )
        {
            LOG(ERROR) << __func__ << " pECNR_ProcessData must not be NULL";
            ret = -1;
            return ret;
        }
        i_scd_type = pECNR_ProcessData->scd_type;
        LOG(DEBUG) << __func__ << " tuning_mode : " << tuning_mode ;
        if (i_scd_type < SCD_TYPE_MAX && i_scd_type > SCD_TYPE_INVALID) {
            LOG(INFO) << __func__ << " scd_type : " << i_scd_type ;
        } else {
            LOG(ERROR) << __func__ << " not supported scd_type " << i_scd_type ;
            return -1 ;
        }
        for (int i= 0 ; i < MAX_PARAM_FILES ; i++) {
            pECNR_ProcessData->scd_buffer[i] = NULL;
            pECNR_ProcessData->scd_buffer_size[i] = 0;
        }
        std::string calibReq(scd_file_name_table[i_scd_type]);
        std::string retCalibPath =::qti::audio::oem::calib::AudioCalibManager::getInstance().getAudioCalibPath(calibReq);
        LOG(DEBUG) << "getAudioCalibPath for SCD returned  " << retCalibPath;
        if (scd_file_name_table[i_scd_type] != NULL || scd_file_name_table_2nd[i_scd_type] != NULL) {

            if (scd_file_name_table[i_scd_type] != NULL) {
                if (strlen(hw_variant) == 0) {
                    snprintf(scd_file_name, sizeof(scd_file_name), "%s%s", scd_file_name_table[i_scd_type], ".scd");
                }
                else {
                    snprintf(scd_file_name, sizeof(scd_file_name), "%s_%s%s", scd_file_name_table[i_scd_type], hw_variant, ".scd");
                }
            }
            if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                if (strlen(hw_variant) == 0) {
                    snprintf(scd_file_name, sizeof(scd_file_name), "%s%s", scd_file_name_table_2nd[i_scd_type], ".scd");
                }
                else {
                    snprintf(scd_file_name, sizeof(scd_file_name), "%s_%s%s", scd_file_name_table_2nd[i_scd_type], hw_variant, ".scd");
                }
            }

            if (tuning_mode) {
                if (scd_file_name_table[i_scd_type] != NULL) {
                    snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                    LOG(DEBUG) << __func__ << " scd_file_path: " << scd_file_path;
                    ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]), &(pECNR_ProcessData->scd_buffer_size[0]), dir, SSE);
                    if(ret) {
                        LOG(ERROR) << __func__ << " Invalid scd path";
                        if(dir)
                            property_set("vendor.audio.ecnr.scd.dl", "");
                        else
                            property_set("vendor.audio.ecnr.scd.ul", "");
                        return ret;
                    }
                }
                if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                    snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                    LOG(DEBUG) << __func__ << " DNN scd_file_path: " << scd_file_path;
                    ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]), &(pECNR_ProcessData->scd_buffer_size[1]), dir, DNN);
                    if(ret) {
                        LOG(ERROR) << __func__ << " Invalid scd path for DNN";
                        return ret;
                    }
                }
            } else {
                if (current_file_path == NULL) {
                    ret = -1;
                    LOG(ERROR) << __func__ << " current_file_path is Null ret = " << ret;
                    return ret;
                } else if (*current_file_path == INVALID_PATH) {
                    ret = 0, rc = 0;
                    if (scd_file_name_table[i_scd_type] != NULL) {
                        snprintf(scd_file_path, sizeof(scd_file_path), "%s", retCalibPath.c_str());
                        LOG(DEBUG) << __func__ << " scd_file_path: " << scd_file_path;
                        ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]), &(pECNR_ProcessData->scd_buffer_size[0]), dir, SSE);
                    }
                    if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                        std::string calibReqdnn(scd_file_name_table_2nd[i_scd_type]);
                        std::string retCalibPathdnn =::qti::audio::oem::calib::AudioCalibManager::getInstance().getAudioCalibPath(calibReqdnn);
                        LOG(DEBUG) << "getAudioCalibPath returned  " << retCalibPathdnn;
                        // reset the variable before use
                        memset(scd_file_path, 0, sizeof(scd_file_path));
                        snprintf(scd_file_path, sizeof(scd_file_path), "%s", retCalibPathdnn.c_str());
                        LOG(DEBUG) << __func__ << " DNN scd_file_path: " << scd_file_path;

                        rc = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]), &(pECNR_ProcessData->scd_buffer_size[1]), dir, DNN);
                    }
                    if (!ret && !rc) {
                        *current_file_path = CALIB_PATH;
                    } else {
                        LOG(WARNING) << __func__ << " CALIB failed for one/both files, fallback to TUNING";
                        ret = 0, rc = 0;
                        if (scd_file_name_table[i_scd_type] != NULL) {
                            snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                            LOG(DEBUG) << __func__ << " scd_file_path: " << scd_file_path;
                            ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]), &(pECNR_ProcessData->scd_buffer_size[0]), dir, SSE);
                        }
                        if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                            snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                            LOG(DEBUG) << __func__ << " DNN scd_file_path: " << scd_file_path;
                            rc = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]), &(pECNR_ProcessData->scd_buffer_size[1]), dir, DNN);
                        }
                        if (!ret && !rc) {
                            *current_file_path = TUNING_PATH;
                        } else {
                            LOG(WARNING) << __func__ << " TUNING failed for one/both files, fallback to DEFAULT";
                            ret = 0, rc = 0;
                            if (scd_file_name_table[i_scd_type] != NULL) {
                                snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH_BK, scd_file_name);
                                LOG(DEBUG) << __func__ << " scd_file_path: " << scd_file_path;
                                ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]), &(pECNR_ProcessData->scd_buffer_size[0]), dir, SSE);
                            }
                            if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                                snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH_BK, scd_file_name);
                                LOG(DEBUG) << __func__ << " DNN scd_file_path: " << scd_file_path;
                                rc = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]), &(pECNR_ProcessData->scd_buffer_size[1]), dir, DNN);
                            }
                            if (!ret && !rc) {
                                *current_file_path = DEFAULT_PATH;
                            } else{
                                LOG(ERROR) << __func__ << " Couldn't find scd file";
                                if(dir)
                                    property_set("vendor.audio.ecnr.scd.dl", "");
                                else
                                    property_set("vendor.audio.ecnr.scd.ul", "");
                                return ret;
                            }
                        }
                    }
                } else if (*current_file_path == TUNING_PATH) {
                    ret = 0, rc = 0;
                    if (scd_file_name_table[i_scd_type] != NULL) {
                        snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                        ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]), &(pECNR_ProcessData->scd_buffer_size[0]), dir, SSE);
                    }
                    if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                        snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                        rc = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]), &(pECNR_ProcessData->scd_buffer_size[1]), dir, DNN);
                    }
                    if (ret && rc) {
                        LOG(ERROR) << __func__ << " Invalid scd path";
                        if(dir)
                            property_set("vendor.audio.ecnr.scd.dl", "");
                        else
                            property_set("vendor.audio.ecnr.scd.ul", "");
                        return ret;
                    }
                } else {
                    if (scd_file_name_table[i_scd_type] != NULL) {
                        snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH_BK, scd_file_name);
                        ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]), &(pECNR_ProcessData->scd_buffer_size[0]), dir, SSE);
                    }
                    if (scd_file_name_table_2nd[i_scd_type] != NULL) {
                        snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH_BK, scd_file_name);
                        ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]), &(pECNR_ProcessData->scd_buffer_size[1]), dir, DNN);
                    }
                    if (ret && rc) {
                        LOG(ERROR) << __func__ << " Invalid scd path";
                        if(dir)
                            property_set("vendor.audio.ecnr.scd.dl", "");
                        else
                            property_set("vendor.audio.ecnr.scd.ul", "");
                        return ret;
                    }
                }
            }
        }
        return ret;
    }

    int HalECNRExtension::audio_extn_setupIOBuffer(tECNR_ProcessData* pECNR_ProcessData, int dir, int in_ch, int out_ch, int framesize, void* in_buffer, void* out_buffer)
    {
        int ret = 0;

        if (NULL == pECNR_ProcessData) {
            LOG(ERROR) << __func__ << " pECNR_ProcessData must not be NULL";
            ret = -1;
            return ret;
        }
        pECNR_ProcessData->pMain = NULL;
        pECNR_ProcessData->audioIO.AudioBufferSampleCnt = framesize;
        pECNR_ProcessData->audioIO.RcvInBufferCnt = 0;
        pECNR_ProcessData->audioIO.RcvInBuffers = NULL;
        pECNR_ProcessData->audioIO.RcvProcBufferCnt = 0;
        pECNR_ProcessData->audioIO.RcvProcBuffers = NULL;

        pECNR_ProcessData->audioIO.RefInBufferCnt = 0;
        pECNR_ProcessData->audioIO.RefInBuffers = NULL;
        pECNR_ProcessData->audioIO.MicInBufferCnt = 0;
        pECNR_ProcessData->audioIO.MicInBuffers = NULL;
        pECNR_ProcessData->audioIO.MicProcBufferCnt = 0;
        pECNR_ProcessData->audioIO.MicProcBuffers = NULL;

        short int **pIn = new short int*[in_ch];
        for ( int i = 0; i < in_ch; ++i ) {
            pIn[i] = (short int *)in_buffer + i*framesize;
        }
        short int **pProc = new short int*[out_ch];
        for ( int i = 0; i < out_ch; ++i ) {
            pProc[i] = (short int *)out_buffer + i*framesize;
        }

        if (dir == DIR_DL) {
          LOG(DEBUG) << __func__ << " setup IOBuffer for Downlink framesize = "<< framesize << " in_ch = " << in_ch << "out_ch = " << out_ch;
            pECNR_ProcessData->audioIO.RcvInBufferCnt = in_ch;
            pECNR_ProcessData->audioIO.RcvInBuffers = pIn;
            pECNR_ProcessData->audioIO.RcvProcBufferCnt = out_ch;
            pECNR_ProcessData->audioIO.RcvProcBuffers = pProc;
        } else {
          LOG(DEBUG) << __func__ << " setup IOBuffer for Uplink framesize = "<< framesize << " in_ch = " << in_ch << "out_ch = " << out_ch;
            pECNR_ProcessData->audioIO.RefInBufferCnt = ECNR_EC_REF_CH;
            pECNR_ProcessData->audioIO.RefInBuffers = &(pIn[ECNR_MIC_CH]);
            pECNR_ProcessData->audioIO.MicInBufferCnt = ECNR_MIC_CH;
            pECNR_ProcessData->audioIO.MicInBuffers = pIn;
            pECNR_ProcessData->audioIO.MicProcBufferCnt = out_ch;
            pECNR_ProcessData->audioIO.MicProcBuffers = pProc;
        }
        return ret;
    }

    int HalECNRExtension::audio_extn_resetIOBuffer(tECNR_ProcessData* pECNR_ProcessData)
    {
        int ret = 0;
       if (NULL==pECNR_ProcessData) {
            LOG(ERROR) << __func__ << " pECNR_ProcessData must not be NULL";
            ret = -1;
            return ret;
        }
        if (pECNR_ProcessData->audioIO.RcvInBuffers) {
            delete[] pECNR_ProcessData->audioIO.RcvInBuffers;
        }
        if (pECNR_ProcessData->audioIO.RcvProcBuffers) {
            delete[] pECNR_ProcessData->audioIO.RcvProcBuffers;
       }
        if (pECNR_ProcessData->audioIO.MicInBuffers) {
            delete[] pECNR_ProcessData->audioIO.MicInBuffers;
        }
       if (pECNR_ProcessData->audioIO.MicProcBuffers) {
            delete[] pECNR_ProcessData->audioIO.MicProcBuffers;
        }
        pECNR_ProcessData->audioIO.AudioBufferSampleCnt = 0;
        pECNR_ProcessData->audioIO.RcvInBufferCnt = 0;
        pECNR_ProcessData->audioIO.RcvInBuffers = NULL;
        pECNR_ProcessData->audioIO.RcvProcBufferCnt = 0;
        pECNR_ProcessData->audioIO.RcvProcBuffers = NULL;
        pECNR_ProcessData->audioIO.RefInBufferCnt = 0;
        pECNR_ProcessData->audioIO.RefInBuffers = NULL;
        pECNR_ProcessData->audioIO.MicInBufferCnt = 0;
        pECNR_ProcessData->audioIO.MicInBuffers = NULL;
        pECNR_ProcessData->audioIO.MicProcBufferCnt = 0;
        pECNR_ProcessData->audioIO.MicProcBuffers = NULL;
        return ret;
    }

    int HalECNRExtension::audio_extn_setupECNR(tECNR_ProcessData* pECNR_ProcessData)
    {
       int ret = 0;
       char ErrorMsg[100] = "";
       if (NULL==pECNR_ProcessData) {
            LOG(ERROR) << __func__ << " pECNR_ProcessData must not be NULL";
            ret = -1;
            return ret;
       }
       ret = audio_extn_ecnrCreate( &pECNR_ProcessData->pMain, NULL );

       if (!ret) {
            for (int i= 0 ; i < MAX_PARAM_FILES ; i++) {
                if (pECNR_ProcessData->scd_buffer_size[i] > 0) {
                    ret = audio_extn_ecnrSetConfigData( pECNR_ProcessData->pMain, pECNR_ProcessData->scd_buffer[i], pECNR_ProcessData->scd_buffer_size[i]);
                    if (ret) break;
                }
            }
       }

       if (!ret) {
          ret = audio_extn_ecnrInitialize( pECNR_ProcessData->pMain );
       }

       if (ret) {
            /*--- get and display error message */
            audio_extn_ecnrGetErrorMessage(pECNR_ProcessData->pMain, ErrorMsg, sizeof(ErrorMsg) );
            LOG(ERROR) << __func__ << " failed with error " << ret << " and error message " << ErrorMsg;
       }
       return ret;
    }

    int HalECNRExtension::audio_extn_cvtformat16_lnterleave_to_deinterleave(void* int_buffer, void* deint_buffer,int frameSz, int numchannel)
    {
        int16_t *src_buffer = (int16_t *)int_buffer;
        int16_t *dst_buffer = (int16_t *)deint_buffer;
        if (int_buffer && deint_buffer && frameSz >0) {
            for (int k = 0 ; k < numchannel ; k++) {
                for (int j = 0; j < frameSz; j++) {
                      dst_buffer[j+k*frameSz] = src_buffer[k+j*numchannel];
                }
            }
            return 0;
        }
        return -1;
    }

    int HalECNRExtension::audio_extn_cvtformat16_delnterleave_to_interleave(void* deint_buffer, void* int_buffer, int frameSz, int numchannel)
    {
        int16_t *src_buffer = (int16_t *)deint_buffer;
        int16_t *dst_buffer = (int16_t *)int_buffer;
        if (int_buffer && deint_buffer && frameSz >0) {
            for (int k = 0 ; k < numchannel ; k++) {
                for (int j = 0; j < frameSz; j++) {
                      dst_buffer[k+j*numchannel] = src_buffer[j+k*frameSz];
                }
            }
            return 0;
        }
        return -1;
    }
