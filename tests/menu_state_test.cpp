#include "menu_state.h"

#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}
#define CHECK(expression) check((expression), #expression, __LINE__)

void action(threadlight::Mode& mode, threadlight::Mode& origin, bool& exit,
            int& exitSelection,
            threadlight::menu::Action input) {
    threadlight::menu::apply(mode, origin, exit, exitSelection, input);
}

void testPauseAndEscapeRules() {
    using threadlight::Mode;
    using threadlight::menu::Action;
    Mode mode = Mode::Play, origin = Mode::Title;
    bool exit = false;
    int exitSelection = threadlight::menu::kDefaultExitSelection;
    action(mode, origin, exit, exitSelection, Action::Escape);
    CHECK(mode == Mode::Play); // Gameplay ESC is inert.
    action(mode, origin, exit, exitSelection, Action::Pause);
    CHECK(mode == Mode::Pause);
    action(mode, origin, exit, exitSelection, Action::OpenSettings);
    CHECK(mode == Mode::Settings);
    CHECK(origin == Mode::Pause);
    action(mode, origin, exit, exitSelection, Action::Escape);
    CHECK(mode == Mode::Pause); // Settings returns to the paused run.
    action(mode, origin, exit, exitSelection, Action::Pause);
    CHECK(mode == Mode::Play); // P resumes.
    action(mode, origin, exit, exitSelection, Action::Pause);
    CHECK(mode == Mode::Pause);
    action(mode, origin, exit, exitSelection, Action::Escape);
    CHECK(mode == Mode::Title); // Paused ESC returns to the start menu.
}

void testSettingsOriginAndExitConfirmation() {
    using threadlight::Mode;
    using threadlight::menu::Action;
    Mode mode = Mode::Title, origin = Mode::Title;
    bool exit = false;
    int exitSelection = threadlight::menu::kExitYesSelection;
    CHECK(threadlight::menu::kDefaultExitSelection == threadlight::menu::kExitNoSelection);
    action(mode, origin, exit, exitSelection, Action::OpenSettings);
    CHECK(mode == Mode::Settings);
    CHECK(origin == Mode::Title);
    action(mode, origin, exit, exitSelection, Action::Back);
    CHECK(mode == Mode::Title);
    action(mode, origin, exit, exitSelection, Action::OpenSettings);
    CHECK(mode == Mode::Settings);
    action(mode, origin, exit, exitSelection, Action::Escape);
    CHECK(mode == Mode::Title);

    action(mode, origin, exit, exitSelection, Action::Escape);
    CHECK(mode == Mode::ExitConfirm);
    CHECK(exitSelection == threadlight::menu::kExitNoSelection);
    exitSelection = threadlight::menu::kExitYesSelection;
    action(mode, origin, exit, exitSelection, Action::Escape);
    CHECK(mode == Mode::Title);
    CHECK(!exit);

    action(mode, origin, exit, exitSelection, Action::RequestExit);
    CHECK(mode == Mode::ExitConfirm);
    CHECK(exitSelection == threadlight::menu::kExitNoSelection);
    exitSelection = threadlight::menu::kExitYesSelection;
    action(mode, origin, exit, exitSelection, threadlight::menu::exitSelectionAction(exitSelection));
    CHECK(exit);
    CHECK(threadlight::menu::exitSelectionAction(threadlight::menu::kDefaultExitSelection) == Action::CancelExit);
}

void testMenuActionsAreScopedToTheirScreen() {
    using threadlight::Mode;
    using threadlight::menu::Action;
    Mode mode = Mode::Pause, origin = Mode::Title;
    bool exit = false;
    int exitSelection = threadlight::menu::kDefaultExitSelection;
    action(mode, origin, exit, exitSelection, Action::ConfirmExit);
    CHECK(!exit);
    action(mode, origin, exit, exitSelection, Action::ReturnToMainMenu);
    CHECK(mode == Mode::Title);
    action(mode, origin, exit, exitSelection, Action::ConfirmExit);
    CHECK(!exit);
}

void testKeyboardFocusWrapsWithinEachMenu() {
    using namespace threadlight::menu;
    CHECK(moveSelection(0, -1, 3) == 2);
    CHECK(moveSelection(2, 1, 3) == 0);
    CHECK(moveSelection(0, -1, 4) == 3);
    CHECK(moveSelection(3, 1, 4) == 0);
    CHECK(settingsFocusCount(0) == 3);
    CHECK(settingsFocusCount(1) == 2);
    CHECK(moveSelection(0, -1, settingsFocusCount(0)) == 2);
    CHECK(moveSelection(1, 1, settingsFocusCount(1)) == 0);

    int tab = 0, focus = 2;
    switchSettingsTab(tab, focus);
    CHECK(tab == 1);
    CHECK(focus == 0);
    switchSettingsTab(tab, focus);
    CHECK(tab == 0);
    CHECK(focus == 0);
    CHECK(moveSelection(kDefaultExitSelection, 1, 2) == kExitYesSelection);
}
}

int main() {
    testPauseAndEscapeRules();
    testSettingsOriginAndExitConfirmation();
    testMenuActionsAreScopedToTheirScreen();
    testKeyboardFocusWrapsWithinEachMenu();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Menu-state navigation checks passed.\n";
}
