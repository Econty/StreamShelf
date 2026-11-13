#include "platform.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    try {
        std::atomic_bool cancel{false};
        auto body = ss::httpPreview(argv[1], cancel,
            ss::Json{{"User-Agent", "StreamShelf-Test-Agent"},
                     {"Referer", "https://page.example/watch"},
                     {"Authorization", "Bearer synthetic-test-secret"},
                     {"Origin", "https://page.example"}}, "");
        if (body != "OK") return 3;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
