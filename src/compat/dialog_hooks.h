// Dialog and property sheet hooks, and the dialog facts store (research R8 "Revision",
// FR-013a). The hooks only record facts that cannot be read later: the template a dialog was
// created from, and the pages of a property sheet. They never change a window (FR-008).
//
// - user32 CreateDialogParamW, DialogBoxParamW and their Indirect variants: while the call
//   runs, a WH_CBT hook on the calling thread records the template of the first #32770 with
//   the call's HINSTANCE. Indirect variants get the template from the most recent RT_DIALOG
//   lookup (kernelbase FindResourceExW / FindResourceW) for that module on the same thread.
// - comctl32 CreatePropertySheetPageW and PropertySheetW: the pages listed in the sheet's
//   header are attached to the sheet window (the first #32770 created on the thread during
//   PropertySheetW) before it is shown. A page window from a module not in the list, created
//   later, is added and the engine is told (OnSheetPagesChanged).
// - comctl32 instances loaded after Wh_ModInit are hooked from a kernelbase LoadLibraryExW
//   hook.

#ifndef WHOLEUI_COMPAT_DIALOG_HOOKS_H
#define WHOLEUI_COMPAT_DIALOG_HOOKS_H

#include <windows.h>

#include <vector>

#include "rules.h"

namespace wholeui::compat {

// Wh_ModInit: sets the hooks (Windhawk applies them when Wh_ModInit returns). A hook that
// cannot be set is logged with log::Warn; its facts stay unknown, which is safe.
void InstallDialogHooks();

// Wh_ModUninit: stops recording, removes the thread CBT hooks still installed and clears the
// store (FR-029). Windhawk removes the function hooks.
void UninstallDialogHooks() noexcept;

// The template `dialog` was created from, or Unknown when no hook saw its creation (created
// before the mod loaded, through an A variant, or an indirect template with no lookup).
// Throws only std::bad_alloc.
TemplateField DialogTemplateOf(HWND dialog);

// The pages of a property sheet. `known` is false when `sheet` is not a sheet seen by the
// hooks. A page whose module or template could not be read has Unknown fields; `incomplete`
// means the page list itself could not be built (treat the sheet as having an unknown page).
struct SheetPages {
    bool known = false;
    bool incomplete = false;
    std::vector<WindowIdentity> pages;
};

// Throws only std::bad_alloc.
SheetPages SheetPagesOf(HWND sheet);

}  // namespace wholeui::compat

#endif  // WHOLEUI_COMPAT_DIALOG_HOOKS_H
