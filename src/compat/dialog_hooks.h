// Dialog template capture and the dialog facts store (research R8, FR-013a, FR-023a). Records
// the one fact that cannot be read later: the template a #32770 was created from. Never
// changes a window (FR-008) and never hooks a function that can wait or run a message loop.
//
// - kernelbase FindResourceExW and FindResourceW: a successful RT_DIALOG lookup (only the
//   outermost call on the thread; FindResourceW calls FindResourceExW) is remembered in a
//   thread-local list and arms a WH_CBT hook on the calling thread.
// - The CBT hook, at HCBT_CREATEWND for a #32770, takes the most recent remembered lookup
//   whose module is the window's HINSTANCE and records HWND -> template. Lookups older than
//   2 s are dropped at each callback; the hook disarms itself when none remain.
// - Hosted pages (property sheet pages) are #32770 windows like any other: their template is
//   recorded the same way. Nothing records a sheet's page list.

#ifndef WHOLEUI_COMPAT_DIALOG_HOOKS_H
#define WHOLEUI_COMPAT_DIALOG_HOOKS_H

#include <windows.h>

#include "rules.h"

namespace wholeui::compat {

// Wh_ModInit: sets the FindResource hooks (Windhawk applies them when Wh_ModInit returns). A
// hook that cannot be set is logged with log::Warn; templates stay unknown, which is safe.
void InstallDialogHooks();

// Wh_ModUninit: stops recording, removes every armed thread CBT hook (from this thread), waits
// until no CBT callback is running and clears the store (FR-023a, FR-029). The wait has no
// time limit; one log::Warn if it lasts over 1 s. Windhawk removes the function hooks.
void UninstallDialogHooks() noexcept;

// The template `dialog` was created from, or Unknown when no lookup matched its creation
// (created before the mod loaded, through an A function, from a template not loaded with
// FindResource*, or in another process). Throws only std::bad_alloc.
TemplateField DialogTemplateOf(HWND dialog);

}  // namespace wholeui::compat

#endif  // WHOLEUI_COMPAT_DIALOG_HOOKS_H
