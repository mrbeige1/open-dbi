// console_menu.cpp - console menu shell implementation. See console_menu.h.
#include "console_menu.h"
#include "../build_version.h"
#include <cstdio>

namespace dbi::ui {

// ANSI escapes the libnx console VT parser understands.
static const char* CLEAR   = "\x1b[2J\x1b[H"; // clear + home
static const char* INVERT  = "\x1b[7m";        // reverse video (selection)
static const char* RED     = "\x1b[31m";       // destructive marker
static const char* RESET   = "\x1b[0m";

// Pump one frame of console output while spinning on input. Returns the buttons
// pressed this frame (0 if none / app is exiting).
static u64 pollOnce(PadState& pad) {
    if (!appletMainLoop()) return 0;
    consoleUpdate(NULL);
    padUpdate(&pad);
    return padGetButtonsDown(&pad);
}

bool confirmPrompt(PadState& pad, const char* label) {
    printf("\n%s%s%s\n", RED, label ? label : "", RESET);
    printf("%sConfirm?  A = yes   B = no%s\n", RED, RESET);
    consoleUpdate(NULL);
    while (appletMainLoop()) {
        u64 down = pollOnce(pad);
        if (down & HidNpadButton_A) return true;
        if (down & HidNpadButton_B) return false;
    }
    return false;
}

void waitForKey(PadState& pad, const char* msg) {
    printf("%s\n", msg ? msg : "Press A/B to return.");
    consoleUpdate(NULL);
    while (appletMainLoop()) {
        u64 down = pollOnce(pad);
        if (down & (HidNpadButton_A | HidNpadButton_B)) return;
    }
}

MenuShell::MenuShell(Menu* root) {
    stack_.push_back(root);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_);
}

void MenuShell::render() {
    Menu* m = stack_.back();
    printf("%s", CLEAR);
    printf("Open-DBI  [%s]\n", OPEN_DBI_VERSION);
    printf("== %s ==\n\n", m->title);
    for (int i = 0; i < (int)m->items.size(); ++i) {
        const MenuItem& it = m->items[i];
        bool sel = (i == m->cursor);
        if (sel)            printf("%s", INVERT);
        if (it.destructive) printf("%s", RED);
        printf(" %s %s%s%s\n",
               sel ? ">" : " ",
               it.destructive ? "!! " : "",
               it.label,
               RESET);                       // reset after every styled line
    }
    printf("\nUp/Down move   A select   B back   + exit\n");
    const char* hint = m->items.empty() ? nullptr : m->items[m->cursor].hint;
    if (hint) printf("%s\n", hint);
}

void MenuShell::run() {
    dirty_ = true;
    while (appletMainLoop()) {
        if (dirty_) { render(); dirty_ = false; }
        consoleUpdate(NULL);
        padUpdate(&pad_);
        u64 down = padGetButtonsDown(&pad_);
        if (!down) continue;

        Menu* m = stack_.back();
        int n = (int)m->items.size();
        if (down & HidNpadButton_Plus) break;
        if (n > 0 && (down & HidNpadButton_Down)) { m->cursor = (m->cursor + 1) % n; dirty_ = true; }
        if (n > 0 && (down & HidNpadButton_Up))   { m->cursor = (m->cursor + n - 1) % n; dirty_ = true; }
        if ((down & HidNpadButton_B) && stack_.size() > 1) { stack_.pop_back(); dirty_ = true; }
        if ((down & HidNpadButton_A) && n > 0) {
            MenuItem& it = m->items[m->cursor];
            if (it.submenu) {
                stack_.push_back(it.submenu);
                dirty_ = true;
            } else if (it.action) {
                if (it.destructive && !confirmPrompt(pad_, it.label)) { dirty_ = true; continue; }
                ActionResult r = it.action();          // action prints its own output
                waitForKey(pad_, "\nPress A/B to return.");
                if (r == ActionResult::Exit) break;
                dirty_ = true;                          // force full redraw on return
            }
        }
    }
}

} // namespace dbi::ui
