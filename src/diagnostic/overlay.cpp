#include "overlay.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "../core/com.h"
#include "../core/log.h"
#include "../core/worker.h"
#include "../tokens/runtime.h"

namespace wholeui::diagnostic::overlay {
namespace {

constexpr wchar_t kWindowClass[] = L"WholeUI.Diagnostic";
constexpr int kCursorGap = 16;   // px at 96 DPI between the cursor and the overlay (R14)
constexpr float kLayoutMax = 100000.0f;  // "no limit" for a text layout, in DIPs

// Everything the overlay owns. Touched only on the worker thread, so no lock.
struct State {
    HWND window = nullptr;
    bool classRegistered = false;
    bool visible = false;
    bool warned = false;  // a draw failure was logged; cleared by the next success

    ComRef<ID2D1Factory> d2d;
    ComRef<IDWriteFactory> dwrite;
    ComRef<ID2D1DCRenderTarget> target;

    HDC memoryDc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ previousBitmap = nullptr;
    SIZE dibSize = {};

    SIZE size = {};    // the overlay in physical px, as last drawn
    UINT dpi = 0;      // the DPI it was drawn for
    std::wstring text; // the text it shows, for a redraw on a DPI change
};

State g;

// Per-monitor-v2 awareness for the calling thread while in scope, so cursor positions,
// monitor rectangles and the window's size are all physical pixels.
class PerMonitorDpiScope {
public:
    PerMonitorDpiScope() noexcept
        : previous_(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {}
    ~PerMonitorDpiScope() {
        if (previous_ != nullptr) {
            SetThreadDpiAwarenessContext(previous_);
        }
    }
    PerMonitorDpiScope(const PerMonitorDpiScope&) = delete;
    PerMonitorDpiScope& operator=(const PerMonitorDpiScope&) = delete;

private:
    DPI_AWARENESS_CONTEXT previous_;
};

bool OnWorker(const wchar_t* function) noexcept {
    if (worker::IsWorkerThread()) {
        return true;
    }
    log::Warn(L"diagnostic", L"overlay %s called off the worker thread; ignored", function);
    return false;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_DPICHANGED:
            // The size comes from UpdateLayeredWindow; Follow redraws for the new DPI.
            return 0;
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

// Logs the first failure after a success; later ones stay quiet until a draw succeeds.
void Fail(const wchar_t* what, HRESULT result) noexcept {
    if (!g.warned) {
        g.warned = true;
        log::Warn(L"diagnostic", L"overlay: %s failed: 0x%08X; overlay hidden", what,
                  static_cast<unsigned>(result));
    }
}

bool EnsureWindow() noexcept {
    if (g.window != nullptr) {
        return true;
    }
    HINSTANCE instance = ModuleInstance();
    if (!g.classRegistered) {
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = kWindowClass;
        if (RegisterClassExW(&windowClass) == 0 &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            Fail(L"RegisterClassEx", HRESULT_FROM_WIN32(GetLastError()));
            return false;
        }
        g.classRegistered = true;
    }
    g.window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kWindowClass, L"WholeUI diagnostic", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance,
        nullptr);
    if (g.window == nullptr) {
        Fail(L"CreateWindowEx", HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    return true;
}

bool EnsureFactories() noexcept {
    if (!g.d2d) {
        HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                           __uuidof(ID2D1Factory), nullptr, g.d2d.PutVoid());
        if (FAILED(result)) {
            Fail(L"D2D1CreateFactory", result);
            return false;
        }
    }
    if (!g.dwrite) {
        HRESULT result =
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                reinterpret_cast<IUnknown**>(g.dwrite.Put()));
        if (FAILED(result)) {
            Fail(L"DWriteCreateFactory", result);
            return false;
        }
    }
    return true;
}

void ReleaseDib() noexcept {
    if (g.memoryDc != nullptr && g.previousBitmap != nullptr) {
        SelectObject(g.memoryDc, g.previousBitmap);
        g.previousBitmap = nullptr;
    }
    if (g.dib != nullptr) {
        DeleteObject(g.dib);
        g.dib = nullptr;
    }
    g.dibSize = {};
}

// A 32-bit top-down DIB of exactly `size`, selected into the memory DC.
bool EnsureDib(SIZE size) noexcept {
    if (g.dib != nullptr && g.dibSize.cx == size.cx && g.dibSize.cy == size.cy) {
        return true;
    }
    if (g.memoryDc == nullptr) {
        g.memoryDc = CreateCompatibleDC(nullptr);
        if (g.memoryDc == nullptr) {
            Fail(L"CreateCompatibleDC", E_FAIL);
            return false;
        }
    }
    ReleaseDib();
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    g.dib = CreateDIBSection(g.memoryDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (g.dib == nullptr) {
        Fail(L"CreateDIBSection", E_OUTOFMEMORY);
        return false;
    }
    g.previousBitmap = SelectObject(g.memoryDc, g.dib);
    g.dibSize = size;
    return true;
}

bool EnsureTarget() noexcept {
    if (g.target) {
        return true;
    }
    D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT result = g.d2d->CreateDCRenderTarget(&properties, g.target.Put());
    if (FAILED(result)) {
        Fail(L"CreateDCRenderTarget", result);
        return false;
    }
    return true;
}

UINT DpiAt(HMONITOR monitor) noexcept {
    UINT dpiX = USER_DEFAULT_SCREEN_DPI;
    UINT dpiY = USER_DEFAULT_SCREEN_DPI;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) || dpiX == 0) {
        return USER_DEFAULT_SCREEN_DPI;
    }
    return dpiX;
}

D2D1_COLOR_F Color(std::uint32_t argb) noexcept {
    return D2D1::ColorF(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(argb & 0xFF) / 255.0f,
                        static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

// 16 px below-right of the cursor; on the other side of the cursor on an axis where that
// would leave the monitor; then kept inside the monitor. Only the last step can overlap the
// cursor's column or row, never both, so the inspected point stays visible (FR-021a).
POINT Placement(POINT cursor, SIZE size, const RECT& monitor, UINT dpi) noexcept {
    int gap = MulDiv(kCursorGap, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    POINT at = {cursor.x + gap, cursor.y + gap};
    if (at.x + size.cx > monitor.right) {
        at.x = cursor.x - gap - size.cx;
    }
    if (at.y + size.cy > monitor.bottom) {
        at.y = cursor.y - gap - size.cy;
    }
    at.x = std::max(at.x, monitor.left);
    at.y = std::max(at.y, monitor.top);
    return at;
}

struct MonitorAt {
    RECT rect;
    UINT dpi;
};

MonitorAt MonitorOf(POINT cursor) noexcept {
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info)) {
        return {RECT{cursor.x - 100000, cursor.y - 100000, cursor.x + 100000, cursor.y + 100000},
                USER_DEFAULT_SCREEN_DPI};
    }
    return {info.rcMonitor, DpiAt(monitor)};
}

// Lays out `text` with the body type style; the values after each tab line up on one tab
// stop past the widest label. The last line (the hint) uses `hintBrush`.
bool MakeLayout(const tokens::Tokens& tokens, const std::wstring& text, ID2D1Brush* hintBrush,
                ComRef<IDWriteTextLayout>& layout) noexcept {
    ComRef<IDWriteTextFormat> format;
    HRESULT result = g.dwrite->CreateTextFormat(
        tokens.fontFamily.c_str(), nullptr, static_cast<DWRITE_FONT_WEIGHT>(tokens.body.weight),
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, tokens.body.size, L"",
        format.Put());
    if (FAILED(result)) {
        Fail(L"CreateTextFormat", result);
        return false;
    }
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, tokens.body.lineHeight,
                           tokens.body.lineHeight * 0.8f);

    float widestLabel = 0;
    std::wstring_view rest(text);
    std::size_t lastLineStart = 0;
    for (std::size_t lineStart = 0; lineStart <= text.size();) {
        std::size_t lineEnd = text.find(L"\r\n", lineStart);
        std::wstring_view line =
            rest.substr(lineStart, lineEnd == std::wstring::npos ? std::wstring::npos
                                                                 : lineEnd - lineStart);
        lastLineStart = lineStart;
        std::size_t tab = line.find(L'\t');
        if (tab != std::wstring_view::npos) {
            ComRef<IDWriteTextLayout> label;
            DWRITE_TEXT_METRICS metrics = {};
            if (SUCCEEDED(g.dwrite->CreateTextLayout(line.data(), static_cast<UINT32>(tab),
                                                     format.Get(), kLayoutMax, kLayoutMax,
                                                     label.Put())) &&
                SUCCEEDED(label->GetMetrics(&metrics))) {
                widestLabel = std::max(widestLabel, metrics.widthIncludingTrailingWhitespace);
            }
        }
        if (lineEnd == std::wstring::npos) {
            break;
        }
        lineStart = lineEnd + 2;
    }
    format->SetIncrementalTabStop(widestLabel + static_cast<float>(tokens.spacing.l));

    result = g.dwrite->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                                        format.Get(), kLayoutMax, kLayoutMax, layout.Put());
    if (FAILED(result)) {
        Fail(L"CreateTextLayout", result);
        return false;
    }
    if (lastLineStart > 0) {
        layout->SetDrawingEffect(
            hintBrush, DWRITE_TEXT_RANGE{static_cast<UINT32>(lastLineStart),
                                         static_cast<UINT32>(text.size() - lastLineStart)});
    }
    return true;
}

// Draws `text` for `dpi` into the DIB and sets g.size. False on failure (logged).
bool Draw(const std::wstring& text, UINT dpi) noexcept {
    if (!EnsureFactories() || !EnsureTarget()) {
        return false;
    }
    const tokens::Tokens& tokens = tokens::Current();
    const float scale = static_cast<float>(dpi) / USER_DEFAULT_SCREEN_DPI;

    ComRef<ID2D1SolidColorBrush> background, stroke, textBrush, hintBrush;
    HRESULT result = g.target->CreateSolidColorBrush(Color(tokens.backgroundBase), background.Put());
    if (SUCCEEDED(result)) result = g.target->CreateSolidColorBrush(Color(tokens.strokeControl), stroke.Put());
    if (SUCCEEDED(result)) result = g.target->CreateSolidColorBrush(Color(tokens.textPrimary), textBrush.Put());
    if (SUCCEEDED(result)) result = g.target->CreateSolidColorBrush(Color(tokens.textSecondary), hintBrush.Put());
    if (FAILED(result)) {
        Fail(L"CreateSolidColorBrush", result);
        g.target.Reset();
        return false;
    }

    ComRef<IDWriteTextLayout> layout;
    if (!MakeLayout(tokens, text, hintBrush.Get(), layout)) {
        return false;
    }
    DWRITE_TEXT_METRICS metrics = {};
    result = layout->GetMetrics(&metrics);
    if (FAILED(result)) {
        Fail(L"GetMetrics", result);
        return false;
    }

    // Size in DIPs, then physical px.
    const float padX = static_cast<float>(tokens.spacing.m);
    const float padY = static_cast<float>(tokens.spacing.s);
    const float width = metrics.widthIncludingTrailingWhitespace + 2 * padX;
    const float height = metrics.height + 2 * padY;
    SIZE size = {static_cast<LONG>(std::ceil(width * scale)),
                 static_cast<LONG>(std::ceil(height * scale))};
    if (!EnsureDib(size)) {
        return false;
    }

    RECT bounds = {0, 0, size.cx, size.cy};
    g.target->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    result = g.target->BindDC(g.memoryDc, &bounds);
    if (FAILED(result)) {
        Fail(L"BindDC", result);
        g.target.Reset();
        return false;
    }
    // Grayscale: ClearType needs an opaque background, and the corners are transparent.
    g.target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    g.target->BeginDraw();
    g.target->Clear(D2D1::ColorF(0, 0, 0, 0));
    const float pixel = 1.0f / scale;  // one physical px in DIPs: the stroke width
    const float dipWidth = static_cast<float>(size.cx) / scale;
    const float dipHeight = static_cast<float>(size.cy) / scale;
    const float radius = static_cast<float>(tokens.radiusOverlay);
    D2D1_ROUNDED_RECT shape = D2D1::RoundedRect(
        D2D1::RectF(pixel / 2, pixel / 2, dipWidth - pixel / 2, dipHeight - pixel / 2), radius,
        radius);
    g.target->FillRoundedRectangle(shape, background.Get());
    g.target->DrawRoundedRectangle(shape, stroke.Get(), pixel);
    g.target->DrawTextLayout(D2D1::Point2F(padX, padY), layout.Get(), textBrush.Get(),
                             D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    result = g.target->EndDraw();
    if (FAILED(result)) {
        // D2DERR_RECREATE_TARGET included: the next Show makes a new target.
        Fail(L"EndDraw", result);
        g.target.Reset();
        return false;
    }
    GdiFlush();
    g.size = size;
    g.dpi = dpi;
    return true;
}

// Pushes the DIB to the window at `at` and shows it without activating it.
bool Present(POINT at) noexcept {
    POINT source = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (!UpdateLayeredWindow(g.window, nullptr, &at, &g.size, g.memoryDc, &source, 0, &blend,
                             ULW_ALPHA)) {
        Fail(L"UpdateLayeredWindow", HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    if (!g.visible) {
        SetWindowPos(g.window, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        g.visible = true;
    }
    return true;
}

void HideWindow() noexcept {
    if (g.window != nullptr && g.visible) {
        ShowWindow(g.window, SW_HIDE);
    }
    g.visible = false;
}

bool ShowNow(POINT cursor, const std::wstring& text) noexcept {
    if (!EnsureWindow()) {
        return false;
    }
    MonitorAt monitor = MonitorOf(cursor);
    if (!Draw(text, monitor.dpi)) {
        return false;
    }
    if (!Present(Placement(cursor, g.size, monitor.rect, monitor.dpi))) {
        return false;
    }
    g.warned = false;
    return true;
}

}  // namespace

bool Show(POINT cursor, const std::wstring& text) noexcept {
    if (!OnWorker(L"Show")) {
        return false;
    }
    PerMonitorDpiScope dpiScope;
    try {
        g.text = text;
    } catch (...) {
        Fail(L"copying the text", E_OUTOFMEMORY);
        HideWindow();
        return false;
    }
    if (!ShowNow(cursor, g.text)) {
        HideWindow();
        return false;
    }
    return true;
}

void Follow(POINT cursor) noexcept {
    if (!OnWorker(L"Follow") || !g.visible) {
        return;
    }
    PerMonitorDpiScope dpiScope;
    MonitorAt monitor = MonitorOf(cursor);
    if (monitor.dpi != g.dpi) {
        if (!ShowNow(cursor, g.text)) {
            HideWindow();
        }
        return;
    }
    POINT at = Placement(cursor, g.size, monitor.rect, monitor.dpi);
    SetWindowPos(g.window, nullptr, at.x, at.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Hide() noexcept {
    if (OnWorker(L"Hide")) {
        HideWindow();
    }
}

void Destroy() noexcept {
    if (!OnWorker(L"Destroy")) {
        return;
    }
    if (g.window != nullptr) {
        DestroyWindow(g.window);
        g.window = nullptr;
    }
    g.visible = false;
    if (g.classRegistered) {
        UnregisterClassW(kWindowClass, ModuleInstance());
        g.classRegistered = false;
    }
    g.target.Reset();
    ReleaseDib();
    if (g.memoryDc != nullptr) {
        DeleteDC(g.memoryDc);
        g.memoryDc = nullptr;
    }
    g.dwrite.Reset();
    g.d2d.Reset();
    g.size = {};
    g.dpi = 0;
    g.text.clear();
    g.text.shrink_to_fit();
    g.warned = false;
}

}  // namespace wholeui::diagnostic::overlay
