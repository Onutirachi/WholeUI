#include "log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace wholeui::log {
namespace {

std::atomic<bool> g_verbose{false};

// Longer messages are cut; a log line never fails.
constexpr int kMaxMessage = 1024;

void Write(const wchar_t* severity, const wchar_t* area, const wchar_t* fmt, va_list args) {
    wchar_t message[kMaxMessage];
    int written = _vsnwprintf(message, kMaxMessage - 1, fmt, args);
    if (written < 0) {
        // Truncated (or a format error): _vsnwprintf leaves the buffer unterminated.
        message[kMaxMessage - 1] = L'\0';
    }
    Wh_Log(L"%s[%s] %s", severity, area, message);
}

}  // namespace

void Info(const wchar_t* area, const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Write(L"", area, fmt, args);
    va_end(args);
}

void Warn(const wchar_t* area, const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Write(L"warning: ", area, fmt, args);
    va_end(args);
}

void Verbose(const wchar_t* area, const wchar_t* fmt, ...) {
    if (!g_verbose.load(std::memory_order_relaxed)) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    Write(L"", area, fmt, args);
    va_end(args);
}

void SetVerbose(bool on) {
    g_verbose.store(on, std::memory_order_relaxed);
}

bool IsVerbose() {
    return g_verbose.load(std::memory_order_relaxed);
}

}  // namespace wholeui::log
