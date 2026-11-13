#include "identity.h"

#include <psapi.h>

#include <iterator>
#include <string>
#include <string_view>

#include "dialog_hooks.h"

namespace wholeui::compat {

namespace {

constexpr wchar_t kDialogClass[] = L"#32770";

// Ancestors walked at most when looking for hosted pages.
constexpr int kMaxAncestors = 256;

// The longest path Windows returns, in characters, terminator included.
constexpr DWORD kLongPath = 32768;

// Calls `read(buffer, capacity)` (returns the characters written, 0 on failure) with a
// MAX_PATH buffer, then once with a long-path buffer when the first one was too small or
// failed. Returns "" when both fail.
template <typename Read>
std::wstring ReadPath(Read read) {
    wchar_t small[MAX_PATH];
    DWORD length = read(small, MAX_PATH);
    if (length > 0 && length < MAX_PATH - 1) {
        return std::wstring(small, length);
    }
    std::wstring big(kLongPath, L'\0');
    length = read(big.data(), kLongPath);
    if (length == 0 || length >= kLongPath - 1) {
        return {};
    }
    big.resize(length);
    return big;
}

// The file name of `path`, lower-cased, or Unknown when `path` is empty.
Field FileName(std::wstring_view path) {
    size_t separator = path.find_last_of(L"\\/");
    std::wstring name(separator == std::wstring_view::npos ? path : path.substr(separator + 1));
    if (name.empty()) {
        return Field::Unknown();
    }
    CharLowerBuffW(name.data(), static_cast<DWORD>(name.size()));
    return Field::Of(std::move(name));
}

Field ModuleFileName(HMODULE module) {
    return FileName(ReadPath([module](wchar_t* buffer, DWORD capacity) {
        return GetModuleFileNameW(module, buffer, capacity);
    }));
}

Field OwnProcess() {
    static const Field process = ModuleFileName(nullptr);
    return process;
}

class ProcessHandle {
public:
    ProcessHandle(DWORD access, DWORD pid) : handle_(OpenProcess(access, FALSE, pid)) {}
    ~ProcessHandle() {
        if (handle_) {
            CloseHandle(handle_);
        }
    }
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    HANDLE get() const { return handle_; }

private:
    HANDLE handle_;
};

Field ProcessOf(DWORD pid) {
    if (pid == GetCurrentProcessId()) {
        return OwnProcess();
    }
    ProcessHandle process(PROCESS_QUERY_LIMITED_INFORMATION, pid);
    if (!process.get()) {
        return Field::Unknown();
    }
    return FileName(ReadPath([&process](wchar_t* buffer, DWORD capacity) {
        DWORD size = capacity;
        return QueryFullProcessImageNameW(process.get(), 0, buffer, &size) ? size : 0;
    }));
}

// The HINSTANCE passed to CreateWindowEx / CreateDialog* (research R7).
Field ModuleOf(HWND hwnd, DWORD pid) {
    auto instance = reinterpret_cast<HMODULE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
    if (!instance) {
        return Field::Unknown();
    }
    if (pid == GetCurrentProcessId()) {
        return ModuleFileName(instance);
    }
    // Denied for elevated or protected processes: Unknown.
    ProcessHandle process(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, pid);
    if (!process.get()) {
        return Field::Unknown();
    }
    return FileName(ReadPath([&process, instance](wchar_t* buffer, DWORD capacity) {
        return GetModuleFileNameExW(process.get(), instance, buffer, capacity);
    }));
}

Field ClassOf(HWND hwnd) {
    wchar_t name[257];  // class names are at most 256 characters
    int length = GetClassNameW(hwnd, name, static_cast<int>(std::size(name)));
    if (length <= 0) {
        return Field::Unknown();
    }
    return Field::Of(std::wstring(name, static_cast<size_t>(length)));
}

bool IsDialogClass(HWND hwnd) {
    wchar_t name[std::size(kDialogClass) + 1];  // one more: a longer name does not match
    int length = GetClassNameW(hwnd, name, static_cast<int>(std::size(name)));
    return length == static_cast<int>(std::size(kDialogClass)) - 1 &&
           std::wstring_view(name, static_cast<size_t>(length)) == kDialogClass;
}

}  // namespace

WindowIdentity ReadIdentity(HWND hwnd) {
    WindowIdentity identity;
    DWORD pid = 0;
    if (!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd, &pid) || pid == 0) {
        identity.failed = true;
        return identity;
    }
    identity.process = ProcessOf(pid);
    identity.module = ModuleOf(hwnd, pid);
    identity.windowClass = ClassOf(hwnd);
    if (identity.windowClass.known) {
        identity.templateId = identity.windowClass.value == kDialogClass
                                  ? DialogTemplateOf(hwnd)  // Unknown for other processes
                                  : TemplateField::NotADialog();
    }
    return identity;
}

HWND TopLevelOf(HWND hwnd) noexcept {
    return hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
}

std::vector<HWND> HostedPagesOf(HWND hwnd, HWND topLevel) {
    std::vector<HWND> pages;
    if (!hwnd || !topLevel || hwnd == topLevel) {
        return pages;
    }
    // The depth limit only guards against a chain changed mid-walk; Windows nests far less.
    int depth = 0;
    for (HWND current = GetAncestor(hwnd, GA_PARENT);
         current && current != topLevel && depth < kMaxAncestors;
         current = GetAncestor(current, GA_PARENT), ++depth) {
        if ((GetWindowLongPtrW(current, GWL_STYLE) & WS_CHILD) && IsDialogClass(current)) {
            pages.push_back(current);
        }
    }
    return pages;
}

}  // namespace wholeui::compat
