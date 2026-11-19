#pragma once
#include "core.hpp"
namespace ss {
struct ExtensionPresence {
    bool installed = false, enabledKnown = false, enabled = false;
    bool developerKnown = false, developerOn = false;
};
// Read only our fixed ID. Empty residual dictionaries do not indicate installation.
inline ExtensionPresence extensionPresence(const Json &preferences) {
    ExtensionPresence result;
    if (!preferences.is_object()) return result;
    auto extensions = preferences.find("extensions");
    if (extensions == preferences.end() || !extensions->is_object()) return result;
    auto ui = extensions->find("ui");
    if (ui != extensions->end() && ui->is_object()) {
        auto developer = ui->find("developer_mode");
        if (developer != ui->end() && developer->is_boolean()) {
            result.developerKnown = true;
            result.developerOn = developer->get<bool>();
        }
    }
    auto settings = extensions->find("settings");
    if (settings == extensions->end() || !settings->is_object()) return result;
    auto entry = settings->find(ExtensionId);
    if (entry == settings->end() || !entry->is_object()) return result;
    auto path = entry->find("path"), manifest = entry->find("manifest");
    // The entry is already scoped to our fixed extension ID. A valid manifest
    // may use a localized __MSG_...__ name, so don't identify it by display
    // text. Empty or residual dictionaries alone are still not installation.
    bool validManifest = false;
    if (manifest != entry->end() && manifest->is_object()) {
        auto name = manifest->find("name"), version = manifest->find("version");
        auto manifestVersion = manifest->find("manifest_version");
        validManifest = name != manifest->end() && name->is_string() && !name->get_ref<const std::string &>().empty() &&
            version != manifest->end() && version->is_string() && !version->get_ref<const std::string &>().empty() &&
            manifestVersion != manifest->end() && manifestVersion->is_number_integer() && manifestVersion->get<int64_t>() > 0;
    }
    result.installed = (path != entry->end() && path->is_string() && !path->get_ref<const std::string &>().empty()) || validManifest;
    if (!result.installed) return result;
    auto reasons = entry->find("disable_reasons");
    if (reasons != entry->end() && reasons->is_array()) {
        result.enabledKnown = true; result.enabled = reasons->empty();
    } else if (reasons != entry->end() && reasons->is_number_integer()) {
        result.enabledKnown = true; result.enabled = reasons->get<int64_t>() == 0;
    } else {
        auto state = entry->find("state");
        if (state != entry->end() && state->is_number_integer()) {
            auto value = state->get<int64_t>();
            if (value == 0 || value == 1) { result.enabledKnown = true; result.enabled = value == 1; }
        }
    }
    return result;
}
}
