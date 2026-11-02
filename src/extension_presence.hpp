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
    result.installed = (path != entry->end() && path->is_string() && !path->get_ref<const std::string &>().empty()) ||
        (manifest != entry->end() && manifest->is_object() && manifest->value("name", std::string()) == "StreamShelf Connector");
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
