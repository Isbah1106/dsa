#pragma once
// InputHandler — maps raw SFML keys to game commands (FR-U3).

#include <SFML/Window/Keyboard.hpp>

enum class Command {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Confirm,
    Step,    // Space: one step along a planned route (acts as Confirm in menus)
    Reveal,  // F2: reveal the whole map (demo / grading aid)
    Back,
    Inventory,
    Use,
    Discard,
    Help,
    Overlay,
    PathToExit,
    Restart,
    Digit
};

struct Input {
    Command cmd = Command::None;
    int digit = -1;  // 1..9 when cmd == Digit
};

namespace input {
Input translate(sf::Keyboard::Key key);
bool isMove(Command c);
int dx(Command c);
int dy(Command c);
// True while any key bound to the given move command is held.
bool isHeld(Command c);
}  // namespace input
