#pragma once
#include "platform.hpp"

namespace ss {
// Continue only when a strong server validator and a valid byte range prove
// that the saved prefix belongs to the same response. Otherwise start over.
void httpDownloadResumable(const std::string &url, const fs::path &path, std::atomic_bool &cancel,
    std::function<void(double)> progress, const Json &headers = Json::object(),
    const std::string &cookieText = "", std::function<void(uint64_t, uint64_t)> onBytes = {});
}
