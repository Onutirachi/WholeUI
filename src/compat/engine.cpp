#include "engine.h"

#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/lock.h"
#include "../core/log.h"
#include "dialog_hooks.h"
#include "identity.h"

namespace wholeui::compat {

namespace {

// Past this many entries, an insert first drops destroyed windows (research R6).
constexpr size_t kSweepThreshold = 4096;

// The pages of a property sheet, shared by the sheet's entry and its children's entries.
using PageList = std::shared_ptr<const std::vector<WindowIdentity>>;

struct Entry {
    WindowClassification result;
    // A different thread or class atom means the HWND was reused: classify again.
    DWORD threadId = 0;
    ATOM classAtom = 0;
    std::uint64_t ruleSetVersion = 0;
    // Kept so a new rule set re-classifies without reading Windows under the lock.
    WindowIdentity identity;
    bool hasTopLevel = false;  // false: the window is itself top-level
    HWND topLevelHwnd = nullptr;
    WindowIdentity topLevel;
    PageList pages;  // the top-level window's pages; null when it is not a known sheet
};

struct State {
    SRWLOCK lock = SRWLOCK_INIT;
    std::shared_ptr<const RuleSet> rules;  // null until the first SetRuleSet
    std::unordered_map<HWND, Entry> cache;
    // Bumped when a sheet's pages change: an entry built from pages read before the change
    // is not cached.
    std::uint64_t pagesEpoch = 0;
};

State g_state;
ListenerList<HWND, Level, Level> g_listeners;

struct LevelChange {
    HWND hwnd;
    Level oldLevel;
    Level newLevel;
};

WindowClassification Failure() {
    WindowClassification result;
    result.reason = Reason::Failure;
    return result;
}

// The pages of `topLevel` when the dialog hooks saw it as a property sheet, else null. An
// incomplete list gets one page with every field Unknown: no rule matches it, so it lowers
// the sheet to Native (FR-013a, FR-018). Reads the dialog store: call without engine locks.
PageList PagesOf(HWND topLevel) {
    SheetPages sheet = SheetPagesOf(topLevel);
    if (!sheet.known) {
        return nullptr;
    }
    if (sheet.incomplete) {
        sheet.pages.emplace_back();
    }
    return std::make_shared<const std::vector<WindowIdentity>>(std::move(sheet.pages));
}

WindowClassification Evaluate(const Entry& entry, const RuleSet& rules) {
    const WindowIdentity* topLevel = entry.hasTopLevel ? &entry.topLevel : nullptr;
    std::span<const WindowIdentity> pages;
    if (entry.pages) {
        pages = *entry.pages;
    }
    Classification pure = Classify(entry.identity, topLevel, pages, rules);
    WindowClassification result;
    result.level = pure.level;
    result.ownLevel = pure.ownLevel;
    result.reason = pure.reason;
    if (pure.rule) {
        result.ruleSource = pure.rule->source;
        result.ruleOrigin = pure.rule->origin;
    }
    return result;
}

const wchar_t* LevelText(Level level) {
    switch (level) {
        case Level::Native: return L"native";
        case Level::Visual: return L"visual";
        case Level::Full: return L"full";
    }
    return L"?";
}

const wchar_t* ReasonText(Reason reason) {
    switch (reason) {
        case Reason::Default: return L"default";
        case Reason::Rule: return L"rule";
        case Reason::Locked: return L"locked";
        case Reason::CappedByTopLevel: return L"capped by top-level";
        case Reason::LoweredByPage: return L"lowered by page";
        case Reason::Failure: return L"failure";
    }
    return L"?";
}

const wchar_t* IdentityFieldText(const Field& field) {
    return field.known ? field.value.c_str() : L"?";
}

const wchar_t* IdentityTemplateText(const TemplateField& field) {
    switch (field.kind) {
        case TemplateField::Kind::Value: return field.value.c_str();
        case TemplateField::Kind::Unknown: return L"?";
        case TemplateField::Kind::NotADialog: return L"-";
    }
    return L"?";
}

// One line per classification while diagnostic mode is on (FR-028). Never a title (FR-010).
void LogClassification(HWND hwnd, const Entry& entry) {
    if (!log::IsVerbose()) {
        return;
    }
    const WindowClassification& result = entry.result;
    const WindowIdentity& id = entry.identity;
    bool hasRule = !result.ruleOrigin.empty();
    log::Verbose(L"compat",
                 L"window %p: %s (own %s, %s%s%s%s); %s %s %s template %s; top-level %p; "
                 L"%u pages",
                 hwnd, LevelText(result.level), LevelText(result.ownLevel),
                 ReasonText(result.reason), hasRule ? L" \"" : L"", result.ruleOrigin.c_str(),
                 hasRule ? L"\"" : L"", IdentityFieldText(id.process),
                 IdentityFieldText(id.module), IdentityFieldText(id.windowClass),
                 IdentityTemplateText(id.templateId),
                 entry.hasTopLevel ? entry.topLevelHwnd : hwnd,
                 entry.pages ? static_cast<unsigned>(entry.pages->size()) : 0u);
}

// Tells listeners of each change. Call after the engine lock is released.
void NotifyChanges(const std::vector<LevelChange>& changes, const wchar_t* cause) {
    for (const LevelChange& change : changes) {
        log::Verbose(L"compat", L"window %p: %s -> %s (%s)", change.hwnd,
                     LevelText(change.oldLevel), LevelText(change.newLevel), cause);
        g_listeners.Notify(change.hwnd, change.oldLevel, change.newLevel);
    }
}

// Caller holds the exclusive lock. IsWindow only reads the handle table.
void SweepDestroyedWindows() {
    std::erase_if(g_state.cache, [](const auto& item) { return !IsWindow(item.first); });
}

WindowClassification ClassifyOrThrow(HWND hwnd) {
    if (!hwnd) {
        log::Warn(L"compat", L"classification failed: null window");
        return Failure();
    }
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    auto classAtom = static_cast<ATOM>(GetClassLongPtrW(hwnd, GCW_ATOM));
    if (threadId == 0) {
        log::Warn(L"compat", L"window %p: classification failed: invalid window", hwnd);
        return Failure();
    }

    std::shared_ptr<const RuleSet> rules;
    std::uint64_t pagesEpoch = 0;
    {
        SharedLock lock(g_state.lock);
        rules = g_state.rules;
        pagesEpoch = g_state.pagesEpoch;
        if (!rules) {
            return WindowClassification{};
        }
        auto found = g_state.cache.find(hwnd);
        if (found != g_state.cache.end() && found->second.threadId == threadId &&
            found->second.classAtom == classAtom &&
            found->second.ruleSetVersion == rules->version) {
            return found->second.result;
        }
    }

    // A child is linked to its parent only after HCBT_CREATEWND / WM_NCCREATE; until then it
    // looks top-level and would escape its top-level window's cap. Native for now, not cached:
    // an expected case, so a verbose line rather than a warning.
    HWND topLevel = TopLevelOf(hwnd);
    if (topLevel == hwnd && (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD)) {
        log::Verbose(L"compat", L"window %p: native (child not linked to its parent yet)", hwnd);
        return Failure();
    }

    // Read Windows and the dialog store outside the lock: module names take the loader lock,
    // and the engine never holds its lock while taking the store's.
    Entry entry;
    entry.threadId = threadId;
    entry.classAtom = classAtom;
    entry.ruleSetVersion = rules->version;
    entry.identity = ReadIdentity(hwnd);
    if (!topLevel) {
        entry.identity.failed = true;
    } else if (topLevel != hwnd) {
        entry.hasTopLevel = true;
        entry.topLevelHwnd = topLevel;
        entry.topLevel = ReadIdentity(topLevel);
    }
    if (topLevel) {
        entry.pages = PagesOf(topLevel);
    }
    entry.result = Evaluate(entry, *rules);
    if (entry.result.reason == Reason::Failure) {
        // Not cached: the window or its top-level window could not be read.
        log::Warn(L"compat", L"window %p: classification failed: unreadable window", hwnd);
        return entry.result;
    }

    LogClassification(hwnd, entry);
    WindowClassification result = entry.result;
    ExclusiveLock lock(g_state.lock);
    // A newer rule set, or a page added to a sheet meanwhile, would make this entry stale:
    // the next query classifies the window again.
    if (g_state.rules == rules && g_state.pagesEpoch == pagesEpoch) {
        if (g_state.cache.size() >= kSweepThreshold) {
            SweepDestroyedWindows();
        }
        g_state.cache.insert_or_assign(hwnd, std::move(entry));
    }
    return result;
}

}  // namespace

WindowClassification Classify(HWND hwnd) noexcept {
    try {
        return ClassifyOrThrow(hwnd);
    } catch (...) {
        log::Warn(L"compat", L"window %p: classification failed: exception", hwnd);
        return Failure();
    }
}

Level LevelOf(HWND hwnd) noexcept {
    return Classify(hwnd).level;
}

SubscriptionId SubscribeLevelChanged(LevelChangedFn fn, void* context) {
    return g_listeners.Add(fn, context);
}

void Unsubscribe(SubscriptionId id) {
    g_listeners.Remove(id);
}

void SetRuleSet(RuleSet rules) {
    std::vector<LevelChange> changes;
    std::shared_ptr<const RuleSet> previous;  // released after the lock
    try {
        auto next = std::make_shared<const RuleSet>(std::move(rules));
        ExclusiveLock lock(g_state.lock);
        previous = std::exchange(g_state.rules, next);
        SweepDestroyedWindows();
        for (auto& [hwnd, entry] : g_state.cache) {
            Level oldLevel = entry.result.level;
            entry.result = Evaluate(entry, *next);
            entry.ruleSetVersion = next->version;
            if (entry.result.level != oldLevel) {
                changes.push_back(LevelChange{hwnd, oldLevel, entry.result.level});
            }
        }
    } catch (...) {
        // Out of memory mid-update: forget every cached window so the next query classifies
        // it against whichever rule set is current. Listeners are not told (FR-018).
        log::Warn(L"compat", L"rule set update failed: exception; cache cleared");
        std::unordered_map<HWND, Entry> dropped;
        ExclusiveLock lock(g_state.lock);
        dropped.swap(g_state.cache);
        return;
    }
    NotifyChanges(changes, L"settings changed");
}

void OnSheetPagesChanged(HWND sheet) noexcept {
    std::vector<LevelChange> changes;
    try {
        PageList pages = PagesOf(sheet);  // before the engine lock (lock order)
        ExclusiveLock lock(g_state.lock);
        g_state.pagesEpoch++;
        if (!g_state.rules) {
            return;
        }
        for (auto& [hwnd, entry] : g_state.cache) {
            bool isSheet = hwnd == sheet && !entry.hasTopLevel;
            bool inSheet = entry.hasTopLevel && entry.topLevelHwnd == sheet;
            if (!isSheet && !inSheet) {
                continue;
            }
            entry.pages = pages;
            Level oldLevel = entry.result.level;
            entry.result = Evaluate(entry, *g_state.rules);
            if (entry.result.level != oldLevel) {
                changes.push_back(LevelChange{hwnd, oldLevel, entry.result.level});
            }
        }
    } catch (...) {
        // Out of memory mid-update: forget the sheet and its children so the next query
        // classifies them again. Listeners are not told (FR-018).
        log::Warn(L"compat", L"sheet %p: page update failed: exception; windows forgotten", sheet);
        ExclusiveLock lock(g_state.lock);
        std::erase_if(g_state.cache, [sheet](const auto& item) {
            return item.first == sheet || item.second.topLevelHwnd == sheet;
        });
        g_state.pagesEpoch++;
        return;
    }
    NotifyChanges(changes, L"sheet page added");
}

void ResetEngine() {
    std::shared_ptr<const RuleSet> rules;
    std::unordered_map<HWND, Entry> cache;
    {
        ExclusiveLock lock(g_state.lock);
        rules.swap(g_state.rules);
        cache.swap(g_state.cache);
    }
    g_listeners.Clear();
}

}  // namespace wholeui::compat
