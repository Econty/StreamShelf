#include "job_access.hpp"
#include <iostream>

using namespace ss;

int main() {
    const auto key = newJobAccessKey();
    const auto anotherKey = newJobAccessKey();
    const auto root = fs::temp_directory_path() / wide("StreamShelf-JobAccessTest-" + key);
    const auto scope = Json::array({"job-1", "format-1", "https://source.test/page", "https://source.test/media", "https://source.test/media", ""});
    const auto jar = std::string("# Netscape HTTP Cookie File\nsource.test\tFALSE\t/\tTRUE\t0\tsession\tsynthetic-private-value\n");
    int result = 0;
    const auto verify = [](bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
        std::cout << "PASS " << message << '\n';
    };
    const auto rejects = [](const auto &call) {
        try { call(); }
        catch (const std::exception &) { return true; }
        return false;
    };
    try {
        verify(validJobAccessKey(key) && key != anotherKey && !validJobAccessKey("../escapes.dpapi"), "identifiers are random and path-safe");
        const JobDownloadAccess initial{{jar, Json{{"User-Agent", "checkpoint-test"}}}, DownloadAccess{jar, Json::object()}};
        const DownloadAccess page{jar, Json{{"Authorization", "test"}, {"User-Agent", "checkpoint-test"}, {"Origin", "https://source.test"}}};
        const auto sameOrigin = subtitleDownloadAccess("https://source.test/page", "https://SOURCE.test:443/track", page);
        const auto otherOrigin = subtitleDownloadAccess("https://source.test/page", "https://cdn.test/track", page);
        verify(sameOrigin.first == jar && sameOrigin.second.contains("Referer") && !sameOrigin.second.contains("Authorization"), "extracted same-origin subtitles use cookie scope without forwarding authorization headers");
        verify(otherOrigin.first.empty() && otherOrigin.second == Json{{"User-Agent", "checkpoint-test"}} &&
            !sameDownloadOrigin("https://source.test/page", "https://source.test:8443/track"), "subtitle CDN request carries no unrelated page credentials");
        saveJobAccess(root, key, scope, initial);
        const auto path = jobAccessPath(root, key);
        verify(readFile(path).find("synthetic-private-value") == std::string::npos, "on-disk bytes do not contain the cookie value");
        const auto restored = readJobAccess(root, key, scope);
        verify(restored && restored->target == initial.target && restored->page == initial.page, "restore exact target and independent page access");
        auto otherScope = scope;
        otherScope[3] = "https://source.test/other?nonce=changed";
        verify(rejects([&] { readJobAccess(root, key, otherScope); }), "different exact URL cannot replay a checkpoint");
        fs::copy_file(path, jobAccessPath(root, anotherKey));
        verify(rejects([&] { readJobAccess(root, anotherKey, scope); }), "copying a checkpoint to another key cannot replay it");
        saveJobAccess(root, key, scope, {{"# Netscape HTTP Cookie File\n", Json::object()}, std::nullopt});
        const auto replaced = readJobAccess(root, key, scope);
        verify(replaced && replaced->target.first.find("synthetic-private-value") == std::string::npos && !replaced->page, "new authoritative snapshot replaces and removes old values");
        verify(rejects([&] { saveJobAccess(root, key, scope, {{jar, Json{{"Authorization", "test"}}}, std::nullopt}); }), "durable checkpoints exclude bearer headers");
        removeOrphanJobAccess(root, {key});
        verify(fs::exists(path) && !fs::exists(jobAccessPath(root, anotherKey)), "prune only orphaned checkpoints");
        eraseJobAccess(root, key);
        verify(!readJobAccess(root, key, scope), "explicit removal erases checkpoint");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    fs::remove(jobAccessPath(root, key), ignored);
    fs::remove(jobAccessPath(root, anotherKey), ignored);
    fs::remove(root / L"download-access", ignored);
    fs::remove(root, ignored);
    return result;
}
