// Reads a window's identity fields from Windows (research R7, FR-010). Never reads titles.

#ifndef WHOLEUI_COMPAT_IDENTITY_H
#define WHOLEUI_COMPAT_IDENTITY_H

#include <windows.h>

#include <vector>

#include "rules.h"

namespace wholeui::compat {

// The identity of `hwnd`, in this process or another one. A field that cannot be read is
// Unknown; `failed` is set only when the window itself is invalid (FR-018). Process and
// module are lower-case file names without a path. The template is NotADialog for any class
// but #32770; for a dialog it is the template the dialog hooks recorded, else Unknown.
// Throws only std::bad_alloc; the engine turns that into a Failure.
WindowIdentity ReadIdentity(HWND hwnd);

// The window's top-level window (GetAncestor GA_ROOT): `hwnd` itself when it is top-level,
// nullptr when the window is invalid.
HWND TopLevelOf(HWND hwnd) noexcept;

// The hosted pages between `hwnd` and its top-level window `topLevel`, nearest first: ancestors
// of class #32770 with WS_CHILD (property sheet pages, child dialogs; research R8). Walked
// with GetAncestor(GA_PARENT) until `topLevel` (GetParent on a top-level window would
// return its owner). Empty when `hwnd` is top-level or has no page above it. Works for
// windows of other processes. Throws only std::bad_alloc.
std::vector<HWND> HostedPagesOf(HWND hwnd, HWND topLevel);

}  // namespace wholeui::compat

#endif  // WHOLEUI_COMPAT_IDENTITY_H
