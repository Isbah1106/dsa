#include "HUD.hpp"

#include "Draw.hpp"
#include "Theme.hpp"

#include <cstdio>

using draw::Align;

namespace {
std::string fmt(const char* f, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

// A small rounded "chip" with a label and value; returns its width.
float statChip(sf::RenderTarget& t, const Fonts& f, const std::string& label, const std::string& value, sf::Vector2f rightCenter,
               sf::Color valueColor = theme::text) {
    float lw = draw::textWidth(f.bold, label, 10, 1.8f);
    float vw = draw::textWidth(f.mono, value, 13);
    float w = lw + vw + 30.f;
    sf::FloatRect r{{rightCenter.x - w, rightCenter.y - 14.f}, {w, 28.f}};
    draw::roundedRect(t, r, 14.f, theme::panel, theme::border, 1.f);
    draw::text(t, f.bold, label, 10, {r.position.x + 12.f, rightCenter.y}, theme::dim, Align::Left, 1.8f);
    draw::text(t, f.mono, value, 13, {r.position.x + w - 12.f, rightCenter.y}, valueColor, Align::Right);
    return w;
}
}  // namespace

sf::Color Hud::logColor(LogKind kind) {
    switch (kind) {
        case LogKind::PlayerHit: return theme::gold;
        case LogKind::EnemyHit: return theme::red;
        case LogKind::Heal: return theme::green;
        case LogKind::Warning: return theme::orange;
        case LogKind::Success: return theme::green;
        case LogKind::Danger: return theme::red;
        case LogKind::Arcane: return theme::violet;
        default: return theme::blue;
    }
}

std::string Hud::formatTime(float seconds) {
    int s = static_cast<int>(seconds);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

sf::FloatRect Hud::mapFrame(const GameSession& s) {
    float w = s.dungeon().width() * theme::kTile;
    float h = s.dungeon().height() * theme::kTile;
    return {{theme::kMapX - 6.f, theme::kMapY - 6.f}, {w + 12.f, h + 12.f}};
}

void Hud::drawBackdrop(sf::RenderTarget& t) const {
    draw::gradientRect(t, {{0, 0}, {float(theme::kWindowW), float(theme::kWindowH)}}, theme::bgTop, theme::bg);
}

void Hud::drawTopBar(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
    const float W = static_cast<float>(theme::kWindowW);
    draw::gradientRect(t, {{0, 0}, {W, theme::kTopBarH}}, sf::Color(20, 24, 36), sf::Color(14, 17, 26));
    draw::rect(t, {{0, theme::kTopBarH - 1}, {W, 1}}, theme::border);

    draw::portalIcon(t, {32.f, 28.f}, 0.85f, h.time);
    draw::text(t, f.title, "DUNGEONBOUND", 20, {56.f, 27.f}, theme::gold, Align::Left, 1.5f);
    float tw = draw::textWidth(f.title, "DUNGEONBOUND", 20, 1.5f);
    draw::text(t, f.regular, "The Lost Realms", 13, {66.f + tw, 28.f}, theme::muted);

    float x = W - 16.f;
    x -= statChip(t, f, "EXPLORED", std::to_string(s.exploredPercent()) + "%", {x, 28.f}) + 8.f;
    x -= statChip(t, f, "TIME", formatTime(h.playSeconds), {x, 28.f}) + 8.f;
    x -= statChip(t, f, "STEPS", std::to_string(s.stats().steps), {x, 28.f}) + 8.f;
    x -= statChip(t, f, "SEED", std::to_string(s.seed()), {x, 28.f}, theme::gold) + 8.f;
    if (h.overlay != OverlayMode::Off) {
        statChip(t, f, "OVERLAY", h.overlay == OverlayMode::Search ? "A* SEARCH" : "GRAPH", {x, 28.f}, theme::teal);
    } else if (h.walking) {
        statChip(t, f, "AUTO", "WALKING", {x, 28.f}, theme::teal);
    }
}

void Hud::drawMapFrame(sf::RenderTarget& t, const GameSession& s) const {
    sf::FloatRect r = mapFrame(s);
    draw::roundedRect(t, {{r.position.x, r.position.y + 4.f}, r.size}, 12.f, sf::Color(0, 0, 0, 90));
    draw::roundedRect(t, r, 12.f, theme::void_, theme::border, 1.f);
}

void Hud::drawSidebar(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
    const float X = theme::kSideX, W = theme::kSideW;
    const Player& p = s.player();

    // ---- Hero card -------------------------------------------------------------
    {
        sf::FloatRect r{{X, 66.f}, {W, 176.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::sectionLabel(t, f.bold, "ADVENTURER", {x + 18.f, y + 22.f});
        draw::text(t, f.bold, "LEVEL " + std::to_string(p.level), 12, {x + W - 18.f, y + 22.f}, theme::gold, Align::Right, 1.5f);

        sf::Vector2f pc{x + 46.f, y + 76.f};
        draw::circle(t, pc, 28.f, theme::panelHi, theme::borderHi, 1.f);
        draw::glow(t, pc, 30.f, sf::Color(255, 190, 100, 50), 4);
        draw::playerIcon(t, pc, 1.75f, 2);

        float bx = x + 88.f, bw = W - 106.f;
        float frac = p.stats.maxHp > 0 ? h.displayHp / p.stats.maxHp : 0.f;
        float ghost = p.stats.maxHp > 0 ? h.ghostHp / p.stats.maxHp : 0.f;
        draw::text(t, f.bold, "HP", 11, {bx, y + 52.f}, theme::muted, Align::Left, 1.5f);
        draw::text(t, f.mono, std::to_string(p.stats.hp) + " / " + std::to_string(p.stats.maxHp), 13, {bx + bw, y + 52.f},
                   theme::text, Align::Right);
        draw::bar(t, {{bx, y + 64.f}, {bw, 12.f}}, frac, draw::hpColor(frac), sf::Color(8, 10, 16), ghost);

        float xf = static_cast<float>(p.xp) / p.xpToNext();
        draw::text(t, f.bold, "XP", 11, {bx, y + 92.f}, theme::muted, Align::Left, 1.5f);
        draw::text(t, f.mono, std::to_string(p.xp) + " / " + std::to_string(p.xpToNext()), 12, {bx + bw, y + 92.f}, theme::muted,
                   Align::Right);
        draw::bar(t, {{bx, y + 103.f}, {bw, 6.f}}, xf, theme::xp, sf::Color(8, 10, 16));

        const char* labels[3] = {"ATK", "DEF", "SPD"};
        int values[3] = {p.stats.attack, p.stats.defense, p.stats.speed};
        sf::Color tints[3] = {theme::orange, theme::blue, theme::teal};
        float cw = (W - 36.f - 16.f) / 3.f;
        for (int i = 0; i < 3; ++i) {
            sf::FloatRect c{{x + 18.f + i * (cw + 8.f), y + 122.f}, {cw, 40.f}};
            draw::roundedRect(t, c, 7.f, theme::panelHi, theme::border, 1.f);
            draw::rect(t, {{c.position.x + 10.f, c.position.y + 12.f}, {3.f, 16.f}}, tints[i]);
            draw::text(t, f.bold, labels[i], 10, {c.position.x + 20.f, c.position.y + 20.f}, theme::muted, Align::Left, 1.5f);
            draw::text(t, f.bold, std::to_string(values[i]), 17, {c.position.x + cw - 12.f, c.position.y + 20.f}, theme::text,
                       Align::Right);
        }
    }

    // ---- Pack card -------------------------------------------------------------
    {
        sf::FloatRect r{{X, 254.f}, {W, 128.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        const Inventory& inv = s.inventory();
        draw::sectionLabel(t, f.bold, "PACK", {x + 18.f, y + 22.f});
        draw::text(t, f.mono, std::to_string(inv.size()) + " / " + std::to_string(inv.capacity()), 12, {x + W - 44.f, y + 22.f},
                   inv.isFull() ? theme::orange : theme::muted, Align::Right);
        draw::keycap(t, f.bold, "I", {x + W - 36.f, y + 22.f}, 10);
        float sw = (W - 36.f - 3 * 8.f) / 4.f;
        for (int i = 0; i < inv.capacity(); ++i) {
            int col = i % 4, row = i / 4;
            sf::FloatRect c{{x + 18.f + col * (sw + 8.f), y + 40.f + row * 42.f}, {sw, 36.f}};
            bool has = i < inv.size();
            draw::roundedRect(t, c, 6.f, has ? theme::panelHi : sf::Color(15, 18, 27), theme::border, 1.f);
            if (has) {
                ItemType type = inv.at(i).type;
                draw::itemIcon(t, type, {c.position.x + c.size.x / 2.f, c.position.y + 19.f}, 1.05f);
            }
        }
    }

    // ---- Objective card ----------------------------------------------------------
    {
        sf::FloatRect r{{X, 394.f}, {W, 104.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::sectionLabel(t, f.bold, "OBJECTIVE", {x + 18.f, y + 22.f});
        bool found = s.exitDiscovered();
        draw::portalIcon(t, {x + 32.f, y + 56.f}, 0.75f, h.time, found ? 1.f : 0.45f);
        draw::text(t, f.bold, found ? "Exit portal located" : "Find the exit portal", 14, {x + 54.f, y + 49.f}, theme::text);
        if (found) {
            float kw = draw::keycap(t, f.bold, "P", {x + 54.f, y + 70.f}, 10);
            draw::text(t, f.regular, "plot an A* route to the portal", 12, {x + 60.f + kw, y + 70.f}, theme::muted);
        } else {
            draw::text(t, f.regular, "Explore to reveal the dungeon", 12, {x + 54.f, y + 70.f}, theme::muted);
        }
        int alive = s.enemiesRemaining();
        draw::text(t, f.regular, std::to_string(alive) + " of " + std::to_string(s.enemies().size()) + " foes remain  ·  The Warden guards the exit",
                   11, {x + 18.f, y + 90.f}, theme::dim);
    }

    // ---- Algorithms card -----------------------------------------------------------
    {
        sf::FloatRect r{{X, 510.f}, {W, 158.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::sectionLabel(t, f.bold, "ALGORITHMS", {x + 18.f, y + 22.f});
        const char* ov = h.overlay == OverlayMode::Off ? "OFF" : h.overlay == OverlayMode::Search ? "A*" : "GRAPH";
        float vw = draw::textWidth(f.mono, ov, 11);
        draw::text(t, f.mono, ov, 11, {x + W - 18.f, y + 22.f}, h.overlay == OverlayMode::Off ? theme::dim : theme::teal, Align::Right);
        draw::keycap(t, f.bold, "Tab", {x + W - 30.f - vw - 32.f, y + 22.f}, 10);

        const Dungeon& d = s.dungeon();
        const PathResult& ps = s.lastSearch();
        std::string astar = "No search yet";
        sf::Color astarColor = theme::dim;
        if (s.stats().pathsPlanned > 0) {
            if (ps.found || s.hasPlannedPath()) {
                astar = std::to_string(ps.cost) + " steps · " + std::to_string(ps.expanded.size()) + " nodes";
                astarColor = theme::teal;
            } else {
                astar = "No path · " + std::to_string(ps.expanded.size()) + " nodes";
                astarColor = theme::orange;
            }
        }
        struct Row {
            const char* label;
            std::string value;
            sf::Color color;
        } rows[4] = {
            {"Graph · adjacency list", std::to_string(d.walkableCount()) + " V · " + std::to_string(d.graph().edgeCount()) + " E", theme::text},
            {"Recursive backtracking", "seed " + std::to_string(s.seed()), theme::text},
            {"A* · Manhattan", astar, astarColor},
            {"Binary heap pushes", s.stats().pathsPlanned > 0 ? std::to_string(ps.heapPushes) + fmt(" · %.2f ms", ps.millis) : "—", theme::text},
        };
        for (int i = 0; i < 4; ++i) {
            float ry = y + 50.f + i * 27.f;
            if (i > 0) draw::rect(t, {{x + 18.f, ry - 13.5f}, {W - 36.f, 1.f}}, sf::Color(255, 255, 255, 10));
            draw::text(t, f.regular, rows[i].label, 12, {x + 18.f, ry}, theme::muted);
            draw::text(t, f.mono, rows[i].value, 12, {x + W - 18.f, ry}, rows[i].color, Align::Right);
        }
    }

    // ---- Controls card ---------------------------------------------------------------
    {
        sf::FloatRect r{{X, 680.f}, {W, 88.f}};
        draw::panel(t, r);
        float x = r.position.x + 18.f, y = r.position.y;
        auto hint = [&](float& cx, float cy, const std::string& key, const std::string& label) {
            cx += draw::keycap(t, f.bold, key, {cx, cy}, 10) + 6.f;
            draw::text(t, f.regular, label, 12, {cx, cy}, theme::muted);
            cx += draw::textWidth(f.regular, label, 12) + 14.f;
        };
        float cx = x;
        hint(cx, y + 26.f, "WASD", "Move");
        hint(cx, y + 26.f, "Click", "Route");
        hint(cx, y + 26.f, "Enter", "Walk");
        cx = x;
        hint(cx, y + 60.f, "I", "Pack");
        hint(cx, y + 60.f, "H", "Help");
        hint(cx, y + 60.f, "Esc", "Pause");
        hint(cx, y + 60.f, "RMB", "Clear");
    }
}

void Hud::drawJournal(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
    sf::FloatRect frame = mapFrame(s);
    float top = frame.position.y + frame.size.y + 12.f;
    sf::FloatRect r{{frame.position.x, top}, {frame.size.x, static_cast<float>(theme::kWindowH) - 12.f - top}};
    draw::panel(t, r);
    float x = r.position.x, y = r.position.y;
    draw::text(t, f.bold, "JOURNAL", 11, {x + 18.f, y + r.size.y / 2.f}, theme::dim, Align::Left, 2.2f);
    draw::rect(t, {{x + 104.f, y + 14.f}, {1.f, r.size.y - 28.f}}, theme::border);

    const DynamicArray<LogLine>& log = s.messages();
    int lines = 3;
    float lh = (r.size.y - 16.f) / lines;
    int start = log.size() - lines;
    if (start < 0) start = 0;
    for (int i = start, row = 0; i < log.size(); ++i, ++row) {
        int age = log.size() - 1 - i;  // 0 = newest
        int alpha = age == 0 ? 255 : age == 1 ? 175 : 110;
        float ly = y + 8.f + lh * (row + 0.5f);
        draw::circle(t, {x + 124.f, ly}, 3.f, draw::withAlpha(logColor(log[i].kind), alpha));
        draw::text(t, f.regular, log[i].text, 13, {x + 136.f, ly}, draw::withAlpha(age == 0 ? theme::text : theme::muted, alpha));
    }
}
