// The diagnostic overlay window (research R14, FR-021a, FR-022).
//
// A layered, click-through, never-activated topmost window drawn with Direct2D/DirectWrite
// from the design tokens. It sits 16 px below-right of the cursor and flips to the other side
// near the edges of the cursor's monitor, so it never covers the inspected point. Being
// click-through, it is never returned by WindowFromPoint.
//
// Every function here runs on the worker thread only (the window belongs to it); a call from
// another thread is logged and ignored. Points are physical screen pixels (per-monitor DPI
// aware): the functions switch the thread to per-monitor-v2 awareness while they run, and so
// must the caller's GetCursorPos.

#ifndef WHOLEUI_DIAGNOSTIC_OVERLAY_H
#define WHOLEUI_DIAGNOSTIC_OVERLAY_H

#include <windows.h>

#include <string>

namespace wholeui::diagnostic::overlay {

// Draws `text` (OverlayText: lines "<label>\t<value>", CRLF between them, the last line is a
// hint drawn in the secondary text color) next to `cursor` and shows the overlay, creating
// the window on first use. Reads tokens::Current() on every call. Returns false when the
// overlay could not be drawn (one log::Warn until a draw succeeds again); the overlay is then
// hidden.
bool Show(POINT cursor, const std::wstring& text) noexcept;

// Moves a shown overlay next to `cursor` without redrawing, unless the cursor is now on a
// monitor with another DPI (then redraws the last text). Does nothing while hidden.
void Follow(POINT cursor) noexcept;

// Hides the overlay; the window and drawing resources are kept for the next Show.
void Hide() noexcept;

// Destroys the window and releases every drawing resource (diagnostic mode off, unload).
// Stops tracking too.
void Destroy() noexcept;

// Cursor tracking (research R14, SC-005): a 50 ms timer on the overlay window reads
// GetCursorPos and WindowFromPoint. The overlay follows the cursor on every tick; the window
// under the cursor and its top-level window are identified and classified (in this process
// through the engine, in another process with ClassifyIdentity) when that window changes and
// once 100 ms after the cursor stops, with one log::Verbose line giving the time from rest to
// update. The same tick polls Ctrl+Alt+Shift+C with GetAsyncKeyState (edge-triggered, not
// swallowed) and copies the top-level window's override line and compatibility-list row to
// the clipboard (FR-021b). No input hooks. Returns false (logged) when the window or timer
// cannot be made.
// Calling it while tracking does nothing and returns true.
bool StartTracking() noexcept;

// Kills the timer and hides the overlay; the window is kept (Destroy releases it).
void StopTracking() noexcept;

}  // namespace wholeui::diagnostic::overlay

namespace wholeui::diagnostic {

// Diagnostic mode lifecycle (research R14, FR-020, FR-023). Both are safe from any thread:
// they post their work to the worker thread, where it runs in order, so a quick off/on/off
// ends in the last requested state.
//
// Start: only one mod instance per session shows the overlay (explorer.exe can run as
// several processes). The instance that owns the named mutex Local\WholeUI.Diagnostic starts
// cursor tracking; the others stay idle and try again every second, so one of them takes
// over when the owner turns the mode off, unloads or exits. Calling Start while on does
// nothing. Returns false when the work cannot be posted (worker not running).
bool Start() noexcept;

// Stop: kills the retry timer and the tracking timer, destroys the overlay window and
// releases the mutex, all in one worker task. Does nothing while off. Must be called before
// worker::Stop (which runs the posted task before the thread exits).
void Stop() noexcept;

}  // namespace wholeui::diagnostic

#endif  // WHOLEUI_DIAGNOSTIC_OVERLAY_H
