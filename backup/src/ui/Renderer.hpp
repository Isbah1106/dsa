#pragma once
// Renderer — draws the dungeon, fog of war, entities, routes and the
// DSA debug overlays (A* search tree / graph edges).

#include "../game/GameSession.hpp"
#include "Assets.hpp"

#include <SFML/Graphics.hpp>

enum class OverlayMode { Off, Search, Graph };

struct MapView {
    sf::Vector2f origin{0.f, 0.f};
    float tile = 26.f;
    bool fog = true;
    bool showPlayer = true;
    OverlayMode overlay = OverlayMode::Off;
    int hoverTile = -1;
    sf::Vector2f playerPos{0.f, 0.f};  // in tile units (smoothly interpolated)
    float time = 0.f;
    float dimAll = 1.f;
};

class Renderer {
public:
    void drawMap(sf::RenderTarget& t, const GameSession& s, const MapView& v) const;
    void drawHoverTooltip(sf::RenderTarget& t, const Fonts& fonts, const GameSession& s, const MapView& v,
                          sf::Vector2f mouse) const;

private:
    float lightAt(const GameSession& s, const MapView& v, int x, int y) const;
};
