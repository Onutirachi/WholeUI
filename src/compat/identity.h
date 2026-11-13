// Reads a window's identity fields from Windows (research R7, FR-010). Never reads titles.

#ifndef WHOLEUI_COMPAT_IDENTITY_H
#define WHOLEUI_COMPAT_IDENTITY_H

#include <windows.h>

#include "rules.h"

namespace wholeui::compat {

// The identity of `hwnd`, in this process or another one. A field that cannot be read is
// Unknown; `failed` is set only when the window itself is invalid (FR-018). Process and
// module are lower-case file names without a path. The template is NotADialog for any class
// but #32770, and Unknown for a dialog (the dialog facts store fills it in, T034).
// Throws only std::bad_alloc; the engine turns that into a Failure.
WindowIdentity ReadIdentity(HWND hwnd);

// The window's top-level window (GetAncestor GA_ROOT): `hwnd` itself when it is top-level,
// nullptr when the window is invalid.
HWND TopLevelOf(HWND hwnd) noexcept;

// The lower-case file name of a module loaded in this process, Unknown when it cannot be read
// (null, or a resource-only handle). Used for property sheet pages (dialog_hooks).
// Throws only std::bad_alloc.
Field ModuleNameOf(HMODULE module);

// The lower-case file name of this process's image. Throws only std::bad_alloc.
Field CurrentProcessName();

}  // namespace wholeui::compat

#endif  // WHOLEUI_COMPAT_IDENTITY_H
