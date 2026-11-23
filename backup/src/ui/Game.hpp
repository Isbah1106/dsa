#pragma once
// Game — window, main loop and the screen state machine:
//
//   Title      -> Exploring
//   Exploring <-> Inventory | Paused
//   Exploring  -> Battle <-> BattleItems (item picker)
//   Battle     -> Exploring (won / escaped) | Defeat (player died)
//   Exploring  -> Victory (exit reached)
//   Help can be opened over Title, Exploring and Paused.
//
// All rules are delegated to GameSession; this class only routes input,
// advances animations and decides which screen to draw.

#include "../game/GameSession.hpp"
#include "Assets.hpp"
#include "BattleScreen.hpp"
#include "HUD.hpp"
#include "InputHandler.hpp"
#include "InventoryPanel.hpp"
#include "Menus.hpp"
#include "Renderer.hpp"

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <string>

class Game {
public:
    explicit Game(std::string exeDir);

    bool init(std::string* error = nullptr);
    int run();

    // Renders every screen to PNG files in `outDir` without opening a window.
    // Used for the README / report screenshots and as a visual smoke test.
    int captureScreens(const std::string& outDir);

private:
    enum class Screen { Title, Exploring, Inventory, Battle, BattleItems, Paused, Help, Victory, Defeat };

    struct Toast {
        std::string text;
        LogKind kind = LogKind::Info;
        float ttl = 0.f;
    };

    // Flow
    void startRun(std::uint32_t seed);
    void setScreen(Screen screen, bool fade = true);
    void openHelp();
    void onMenu(int id);
    static std::uint32_t randomSeed();

    // Input
    void handleEvent(const sf::Event& event);
    void onKey(sf::Keyboard::Key key);
    void onText(char32_t unicode);
    void onMouseMove(sf::Vector2f p);
    void onMouseDown(sf::Vector2f p, bool left);
    void exploreInput(const Input& in);
    void tryMove(Command c);
    void onMoveResult(MoveResult r);
    void inventoryAct(InventoryAction action);
    void battleEvent(BattleUiEvent event);
    void leaveBattle();
    void toast(const std::string& text, LogKind kind);

    // Frame
    void update(float dt);
    void updateExplore(float dt);
    void render(sf::RenderTarget& t);
    void renderTitle(sf::RenderTarget& t);
    void renderExplore(sf::RenderTarget& t, bool interactive, bool reveal);
    void renderToast(sf::RenderTarget& t);
    void applyLetterbox(sf::Vector2u windowSize);
    int tileAtPoint(sf::Vector2f p) const;

    // Screenshot helpers
    bool debugWalkTo(int tile);

    std::string exeDir_;
    sf::RenderWindow window_;
    sf::View view_;
    Fonts fonts_;

    GameSession session_;
    GameSession backdrop_;  // decorative dungeon behind the title screen
    Renderer renderer_;
    Hud hud_;
    HudState hudState_;
    InventoryPanel inventory_;
    BattleScreen battle_;
    Menu titleMenu_, pauseMenu_, endMenu_;

    Screen screen_ = Screen::Title;
    Screen helpReturn_ = Screen::Title;
    OverlayMode overlay_ = OverlayMode::Off;
    bool seedEntry_ = false;
    std::string seedText_;

    sf::Vector2f mouse_{-100.f, -100.f};
    sf::Vector2f playerVis_{0.f, 0.f};  // smoothed player position, in tiles
    int hoverTile_ = -1;
    bool focused_ = true;
    bool walking_ = false;
    float walkTimer_ = 0.f;
    Command heldMove_ = Command::None;
    float repeatTimer_ = 0.f;

    float time_ = 0.f;
    float playSeconds_ = 0.f;
    float fade_ = 0.f;
    Toast toast_;
};
