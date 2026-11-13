// PURE: C++ standard library only (compiled into the mod and the unit tests).
//
// Compatibility rules and the pure classifier (data-model.md "Compatibility rule",
// "Resolution order"). The engine (engine.h) reads window identities and caches the results;
// this file only decides a level from identities and rules.

#ifndef WHOLEUI_COMPAT_RULES_H
#define WHOLEUI_COMPAT_RULES_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wholeui::compat {

// Ordered: a lower level touches less. min() of two levels is the safer one.
enum class Level : std::uint8_t { Native, Visual, Full };

enum class Reason : std::uint8_t {
    Default,           // no rule matched
    Rule,              // the winning rule's level
    Locked,            // the top-level window is a shell surface (FR-015)
    CappedByTopLevel,  // a child lowered to its top-level window's level (FR-013)
    LoweredByPage,     // a property sheet lowered to its lowest page (FR-013a)
    Failure,           // an identity read failed (FR-018)
};

enum class RuleSource : std::uint8_t { BuiltIn, Override };

// A process, module or class name, or Unknown when it could not be read. An Unknown field
// never matches a rule that names it.
struct Field {
    bool known = false;
    std::wstring value;

    static Field Of(std::wstring value) { return Field{true, std::move(value)}; }
    static Field Unknown() { return Field{}; }
};

// A dialog template ID: a value, Unknown (a dialog whose template was not seen), or
// NotADialog. Only a value matches a rule that names a template.
struct TemplateField {
    enum class Kind : std::uint8_t { Value, Unknown, NotADialog };

    Kind kind = Kind::Unknown;
    std::wstring value;

    static TemplateField Of(std::wstring value) {
        return TemplateField{Kind::Value, std::move(value)};
    }
    static TemplateField Unknown() { return TemplateField{}; }
    static TemplateField NotADialog() { return TemplateField{Kind::NotADialog, {}}; }
};

struct WindowIdentity {
    Field process;     // file name, no path
    Field module;      // file name of the creating module, no path
    Field windowClass;
    TemplateField templateId;
    bool failed = false;  // a read failed abnormally (not a plain Unknown): step 1
};

// A built-in row of docs/compatibility.md or a parsed override. At least one of the four
// identity fields is named; a named field must equal the window's (case-insensitive).
struct Rule {
    std::optional<std::wstring> process;
    std::optional<std::wstring> module;
    std::optional<std::wstring> windowClass;
    std::optional<std::wstring> templateId;
    Level level = Level::Native;
    RuleSource source = RuleSource::BuiltIn;
    bool locked = false;  // shell surface: matched against the top-level window, always Native
    std::wstring origin;  // window name for a built-in, the entry text for an override
};

// Immutable once published by the engine; a settings change builds a new one.
struct RuleSet {
    std::vector<Rule> builtIns;
    std::vector<Rule> overrides;
    std::uint64_t version = 0;
};

struct Classification {
    Level level = Level::Native;     // effective level
    Level ownLevel = Level::Native;  // level from the window's own rules (step 3)
    Reason reason = Reason::Default;
    const Rule* rule = nullptr;      // the winning rule (the locked one for Locked), if any
    const WindowIdentity* page = nullptr;  // LoweredByPage: the first lowest page (into `pages`)
};

// A copy of Classification that stays valid after the rule set and pages are replaced
// (returned by the engine, shown by diagnostic mode).
struct WindowClassification {
    Level level = Level::Native;     // effective level
    Level ownLevel = Level::Native;  // before the top-level and page caps
    Reason reason = Reason::Default;
    RuleSource ruleSource = RuleSource::BuiltIn;  // meaningful only when ruleOrigin is set
    std::wstring ruleOrigin;                      // empty when no rule
    Field pageModule;                             // LoweredByPage: the lowering page's module
};

// Copies `pure` (its rule and page may then be released).
WindowClassification CopyOf(const Classification& pure);

// Classifies `window` (data-model.md "Resolution order", steps 1 to 5). `topLevel` is the
// window's top-level window, or nullptr when `window` is itself top-level. `pages` are the
// property sheet pages of the top-level window, if it is a sheet. The returned rule pointer
// points into `rules`.
Classification Classify(const WindowIdentity& window, const WindowIdentity* topLevel,
                        std::span<const WindowIdentity> pages, const RuleSet& rules) noexcept;

// The rules generated from docs/compatibility.md, in document order.
const std::vector<Rule>& BuiltInRules();

// Case-insensitive comparison for names in rules (ASCII letters fold; other characters must
// be equal). Process and module names are read lower-cased, so this only matters for typed
// values.
bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept;

}  // namespace wholeui::compat

#endif  // WHOLEUI_COMPAT_RULES_H
