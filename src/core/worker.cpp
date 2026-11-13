#include "worker.h"

#include <windows.h>
#include <roapi.h>

#include <atomic>
#include <deque>
#include <utility>

#include "lock.h"
#include "log.h"

namespace wholeui::worker {
namespace {

// Posted to the worker's message-only window. A message-only window, not a thread message,
// so a modal loop running on the worker thread (a window it owns) does not drop the message.
constexpr UINT kWakeMessage = WM_APP + 1;  // run the queued tasks
constexpr UINT kQuitMessage = WM_APP + 2;  // destroy the window and leave the loop

constexpr wchar_t kWindowClass[] = L"WholeUI.Worker";

struct State {
    SRWLOCK lock = SRWLOCK_INIT;
    HANDLE thread = nullptr;     // set while the worker runs; Stop takes it
    DWORD threadId = 0;
    HWND window = nullptr;       // null until the thread is ready and after it is destroyed
    bool accepting = false;      // Post queues tasks
    bool stopRequested = false;  // Stop ran before the thread was ready
    bool wakePending = false;    // a wake message is in the window's queue
    std::deque<std::function<void()>> tasks;
};

State g_state;
std::atomic<DWORD> g_workerThreadId{0};

// Runs every queued task, including tasks the tasks post, until the queue is empty.
void RunTasks() noexcept {
    for (;;) {
        std::deque<std::function<void()>> batch;
        {
            ExclusiveLock guard(g_state.lock);
            batch.swap(g_state.tasks);
            g_state.wakePending = false;
        }
        if (batch.empty()) {
            return;
        }
        for (std::function<void()>& task : batch) {
            try {
                task();
            } catch (...) {
                log::Warn(L"worker", L"a task threw an exception; skipped");
            }
        }
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case kWakeMessage:
            RunTasks();
            return 0;
        case kQuitMessage:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY: {
            {
                ExclusiveLock guard(g_state.lock);
                g_state.window = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

HINSTANCE ModuleInstance() noexcept {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&WindowProc), &module);
    return module;
}

// Called on the worker thread when it cannot run its loop: Post stops queuing and the tasks
// already queued are dropped (they need the thread's WinRT apartment).
void Abandon(const wchar_t* why) noexcept {
    size_t dropped = 0;
    {
        ExclusiveLock guard(g_state.lock);
        g_state.accepting = false;
        dropped = g_state.tasks.size();
        g_state.tasks.clear();
    }
    log::Warn(L"worker", L"%s; worker not running, %u queued tasks dropped", why,
              static_cast<unsigned>(dropped));
}

DWORD WINAPI ThreadProc(void*) {
    g_workerThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);

    HRESULT initResult = RoInitialize(RO_INIT_SINGLETHREADED);
    bool roInitialized = SUCCEEDED(initResult);
    if (!roInitialized) {
        // Tasks still run; a task that needs WinRT fails and falls back on its own.
        log::Warn(L"worker", L"RoInitialize failed: 0x%08X", static_cast<unsigned>(initResult));
    }

    HINSTANCE instance = ModuleInstance();
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    bool registered = RegisterClassExW(&windowClass) != 0;
    if (!registered && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Abandon(L"RegisterClassEx failed");
        if (roInitialized) {
            RoUninitialize();
        }
        return 1;
    }

    HWND window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                  instance, nullptr);
    if (window == nullptr) {
        Abandon(L"CreateWindowEx failed");
    } else {
        bool stop = false;
        {
            ExclusiveLock guard(g_state.lock);
            g_state.window = window;
            stop = g_state.stopRequested;
        }
        log::Verbose(L"worker", L"started (thread %lu)", GetCurrentThreadId());
        RunTasks();  // posted before the window existed
        if (stop) {
            DestroyWindow(window);
        }
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        // Normally Stop already closed the queue. If a task left the loop on its own, close it
        // here so later Posts fail instead of queuing tasks that never run.
        bool expected = true;
        {
            ExclusiveLock guard(g_state.lock);
            expected = g_state.stopRequested;
            g_state.accepting = false;
        }
        if (!expected) {
            log::Warn(L"worker", L"message loop ended before Stop; worker not running");
        }
        // Tasks queued after the last wake (their wake message reached a destroyed window).
        RunTasks();
        if (IsWindow(window)) {
            DestroyWindow(window);
        }
    }

    UnregisterClassW(kWindowClass, instance);
    if (roInitialized) {
        RoUninitialize();
    }
    log::Verbose(L"worker", L"stopped");
    return 0;
}

}  // namespace

bool Start() noexcept {
    ExclusiveLock guard(g_state.lock);
    if (g_state.thread != nullptr) {
        return true;
    }
    g_state.accepting = true;
    g_state.stopRequested = false;
    g_state.wakePending = false;
    g_state.window = nullptr;
    DWORD threadId = 0;
    // The thread waits on the lock held here before it publishes its window, which is fine:
    // the lock is released when Start returns.
    HANDLE thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, &threadId);
    if (thread == nullptr) {
        g_state.accepting = false;
        log::Warn(L"worker", L"CreateThread failed: error %lu; worker not running",
                  GetLastError());
        return false;
    }
    g_state.thread = thread;
    g_state.threadId = threadId;
    return true;
}

void Stop() noexcept {
    HANDLE thread = nullptr;
    HWND window = nullptr;
    DWORD threadId = 0;
    {
        ExclusiveLock guard(g_state.lock);
        if (g_state.thread == nullptr) {
            return;
        }
        if (g_state.threadId == GetCurrentThreadId()) {
            log::Warn(L"worker", L"Stop called on the worker thread; ignored");
            return;
        }
        thread = g_state.thread;
        threadId = g_state.threadId;
        g_state.thread = nullptr;
        g_state.accepting = false;
        g_state.stopRequested = true;
        window = g_state.window;
    }

    // Without a window the thread is still starting (it sees stopRequested when it is ready)
    // or already gave up.
    if (window != nullptr && !PostMessageW(window, kQuitMessage, 0, 0)) {
        // The window's queue is full or the window is gone: leave the loop directly. The
        // thread runs the remaining tasks after its loop.
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    ExclusiveLock guard(g_state.lock);
    g_state.threadId = 0;
    g_state.window = nullptr;
    g_state.stopRequested = false;
    g_state.tasks.clear();  // empty unless the thread failed before its loop
    g_workerThreadId.store(0, std::memory_order_relaxed);
}

bool Post(std::function<void()> task) noexcept {
    HWND wake = nullptr;
    try {
        ExclusiveLock guard(g_state.lock);
        if (!g_state.accepting) {
            return false;
        }
        g_state.tasks.push_back(std::move(task));
        if (g_state.window != nullptr && !g_state.wakePending) {
            g_state.wakePending = true;
            wake = g_state.window;
        }
    } catch (...) {
        log::Warn(L"worker", L"Post failed: out of memory; task dropped");
        return false;
    }
    if (wake != nullptr && !PostMessageW(wake, kWakeMessage, 0, 0)) {
        // The task stays queued: the next successful wake, or the drain at Stop, runs it.
        ExclusiveLock guard(g_state.lock);
        g_state.wakePending = false;
    }
    return true;
}

bool IsWorkerThread() noexcept {
    DWORD id = g_workerThreadId.load(std::memory_order_relaxed);
    return id != 0 && id == GetCurrentThreadId();
}

}  // namespace wholeui::worker
