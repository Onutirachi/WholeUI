#include "engine.h"

#include <cwchar>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/lock.h"
#include "../core/log.h"
#include "identity.h"

namespace wholeui::compat {

namespace {

// Past this many entries, an insert first drops destroyed windows (research R6).
constexpr size_t kSweepThreshold = 4096;

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
    // The hosted pages between the window and its top-level window, nearest first.
    std::vector<WindowIdentity> pages;
    HWND nearestPage = nullptr;  // null when there is no page
};

struct State {
    SRWLOCK lock = SRWLOCK_INIT;
    std::shared_ptr<const RuleSet> rules;  // null until the first SetRuleSet
    std::unordered_map<HWND, Entry> cache;
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

WindowClassification Evaluate(const Entry& entry, const RuleSet& rules) {
    const WindowIdentity* topLevel = entry.hasTopLevel ? &entry.topLevel : nullptr;
    return CopyOf(Classify(entry.identity, entry.pages, topLevel, rules));
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
        case Reason::CappedByPage: return L"capped by page";
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

// "; page <hwnd> (<module> template <id>), N pages" for the nearest page, "" without one.
std::wstring PageText(const Entry& entry) {
    if (entry.pages.empty()) {
        return {};
    }
    const WindowIdentity& page = entry.pages.front();
    wchar_t text[64];
    swprintf_s(text, L"; page %p (", entry.nearestPage);
    std::wstring result = text;
    result += IdentityFieldText(page.module);
    result += L" template ";
    result += IdentityTemplateText(page.templateId);
    swprintf_s(text, L"), %u page%s", static_cast<unsigned>(entry.pages.size()),
               entry.pages.size() == 1 ? L"" : L"s");
    result += text;
    return result;
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
                 L"window %p: %s (own %s, %s%s%s%s); %s %s %s template %s; top-level %p%s",
                 hwnd, LevelText(result.level), LevelText(result.ownLevel),
                 ReasonText(result.reason), hasRule ? L" \"" : L"", result.ruleOrigin.c_str(),
                 hasRule ? L"\"" : L"", IdentityFieldText(id.process),
                 IdentityFieldText(id.module), IdentityFieldText(id.windowClass),
                 IdentityTemplateText(id.templateId),
                 entry.hasTopLevel ? entry.topLevelHwnd : hwnd, PageText(entry).c_str());
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
    {
        SharedLock lock(g_state.lock);
        rules = g_state.rules;
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

    // Read Windows (window, hosted pages, top-level) and the dialog store outside the lock:
    // module names take the loader lock, and the engine never holds its lock while taking
    // the store's.
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
        std::vector<HWND> pages = HostedPagesOf(hwnd, topLevel);
        if (!pages.empty()) {
            entry.nearestPage = pages.front();
        }
        entry.pages.reserve(pages.size());
        for (HWND page : pages) {
            entry.pages.push_back(ReadIdentity(page));
        }
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
    // A newer rule set meanwhile would make this entry stale: the next query classifies the
    // window again.
    if (g_state.rules == rules) {
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

WindowClassification ClassifyIdentity(const WindowIdentity& window,
                                      std::span<const WindowIdentity> pages,
                                      const WindowIdentity* topLevel) noexcept {
    try {
        std::shared_ptr<const RuleSet> rules;
        {
            SharedLock lock(g_state.lock);
            rules = g_state.rules;
        }
        if (!rules) {
            return WindowClassification{};
        }
        // The shared_ptr keeps the rule set alive while CopyOf reads the matched rule.
        return CopyOf(Classify(window, pages, topLevel, *rules));
    } catch (...) {
        log::Warn(L"compat", L"identity classification failed: exception");
        return Failure();
    }
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
