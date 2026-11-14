// PURE: C++ standard library only (compiled into the mod and the unit tests).

#include "overrides.h"

namespace wholeui::settings {

namespace {

using compat::EqualsIgnoreCase;
using compat::Level;
using compat::Rule;

bool IsSpace(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\v' || c == L'\f';
}

std::wstring_view Trim(std::wstring_view text) {
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    while (!text.empty() && IsSpace(text.back())) text.remove_suffix(1);
    return text;
}

std::wstring Quoted(std::wstring_view text) {
    return L"\"" + std::wstring(text) + L"\"";
}

std::optional<Level> ParseLevel(std::wstring_view value) {
    if (EqualsIgnoreCase(value, L"native")) return Level::Native;
    if (EqualsIgnoreCase(value, L"visual")) return Level::Visual;
    if (EqualsIgnoreCase(value, L"full")) return Level::Full;
    return std::nullopt;
}

bool IsPath(std::wstring_view value) {
    return value.find_first_of(L"/\\:") != std::wstring_view::npos;
}

// Every field `general` names is named by `specific` with an equal value.
bool Covers(const std::optional<std::wstring>& specific,
            const std::optional<std::wstring>& general) {
    return !general || (specific && EqualsIgnoreCase(*specific, *general));
}

}  // namespace

OverrideResult ParseOverride(int index, std::wstring_view text) {
    std::wstring_view entry = Trim(text);
    if (entry.empty()) {
        return {};
    }
    auto fail = [&](std::wstring reason) {
        return OverrideResult{std::nullopt, OverrideError{index, std::wstring(entry),
                                                          std::move(reason)}};
    };

    Rule rule;
    rule.source = compat::RuleSource::Override;
    rule.origin = std::wstring(entry);
    bool hasLevel = false;

    std::wstring_view rest = entry;
    while (true) {
        size_t end = rest.find(L';');
        std::wstring_view segment = Trim(rest.substr(0, end));
        if (!segment.empty()) {
            size_t equals = segment.find(L'=');
            std::wstring_view name;
            if (equals != std::wstring_view::npos) {
                name = Trim(segment.substr(0, equals));
            }
            if (name.empty()) {
                return fail(L"malformed field " + Quoted(segment));
            }
            std::wstring_view value = Trim(segment.substr(equals + 1));

            std::optional<std::wstring>* slot = nullptr;
            bool isLevel = false;
            if (EqualsIgnoreCase(name, L"process")) slot = &rule.process;
            else if (EqualsIgnoreCase(name, L"module")) slot = &rule.module;
            else if (EqualsIgnoreCase(name, L"class")) slot = &rule.windowClass;
            else if (EqualsIgnoreCase(name, L"template")) slot = &rule.templateId;
            else if (EqualsIgnoreCase(name, L"level")) isLevel = true;
            else return fail(L"unknown field " + Quoted(name));

            if (isLevel ? hasLevel : slot->has_value()) {
                return fail(L"duplicate field " + Quoted(name));
            }
            if (value.empty()) {
                return fail(L"empty value for " + Quoted(name));
            }
            if (isLevel) {
                std::optional<Level> level = ParseLevel(value);
                if (!level) {
                    return fail(L"unknown level");
                }
                rule.level = *level;
                hasLevel = true;
            } else {
                if ((slot == &rule.process || slot == &rule.module) && IsPath(value)) {
                    return fail(L"path not allowed in " + Quoted(name));
                }
                *slot = std::wstring(value);
            }
        }
        if (end == std::wstring_view::npos) {
            break;
        }
        rest.remove_prefix(end + 1);
    }

    if (!hasLevel) {
        return fail(L"missing level");
    }
    if (!rule.process && !rule.module && !rule.windowClass && !rule.templateId) {
        return fail(L"no identity field");
    }
    return OverrideResult{std::move(rule), std::nullopt};
}

bool NamesShellSurface(const Rule& candidate, std::span<const Rule> builtIns) {
    for (const Rule& locked : builtIns) {
        if (locked.locked && Covers(candidate.process, locked.process) &&
            Covers(candidate.module, locked.module) &&
            Covers(candidate.windowClass, locked.windowClass) &&
            Covers(candidate.templateId, locked.templateId)) {
            return true;
        }
    }
    return false;
}

}  // namespace wholeui::settings
