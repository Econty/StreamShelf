#pragma once
// Include before windows.h: WinSock 2 and the legacy WinSock headers conflict.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include "guide_profile_page.hpp"

namespace ss {
inline std::string guideProfilePageTitle(std::string_view url) {
    constexpr std::string_view host = "http://127.0.0.1:", path = "/streamshelf-profile-";
    if (!url.starts_with(host)) return {};
    url.remove_prefix(host.size());
    const auto slash = url.find('/');
    if (slash == 0 || slash == url.npos || url.substr(0, slash).find_first_not_of("0123456789") != url.npos) return {};
    url.remove_prefix(slash);
    if (!url.starts_with(path)) return {};
    url.remove_prefix(path.size());
    if (url.size() != 32 || url.find_first_not_of("0123456789abcdef") != url.npos) return {};
    return "StreamShelf " + std::string(url);
}
// A short-lived loopback page lets the unchanged, signed extension identify
// the browser profile containing the exact window opened by the guide.
class GuideProfileChallenge {
    SOCKET listener = INVALID_SOCKET;
    std::atomic_bool stopped{false};
    std::atomic_bool pageSeen{false};
    std::atomic<GuideProbeProgress> progress{GuideProbeProgress::waiting};
    std::thread server;
    bool winsockStarted = false;
    std::string page;

    void serve() {
        bool pageDelivered = false;
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (!stopped && std::chrono::steady_clock::now() < until) {
            fd_set reads; FD_ZERO(&reads); FD_SET(listener, &reads);
            // The guide can cancel a probe from its UI thread. Keep shutdown
            // responsive even when no browser ever opens the local page.
            timeval timeout{0, 20000};
            if (select(0, &reads, nullptr, nullptr, &timeout) <= 0) continue;
            SOCKET peer = accept(listener, nullptr, nullptr);
            if (peer == INVALID_SOCKET) continue;
            DWORD receiveTimeout = 20;
            setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&receiveTimeout), sizeof(receiveTimeout));
            setsockopt(peer, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&receiveTimeout), sizeof(receiveTimeout));
            std::string request;
            const auto accepted = std::chrono::steady_clock::now();
            auto deadline = accepted + std::chrono::milliseconds(120);
            while (!stopped && request.size() < 16 * 1024 && request.find("\r\n\r\n") == std::string::npos &&
                   std::chrono::steady_clock::now() < deadline) {
                char chunk[2048];
                int count = recv(peer, chunk, int(sizeof(chunk)), 0);
                if (count > 0) {
                    if (request.empty()) deadline = accepted + std::chrono::milliseconds(500);
                    request.append(chunk, size_t(count));
                }
                else if (!count || WSAGetLastError() != WSAETIMEDOUT) break;
            }
            const bool complete = request.find("\r\n\r\n") != std::string::npos;
            const bool matches = complete && request.starts_with("GET " + path + " ");
            const bool status = complete && request.starts_with("GET " + path + "/status ");
            if (status && pageDelivered) pageSeen = true;
            const std::string body = matches ? page : status
                ? "{\"stage\":" + std::to_string(int(progress.load())) + "}" : "Not found";
            const std::string response = std::string(matches || status ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n") +
                (matches ? "Content-Type: text/html; charset=utf-8\r\n"
                         : status ? "Content-Type: application/json; charset=utf-8\r\n" : "Content-Type: text/plain; charset=utf-8\r\n") +
                "Cache-Control: no-store\r\nContent-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
                "connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'\r\n"
                "X-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body;
            size_t sent = 0;
            for (; !stopped && sent < response.size();) {
                int count = send(peer, response.data() + sent, int(response.size() - sent), 0);
                if (count <= 0) break;
                sent += size_t(count);
            }
            if (matches && sent == response.size()) pageDelivered = true;
            shutdown(peer, SD_SEND);
            closesocket(peer);
        }
    }
  public:
    std::string path, url, pingToken;
    ULONGLONG started = GetTickCount64();
    explicit GuideProfileChallenge(GuideProbePurpose purpose = GuideProbePurpose::verifyConnector,
                                   bool english = false, bool light = false, bool reduced = false) {
        unsigned char random[16]{};
        if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
            throw std::runtime_error("Не удалось подготовить проверку профиля браузера");
        static constexpr char hex[] = "0123456789abcdef";
        std::string token;
        for (auto b : random) { token += hex[b >> 4]; token += hex[b & 15]; }
        path = "/streamshelf-profile-" + token;
        pingToken = "profile-" + token;
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("Не удалось запустить проверку профиля браузера");
        winsockStarted = true;
        try {
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET) throw std::runtime_error("Не удалось открыть локальную проверку профиля");
            sockaddr_in address{}; address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = 0;
            if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR ||
                listen(listener, 4) == SOCKET_ERROR) throw std::runtime_error("Не удалось запустить локальную проверку профиля");
            int length = sizeof(address);
            if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == SOCKET_ERROR)
                throw std::runtime_error("Не удалось определить адрес проверки профиля");
            url = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + path;
            page = guideProfilePage(guideProfilePageTitle(url), purpose, english, light, reduced);
            server = std::thread([this] { serve(); });
        } catch (...) {
            if (listener != INVALID_SOCKET) closesocket(listener);
            WSACleanup(); winsockStarted = false;
            throw;
        }
    }
    void advance(GuideProbeProgress next) {
        auto previous = progress.load();
        while (previous < next && !progress.compare_exchange_weak(previous, next)) {}
    }
    bool pageReady() const { return pageSeen; }
    ~GuideProfileChallenge() {
        stopped = true;
        if (server.joinable()) server.join();
        if (listener != INVALID_SOCKET) closesocket(listener);
        if (winsockStarted) WSACleanup();
    }
};
}
