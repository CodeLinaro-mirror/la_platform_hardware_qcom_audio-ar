/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <include/extensions/BusDuckConfig.h>

// Constructor
BusDuckConfigParser::BusDuckConfigParser() {}

// Destructor
BusDuckConfigParser::~BusDuckConfigParser() {}

// Start tag handler (static)
void BusDuckConfigParser::startTagHandler(void* userData, const char* tagName, const char** attr) {

    if (!userData || !tagName || !attr) {
        LOG(ERROR) << " userData or tagName or attr is NULL";
        return;
    }

    auto* parser = static_cast<BusDuckConfigParser*>(userData);
    if (strcmp(tagName, "priority") == 0) {
        for (int i = 0; attr[i]; i += 2) {
            if (strcmp(attr[i], "in_src") == 0) parser->temp_property.in_src = attr[i + 1];
            else if (strcmp(attr[i], "running_src") == 0) parser->temp_property.running_src = attr[i + 1];
            else if (strcmp(attr[i], "duck") == 0) parser->temp_property.duck = (strcmp(attr[i + 1], "true") == 0);
            else if (strcmp(attr[i], "Gain") == 0) parser->temp_property.gain = std::stoi(attr[i + 1]);
            else if (strcmp(attr[i], "vol_override") == 0) parser->temp_property.vol_override = (strcmp(attr[i + 1], "true") == 0);
        }
    }
}

// End tag handler (static)
void BusDuckConfigParser::endTagHandler(void* userData, const char* tagName) {

    if (!userData || !tagName) {
        LOG(ERROR) << " userData or tagName or attr is NULL";
        return;
    }

    auto* parser = static_cast<BusDuckConfigParser*>(userData);
    if (strcmp(tagName, "priority") == 0) {
        parser->all_priorities.push_back(parser->temp_property);
        parser->temp_property = Property(); // Reset temp property
    }
}

// method to parse XML content
bool BusDuckConfigParser::parseConfig(const std::string& xmlFilePath) {
    FILE* file = fopen(xmlFilePath.c_str(), "r");
    if (!file) {
        LOG(ERROR) << " Failed to open XML file: " << xmlFilePath;
        return false;
    }

    // Create the XML parser
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser) {
        LOG(ERROR) << " Failed to create XML parser.";
        fclose(file);
        return false;
    }
    else{
        LOG(ERROR) << " created XML parser.";
    }

    // Set user data and handlers
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, startTagHandler, endTagHandler);

    // Buffer for reading the file
    char buffer[1024];
    bool success = true;

    // Read and parse the XML file in chunks
    while (true) {
        size_t bytesRead = fread(buffer, 1, 1024, file);
        if (ferror(file)) {
            LOG(ERROR) << __func__ << " Error reading the XML file.";
            success = false;
            break;
        }
        else{
            LOG(ERROR) << " read XML file successfully.";
        }

        // Parse the buffer
        bool final = feof(file); // Check if this is the last chunk
        if (!XML_Parse(parser, buffer, bytesRead, final)) {
            LOG(ERROR) << __func__  << " Parse error at line "
                      << XML_GetCurrentLineNumber(parser) << ": "
                      << XML_ErrorString(XML_GetErrorCode(parser));
            success = false;
            break;
        }

        if (final) {
            break;
        }
    }

    // Cleanup
    XML_ParserFree(parser);
    fclose(file);

    return success;
}

//
const std::vector<Property>& BusDuckConfigParser::getProperties() const {
    return all_priorities;
}

// map string to AudioUsage
// duck_configuration xml entities are all read as strings,
// differentiate based on exact string and convert to a FVM/RBVM type
// TODO: differentitate different types in duck_configuration xml itself
StreamType getAudioUsageFromString(const std::string& src) {
    if (src == "MEDIA") return AudioUsage::MEDIA;
    else if (src == "ASSISTANCE_NAVIGATION_GUIDANCE") return AudioUsage::ASSISTANCE_NAVIGATION_GUIDANCE;
    else if (src == "WELCOME_SEQUENCE") return UseCase::WELCOME_SEQUENCE;
    else if (src == "ROAD_ADAS") return UseCase::ROAD_ADAS;
    else if (src == "VEHICLE_WARNING") return UseCase::VEHICLE_WARNING;
    else if (src == "VEHICLE_SAFETY_WARNING") return UseCase::VEHICLE_WARNING;
    else if (src == "CYBER") return Type::CYBER;
    else if (src == "TCU") return Type::TCU;
    else if (src == "STATIC_POWER_LIMITATION") return Type::STATIC_POWER_LIMITATION;
    else if (src == "DELIVERY_MODE") return Type::DELIVERY_MODE;
    else if (src == "AM") return RadioAudioSource::AM;
    else if (src == "FM") return RadioAudioSource::FM;
    else if (src == "BUS00_MEDIA" || src == "BUS01_SYS_NOTIFICATION" ||
             src == "BUS02_NAV_GUIDANCE" || src == "BUS03_PHONE" ||
             src == "BUS0F_NAV_GUIDANCE2" || src == "THERMAL_MITIGATION" ||
             src == "RADIO_AAM_MUTE_ORDER" || src == "NIGHT_MODE" ||
             src == "DEVICE_TEMPERATURE_STATUS" ) return src;
    return AudioUsage::UNKNOWN;
}

bool BusDuckConfigParser::populateAudioFocusConfig(std::unordered_map<StreamType,
                                                        std::unordered_map<StreamType, ParseParams> > &configMap){
    for (const auto& property : all_priorities) {
        StreamType inSrcUsage = getAudioUsageFromString(property.in_src);
        StreamType runningSrcUsage = getAudioUsageFromString(property.running_src);
        if (property.duck)
            configMap[inSrcUsage][runningSrcUsage] = ParseParams{
                                    .gain = static_cast<float>(property.gain),
                                    .vol_override = property.vol_override,
                                };
    }
    return true;
}
