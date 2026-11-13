# Contract: Interfaces for later specs (FR-019, FR-027)

The C++ surface later specs use. Everything else in the foundation is internal. Names are
binding; exact signatures may be refined during implementation if the spec's behavior is
kept, and this file is updated with them.

## Compatibility engine (`src/compat/engine.h`)

```cpp
namespace wholeui::compat {

enum class Level : uint8_t { Native, Visual, Full };

// rules.h already has the pure Classification (with a Rule pointer into the rule set);
// the engine returns a copy that stays valid after the rule set is replaced.
struct WindowClassification {
    Level level;            // effective level
    Level ownLevel;         // before top-level / page caps
    Reason reason;          // Default, Rule, Locked, CappedByTopLevel, LoweredByPage, Failure
    RuleSource ruleSource;  // meaningful only when ruleOrigin is not empty
    std::wstring ruleOrigin;// empty when no rule
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

## Design tokens (`src/tokens/tokens.h`)

```cpp
namespace wholeui::tokens {

struct Tokens;                       // fields as in data-model.md, all const
const Tokens& Current();             // snapshot valid until the next change notification
using TokensChangedFn = void (*)(void* context);
SubscriptionId SubscribeChanged(TokensChangedFn fn, void* context);
void Unsubscribe(SubscriptionId id);

Tokens Resolve(const TokenInputs& in);  // pure; used by tests

}
```

`Current()` returns a reference to an immutable snapshot; a change swaps the snapshot and
then notifies. A caller that keeps values across a notification re-reads `Current()`.

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
