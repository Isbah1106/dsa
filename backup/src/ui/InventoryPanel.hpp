#pragma once
// InventoryPanel — modal panel for viewing, inspecting, using and discarding
// items (FR-I1, FR-I3, FR-U2). The same panel is the item picker in battle.
//
// The panel only tracks the cursor and feedback text; the rules (what an item
// does, what happens on an empty pack) stay in the game logic layer.

#include "../game/Entity.hpp"
#include "../game/Inventory.hpp"
#include "Assets.hpp"
#include "InputHandler.hpp"

#include <SFML/Graphics.hpp>

#include <string>

enum class InventoryMode { Explore, Battle };
enum class InventoryAction { None, Close, Use, Discard };

class InventoryPanel {
public:
    void open(InventoryMode mode);
    InventoryMode mode() const { return mode_; }
    int selected() const { return selected_; }

    InventoryAction handleInput(const Input& in, const Inventory& inv);
    InventoryAction handleClick(sf::Vector2f mouse, const Inventory& inv);
    void handleHover(sf::Vector2f mouse);
    void update(float dt);
    void draw(sf::RenderTarget& t, const Fonts& f, const Inventory& inv, const Player& player) const;

    void setFeedback(const std::string& message, bool positive);
    // Keeps the cursor on a valid slot after an item was used or discarded.
    void clampSelection(const Inventory& inv);

private:
    InventoryAction requestDiscard(const Inventory& inv);

    InventoryMode mode_ = InventoryMode::Explore;
    int selected_ = 0;
    int hoverRow_ = -1;
    int hoverButton_ = -1;
    int discardArmed_ = -1;  // slot waiting for a second "discard" press
    std::string feedback_;
    bool feedbackPositive_ = false;
    float feedbackTimer_ = 0.f;
};
