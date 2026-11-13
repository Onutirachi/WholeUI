#include "engine.h"

#include <memory>
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
    Classification pure = Classify(entry.identity, topLevel, {}, rules);
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

    // Read Windows outside the lock: module names take the loader lock.
    Entry entry;
    entry.threadId = threadId;
    entry.classAtom = classAtom;
    entry.ruleSetVersion = rules->version;
    entry.identity = ReadIdentity(hwnd);
    HWND topLevel = TopLevelOf(hwnd);
    if (!topLevel) {
        entry.identity.failed = true;
    } else if (topLevel != hwnd) {
        entry.hasTopLevel = true;
        entry.topLevelHwnd = topLevel;
        entry.topLevel = ReadIdentity(topLevel);
    }
    entry.result = Evaluate(entry, *rules);
    if (entry.result.reason == Reason::Failure) {
        // Not cached: the window or its top-level window could not be read.
        log::Warn(L"compat", L"window %p: classification failed: unreadable window", hwnd);
        return entry.result;
    }

    WindowClassification result = entry.result;
    ExclusiveLock lock(g_state.lock);
    if (g_state.rules == rules) {  // a newer rule set would make this entry stale
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
    for (const LevelChange& change : changes) {
        g_listeners.Notify(change.hwnd, change.oldLevel, change.newLevel);
    }
}

void OnSheetPagesChanged(HWND sheet) noexcept {
    std::vector<LevelChange> changes;
    try {
        ExclusiveLock lock(g_state.lock);
        if (!g_state.rules) {
            return;
        }
        for (auto& [hwnd, entry] : g_state.cache) {
            if (hwnd != sheet && entry.topLevelHwnd != sheet) {
                continue;
            }
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
        return;
    }
    for (const LevelChange& change : changes) {
        g_listeners.Notify(change.hwnd, change.oldLevel, change.newLevel);
    }
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
