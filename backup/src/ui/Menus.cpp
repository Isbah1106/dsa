#include "Menus.hpp"

#include "Draw.hpp"
#include "HUD.hpp"
#include "Theme.hpp"

#include <cmath>

using draw::Align;

namespace {
const float kW = static_cast<float>(theme::kWindowW);
const float kH = static_cast<float>(theme::kWindowH);

// Project credits, taken from the PRD header. Edit here to change them everywhere.
const char* const kCredits = "Isbah Rafay (25K-0664)  ·  Maheen Kamran (25K-0598)  ·  BCS-3F  ·  FAST-NUCES Karachi";

void card(sf::RenderTarget& t, sf::FloatRect r, sf::Color border = theme::borderHi) {
    draw::roundedRect(t, {{r.position.x, r.position.y + 8.f}, r.size}, 16.f, sf::Color(0, 0, 0, 120));
    draw::roundedRect(t, r, 16.f, theme::panel, border, 1.f);
}

// keycap + label pair; advances `x`.
void hint(sf::RenderTarget& t, const Fonts& f, float& x, float y, const std::string& key, const std::string& label) {
    x += draw::keycap(t, f.bold, key, {x, y}, 10) + 6.f;
    if (label.empty()) return;
    draw::text(t, f.regular, label, 12, {x, y}, theme::muted);
    x += draw::textWidth(f.regular, label, 12) + 16.f;
}
}  // namespace

// =========================================================================== Menu
void Menu::clear() {
    items_.clear();
    selected_ = 0;
}

void Menu::add(const std::string& label, const std::string& hintText, int id) { items_.push_back({label, hintText, id}); }

void Menu::setLayout(sf::Vector2f topLeft, float width, float itemHeight, float gap) {
    origin_ = topLeft;
    width_ = width;
    itemHeight_ = itemHeight;
    gap_ = gap;
}

void Menu::setHint(int id, const std::string& hintText) {
    int i = indexOf(id);
    if (i >= 0) items_[i].hint = hintText;
}

void Menu::select(int index) {
    if (index >= 0 && index < items_.size()) selected_ = index;
}

int Menu::indexOf(int id) const {
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id == id) return i;
    }
    return -1;
}

sf::FloatRect Menu::itemRect(int index) const { return {{origin_.x, origin_.y + index * (itemHeight_ + gap_)}, {width_, itemHeight_}}; }

int Menu::handleInput(const Input& in) {
    if (items_.empty()) return -1;
    switch (in.cmd) {
        case Command::MoveUp:
            selected_ = (selected_ + items_.size() - 1) % items_.size();
            break;
        case Command::MoveDown:
            selected_ = (selected_ + 1) % items_.size();
            break;
        case Command::Confirm:
        case Command::Step:
            return items_[selected_].id;
        default:
            break;
    }
    return -1;
}

int Menu::handleClick(sf::Vector2f mouse) const {
    for (int i = 0; i < items_.size(); ++i) {
        if (itemRect(i).contains(mouse)) return items_[i].id;
    }
    return -1;
}

void Menu::handleHover(sf::Vector2f mouse) {
    for (int i = 0; i < items_.size(); ++i) {
        if (itemRect(i).contains(mouse)) selected_ = i;
    }
}

void Menu::draw(sf::RenderTarget& t, const Fonts& f, sf::Color accent) const {
    for (int i = 0; i < items_.size(); ++i) {
        const sf::FloatRect r = itemRect(i);
        const bool sel = i == selected_;
        const float cy = r.position.y + r.size.y / 2.f;
        draw::roundedRect(t, {{r.position.x, r.position.y + 2.f}, r.size}, 9.f, sf::Color(0, 0, 0, 60));
        draw::roundedRect(t, r, 9.f, sel ? draw::mix(theme::panelHi, accent, 0.15f) : sf::Color(19, 23, 34, 235), sel ? accent : theme::border, 1.f);
        if (sel) draw::rect(t, {{r.position.x + 1.f, r.position.y + 12.f}, {3.f, r.size.y - 24.f}}, accent);
        draw::text(t, f.bold, items_[i].label, 15, {r.position.x + 22.f, cy}, sel ? theme::text : theme::muted);
        if (!items_[i].hint.empty())
            draw::text(t, f.regular, items_[i].hint, 12, {r.position.x + r.size.x - 18.f, cy}, sel ? theme::muted : theme::dim, Align::Right);
    }
}

// ======================================================================== screens
namespace screens {

void setupTitleMenu(Menu& menu) {
    menu.clear();
    menu.add("New Game", "Random dungeon", menu_id::NewGame);
    menu.add("Play a Seed", "Replay a layout", menu_id::PlaySeed);
    menu.add("How to Play", "Controls & legend", menu_id::HowToPlay);
    menu.add("Quit", "", menu_id::Quit);
    menu.setLayout({96.f, 404.f}, 400.f, 48.f, 8.f);
}

void setupPauseMenu(Menu& menu) {
    menu.clear();
    menu.add("Resume", "Esc", menu_id::Resume);
    menu.add("Restart Dungeon", "Same seed", menu_id::Restart);
    menu.add("New Dungeon", "Random seed", menu_id::NewDungeon);
    menu.add("How to Play", "", menu_id::HowToPlay);
    menu.add("Main Menu", "", menu_id::MainMenu);
    menu.setLayout({494.f, 288.f}, 352.f, 46.f, 8.f);
}

void setupEndMenu(Menu& menu) {
    menu.clear();
    menu.add("New Dungeon", "Random seed", menu_id::NewDungeon);
    menu.add("Retry This Dungeon", "R", menu_id::Restart);
    menu.add("Main Menu", "", menu_id::MainMenu);
    menu.setLayout({490.f, 494.f}, 360.f, 44.f, 8.f);
}

void drawTitle(sf::RenderTarget& t, const Fonts& f, const Menu& menu, bool seedEntry, const std::string& seedText, float time) {
    // Darken the backdrop: heavy on the left where the text sits.
    draw::hGradientRect(t, {{0.f, 0.f}, {kW, kH}}, sf::Color(8, 10, 16, 252), sf::Color(8, 10, 16, 120));
    draw::gradientRect(t, {{0.f, 0.f}, {kW, 180.f}}, sf::Color(8, 10, 16, 210), sf::Color(8, 10, 16, 0));
    draw::gradientRect(t, {{0.f, kH - 190.f}, {kW, 190.f}}, sf::Color(8, 10, 16, 0), sf::Color(8, 10, 16, 240));

    const float x = 96.f;
    draw::portalIcon(t, {x + 24.f, 138.f}, 1.6f, time);
    draw::tag(t, f.bold, "DATA STRUCTURES & ALGORITHMS  ·  LAB PROJECT", {x + 66.f + 168.f, 138.f}, theme::teal);

    draw::text(t, f.title, "DUNGEONBOUND", 64, {x - 3.f, 228.f}, theme::gold, Align::Left, 1.6f);
    draw::text(t, f.bold, "THE LOST REALMS", 16, {x + 2.f, 286.f}, theme::text, Align::Left, 7.f);
    draw::rect(t, {{x + 2.f, 310.f}, {72.f, 2.f}}, theme::gold);
    draw::wrapped(t, f.regular,
                  "Explore a procedurally generated dungeon, gather relics and outwit its guardians. "
                  "Every system runs on hand-built data structures.",
                  14, {x + 2.f, 326.f}, 500.f, theme::muted, 22.f);

    menu.draw(t, f, theme::gold);

    // Seed entry field, drawn over the "Play a Seed" row.
    const int seedRow = menu.indexOf(menu_id::PlaySeed);
    if (seedEntry && seedRow >= 0) {
        const sf::FloatRect r = menu.itemRect(seedRow);
        const sf::FloatRect field{{r.position.x + r.size.x - 186.f, r.position.y + 8.f}, {174.f, r.size.y - 16.f}};
        const float cy = field.position.y + field.size.y / 2.f;
        draw::roundedRect(t, field, 6.f, sf::Color(10, 12, 18), theme::gold, 1.f);
        const bool caret = std::fmod(time, 1.f) < 0.55f;
        if (seedText.empty()) {
            draw::text(t, f.regular, "type a number", 12, {field.position.x + 12.f, cy}, theme::dim);
            if (caret) draw::rect(t, {{field.position.x + 8.f, cy - 9.f}, {1.5f, 18.f}}, theme::gold);
        } else {
            draw::text(t, f.mono, seedText, 15, {field.position.x + 12.f, cy}, theme::text);
            if (caret) draw::rect(t, {{field.position.x + 14.f + draw::textWidth(f.mono, seedText, 15), cy - 9.f}, {1.5f, 18.f}}, theme::gold);
        }
    }

    float hx = x;
    const float hy = menu.itemRect(menu.size() - 1).position.y + 48.f + 34.f;
    if (seedEntry) {
        hint(t, f, hx, hy, "0-9", "Type a seed");
        hint(t, f, hx, hy, "Enter", "Start");
        hint(t, f, hx, hy, "Esc", "Cancel");
    } else {
        hint(t, f, hx, hy, "W", "");
        hx -= 2.f;
        hint(t, f, hx, hy, "S", "Navigate");
        hint(t, f, hx, hy, "Enter", "Select");
        hint(t, f, hx, hy, "Mouse", "Click");
    }

    // Footer: credits on the left, the four core DSA components on the right.
    draw::text(t, f.regular, kCredits, 11, {x, kH - 38.f}, theme::dim);
    const char* tags[4] = {"BINARY HEAP", "A* PATHFINDING", "RECURSIVE BACKTRACKING", "GRAPH"};
    float tx = kW - 40.f;
    for (const char* label : tags) {
        const float w = draw::textWidth(f.bold, label, 10, 1.6f) + 22.f;
        draw::tag(t, f.bold, label, {tx - w / 2.f, kH - 38.f}, theme::muted);
        tx -= w + 8.f;
    }
}

void drawPause(sf::RenderTarget& t, const Fonts& f, const Menu& menu, const GameSession& s, float playSeconds) {
    draw::scrim(t, 175);
    const sf::FloatRect c{{470.f, 170.f}, {400.f, 440.f}};
    card(t, c);
    const float cx = c.position.x + c.size.x / 2.f;
    draw::text(t, f.title, "PAUSED", 26, {cx, c.position.y + 44.f}, theme::gold, Align::Center, 3.f);
    draw::text(t, f.mono,
               "Seed " + std::to_string(s.seed()) + "  ·  " + Hud::formatTime(playSeconds) + "  ·  " + std::to_string(s.exploredPercent()) + "% explored",
               12, {cx, c.position.y + 76.f}, theme::muted, Align::Center);
    draw::rect(t, {{c.position.x + 24.f, c.position.y + 98.f}, {c.size.x - 48.f, 1.f}}, theme::border);
    menu.draw(t, f, theme::gold);
    float hx = cx - 92.f;
    const float hy = c.position.y + c.size.y - 30.f;
    hint(t, f, hx, hy, "Enter", "Select");
    hint(t, f, hx, hy, "Esc", "Resume");
}

void drawHelp(sf::RenderTarget& t, const Fonts& f, float time) {
    draw::scrim(t, 195);
    const sf::FloatRect p{{150.f, 70.f}, {1040.f, 640.f}};
    card(t, p);
    const float px = p.position.x, py = p.position.y;

    draw::text(t, f.title, "HOW TO PLAY", 24, {px + 32.f, py + 40.f}, theme::gold, Align::Left, 2.f);
    draw::text(t, f.regular, "Reach the exit portal alive. The Warden guards it.", 13, {px + 250.f, py + 42.f}, theme::muted);
    float cx = px + p.size.x - 110.f;
    hint(t, f, cx, py + 40.f, "Esc", "Close");
    draw::rect(t, {{px + 32.f, py + 68.f}, {p.size.x - 64.f, 1.f}}, theme::border);

    // ---- Column 1: controls ---------------------------------------------------------
    const float c1 = px + 32.f;
    draw::sectionLabel(t, f.bold, "EXPLORING", {c1, py + 96.f}, theme::gold);
    struct KeyRow {
        const char* key;
        const char* desc;
    };
    const KeyRow explore[] = {
        {"WASD", "Move (arrow keys also work)"},
        {"Click", "Plot an A* route to a tile"},
        {"Enter", "Walk or stop the plotted route"},
        {"Space", "Take a single step on the route"},
        {"P", "Plot a route to the exit portal"},
        {"RMB", "Clear the route"},
        {"I", "Open the inventory"},
        {"Tab", "Cycle overlays: A* search, graph"},
        {"F2", "Reveal the whole map (demo aid)"},
        {"H", "Show this help"},
        {"Esc", "Pause or go back"},
    };
    float y = py + 126.f;
    for (const KeyRow& row : explore) {
        draw::keycap(t, f.bold, row.key, {c1, y}, 10);
        draw::text(t, f.regular, row.desc, 13, {c1 + 72.f, y}, theme::text);
        y += 30.f;
    }
    y += 8.f;
    draw::sectionLabel(t, f.bold, "INVENTORY & BATTLE", {c1, y}, theme::gold);
    y += 30.f;
    const KeyRow other[] = {
        {"U", "Use the selected item"},
        {"X", "Discard it (press twice)"},
        {"1-4", "Attack, Defend, Item, Escape"},
        {"Enter", "Confirm the highlighted action"},
    };
    for (const KeyRow& row : other) {
        draw::keycap(t, f.bold, row.key, {c1, y}, 10);
        draw::text(t, f.regular, row.desc, 13, {c1 + 72.f, y}, theme::text);
        y += 30.f;
    }

    // ---- Column 2: legend -------------------------------------------------------------
    const float c2 = px + 384.f;
    draw::rect(t, {{c2 - 24.f, py + 88.f}, {1.f, p.size.y - 120.f}}, theme::border);
    draw::sectionLabel(t, f.bold, "LEGEND", {c2, py + 96.f}, theme::gold);
    y = py + 128.f;
    auto legend = [&](const std::string& name, const std::string& detail) {
        draw::text(t, f.bold, name, 13, {c2 + 36.f, y - 7.f}, theme::text);
        draw::text(t, f.regular, detail, 11, {c2 + 36.f, y + 9.f}, theme::muted);
        y += 37.f;
    };
    draw::playerIcon(t, {c2 + 12.f, y}, 0.9f, 1);
    legend("You", "The marker shows the way you face");
    draw::portalIcon(t, {c2 + 12.f, y}, 0.7f, time);
    legend("Exit portal", "Step in to win the run");
    for (int k = 0; k < static_cast<int>(EnemyKind::Count); ++k) {
        const EnemyKind kind = static_cast<EnemyKind>(k);
        const EnemyInfo& info = enemyInfo(kind);
        draw::enemyIcon(t, kind, {c2 + 12.f, y + (kind == EnemyKind::Warden ? 3.f : 0.f)}, kind == EnemyKind::Warden ? 0.72f : 0.8f, time);
        legend(info.name, std::to_string(info.stats.maxHp) + " HP  ·  " + std::to_string(info.stats.attack) + " ATK  ·  " +
                              std::to_string(info.stats.speed) + " SPD" + (kind == EnemyKind::Warden ? "  ·  boss" : ""));
    }
    for (int k = 0; k < static_cast<int>(ItemType::Count); ++k) {
        const ItemType type = static_cast<ItemType>(k);
        draw::itemIcon(t, type, {c2 + 12.f, y}, 0.95f);
        legend(itemInfo(type).name, itemInfo(type).effect);
    }

    // ---- Column 3: algorithms -----------------------------------------------------------
    const float c3 = px + 706.f;
    draw::rect(t, {{c3 - 24.f, py + 88.f}, {1.f, p.size.y - 120.f}}, theme::border);
    draw::sectionLabel(t, f.bold, "UNDER THE HOOD", {c3, py + 96.f}, theme::gold);
    struct Blurb {
        const char* title;
        const char* body;
    };
    const Blurb blurbs[] = {
        {"Graph  ·  adjacency list",
         "Every walkable tile is a vertex. Neighbouring tiles share an edge, stored in hand-written linked lists. Press Tab twice to see it."},
        {"Recursive backtracking",
         "The generator carves a maze cell by cell and backtracks at dead ends. Rooms and loops are added, then a graph search proves "
         "every tile is reachable."},
        {"A* pathfinding",
         "Click a tile to find the shortest route using a Manhattan-distance heuristic. Press Tab to see every node the search expanded."},
        {"Binary heap",
         "One custom heap orders the A* open set by cost and the battle turn queue by each fighter's next action time."},
    };
    y = py + 126.f;
    for (const Blurb& b : blurbs) {
        draw::rect(t, {{c3, y - 7.f}, {3.f, 14.f}}, theme::teal);
        draw::text(t, f.bold, b.title, 13, {c3 + 12.f, y}, theme::text);
        y += 14.f;
        y += draw::wrapped(t, f.regular, b.body, 12, {c3 + 12.f, y}, 290.f, theme::muted, 19.f) + 22.f;
    }
}

void drawEnd(sf::RenderTarget& t, const Fonts& f, const Menu& menu, const GameSession& s, bool victory, float playSeconds, float time) {
    draw::scrim(t, victory ? 165 : 190);
    if (!victory) draw::rect(t, {{0.f, 0.f}, {kW, kH}}, sf::Color(90, 14, 14, 46));

    const sf::Color accent = victory ? theme::gold : theme::red;
    const sf::FloatRect c{{340.f, 88.f}, {660.f, 604.f}};
    card(t, c, draw::withAlpha(accent, 150));
    const float cx = c.position.x + c.size.x / 2.f, cy = c.position.y;

    if (victory) {
        draw::portalIcon(t, {cx, cy + 68.f}, 2.1f, time);
    } else {
        draw::glow(t, {cx, cy + 68.f}, 60.f, sf::Color(229, 86, 78, 90), 5);
        draw::enemyIcon(t, EnemyKind::Skeleton, {cx, cy + 68.f}, 2.5f, time);
    }
    draw::text(t, f.title, victory ? "VICTORY" : "DEFEATED", 40, {cx, cy + 142.f}, accent, Align::Center, 4.f);
    draw::text(t, f.regular, victory ? "You stepped through the portal and escaped the Lost Realms." : "The dungeon claims another adventurer.", 14,
               {cx, cy + 180.f}, theme::muted, Align::Center);

    const RunStats& st = s.stats();
    struct Cell {
        const char* label;
        std::string value;
    };
    const Cell cells[8] = {
        {"TIME", Hud::formatTime(playSeconds)},
        {"STEPS", std::to_string(st.steps)},
        {"LEVEL", std::to_string(s.player().level)},
        {"EXPLORED", std::to_string(s.exploredPercent()) + "%"},
        {"FOES SLAIN", std::to_string(st.kills) + " / " + std::to_string(s.enemies().size())},
        {"ITEMS FOUND", std::to_string(st.itemsCollected)},
        {"DAMAGE DEALT", std::to_string(st.damageDealt)},
        {"DAMAGE TAKEN", std::to_string(st.damageTaken)},
    };
    const float gx = c.position.x + 32.f, gy = cy + 212.f, gap = 10.f;
    const float cw = (c.size.x - 64.f - 3.f * gap) / 4.f, ch = 64.f;
    for (int i = 0; i < 8; ++i) {
        const sf::FloatRect r{{gx + (i % 4) * (cw + gap), gy + (i / 4) * (ch + gap)}, {cw, ch}};
        draw::roundedRect(t, r, 8.f, theme::panelHi, theme::border, 1.f);
        draw::text(t, f.bold, cells[i].label, 9, {r.position.x + 14.f, r.position.y + 19.f}, theme::dim, Align::Left, 1.6f);
        draw::text(t, f.bold, cells[i].value, 19, {r.position.x + 14.f, r.position.y + 43.f}, theme::text);
    }
    draw::text(t, f.mono, "Dungeon seed " + std::to_string(s.seed()) + "  ·  enter it on the title screen to replay this layout", 11,
               {cx, cy + 374.f}, theme::dim, Align::Center);

    menu.draw(t, f, accent);
}

}  // namespace screens
