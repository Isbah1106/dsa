#pragma once
// HUD — exploration chrome: top bar, map frame, sidebar status cards and
// the journal (message log) strip (FR-U1).

#include "../game/GameSession.hpp"
#include "Assets.hpp"
#include "Renderer.hpp"

#include <SFML/Graphics.hpp>

struct HudState {
    float displayHp = 0.f;  // animated HP value
    float ghostHp = 0.f;    // trailing "damage taken" value
    float time = 0.f;
    float playSeconds = 0.f;
    OverlayMode overlay = OverlayMode::Off;
    bool walking = false;
};

class Hud {
public:
    static sf::FloatRect mapFrame(const GameSession& s);

    void drawBackdrop(sf::RenderTarget& t) const;
    void drawTopBar(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const;
    void drawMapFrame(sf::RenderTarget& t, const GameSession& s) const;
    void drawSidebar(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const;
    void drawJournal(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const;

    static sf::Color logColor(LogKind kind);
    static std::string formatTime(float seconds);
};
