ifeq ($(TARGET_USES_QMAA),true)
    ifneq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO),true)
    #QMAA Mode is enabled
    TARGET_IS_HEADLESS := true
    endif
endif

#Packages that should not be installed in QMAA are enabled here
ifneq ($(TARGET_IS_HEADLESS),true)

#AGM
AUDIO_AGM := libagmclient
AUDIO_AGM += libagmipcservice
AUDIO_AGM += libagm
AUDIO_AGM += agmplay
AUDIO_AGM += agmcap
AUDIO_AGM += libagmmixer
AUDIO_AGM += agmcompressplay
AUDIO_AGM += libagm_mixer_plugin
AUDIO_AGM += libagm_pcm_plugin
AUDIO_AGM += libagm_compress_plugin
AUDIO_AGM += agmcompresscap
AUDIO_AGM += agmvoiceui
AUDIO_AGM += agmhostless

#PAL Module
AUDIO_PAL := libar-pal
AUDIO_PAL += lib_bt_bundle
AUDIO_PAL += lib_bt_aptx
AUDIO_PAL += lib_bt_ble
AUDIO_PAL += PalTest
AUDIO_PAL += libaudiochargerlistener
AUDIO_PAL += libhfp_pal
ifneq ($(call soong_config_get, qtiaudio, hy11), true)
ifneq ($(call soong_config_get, qtiaudio, hy22), true)
AUDIO_PAL += catf
endif
endif

#PAL Service
AUDIO_PAL += libpalclient
AUDIO_PAL += libpalipcservice
AUDIO_PAL += libpaleventnotifier
#PAL Stream Modules
AUDIO_PAL += libstream_compress
AUDIO_PAL += libstream_incall
AUDIO_PAL += libstream_commonproxy
AUDIO_PAL += libstream_contextproxy
AUDIO_PAL += libstream_sensorpcmdata
AUDIO_PAL += libstream_sensorrenderer
AUDIO_PAL += libstream_ultrasound
AUDIO_PAL += libstream_pcm
AUDIO_PAL += libstream_haptics
AUDIO_PAL += libstream_acd
AUDIO_PAL += libstream_nontunnel
AUDIO_PAL += libstream_soundtrigger
AUDIO_PAL += libstream_dummy
AUDIO_PAL += libstream_asr
#PAL Sessions Modules
AUDIO_PAL += libsession_agm
AUDIO_PAL += libsession_pcm
AUDIO_PAL += libsession_compress
AUDIO_PAL += libsession_voice
AUDIO_PAL += libsession_compress_config
AUDIO_PAL += libsession_pcm_config
AUDIO_PAL += libsession_voice_config
AUDIO_PAL += libsession_config_utils
#PAL Device Modules
AUDIO_PAL += libdev_bt
AUDIO_PAL += libdev_display
AUDIO_PAL += libdev_dummy
AUDIO_PAL += libdev_ec_ref
AUDIO_PAL += libdev_ext_ec
AUDIO_PAL += libdev_fm
AUDIO_PAL += libdev_handset
AUDIO_PAL += libdev_handset_mic
AUDIO_PAL += libdev_handset_va
AUDIO_PAL += libdev_haptics
AUDIO_PAL += libdev_headphone
AUDIO_PAL += libdev_headset_mic
AUDIO_PAL += libdev_headset_va
AUDIO_PAL += libdev_proxy
AUDIO_PAL += libdev_speaker
AUDIO_PAL += libdev_speaker_mic
AUDIO_PAL += libdev_ultrasound
AUDIO_PAL += libdev_usb

# C2 Audio
AUDIO_C2 := libqc2audio_base
AUDIO_C2 += libqc2audio_utils
AUDIO_C2 += libqc2audio_platform
AUDIO_C2 += libqc2audio_core
AUDIO_C2 += libqc2audio_basecodec
AUDIO_C2 += libqc2audio_hooks
AUDIO_C2 += libqc2audio_swaudiocodec
AUDIO_C2 += libqc2audio_hwaudiocodec
AUDIO_C2 += vendor.qti.media.c2audio@1.0-service
AUDIO_C2 += libEvrcSwCodec
AUDIO_C2 += libQcelp13SwCodec
AUDIO_C2 += c2audio.vendor.base-arm.policy
AUDIO_C2 += c2audio.vendor.ext-arm.policy
AUDIO_C2 += c2audio.vendor.base-arm64.policy
AUDIO_C2 += c2audio.vendor.ext-arm64.policy

AUDIO_TEST := mcs_test
AUDIO_TEST += ar_util_in_test_example


AUDIO_MODULES := ftm_test_config
AUDIO_MODULES += ftm_test_config_canoe-mtp-qmp-snd-card
AUDIO_MODULES += ftm_test_config_canoe-mtp-wsa884x-snd-card
AUDIO_MODULES += ftm_test_config_canoe-qrd-snd-card
AUDIO_MODULES += ftm_test_config_canoe-qrd-wsa884x-snd-card
AUDIO_MODULES += audioadsprpcd
AUDIO_MODULES += MTP_acdb_cal.acdb
AUDIO_MODULES += MTP_workspaceFileXml.qwsp
AUDIO_MODULES += MTP_canoe_wsa884x_acdb_cal.acdb
AUDIO_MODULES += MTP_canoe_wsa884x_workspaceFileXml.qwsp
AUDIO_MODULES += MTP_canoe_qmp_acdb_cal.acdb
AUDIO_MODULES += MTP_canoe_qmp_workspaceFileXml.qwsp
AUDIO_MODULES += CDP_acdb_cal.acdb
AUDIO_MODULES += CDP_workspaceFileXml.qwsp
AUDIO_MODULES += CDP_wsa884x_acdb_cal.acdb
AUDIO_MODULES += CDP_wsa884x_workspaceFileXml.qwsp
AUDIO_MODULES += QRD_acdb_cal.acdb
AUDIO_MODULES += QRD_workspaceFileXml.qwsp
AUDIO_MODULES += IDP_UPD_acdb_cal.acdb
AUDIO_MODULES += IDP_UPD_workspaceFileXml.qwsp
AUDIO_MODULES += QRD_canoe_wsa884x_acdb_cal.acdb
AUDIO_MODULES += QRD_canoe_wsa884x_workspaceFileXml.qwsp
AUDIO_MODULES += ATP_acdb_cal.acdb
AUDIO_MODULES += ATP_workspaceFileXml.qwsp
AUDIO_MODULES += fai__2.3.0_0.1__3.0.0_0.0__eai_1.10.pmd
AUDIO_MODULES += fai__2.3.0_0.1__3.0.0_0.0__eai_1.36_enpu2_comp.pmd
AUDIO_MODULES += fai__2.0.0_0.1__3.0.0_0.0__eai_1.36_enpu2.pmd
AUDIO_MODULES += fai__2.7.2_0.0__3.0.0_0.0__eai_1.36_enpu2.pmd
AUDIO_MODULES += fai__2.7.20_0.0__3.0.0_0.0__eai_1.36_enpu2.pmd
AUDIO_MODULES += fai__3.0.0_0.0__eai_1.36_enpu2.pmd
AUDIO_MODULES += ffv__7.0.1_0.1__eai_5.4_enpu_v6.pmd
AUDIO_MODULES += ffv__7.0.1_0.2__eai_5.4_enpu_v6.pmd
AUDIO_MODULES += hk01b_relu_eAI_5.3_eNPU_V6_adsp_i.pmd
AUDIO_MODULES += click.pcm
AUDIO_MODULES += double_click.pcm
AUDIO_MODULES += heavy_click.pcm
AUDIO_MODULES += pop.pcm
AUDIO_MODULES += reserved_1.pcm
AUDIO_MODULES += reserved_2.pcm
AUDIO_MODULES += reserved_3.pcm
AUDIO_MODULES += reserved_4.pcm
AUDIO_MODULES += reserved_5.pcm
AUDIO_MODULES += reserved_6.pcm
AUDIO_MODULES += reserved_7.pcm
AUDIO_MODULES += reserved_8.pcm
AUDIO_MODULES += texture_tick.pcm
AUDIO_MODULES += thud.pcm
AUDIO_MODULES += tick.pcm
AUDIO_MODULES += haptics_rx_tuning_0_cdp.bin
AUDIO_MODULES += haptics_rx_tuning_0_mtp.bin
AUDIO_MODULES += haptics_rx_tuning_0_qrd.bin
AUDIO_MODULES += haptics_vi_tuning_0_cdp.bin
AUDIO_MODULES += haptics_vi_tuning_0_mtp.bin
AUDIO_MODULES += haptics_vi_tuning_0_qrd.bin
AUDIO_MODULES += libfmpal
AUDIO_MODULES += event.eai
AUDIO_MODULES += music.eai
AUDIO_MODULES += speech.eai
AUDIO_MODULES += environment.eai
AUDIO_MODULES += conv_detection.eai
AUDIO_MODULES += libqtigefar
AUDIO_MODULES += audiodsd2pcmtest
AUDIO_MODULES += mm-audio-ftm
AUDIO_MODULES += libmcs
AUDIO_MODULES += libquasar
AUDIO_MODULES += sensors.dynamic_sensor_hal
AUDIO_MODULES += libvui_dmgr
AUDIO_MODULES += libvui_dmgr_client
AUDIO_MODULES += libaudiofeaturestats
AUDIO_MODULES += hotword_plugin
AUDIO_MODULES += customva_plugin
AUDIO_MODULES += sva_plugin
AUDIO_MODULES += libvui_utils
AUDIO_MODULES += libVoiceSdk
AUDIO_MODULES += libtensorflowlite_c
AUDIO_MODULES += libqasr
AUDIO_MODULES += libQnnHtp
AUDIO_MODULES += libQnnHtpV79Skel
AUDIO_MODULES += libQnnHtpV79Stub
AUDIO_MODULES += libQnnSystem

AUDIO_MODULES += $(AUDIO_AGM)
AUDIO_MODULES += $(AUDIO_PAL)
AUDIO_MODULES += $(AUDIO_C2)
AUDIO_MODULES += $(AUDIO_TEST)

 # sound trigger aidl library
AUDIO_MODULES += libsoundtriggerhal.qti

# enable Listen Sound Model aidl 1.0
AUDIO_MODULES += \
    liblistensoundmodelaidl \
    liblistensoundmodel2vendor \
    vendor.qti.hardware.ListenSoundModelAidl-V1-ndk.vendor

# AIDL Audio modules

AUDIO_MODULES += \
    audiohalservice.qti \
    libaudiocorehal.qti \
    libaudiocorehal.default \
    libaudioeffecthal.qti

# add modules for fuzzing
ifneq ($(filter audio,$(QC_HWASAN))$(filter hwaddress,$(SANITIZE_TARGET)),)
AUDIO_MODULES += fuzz-audio-hal
endif

endif
