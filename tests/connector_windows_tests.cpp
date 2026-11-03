#include "platform.hpp"
#include "pipe_io.hpp"
#ifndef STREAMSHELF_BASELINE
#include "browser_identity.hpp"
#endif
#include <iostream>
#include <future>

namespace {
using namespace ss;
struct Pipes {
    Handle server, client;
    Pipes(bool asynchronous) {
        static unsigned serial = 0;
        auto name = L"\\\\.\\pipe\\StreamShelf-regression-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
        server.h = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX |
            (asynchronous ? FILE_FLAG_OVERLAPPED : 0), PIPE_TYPE_BYTE | PIPE_WAIT |
            PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, nullptr);
        if (server.h == INVALID_HANDLE_VALUE) throw std::runtime_error("Create pipe");
        client.h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (client.h == INVALID_HANDLE_VALUE) throw std::runtime_error("Open pipe");
        if (asynchronous) {
            std::atomic_bool stop{false};
            if (!pipeio::connect(server, stop)) throw std::runtime_error("Connect pipe");
        } else if (!ConnectNamedPipe(server, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
            throw std::runtime_error("Connect synchronous pipe");
    }
};
int passed = 0, failed = 0;
void check(const char* label, bool result) {
    std::cout << (result ? "PASS " : "FAIL ") << label << std::endl;
    result ? ++passed : ++failed;
}
void duplex(bool reverse = false) {
#ifdef STREAMSHELF_BASELINE
    Pipes pipes(false);
#else
    Pipes pipes(true);
#endif
    auto sendHandle = reverse ? pipes.client.h : pipes.server.h;
    auto peer = reverse ? pipes.server.h : pipes.client.h;
    Session session;
    session.pipe = sendHandle;
    std::atomic_bool reading{false}, readEnded{false}, sent{false}, sendOk{false};
    BYTE incoming = 0;
    std::thread reader([&] {
        reading = true;
#ifdef STREAMSHELF_BASELINE
        DWORD count = 0;
        ReadFile(sendHandle, &incoming, 1, &count, nullptr);
#else
        pipeio::transfer(sendHandle, &incoming, 1, false);
#endif
        readEnded = true;
    });
    while (!reading) Sleep(1);
    Sleep(100); // Let the receive operation enter its pending state.
    auto start = GetTickCount64();
    std::thread writer([&] {
        try { session.send({{"type", "scanStart"}, {"tabId", 7}}); sendOk = true; }
        catch (...) {}
        sent = true;
    });
    while (!sent && GetTickCount64() - start < 1000) Sleep(5);
    check(reverse ? "client writes while its read is pending" : "scan command writes while server read is pending",
          sent && sendOk && !readEnded);
#ifdef STREAMSHELF_BASELINE
    CancelSynchronousIo(reader.native_handle());
#else
    CancelIoEx(sendHandle, nullptr);
#endif
    reader.join();
    while (!sent && GetTickCount64() - start < 3000) Sleep(5);
    if (!sent) {
        CancelSynchronousIo(writer.native_handle());
        CancelIoEx(sendHandle, nullptr);
    }
    writer.join();
    if (sendOk) {
        uint32_t size = 0;
        bool ok = pipeio::transfer(peer, &size, 4, false, 1000) && size < MaxFrame;
        std::string bytes(ok ? size : 0, '\0');
        if (ok) ok = pipeio::transfer(peer, bytes.data(), size, false, 1000);
        auto message = Json::parse(bytes, nullptr, false);
        check("complete framed scan command reaches peer", ok && str(message, "type") == "scanStart");
    }
}
void blockedPeer() {
    Pipes pipes(true);
    Session session; session.pipe = pipes.server;
    std::atomic_bool done{false}, rejected{false};
    auto start = GetTickCount64();
    std::thread writer([&] {
        try { session.send({{"type", "test"}, {"payload", std::string(MaxFrame - 100, 'x')}}); }
        catch (...) { rejected = true; }
        done = true;
    });
    while (!done && GetTickCount64() - start < 6500) Sleep(10);
    bool bounded = done && rejected;
    if (!done) CancelIoEx(pipes.server, nullptr);
    writer.join();
    check("non-reading peer times out instead of hanging", bounded);
}
void cancelRead() {
    Pipes pipes(true); BYTE byte = 0;
    auto pending = std::async(std::launch::async, [&] { return pipeio::transfer(pipes.server, &byte, 1, false); });
    Sleep(100); CancelIoEx(pipes.server, nullptr);
    auto ready = pending.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
    check("pending read can be cancelled promptly", ready && !pending.get());
}
#ifndef STREAMSHELF_BASELINE
void identityAndRetry() {
    std::map<DWORD, DWORD> parents{{10, 20}, {20, 30}, {30, 30}};
    std::map<DWORD, std::wstring> paths{{10, L"C:\\App\\StreamShelf.Host.exe"}, {20, L"C:\\Windows\\System32\\cmd.exe"}, {30, L"C:\\Browser\\Chrome.exe"}};
    auto path = browserFromAncestors(10, parents, [&](DWORD pid) { return paths[pid]; });
    check("host parent chain identifies browser through cmd", path == "C:\\Browser\\Chrome.exe");
    check("browser executable matches case-insensitively", sameBrowserExecutable(path, "c:\\browser\\chrome.EXE"));
    check("different browser and unknown process cannot confirm", !sameBrowserExecutable(path, "C:\\Yandex\\browser.exe") && !sameBrowserExecutable("", path));
    paths[30] = L"C:\\Other\\unknown.exe";
    check("unknown and cyclic ancestry remain unverified", browserFromAncestors(10, parents, [&](DWORD pid) { return paths[pid]; }).empty());
    Pipes pipes(true); Session session; session.pipe = pipes.server;
    auto receive = [&]() {
        uint32_t size = 0;
        if (!pipeio::transfer(pipes.client, &size, 4, false, 1000) || !size || size >= MaxFrame) throw std::runtime_error("ping frame length");
        std::string body(size, '\0');
        if (!pipeio::transfer(pipes.client, body.data(), size, false, 1000)) throw std::runtime_error("ping frame data");
        return Json::parse(body);
    };
    session.verifyConnection(); auto first = receive();
    session.verifyConnection(); auto second = receive();
    auto oldId = str(first, "requestId"), newId = str(second, "requestId");
    check("retry sends a fresh challenge", str(first, "type") == "ping" && !oldId.empty() && oldId != newId);
    check("stale and wrong pong do not confirm", !session.acceptVerification(oldId) && !session.acceptVerification("wrong") && !session.verified);
    check("matching pong confirms exactly once", session.acceptVerification(newId) && session.verified && !session.acceptVerification(newId));
    check("matching pong records the time for connection expiry", session.lastPong > 0 && session.lastPong <= GetTickCount64());
    std::atomic_bool cancel{false};
    const std::string url="https://example.test/video?token=one";
    auto request=[&](const std::string &target, Json reply) {
        auto pending=std::async(std::launch::async,[&]{return session.credentials(target,cancel);});
        auto frame=receive();
        reply["requestId"]=str(frame,"requestId");
        session.acceptCredentialReply(reply);
        return pending.get();
    };
    auto firstSnapshot=request(url,{{"url",url},{"headers",{{"Referer","https://example.test/"}}}});
    auto cached=request(url,{{"type","error"},{"message","tab changed"}});
    check("exact resource snapshot survives a tab change",cached==firstSnapshot);
    bool rejected=false;
    try { request("https://example.test/video?token=two",{{"type","error"},{"message","tab changed"}}); }
    catch(const std::exception &) { rejected=true; }
    check("different resource URL cannot reuse a snapshot",rejected);
    auto abandoned=std::async(std::launch::async,[&]{try { session.credentials(url,cancel); } catch (...) {} });
    auto oldRequest=receive();
    cancel=true; abandoned.get(); cancel=false;
    for (int i=0; i<120; ++i) session.acceptCredentialReply({{"requestId",str(oldRequest,"requestId")},{"url",url}});
    { std::lock_guard lock(session.mutex); check("late cancelled replies are discarded",session.replies.empty() && session.pendingCredentials.empty()); }
    auto fresh=request(url,{{"url",url},{"headers",{{"Referer","fresh"}}}});
    check("new request succeeds after late replies",str(fresh["headers"],"Referer")=="fresh");

}
#endif
}
int main() {
    std::thread([] { Sleep(20000); ExitProcess(124); }).detach();
    try {
        duplex();
#ifndef STREAMSHELF_BASELINE
        duplex(true);
        blockedPeer();
        cancelRead();
        identityAndRetry();
#endif
    } catch (const std::exception& e) { std::cerr << e.what() << std::endl; ++failed; }
    std::cout << "RESULT " << passed << " passed, " << failed << " failed" << std::endl;
    return failed ? 1 : 0;
}
