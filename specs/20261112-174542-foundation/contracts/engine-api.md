# Contract: Interfaces for later specs (FR-019, FR-027)

The C++ surface later specs use. Everything else in the foundation is internal. Names are
binding; exact signatures may be refined during implementation if the spec's behavior is
kept, and this file is updated with them.

## Compatibility engine (`src/compat/engine.h`)

```cpp
namespace wholeui::compat {

enum class Level : uint8_t { Native, Visual, Full };

// rules.h has the pure Classification (with a Rule pointer into the rule set and, for
// LoweredByPage, a pointer to the first lowest page). WindowClassification, also in rules.h
// (PURE, so diagnostic/format can use it), is the copy made by CopyOf: it stays valid after
// the rule set and pages are replaced.
struct WindowClassification {
    Level level;            // effective level
    Level ownLevel;         // before top-level / page caps
    Reason reason;          // Default, Rule, Locked, CappedByTopLevel, LoweredByPage, Failure
    RuleSource ruleSource;  // meaningful only when ruleOrigin is not empty
    std::wstring ruleOrigin;// empty when no rule
    Field pageModule;       // LoweredByPage: module of the page that lowered it (may be Unknown)
};

// Safe from any thread. Never throws. Any failure yields Level::Native (FR-018).
WindowClassification Classify(HWND hwnd);
Level LevelOf(HWND hwnd);   // Classify(hwnd).level

// Called after a cached window's effective level changes (settings edited, late page).
// The callback runs on the thread that caused the change, without engine locks held.
// Returns a token; Unsubscribe before the subscribing module unloads.
using LevelChangedFn = void (*)(HWND hwnd, Level oldLevel, Level newLevel, void* context);
SubscriptionId SubscribeLevelChanged(LevelChangedFn fn, void* context);
void Unsubscribe(SubscriptionId id);

}
```

Used by `src/main.cpp` only (not by later specs):

```cpp
void SetRuleSet(RuleSet rules);  // swap, re-classify cached windows, notify changes
void ResetEngine();              // unload: drop the rule set, cache and listeners
```

Used by `src/diagnostic/overlay.cpp` only (FR-021, US3 scenario 5):

```cpp
// The level the current rules give a window of another process, from identities read with
// ReadIdentity. Rules only: no cache, no listeners, no property sheet pages; Unknown fields
// match no rule. topLevel == nullptr: the window is itself top-level. Never throws.
WindowClassification ClassifyIdentity(const WindowIdentity& window,
                                      const WindowIdentity* topLevel) noexcept;
```

Used by `src/compat/dialog_hooks.cpp` only:

```cpp
// A page from a module not listed before was added to a sheet after it was created
// (FR-013a): re-classify the cached sheet and its children, notify changed levels.
void OnSheetPagesChanged(HWND sheet) noexcept;
```

Rules for callers:

- Ask before changing a window, every time it matters (cheap: cached).
- On `newLevel < oldLevel`, restore the window to how Windows draws it (constitution I).
- Do not rely on the answer for a child window while it is being created (`HCBT_CREATEWND`,
  `WM_NCCREATE`): it is not linked to its parent yet and looks top-level, so the engine
  answers `Native`/`Failure` without caching it (a verbose line, not a warning). Ask again
  once creation returns (`WM_CREATE` or later).

Classification reads the window, its top-level window and, when the top-level window is a
property sheet the dialog hooks saw, the sheet's pages (`SheetPagesOf`; an incomplete list
counts as one page with every field Unknown, which lowers the sheet to `Native`). A dialog's
template comes from `DialogTemplateOf`. The engine never holds its lock while it takes the
dialog store's lock, and `dialog_hooks` never calls the engine while it holds its own. While
diagnostic mode is on, each classification (not a cache hit) and each level change writes one
`log::Verbose` line with the level, own level, reason, rule, identity fields, top-level window
and page count; never a title.

## Design tokens (`src/tokens/tokens.h`, `src/tokens/runtime.h`)

```cpp
namespace wholeui::tokens {

// tokens.h (PURE)
struct Tokens;                       // fields as in data-model.md
Tokens Resolve(const TokenInputs& in);  // pure; used by tests

// runtime.h
const Tokens& Current() noexcept;    // snapshot; the reference stays valid until unload
using TokensChangedFn = void (*)(void* context);
SubscriptionId SubscribeChanged(TokensChangedFn fn, void* context);
void Unsubscribe(SubscriptionId id);

}
```

`Current()` returns a reference to an immutable snapshot; before the first system read it is
`Resolve(DefaultInputs())`. A change swaps the snapshot and then notifies, on the worker
thread with no lock held. Old snapshots are kept until unload, so a reference read on another
thread during a change stays valid; a caller that keeps values across a notification
re-reads `Current()`. A system read equal to the current inputs is skipped: no swap, no log
line, no notification (Windows raises one change several times). Each swap writes one
`log::Info` line with mode, accent, text scale and font family.

Used by `src/main.cpp` only:

```cpp
void OnSystemInputs(const TokenInputs& inputs, void* context) noexcept;  // system::InputsFn
void ResetTokens() noexcept;  // unload, after system::Stop and worker::Stop
```

## Logging (`src/core/log.h`)

```cpp
namespace wholeui::log {
void Info(const wchar_t* area, const wchar_t* fmt, ...);     // startup, settings, token changes
void Warn(const wchar_t* area, const wchar_t* fmt, ...);     // ignored overrides, fallbacks, failures
void Verbose(const wchar_t* area, const wchar_t* fmt, ...);  // per-window lines; only while diagnostic mode is on
void SetVerbose(bool on);                                    // diagnostic mode turns it on (FR-028)
bool IsVerbose();
}
```

All three forward to `Wh_Log` as `[area] message` (`Warn` adds `warning: ` before it).
`area` names the writing module (`core`, `compat`, `settings`, `tokens`, `diagnostic`).
Format strings use the Windows wide printf rules (`%s` is a wide string); messages longer
than 1023 characters are cut.
