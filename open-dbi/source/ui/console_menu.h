// console_menu.h - a small console-based menu shell for Open-DBI.
//
// Renders a navigable menu on the libnx text console (no framebuffer): Up/Down
// move the cursor, A selects / enters a submenu, B goes back, + exits. Menus form
// a tree (a leaf runs an Action; a node opens a submenu) and the shell keeps a
// screen stack so B pops back to the parent. Destructive items are gated behind a
// confirm prompt. Leaf actions own their own console output; the shell pauses
// ("Press A/B to return") then redraws.
#pragma once
#include <switch.h>
#include <vector>

namespace dbi::ui {

// What a leaf action tells the shell to do when it finishes.
enum class ActionResult { Return, Exit };   // Return -> back to menu; Exit -> quit app

// Leaf actions are stateless free functions (raw pointer: no std::function code
// size, and no -fno-exceptions "empty target -> terminate" edge).
using Action = ActionResult(*)();

struct Menu;  // fwd

struct MenuItem {
    const char* label;
    Action      action = nullptr;      // leaf: invoked on A (null if submenu set)
    Menu*       submenu = nullptr;      // node: A pushes this submenu
    bool        destructive = false;    // gated behind a confirm prompt
    const char* hint = nullptr;         // optional one-line footer help
};

struct Menu {
    const char*           title;
    std::vector<MenuItem> items;
    int                   cursor = 0;
};

// Owns the screen stack, input, and rendering. The root and all submenus must
// outlive the shell (use long-lived/static Menu objects; never pointers into a
// growing vector).
class MenuShell {
public:
    explicit MenuShell(Menu* root);
    void run();                         // blocks until an action returns Exit or + is pressed
private:
    void render();                      // full clear + reprint of stack_.back()
    std::vector<Menu*> stack_;
    bool     dirty_ = true;
    PadState pad_;
};

// Standalone input helpers; share a single PadState by reference so input is not
// double-owned. confirmPrompt returns true on A (proceed), false on B (cancel).
bool confirmPrompt(PadState& pad, const char* label);
void waitForKey(PadState& pad, const char* msg);

} // namespace dbi::ui
