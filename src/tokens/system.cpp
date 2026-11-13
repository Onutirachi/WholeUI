#include "system.h"

#include <windows.h>
#include <dwrite.h>
#include <roapi.h>
#include <winstring.h>
// In mingw-w64, `boolean` is `unsigned char`, so the public llvm-mingw windows.foundation.h
// defines IReference<BYTE> twice (as BYTE and as boolean) and fails to compile. Windhawk's
// copy of the header drops the boolean block; marking it as already defined does the same
// with the pinned toolchain CI uses (research R1).
#define ____FIReference_1_boolean_FWD_DEFINED__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.ui.viewmanagement.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <new>

#include "../core/log.h"
#include "../core/worker.h"

namespace wholeui::tokens::system {
namespace {

namespace vm = ABI::Windows::UI::ViewManagement;
using ChangedHandler = ABI::Windows::Foundation::ITypedEventHandler<vm::UISettings*, IInspectable*>;

constexpr wchar_t kVariableTextFamily[] = L"Segoe UI Variable Text";
constexpr double kMinTextScale = 1.0;
constexpr double kMaxTextScale = 2.25;

// How long the release task waits for UISettings to drop its references to the handler
// (an event already being delivered on another thread holds one until it returns).
constexpr int kHandlerReleaseWaitMs = 1000;
constexpr int kHandlerReleasePollMs = 10;

// Owns one COM reference.
template <typename T>
class ComRef {
public:
    ComRef() = default;
    ComRef(const ComRef&) = delete;
    ComRef& operator=(const ComRef&) = delete;
    ~ComRef() { Reset(); }

    T* Get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T** Put() noexcept {
        Reset();
        return &ptr_;
    }
    void** PutVoid() noexcept { return reinterpret_cast<void**>(Put()); }
    void Reset() noexcept {
        if (ptr_ != nullptr) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

// Handler objects alive, so the release task can wait until none is left before the DLL
// unloads.
std::atomic<int> g_liveHandlers{0};

// True while a re-read task is queued on the worker; several events in a row (Windows sends
// ColorValuesChanged more than once per change) cost one read.
std::atomic<bool> g_rereadPending{false};

void Reread() noexcept;

// Receives ColorValuesChanged and TextScaleFactorChanged. UISettings raises them on a thread
// of its own, so the handler is agile and only posts a re-read to the worker.
class ChangeHandler final : public ChangedHandler, public IAgileObject {
public:
    ChangeHandler() noexcept { g_liveHandlers.fetch_add(1, std::memory_order_relaxed); }
    ChangeHandler(const ChangeHandler&) = delete;
    ChangeHandler& operator=(const ChangeHandler&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (IsEqualIID(iid, __uuidof(IUnknown)) || IsEqualIID(iid, __uuidof(ChangedHandler))) {
            *object = static_cast<ChangedHandler*>(this);
        } else if (IsEqualIID(iid, __uuidof(IAgileObject))) {
            *object = static_cast<IAgileObject*>(this);
        } else {
            *object = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return refs_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG left = refs_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (left == 0) {
            delete this;
        }
        return left;
    }

    HRESULT STDMETHODCALLTYPE Invoke(vm::IUISettings*, IInspectable*) override {
        if (!g_rereadPending.exchange(true, std::memory_order_acq_rel)) {
            // Post fails once the worker is stopping; the release task is then on its way.
            if (!worker::Post(Reread)) {
                g_rereadPending.store(false, std::memory_order_release);
            }
        }
        return S_OK;
    }

private:
    ~ChangeHandler() { g_liveHandlers.fetch_sub(1, std::memory_order_release); }

    std::atomic<ULONG> refs_{1};
};

// Worker-thread state. Only the worker thread touches it.
struct State {
    bool started = false;
    InputsFn onInputs = nullptr;
    void* context = nullptr;
    ComRef<vm::IUISettings2> settings2;  // text scale
    ComRef<vm::IUISettings3> settings3;  // colors
    ComRef<ChangeHandler> handler;
    EventRegistrationToken colorToken = {};
    EventRegistrationToken textScaleToken = {};
    bool colorRegistered = false;
    bool textScaleRegistered = false;
};

State g_state;

std::uint32_t ToArgb(const ABI::Windows::UI::Color& color) noexcept {
    return (static_cast<std::uint32_t>(color.A) << 24) |
           (static_cast<std::uint32_t>(color.R) << 16) |
           (static_cast<std::uint32_t>(color.G) << 8) | static_cast<std::uint32_t>(color.B);
}

// Microsoft's documented test for a light color ("Support dark and light themes in Win32
// apps"): the app mode is dark when the background color is not light.
bool IsColorLight(const ABI::Windows::UI::Color& color) noexcept {
    return 5 * color.G + 2 * color.R + color.B > 8 * 128;
}

void CreateSettings() noexcept {
    HSTRING_HEADER header;
    HSTRING className = nullptr;
    HRESULT hr = WindowsCreateStringReference(
        RuntimeClass_Windows_UI_ViewManagement_UISettings,
        static_cast<UINT32>(std::size(RuntimeClass_Windows_UI_ViewManagement_UISettings) - 1),
        &header, &className);
    ComRef<IActivationFactory> factory;
    if (SUCCEEDED(hr)) {
        hr = RoGetActivationFactory(className, __uuidof(IActivationFactory), factory.PutVoid());
    }
    ComRef<IInspectable> instance;
    if (SUCCEEDED(hr)) {
        hr = factory->ActivateInstance(instance.Put());
    }
    if (FAILED(hr)) {
        log::Warn(L"tokens", L"creating UISettings failed: 0x%08X; mode, accent and text scale "
                  L"use their defaults", static_cast<unsigned>(hr));
        return;
    }
    hr = instance->QueryInterface(__uuidof(vm::IUISettings3), g_state.settings3.PutVoid());
    if (FAILED(hr)) {
        log::Warn(L"tokens", L"UISettings has no IUISettings3: 0x%08X; mode and accent use "
                  L"their defaults", static_cast<unsigned>(hr));
    }
    hr = instance->QueryInterface(__uuidof(vm::IUISettings2), g_state.settings2.PutVoid());
    if (FAILED(hr)) {
        log::Warn(L"tokens", L"UISettings has no IUISettings2: 0x%08X; text scale uses its "
                  L"default", static_cast<unsigned>(hr));
    }
}

void Register() noexcept {
    ChangeHandler* handler = new (std::nothrow) ChangeHandler();
    if (handler == nullptr) {
        log::Warn(L"tokens", L"creating the change handler failed: out of memory; system "
                  L"changes are not followed");
        return;
    }
    *g_state.handler.Put() = handler;
    if (g_state.settings3) {
        HRESULT hr = g_state.settings3->add_ColorValuesChanged(handler, &g_state.colorToken);
        g_state.colorRegistered = SUCCEEDED(hr);
        if (FAILED(hr)) {
            log::Warn(L"tokens", L"registering ColorValuesChanged failed: 0x%08X; mode and "
                      L"accent changes are not followed", static_cast<unsigned>(hr));
        }
    }
    if (g_state.settings2) {
        HRESULT hr =
            g_state.settings2->add_TextScaleFactorChanged(handler, &g_state.textScaleToken);
        g_state.textScaleRegistered = SUCCEEDED(hr);
        if (FAILED(hr)) {
            log::Warn(L"tokens", L"registering TextScaleFactorChanged failed: 0x%08X; text "
                      L"scale changes are not followed", static_cast<unsigned>(hr));
        }
    }
}

void ReadColors(TokenInputs& inputs) noexcept {
    if (!g_state.settings3) {
        return;  // already logged when UISettings was created
    }
    vm::IUISettings3* settings = g_state.settings3.Get();

    ABI::Windows::UI::Color background = {};
    HRESULT hr = settings->GetColorValue(vm::UIColorType_Background, &background);
    if (SUCCEEDED(hr)) {
        inputs.dark = !IsColorLight(background);
    } else {
        log::Warn(L"tokens", L"reading the background color failed: 0x%08X; light mode",
                  static_cast<unsigned>(hr));
    }

    struct Shade {
        vm::UIColorType type;
        std::uint32_t TokenInputs::*field;
    };
    static constexpr Shade kShades[] = {
        {vm::UIColorType_Accent, &TokenInputs::accent},
        {vm::UIColorType_AccentLight1, &TokenInputs::accentLight1},
        {vm::UIColorType_AccentLight2, &TokenInputs::accentLight2},
        {vm::UIColorType_AccentLight3, &TokenInputs::accentLight3},
        {vm::UIColorType_AccentDark1, &TokenInputs::accentDark1},
        {vm::UIColorType_AccentDark2, &TokenInputs::accentDark2},
        {vm::UIColorType_AccentDark3, &TokenInputs::accentDark3},
    };
    TokenInputs read = inputs;
    for (const Shade& shade : kShades) {
        ABI::Windows::UI::Color color = {};
        hr = settings->GetColorValue(shade.type, &color);
        if (FAILED(hr)) {
            log::Warn(L"tokens", L"reading accent color %d failed: 0x%08X; default accent",
                      static_cast<int>(shade.type), static_cast<unsigned>(hr));
            return;  // `inputs` keeps the default accent and shades
        }
        read.*shade.field = ToArgb(color);
    }
    for (const Shade& shade : kShades) {
        inputs.*shade.field = read.*shade.field;
    }
}

void ReadTextScale(TokenInputs& inputs) noexcept {
    if (!g_state.settings2) {
        return;  // already logged when UISettings was created
    }
    DOUBLE scale = 0;
    HRESULT hr = g_state.settings2->get_TextScaleFactor(&scale);
    if (FAILED(hr) || !std::isfinite(scale)) {
        log::Warn(L"tokens", L"reading the text scale failed: 0x%08X; 100 %%",
                  static_cast<unsigned>(FAILED(hr) ? hr : E_UNEXPECTED));
        return;
    }
    if (scale < kMinTextScale || scale > kMaxTextScale) {
        double clamped = scale < kMinTextScale ? kMinTextScale : kMaxTextScale;
        log::Warn(L"tokens", L"text scale %.2f out of range; using %.2f", scale, clamped);
        scale = clamped;
    }
    inputs.textScale = static_cast<float>(scale);
}

void ReadFont(TokenInputs& inputs) noexcept {
    ComRef<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(factory.Put()));
    ComRef<IDWriteFontCollection> fonts;
    if (SUCCEEDED(hr)) {
        hr = factory->GetSystemFontCollection(fonts.Put(), FALSE);
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(hr)) {
        hr = fonts->FindFamilyName(kVariableTextFamily, &index, &exists);
    }
    if (FAILED(hr)) {
        log::Warn(L"tokens", L"checking for \"%s\" failed: 0x%08X; assumed present",
                  kVariableTextFamily, static_cast<unsigned>(hr));
        return;
    }
    inputs.variableFontAvailable = exists != FALSE;
    if (!exists) {
        log::Warn(L"tokens", L"\"%s\" is not installed; using Segoe UI", kVariableTextFamily);
    }
}

// Reads every input (each failed read keeps its default) and hands them to the callback.
void ReadAndNotify() noexcept {
    TokenInputs inputs = DefaultInputs();
    ReadColors(inputs);
    ReadTextScale(inputs);
    ReadFont(inputs);
    if (g_state.onInputs != nullptr) {
        g_state.onInputs(inputs, g_state.context);
    }
}

void Reread() noexcept {
    g_rereadPending.store(false, std::memory_order_release);
    if (g_state.started) {
        ReadAndNotify();
    }
}

void StartOnWorker(InputsFn onInputs, void* context) noexcept {
    if (g_state.started) {
        return;
    }
    g_state.started = true;
    g_state.onInputs = onInputs;
    g_state.context = context;
    CreateSettings();
    // Registered before the first read, so a change made during the read is not missed.
    if (g_state.settings2 || g_state.settings3) {
        Register();
    }
    ReadAndNotify();
}

void StopOnWorker() noexcept {
    if (!g_state.started) {
        return;
    }
    if (g_state.colorRegistered) {
        g_state.settings3->remove_ColorValuesChanged(g_state.colorToken);
        g_state.colorRegistered = false;
    }
    if (g_state.textScaleRegistered) {
        g_state.settings2->remove_TextScaleFactorChanged(g_state.textScaleToken);
        g_state.textScaleRegistered = false;
    }
    g_state.handler.Reset();
    g_state.settings2.Reset();
    g_state.settings3.Reset();
    g_state.onInputs = nullptr;
    g_state.context = nullptr;
    g_state.started = false;

    // An event being delivered while the handlers were removed still holds the handler; its
    // code is in this DLL, so wait (briefly) for it to return before the DLL can unload.
    for (int waited = 0; g_liveHandlers.load(std::memory_order_acquire) != 0;
         waited += kHandlerReleasePollMs) {
        if (waited >= kHandlerReleaseWaitMs) {
            log::Warn(L"tokens", L"UISettings still holds the change handler after %d ms",
                      kHandlerReleaseWaitMs);
            break;
        }
        Sleep(kHandlerReleasePollMs);
    }
}

}  // namespace

bool Start(InputsFn onInputs, void* context) noexcept {
    bool posted = false;
    try {
        posted = worker::Post([onInputs, context] { StartOnWorker(onInputs, context); });
    } catch (...) {
        posted = false;  // copying the task into std::function ran out of memory
    }
    if (!posted) {
        log::Warn(L"tokens", L"the worker did not accept the system reads; tokens stay on "
                  L"their defaults");
    }
    return posted;
}

void Stop() noexcept {
    // Post fails only when the worker is not running, and then the start task never ran
    // either (worker::Stop drains every task it accepted).
    worker::Post(StopOnWorker);
}

}  // namespace wholeui::tokens::system
