// The current design tokens and their change notifications (contracts/engine-api.md "Design
// tokens", FR-025, FR-028).
//
// Holds one immutable snapshot of resolved tokens. Before the first system read it holds the
// tokens of DefaultInputs. Each new set of inputs from tokens/system.h is resolved, swapped in
// as the new snapshot, logged with log::Info, and then the listeners are notified. A set equal
// to the current inputs is skipped (no swap, no log, no notification): Windows raises one
// change several times.

#ifndef WHOLEUI_TOKENS_RUNTIME_H
#define WHOLEUI_TOKENS_RUNTIME_H

#include "../core/subscriptions.h"
#include "tokens.h"

namespace wholeui::tokens {

// The current snapshot. The reference stays valid until ResetTokens (mod unload): a swap keeps
// the old snapshot alive, so a caller reading it on another thread during a change is safe.
// A caller that keeps values across a change notification re-reads Current(). Any thread.
const Tokens& Current() noexcept;

// Called after each swap, on the thread that delivered the inputs (the worker), with no lock
// of this module held. Unsubscribe before the subscribing module unloads.
using TokensChangedFn = void (*)(void* context);
SubscriptionId SubscribeChanged(TokensChangedFn fn, void* context);
void Unsubscribe(SubscriptionId id);

// Used by src/main.cpp only.

// A system::InputsFn: resolve, swap, log, notify (or skip when unchanged). `context` is unused.
// If resolving fails (out of memory), the current snapshot stays and one warning is logged.
void OnSystemInputs(const TokenInputs& inputs, void* context) noexcept;

// Unload: drop the listeners and every snapshot. Call after system::Stop and worker::Stop,
// when no other code of the mod reads the tokens; Current() then returns the defaults again.
void ResetTokens() noexcept;

}  // namespace wholeui::tokens

#endif  // WHOLEUI_TOKENS_RUNTIME_H
