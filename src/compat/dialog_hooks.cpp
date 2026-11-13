#include "dialog_hooks.h"

#include <commctrl.h>

#include <atomic>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../core/lock.h"
#include "../core/log.h"
#include "engine.h"
#include "identity.h"

// A named namespace around the anonymous one: in the amalgamation every .cpp shares one
// translation unit, so compat's own anonymous namespace (engine.cpp, identity.cpp) is shared.
namespace wholeui::compat::dialogs {

namespace {

constexpr wchar_t kArea[] = L"dialogs";
constexpr wchar_t kDialogClassName[] = L"#32770";

// RT_DIALOG lookups remembered per thread (research R8 "Revision").
constexpr unsigned kLookupCount = 16;
// Past this many entries, an insert first drops destroyed windows.
constexpr size_t kDialogSweepThreshold = 4096;
constexpr size_t kSheetSweepThreshold = 64;
// Page handles never passed to PropertySheetW are dropped past this many.
constexpr size_t kPageHandleLimit = 1024;
// comctl32 instances that can be hooked (v5 and v6 are the usual two).
constexpr int kComctlSlots = 4;

std::atomic<bool> g_active{false};

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

// A numeric ID as a decimal string ("1040"), or the resource name. Throws only bad_alloc.
TemplateField TemplateOf(LPCWSTR name) {
    if (!name) {
        return TemplateField::Unknown();
    }
    WORD id = 0;
    if (IS_INTRESOURCE(name)) {
        id = LOWORD(reinterpret_cast<uintptr_t>(name));
    } else if (!NumericName(name, id)) {
        return TemplateField::Of(name);
    }
    return id ? TemplateField::Of(std::to_wstring(id)) : TemplateField::Unknown();
}

// String resource names seen in RT_DIALOG lookups, interned so that thread-local state can
// point at one without owning memory. Never cleared before the DLL unloads: a thread may still
// hold a pointer after Wh_ModUninit.
struct NameTable {
    SRWLOCK lock = SRWLOCK_INIT;
    std::unordered_set<std::wstring> names;
};
NameTable g_names;

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
// means the name could not be stored: the lookup still hides older ones for that module.
struct Lookup {
    HMODULE module;
    const std::wstring* name;
    WORD id;
};

// One hooked dialog call in progress on the thread (lives on the hook's stack).
struct Scope {
    HINSTANCE instance;
    bool captures;  // false for PropertySheetW, which creates no dialog of its own
    bool done;      // its dialog was seen
    TemplateField templateId;
    Scope* outer;
};

// The pages of a PropertySheetW call in progress, until its sheet window is created.
struct PendingSheet {
    HWND hwnd = nullptr;
    bool incomplete = false;
    std::vector<WindowIdentity> pages;
    PendingSheet* outer = nullptr;
};

// Trivially destructible on purpose: a thread_local destructor would run at thread exit,
// possibly after the mod is unloaded. Zero at thread start.
struct ThreadState {
    Lookup lookups[kLookupCount];
    unsigned lookupNext;
    unsigned lookupCount;
    unsigned depth;  // hooked dialog calls in progress
    HHOOK cbt;
    Scope* scope;  // innermost call
    PendingSheet* sheet;
    bool hookingComctl;  // HookComctlInstances runs on this thread
};
static_assert(std::is_trivially_destructible_v<ThreadState>);
thread_local ThreadState t_state;

void Remember(HMODULE module, LPCWSTR name) noexcept {
    if (!name || !g_active.load(std::memory_order_relaxed)) {
        return;
    }
    Lookup lookup{module, nullptr, 0};
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
    thread.lookups[thread.lookupNext] = lookup;
    thread.lookupNext = (thread.lookupNext + 1) % kLookupCount;
    if (thread.lookupCount < kLookupCount) {
        ++thread.lookupCount;
    }
}

// The template of the most recent lookup on this thread for `module`. Throws only bad_alloc.
TemplateField RecentLookup(HINSTANCE module) {
    const ThreadState& thread = t_state;
    for (unsigned i = 1; i <= thread.lookupCount; ++i) {
        unsigned index = (thread.lookupNext + kLookupCount - i) % kLookupCount;
        const Lookup& lookup = thread.lookups[index];
        if (lookup.module != module) {
            continue;
        }
        if (lookup.name) {
            return TemplateField::Of(*lookup.name);
        }
        return lookup.id ? TemplateField::Of(std::to_wstring(lookup.id)) : TemplateField::Unknown();
    }
    return TemplateField::Unknown();
}

// ---- Facts store ----------------------------------------------------------------------------

struct DialogFact {
    DWORD threadId;  // a different thread means the HWND was reused
    TemplateField templateId;
};

struct SheetFact {
    DWORD threadId;
    bool incomplete;
    std::vector<WindowIdentity> pages;
};

struct PageRecord {
    Field module;
    TemplateField templateId;
};

struct Store {
    SRWLOCK lock = SRWLOCK_INIT;
    std::unordered_map<HWND, DialogFact> dialogs;
    std::unordered_map<HWND, SheetFact> sheets;
    std::unordered_map<HPROPSHEETPAGE, PageRecord> pageHandles;
};
Store g_store;

WindowIdentity PageIdentity(Field module, TemplateField templateId) {
    WindowIdentity page;
    page.process = CurrentProcessName();
    page.module = std::move(module);
    page.windowClass = Field::Of(kDialogClassName);
    page.templateId = std::move(templateId);
    return page;
}

const wchar_t* FieldText(const Field& field) {
    return field.known ? field.value.c_str() : L"unknown";
}

const wchar_t* TemplateText(const TemplateField& field) {
    return field.kind == TemplateField::Kind::Value ? field.value.c_str() : L"unknown";
}

// Caller holds the exclusive store lock. IsWindow only reads the handle table.
template <typename Map>
void SweepDestroyed(Map& map, size_t threshold) {
    if (map.size() >= threshold) {
        std::erase_if(map, [](const auto& item) { return !IsWindow(item.first); });
    }
}

void StoreDialog(HWND hwnd, TemplateField templateId) {
    if (log::IsVerbose()) {
        log::Verbose(kArea, L"dialog %p: template %s", hwnd, TemplateText(templateId));
    }
    DialogFact fact{GetCurrentThreadId(), std::move(templateId)};
    ExclusiveLock lock(g_store.lock);
    SweepDestroyed(g_store.dialogs, kDialogSweepThreshold);
    g_store.dialogs.insert_or_assign(hwnd, std::move(fact));
}

void StoreSheet(HWND hwnd, PendingSheet& pending) {
    if (log::IsVerbose()) {
        std::wstring text;
        for (const WindowIdentity& page : pending.pages) {
            text += L" ";
            text += FieldText(page.module);
            text += L" ";
            text += TemplateText(page.templateId);
            text += L";";
        }
        log::Verbose(kArea, L"sheet %p: %u pages%s%s", hwnd,
                     static_cast<unsigned>(pending.pages.size()),
                     pending.incomplete ? L" (list incomplete)" : L"", text.c_str());
    }
    SheetFact fact{GetCurrentThreadId(), pending.incomplete, std::move(pending.pages)};
    ExclusiveLock lock(g_store.lock);
    SweepDestroyed(g_store.sheets, kSheetSweepThreshold);
    g_store.sheets.insert_or_assign(hwnd, std::move(fact));
}

// A child #32770 of a known sheet whose module is not one of its pages: a page added after
// the sheet was created (FR-013a). Adds it and tells the engine.
void CheckLatePage(HWND sheet, HINSTANCE instance, const TemplateField& templateId) {
    DWORD threadId = GetWindowThreadProcessId(sheet, nullptr);
    {
        SharedLock lock(g_store.lock);
        if (!g_store.sheets.contains(sheet)) {
            return;  // the common case: the parent is not a sheet
        }
    }
    Field module = ModuleNameOf(instance);
    WindowIdentity page = PageIdentity(module, templateId);
    {
        ExclusiveLock lock(g_store.lock);
        auto found = g_store.sheets.find(sheet);
        if (found == g_store.sheets.end() || found->second.threadId != threadId) {
            return;
        }
        for (const WindowIdentity& known : found->second.pages) {
            if (module.known && known.module.known && known.module.value == module.value) {
                return;
            }
        }
        found->second.pages.push_back(std::move(page));
    }
    log::Verbose(kArea, L"sheet %p: page added later: %s %s", sheet, FieldText(module),
                 TemplateText(templateId));
    OnSheetPagesChanged(sheet);
}

// ---- Thread CBT hook ------------------------------------------------------------------------

// CBT hooks installed on any thread, so Wh_ModUninit can remove those still in place.
// Whoever takes a hook out of the list unhooks it.
struct HookRegistry {
    SRWLOCK lock = SRWLOCK_INIT;
    std::vector<HHOOK> hooks;
};
HookRegistry g_hooks;
std::atomic<bool> g_cbtFailureLogged{false};

bool IsDialogWindow(HWND hwnd) {
    wchar_t name[std::size(kDialogClassName) + 1];
    int length = GetClassNameW(hwnd, name, static_cast<int>(std::size(name)));
    return length == static_cast<int>(std::size(kDialogClassName)) - 1 &&
           std::wstring_view(name, static_cast<size_t>(length)) == kDialogClassName;
}

void OnCreateWindow(HWND hwnd, const CREATESTRUCTW* create) noexcept {
    try {
        if (!create || !IsDialogWindow(hwnd)) {
            return;
        }
        ThreadState& thread = t_state;
        // The sheet is the first #32770 created on the thread during PropertySheetW, inside
        // whichever call creates it.
        if (thread.sheet && !thread.sheet->hwnd) {
            thread.sheet->hwnd = hwnd;
            StoreSheet(hwnd, *thread.sheet);
        }
        TemplateField templateId = TemplateField::Unknown();
        Scope* scope = thread.scope;
        if (scope && scope->captures && !scope->done && create->hInstance == scope->instance) {
            scope->done = true;
            templateId = scope->templateId;
            StoreDialog(hwnd, templateId);
        }
        if ((create->style & WS_CHILD) && create->hwndParent) {
            CheckLatePage(create->hwndParent, create->hInstance, templateId);
        }
    } catch (...) {
        log::Warn(kArea, L"window %p: recording dialog facts failed: exception", hwnd);
    }
}

LRESULT CALLBACK CbtProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_CREATEWND && g_active.load(std::memory_order_relaxed)) {
        LastErrorGuard error;
        const auto* create = reinterpret_cast<const CBT_CREATEWNDW*>(lParam);
        OnCreateWindow(reinterpret_cast<HWND>(wParam), create ? create->lpcs : nullptr);
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void InstallCbt() noexcept {
    if (!g_active.load()) {
        return;
    }
    HHOOK hook = SetWindowsHookExW(WH_CBT, CbtProc, nullptr, GetCurrentThreadId());
    if (!hook) {
        if (!g_cbtFailureLogged.exchange(true)) {
            log::Warn(kArea, L"thread CBT hook failed: error %u; dialog facts unknown",
                      static_cast<unsigned>(GetLastError()));
        }
        return;
    }
    try {
        ExclusiveLock lock(g_hooks.lock);
        if (g_active.load()) {
            g_hooks.hooks.push_back(hook);
            t_state.cbt = hook;
            return;
        }
    } catch (...) {
    }
    UnhookWindowsHookEx(hook);
}

void RemoveCbt() noexcept {
    HHOOK hook = std::exchange(t_state.cbt, nullptr);
    if (!hook) {
        return;
    }
    bool owned = false;
    {
        ExclusiveLock lock(g_hooks.lock);
        auto& hooks = g_hooks.hooks;
        for (size_t i = 0; i < hooks.size(); ++i) {
            if (hooks[i] == hook) {
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

// Marks one hooked dialog call on the thread. The outermost one installs the CBT hook; when it
// returns, the hook is removed and the remembered lookups are cleared.
class CallScope {
public:
    CallScope(HINSTANCE instance, bool captures, TemplateField templateId) noexcept
        : scope_{instance, captures, false, std::move(templateId), t_state.scope} {
        LastErrorGuard error;
        ThreadState& thread = t_state;
        if (thread.depth++ == 0) {
            InstallCbt();
        }
        thread.scope = &scope_;
    }
    ~CallScope() {
        LastErrorGuard error;  // keep the error set by the original function
        ThreadState& thread = t_state;
        thread.scope = scope_.outer;
        if (--thread.depth == 0) {
            RemoveCbt();
            thread.lookupCount = 0;
        }
    }
    CallScope(const CallScope&) = delete;
    CallScope& operator=(const CallScope&) = delete;

private:
    Scope scope_;
};

// The template of a non-indirect call. Never throws: Unknown on failure.
TemplateField CallTemplate(LPCWSTR name) noexcept {
    if (!g_active.load(std::memory_order_relaxed)) {
        return TemplateField::Unknown();
    }
    try {
        return TemplateOf(name);
    } catch (...) {
        return TemplateField::Unknown();
    }
}

// The template of an indirect call. Never throws: Unknown on failure.
TemplateField IndirectTemplate(HINSTANCE instance) noexcept {
    if (!g_active.load(std::memory_order_relaxed)) {
        return TemplateField::Unknown();
    }
    try {
        return RecentLookup(instance);
    } catch (...) {
        return TemplateField::Unknown();
    }
}

// ---- user32 and kernelbase hooks ------------------------------------------------------------

decltype(&CreateDialogParamW) g_createDialogParam;
decltype(&DialogBoxParamW) g_dialogBoxParam;
decltype(&CreateDialogIndirectParamW) g_createDialogIndirectParam;
decltype(&DialogBoxIndirectParamW) g_dialogBoxIndirectParam;
decltype(&FindResourceExW) g_findResourceEx;
decltype(&FindResourceW) g_findResource;
decltype(&LoadLibraryExW) g_loadLibraryEx;

HWND WINAPI CreateDialogParamHook(HINSTANCE instance, LPCWSTR name, HWND parent, DLGPROC proc,
                                  LPARAM param) {
    CallScope scope(instance, true, CallTemplate(name));
    return g_createDialogParam(instance, name, parent, proc, param);
}

INT_PTR WINAPI DialogBoxParamHook(HINSTANCE instance, LPCWSTR name, HWND parent, DLGPROC proc,
                                  LPARAM param) {
    CallScope scope(instance, true, CallTemplate(name));
    return g_dialogBoxParam(instance, name, parent, proc, param);
}

HWND WINAPI CreateDialogIndirectParamHook(HINSTANCE instance, LPCDLGTEMPLATEW dialogTemplate,
                                          HWND parent, DLGPROC proc, LPARAM param) {
    CallScope scope(instance, true, IndirectTemplate(instance));
    return g_createDialogIndirectParam(instance, dialogTemplate, parent, proc, param);
}

INT_PTR WINAPI DialogBoxIndirectParamHook(HINSTANCE instance, LPCDLGTEMPLATEW dialogTemplate,
                                          HWND parent, DLGPROC proc, LPARAM param) {
    CallScope scope(instance, true, IndirectTemplate(instance));
    return g_dialogBoxIndirectParam(instance, dialogTemplate, parent, proc, param);
}

HRSRC WINAPI FindResourceExHook(HMODULE module, LPCWSTR type, LPCWSTR name, WORD language) {
    HRSRC resource = g_findResourceEx(module, type, name, language);
    if (resource && type == RT_DIALOG) {
        Remember(module, name);
    }
    return resource;
}

HRSRC WINAPI FindResourceHook(HMODULE module, LPCWSTR name, LPCWSTR type) {
    HRSRC resource = g_findResource(module, name, type);
    if (resource && type == RT_DIALOG) {
        Remember(module, name);
    }
    return resource;
}

// ---- comctl32 hooks -------------------------------------------------------------------------

using PropertySheetFn = INT_PTR(WINAPI*)(LPCPROPSHEETHEADERW);
using CreatePageFn = HPROPSHEETPAGE(WINAPI*)(LPCPROPSHEETPAGEW);

void RecordPageHandle(HPROPSHEETPAGE handle, LPCPROPSHEETPAGEW page) noexcept {
    try {
        if (page->dwSize < PROPSHEETPAGEW_V1_SIZE) {
            return;  // unreadable: PropertySheetW lists the handle as an unknown page
        }
        PageRecord record{ModuleNameOf(page->hInstance),
                          (page->dwFlags & PSP_DLGINDIRECT) ? TemplateField::Unknown()
                                                            : TemplateOf(page->pszTemplate)};
        ExclusiveLock lock(g_store.lock);
        if (g_store.pageHandles.size() >= kPageHandleLimit) {
            g_store.pageHandles.clear();
        }
        g_store.pageHandles.insert_or_assign(handle, std::move(record));
    } catch (...) {
        log::Warn(kArea, L"page %p: recording failed: exception", handle);
    }
}

HPROPSHEETPAGE CreatePageCommon(CreatePageFn original, LPCPROPSHEETPAGEW page) {
    HPROPSHEETPAGE handle = original(page);
    if (handle && page && g_active.load(std::memory_order_relaxed)) {
        LastErrorGuard error;
        RecordPageHandle(handle, page);
    }
    return handle;
}

// The pages listed in a sheet header. Returns false when the list could not be read whole.
bool CollectPages(const PROPSHEETHEADERW& header, std::vector<WindowIdentity>& pages) {
    if (header.dwFlags & PSH_PROPSHEETPAGE) {
        const auto* cursor = reinterpret_cast<const BYTE*>(header.ppsp);
        for (UINT i = 0; i < header.nPages; ++i) {
            const auto* page = reinterpret_cast<const PROPSHEETPAGEW*>(cursor);
            if (!page || page->dwSize < PROPSHEETPAGEW_V1_SIZE) {
                return false;
            }
            pages.push_back(PageIdentity(ModuleNameOf(page->hInstance),
                                         (page->dwFlags & PSP_DLGINDIRECT)
                                             ? TemplateField::Unknown()
                                             : TemplateOf(page->pszTemplate)));
            cursor += page->dwSize;
        }
        return true;
    }
    if (header.nPages > 0 && !header.phpage) {
        return false;
    }
    for (UINT i = 0; i < header.nPages; ++i) {
        PageRecord record{Field::Unknown(), TemplateField::Unknown()};
        {
            ExclusiveLock lock(g_store.lock);
            auto found = g_store.pageHandles.find(header.phpage[i]);
            if (found != g_store.pageHandles.end()) {
                record = std::move(found->second);
                g_store.pageHandles.erase(found);
            }
        }
        pages.push_back(PageIdentity(std::move(record.module), std::move(record.templateId)));
    }
    return true;
}

// Makes the call's pages the thread's pending sheet while PropertySheetW runs.
class SheetScope {
public:
    explicit SheetScope(PendingSheet& pending) : pending_(pending) {
        pending_.outer = t_state.sheet;
        t_state.sheet = &pending_;
    }
    ~SheetScope() { t_state.sheet = pending_.outer; }
    SheetScope(const SheetScope&) = delete;
    SheetScope& operator=(const SheetScope&) = delete;

private:
    PendingSheet& pending_;
};

INT_PTR PropertySheetCommon(PropertySheetFn original, LPCPROPSHEETHEADERW header) {
    if (!header || !g_active.load(std::memory_order_relaxed)) {
        return original(header);
    }
    PendingSheet pending;
    {
        LastErrorGuard error;
        try {
            pending.incomplete = !CollectPages(*header, pending.pages);
        } catch (...) {
            pending.incomplete = true;
            pending.pages.clear();
        }
    }
    SheetScope sheet(pending);
    CallScope scope(nullptr, false, TemplateField::Unknown());
    return original(header);
}

template <int Slot>
struct ComctlHooks {
    static inline PropertySheetFn propertySheet = nullptr;
    static inline CreatePageFn createPage = nullptr;

    static INT_PTR WINAPI PropertySheetHook(LPCPROPSHEETHEADERW header) {
        return PropertySheetCommon(propertySheet, header);
    }
    static HPROPSHEETPAGE WINAPI CreatePageHook(LPCPROPSHEETPAGEW page) {
        return CreatePageCommon(createPage, page);
    }
};

struct ComctlSlot {
    void* propertySheetHook;
    void** propertySheetOriginal;
    void* createPageHook;
    void** createPageOriginal;
};

template <int Slot>
ComctlSlot SlotOf() {
    using Hooks = ComctlHooks<Slot>;
    return ComctlSlot{reinterpret_cast<void*>(&Hooks::PropertySheetHook),
                      reinterpret_cast<void**>(&Hooks::propertySheet),
                      reinterpret_cast<void*>(&Hooks::CreatePageHook),
                      reinterpret_cast<void**>(&Hooks::createPage)};
}

struct ComctlState {
    SRWLOCK lock = SRWLOCK_INIT;
    HMODULE hooked[kComctlSlots] = {};
    int count = 0;
    bool fullLogged = false;
    wchar_t systemPath[MAX_PATH] = {};  // %windir%\system32\comctl32.dll (v5)
};
ComctlState g_comctl;

// Caller holds g_comctl.lock.
void HookComctl(HMODULE module, bool apply) {
    for (int i = 0; i < g_comctl.count; ++i) {
        if (g_comctl.hooked[i] == module) {
            return;
        }
    }
    if (g_comctl.count == kComctlSlots) {
        if (!g_comctl.fullLogged) {
            g_comctl.fullLogged = true;
            log::Warn(kArea, L"comctl32 at %p not hooked: more than %d instances", module,
                      kComctlSlots);
        }
        return;
    }
    static const ComctlSlot kSlots[kComctlSlots] = {SlotOf<0>(), SlotOf<1>(), SlotOf<2>(),
                                                    SlotOf<3>()};
    const ComctlSlot& slot = kSlots[g_comctl.count];
    g_comctl.hooked[g_comctl.count++] = module;  // tried once, even if a hook fails
    void* sheet = reinterpret_cast<void*>(GetProcAddress(module, "PropertySheetW"));
    void* page = reinterpret_cast<void*>(GetProcAddress(module, "CreatePropertySheetPageW"));
    bool sheetSet =
        sheet && Wh_SetFunctionHook(sheet, slot.propertySheetHook, slot.propertySheetOriginal);
    bool pageSet = page && Wh_SetFunctionHook(page, slot.createPageHook, slot.createPageOriginal);
    if (apply) {
        Wh_ApplyHookOperations();
    }
    if (sheetSet && pageSet) {
        log::Info(kArea, L"comctl32 at %p hooked", module);
    } else {
        log::Warn(kArea,
                  L"comctl32 at %p: hook failed (PropertySheetW %s, CreatePropertySheetPageW %s)",
                  module, sheetSet ? L"set" : L"failed", pageSet ? L"set" : L"failed");
    }
}

// Hooks the comctl32 instances loaded now and not hooked yet: the one the current activation
// context resolves (v6 in explorer.exe) and the system32 one (v5). Cheap: two loader lookups.
void HookComctlInstances(bool apply) noexcept {
    ThreadState& thread = t_state;
    if (thread.hookingComctl) {
        return;  // Wh_ApplyHookOperations loading a library
    }
    thread.hookingComctl = true;
    try {
        HMODULE candidates[] = {GetModuleHandleW(L"comctl32.dll"),
                                g_comctl.systemPath[0] ? GetModuleHandleW(g_comctl.systemPath)
                                                       : nullptr};
        ExclusiveLock lock(g_comctl.lock);
        for (HMODULE module : candidates) {
            if (module) {
                HookComctl(module, apply);
            }
        }
    } catch (...) {
    }
    thread.hookingComctl = false;
}

HMODULE WINAPI LoadLibraryExHook(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE module = g_loadLibraryEx(name, file, flags);
    constexpr DWORD kDataOnly = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
                                LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (module && !(flags & kDataOnly) && g_active.load(std::memory_order_relaxed)) {
        LastErrorGuard error;
        HookComctlInstances(true);
    }
    return module;
}

template <typename Fn>
void Hook(HMODULE module, const wchar_t* moduleName, const char* name, Fn hook, Fn* original) {
    void* target = module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    if (!target || !Wh_SetFunctionHook(target, reinterpret_cast<void*>(hook),
                                       reinterpret_cast<void**>(original))) {
        log::Warn(kArea, L"%s %S: hook failed; dialog facts from it stay unknown", moduleName,
                  name);
    }
}

}  // namespace

}  // namespace wholeui::compat::dialogs

namespace wholeui::compat {

void InstallDialogHooks() {
    using namespace dialogs;
    g_active.store(true);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    Hook(user32, L"user32", "CreateDialogParamW", &CreateDialogParamHook, &g_createDialogParam);
    Hook(user32, L"user32", "DialogBoxParamW", &DialogBoxParamHook, &g_dialogBoxParam);
    Hook(user32, L"user32", "CreateDialogIndirectParamW", &CreateDialogIndirectParamHook,
         &g_createDialogIndirectParam);
    Hook(user32, L"user32", "DialogBoxIndirectParamW", &DialogBoxIndirectParamHook,
         &g_dialogBoxIndirectParam);

    HMODULE kernelBase = GetModuleHandleW(L"kernelbase.dll");
    Hook(kernelBase, L"kernelbase", "FindResourceExW", &FindResourceExHook, &g_findResourceEx);
    Hook(kernelBase, L"kernelbase", "FindResourceW", &FindResourceHook, &g_findResource);
    Hook(kernelBase, L"kernelbase", "LoadLibraryExW", &LoadLibraryExHook, &g_loadLibraryEx);

    UINT length = GetSystemDirectoryW(g_comctl.systemPath, MAX_PATH);
    constexpr wchar_t kFile[] = L"\\comctl32.dll";
    if (length == 0 || length + std::size(kFile) > MAX_PATH) {
        g_comctl.systemPath[0] = L'\0';
    } else {
        wcscpy_s(g_comctl.systemPath + length, MAX_PATH - length, kFile);
    }
    HookComctlInstances(false);  // Windhawk applies these when Wh_ModInit returns
    if (g_comctl.count == 0) {
        log::Info(kArea, L"comctl32 not loaded yet; hooked when it loads");
    }
}

void UninstallDialogHooks() noexcept {
    using namespace dialogs;
    g_active.store(false);
    std::vector<HHOOK> hooks;
    {
        ExclusiveLock lock(g_hooks.lock);
        hooks.swap(g_hooks.hooks);
    }
    for (HHOOK hook : hooks) {
        UnhookWindowsHookEx(hook);
    }
    std::unordered_map<HWND, DialogFact> dialogs;
    std::unordered_map<HWND, SheetFact> sheets;
    std::unordered_map<HPROPSHEETPAGE, PageRecord> pageHandles;
    {
        ExclusiveLock lock(g_store.lock);
        dialogs.swap(g_store.dialogs);
        sheets.swap(g_store.sheets);
        pageHandles.swap(g_store.pageHandles);
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

SheetPages SheetPagesOf(HWND sheet) {
    using namespace dialogs;
    DWORD threadId = GetWindowThreadProcessId(sheet, nullptr);
    SheetPages result;
    SharedLock lock(g_store.lock);
    auto found = g_store.sheets.find(sheet);
    if (found == g_store.sheets.end() || found->second.threadId != threadId) {
        return result;
    }
    result.known = true;
    result.incomplete = found->second.incomplete;
    result.pages = found->second.pages;
    return result;
}

}  // namespace wholeui::compat
