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

const char * const scd_file_name_table[SCD_TYPE_MAX] = {
 [TEL_BT_NB_UL] = "TEL_BT_NB_UL",
 [TEL_BT_NB_DL] = "TEL_BT_NB_DL",
 [TEL_BT_WB_UL] = "TEL_BT_WB_UL",
 [TEL_BT_WB_DL] = "TEL_BT_WB_DL",
 [TEL_CP_8K_USB] = "TEL_CP_8K_USB_UL",
 [TEL_CP_8K_USB_DL] = "TEL_CP_8K_USB_DL",
 [TEL_CP_16K_USB] = "TEL_CP_16K_USB_UL",
 [TEL_CP_16K_USB_DL] = "TEL_CP_16K_USB_DL",
 [TEL_CP_32K_USB] = "TEL_CP_32K_USB_UL",
 [TEL_CP_32K_USB_DL] = "TEL_CP_32K_USB_DL",
 [TEL_CP_24K_USB] = "TEL_CP_24K_USB_UL",
 [TEL_CP_24K_USB_DL] = "TEL_CP_24K_USB_DL",
 [TEL_CP_48K_USB] = "TEL_CP_48K_USB_UL",
 [TEL_CP_48K_USB_DL] = "TEL_CP_48K_USB_DL",
 [TEL_CP_8K_WIFI] = "TEL_CP_8K_WIFI_UL",
 [TEL_CP_8K_WIFI_DL] = "TEL_CP_8K_WIFI_DL",
 [TEL_CP_16K_WIFI] = "TEL_CP_16K_WIFI_UL",
 [TEL_CP_16K_WIFI_DL] = "TEL_CP_16K_WIFI_DL",
 [TEL_CP_32K_WIFI] = "TEL_CP_32K_WIFI_UL",
 [TEL_CP_32K_WIFI_DL] = "TEL_CP_32K_WIFI_DL",
 [TEL_CP_24K_WIFI] = "TEL_CP_24K_WIFI_UL",
 [TEL_CP_24K_WIFI_DL] = "TEL_CP_24K_WIFI_DL",
 [TEL_CP_48K_WIFI] = "TEL_CP_48K_WIFI_UL",
 [TEL_CP_48K_WIFI_DL] = "TEL_CP_48K_WIFI_DL",
 [VR_16K] = "VR_16K",
};

const char * const scd_file_name_table_2nd[SCD_TYPE_MAX] = {
 [TEL_BT_NB_UL] = NULL,
 [TEL_BT_NB_DL] = NULL,
 [TEL_BT_WB_UL] = "2nd_TEL_BT_WB_UL",
 [TEL_BT_WB_DL] = NULL,
 [TEL_CP_8K_USB] = NULL,
 [TEL_CP_8K_USB_DL] = NULL,
 [TEL_CP_16K_USB] = NULL,
 [TEL_CP_16K_USB_DL] = NULL,
 [TEL_CP_32K_USB] = NULL,
 [TEL_CP_32K_USB_DL] = NULL,
 [TEL_CP_24K_USB] = NULL,
 [TEL_CP_24K_USB_DL] = NULL,
 [TEL_CP_48K_USB] = NULL,
 [TEL_CP_48K_USB_DL] = NULL,
 [TEL_CP_8K_WIFI] = NULL,
 [TEL_CP_8K_WIFI_DL] = NULL,
 [TEL_CP_16K_WIFI] = NULL,
 [TEL_CP_16K_WIFI_DL] = NULL,
 [TEL_CP_32K_WIFI] = NULL,
 [TEL_CP_32K_WIFI_DL] = NULL,
 [TEL_CP_24K_WIFI] = NULL,
 [TEL_CP_24K_WIFI_DL] = NULL,
 [TEL_CP_48K_WIFI] = NULL,
 [TEL_CP_48K_WIFI_DL] = NULL,
 [VR_16K] = "2nd_VR_16K",
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

    int HalECNRExtension::audio_extn_getSCDtype(uint32_t sample_rate, uint32_t vocoder_type, uint32_t ecnr_type, uint32_t conneection_type, uint32_t dir) {

        int scd_type = SCD_TYPE_INVALID;

        if (ecnr_type == ECNR_TYPE_VR) {
            if (sample_rate == 16000) {
                return VR_16K;
            } else {
                LOG(ERROR) << __func__ << " not supported sampling rate " << sample_rate<< " for ecnr_type " << ecnr_type ;
                return SCD_TYPE_INVALID;
            }
        } else if (ecnr_type == ECNR_TYPE_TEL) {
            if (conneection_type == CP_CONNECTION_WIFI) {
                switch (sample_rate) {
                    case 48000 :
                        scd_type = TEL_CP_48K_WIFI;
                        break;
                    case 32000 :
                        scd_type = TEL_CP_32K_WIFI;
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
                        LOG(ERROR) << __func__ << " not supported sampling rate " << sample_rate<< " for ecnr_type " << ecnr_type ;
                        break;
                    }
            } else {
                switch (sample_rate) {
                    case 48000 :
                        scd_type = TEL_CP_48K_USB;
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
                        LOG(ERROR) << __func__ << " not supported sampling rate " << sample_rate<< " for ecnr_type " << ecnr_type ;
                        break;
                }
            }
            if (scd_type < SCD_TYPE_MAX && scd_type > SCD_TYPE_INVALID )
                scd_type = scd_type + dir;
        } else {
            LOG(ERROR) << __func__ << " invalid ecnr type " << ecnr_type ;
            return SCD_TYPE_INVALID;
        }
        LOG(ERROR) << __func__ << " return scd_type " << scd_type ;
        return scd_type;
    }
    int HalECNRExtension::audio_extn_fillSCDbuffer(char * scd_file_name, uint32_t** scd_buffer, uint32_t* scd_buffer_size) {
        FILE *fp_scd = NULL;
        long filesize = 0;
        long numsize = 0;
        long read_num = 0;
        uint32_t* i_scd_buffer = NULL;
        *scd_buffer_size = 0;

        if (scd_file_name != NULL & strlen(scd_file_name)> 0) {
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
                LOG(ERROR) << __func__ << " failed to all data from scd file file size = " << numsize << " read bytes = " << read_num;
                free(i_scd_buffer);
                i_scd_buffer=NULL;
                return -1;
            }
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
    int HalECNRExtension::audio_extn_getSCDdata(tECNR_ProcessData* pECNR_ProcessData) {

        long filesize = 0;
        long numsize = 0;
        long read_num = 0;
        int32_t* i_scd_buffer = NULL;
        int ret = 0;
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
        if (scd_file_name_table[i_scd_type] != NULL) {
            if (strlen(hw_variant) == 0) {
                snprintf(scd_file_name, sizeof(scd_file_name), "%s%s",scd_file_name_table[i_scd_type],".scd");
            } else {
                snprintf(scd_file_name, sizeof(scd_file_name), "%s_%s%s",scd_file_name_table[i_scd_type],hw_variant,".scd");
            }
            if (!tuning_mode) {
                snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH ,scd_file_name);
                ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]),&(pECNR_ProcessData->scd_buffer_size[0]));
            } else {
                ret = tuning_mode;
            }
            if (ret) {
                snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH_BK , scd_file_name);
                ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[0]),&(pECNR_ProcessData->scd_buffer_size[0]));
            }
        }
        if (!ret && scd_file_name_table_2nd[i_scd_type] != NULL) {
            if (strlen(hw_variant) == 0) {
                snprintf(scd_file_name, sizeof(scd_file_name), "%s%s",scd_file_name_table_2nd[i_scd_type],".scd");
            } else {
                snprintf(scd_file_name, sizeof(scd_file_name), "%s_%s%s",scd_file_name_table_2nd[i_scd_type],hw_variant,".scd");
            }
            if (!tuning_mode) {
                snprintf(scd_file_path, sizeof(scd_file_path), "%s%s", SCD_PATH, scd_file_name);
                ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]),&(pECNR_ProcessData->scd_buffer_size[1]));
            } else {
                ret = tuning_mode;
            }
            if (ret) {
                snprintf(scd_file_path, sizeof(scd_file_path), "%s%s",SCD_PATH_BK, scd_file_name);
                ret = audio_extn_fillSCDbuffer(scd_file_path, &(pECNR_ProcessData->scd_buffer[1]),&(pECNR_ProcessData->scd_buffer_size[1]));
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
