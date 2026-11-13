#include "dialog_hooks.h"

#include <atomic>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../core/lock.h"
#include "../core/log.h"

// A named namespace around the anonymous one: in the amalgamation every .cpp shares one
// translation unit, so compat's own anonymous namespace (engine.cpp, identity.cpp) is shared.
namespace wholeui::compat::dialogs {

namespace {

constexpr wchar_t kArea[] = L"dialogs";
constexpr wchar_t kDialogClassName[] = L"#32770";

// RT_DIALOG lookups remembered per thread, and how long one stays usable (research R8).
constexpr unsigned kLookupCount = 16;
constexpr ULONGLONG kLookupLifetimeMs = 2000;
// Past this many entries, an insert into the facts store first drops destroyed windows.
constexpr size_t kDialogSweepThreshold = 4096;
// Past this many armed hooks, arming first drops entries of threads that have exited.
constexpr size_t kHookPruneThreshold = 16;
// Unload logs one warning when waiting for running CBT callbacks takes longer than this.
constexpr ULONGLONG kUnloadWaitWarnMs = 1000;

// False before InstallDialogHooks and from the start of UninstallDialogHooks: nothing is
// remembered, armed or recorded.
std::atomic<bool> g_active{false};
// CbtProc calls running on any thread. Unload waits until it drops to zero.
std::atomic<long> g_inFlight{0};

// Saves the thread's last error and restores it on scope exit, so a hook never changes what
// the caller reads with GetLastError.
class LastErrorGuard {
public:
    LastErrorGuard() : error_(GetLastError()) {}
    ~LastErrorGuard() { SetLastError(error_); }
    LastErrorGuard(const LastErrorGuard&) = delete;
    LastErrorGuard& operator=(const LastErrorGuard&) = delete;

private:
    DWORD error_;
};

// ---- Template names -----------------------------------------------------------------------

// "#123" is how FindResource spells a numeric ID as a string.
bool NumericName(const wchar_t* name, WORD& id) {
    if (name[0] != L'#' || name[1] == L'\0') {
        return false;
    }
    unsigned value = 0;
    for (const wchar_t* c = name + 1; *c; ++c) {
        if (*c < L'0' || *c > L'9') {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(*c - L'0');
        if (value > 0xFFFF) {
            return false;
        }
    }
    id = static_cast<WORD>(value);
    return true;
}

// String resource names seen in RT_DIALOG lookups, interned so that thread-local state can
// point at one without owning memory. Never cleared: a thread may still hold a pointer after
// Wh_ModUninit, until the DLL unloads.
struct NameTable {
    SRWLOCK lock = SRWLOCK_INIT;
    std::unordered_set<std::wstring> names;
};
NameTable g_names;

// Throws only bad_alloc.
const std::wstring* Intern(const wchar_t* name) {
    std::wstring key(name);
    {
        SharedLock lock(g_names.lock);
        auto found = g_names.names.find(key);
        if (found != g_names.names.end()) {
            return &*found;
        }
    }
    ExclusiveLock lock(g_names.lock);
    return &*g_names.names.insert(std::move(key)).first;
}

// ---- Thread state ---------------------------------------------------------------------------

// An RT_DIALOG lookup: `name` when the resource has a string name, else `id`. Both empty
// means the name could not be stored: the lookup still matches its dialog, as Unknown.
struct Lookup {
    HMODULE module;
    const std::wstring* name;
    WORD id;
    ULONGLONG tick;  // GetTickCount64 when the lookup returned
};

// Trivially destructible on purpose: a thread_local destructor would run at thread exit,
// possibly after the mod is unloaded. Zero at thread start.
struct ThreadState {
    Lookup lookups[kLookupCount];  // oldest first
    unsigned lookupCount;
    unsigned lookupDepth;  // RT_DIALOG FindResource* calls in progress (outermost is recorded)
    unsigned cbtDepth;     // CbtProc calls in progress on this thread
    HHOOK cbt;             // this thread's armed CBT hook
};
static_assert(std::is_trivially_destructible_v<ThreadState>);
thread_local ThreadState t_state;

void DropLookup(ThreadState& thread, unsigned index) noexcept {
    for (unsigned i = index + 1; i < thread.lookupCount; ++i) {
        thread.lookups[i - 1] = thread.lookups[i];
    }
    --thread.lookupCount;
}

// Drops lookups older than kLookupLifetimeMs. They are in time order, so only a prefix goes.
void ExpireLookups(ThreadState& thread, ULONGLONG now) noexcept {
    unsigned expired = 0;
    while (expired < thread.lookupCount && now - thread.lookups[expired].tick > kLookupLifetimeMs) {
        ++expired;
    }
    if (expired == 0) {
        return;
    }
    for (unsigned i = expired; i < thread.lookupCount; ++i) {
        thread.lookups[i - expired] = thread.lookups[i];
    }
    thread.lookupCount -= expired;
}

void Remember(HMODULE module, LPCWSTR name) noexcept {
    if (!name) {
        return;
    }
    Lookup lookup{module ? module : GetModuleHandleW(nullptr), nullptr, 0, GetTickCount64()};
    try {
        if (IS_INTRESOURCE(name)) {
            lookup.id = LOWORD(reinterpret_cast<uintptr_t>(name));
        } else if (!NumericName(name, lookup.id)) {
            lookup.name = Intern(name);
        }
    } catch (...) {
        lookup.name = nullptr;
        lookup.id = 0;
    }
    ThreadState& thread = t_state;
    ExpireLookups(thread, lookup.tick);
    if (thread.lookupCount == kLookupCount) {
        DropLookup(thread, 0);
    }
    thread.lookups[thread.lookupCount++] = lookup;
}

// Throws only bad_alloc.
TemplateField TemplateOf(const Lookup& lookup) {
    if (lookup.name) {
        return TemplateField::Of(*lookup.name);
    }
    return lookup.id ? TemplateField::Of(std::to_wstring(lookup.id)) : TemplateField::Unknown();
}

// ---- Facts store ----------------------------------------------------------------------------

struct DialogFact {
    DWORD threadId;  // a different thread means the HWND was reused
    TemplateField templateId;
};

struct Store {
    SRWLOCK lock = SRWLOCK_INIT;
    std::unordered_map<HWND, DialogFact> dialogs;
};
Store g_store;

// Throws only bad_alloc.
void StoreDialog(HWND hwnd, TemplateField templateId) {
    DialogFact fact{GetCurrentThreadId(), std::move(templateId)};
    ExclusiveLock lock(g_store.lock);
    if (g_store.dialogs.size() >= kDialogSweepThreshold) {
        // IsWindow only reads the handle table.
        std::erase_if(g_store.dialogs, [](const auto& item) { return !IsWindow(item.first); });
    }
    g_store.dialogs.insert_or_assign(hwnd, std::move(fact));
}

// ---- Thread CBT hook ------------------------------------------------------------------------

// Armed CBT hooks, so Wh_ModUninit can remove them from its own thread. Whoever takes a hook
// out of the list unhooks it. An entry whose thread exited without disarming holds a hook
// Windows already freed; it is pruned or replaced when another thread arms.
struct ArmedHook {
    DWORD threadId;
    HHOOK hook;
};

struct HookRegistry {
    SRWLOCK lock = SRWLOCK_INIT;
    std::vector<ArmedHook> hooks;
};
HookRegistry g_hooks;
std::atomic<bool> g_armFailureLogged{false};

bool ThreadRunning(DWORD threadId) noexcept {
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
    if (!thread) {
        return false;
    }
    DWORD exitCode = 0;
    bool running = GetExitCodeThread(thread, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(thread);
    return running;
}

// Caller holds the exclusive registry lock.
void PruneExitedThreads() noexcept {
    std::erase_if(g_hooks.hooks, [](const ArmedHook& armed) { return !ThreadRunning(armed.threadId); });
}

bool IsDialogWindow(HWND hwnd) noexcept {
    wchar_t name[std::size(kDialogClassName) + 1];
    int length = GetClassNameW(hwnd, name, static_cast<int>(std::size(name)));
    return length == static_cast<int>(std::size(kDialogClassName)) - 1 &&
           std::wstring_view(name, static_cast<size_t>(length)) == kDialogClassName;
}

// HCBT_CREATEWND for a #32770: the most recent lookup for the window's module is its template.
void OnCreateWindow(HWND hwnd, const CREATESTRUCTW* create, ULONGLONG now) noexcept {
    try {
        if (!create || !IsDialogWindow(hwnd)) {
            return;
        }
        ThreadState& thread = t_state;
        HMODULE module = reinterpret_cast<HMODULE>(create->hInstance);
        for (unsigned i = thread.lookupCount; i-- > 0;) {
            if (thread.lookups[i].module != module) {
                continue;
            }
            Lookup lookup = thread.lookups[i];
            DropLookup(thread, i);
            TemplateField templateId = TemplateOf(lookup);
            if (log::IsVerbose()) {
                log::Verbose(kArea, L"dialog %p: module %p, template %s (lookup %llu ms old)",
                             hwnd, module,
                             templateId.kind == TemplateField::Kind::Value
                                 ? templateId.value.c_str()
                                 : L"unknown",
                             now - lookup.tick);
            }
            StoreDialog(hwnd, std::move(templateId));
            return;
        }
        log::Verbose(kArea, L"dialog %p: module %p, no lookup; template unknown", hwnd, module);
    } catch (...) {
        log::Warn(kArea, L"window %p: recording the dialog template failed: exception", hwnd);
    }
}

// Removes this thread's armed hook. Safe inside CbtProc after CallNextHookEx.
void Disarm() noexcept {
    ThreadState& thread = t_state;
    HHOOK hook = std::exchange(thread.cbt, nullptr);
    if (!hook) {
        return;
    }
    bool owned = false;
    {
        ExclusiveLock lock(g_hooks.lock);
        auto& hooks = g_hooks.hooks;
        for (size_t i = 0; i < hooks.size(); ++i) {
            if (hooks[i].hook == hook) {
                hooks[i] = hooks.back();
                hooks.pop_back();
                owned = true;
                break;
            }
        }
    }
    if (owned) {
        UnhookWindowsHookEx(hook);
    }
}

// Every statement that runs this module's code sits between the increment and the decrement,
// so unload can wait for it. The decrement is the last statement before `return`.
LRESULT CALLBACK CbtProc(int code, WPARAM wParam, LPARAM lParam) {
    g_inFlight.fetch_add(1, std::memory_order_acquire);
    ThreadState& thread = t_state;
    ++thread.cbtDepth;
    LRESULT result;
    if (!g_active.load(std::memory_order_relaxed)) {
        result = CallNextHookEx(nullptr, code, wParam, lParam);
    } else {
        {
            LastErrorGuard error;
            ULONGLONG now = GetTickCount64();
            ExpireLookups(thread, now);
            if (code == HCBT_CREATEWND) {
                const auto* create = reinterpret_cast<const CBT_CREATEWNDW*>(lParam);
                OnCreateWindow(reinterpret_cast<HWND>(wParam), create ? create->lpcs : nullptr,
                               now);
            }
        }
        result = CallNextHookEx(nullptr, code, wParam, lParam);
        // A lookup made during CallNextHookEx keeps the hook armed.
        if (thread.lookupCount == 0 && g_active.load(std::memory_order_relaxed)) {
            LastErrorGuard error;
            Disarm();
        }
    }
    --thread.cbtDepth;
    g_inFlight.fetch_sub(1, std::memory_order_release);
    return result;
}

void Arm() noexcept {
    ThreadState& thread = t_state;
    if (thread.cbt || !g_active.load(std::memory_order_relaxed)) {
        return;
    }
    DWORD threadId = GetCurrentThreadId();
    HHOOK hook = SetWindowsHookExW(WH_CBT, CbtProc, nullptr, threadId);
    if (!hook) {
        if (!g_armFailureLogged.exchange(true)) {
            log::Warn(kArea, L"thread CBT hook failed: error %u; dialog templates unknown",
                      static_cast<unsigned>(GetLastError()));
        }
        return;
    }
    try {
        ExclusiveLock lock(g_hooks.lock);
        // Checked under the lock: unload clears g_active before it takes the list.
        if (g_active.load()) {
            auto& hooks = g_hooks.hooks;
            // This thread is not armed, so an entry with its ID is from an exited thread.
            std::erase_if(hooks, [threadId](const ArmedHook& armed) {
                return armed.threadId == threadId;
            });
            if (hooks.size() >= kHookPruneThreshold) {
                PruneExitedThreads();
            }
            hooks.push_back(ArmedHook{threadId, hook});
            thread.cbt = hook;
            return;
        }
    } catch (...) {
    }
    UnhookWindowsHookEx(hook);
}

// ---- kernelbase hooks -----------------------------------------------------------------------

decltype(&FindResourceExW) g_findResourceEx;
decltype(&FindResourceW) g_findResource;

// Runs an RT_DIALOG lookup; when it succeeds and is the outermost on the thread, remembers it
// and arms the thread's CBT hook.
template <typename Call>
HRSRC DialogLookup(HMODULE module, LPCWSTR name, Call call) {
    ThreadState& thread = t_state;
    bool outermost = thread.lookupDepth++ == 0;
    HRSRC resource = call();
    --thread.lookupDepth;
    if (resource && outermost && g_active.load(std::memory_order_relaxed)) {
        LastErrorGuard error;
        Remember(module, name);
        Arm();
    }
    return resource;
}

HRSRC WINAPI FindResourceExHook(HMODULE module, LPCWSTR type, LPCWSTR name, WORD language) {
    if (type != RT_DIALOG) {
        return g_findResourceEx(module, type, name, language);
    }
    return DialogLookup(module, name,
                        [&] { return g_findResourceEx(module, type, name, language); });
}

HRSRC WINAPI FindResourceHook(HMODULE module, LPCWSTR name, LPCWSTR type) {
    if (type != RT_DIALOG) {
        return g_findResource(module, name, type);
    }
    return DialogLookup(module, name, [&] { return g_findResource(module, name, type); });
}

template <typename Fn>
void Hook(HMODULE module, const char* name, Fn hook, Fn* original) {
    void* target = module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    if (!target || !Wh_SetFunctionHook(target, reinterpret_cast<void*>(hook),
                                       reinterpret_cast<void**>(original))) {
        log::Warn(kArea, L"kernelbase %S: hook failed; dialog templates from it stay unknown",
                  name);
    }
}

}  // namespace

}  // namespace wholeui::compat::dialogs

namespace wholeui::compat {

void InstallDialogHooks() {
    using namespace dialogs;
    g_active.store(true);
    HMODULE kernelBase = GetModuleHandleW(L"kernelbase.dll");
    Hook(kernelBase, "FindResourceExW", &FindResourceExHook, &g_findResourceEx);
    Hook(kernelBase, "FindResourceW", &FindResourceHook, &g_findResource);
}

void UninstallDialogHooks() noexcept {
    using namespace dialogs;
    g_active.store(false);

    std::vector<ArmedHook> hooks;
    {
        ExclusiveLock lock(g_hooks.lock);
        hooks.swap(g_hooks.hooks);
    }
    for (const ArmedHook& armed : hooks) {
        UnhookWindowsHookEx(armed.hook);  // fails harmlessly for a thread that exited
    }

    // Never ends early: a callback still inside CallNextHookEx would return into freed code.
    // Callbacks on this thread (none expected) cannot finish while it waits, so they are not
    // waited for.
    const long own = static_cast<long>(t_state.cbtDepth);
    ULONGLONG start = GetTickCount64();
    bool warned = false;
    while (g_inFlight.load(std::memory_order_acquire) > own) {
        Sleep(1);
        ULONGLONG waited = GetTickCount64() - start;
        if (!warned && waited > kUnloadWaitWarnMs) {
            warned = true;
            log::Warn(kArea, L"unload waits for %ld running CBT callbacks (over %llu ms)",
                      g_inFlight.load() - own, waited);
        }
    }
    log::Verbose(kArea, L"unloaded: %u CBT hooks removed, waited %llu ms",
                 static_cast<unsigned>(hooks.size()), GetTickCount64() - start);

    std::unordered_map<HWND, DialogFact> dialogs;
    {
        ExclusiveLock lock(g_store.lock);
        dialogs.swap(g_store.dialogs);
    }
}

TemplateField DialogTemplateOf(HWND dialog) {
    using namespace dialogs;
    DWORD threadId = GetWindowThreadProcessId(dialog, nullptr);
    SharedLock lock(g_store.lock);
    auto found = g_store.dialogs.find(dialog);
    if (found == g_store.dialogs.end() || found->second.threadId != threadId) {
        return TemplateField::Unknown();
    }
    return found->second.templateId;
}

}  // namespace wholeui::compat
