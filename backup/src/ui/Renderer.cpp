#include "Renderer.hpp"

#include "Draw.hpp"
#include "Theme.hpp"

#include <algorithm>
#include <cmath>

namespace {
unsigned hashTile(int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

void quad(sf::VertexArray& va, float x, float y, float w, float h, sf::Color c) {
    va.append({{x, y}, c});
    va.append({{x + w, y}, c});
    va.append({{x + w, y + h}, c});
    va.append({{x, y}, c});
    va.append({{x + w, y + h}, c});
    va.append({{x, y + h}, c});
}
}  // namespace

float Renderer::lightAt(const GameSession& s, const MapView& v, int x, int y) const {
    if (!v.fog) return 0.82f;
    int idx = s.dungeon().index(x, y);
    if (!s.isExplored(idx)) return 0.f;
    if (!s.isVisible(idx)) return 0.34f;
    float dx = x - v.playerPos.x, dy = y - v.playerPos.y;
    float d = std::sqrt(dx * dx + dy * dy) / static_cast<float>(GameSession::kSightRadius);
    float l = 1.f - 0.5f * std::pow(d > 1.f ? 1.f : d, 1.4f);
    return l < 0.42f ? 0.42f : l;
}

void Renderer::drawMap(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
    const Dungeon& d = s.dungeon();
    const float T = v.tile;
    const sf::Vector2f o = v.origin;
    const float gap = T >= 20.f ? 1.f : 0.f;

    // --- Tiles (batched into one vertex array) ---------------------------------
    sf::VertexArray tiles(sf::PrimitiveType::Triangles);
    for (int y = 0; y < d.height(); ++y) {
        for (int x = 0; x < d.width(); ++x) {
            float light = lightAt(s, v, x, y) * v.dimAll;
            float px = o.x + x * T, py = o.y + y * T;

            if (d.isWalkable(x, y)) {
                if (light <= 0.f) continue;
                unsigned h = hashTile(x, y);
                float vary = 0.93f + (h % 100) / 100.f * 0.12f;
                sf::Color base = d.roomAt(x, y) != -1 ? theme::floorRoom : theme::floorCorridor;
                quad(tiles, px + gap, py + gap, T - 2 * gap, T - 2 * gap, draw::scale(base, vary * light * 1.15f));
                // Occasional speckle for texture.
                if (h % 7 == 0)
                    quad(tiles, px + (h % 13) / 13.f * (T - 6) + 2, py + (h % 11) / 11.f * (T - 6) + 2, 2, 2,
                         draw::scale(base, 1.35f * light));
            } else {
                // Only draw walls that border explored floor; the rest is void.
                bool edge = false;
                float edgeLight = 0.f;
                for (int oy = -1; oy <= 1; ++oy)
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!d.isWalkable(x + ox, y + oy)) continue;
                        float l = lightAt(s, v, x + ox, y + oy);
                        if (l > 0.f) {
                            edge = true;
                            if (l > edgeLight) edgeLight = l;
                        }
                    }
                if (!edge) continue;
                edgeLight *= v.dimAll;
                bool faceBelow = d.isWalkable(x, y + 1) && lightAt(s, v, x, y + 1) > 0.f;
                float faceH = faceBelow ? T * 0.3f : 0.f;
                quad(tiles, px, py, T, T - faceH, draw::scale(theme::wallTop, edgeLight));
                quad(tiles, px, py, T, 2.f, draw::scale(theme::wallTop, edgeLight * 1.25f));
                if (faceBelow) quad(tiles, px, py + T - faceH, T, faceH, draw::scale(theme::wallFace, edgeLight));
            }
        }
    }
    t.draw(tiles);

    // --- DSA overlays ----------------------------------------------------------
    if (v.overlay == OverlayMode::Search) {
        const PathResult& search = s.lastSearch();
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        int n = search.expanded.size();
        for (int i = 0; i < n; ++i) {
            int idx = search.expanded[i];
            float k = n > 1 ? static_cast<float>(i) / (n - 1) : 0.f;
            sf::Color c = draw::withAlpha(draw::mix(theme::blue, theme::teal, k), 70);
            quad(va, o.x + d.xOf(idx) * T + 2, o.y + d.yOf(idx) * T + 2, T - 4, T - 4, c);
        }
        t.draw(va);
    } else if (v.overlay == OverlayMode::Graph) {
        sf::VertexArray lines(sf::PrimitiveType::Lines);
        const Graph& g = d.graph();
        for (int u = 0; u < g.vertexCount(); ++u) {
            if (v.fog && !s.isExplored(u)) continue;
            sf::Vector2f a{o.x + (d.xOf(u) + 0.5f) * T, o.y + (d.yOf(u) + 0.5f) * T};
            for (const GraphEdge* e = g.edges(u); e; e = e->next) {
                if (e->to < u || (v.fog && !s.isExplored(e->to))) continue;
                sf::Vector2f b{o.x + (d.xOf(e->to) + 0.5f) * T, o.y + (d.yOf(e->to) + 0.5f) * T};
                lines.append({a, draw::withAlpha(theme::teal, 120)});
                lines.append({b, draw::withAlpha(theme::teal, 120)});
            }
        }
        t.draw(lines);
        for (int u = 0; u < g.vertexCount(); ++u) {
            if (g.degree(u) == 0 || (v.fog && !s.isExplored(u))) continue;
            draw::circle(t, {o.x + (d.xOf(u) + 0.5f) * T, o.y + (d.yOf(u) + 0.5f) * T}, 2.f,
                         draw::withAlpha(theme::teal, 200), sf::Color::Transparent, 0.f, 8);
        }
    }

    auto center = [&](int idx) { return sf::Vector2f{o.x + (d.xOf(idx) + 0.5f) * T, o.y + (d.yOf(idx) + 0.5f) * T}; };
    const float unit = T / 26.f;

    // --- Planned A* route --------------------------------------------------------
    if (s.hasPlannedPath()) {
        const DynamicArray<int>& path = s.lastSearch().path;
        int startAt = s.pathCursor();
        for (int i = startAt; i + 1 < path.size(); ++i)
            draw::line(t, center(path[i]), center(path[i + 1]), draw::withAlpha(theme::teal, 90), 3.f * unit);
        for (int i = startAt + 1; i < path.size() - 1; ++i) {
            float phase = std::fmod(v.time * 8.f - i * 0.6f, 6.f);
            float k = phase < 1.f ? 1.f - phase : 0.f;
            draw::circle(t, center(path[i]), (2.2f + 1.6f * k) * unit, draw::withAlpha(draw::mix(theme::teal, sf::Color::White, k), 230));
        }
        sf::Vector2f goal = center(path.back());
        float pulse = 0.5f + 0.5f * std::sin(v.time * 5.f);
        draw::ring(t, goal, (8.f + 2.f * pulse) * unit, 2.f * unit, draw::withAlpha(theme::teal, 230));
        draw::glow(t, goal, 14.f * unit, draw::withAlpha(theme::teal, 120), 4);
    }

    // --- Exit portal -------------------------------------------------------------
    if (!v.fog || s.isExplored(d.exit)) {
        float a = (v.fog && !s.isVisible(d.exit)) ? 0.55f : 1.f;
        draw::portalIcon(t, center(d.exit), unit * 1.05f, v.time, a * v.dimAll);
    }

    // --- Items ---------------------------------------------------------------------
    for (const FloorItem& fi : s.floorItems()) {
        if (fi.taken || (v.fog && !s.isExplored(fi.tile))) continue;
        float a = (v.fog && !s.isVisible(fi.tile)) ? 0.45f : 1.f;
        sf::Vector2f c = center(fi.tile);
        float bob = std::sin(v.time * 3.f + fi.tile) * 1.5f * unit;
        draw::shadow(t, {c.x, c.y + 8.f * unit}, 6.f * unit, 2.f * unit, static_cast<int>(90 * a));
        if (a > 0.9f) draw::glow(t, {c.x, c.y + bob}, 12.f * unit, draw::withAlpha(draw::itemColor(fi.item.type), 70), 3);
        draw::itemIcon(t, fi.item.type, {c.x, c.y - 1.f * unit + bob}, 0.82f * unit, a * v.dimAll);
    }

    // --- Enemies -----------------------------------------------------------------
    for (const Enemy& e : s.enemies()) {
        if (!e.alive || (v.fog && !s.isExplored(e.tile))) continue;
        float a = (v.fog && !s.isVisible(e.tile)) ? 0.45f : 1.f;
        sf::Vector2f c = center(e.tile);
        draw::shadow(t, {c.x, c.y + 9.f * unit}, 8.f * unit, 2.5f * unit, static_cast<int>(100 * a));
        float bob = std::sin(v.time * 2.2f + e.uid) * 1.f * unit;
        draw::enemyIcon(t, e.kind, {c.x, c.y + bob}, (e.boss ? 0.86f : 0.78f) * unit, v.time + e.uid, a * v.dimAll);
        if (e.stats.hp < e.stats.maxHp) {
            float f = static_cast<float>(e.stats.hp) / e.stats.maxHp;
            draw::bar(t, {{c.x - 10.f * unit, c.y - 15.f * unit}, {20.f * unit, 3.f * unit}}, f, draw::hpColor(f),
                      sf::Color(0, 0, 0, 170));
        }
    }

    // --- Player ------------------------------------------------------------------
    if (v.showPlayer) {
        sf::Vector2f pc{o.x + (v.playerPos.x + 0.5f) * T, o.y + (v.playerPos.y + 0.5f) * T};
        draw::glow(t, pc, 40.f * unit, sf::Color(255, 196, 110, 60), 6);
        draw::shadow(t, {pc.x, pc.y + 9.f * unit}, 8.f * unit, 2.5f * unit, 110);
        draw::playerIcon(t, pc, 0.82f * unit, s.lastDirection());
    }

    // --- Hover cursor ------------------------------------------------------------
    if (v.hoverTile >= 0 && d.isValidIndex(v.hoverTile)) {
        bool ok = s.isExplored(v.hoverTile) && d.isWalkableIdx(v.hoverTile);
        sf::Color c = ok ? draw::withAlpha(theme::teal, 220) : draw::withAlpha(theme::red, 170);
        sf::FloatRect r{{o.x + d.xOf(v.hoverTile) * T + 1, o.y + d.yOf(v.hoverTile) * T + 1}, {T - 2, T - 2}};
        draw::roundedRect(t, r, 4.f, draw::withAlpha(c, 28), c, 1.5f);
    }
}

void Renderer::drawHoverTooltip(sf::RenderTarget& t, const Fonts& fonts, const GameSession& s, const MapView& v,
                                sf::Vector2f mouse) const {
    if (v.hoverTile < 0 || !s.isExplored(v.hoverTile)) return;
    std::string title, sub;
    sf::Color accent = theme::text;
    int e = s.enemyAt(v.hoverTile);
    int it = s.itemAt(v.hoverTile);
    if (e != -1) {
        const Enemy& en = s.enemies()[e];
        const EnemyInfo& info = enemyInfo(en.kind);
        title = std::string(info.name) + (en.boss ? "  ·  BOSS" : "");
        sub = "HP " + std::to_string(en.stats.hp) + "/" + std::to_string(en.stats.maxHp) + "   ATK " + std::to_string(en.stats.attack) +
              "   DEF " + std::to_string(en.stats.defense) + "   SPD " + std::to_string(en.stats.speed);
        accent = en.boss ? theme::violet : theme::red;
    } else if (it != -1) {
        const ItemInfo& info = itemInfo(s.floorItems()[it].item.type);
        title = info.name;
        sub = info.effect;
        accent = draw::itemColor(s.floorItems()[it].item.type);
    } else if (v.hoverTile == s.dungeon().exit) {
        title = "Exit Portal";
        sub = "Step through to escape the Lost Realms";
        accent = theme::violet;
    } else {
        return;
    }

    float w = std::max(draw::textWidth(fonts.bold, title, 14), draw::textWidth(fonts.regular, sub, 12)) + 28.f;
    float h = 52.f;
    sf::Vector2f p{mouse.x + 16.f, mouse.y + 16.f};
    if (p.x + w > theme::kSideX - 8.f) p.x = mouse.x - w - 12.f;
    if (p.y + h > theme::kWindowH - 8.f) p.y = mouse.y - h - 12.f;
    sf::FloatRect r{p, {w, h}};
    draw::roundedRect(t, {{p.x, p.y + 4}, r.size}, 8.f, sf::Color(0, 0, 0, 110));
    draw::roundedRect(t, r, 8.f, sf::Color(24, 28, 41, 248), theme::borderHi, 1.f);
    draw::rect(t, {{p.x + 1, p.y + 10}, {3.f, h - 20}}, accent);
    draw::text(t, fonts.bold, title, 14, {p.x + 14, p.y + 17}, theme::text);
    draw::text(t, fonts.regular, sub, 12, {p.x + 14, p.y + 36}, theme::muted);
}
