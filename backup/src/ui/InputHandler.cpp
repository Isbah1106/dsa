#include "InputHandler.hpp"

namespace input {

Input translate(sf::Keyboard::Key key) {
    using K = sf::Keyboard::Key;
    Input in;
    switch (key) {
        case K::W: case K::Up: in.cmd = Command::MoveUp; break;
        case K::S: case K::Down: in.cmd = Command::MoveDown; break;
        case K::A: case K::Left: in.cmd = Command::MoveLeft; break;
        case K::D: case K::Right: in.cmd = Command::MoveRight; break;
        case K::Enter: in.cmd = Command::Confirm; break;
        case K::Space: in.cmd = Command::Step; break;
        case K::F2: in.cmd = Command::Reveal; break;
        case K::Escape: case K::Backspace: in.cmd = Command::Back; break;
        case K::I: case K::B: in.cmd = Command::Inventory; break;
        case K::U: case K::E: in.cmd = Command::Use; break;
        case K::X: case K::Delete: in.cmd = Command::Discard; break;
        case K::H: case K::F1: in.cmd = Command::Help; break;
        case K::Tab: in.cmd = Command::Overlay; break;
        case K::P: in.cmd = Command::PathToExit; break;
        case K::R: in.cmd = Command::Restart; break;
        default: break;
    }
    int k = static_cast<int>(key);
    if (k >= static_cast<int>(K::Num1) && k <= static_cast<int>(K::Num9)) {
        in.cmd = Command::Digit;
        in.digit = k - static_cast<int>(K::Num1) + 1;
    } else if (k >= static_cast<int>(K::Numpad1) && k <= static_cast<int>(K::Numpad9)) {
        in.cmd = Command::Digit;
        in.digit = k - static_cast<int>(K::Numpad1) + 1;
    }
    return in;
}

bool isMove(Command c) {
    return c == Command::MoveUp || c == Command::MoveDown || c == Command::MoveLeft || c == Command::MoveRight;
}

int dx(Command c) { return c == Command::MoveLeft ? -1 : c == Command::MoveRight ? 1 : 0; }
int dy(Command c) { return c == Command::MoveUp ? -1 : c == Command::MoveDown ? 1 : 0; }

bool isHeld(Command c) {
    using K = sf::Keyboard::Key;
    auto down = [](K k) { return sf::Keyboard::isKeyPressed(k); };
    switch (c) {
        case Command::MoveUp: return down(K::W) || down(K::Up);
        case Command::MoveDown: return down(K::S) || down(K::Down);
        case Command::MoveLeft: return down(K::A) || down(K::Left);
        case Command::MoveRight: return down(K::D) || down(K::Right);
        default: return false;
    }
}

}  // namespace input
