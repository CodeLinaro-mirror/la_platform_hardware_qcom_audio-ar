ifeq ($(TARGET_USES_QMAA),true)
    ifneq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO),true)
    #QMAA Mode is enabled
    AUDIO_MODULES_DISABLED := true
    endif
endif

#Packages that should not be installed in QMAA are enabled here
ifneq ($(AUDIO_MODULES_DISABLED),true)

#AGM
ifeq (,$(filter gen4_gvm_gy gen5_gvm gen5_gvm_gy, $(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX)))
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
endif

#PAL Module
AUDIO_PAL := libar-pal
AUDIO_PAL += lib_bt_bundle
AUDIO_PAL += lib_bt_aptx
AUDIO_PAL += lib_bt_ble
AUDIO_PAL += catf
AUDIO_PAL += PalTest
AUDIO_PAL += libaudiochargerlistener
AUDIO_PAL += libhfp_pal
AUDIO_PAL += lib_default_plugin_controls
AUDIO_PAL += libautohal_pal
#PAL Service
AUDIO_PAL += libpalclient
AUDIO_PAL += libpalipcservice
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
AUDIO_PAL += libstream_acdb
#PAL Sessions Modules
AUDIO_PAL += libsession_agm
AUDIO_PAL += libsession_pcm
AUDIO_PAL += libsession_compress
AUDIO_PAL += libsession_voice
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
AUDIO_PAL += libdev_a2bmic
AUDIO_PAL += libdev_a2bspeaker
AUDIO_PAL += libdev_a2b2mic
AUDIO_PAL += libdev_a2b2speaker
AUDIO_PAL += libdev_ultrasound
AUDIO_PAL += libdev_usb
AUDIO_PAL += libdev_hfpuplink
AUDIO_PAL += libdev_hfpdownlink

AUDIO_ACDB := workspaceFileXml.qwsp
AUDIO_ACDB += acdb_cal.acdb
AUDIO_ACDB += acdb_cal.acdbdelta

ifeq (,$(filter gen4_gvm_gy gen5_gvm gen5_gvm_gy, $(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX)))
AUDIO_MODULES := $(AUDIO_AGM)
endif
AUDIO_MODULES += $(AUDIO_PAL)
AUDIO_MODULES += $(AUDIO_ACDB)

# AWE PAL PLUGIN and dependency packages
ifneq (,$(filter gen4_gvm_gy gen5_gvm gen5_gvm_gy, $(TARGET_BOARD_PLATFORM)$(TARGET_BOARD_SUFFIX)))
AUDIO_AWE += libsession_awe
AUDIO_AWE += libvui_intf
AUDIO_AWE += libcustomva_intf
AUDIO_AWE += libhotword_intf
AUDIO_AWE += libarmemlog
AUDIO_AWE += libtinyalsav2
AUDIO_MODULES += $(AUDIO_AWE)
endif

 # sound trigger aidl library
#AUDIO_MODULES += libsoundtriggerhal.qti

# enable Listen Sound Model aidl 1.0
#AUDIO_MODULES += \
    liblistensoundmodelaidl

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
