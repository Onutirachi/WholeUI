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
void Destroy() noexcept;

}  // namespace wholeui::diagnostic::overlay

#endif  // WHOLEUI_DIAGNOSTIC_OVERLAY_H
