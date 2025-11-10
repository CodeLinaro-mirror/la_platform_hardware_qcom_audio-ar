/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AHAL_PlatformUtils_QTI"

#include <android-base/logging.h>
#include <qti-audio-core/Platform.h>
#include <qti-audio-core/PlatformUtils.h>
#include <qti-audio-core/Utils.h>
#include <qti-audio/PlatformConverter.h>
#include <system/audio.h>

#include <map>
#include <set>
#include <vector>

using ::aidl::android::hardware::audio::core::VendorParameter;
using ::aidl::android::media::audio::common::AudioChannelLayout;
using ::aidl::android::media::audio::common::AudioFormatDescription;
using ::aidl::android::media::audio::common::AudioFormatType;
using ::aidl::android::media::audio::common::AudioPlaybackRate;
using ::aidl::android::media::audio::common::AudioProfile;
using ::aidl::android::media::audio::common::PcmType;
using ::aidl::qti::audio::core::VString;

namespace qti::audio::core {

using AudioChannelCountToMaskMap = std::map<unsigned int, AudioChannelLayout>;
AudioChannelLayout getInvalidChannelLayout() {
    static const AudioChannelLayout invalidChannelLayout =
            AudioChannelLayout::make<AudioChannelLayout::Tag::invalid>(0);
    return invalidChannelLayout;
}

static AudioChannelCountToMaskMap createChannelMaskMap(
        const std::set<AudioChannelLayout>& channelMasks) {
    AudioChannelCountToMaskMap channelMaskToCountMap;
    for (const auto& channelMask : channelMasks) {
        channelMaskToCountMap.emplace(getChannelCount(channelMask), channelMask);
    }
    return channelMaskToCountMap;
}
#define MAKE_LAYOUT_MASK(n) \
    AudioChannelLayout::make<AudioChannelLayout::Tag::layoutMask>(AudioChannelLayout::LAYOUT_##n)

const AudioChannelCountToMaskMap& getSupportedChannelOutLayoutMap() {
    static const std::set<AudioChannelLayout> supportedOutChannelLayouts = {
            MAKE_LAYOUT_MASK(MONO),     MAKE_LAYOUT_MASK(STEREO),  MAKE_LAYOUT_MASK(2POINT1),
            MAKE_LAYOUT_MASK(QUAD),     MAKE_LAYOUT_MASK(PENTA),   MAKE_LAYOUT_MASK(5POINT1),
            MAKE_LAYOUT_MASK(6POINT1),  MAKE_LAYOUT_MASK(7POINT1), MAKE_LAYOUT_MASK(7POINT1POINT4),
            MAKE_LAYOUT_MASK(22POINT2),
    };
    static const AudioChannelCountToMaskMap outLayouts =
            createChannelMaskMap(supportedOutChannelLayouts);
    return outLayouts;
}

const AudioChannelCountToMaskMap& getSupportedChannelInLayoutMap() {
    static const std::set<AudioChannelLayout> supportedInChannelLayouts = {
            MAKE_LAYOUT_MASK(MONO), MAKE_LAYOUT_MASK(STEREO),
    };
    static const AudioChannelCountToMaskMap inLayouts =
            createChannelMaskMap(supportedInChannelLayouts);
    return inLayouts;
}
#undef MAKE_LAYOUT_MASK
#define MAKE_INDEX_MASK(n) \
    AudioChannelLayout::make<AudioChannelLayout::Tag::indexMask>(AudioChannelLayout::INDEX_MASK_##n)

const AudioChannelCountToMaskMap& getSupportedChannelIndexLayoutMap() {
    static const std::set<AudioChannelLayout> supportedIndexChannelLayouts = {
            MAKE_INDEX_MASK(1),  MAKE_INDEX_MASK(2),  MAKE_INDEX_MASK(3),  MAKE_INDEX_MASK(4),
            MAKE_INDEX_MASK(5),  MAKE_INDEX_MASK(6),  MAKE_INDEX_MASK(7),  MAKE_INDEX_MASK(8),
            MAKE_INDEX_MASK(9),  MAKE_INDEX_MASK(10), MAKE_INDEX_MASK(11), MAKE_INDEX_MASK(12),
            MAKE_INDEX_MASK(13), MAKE_INDEX_MASK(14), MAKE_INDEX_MASK(15), MAKE_INDEX_MASK(16),
            MAKE_INDEX_MASK(17), MAKE_INDEX_MASK(18), MAKE_INDEX_MASK(19), MAKE_INDEX_MASK(20),
            MAKE_INDEX_MASK(21), MAKE_INDEX_MASK(22), MAKE_INDEX_MASK(23), MAKE_INDEX_MASK(24),
    };
    static const AudioChannelCountToMaskMap indexLayouts =
            createChannelMaskMap(supportedIndexChannelLayouts);
    return indexLayouts;
}

#undef MAKE_INDEX_MASK

AudioChannelLayout getChannelLayoutMaskFromChannelCount(unsigned int channelCount, int isInput) {
    return findValueOrDefault(
            isInput ? getSupportedChannelInLayoutMap() : getSupportedChannelOutLayoutMap(),
            channelCount, getInvalidChannelLayout());
}

AudioChannelLayout getChannelIndexMaskFromChannelCount(unsigned int channelCount) {
    return findValueOrDefault(getSupportedChannelIndexLayoutMap(), channelCount,
                              getInvalidChannelLayout());
}
std::vector<AudioChannelLayout> getChannelMasksFromProfile(
        pal_param_device_capability_t* capability) {
    const bool isInput = !capability->is_playback;
    std::vector<AudioChannelLayout> channels;
    for (size_t i = 0; i < AUDIO_PORT_MAX_CHANNEL_MASKS && capability->config->mask[i] != 0; ++i) {
        auto channelCount =
                isInput ? audio_channel_count_from_in_mask(capability->config->mask[i])
                        : audio_channel_count_from_out_mask(capability->config->mask[i]);
        auto layoutMask = getChannelLayoutMaskFromChannelCount(channelCount, isInput);
        if (layoutMask.getTag() == AudioChannelLayout::Tag::layoutMask) {
            channels.push_back(layoutMask);
        }
        auto indexMask = getChannelIndexMaskFromChannelCount(channelCount);
        if (indexMask.getTag() == AudioChannelLayout::Tag::indexMask) {
            channels.push_back(indexMask);
        }
    }
    return channels;
}
std::vector<int> getSampleRatesFromProfile(pal_param_device_capability_t* capability) {
    std::vector<int> sampleRates;
    for (int i = 0; capability->config->sample_rate[i] != 0; i++) {
        sampleRates.push_back(capability->config->sample_rate[i]);
    }
    return sampleRates;
}

static AudioFormatDescription make_AudioFormatDescription(AudioFormatType type) {
    AudioFormatDescription result;
    result.type = type;
    return result;
}
static AudioFormatDescription make_AudioFormatDescription(PcmType pcm) {
    auto result = make_AudioFormatDescription(AudioFormatType::PCM);
    result.pcm = pcm;
    return result;
}

static AudioFormatDescription getLegacyToAidlFormat(int palFormat) {
    switch (palFormat) {
        case PCM_16_BIT:
            return make_AudioFormatDescription(PcmType::INT_16_BIT);
        case PCM_32_BIT:
            return make_AudioFormatDescription(PcmType::INT_32_BIT);
        case PCM_24_BIT_PACKED:
            return make_AudioFormatDescription(PcmType::INT_24_BIT);
        default:
            return AudioFormatDescription();
    }
}

std::vector<AudioProfile> getSupportedAudioProfiles(pal_param_device_capability_t* capability,
                                                    std::string devName) {
    std::vector<AudioProfile> supportedProfiles;
    std::vector<AudioChannelLayout> channels = getChannelMasksFromProfile(capability);
    std::vector<int> sampleRates = getSampleRatesFromProfile(capability);

    std::string name = devName + "_" + (capability->is_playback ? "out" : "in");
    for (size_t i = 0; i < MAX_SUPPORTED_FORMATS && capability->config->format[i] != 0; ++i) {
        auto audioFormatDescription = getLegacyToAidlFormat(capability->config->format[i]);
        if (audioFormatDescription.type == AudioFormatType::DEFAULT) {
            LOG(WARNING) << __func__ << ": unknown pcm type= " << capability->config->format[i];
            continue;
        }

        AudioProfile audioProfile = {.name = name,
                                     .format = audioFormatDescription,
                                     .channelMasks = channels,
                                     .sampleRates = sampleRates};

        LOG(VERBOSE) << __func__ << " found profile " << audioProfile.toString();
        supportedProfiles.push_back(std::move(audioProfile));
    }
    return supportedProfiles;
}

bool isValidPlaybackRate(
        const ::aidl::android::media::audio::common::AudioPlaybackRate& playbackRate) {
    // For fallback mode MUTE, out of range values should not be rejected.
    if (playbackRate.fallbackMode != AudioPlaybackRate::TimestretchFallbackMode::MUTE) {
        if (playbackRate.speed < 0.1f || playbackRate.speed > 2.0f) {
            LOG(ERROR) << __func__ << ": unsupported speed " << playbackRate.toString();
            return false;
        }

        if (playbackRate.pitch != 1.0f) {
            LOG(ERROR) << __func__ << ": unsupported pitch " << playbackRate.toString();
            return false;
        }
    }

    auto isValidStretchMode = [=](const auto& stretchMode) {
        return (stretchMode == AudioPlaybackRate::TimestretchMode::DEFAULT ||
                stretchMode == AudioPlaybackRate::TimestretchMode::VOICE);
    };

    if (!isValidStretchMode(playbackRate.timestretchMode)) {
        LOG(ERROR) << __func__ << ": unsupported timstrecth mode " << playbackRate.toString();
        return false;
    }

    auto isValidFallbackMode = [=](const auto& fallMode) {
        return (fallMode == AudioPlaybackRate::TimestretchFallbackMode::MUTE ||
                fallMode == AudioPlaybackRate::TimestretchFallbackMode::FAIL);
    };

    if (!isValidFallbackMode(playbackRate.fallbackMode)) {
        LOG(ERROR) << __func__ << ": unsupported fallback mode " << playbackRate.toString();
        return false;
    }

    return true;
}

void setPalDeviceCustomKey(pal_device& palDevice, const std::string& customKey) noexcept {
    strlcpy(palDevice.custom_config.custom_key, customKey.c_str(), PAL_MAX_CUSTOM_KEY_SIZE);
}

std::vector<uint8_t> makePalVolumes(std::vector<float> const& volumes) noexcept {
    if (volumes.empty()) {
        return {};
    }

    auto channels = volumes.size();
    auto palChannelInfo = PlatformConverter::getPalChannelInfoForChannelCount(channels);

    const auto dataLength = sizeof(pal_volume_data) + sizeof(pal_channel_vol_kv) * channels;
    auto data = std::vector<uint8_t>(dataLength);
    auto palVolumeData = reinterpret_cast<pal_volume_data*>(data.data());
    palVolumeData->no_of_volpair = channels;

    for (unsigned long channel = 0; channel < channels; channel++) {
        palVolumeData->volume_pair[channel].channel_mask = palChannelInfo->ch_map[channel];
        palVolumeData->volume_pair[channel].vol = volumes[channel];
    }
    return data;
}

bool makePalDeviceAddress(const AudioDeviceAddress& address, pal_device* const palDevice) {
    switch (address.getTag()) {
        case AudioDeviceAddress::Tag::mac: {
            const std::vector<uint8_t>& mac = address.get<AudioDeviceAddress::mac>();
            if (mac.size() != 6) return false;
            std::memcpy(palDevice->addressV1.mac, mac.data(), 6);

        } break;
        case AudioDeviceAddress::Tag::ipv4: {
            const std::vector<uint8_t>& ipv4 = address.get<AudioDeviceAddress::ipv4>();
            if (ipv4.size() != 4) return false;
            std::memcpy(palDevice->addressV1.ipv4, ipv4.data(), 4);
        } break;
        case AudioDeviceAddress::Tag::ipv6: {
            const std::vector<int32_t>& ipv6 = address.get<AudioDeviceAddress::ipv6>();
            if (ipv6.size() != 8) return false;
            std::memcpy(palDevice->addressV1.ipv6, ipv6.data(), 8);
        } break;
        case AudioDeviceAddress::Tag::alsa: {
            const std::vector<int32_t>& alsa = address.get<AudioDeviceAddress::alsa>();
            if (!isValidAlsaAddr(alsa)) return false;
            std::memcpy(palDevice->addressV1.alsa, alsa.data(), 2);
        } break;
        case AudioDeviceAddress::Tag::id: {
            const std::string& id = address.get<AudioDeviceAddress::id>();
            strlcpy(palDevice->addressV1.id, id.c_str(), sizeof(palDevice->addressV1.id));
        } break;
    }
    return true;
}

bool compare(const pal_device& device1, const pal_device& device2) {
    if (device1.id != device2.id) {
        return false;
    }

    PalAddressTag tag1 = getAddressTag(device1.id);
    PalAddressTag tag2 = getAddressTag(device2.id);

    if (tag1 != tag2) {
        return false;
    }

    switch (tag1) {
        case PalAddressTag::ID:
            return std::memcmp(device1.addressV1.id, device2.addressV1.id,
                               sizeof(device1.addressV1.id)) == 0;
        case PalAddressTag::MAC:
            return std::memcmp(device1.addressV1.mac, device2.addressV1.mac,
                               sizeof(device1.addressV1.mac)) == 0;
        case PalAddressTag::ALSA:
            return std::memcmp(device1.addressV1.alsa, device2.addressV1.alsa,
                               sizeof(device1.addressV1.alsa)) == 0;
        case PalAddressTag::IPv4:
            return std::memcmp(device1.addressV1.ipv4, device2.addressV1.ipv4,
                               sizeof(device1.addressV1.ipv4)) == 0;
        case PalAddressTag::IPv6:
            return std::memcmp(device1.addressV1.ipv6, device2.addressV1.ipv6,
                               sizeof(device1.addressV1.ipv6)) == 0;
        default:
            return false;
    }
}


static std::string streamDirectionToString(pal_stream_direction_t direction) {
    switch (direction) {
        case PAL_AUDIO_OUTPUT:
            return "OUTPUT";
        case PAL_AUDIO_INPUT:
            return "INPUT";
        case PAL_AUDIO_INPUT_OUTPUT:
            return "INPUT_OUTPUT";
        default:
            return "UNKNOWN";
    }
}

static std::string audioFormatToString(pal_audio_fmt_t fmt) {
    for (const auto& pair : PalAudioFormatMap) {
        if (pair.second == fmt) {
            return pair.first;
        }
    }
    return "UNKNOWN";
}

static std::string channelInfoToString(const struct pal_channel_info& ch_info) {
    std::ostringstream oss;
    oss << "channels=" << ch_info.channels;

    // oss << ", ch_map=[";
    // for (uint16_t i = 0; i < ch_info.channels && i < PAL_MAX_CHANNELS_SUPPORTED; ++i) {
    //     if (i > 0) oss << ",";
    //     oss << static_cast<int>(ch_info.ch_map[i]);
    // }
    // oss << "]";
    return oss.str();
}

static std::string mediaConfigToString(const struct pal_media_config& config) {
    std::ostringstream oss;
    oss << "format=" << audioFormatToString(config.aud_fmt_id)
        << ", sample_rate=" << config.sample_rate << ", bit_width=" << config.bit_width << ", "
        << channelInfoToString(config.ch_info);
    return oss.str();
}

std::string toString(const pal_stream_attributes& attributes) {
    std::ostringstream oss;

    std::string streamType = "UNKNOWN";
    if (streamNameLUT.find(attributes.type) != streamNameLUT.end()) {
        streamType = streamNameLUT.at(attributes.type);
    }

    oss << "attributes {" << streamDirectionToString(attributes.direction) << "," << attributes.type
        << ":" << streamType;
    //  << ", flags: " << streamFlagsToString(attributes.flags);

    if (attributes.direction & PAL_AUDIO_OUTPUT) {
        oss << ", out_config {" << mediaConfigToString(attributes.out_media_config) << "}";
    }

    if (attributes.direction & PAL_AUDIO_INPUT) {
        oss << ", in_config {" << mediaConfigToString(attributes.in_media_config) << "}";
    }

    oss << "}";
    return oss.str();
}

ndk::ScopedAStatus toBinderStatus(PlaybackRateStatus ret) {
    switch (ret) {
        case PlaybackRateStatus::SUCCESS:
            return ndk::ScopedAStatus::ok();
        case PlaybackRateStatus::UNSUPPORTED:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
}

}  // namespace qti::audio::core