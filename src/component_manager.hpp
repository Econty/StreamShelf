#pragma once
#include "platform.hpp"
namespace ss {
struct Component {
    std::string name, installed, latest, status, url, checksumUrl;
    bool update = false;
};
std::vector<int> componentVersion(const std::string &);
bool componentNewer(const std::string &latest, const std::string &installed);
bool componentFilePresent(const fs::path &);
std::wstring terminalComponentCommand(const std::wstring &, const fs::path &);
std::vector<Component> scanComponents(const fs::path &, std::atomic_bool &, Log trace = {});
std::vector<Component> refreshInstalledComponents(const fs::path &, std::vector<Component>, std::atomic_bool &);
std::vector<uint64_t> componentFingerprint(const fs::path &);
enum class ComponentPhase { Downloading, Verifying, Extracting, Installing };
struct ComponentProgress {
    std::string name;
    ComponentPhase phase = ComponentPhase::Downloading;
    uint64_t received = 0, total = 0;
};
using ComponentProgressCallback = std::function<void(const ComponentProgress &)>;
inline double componentProgressPercent(const ComponentProgress &p) {
    if (p.phase != ComponentPhase::Downloading || !p.total) return -1;
    return std::clamp(100.0 * double(p.received) / double(p.total), 0.0, 100.0);
}
void installComponents(const fs::path &, const std::vector<Component> &, std::atomic_bool &, Log, ComponentProgressCallback progress = {});
}
