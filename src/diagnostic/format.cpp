// PURE: C++ standard library only (compiled into the mod and the unit tests).

#include "format.h"

namespace wholeui::diagnostic {

using compat::Field;
using compat::Level;
using compat::Reason;
using compat::RuleSource;
using compat::TemplateField;
using compat::WindowClassification;
using compat::WindowIdentity;

namespace {

constexpr std::wstring_view kNewLine = L"\r\n";
constexpr std::wstring_view kSeparator = L"  ·  ";  // two spaces, middle dot, two spaces
constexpr std::wstring_view kDash = L"—";            // em dash: "not named" in the table
constexpr std::wstring_view kUnknown = L"unknown";

bool Readable(const Field& field) { return field.known && !field.value.empty(); }

std::wstring_view Shown(const Field& field) {
    return Readable(field) ? std::wstring_view(field.value) : kUnknown;
}

std::wstring_view ShownTemplate(const TemplateField& templateId) {
    switch (templateId.kind) {
        case TemplateField::Kind::Value:
            return templateId.value.empty() ? kUnknown : std::wstring_view(templateId.value);
        case TemplateField::Kind::NotADialog: return L"not a dialog";
        case TemplateField::Kind::Unknown: break;
    }
    return kUnknown;
}

bool ReadableTemplate(const TemplateField& templateId) {
    return templateId.kind == TemplateField::Kind::Value && !templateId.value.empty();
}

// "<class>  ·  <module>  ·  template <id>"
std::wstring IdentityText(const WindowIdentity& identity) {
    std::wstring text(Shown(identity.windowClass));
    text += kSeparator;
    text += Shown(identity.module);
    text += kSeparator;
    text += L"template ";
    text += ShownTemplate(identity.templateId);
    return text;
}

std::wstring ReasonText(const WindowClassification& classification) {
    switch (classification.reason) {
        case Reason::Default: return L"default";
        case Reason::Rule:
            return classification.ruleSource == RuleSource::Override ? L"override" : L"rule";
        case Reason::Locked: return L"shell surface";
        case Reason::CappedByTopLevel: return L"capped by top-level";
        case Reason::CappedByPage: return L"capped by page";
        case Reason::Failure: return L"failure";
    }
    return L"failure";
}

void AppendLine(std::wstring& text, std::wstring_view label, std::wstring_view value) {
    text += label;
    text += L'\t';
    text += value;
    text += kNewLine;
}

void AppendOverrideField(std::wstring& line, std::wstring_view name, std::wstring_view value) {
    line += name;
    line += L'=';
    line += value;
    line += L"; ";
}

void AppendCell(std::wstring& row, std::wstring_view value) {
    row += L' ';
    row += value.empty() ? kDash : value;
    row += L" |";
}

std::wstring_view CellOf(const Field& field) {
    return Readable(field) ? std::wstring_view(field.value) : kDash;
}

}  // namespace

const wchar_t* LevelText(Level level) noexcept {
    switch (level) {
        case Level::Native: return L"native";
        case Level::Visual: return L"visual";
        case Level::Full: return L"full";
    }
    return L"native";
}

std::wstring OverlayText(const WindowIdentity& window, const WindowIdentity& topLevel,
                         const WindowClassification& classification, bool inProcess) {
    std::wstring text;
    AppendLine(text, L"Window", IdentityText(window));
    AppendLine(text, L"Top-level", IdentityText(topLevel));

    std::wstring process(Shown(window.process));
    if (!inProcess) {
        process += L"  (not running WholeUI)";
    }
    AppendLine(text, L"Process", process);

    std::wstring level(LevelText(classification.level));
    level += kSeparator;
    level += ReasonText(classification);
    if (!classification.ruleOrigin.empty()) {
        level += kSeparator;
        level += classification.ruleOrigin;
    }
    AppendLine(text, L"Level", level);

    text += L"Ctrl+Alt+Shift+C copies";
    return text;
}

std::wstring OverrideLine(const WindowIdentity& topLevel, Level level) {
    std::wstring line;
    if (Readable(topLevel.process)) {
        AppendOverrideField(line, L"process", topLevel.process.value);
    }
    if (Readable(topLevel.module)) {
        AppendOverrideField(line, L"module", topLevel.module.value);
    }
    if (Readable(topLevel.windowClass)) {
        AppendOverrideField(line, L"class", topLevel.windowClass.value);
    }
    if (ReadableTemplate(topLevel.templateId)) {
        AppendOverrideField(line, L"template", topLevel.templateId.value);
    }
    line += L"level=";
    line += LevelText(level);
    return line;
}

std::wstring TableRow(const WindowIdentity& topLevel, Level level, std::wstring_view build) {
    std::wstring row = L"|";
    AppendCell(row, L"<name>");
    AppendCell(row, CellOf(topLevel.process));
    AppendCell(row, CellOf(topLevel.module));
    AppendCell(row, CellOf(topLevel.windowClass));
    AppendCell(row, ReadableTemplate(topLevel.templateId)
                        ? std::wstring_view(topLevel.templateId.value)
                        : kDash);
    AppendCell(row, LevelText(level));
    AppendCell(row, build);
    AppendCell(row, kDash);  // Validated: filled in by the maintainer
    return row;
}

std::wstring BuildText(const std::optional<std::wstring>& currentBuild,
                       std::optional<std::uint32_t> ubr) {
    if (!currentBuild || currentBuild->empty() || !ubr) {
        return {};
    }
    return *currentBuild + L'.' + std::to_wstring(*ubr);
}

std::wstring ClipboardText(const WindowIdentity& topLevel, Level level, std::wstring_view build) {
    std::wstring text = OverrideLine(topLevel, level);
    text += kNewLine;
    text += TableRow(topLevel, level, build);
    return text;
}

}  // namespace wholeui::diagnostic
