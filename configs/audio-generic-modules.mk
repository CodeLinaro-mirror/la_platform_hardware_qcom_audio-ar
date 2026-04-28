
ifeq ($(TARGET_USES_QMAA),true)
    ifneq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO),true)
	        #QMAA Mode is enabled
        TARGET_IS_HEADLESS := true
    endif
endif
#Packages that should not be installed in QMAA are enabled here
ifneq ($(TARGET_IS_HEADLESS),true)
#MM_AUDIO product packages
MM_AUDIO := libcapiv2uvvendor
MM_AUDIO += libsoundtriggerhal.qti
MM_AUDIO += libAlacSwDec
MM_AUDIO += libApeSwDec
MM_AUDIO += libcapiv2svacnnvendor
MM_AUDIO += libcapiv2svarnnvendor
MM_AUDIO += libcapiv2udk7vendor
MM_AUDIO += libdsd2pcm
MM_AUDIO += libFlacSwDec
MM_AUDIO += libbatterylistener
MM_AUDIO += audioflacapp
MM_AUDIO += liblx-osal

#AOSP effects
MM_AUDIO += libbundleaidl
MM_AUDIO += libdownmixaidl
MM_AUDIO += libdynamicsprocessingaidl

MM_AUDIO += libloudnessenhanceraidl
MM_AUDIO += libreverbaidl
MM_AUDIO += libvisualizeraidl

#QTI effects
MM_AUDIO += libvolumelistener
MM_AUDIO += libqcompostprocbundle
MM_AUDIO += libqcomvisualizer
MM_AUDIO += libqcomvoiceprocessing
#KERNEL_TESTS
#KERNEL_TESTS := mm-audio-native-test

# Add VTS modules for userdebug and eng builds
#AUDIO_TESTS
ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
AUDIO_TESTS += VtsHalAudioCoreTargetTest
AUDIO_TESTS += VtsHalAudioEffectFactoryTargetTest
AUDIO_TESTS += VtsHalAudioEffectTargetTest
AUDIO_TESTS += VtsHalDownmixTargetTest
AUDIO_TESTS += VtsHalEnvironmentalReverbTargetTest
AUDIO_TESTS += VtsHalEqualizerTargetTest
AUDIO_TESTS += VtsHalHapticGeneratorTargetTest
AUDIO_TESTS += VtsHalLoudnessEnhancerTargetTest
AUDIO_TESTS += VtsHalPresetReverbTargetTest
AUDIO_TESTS += VtsHalVirtualizerTargetTest
AUDIO_TESTS += VtsHalVisualizerTargetTest
AUDIO_TESTS += VtsHalVolumeTargetTest
AUDIO_TESTS += VtsHalAECTargetTest
AUDIO_TESTS += VtsHalAGC1TargetTest
AUDIO_TESTS += VtsHalAGC2TargetTest
AUDIO_TESTS += VtsHalNSTargetTest
AUDIO_TESTS += VtsHalDynamicsProcessingTargetTest
AUDIO_TESTS += VtsHalBassBoostTargetTest
AUDIO_TESTS += VtsHalSpatializerTargetTest
endif

AUDIO_GENERIC_MODULES += $(MM_AUDIO)
AUDIO_GENERIC_MODULES += $(AUDIO_TESTS)
#AUDIO_GENERIC_MODULES += $(KERNEL_TESTS)

endif
