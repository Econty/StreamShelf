#pragma once
#include "core.hpp"

namespace ss {
// A query string can select a different rendition or grant access, so compare
// the complete address and page. Never merge resources with unknown page context.
inline bool sameDiscoveredResource(const Media &a, const Media &b) {
    return !a.url.empty() && a.url == b.url && !a.page.empty() && a.page == b.page;
}

inline void mergeNetworkMetadata(Media &existing, const Media &network) {
    if (existing.size <= 0 && network.size > 0) existing.size = network.size;
    if (existing.thumbnail.empty() && !network.thumbnail.empty()) existing.thumbnail = network.thumbnail;
    // Preserve yt-dlp's format title, language, codec and quality information.
    if (!existing.format) {
        if (!network.title.empty()) existing.title = network.title;
        if (!network.quality.empty() && existing.quality != "Уточняем параметры…") existing.quality = network.quality;
    }
}
} // namespace ss
