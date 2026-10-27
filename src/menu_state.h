#pragma once

#include "game_logic.h"

namespace threadlight::menu {

// Fixture render checks deliberately run in a hidden window.
inline bool shouldAutoPause(Mode mode, bool focused, bool minimized, bool hiddenFixture) {
    return mode == Mode::Play && !hiddenFixture && (!focused || minimized);
}

class FocusOwner {
public:
    void update(bool mouseMoved, bool mouseClicked, bool keyboardUsed) {
        if (mouseMoved || mouseClicked) mouseOwns_ = true;
        if (keyboardUsed) mouseOwns_ = false;
    }
    bool mouseOwnsFocus() const { return mouseOwns_; }
private:
    bool mouseOwns_ = true;
};

enum class Action {
    Pause,
    Resume,
    StartGame,
    OpenSettings,
    Back,
    ReturnToMainMenu,
    RequestExit,
    ConfirmExit,
    CancelExit,
    Escape
};

constexpr int kExitYesSelection = 0;
constexpr int kExitNoSelection = 1;
constexpr int kDefaultExitSelection = kExitNoSelection;

inline Action exitSelectionAction(int selection) {
    return selection == kExitYesSelection ? Action::ConfirmExit : Action::CancelExit;
}

inline int moveSelection(int selection, int direction, int optionCount) {
    if (optionCount <= 0 || direction == 0) return 0;
    const int step = direction < 0 ? optionCount - 1 : 1;
    return (selection + step) % optionCount;
}

inline int settingsFocusCount(int tab) {
    return tab == 0 ? 4 : 2;
}

inline void switchSettingsTab(int& tab, int& focus) {
    tab = 1 - tab;
    focus = 0;
}

inline void apply(Mode& mode, Mode& settingsReturn, bool& exitRequested,
                  int& exitSelection, Action action) {
    switch (action) {
    case Action::Pause:
        if (mode == Mode::Play) mode = Mode::Pause;
        else if (mode == Mode::Pause) mode = Mode::Play;
        break;
    case Action::Resume:
        if (mode == Mode::Pause) mode = Mode::Play;
        break;
    case Action::StartGame:
        if (mode == Mode::Title) mode = Mode::Play;
        break;
    case Action::OpenSettings:
        if (mode == Mode::Title || mode == Mode::Pause) {
            settingsReturn = mode;
            mode = Mode::Settings;
        }
        break;
    case Action::Back:
        if (mode == Mode::Settings) mode = settingsReturn;
        break;
    case Action::ReturnToMainMenu:
        if (mode == Mode::Pause) mode = Mode::Title;
        break;
    case Action::RequestExit:
        if (mode == Mode::Title) {
            mode = Mode::ExitConfirm;
            exitSelection = kDefaultExitSelection;
        }
        break;
    case Action::ConfirmExit:
        if (mode == Mode::ExitConfirm) exitRequested = true;
        break;
    case Action::CancelExit:
        if (mode == Mode::ExitConfirm) mode = Mode::Title;
        break;
    case Action::Escape:
        if (mode == Mode::Title) {
            mode = Mode::ExitConfirm;
            exitSelection = kDefaultExitSelection;
        }
        else if (mode == Mode::Pause) mode = Mode::Title;
        else if (mode == Mode::Settings) mode = settingsReturn;
        else if (mode == Mode::ExitConfirm || mode == Mode::Won || mode == Mode::Lost)
            mode = Mode::Title;
        // ESC is intentionally inert during active gameplay and upgrades.
        break;
    }
}

} // namespace threadlight::menu
