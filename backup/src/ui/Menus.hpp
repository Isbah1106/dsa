#pragma once
// Menus — a keyboard + mouse menu widget and the full-screen / modal screens
// built around it: title, pause, how-to-play and the victory / defeat summary
// (FR-U2, FR-U3, FR-U4, FR-B6, FR-M3).

#include "../core/DynamicArray.hpp"
#include "../game/GameSession.hpp"
#include "Assets.hpp"
#include "InputHandler.hpp"

#include <SFML/Graphics.hpp>

#include <string>

namespace menu_id {
enum Id { NewGame, PlaySeed, HowToPlay, Quit, Resume, Restart, NewDungeon, MainMenu };
}

struct MenuItem {
    std::string label;
    std::string hint;
    int id = 0;
};

class Menu {
public:
    void clear();
    void add(const std::string& label, const std::string& hint, int id);
    void setLayout(sf::Vector2f topLeft, float width, float itemHeight = 46.f, float gap = 8.f);
    void setHint(int id, const std::string& hint);
    void select(int index);

    // Each returns the id of the activated item, or -1.
    int handleInput(const Input& in);
    int handleClick(sf::Vector2f mouse) const;
    void handleHover(sf::Vector2f mouse);

    void draw(sf::RenderTarget& t, const Fonts& f, sf::Color accent) const;
    sf::FloatRect itemRect(int index) const;
    int indexOf(int id) const;
    int size() const { return items_.size(); }

private:
    DynamicArray<MenuItem> items_;
    int selected_ = 0;
    sf::Vector2f origin_{0.f, 0.f};
    float width_ = 320.f;
    float itemHeight_ = 46.f;
    float gap_ = 8.f;
};

namespace screens {

void setupTitleMenu(Menu& menu);
void setupPauseMenu(Menu& menu);
void setupEndMenu(Menu& menu);

// The title screen draws over a backdrop the caller has already rendered.
void drawTitle(sf::RenderTarget& t, const Fonts& f, const Menu& menu, bool seedEntry, const std::string& seedText, float time);
void drawPause(sf::RenderTarget& t, const Fonts& f, const Menu& menu, const GameSession& s, float playSeconds);
void drawHelp(sf::RenderTarget& t, const Fonts& f, float time);
void drawEnd(sf::RenderTarget& t, const Fonts& f, const Menu& menu, const GameSession& s, bool victory, float playSeconds, float time);

}  // namespace screens
