// Logging through Windhawk's log (contracts/engine-api.md "Logging", FR-028).
//
// Each line is "[area] message". `area` names the module that writes it, for example
// L"compat" or L"tokens". Info and Warn always write; Verbose writes only while verbose
// logging is on (diagnostic mode), so normal operation stays quiet.
//
// Format strings follow the Windows wide printf rules: %s is a wide string.

#ifndef WHOLEUI_CORE_LOG_H
#define WHOLEUI_CORE_LOG_H

namespace wholeui::log {

void Info(const wchar_t* area, const wchar_t* fmt, ...);     // startup, settings, tokens
void Warn(const wchar_t* area, const wchar_t* fmt, ...);     // ignored input, fallbacks, failures
void Verbose(const wchar_t* area, const wchar_t* fmt, ...);  // per-window lines

// Turns Verbose on or off. Safe from any thread.
void SetVerbose(bool on);
bool IsVerbose();

}  // namespace wholeui::log

#endif  // WHOLEUI_CORE_LOG_H
