#pragma once
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace ss::pipeio {
// A duplex synchronous file handle serializes a pending read and a write.
// Each named-pipe operation therefore owns an independent OVERLAPPED event.
inline bool transfer(HANDLE pipe, void* data, size_t size, bool write,
                     DWORD timeout = INFINITE) {
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) return false;
    auto* bytes = static_cast<BYTE*>(data);
    const auto deadline = timeout == INFINITE ? 0 : GetTickCount64() + timeout;
    bool success = true;
    while (size) {
        ResetEvent(event);
        OVERLAPPED operation{};
        operation.hEvent = event;
        DWORD count = 0;
        const auto requested = DWORD(std::min<size_t>(size, MAXDWORD));
        BOOL done = write ? WriteFile(pipe, bytes, requested, &count, &operation)
                          : ReadFile(pipe, bytes, requested, &count, &operation);
        if (!done) {
            if (GetLastError() != ERROR_IO_PENDING) { success = false; break; }
            DWORD wait = INFINITE;
            if (timeout != INFINITE) {
                auto now = GetTickCount64();
                wait = now >= deadline ? 0 : DWORD(deadline - now);
            }
            if (WaitForSingleObject(event, wait) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &operation);
                // The stack OVERLAPPED and its buffer must survive cancellation completion.
                GetOverlappedResult(pipe, &operation, &count, TRUE);
                SetLastError(ERROR_TIMEOUT);
                success = false;
                break;
            }
            if (!GetOverlappedResult(pipe, &operation, &count, FALSE)) {
                success = false;
                break;
            }
        }
        if (!count) { success = false; break; }
        bytes += count;
        size -= count;
    }
    CloseHandle(event);
    return success;
}

inline bool connect(HANDLE pipe, const std::atomic_bool& stopping) {
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) return false;
    OVERLAPPED operation{};
    operation.hEvent = event;
    bool connected = ConnectNamedPipe(pipe, &operation) != FALSE;
    auto error = connected ? ERROR_SUCCESS : GetLastError();
    if (error == ERROR_PIPE_CONNECTED) connected = true;
    else if (error == ERROR_IO_PENDING) {
        while (!stopping) {
            auto state = WaitForSingleObject(event, 100);
            if (state == WAIT_OBJECT_0) {
                DWORD ignored = 0;
                connected = GetOverlappedResult(pipe, &operation, &ignored, FALSE) != FALSE;
                break;
            }
            if (state == WAIT_FAILED) break;
        }
        if (!connected) {
            CancelIoEx(pipe, &operation);
            DWORD ignored = 0;
            GetOverlappedResult(pipe, &operation, &ignored, TRUE);
        }
    }
    CloseHandle(event);
    return connected && !stopping;
}
} // namespace ss::pipeio
