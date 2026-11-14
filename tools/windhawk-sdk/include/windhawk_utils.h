#pragma once

#include <windhawk_api.h>

#include <commctrl.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <type_traits>

// Windhawk utilities that can be used by including this header file.
namespace WindhawkUtils {

struct SYMBOL_HOOK {
    /**
     * @brief A strong-type constructor.
     * @since Windhawk v1.4
     * @param symbols The list of symbol names to match. The first symbol that
     *     will match one of the names will be used.
     * @param originalFunction The pointer to the function pointer that will
     *     receive the symbol address. Can be `nullptr`.
     * @param hookFunction The hook function to set, or `nullptr` to only
     *     retrieve's the symbol's address.
     * @param optional If set to `true`, the absence of this symbol isn't
     *     considered an error. If the symbol isn't found, `originalFunction`
     *     will be left unchanged.
     */
    template <typename Prototype>
    SYMBOL_HOOK(std::initializer_list<std::wstring_view> symbols,
                Prototype** originalFunction,
                std::type_identity_t<Prototype*> hookFunction = nullptr,
                bool optional = false)
        : symbols(CopySymbols(symbols)),
          symbolsCount(symbols.size()),
          originalFunction(reinterpret_cast<void**>(originalFunction)),
          hookFunction(reinterpret_cast<void*>(hookFunction)),
          optional(optional) {}

    /**
     * @brief A strong-type constructor without `originalFunction`.
     * @since Windhawk v1.7.1
     */
    template <typename Prototype>
    SYMBOL_HOOK(std::initializer_list<std::wstring_view> symbols,
                nullptr_t,
                Prototype* hookFunction = nullptr,
                bool optional = false)
        : symbols(CopySymbols(symbols)),
          symbolsCount(symbols.size()),
          originalFunction(nullptr),
          hookFunction(reinterpret_cast<void*>(hookFunction)),
          optional(optional) {}

    /**
     * @brief A default constructor.
     * @since Windhawk v1.4.1
     */
    SYMBOL_HOOK() = default;

   private:
    struct SymbolString {
        std::unique_ptr<WCHAR[]> string;
        size_t length = 0;

        // We assume that `std::unique_ptr` is ABI-compatible with a pointer.
        static_assert(sizeof(string) == sizeof(string.get()));
    };

    std::unique_ptr<SymbolString[]> CopySymbols(
        std::initializer_list<std::wstring_view> symbols) {
        auto result = std::make_unique<SymbolString[]>(symbols.size());
        SymbolString* p = result.get();
        for (const auto& symbol : symbols) {
            p->string = std::make_unique<WCHAR[]>(symbol.size());
            std::copy(symbol.begin(), symbol.end(), p->string.get());
            p->length = symbol.size();
            p++;
        }
        return result;
    }

    std::unique_ptr<SymbolString[]> symbols;
    size_t symbolsCount = 0;
    void** originalFunction = nullptr;
    void* hookFunction = nullptr;
    bool optional = false;

    // We assume that `std::unique_ptr` is ABI-compatible with a pointer.
    static_assert(sizeof(symbols) == sizeof(symbols.get()));  // NOLINT
};

/**
 * @brief Finds module functions by their symbol names and optionally hooks
 *     them. Caches the result to avoid loading symbols each time. The Windhawk
 *     Symbol Helper tool can be useful for listing symbol names of a module.
 * @since Windhawk v1.3, `options` param since v1.5
 * @param module The module to look up.
 * @param symbolHooks The array of lookup entries.
 * @param symbolHooksCount The size of the array.
 * @param options Can be used to customize the symbol enumeration. Pass
 *     `nullptr` to use the default options.
 * @return A boolean value indicating whether the function succeeded.
 */
bool HookSymbols(HMODULE module,
                 const SYMBOL_HOOK* symbolHooks,
                 size_t symbolHooksCount,
                 const WH_HOOK_SYMBOLS_OPTIONS* options = nullptr);

using WH_SUBCLASSPROC = LRESULT(CALLBACK*)(HWND hWnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam,
                                           DWORD_PTR dwRefData);

/**
 * @brief Subclasses a window. Similar to `SetWindowSubclass`, but can be called
 *     from any thread, unlike `SetWindowSubclass` which can only be called in
 *     the thread of the target window.
 * @since Windhawk v1.3
 * @param hWnd The window to subclass.
 * @param pfnSubclass The subclass window procedure.
 * @param dwRefData Custom reference data. This value is passed to the subclass
 *     procedure in the `dwRefData` parameter.
 * @return A boolean value indicating whether the function succeeded.
 */
BOOL SetWindowSubclassFromAnyThread(HWND hWnd,
                                    WH_SUBCLASSPROC pfnSubclass,
                                    DWORD_PTR dwRefData);

/**
 * @brief Removes a subclass from a window.
 * @since Windhawk v1.3
 * @param hWnd The window being subclassed.
 * @param pfnSubclass The subclass window procedure.
 */
void RemoveWindowSubclassFromAnyThread(HWND hWnd, WH_SUBCLASSPROC pfnSubclass);

/**
 * @brief Removes all window subclasses that were set via
 *     `SetWindowSubclassFromAnyThread`.
 * @since Windhawk v2.0
 */
void RemoveAllWindowSubclasses();

/**
 * @brief A strong-type wrapper for `Wh_SetFunctionHook`. Registers a hook for
 *     the specified target function. Can't be called after `Wh_ModBeforeUninit`
 *     returns. Registered hook operations can be applied with
 *     `Wh_ApplyHookOperations`.
 * @since Windhawk v1.5
 * @param targetFunction A pointer to the target function, which will be
 *     overridden by the detour function.
 * @param hookFunction A pointer to the detour function, which will override the
 *     target function.
 * @param originalFunction A pointer to the trampoline function, which will be
 *     used to call the original target function. Can be `nullptr`.
 * @return A boolean value indicating whether the function succeeded.
 */
template <typename Prototype>
BOOL SetFunctionHook(Prototype* targetFunction,
                     Prototype* hookFunction,
                     Prototype** originalFunction) {
    return Wh_SetFunctionHook(reinterpret_cast<void*>(targetFunction),
                              reinterpret_cast<void*>(hookFunction),
                              reinterpret_cast<void**>(originalFunction));
}

/**
 * @brief A strong-type wrapper for `Wh_SetFunctionHook` without
 *     `originalFunction`.
 * @since Windhawk v1.7.1
 */
template <typename Prototype>
BOOL SetFunctionHook(Prototype* targetFunction,
                     Prototype* hookFunction,
                     nullptr_t) {
    return Wh_SetFunctionHook(reinterpret_cast<void*>(targetFunction),
                              reinterpret_cast<void*>(hookFunction), nullptr);
}

/**
 * @brief A deprecated alias for `SetFunctionHook`.
 * @since Windhawk v1.4
 */
template <typename Prototype>
[[deprecated("Use WindhawkUtils::SetFunctionHook() instead.")]]
BOOL Wh_SetFunctionHookT(Prototype* targetFunction,
                         Prototype* hookFunction,
                         Prototype** originalFunction) {
    return Wh_SetFunctionHook(reinterpret_cast<void*>(targetFunction),
                              reinterpret_cast<void*>(hookFunction),
                              reinterpret_cast<void**>(originalFunction));
}

/**
 * @brief A RAII wrapper for a string setting. Can be created with
 *     `auto string = StringSetting::make("SettingName")` or
 *     `auto string = StringSetting(Wh_GetStringSetting("SettingName"))`.
 *     `Wh_FreeStringSetting` is called upon destruction.
 * @since Windhawk v1.4
 */
class StringSetting {
   public:
    StringSetting() = default;
    explicit StringSetting(PCWSTR valueName) : m_stringSetting(valueName) {}
    operator PCWSTR() const { return m_stringSetting.get(); }
    PCWSTR get() const { return m_stringSetting.get(); }

    template <typename... Args>
    static StringSetting make(PCWSTR valueName, Args... args) {
        return StringSetting(Wh_GetStringSetting(valueName, args...));
    }

   private:
    // https://stackoverflow.com/a/51274008
    template <auto fn>
    struct deleter_from_fn {
        template <typename T>
        constexpr void operator()(T* arg) const {
            fn(arg);
        }
    };
    using string_setting_unique_ptr =
        std::unique_ptr<const WCHAR[], deleter_from_fn<Wh_FreeStringSetting>>;

    string_setting_unique_ptr m_stringSetting;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation

inline bool HookSymbols(HMODULE module,
                        const SYMBOL_HOOK* symbolHooks,
                        size_t symbolHooksCount,
                        const WH_HOOK_SYMBOLS_OPTIONS* options) {
#ifndef WH_EDITING
    // Both types are assumed to be ABI-compatible.
    static_assert(sizeof(*symbolHooks) == sizeof(WH_SYMBOL_HOOK));

    return InternalWh_HookSymbols_Wrapper(
        module, reinterpret_cast<const WH_SYMBOL_HOOK*>(symbolHooks),
        symbolHooksCount, options);
#else
    return false;
#endif
}

// Implementation details.
namespace detail {

// wParam - subclass operation type (see SubclassOp enum)
// lParam - if wParam is kSubclass, subclass data
// lParam - if wParam is kUnsubclass, the subclass window procedure
enum SubclassOp : WPARAM {
    kSubclass = 1,
    kUnsubclass,
};

inline UINT GetSubclassRegisteredMsg() {
    static UINT subclassRegisteredMsg = RegisterWindowMessage(
        L"Windhawk_SetWindowSubclassFromAnyThread_" WH_MOD_ID);
    return subclassRegisteredMsg;
}

inline constexpr size_t kSubclassedWindowsInitialCapacity = 16;

struct SubclassedWindow {
    HWND hWnd;
    WH_SUBCLASSPROC pfnSubclass;
};

struct SubclassedWindowsState {
    SRWLOCK lock = SRWLOCK_INIT;
    SubclassedWindow* windows = nullptr;
    size_t count = 0;
    size_t capacity = 0;

    SubclassedWindowsState() = default;
    ~SubclassedWindowsState() {
        for (size_t i = 0; i < count; i++) {
            SendMessage(windows[i].hWnd, GetSubclassRegisteredMsg(),
                        SubclassOp::kUnsubclass,
                        (LPARAM)windows[i].pfnSubclass);
        }
        if (windows) {
            HeapFree(GetProcessHeap(), 0, windows);
        }
    }

    SubclassedWindowsState(const SubclassedWindowsState&) = delete;
    SubclassedWindowsState& operator=(const SubclassedWindowsState&) = delete;
    SubclassedWindowsState(SubclassedWindowsState&&) = delete;
    SubclassedWindowsState& operator=(SubclassedWindowsState&&) = delete;
};

inline SubclassedWindowsState& GetSubclassedWindowsState() {
    static SubclassedWindowsState state;
    return state;
}

inline void TrackSubclassedWindow(HWND hWnd, WH_SUBCLASSPROC pfnSubclass) {
    auto& state = GetSubclassedWindowsState();
    AcquireSRWLockExclusive(&state.lock);

    if (state.count >= state.capacity) {
        size_t newCapacity = state.capacity == 0
                                 ? kSubclassedWindowsInitialCapacity
                                 : state.capacity * 2;
        SubclassedWindow* newWindows;
        if (!state.windows) {
            newWindows = static_cast<SubclassedWindow*>(HeapAlloc(
                GetProcessHeap(), 0, newCapacity * sizeof(SubclassedWindow)));
        } else {
            newWindows = static_cast<SubclassedWindow*>(
                HeapReAlloc(GetProcessHeap(), 0, state.windows,
                            newCapacity * sizeof(SubclassedWindow)));
        }
        if (newWindows) {
            state.windows = newWindows;
            state.capacity = newCapacity;
        }
    }

    if (state.count < state.capacity) {
        state.windows[state.count].hWnd = hWnd;
        state.windows[state.count].pfnSubclass = pfnSubclass;
        state.count++;
    }

    ReleaseSRWLockExclusive(&state.lock);
}

inline void UntrackSubclassedWindow(HWND hWnd, WH_SUBCLASSPROC pfnSubclass) {
    auto& state = GetSubclassedWindowsState();
    AcquireSRWLockExclusive(&state.lock);

    for (size_t i = 0; i < state.count; i++) {
        if (state.windows[i].hWnd == hWnd &&
            state.windows[i].pfnSubclass == pfnSubclass) {
            // Move last element to fill the gap.
            state.windows[i] = state.windows[state.count - 1];
            state.count--;
            break;
        }
    }

    // Free or compact the array based on usage.
    if (state.count == 0) {
        if (state.windows) {
            HeapFree(GetProcessHeap(), 0, state.windows);
            state.windows = nullptr;
            state.capacity = 0;
        }
    } else if (state.count < state.capacity / 2 &&
               state.capacity > kSubclassedWindowsInitialCapacity) {
        size_t newCapacity = state.capacity / 2;
        SubclassedWindow* newWindows = static_cast<SubclassedWindow*>(
            HeapReAlloc(GetProcessHeap(), 0, state.windows,
                        newCapacity * sizeof(SubclassedWindow)));
        if (newWindows) {
            state.windows = newWindows;
            state.capacity = newCapacity;
        }
    }

    ReleaseSRWLockExclusive(&state.lock);
}

inline LRESULT WINAPI SubclassProcWrapper(HWND hWnd,
                                          UINT uMsg,
                                          WPARAM wParam,
                                          LPARAM lParam,
                                          UINT_PTR uIdSubclass,
                                          DWORD_PTR dwRefData) {
    // Turns out we must remove the window subclass before the window being
    // subclassed is destroyed.
    // https://devblogs.microsoft.com/oldnewthing/20031111-00/?p=41883
    if (uMsg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, SubclassProcWrapper, uIdSubclass);
        UntrackSubclassedWindow(hWnd, (WH_SUBCLASSPROC)uIdSubclass);
    } else if (uMsg == GetSubclassRegisteredMsg() &&
               wParam == SubclassOp::kUnsubclass &&
               (UINT_PTR)lParam == uIdSubclass) {
        RemoveWindowSubclass(hWnd, SubclassProcWrapper, uIdSubclass);
    }

    auto pfnSubclass = (WH_SUBCLASSPROC)uIdSubclass;
    return pfnSubclass(hWnd, uMsg, wParam, lParam, dwRefData);
}

struct SET_WINDOW_SUBCLASS_FROM_ANY_THREAD_PARAM {
    SUBCLASSPROC pfnSubclass;
    UINT_PTR uIdSubclass;
    DWORD_PTR dwRefData;
    BOOL result;
};

inline LRESULT WINAPI CallWndProcToSetWindowSubclass(int nCode,
                                                     WPARAM wParam,
                                                     LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const CWPSTRUCT* cwp = (const CWPSTRUCT*)lParam;
        if (cwp->message == GetSubclassRegisteredMsg() &&
            cwp->wParam == SubclassOp::kSubclass) {
            SET_WINDOW_SUBCLASS_FROM_ANY_THREAD_PARAM* param =
                (SET_WINDOW_SUBCLASS_FROM_ANY_THREAD_PARAM*)cwp->lParam;
            param->result =
                SetWindowSubclass(cwp->hwnd, param->pfnSubclass,
                                  param->uIdSubclass, param->dwRefData);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

}  // namespace detail

inline BOOL SetWindowSubclassFromAnyThread(HWND hWnd,
                                           WH_SUBCLASSPROC pfnSubclass,
                                           DWORD_PTR dwRefData) {
    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (dwThreadId == 0) {
        return FALSE;
    }

    if (dwThreadId == GetCurrentThreadId()) {
        BOOL result = SetWindowSubclass(hWnd, detail::SubclassProcWrapper,
                                        (UINT_PTR)pfnSubclass, dwRefData);
        if (result) {
            detail::TrackSubclassedWindow(hWnd, pfnSubclass);
        }
        return result;
    }

    HHOOK hook =
        SetWindowsHookEx(WH_CALLWNDPROC, detail::CallWndProcToSetWindowSubclass,
                         nullptr, dwThreadId);
    if (!hook) {
        return FALSE;
    }

    detail::SET_WINDOW_SUBCLASS_FROM_ANY_THREAD_PARAM param{
        .pfnSubclass = detail::SubclassProcWrapper,
        .uIdSubclass = (UINT_PTR)pfnSubclass,
        .dwRefData = dwRefData,
        .result = FALSE,
    };

    SendMessage(hWnd, detail::GetSubclassRegisteredMsg(),
                detail::SubclassOp::kSubclass, (WPARAM)&param);

    UnhookWindowsHookEx(hook);

    if (param.result) {
        detail::TrackSubclassedWindow(hWnd, pfnSubclass);
    }

    return param.result;
}

inline void RemoveWindowSubclassFromAnyThread(HWND hWnd,
                                              WH_SUBCLASSPROC pfnSubclass) {
    SendMessage(hWnd, detail::GetSubclassRegisteredMsg(),
                detail::SubclassOp::kUnsubclass, (LPARAM)pfnSubclass);
    detail::UntrackSubclassedWindow(hWnd, pfnSubclass);
}

inline void RemoveAllWindowSubclasses() {
    auto& state = detail::GetSubclassedWindowsState();
    AcquireSRWLockExclusive(&state.lock);

    // Move state to local variables
    detail::SubclassedWindow* windows = state.windows;
    size_t count = state.count;

    // Reset state
    state.windows = nullptr;
    state.count = 0;
    state.capacity = 0;

    ReleaseSRWLockExclusive(&state.lock);

    // Send unsubclass messages outside the lock
    if (windows) {
        for (size_t i = 0; i < count; i++) {
            SendMessage(windows[i].hWnd, detail::GetSubclassRegisteredMsg(),
                        detail::SubclassOp::kUnsubclass,
                        (LPARAM)windows[i].pfnSubclass);
        }
        HeapFree(GetProcessHeap(), 0, windows);
    }
}

}  // namespace WindhawkUtils
