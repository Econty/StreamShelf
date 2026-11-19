#include "guide_profile_challenge.hpp"
#include <iostream>
#include <memory>

using ss::GuideProfileChallenge;
SOCKET connectProbe(const GuideProfileChallenge &probe) {
    SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto begin = probe.url.find(':', 7) + 1;
    address.sin_port = htons(static_cast<unsigned short>(std::stoi(probe.url.substr(begin))));
    if (connect(peer, reinterpret_cast<sockaddr *>(&address), sizeof(address))) throw std::runtime_error("connect");
    DWORD timeout = 1500; setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout), sizeof(timeout));
    return peer;
}
std::string request(const GuideProfileChallenge &probe, const std::string &path, bool split, std::string_view method = "GET") {
    SOCKET peer = connectProbe(probe);
    const std::string wire = std::string(method) + " " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    if (split) {
        const auto splitAt = wire.find("\r\n") + 2;
        send(peer, wire.data(), int(splitAt), 0);
        fd_set readable; FD_ZERO(&readable); FD_SET(peer, &readable);
        timeval pause{0, 35000};
        if (select(0, &readable, nullptr, nullptr, &pause) != 0) {
            closesocket(peer);
            throw std::runtime_error("responded before all request headers arrived");
        }
        send(peer, wire.data() + splitAt, int(wire.size() - splitAt), 0);
    } else send(peer, wire.data(), int(wire.size()), 0);
    std::string response; char bytes[512];
    for (int n; (n = recv(peer, bytes, sizeof(bytes), 0)) > 0;) response.append(bytes, n);
    closesocket(peer); return response;
}
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--serve") {
            GuideProfileChallenge page(ss::GuideProbePurpose::openExtensions, true, false, true);
            std::cout << page.url << std::endl;
            while (!page.pageReady() && GetTickCount64() - page.started < 15000) Sleep(20);
            if (!page.pageReady()) throw std::runtime_error("browser did not report a rendered page");
            Sleep(300);
            return 0;
        }
        GuideProfileChallenge first(ss::GuideProbePurpose::openExtensions, false, true, true);
        GuideProfileChallenge second(ss::GuideProbePurpose::verifyConnector, true);
        auto check = [](bool ok, const char *name) { if (!ok) throw std::runtime_error(name); std::cout << "PASS " << name << '\n'; };
        check(first.url != second.url && first.pingToken != second.pingToken, "independent probes have unique URLs and ping tokens");
        check(request(first, first.path + "/status", false).ends_with("{\"stage\":0}") && !first.pageReady(),
              "a status request before HTML is not a page-ready signal");
        check(request(first, first.path, false).rfind("HTTP/1.1 200 OK", 0) == 0, "exact nonce returns HTTP 200");
        const auto title = ss::guideProfilePageTitle(first.url);
        const auto page = request(first, first.path, false);
        check(!title.empty() && title != ss::guideProfilePageTitle(second.url) &&
              page.find("<title>" + title + "</title>") != std::string::npos,
              "exact probe exposes its unique title when SmartBox hides the URL path");
        check(page.find("<html lang=\"ru\">") != page.npos && page.find("class=\"opening light reduced\"") != page.npos &&
              page.find("prefers-reduced-motion") != page.npos, "opening page follows language, theme and motion settings");
        const auto alternate = request(second, second.path, false);
        check(alternate.find("<html lang=\"en\">") != alternate.npos && alternate.find("class=\"checking\"") != alternate.npos,
              "connection check uses its own localized stages");
        check(request(first, first.path + "/status", false).ends_with("{\"stage\":0}") && first.pageReady(),
              "the loaded page can report readiness without changing the verification stage");
        first.advance(ss::GuideProbeProgress::matched);
        check(request(first, first.path + "/status", false).ends_with("{\"stage\":1}"), "status reflects the guide's observation");
        first.advance(ss::GuideProbeProgress::proceeding);
        first.advance(ss::GuideProbeProgress::waiting);
        check(request(first, first.path + "/status", false).ends_with("{\"stage\":2}"), "completed progress does not regress");
        check(request(first, second.path + "/status", false).rfind("HTTP/1.1 404", 0) == 0 &&
              request(first, first.path + "/status", false, "POST").rfind("HTTP/1.1 404", 0) == 0 &&
              request(first, first.path + "/status", false).ends_with("{\"stage\":2}"), "status is bound to this probe and accepts no commands");
        check(ss::guideProfilePageTitle(first.url.substr(0, first.url.find('/', 7))).empty() &&
              request(first, "/wrong", false).find(title) == std::string::npos,
              "port alone and a wrong path do not identify the probe");
        check(request(first, first.path, true).rfind("HTTP/1.1 200 OK", 0) == 0, "split HTTP headers preserve the complete page");
        check(request(first, "/wrong", false).rfind("HTTP/1.1 404 Not Found", 0) == 0, "unrecognized path returns HTTP 404");
        auto stalled = std::make_unique<GuideProfileChallenge>();
        SOCKET peer = connectProbe(*stalled); send(peer, "GET ", 4, 0); Sleep(10);
        auto at = GetTickCount64(); stalled.reset(); auto elapsed = GetTickCount64() - at;
        closesocket(peer);
        check(elapsed < 250, "cancellation remains responsive with stalled client");
        std::cout << "stalled client shutdown: " << elapsed << " ms\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
