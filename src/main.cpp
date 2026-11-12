// WholeUI: the root of the amalgamation. tools/build.py inlines each project include below,
// in this order, into build/wholeui.wh.cpp (research R4). Never edit that file by hand.
//
// Quoted includes are relative to the including file. A module's .cpp follows its .h.

#include "core/log.h"
#include "core/log.cpp"
#include "core/subscriptions.h"

BOOL Wh_ModInit() {
    wholeui::log::Info(L"core", L"WholeUI %s loaded", WH_MOD_VERSION);
    return TRUE;
}

void Wh_ModSettingsChanged() {}

void Wh_ModUninit() {}
