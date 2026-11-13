#include "http_resume.hpp"
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    try {
        if (std::string(argv[1]) == "seed") {
            ss::writeFile(L"partial.part", "alpha");
            ss::protectFile(L"partial.part.resume", ss::Json{{"url", argv[2]}, {"etag", "\"v1\""}}.dump());
        } else {
            std::atomic_bool cancel{false};
            ss::httpDownloadResumable(argv[2], L"partial.part", cancel, [](double) {});
        }
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
