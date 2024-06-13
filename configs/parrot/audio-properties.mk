# spf hdr record either true or false
AUDIO_HAL_PROP += \
vendor.audio.hdr.spf.record.enable=false

# spf hdr record either true or false
AUDIO_HAL_PROP += \
vendor.audio.hdr.record.enable=false

#compress offload
AUDIO_HAL_PROP += \
vendor.audio.offload.buffer.size.kb=32

# compress capture feature related
AUDIO_HAL_PROP += \
vendor.audio.compress_capture.enabled=true \
vendor.audio.compress_capture.aac=true
# compress capture end


PRODUCT_VENDOR_PROPERTIES += \
    $(AUDIO_HAL_PROP)