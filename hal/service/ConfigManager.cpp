/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AHAL_Service_QTI"

#include "ConfigManager.h"
#include <log/log.h>
#include <system/audio_config.h>
#include <tinyxml2.h>
#include <unistd.h>

using namespace tinyxml2;

/** @return xml dump of the provided element.
 * By not providing a printer, it is implicitly created in the caller context.
 * In such case the return pointer has the same lifetime as the expression containing dump().
 */
const char *dump(const XMLElement &element, XMLPrinter &&printer = {}) {
    element.Accept(&printer);
    return printer.CStr();
}

/** @return all `node`s children that are elements and match the tag if provided. */
std::vector<std::reference_wrapper<const XMLElement>> getChildren(const XMLNode &node,
                                                                  const char *childTag = nullptr) {
    std::vector<std::reference_wrapper<const XMLElement>> children;
    for (auto *child = node.FirstChildElement(childTag); child != nullptr;
         child = child->NextSiblingElement(childTag)) {
        children.emplace_back(*child);
    }
    return children;
}

void parseLibrary(const XMLElement &xmlLibrary, Interfaces &interfaces) {
    const char *name = xmlLibrary.Attribute("name");
    const char *libraryName = xmlLibrary.Attribute("libraryName");
    const char *method = xmlLibrary.Attribute("method");
    const char *mandatory = xmlLibrary.Attribute("mandatory");
    if (name == nullptr || method == nullptr || mandatory == nullptr || libraryName == nullptr) {
        ALOGE("library must have a name and a method: %s", dump(xmlLibrary));
        return;
    }
    bool mandatoryValue = false;
    if (strcmp(mandatory, "true") == 0) mandatoryValue = true;
    interfaces.push_back({name, libraryName, method, mandatoryValue});
}

Interfaces parseWithPath(std::string &&path) {
    ALOGI("parse interfaces %s ", path.c_str());
    XMLDocument doc;
    doc.LoadFile(path.c_str());
    if (doc.Error()) {
        ALOGE("Failed to parse %s: Tinyxml2 error (%d): %s", path.c_str(), doc.ErrorID(),
              doc.ErrorStr());
        return {};
    }
    Interfaces interfaces;

    for (auto &xmlLibraries : getChildren(doc, "libraries")) {
        for (auto &xmlLibrary : getChildren(xmlLibraries, "library")) {
            parseLibrary(xmlLibrary, interfaces);
        }
    }
    return interfaces;
}

Interfaces parseInterfaces() {
    auto path = ::android::audio_find_readable_configuration_file(DEFAULT_NAME);
    if (path.empty()) {
        LOG_ALWAYS_FATAL("%s %s not found, validate configurations", __func__, DEFAULT_NAME);
    }

    return parseWithPath(std::move(path));
}