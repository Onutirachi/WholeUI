// System reads for the design tokens (research R11, R12).
//
// Reads the inputs of the token resolver from Windows on the worker thread (core/worker.h):
// the accent and its six shades, light or dark app mode and the text scale from WinRT
// UISettings, and whether "Segoe UI Variable Text" is installed from DirectWrite. It reads
// them once when started and again each time UISettings reports a color or text scale
// change, and hands each complete set of inputs to one callback.
//
// A read that fails gives that input its default (DefaultInputs) and logs one warning. The
// accent and its shades are one input: if any of the seven colors cannot be read, all seven
// take their defaults, so the shades always belong to the accent.

#ifndef WHOLEUI_TOKENS_SYSTEM_H
#define WHOLEUI_TOKENS_SYSTEM_H

#include "tokens.h"

namespace wholeui::tokens::system {

// Receives a complete set of inputs. Always called on the worker thread, never while a lock
// of this module is held: once after Start, then after each system change.
using InputsFn = void (*)(const TokenInputs& inputs, void* context);

// Posts the first read and the event registration to the worker thread. Returns false (and
// logs a warning) when the worker does not accept the task; `onInputs` is then never called.
// Calling Start again while the system reads run does nothing.
bool Start(InputsFn onInputs, void* context) noexcept;

// Posts the unregistration and the release of the WinRT objects to the worker thread. Call it
// before worker::Stop, which waits for that task. After it runs, `onInputs` is not called
// again. Does nothing if Start was not called.
void Stop() noexcept;

}  // namespace wholeui::tokens::system

#endif  // WHOLEUI_TOKENS_SYSTEM_H
