// The compatibility engine (contracts/engine-api.md, FR-019): the level of a window, cached
// per HWND, and notifications when a cached window's level changes.
//
// Classification is lazy (research R6): a window is identified and classified the first time
// someone asks. The rule set is an immutable snapshot swapped under an SRWLOCK; listeners
// run after the lock is released (research R13).

#ifndef WHOLEUI_COMPAT_ENGINE_H
#define WHOLEUI_COMPAT_ENGINE_H

#include <windows.h>

#include <span>
#include <string>

#include "../core/subscriptions.h"
#include "rules.h"

namespace wholeui::compat {

// Safe from any thread. Never throws. Any failure gives Native with reason Failure and one
// log::Warn (FR-018). Before the first SetRuleSet (and after ResetEngine) every window is
// Native with reason Default.
WindowClassification Classify(HWND hwnd) noexcept;
Level LevelOf(HWND hwnd) noexcept;

// For diagnostic mode: the level the current rules give a window of another process, from
// identities read with ReadIdentity (FR-021, US3 scenario 5). Rules only: no cache, no
// listeners; Unknown fields match no rule. `pages` are the hosted pages between the window
// and its top-level window, nearest first (HostedPagesOf; may be empty). `topLevel` is
// nullptr when the window is itself top-level. Safe from any thread; never throws (failure:
// Native/Failure); before the first SetRuleSet, Native/Default.
WindowClassification ClassifyIdentity(const WindowIdentity& window,
                                      std::span<const WindowIdentity> pages,
                                      const WindowIdentity* topLevel) noexcept;

// Called after a cached window's effective level changes (settings edited), on
// the thread that caused the change, without engine locks held. Unsubscribe before the
// subscribing module unloads. On newLevel < oldLevel, restore the window (constitution I).
using LevelChangedFn = void (*)(HWND hwnd, Level oldLevel, Level newLevel, void* context);
SubscriptionId SubscribeLevelChanged(LevelChangedFn fn, void* context);
void Unsubscribe(SubscriptionId id);

// For src/main.cpp. SetRuleSet publishes a new rule set, drops destroyed windows from the
// cache, re-classifies the others and notifies listeners of each changed level (FR-016).
// ResetEngine drops the rule set, the cache and every listener (FR-029).
void SetRuleSet(RuleSet rules);
void ResetEngine();

}  // namespace wholeui::compat

#endif  // WHOLEUI_COMPAT_ENGINE_H
