#pragma once
// BattleScreen — presentation and input for turn-based combat (FR-B3, FR-U2).
//
// All rules live in BattleSystem; this class turns key presses and clicks into
// BattleAction requests, paces the enemy's turns, and animates the outcome.
// The strip at the top is a live view of the binary-heap turn queue.

#include "../core/DynamicArray.hpp"
#include "../game/GameSession.hpp"
#include "Assets.hpp"
#include "InputHandler.hpp"

#include <SFML/Graphics.hpp>

#include <string>

enum class BattleUiEvent { None, OpenItems, Finished };

class BattleScreen {
public:
    void begin(const GameSession& s);

    BattleUiEvent handleInput(const Input& in, GameSession& s);
    BattleUiEvent handleClick(sf::Vector2f mouse, GameSession& s);
    void handleHover(sf::Vector2f mouse);
    void update(float dt, GameSession& s);
    void draw(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const;

    void setFeedback(const std::string& message);
    // Call after any accepted player action (including items used via the pack).
    void onPlayerActed();

private:
    struct Floater {
        sf::Vector2f pos{0.f, 0.f};
        std::string text;
        sf::Color color = sf::Color::White;
        float age = 0.f;
        float life = 1.f;
        unsigned size = 22;
    };

    BattleUiEvent activate(int action, GameSession& s);
    void consumeFx(BattleSystem& b);
    void spawnFloater(int who, const std::string& text, sf::Color color, unsigned size);

    void drawTopBar(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const;
    void drawTurnOrder(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const;
    void drawArena(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const;
    void drawNameplate(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b, int who) const;
    void drawActions(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const;
    void drawLog(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const;
    void drawResult(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const;

    int selected_ = 0;  // 0 attack, 1 defend, 2 item, 3 escape
    int hover_ = -1;
    float time_ = 0.f;
    float intro_ = 0.f;       // 0..1 entrance animation
    float lock_ = 0.f;        // input lock while an action animates
    float enemyTimer_ = 0.f;  // "thinking" delay before the enemy acts
    float overTimer_ = 0.f;   // time since the battle ended
    float shake_[2] = {0.f, 0.f};
    float flash_[2] = {0.f, 0.f};
    float lunge_[2] = {0.f, 0.f};
    float hpShown_[2] = {0.f, 0.f};
    float hpGhost_[2] = {0.f, 0.f};
    std::string feedback_;
    float feedbackTimer_ = 0.f;
    DynamicArray<Floater> floaters_;
};
