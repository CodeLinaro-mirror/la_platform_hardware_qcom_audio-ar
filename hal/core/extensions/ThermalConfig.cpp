/*
* Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include <include/extensions/ThermalConfig.h>
//Thermal Parser

// Constructor
ThermalParser::ThermalParser() {}

// Destructor
ThermalParser::~ThermalParser() {}

// Start tag handler (static)
extern "C" __attribute__((visibility("default"))) void ThermalParser::startTagHandler(void* userData, const char* tagName, const char** attr) {
    auto* parser = static_cast<ThermalParser*>(userData);

    if (strcmp(tagName, "Condition") == 0) {
        for (int i = 0; attr[i]; i += 2) {
            if (strcmp(attr[i], "tempLower") == 0) parser->temp_property.tempLower = std::stoi(attr[i + 1]);
            else if (strcmp(attr[i], "tempUpper") == 0) parser->temp_property.tempUpper = std::stoi(attr[i + 1]);
            else if (strcmp(attr[i], "gain") == 0) parser->temp_property.gain = std::stoi(attr[i + 1]);
        }
    }
}

// End tag handler (static)
extern "C" __attribute__((visibility("default"))) void ThermalParser::endTagHandler(void* userData, const char* tagName) {
    auto* parser = static_cast<ThermalParser*>(userData);

    if (strcmp(tagName, "Condition") == 0) {
        parser->all_conditions.push_back(parser->temp_property);
        parser->temp_property = ThermalCondition(); // Reset temp property
    }
}

// method to parse XML content
extern "C" __attribute__((visibility("default"))) bool ThermalParser::parseConfig(const std::string& xmlFilePath) {
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
        bool finalChunk = feof(file); // Check if this is the last chunk
        if (!XML_Parse(parser, buffer, bytesRead, finalChunk)) {
            LOG(ERROR) << __func__  << " Parse error at line "
                      << XML_GetCurrentLineNumber(parser) << ": "
                      << XML_ErrorString(XML_GetErrorCode(parser));
            success = false;
            break;
        }
        if (finalChunk) {
            break;
        }
    }
    // Cleanup
    XML_ParserFree(parser);
    fclose(file);

    return success;
}

extern "C" __attribute__((visibility("default"))) const std::vector<ThermalCondition>& ThermalParser::getConditions() const {
    return all_conditions;
}
