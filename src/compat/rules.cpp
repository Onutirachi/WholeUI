// PURE: C++ standard library only (compiled into the mod and the unit tests).

#include "rules.h"

#include <algorithm>

namespace wholeui::compat {

namespace {

wchar_t FoldAscii(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

bool Matches(const std::optional<std::wstring>& named, const Field& field) {
    return !named || (field.known && EqualsIgnoreCase(*named, field.value));
}

bool Matches(const std::optional<std::wstring>& named, const TemplateField& field) {
    return !named ||
           (field.kind == TemplateField::Kind::Value && EqualsIgnoreCase(*named, field.value));
}

bool Matches(const Rule& rule, const WindowIdentity& window) {
    return Matches(rule.process, window.process) && Matches(rule.module, window.module) &&
           Matches(rule.windowClass, window.windowClass) &&
           Matches(rule.templateId, window.templateId);
}

int Specificity(const Rule& rule) {
    return (rule.process ? 1 : 0) + (rule.module ? 1 : 0) + (rule.windowClass ? 1 : 0) +
           (rule.templateId ? 1 : 0);
}

// True when `candidate` beats `best` (step 3): more named fields, then Override over
// BuiltIn, then the lower level. A full tie keeps the first rule found.
bool Beats(const Rule& candidate, const Rule& best) {
    int a = Specificity(candidate), b = Specificity(best);
    if (a != b) {
        return a > b;
    }
    if (candidate.source != best.source) {
        return candidate.source == RuleSource::Override;
    }
    return candidate.level < best.level;
}

// Step 3: the winning rule for the window, or nullptr.
const Rule* OwnRule(const WindowIdentity& window, const RuleSet& rules) {
    const Rule* best = nullptr;
    for (const std::vector<Rule>* list : {&rules.builtIns, &rules.overrides}) {
        for (const Rule& rule : *list) {
            if (Matches(rule, window) && (!best || Beats(rule, *best))) {
                best = &rule;
            }
        }
    }
    return best;
}

Level OwnLevel(const WindowIdentity& window, const RuleSet& rules) {
    const Rule* rule = OwnRule(window, rules);
    return rule ? rule->level : Level::Native;
}

// Step 2: the locked rule matching the top-level window, or nullptr.
const Rule* LockedRule(const WindowIdentity& topLevel, const RuleSet& rules) {
    for (const Rule& rule : rules.builtIns) {
        if (rule.locked && Matches(rule, topLevel)) {
            return &rule;
        }
    }
    return nullptr;
}

// Step 4: the top-level window's level lowered to its lowest page. `lowest` receives the
// first page with that level when it is below `level`, else nullptr.
Level LoweredByPages(Level level, std::span<const WindowIdentity> pages, const RuleSet& rules,
                     const WindowIdentity** lowest = nullptr) {
    if (lowest) {
        *lowest = nullptr;
    }
    for (const WindowIdentity& page : pages) {
        Level pageLevel = OwnLevel(page, rules);
        if (pageLevel < level) {
            level = pageLevel;
            if (lowest) {
                *lowest = &page;
            }
        }
    }
    return level;
}

Classification NativeBecause(Reason reason, const Rule* rule = nullptr) {
    return Classification{Level::Native, Level::Native, reason, rule};
}

}  // namespace

bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (FoldAscii(a[i]) != FoldAscii(b[i])) {
            return false;
        }
    }
    return true;
}

Classification Classify(const WindowIdentity& window, const WindowIdentity* topLevel,
                        std::span<const WindowIdentity> pages, const RuleSet& rules) noexcept {
    // Step 1: a failed read anywhere the result depends on is fail-safe Native.
    const WindowIdentity& top = topLevel ? *topLevel : window;
    if (window.failed || top.failed ||
        std::any_of(pages.begin(), pages.end(),
                    [](const WindowIdentity& page) { return page.failed; })) {
        return NativeBecause(Reason::Failure);
    }

    // Step 2: shell surfaces, and everything inside them.
    if (const Rule* locked = LockedRule(top, rules)) {
        return NativeBecause(Reason::Locked, locked);
    }

    // Step 3: the window's own level.
    Classification result;
    result.rule = OwnRule(window, rules);
    result.ownLevel = result.rule ? result.rule->level : Level::Native;
    result.level = result.ownLevel;
    result.reason = result.rule ? Reason::Rule : Reason::Default;

    if (!topLevel) {
        // Step 4: a sheet is no higher than its lowest page.
        Level lowered = LoweredByPages(result.ownLevel, pages, rules, &result.page);
        if (lowered < result.ownLevel) {
            result.level = lowered;
            result.reason = Reason::LoweredByPage;
        }
        return result;
    }

    // Step 5: a child is no higher than its top-level window's effective level.
    Level cap = LoweredByPages(OwnLevel(top, rules), pages, rules);
    if (cap < result.ownLevel) {
        result.level = cap;
        result.reason = Reason::CappedByTopLevel;
    }
    return result;
}

WindowClassification CopyOf(const Classification& pure) {
    WindowClassification result;
    result.level = pure.level;
    result.ownLevel = pure.ownLevel;
    result.reason = pure.reason;
    if (pure.rule) {
        result.ruleSource = pure.rule->source;
        result.ruleOrigin = pure.rule->origin;
    }
    if (pure.page) {
        result.pageModule = pure.page->module;
    }
    return result;
}

namespace {

struct BuiltInRow {
    const wchar_t* origin;
    const wchar_t* process;
    const wchar_t* module;
    const wchar_t* windowClass;
    const wchar_t* templateId;
    Level level;
    bool locked;
};

constexpr BuiltInRow kBuiltInRows[] = {
#include "../../build/gen/builtin_rules.inc"
};

std::optional<std::wstring> Named(const wchar_t* value) {
    return value ? std::optional<std::wstring>(value) : std::nullopt;
}

}  // namespace

const std::vector<Rule>& BuiltInRules() {
    static const std::vector<Rule> rules = [] {
        std::vector<Rule> out;
        for (const BuiltInRow& row : kBuiltInRows) {
            Rule rule;
            rule.process = Named(row.process);
            rule.module = Named(row.module);
            rule.windowClass = Named(row.windowClass);
            rule.templateId = Named(row.templateId);
            rule.level = row.level;
            rule.source = RuleSource::BuiltIn;
            rule.locked = row.locked;
            rule.origin = row.origin;
            out.push_back(std::move(rule));
        }
        return out;
    }();
    return rules;
}

}  // namespace wholeui::compat
