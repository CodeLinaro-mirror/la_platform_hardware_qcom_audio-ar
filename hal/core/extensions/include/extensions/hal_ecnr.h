/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <string>

#ifdef ECNR_HAL_TUNE
#include "extensions/hal_ecnr_tune.h"
#endif


#define ECNR_MIC_CH              2
#define ECNR_EC_REF_CH           4
#define ECNR_MIC_EC_CH           6
#define ECNR_IN_DL_CH    1
#define ECNR_OUT_DL_CH    1
#define ECNR_OUT_UL_CH    1
#define DIR_UL    0
#define DIR_DL    1
#define ECNR_TYPE_VR 0
#define ECNR_TYPE_TEL 1
#define ECNR_TYPE_LEGACY_SIRI 2
#define ECNR_TYPE_FACETIME 3
#define CP_CONNECTION_USB 0
#define CP_CONNECTION_WIFI 1
#define CP_SAMPLERATE "cp_sample"
#define CP_TYPE "cp_type"
#define CP_VOCODER_SAMPLERATE "vocoder_sample"
#define CP_TRANSPORT "cp_connection"
#define CRC_LEN 4
#define CRC_MAX (CRC_LEN + 5) // This +5 is added to append 8 characters with '\0'
#define SSE 1
#define DNN 0


#ifdef __LP64__
#define LIBS "/vendor/lib64/"
#else
#define LIBS "/vendor/lib/"
#endif
#define AUDIO_HAL_DUMP_PATH "/data/vendor/audio/"
#define HFP_ECNR_LIB_PATH "libhfpECNR_pal.so"
#define ECNR_LIB_PATH "libsse.so"
#define ECNR_FEATURE_PROP "vendor.audio.feature.ecnr_hal.enable"
#define ECNR_DUMP_FEATURE_PROP "vendor.audio.feature.ecnr.dump"
#define VARIANT_PROP "vendor.audio.variant"
#define TUNE_MODE_PROP "vendor.audio.ecnr.tune_mode"
#define SCD_PATH "/data/audio/"
#define SCD_PATH_BK "/vendor/etc/ecnr_scd/"
#define MAX_PARAM_FILES 2

typedef enum {
    SCD_TYPE_INVALID = -1,
    TEL_BT_NB_UL = 0,
    TEL_BT_NB_DL,
    TEL_BT_WB_UL,
    TEL_BT_WB_DL,
    TEL_CP_8K_USB, //4
    TEL_CP_8K_USB_DL,
    TEL_CP_16K_USB,
    TEL_CP_16K_USB_DL,
    TEL_CP_32K_USB,
    TEL_CP_32K_USB_DL,
    TEL_CP_24K_USB,
    TEL_CP_24K_USB_DL,
    TEL_CP_48K_USB,
    TEL_CP_48K_USB_DL,
    TEL_CP_8K_WIFI, //14
    TEL_CP_8K_WIFI_DL,
    TEL_CP_16K_WIFI,
    TEL_CP_16K_WIFI_DL,
    TEL_CP_32K_WIFI,
    TEL_CP_32K_WIFI_DL,
    TEL_CP_24K_WIFI,
    TEL_CP_24K_WIFI_DL,
    TEL_CP_48K_WIFI,
    TEL_CP_48K_WIFI_DL,
    VR_16K,        //22
    LEGACY_SIRI_USB_UL,
    LEGACY_SIRI_WIFI_UL,
    FACETIME_USB,
    FACETIME_USB_DL,
    FACETIME_WIFI,
    FACETIME_WIFI_DL,
    SCD_TYPE_MAX,
} scd_type_t;

typedef enum {
   USECASE_NONE = 0,
   CALL,
   FACETIME,
   SIRI,
   ESIRI_IN,
   ESIRI_OUT,
} cp_type;

typedef enum {
   INVALID_PATH = -1,
   CALIB_PATH,
   TUNING_PATH,
   DEFAULT_PATH,
   MAX_SCD_PATH_INDEX,
} scd_file_path_index_t;

typedef struct ecnrMainStruct tECNR_Main;

typedef struct tECNR_AudioIO
{
   unsigned int AudioBufferSampleCnt;
   unsigned int MicInBufferCnt;
   unsigned int RefInBufferCnt;
   unsigned int RcvInBufferCnt;
   unsigned int MicProcBufferCnt;
   unsigned int RcvProcBufferCnt;

   short int **MicInBuffers;
   short int **RefInBuffers;
   short int **RcvInBuffers;
   short int **MicProcBuffers;
   short int **RcvProcBuffers;
} tECNR_AudioIO;

typedef struct tECNR_TuneIO
{
   unsigned int    InBufferSize;
   unsigned int    InBufferUsedSize;
   void*           InBuffer;
   unsigned int    OutBufferSize;
   unsigned int    OutBufferUsedSize;
   void*           OutBuffer;
} tECNR_TuneIO;


typedef struct tECNR_ProcessData
{
    tECNR_Main*       pMain;
    tECNR_AudioIO     audioIO;
    uint32_t *scd_buffer[MAX_PARAM_FILES];
    uint32_t  scd_buffer_size[MAX_PARAM_FILES];
    tECNR_TuneIO sECNRTuneIO;
    int scd_type;
    int ecnr_type;
}tECNR_ProcessData;



typedef int (*ecnrCreate_t)(    tECNR_Main      **Main, void* unsued );
typedef int (*ecnrInitialize_t)( tECNR_Main* pMain );

typedef int (*ecnrDestroy_t)( tECNR_Main  **pMain );

typedef int (*ecnrReset_t)( tECNR_Main* pMain, const unsigned int ResetMode );

typedef int (*ecnrProcess_t)( tECNR_Main*    pMain,
                tECNR_AudioIO  AudioIO,
                tECNR_TuneIO*  pTuneIO );

#ifdef ECNR_HAL_HIRES
typedef int (*ecnrProcessHiRes_t)(  tECNR_Main*        pMain,
                      tECNR_AudioIOHiRes AudioIO,
                      tECNR_TuneIO*      pTuneIO );
#endif
typedef int (*ecnrSetData_t)(      tECNR_Main*     pMain,
               const unsigned int  DataID,
               const int           iChannel,
                     unsigned int* puSize,
               const void*         pData);

typedef int (*ecnrGetData_t)(       tECNR_Main* const  pMain,
                const unsigned int     DataID,
                const int              iChannel,
                unsigned int*    puSize,
                void*            pData);

typedef int (*ecnrSetEffect_t)(       tECNR_Main*      pMain,
                const unsigned int   EffectID,
                const int            Channel,
                const unsigned int   InputCnt,
                int*           pData);
typedef int (*ecnrGetEffect_t)(       tECNR_Main* const  pMain,
                const unsigned int     EffectID,
                const int              Channel,
                const unsigned int     OutputCnt,
                int*             pData);

typedef int (*ecnrGetErrorMessage_t)( const tECNR_Main* pMain,
                char*     pErrorMessage,
                const unsigned int MaxMessageLength );

typedef void (*ecnrGetVersion_t)(       unsigned int* pArrayLen,
                const int**         pVersionArray,
                const char**        pVersionString,
                const char**        pVersionComment );


typedef int (*ecnrSetConfigData_t)(tECNR_Main* pMain, const void* const pCfgData, const unsigned int uCfgDataSize);

typedef int (*extn_getSCDdata_t)(int32_t** scd_buffer, uint32_t* scd_buffer_size, int scd_type);
typedef int (*extn_setupECNR_t)(tECNR_ProcessData* pECNR_ProcessData);
typedef void (*extn_ecnrGetVersion_t)();

typedef int (*extn_cvtformat16_ECNRin_t)(void* int_buffer, void* deint_buffer,int frameSz);
typedef int (*extn_cvtformat16_ECNRout_t)(void* int_buffer, void* deint_buffer,int frameSz, int numchannel);

#ifdef ECNR_HAL_TUNE
typedef int (*extn_setupECNR_TuneIF_t)(tECNR_TuneIFData* pECNR_TuneIFData, int portid);
typedef int (*extn_close_TuneIF_t)(tECNR_TuneIFData* pECNR_TuneIFData);
typedef int (*extn_get_TuneIO_buffer_t)(tECNR_TuneIFData* pECNR_TuneIFData, tECNR_TuneIO* pSseTuneIO);
typedef int (*extn_feedback_TuneIO_buffer_t)(tECNR_TuneIFData* pECNR_TuneIFData, tECNR_TuneIO* pSseTuneIO);
#endif

typedef int (*extn_setupIOBuffer_t)(tECNR_ProcessData* pECNR_ProcessData, int dir, int in_ch, int out_ch, int framesize, void* in_buffer, void* out_buffer);
typedef int (*extn_resetIOBuffer_t)(tECNR_ProcessData* pECNR_ProcessData);
class HalECNRExtension {
    public:
    HalECNRExtension();
    ~HalECNRExtension();

    bool audio_extn_getEnablement();
    int audio_extn_ecnrCreate(        tECNR_Main        **Main, void *unused );
    int audio_extn_ecnrInitialize( tECNR_Main* pMain );
    int audio_extn_ecnrDestroy( tECNR_Main  **pMain );
    int audio_extn_ecnrReset( tECNR_Main* pMain, const unsigned int ResetMode );
    int audio_extn_ecnrProcess( tECNR_Main*      pMain,
                        tECNR_AudioIO  AudioIO,
                        tECNR_TuneIO*  pTuneIO );
    void carplay_set_parameters(struct str_parms *params);
#ifdef ECNR_HAL_HIRES
    int audio_extn_ecnrProcessHiRes(  tECNR_Main*        pMain,
                              tECNR_AudioIOHiRes AudioIO,
                              tECNR_TuneIO*      pTuneIO );
#endif
    int audio_extn_ecnrSetData(        tECNR_Main*     pMain,
                       const unsigned int  DataID,
                       const int           iChannel,
                             unsigned int* puSize,
                       const void*           pData);

    int audio_extn_ecnrGetData(         tECNR_Main* const    pMain,
                        const unsigned int       DataID,
                        const int               iChannel,
                              unsigned int*    puSize,
                              void*            pData);

    int audio_extn_ecnrSetEffect(       tECNR_Main*        pMain,
                          const unsigned int   EffectID,
                          const int            Channel,
                          const unsigned int   InputCnt,
                                int*           pData);

    int audio_extn_ecnrGetEffect(       tECNR_Main* const  pMain,
                          const unsigned int     EffectID,
                          const int              Channel,
                          const unsigned int     OutputCnt,
                                int*             pData);

    int audio_extn_ecnrGetErrorMessage( const tECNR_Main* pMain,
                                      char*     pErrorMessage,
                                const unsigned int MaxMessageLength );

    void audio_extn_ecnrGetVersion();
    int audio_extn_ecnrSetConfigData(tECNR_Main*   pMain, const void* const pCfgData, const unsigned int uCfgDataSize);
    int audio_extn_getSCDtype(uint32_t sample_Rate, int vocoder_rate, uint32_t ecnr_type, int connection_type, uint32_t dir);
    int audio_extn_fillSCDbuffer(char * scd_file_name, uint32_t** scd_buffer, uint32_t* scd_buffer_size, uint32_t dir, uint16_t data);
    int find_crc(FILE *fd, char * scd_file_name, uint32_t dir, uint16_t data);
    int audio_extn_getSCDdata(tECNR_ProcessData* pECNR_ProcessData, uint32_t dir, int *current_file_path);
    int audio_extn_setupECNR( tECNR_ProcessData* pECNR_ProcessData);
    int audio_extn_setupIOBuffer(tECNR_ProcessData* pECNR_ProcessData, int dir, int in_ch, int out_ch, int framesize, void* in_buffer, void* out_buffer);
    int audio_extn_resetIOBuffer(tECNR_ProcessData* pECNR_ProcessData);
    int audio_extn_cvtformat16_lnterleave_to_deinterleave(void* int_buffer, void* deint_buffer,int frameSz, int numchannel);
    int audio_extn_cvtformat16_delnterleave_to_interleave(void* deint_buffer, void* int_buffer, int frameSz, int numchannel);
    int get_vocoder_rate() const;
    int get_conn_type() const;
    int get_cp_type() const;
    void set_vocoder_rate(int data);
    void set_conn_type(int data);
    void set_cp_type(int data);
    int carplay_param_converter(char * data);
#ifdef ECNR_HAL_TUNE
    int audio_extn_setupECNR_TuneIF(tECNR_TuneIFData* pECNR_TuneIFData, int portid);
    int audio_extn_close_TuneIF(tECNR_TuneIFData* pECNR_TuneIFData);
    int audio_extn_get_TuneIO_buffer(tECNR_TuneIFData* pECNR_TuneIFData, tECNR_TuneIO* pSseTuneIO);
    int audio_extn_feedback_TuneIO_buffer(tECNR_TuneIFData* pECNR_TuneIFData, tECNR_TuneIO* pSseTuneIO);
#endif

  protected:
    void* mHandle = nullptr;
    bool mEnabled;

  private:
    const std::string mLibraryName{ECNR_LIB_PATH};
    const std::string mHalECNRProperty{ECNR_FEATURE_PROP};
    char hw_variant[PROPERTY_VALUE_MAX];
    ecnrCreate_t ecnrCreate;
    ecnrInitialize_t ecnrInitialize;
    ecnrDestroy_t ecnrDestroy;
    ecnrReset_t ecnrReset;
    ecnrProcess_t ecnrProcess;
#ifdef ECNR_HAL_HIRES
    ecnrProcessHiRes_t ecnrProcessHiRes;
#endif
    ecnrSetData_t ecnrSetData;
    ecnrGetData_t ecnrGetData;
    ecnrSetEffect_t ecnrSetEffect;
    ecnrGetEffect_t ecnrGetEffect;
    ecnrGetErrorMessage_t ecnrGetErrorMessage;
    ecnrGetVersion_t ecnrGetVersion;
    ecnrSetConfigData_t ecnrSetConfigData;
    int carplay_sample_rate;
    int carplay_type;
    int vocoder_sample_rate;
    int connect_type;
};
