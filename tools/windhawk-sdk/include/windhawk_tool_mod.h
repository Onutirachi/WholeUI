#pragma once

#include <windhawk_api.h>

// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. More details:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated process.
//
// Include this file and use the following callbacks in the mod implementation:
// BOOL WhTool_ModInit();
// void WhTool_ModUninit();
// void WhTool_ModSettingsChanged();
// void WhTool_ModEntryPoint();
//
// The mod is also loaded into the processes it's configured to run in, where it
// only serves as a launcher for the dedicated process. Code which runs in these
// processes can be added with the following callbacks:
// BOOL WhToolLauncher_ModInit(BOOL* bLaunch);
// void WhToolLauncher_ModAfterInit();
// void WhToolLauncher_ModBeforeUninit();
// void WhToolLauncher_ModUninit();
// void WhToolLauncher_ModSettingsChanged(BOOL* bReload);
//
// Return FALSE from WhToolLauncher_ModInit to unload the mod from the process.
// The out parameters hold the default behavior and can be overridden: bLaunch
// launches the dedicated process, bReload reloads the mod, which launches the
// dedicated process if it isn't running.

#ifndef WH_EDITING

// Mark the fact that it's a tool mod in the export table.
inline __declspec(dllexport) bool InternalWhToolModMarker = true;

namespace windhawk_tool_mod {

inline HMODULE GetCurrentModuleHandle() {
    static HMODULE module = []() {
        HMODULE handle;
        if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               L"", &handle)) {
            return (HMODULE) nullptr;
        }
        return handle;
    }();
    return module;
}

inline BOOL CallModInit() {
    using WH_MOD_INIT_T = BOOL(__cdecl*)();
    static auto modInit = reinterpret_cast<WH_MOD_INIT_T>(
        GetProcAddress(GetCurrentModuleHandle(), "_Z14WhTool_ModInitv"));
    if (modInit) {
        return modInit();
    }
    return TRUE;
}

inline void CallModUninit() {
    using WH_MOD_UNINIT_T = void(__cdecl*)();
    static auto modUninit = reinterpret_cast<WH_MOD_UNINIT_T>(
        GetProcAddress(GetCurrentModuleHandle(), "_Z16WhTool_ModUninitv"));
    if (modUninit) {
        modUninit();
    }
}

inline void CallModSettingsChanged() {
    using WH_MOD_SETTINGS_CHANGED_T = void(__cdecl*)();
    static auto modSettingsChanged =
        reinterpret_cast<WH_MOD_SETTINGS_CHANGED_T>(GetProcAddress(
            GetCurrentModuleHandle(), "_Z25WhTool_ModSettingsChangedv"));
    if (modSettingsChanged) {
        modSettingsChanged();
    }
}

inline void CallModEntryPoint() {
    using WH_MOD_ENTRY_POINT_T = void(__cdecl*)();
    static auto modEntryPoint = reinterpret_cast<WH_MOD_ENTRY_POINT_T>(
        GetProcAddress(GetCurrentModuleHandle(), "_Z20WhTool_ModEntryPointv"));
    if (modEntryPoint) {
        modEntryPoint();
        ExitProcess(0);
    }
}

inline BOOL CallLauncherModInit(BOOL* bLaunch) {
    using WH_MOD_INIT_T = BOOL(__cdecl*)(BOOL*);
    static auto modInit = reinterpret_cast<WH_MOD_INIT_T>(GetProcAddress(
        GetCurrentModuleHandle(), "_Z22WhToolLauncher_ModInitPi"));
    if (modInit) {
        return modInit(bLaunch);
    }
    return TRUE;
}

inline void CallLauncherModAfterInit() {
    using WH_MOD_AFTER_INIT_T = void(__cdecl*)();
    static auto modAfterInit =
        reinterpret_cast<WH_MOD_AFTER_INIT_T>(GetProcAddress(
            GetCurrentModuleHandle(), "_Z27WhToolLauncher_ModAfterInitv"));
    if (modAfterInit) {
        modAfterInit();
    }
}

inline void CallLauncherModBeforeUninit() {
    using WH_MOD_BEFORE_UNINIT_T = void(__cdecl*)();
    static auto modBeforeUninit =
        reinterpret_cast<WH_MOD_BEFORE_UNINIT_T>(GetProcAddress(
            GetCurrentModuleHandle(), "_Z30WhToolLauncher_ModBeforeUninitv"));
    if (modBeforeUninit) {
        modBeforeUninit();
    }
}

inline void CallLauncherModUninit() {
    using WH_MOD_UNINIT_T = void(__cdecl*)();
    static auto modUninit = reinterpret_cast<WH_MOD_UNINIT_T>(GetProcAddress(
        GetCurrentModuleHandle(), "_Z24WhToolLauncher_ModUninitv"));
    if (modUninit) {
        modUninit();
    }
}

inline void CallLauncherModSettingsChanged(BOOL* bReload) {
    using WH_MOD_SETTINGS_CHANGED_T = void(__cdecl*)(BOOL*);
    static auto modSettingsChanged =
        reinterpret_cast<WH_MOD_SETTINGS_CHANGED_T>(
            GetProcAddress(GetCurrentModuleHandle(),
                           "_Z33WhToolLauncher_ModSettingsChangedPi"));
    if (modSettingsChanged) {
        modSettingsChanged(bReload);
    }
}

inline bool g_isToolModProcessLauncher;
inline bool g_launchToolModProcess = true;

inline void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    CallModEntryPoint();
    ExitThread(0);
}

inline BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return FALSE;
    }

    PCWSTR currentProcessName = wcsrchr(currentProcessPath, L'\\');
    if (currentProcessName) {
        currentProcessName++;
    } else {
        currentProcessName = currentProcessPath;
    }

    bool isWindhawkProcess = _wcsicmp(currentProcessName, L"windhawk.exe") == 0;

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    if (isWindhawkProcess) {
        const PCWSTR excludeFlags[] = {
            L"-service",
            L"-service-start",
            L"-service-stop",
            L"-run-ui",
            L"-run-ui-as-admin",
            L"-run-ui-in-safe-mode",
            L"-service-start-and-run-ui",
            L"-check-for-updates",
            L"-new-updates-found",
            L"-app-settings-changed",
            L"-exit",
            L"-restart",
            L"-restart-bg",
        };

        for (int i = 1; !isExcluded && i < argc; i++) {
            // Exclude flags starting with "-x-" for forward compatibility with
            // potential future flags.
            if (wcsncmp(argv[i], L"-x-", 3) == 0) {
                isExcluded = true;
                break;
            }
            for (const auto& flag : excludeFlags) {
                if (wcscmp(argv[i], flag) == 0) {
                    isExcluded = true;
                    break;
                }
            }
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-windhawk-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        // Allows Windhawk to check whether a windhawk.exe process is a tool mod
        // process.
        WCHAR pidMutexName[sizeof("Global\\windhawk-tool-mod-pid=1234567890")];
        wsprintf(pidMutexName, L"Global\\windhawk-tool-mod-pid=%u",
                 GetCurrentProcessId());
        CreateMutex(nullptr, FALSE, pidMutexName);

        constexpr int kMaxToolModProcessMutexAttempts = 4;
        for (int i = 0; i < kMaxToolModProcessMutexAttempts; i++) {
            HANDLE toolModProcessMutex =
                CreateMutex(nullptr, FALSE, L"windhawk-tool-mod_" WH_MOD_ID);
            if (!toolModProcessMutex) {
                Wh_Log(L"CreateMutex failed");
                ExitProcess(1);
            }

            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                break;
            }
            CloseHandle(toolModProcessMutex);
            toolModProcessMutex = nullptr;

            if (i == kMaxToolModProcessMutexAttempts - 1) {
                Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
                ExitProcess(1);
            }

            Sleep(100);
        }

        IMAGE_DOS_HEADER* dosHeader =
            (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* ntHeaders =
            (IMAGE_NT_HEADERS*)((BYTE*)dosHeader + dosHeader->e_lfanew);

        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = (BYTE*)dosHeader + entryPointRVA;

        if (!Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr)) {
            ExitProcess(1);
        }

        if (!CallModInit()) {
            ExitProcess(1);
        }

        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    g_isToolModProcessLauncher = true;

    BOOL launch = TRUE;
    if (!CallLauncherModInit(&launch)) {
        return FALSE;
    }

    g_launchToolModProcess = launch;

    return TRUE;
}

inline void LaunchToolModProcess() {
    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    constexpr size_t kCommandLineMaxSize =
        MAX_PATH + 2 +
        (sizeof(L" -windhawk-tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1;
    WCHAR commandLine[kCommandLineMaxSize];
    wsprintf(commandLine, L"\"%s\" -windhawk-tool-mod \"%s\"",
             currentProcessPath, WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si{
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_FORCEOFFFEEDBACK,
    };
    PROCESS_INFORMATION pi;
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

inline void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    if (g_launchToolModProcess) {
        LaunchToolModProcess();
    }

    CallLauncherModAfterInit();
}

inline void Wh_ModBeforeUninit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    CallLauncherModBeforeUninit();
}

inline void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) {
        CallLauncherModUninit();
        return;
    }

    CallModUninit();
    ExitProcess(0);
}

inline void Wh_ModSettingsChanged(BOOL* bReload) {
    if (g_isToolModProcessLauncher) {
        // Reloading launches a new instance of the dedicated process if it
        // isn't running.
        BOOL reload = TRUE;
        CallLauncherModSettingsChanged(&reload);
        if (bReload) {
            *bReload = reload;
        }
        return;
    }

    CallModSettingsChanged();
}

}  // namespace windhawk_tool_mod

#define WH_INTERNAL(x) (x)
#define WH_INTERNAL_OR(x, y) (x)
#else
#define WH_INTERNAL(x)
#define WH_INTERNAL_OR(x, y) (y)
#endif

// Use `WhTool_ModInit` or `WhToolLauncher_ModInit` for a Windhawk tool mod.
__declspec(dllexport) inline void Wh_ModInit() {
    WH_INTERNAL(windhawk_tool_mod::Wh_ModInit());
}

// Use `WhToolLauncher_ModAfterInit` for a Windhawk tool mod.
// `WhTool_ModAfterInit` is unsupported in tool mods.
__declspec(dllexport) inline void Wh_ModAfterInit() {
    WH_INTERNAL(windhawk_tool_mod::Wh_ModAfterInit());
}

// Use `WhToolLauncher_ModBeforeUninit` for a Windhawk tool mod.
// `WhTool_ModBeforeUninit` is unsupported in tool mods.
__declspec(dllexport) inline void Wh_ModBeforeUninit() {
    WH_INTERNAL(windhawk_tool_mod::Wh_ModBeforeUninit());
}

// Use `WhTool_ModUninit` or `WhToolLauncher_ModUninit` for a Windhawk tool mod.
__declspec(dllexport) inline void Wh_ModUninit() {
    WH_INTERNAL(windhawk_tool_mod::Wh_ModUninit());
}

// Use `WhTool_ModSettingsChanged` or `WhToolLauncher_ModSettingsChanged` for a
// Windhawk tool mod.
__declspec(dllexport) inline void Wh_ModSettingsChanged() {
    WH_INTERNAL(windhawk_tool_mod::Wh_ModSettingsChanged(nullptr));
}

// `Wh_ModSettingsChanged(BOOL* bReload)` is unsupported in tool mods.
__declspec(dllexport) inline BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    WH_INTERNAL(windhawk_tool_mod::Wh_ModSettingsChanged(bReload));
    return TRUE;
}
