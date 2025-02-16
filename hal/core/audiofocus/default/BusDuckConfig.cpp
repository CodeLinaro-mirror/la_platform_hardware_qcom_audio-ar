/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "BusDuckConfig.h"

// Constructor
BusDuckConfigParser::BusDuckConfigParser() {}

// Destructor
BusDuckConfigParser::~BusDuckConfigParser() {}

// Start tag handler (static)
void BusDuckConfigParser::startTagHandler(void* userData,
                                    const char* tagName, const char** attr) {
    auto* parser = static_cast<BusDuckConfigParser*>(userData);

    if (strcmp(tagName, "priority") == 0) {
        for (int i = 0; attr[i]; i += 2) {
            if (strcmp(attr[i], "in_src") == 0)
                parser->temp_property.in_src = attr[i + 1];
            else if (strcmp(attr[i], "running_src") == 0)
                parser->temp_property.running_src = attr[i + 1];
            else if (strcmp(attr[i], "duck") == 0)
                parser->temp_property.duck = (strcmp(attr[i + 1], "true") == 0);
            else if (strcmp(attr[i], "Gain") == 0)
                parser->temp_property.gain = std::stoi(attr[i + 1]);
        }
    }
}

// End tag handler (static)
void BusDuckConfigParser::endTagHandler(void* userData, const char* tagName) {
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
        LOG(ERROR) << "Failed to open XML file: " << xmlFilePath;
        return false;
    }

    // Create the XML parser
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser) {
        LOG(ERROR) << "Failed to create XML parser.";
        fclose(file);
        return false;
    } else {
        LOG(ERROR) << "created XML parser.";
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
            LOG(ERROR) << __func__ << "Error reading the XML file.";
            success = false;
            break;
        } else {
            LOG(ERROR) << "read XML file successfully.";
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
AudioUsage getAudioUsageFromString(const std::string& src) {
    if (src == "MEDIA")
        return AudioUsage::MEDIA;
    if (src == "ASSISTANCE_NAVIGATION_GUIDANCE")
        return AudioUsage::ASSISTANCE_NAVIGATION_GUIDANCE;
    if (src == "EMERGENCY")
        return AudioUsage::EMERGENCY;
    return AudioUsage::UNKNOWN;
}

bool BusDuckConfigParser::populateAudioFocusConfig(std::unordered_map< AudioUsage,
                                std::unordered_map<AudioUsage, float> > &configMap) {
    for (const auto& property : all_priorities) {
        AudioUsage inSrcUsage = getAudioUsageFromString(property.in_src);
        AudioUsage runningSrcUsage = getAudioUsageFromString(property.running_src);

        if (property.duck) {
            configMap[inSrcUsage][runningSrcUsage] = property.gain;
        }
    }
    return true;
}
