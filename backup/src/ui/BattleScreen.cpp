#include "BattleScreen.hpp"

#include "Draw.hpp"
#include "HUD.hpp"
#include "Theme.hpp"

#include <cmath>
#include <cstdio>

using draw::Align;

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kLunge = 0.32f;
constexpr float kShake = 0.36f;
constexpr float kFlash = 0.26f;
constexpr float kEnemyDelay = 0.85f;

const sf::FloatRect kTurns{{16.f, 68.f}, {1308.f, 74.f}};
const sf::FloatRect kArena{{16.f, 154.f}, {1308.f, 372.f}};
const sf::FloatRect kActions{{16.f, 538.f}, {660.f, 230.f}};
const sf::FloatRect kLog{{688.f, 538.f}, {636.f, 230.f}};
const sf::Vector2f kAnchor[2] = {{360.f, 398.f}, {980.f, 398.f}};

sf::FloatRect actionRect(int i) { return {{34.f + (i % 2) * 318.f, 578.f + (i / 2) * 90.f}, {306.f, 78.f}}; }

sf::FloatRect plateRect(int who) { return who == 0 ? sf::FloatRect{{40.f, 172.f}, {420.f, 96.f}} : sf::FloatRect{{880.f, 172.f}, {420.f, 96.f}}; }

float clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

float easeOut(float k) {
    k = 1.f - clamp01(k);
    return 1.f - k * k * k;
}

std::string upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

std::string fmt1(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}
}  // namespace

// ------------------------------------------------------------------ lifecycle
void BattleScreen::begin(const GameSession& s) {
    selected_ = 0;
    hover_ = -1;
    time_ = intro_ = lock_ = enemyTimer_ = overTimer_ = 0.f;
    for (int i = 0; i < 2; ++i) shake_[i] = flash_[i] = lunge_[i] = 0.f;
    feedback_.clear();
    feedbackTimer_ = 0.f;
    floaters_.clear();

    const BattleSystem& b = s.battle();
    hpShown_[0] = hpGhost_[0] = b.player() ? static_cast<float>(b.player()->stats.hp) : 0.f;
    hpShown_[1] = hpGhost_[1] = b.enemy() ? static_cast<float>(b.enemy()->stats.hp) : 0.f;
}

void BattleScreen::setFeedback(const std::string& message) {
    feedback_ = message;
    feedbackTimer_ = message.empty() ? 0.f : 3.f;
}

void BattleScreen::onPlayerActed() {
    lock_ = 0.4f;
    enemyTimer_ = 0.f;
    feedbackTimer_ = 0.f;
}

// ---------------------------------------------------------------------- input
BattleUiEvent BattleScreen::activate(int action, GameSession& s) {
    BattleSystem& b = s.battle();
    if (b.isOver()) return overTimer_ > 0.45f ? BattleUiEvent::Finished : BattleUiEvent::None;
    if (b.phase() != BattlePhase::PlayerTurn) {
        setFeedback("Wait for your turn.");
        return BattleUiEvent::None;
    }
    if (lock_ > 0.f) return BattleUiEvent::None;

    ActionResult r;
    switch (action) {
        case 0: r = b.playerAct(BattleAction::Attack); break;
        case 1: r = b.playerAct(BattleAction::Defend); break;
        case 2:
            // An empty pack is rejected by the battle rules without costing the turn (FR-B5).
            if (!s.inventory().isEmpty()) return BattleUiEvent::OpenItems;
            r = b.playerAct(BattleAction::UseItem, -1);
            break;
        case 3: r = b.playerAct(BattleAction::Escape); break;
        default: return BattleUiEvent::None;
    }
    if (r.accepted) onPlayerActed();
    else setFeedback(r.feedback);
    return BattleUiEvent::None;
}

BattleUiEvent BattleScreen::handleInput(const Input& in, GameSession& s) {
    if (s.battle().isOver()) {
        bool confirm = in.cmd == Command::Confirm || in.cmd == Command::Step;
        return confirm && overTimer_ > 0.45f ? BattleUiEvent::Finished : BattleUiEvent::None;
    }
    switch (in.cmd) {
        case Command::MoveLeft:
            if (selected_ % 2 == 1) --selected_;
            break;
        case Command::MoveRight:
            if (selected_ % 2 == 0) ++selected_;
            break;
        case Command::MoveUp:
            if (selected_ >= 2) selected_ -= 2;
            break;
        case Command::MoveDown:
            if (selected_ < 2) selected_ += 2;
            break;
        case Command::Digit:
            if (in.digit >= 1 && in.digit <= 4) {
                selected_ = in.digit - 1;
                return activate(selected_, s);
            }
            break;
        case Command::Confirm:
        case Command::Step:
            return activate(selected_, s);
        case Command::Inventory:
            selected_ = 2;
            return activate(2, s);
        default:
            break;
    }
    return BattleUiEvent::None;
}

void BattleScreen::handleHover(sf::Vector2f mouse) {
    hover_ = -1;
    for (int i = 0; i < 4; ++i) {
        if (actionRect(i).contains(mouse)) hover_ = i;
    }
}

BattleUiEvent BattleScreen::handleClick(sf::Vector2f mouse, GameSession& s) {
    if (s.battle().isOver()) return overTimer_ > 0.45f ? BattleUiEvent::Finished : BattleUiEvent::None;
    for (int i = 0; i < 4; ++i) {
        if (actionRect(i).contains(mouse)) {
            selected_ = i;
            return activate(i, s);
        }
    }
    return BattleUiEvent::None;
}

// --------------------------------------------------------------------- update
void BattleScreen::spawnFloater(int who, const std::string& text, sf::Color color, unsigned size) {
    Floater fl;
    fl.pos = {kAnchor[who].x + (who == 0 ? 34.f : -34.f), kAnchor[who].y - 84.f - 18.f * (floaters_.size() % 3)};
    fl.text = text;
    fl.color = color;
    fl.size = size;
    fl.life = 1.15f;
    floaters_.push_back(fl);
}

void BattleScreen::consumeFx(BattleSystem& b) {
    BattleFx fx;
    while (b.popFx(fx)) {
        const int who = fx.target == Actor::Player ? 0 : 1;
        switch (fx.type) {
            case BattleFx::Type::Damage:
                spawnFloater(who, "-" + std::to_string(fx.amount), fx.critical ? theme::gold : (who == 0 ? theme::red : theme::text),
                             fx.critical ? 34u : 26u);
                shake_[who] = kShake;
                flash_[who] = kFlash;
                lunge_[1 - who] = kLunge;
                break;
            case BattleFx::Type::Heal:
                spawnFloater(who, "+" + std::to_string(fx.amount), theme::green, 26u);
                break;
            case BattleFx::Type::Guard:
                spawnFloater(who, "GUARD", theme::blue, 18u);
                break;
            case BattleFx::Type::Miss:
                spawnFloater(who, "NO ESCAPE", theme::orange, 18u);
                break;
            case BattleFx::Type::Buff:
                spawnFloater(who, "POWER UP", theme::violet, 18u);
                break;
        }
    }
}

void BattleScreen::update(float dt, GameSession& s) {
    time_ += dt;
    intro_ = clamp01(intro_ + dt * 2.6f);
    if (lock_ > 0.f) lock_ -= dt;
    if (feedbackTimer_ > 0.f) feedbackTimer_ -= dt;
    for (int i = 0; i < 2; ++i) {
        if (shake_[i] > 0.f) shake_[i] -= dt;
        if (flash_[i] > 0.f) flash_[i] -= dt;
        if (lunge_[i] > 0.f) lunge_[i] -= dt;
    }
    for (int i = floaters_.size() - 1; i >= 0; --i) {
        floaters_[i].age += dt;
        if (floaters_[i].age >= floaters_[i].life) floaters_.removeAt(i);
    }

    BattleSystem& b = s.battle();
    if (!b.active() || !b.player() || !b.enemy()) return;
    consumeFx(b);

    // Health bars ease toward the real value; a pale "ghost" trails behind.
    const float target[2] = {static_cast<float>(b.player()->stats.hp), static_cast<float>(b.enemy()->stats.hp)};
    const float maxHp[2] = {static_cast<float>(b.player()->stats.maxHp), static_cast<float>(b.enemy()->stats.maxHp)};
    for (int i = 0; i < 2; ++i) {
        hpShown_[i] += (target[i] - hpShown_[i]) * clamp01(dt * 12.f);
        if (hpGhost_[i] > hpShown_[i]) {
            hpGhost_[i] -= dt * maxHp[i] * 0.4f;
            if (hpGhost_[i] < hpShown_[i]) hpGhost_[i] = hpShown_[i];
        } else {
            hpGhost_[i] = hpShown_[i];
        }
    }

    if (b.isOver()) {
        overTimer_ += dt;
    } else if (b.phase() == BattlePhase::EnemyTurn) {
        // The enemy acts on its own after a short, readable pause.
        enemyTimer_ += dt;
        if (enemyTimer_ >= kEnemyDelay && lock_ <= 0.f) {
            enemyTimer_ = 0.f;
            b.enemyAct();
            lock_ = 0.3f;
        }
    } else {
        enemyTimer_ = 0.f;
    }
}

// ----------------------------------------------------------------------- draw
void BattleScreen::draw(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
    const BattleSystem& b = s.battle();
    draw::gradientRect(t, {{0.f, 0.f}, {float(theme::kWindowW), float(theme::kWindowH)}}, theme::bgTop, theme::bg);
    if (!b.player() || !b.enemy()) return;

    drawTopBar(t, f, b);
    drawTurnOrder(t, f, b);
    drawArena(t, f, b);
    drawActions(t, f, s);
    drawLog(t, f, b);
    if (b.isOver()) drawResult(t, f, b);
}

void BattleScreen::drawTopBar(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
    const float W = static_cast<float>(theme::kWindowW);
    draw::gradientRect(t, {{0.f, 0.f}, {W, theme::kTopBarH}}, sf::Color(20, 24, 36), sf::Color(14, 17, 26));
    draw::rect(t, {{0.f, theme::kTopBarH - 1.f}, {W, 1.f}}, theme::border);

    draw::portalIcon(t, {32.f, 28.f}, 0.85f, time_);
    draw::text(t, f.title, "DUNGEONBOUND", 20, {56.f, 27.f}, theme::gold, Align::Left, 1.5f);
    float tw = draw::textWidth(f.title, "DUNGEONBOUND", 20, 1.5f);
    draw::text(t, f.regular, "The Lost Realms", 13, {66.f + tw, 28.f}, theme::muted);

    const Enemy& e = *b.enemy();
    const EnemyInfo& info = enemyInfo(e.kind);
    const std::string label = e.boss ? "BOSS BATTLE" : "ENCOUNTER";
    const sf::Color accent = e.boss ? theme::violet : theme::red;
    float lw = draw::textWidth(f.bold, label, 10, 1.6f) + 22.f;
    draw::tag(t, f.bold, label, {W - 16.f - lw / 2.f, 28.f}, accent);
    draw::text(t, f.regular, std::string(info.name) + "  ·  " + info.title, 13, {W - 28.f - lw, 28.f}, theme::muted, Align::Right);
}

void BattleScreen::drawTurnOrder(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
    draw::panel(t, kTurns);
    draw::sectionLabel(t, f.bold, "TURN ORDER", {34.f, 94.f});
    draw::text(t, f.regular, "Binary min-heap  ·  key = next action time", 11, {34.f, 118.f}, theme::dim);

    DynamicArray<TurnEvent> order = b.upcoming(8);
    if (order.empty()) {
        draw::text(t, f.regular, "The turn queue is empty: the battle is over.", 13, {300.f, 105.f}, theme::muted);
        return;
    }
    const EnemyKind kind = b.enemy()->kind;
    const std::string enemyName = upper(b.enemy()->name());
    for (int i = 0; i < order.size(); ++i) {
        sf::FloatRect r{{286.f + i * 128.f, 82.f}, {118.f, 46.f}};
        const bool isPlayer = order[i].actor == Actor::Player;
        const bool now = i == 0;
        const sf::Color accent = isPlayer ? theme::gold : draw::enemyColor(kind);
        const float cy = r.position.y + r.size.y / 2.f;

        draw::roundedRect(t, r, 8.f, now ? draw::mix(theme::panelHi, accent, 0.2f) : theme::panelHi, now ? accent : theme::border, 1.f);
        if (i > 0) {  // chevron between chips
            const float ax = r.position.x - 7.f;
            draw::line(t, {ax - 2.f, cy - 4.f}, {ax + 2.f, cy}, theme::dim, 1.5f);
            draw::line(t, {ax + 2.f, cy}, {ax - 2.f, cy + 4.f}, theme::dim, 1.5f);
        }
        if (isPlayer) draw::playerIcon(t, {r.position.x + 23.f, cy}, 0.8f, 1, now ? 1.f : 0.75f);
        else draw::enemyIcon(t, kind, {r.position.x + 23.f, cy + 1.f}, 0.62f, time_, now ? 1.f : 0.75f);

        draw::text(t, f.bold, isPlayer ? "YOU" : enemyName, 11, {r.position.x + 44.f, r.position.y + 16.f}, now ? theme::text : theme::muted,
                   Align::Left, 1.2f);
        draw::text(t, f.mono, now ? "ACTING" : "t = " + fmt1(order[i].time), 10, {r.position.x + 44.f, r.position.y + 32.f},
                   now ? accent : theme::dim);
    }
}

void BattleScreen::drawNameplate(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b, int who) const {
    const sf::FloatRect r = plateRect(who);
    const Stats& st = who == 0 ? b.player()->stats : b.enemy()->stats;
    const bool guarding = who == 0 ? b.playerDefending() : b.enemyDefending();
    const sf::Color accent = who == 0 ? theme::gold : draw::enemyColor(b.enemy()->kind);
    const float x = r.position.x, y = r.position.y, w = r.size.x;

    draw::roundedRect(t, {{x, y + 3.f}, r.size}, 10.f, sf::Color(0, 0, 0, 80));
    draw::roundedRect(t, r, 10.f, sf::Color(19, 23, 34, 240), theme::border, 1.f);
    draw::rect(t, {{x + 1.f, y + 14.f}, {3.f, r.size.y - 28.f}}, accent);

    const std::string name = who == 0 ? "Adventurer" : b.enemy()->name();
    const std::string sub = who == 0 ? "Level " + std::to_string(b.player()->level) : enemyInfo(b.enemy()->kind).title;
    draw::text(t, f.bold, name, 16, {x + 20.f, y + 23.f}, theme::text);
    draw::text(t, f.regular, sub, 12, {x + 30.f + draw::textWidth(f.bold, name, 16), y + 24.f}, theme::muted);
    draw::text(t, f.mono, std::to_string(st.hp) + " / " + std::to_string(st.maxHp), 13, {x + w - 20.f, y + 23.f}, theme::text, Align::Right);

    const float frac = st.maxHp > 0 ? hpShown_[who] / st.maxHp : 0.f;
    const float ghost = st.maxHp > 0 ? hpGhost_[who] / st.maxHp : 0.f;
    draw::bar(t, {{x + 20.f, y + 41.f}, {w - 40.f, 14.f}}, frac, draw::hpColor(frac), sf::Color(8, 10, 16), ghost);

    draw::text(t, f.mono, "ATK " + std::to_string(st.attack) + "    DEF " + std::to_string(st.defense) + "    SPD " + std::to_string(st.speed),
               11, {x + 20.f, y + 76.f}, theme::muted);
    float tx = x + w - 20.f;
    if (guarding) {
        float tw = draw::textWidth(f.bold, "GUARDING", 10, 1.6f) + 22.f;
        draw::tag(t, f.bold, "GUARDING", {tx - tw / 2.f, y + 76.f}, theme::blue);
        tx -= tw + 8.f;
    }
    if (who == 1 && b.enemy()->boss) {
        float tw = draw::textWidth(f.bold, "BOSS", 10, 1.6f) + 22.f;
        draw::tag(t, f.bold, "BOSS", {tx - tw / 2.f, y + 76.f}, theme::violet);
    }
}

void BattleScreen::drawArena(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
    draw::panel(t, kArena, sf::Color(14, 17, 26));
    const float ax = kArena.position.x + 4.f, ay = kArena.position.y + 4.f;
    const float aw = kArena.size.x - 8.f, ah = kArena.size.y - 8.f;
    const float horizon = 432.f;
    draw::gradientRect(t, {{ax, ay}, {aw, horizon - ay}}, sf::Color(14, 17, 27), sf::Color(22, 26, 41));
    draw::gradientRect(t, {{ax, horizon}, {aw, ay + ah - horizon}}, sf::Color(30, 35, 52), sf::Color(18, 21, 32));
    draw::rect(t, {{ax, horizon}, {aw, 1.f}}, sf::Color(255, 255, 255, 14));

    const EnemyKind kind = b.enemy()->kind;
    const bool dead[2] = {b.player()->stats.hp <= 0, b.enemy()->stats.hp <= 0};
    const bool guarding[2] = {b.playerDefending(), b.enemyDefending()};
    const float enter = easeOut(intro_);

    draw::glow(t, {kAnchor[0].x, kAnchor[0].y - 20.f}, 190.f, sf::Color(255, 190, 110, 34), 6);
    draw::glow(t, {kAnchor[1].x, kAnchor[1].y - 20.f}, 210.f, draw::withAlpha(draw::enemyColor(kind), 40), 6);

    // Centre: VS marker and turn counter.
    draw::text(t, f.title, "VS", 26, {670.f, 208.f}, sf::Color(74, 82, 104), Align::Center, 2.f);
    draw::text(t, f.mono, "TURN " + std::to_string(b.round() + (b.isOver() ? 0 : 1)), 11, {670.f, 238.f}, theme::dim, Align::Center);

    for (int who = 0; who < 2; ++who) {
        const float dir = who == 0 ? 1.f : -1.f;
        sf::Vector2f pos = kAnchor[who];
        const sf::Vector2f base = pos;
        pos.x -= dir * (1.f - enter) * 140.f;
        if (lunge_[who] > 0.f) pos.x += dir * 76.f * std::sin(kPi * (1.f - lunge_[who] / kLunge));
        if (shake_[who] > 0.f) pos.x += std::sin(time_ * 85.f) * 9.f * (shake_[who] / kShake);
        float alpha = enter * (dead[who] ? 0.22f : 1.f);
        if (dead[who]) pos.y += 14.f;

        // Platform.
        draw::oval(t, {base.x, base.y + 62.f}, 104.f, 22.f, sf::Color(0, 0, 0, 120));
        draw::oval(t, {base.x, base.y + 58.f}, 96.f, 18.f, sf::Color(40, 46, 66));
        draw::oval(t, {base.x, base.y + 56.f}, 88.f, 15.f, sf::Color(52, 59, 82));

        if (who == 0) {
            draw::playerIcon(t, pos, 4.8f, 1, alpha);
        } else {
            const float bob = dead[1] ? 0.f : std::sin(time_ * 2.2f) * 4.f;
            draw::enemyIcon(t, kind, {pos.x, pos.y + bob - (b.enemy()->boss ? 0.f : 4.f)}, b.enemy()->boss ? 4.6f : 5.0f, time_, alpha);
        }
        if (flash_[who] > 0.f)
            draw::glow(t, pos, 96.f, sf::Color(255, 255, 255, static_cast<std::uint8_t>(210.f * flash_[who] / kFlash)), 5);
        if (guarding[who] && !dead[who]) {
            const float pulse = 0.5f + 0.5f * std::sin(time_ * 4.f);
            draw::ring(t, pos, 70.f + 3.f * pulse, 3.f, draw::withAlpha(theme::blue, 150 + static_cast<int>(70 * pulse)));
            draw::ring(t, pos, 78.f + 3.f * pulse, 1.5f, draw::withAlpha(theme::blue, 70));
        }
        drawNameplate(t, f, b, who);
    }

    // Floating combat text.
    for (const Floater& fl : floaters_) {
        const float k = fl.age / fl.life;
        const float y = fl.pos.y - 64.f * easeOut(k);
        const int alpha = k < 0.65f ? 255 : static_cast<int>(255.f * (1.f - (k - 0.65f) / 0.35f));
        draw::text(t, f.bold, fl.text, fl.size, {fl.pos.x, y + 2.f}, sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha * 0.7f)), Align::Center);
        draw::text(t, f.bold, fl.text, fl.size, {fl.pos.x, y}, draw::withAlpha(fl.color, alpha), Align::Center);
    }
}

void BattleScreen::drawActions(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
    const BattleSystem& b = s.battle();
    const bool playerTurn = b.phase() == BattlePhase::PlayerTurn;
    draw::panel(t, kActions);

    const float hy = kActions.position.y + 21.f;
    if (b.isOver()) {
        draw::sectionLabel(t, f.bold, "BATTLE OVER", {34.f, hy}, theme::muted);
    } else if (playerTurn) {
        draw::sectionLabel(t, f.bold, "YOUR MOVE", {34.f, hy}, theme::gold);
    } else {
        const std::string dots(1 + static_cast<int>(time_ * 3.f) % 3, '.');
        draw::sectionLabel(t, f.bold, upper(b.enemy()->name()) + " IS ACTING " + dots, {34.f, hy}, theme::red);
    }
    const float right = kActions.position.x + kActions.size.x - 18.f;
    if (feedbackTimer_ > 0.f && !feedback_.empty()) {
        const int alpha = feedbackTimer_ < 0.5f ? static_cast<int>(255 * feedbackTimer_ / 0.5f) : 255;
        draw::text(t, f.regular, feedback_, 12, {right, hy}, draw::withAlpha(theme::orange, alpha), Align::Right);
    } else if (!b.isOver()) {
        draw::text(t, f.regular, "1-4 or arrows + Enter", 11, {right, hy}, theme::dim, Align::Right);
    }

    int lo = 0, hi = 0;
    b.damageRange(b.player()->stats, b.enemy()->stats, lo, hi);
    if (b.enemyDefending()) {
        lo = lo / 2 < 1 ? 1 : lo / 2;
        hi = hi / 2 < 1 ? 1 : hi / 2;
    }
    const int items = s.inventory().size();
    const char* titles[4] = {"Attack", "Defend", "Item", "Escape"};
    const std::string subs[4] = {
        "Deal " + std::to_string(lo) + "-" + std::to_string(hi) + " damage" + (b.enemyDefending() ? " (enemy guarding)" : ""),
        "Halve damage until your next turn",
        items == 0 ? "Your pack is empty" : std::to_string(items) + (items == 1 ? " item in your pack" : " items in your pack"),
        b.canEscape() ? std::to_string(static_cast<int>(b.escapeChance() * 100.0 + 0.5)) + "% chance to flee" : "Impossible in a boss fight",
    };
    const sf::Color tints[4] = {theme::orange, theme::blue, theme::red, theme::teal};

    for (int i = 0; i < 4; ++i) {
        const sf::FloatRect r = actionRect(i);
        const bool sel = playerTurn && i == selected_;
        const bool hov = playerTurn && i == hover_;
        const float cy = r.position.y + r.size.y / 2.f;

        sf::Color fill = theme::panelHi, border = theme::border;
        if (!playerTurn) fill = sf::Color(17, 20, 30);
        else if (sel) {
            fill = draw::mix(theme::panelHi, theme::gold, 0.14f);
            border = theme::gold;
        } else if (hov) {
            fill = theme::panelHover;
            border = theme::borderHi;
        }
        draw::roundedRect(t, {{r.position.x, r.position.y + 2.f}, r.size}, 9.f, sf::Color(0, 0, 0, 70));
        draw::roundedRect(t, r, 9.f, fill, border, 1.f);

        const float a = playerTurn ? 1.f : 0.4f;
        const sf::Vector2f ic{r.position.x + 42.f, cy};
        const sf::Color tint = draw::withAlpha(tints[i], static_cast<int>(255 * a));
        draw::circle(t, ic, 25.f, sf::Color(13, 16, 24), draw::withAlpha(tints[i], static_cast<int>(110 * a)), 1.f, 40);
        switch (i) {
            case 0:  // sword
                draw::line(t, {ic.x - 9.f, ic.y + 9.f}, {ic.x + 10.f, ic.y - 10.f}, tint, 3.2f);
                draw::line(t, {ic.x - 9.f, ic.y + 1.f}, {ic.x - 1.f, ic.y + 9.f}, tint, 3.2f);
                break;
            case 1: draw::itemIcon(t, ItemType::IronSkin, {ic.x, ic.y - 1.f}, 1.3f, a); break;
            case 2: draw::itemIcon(t, ItemType::HealthPotion, {ic.x, ic.y + 1.f}, 1.25f, a); break;
            default:  // double chevron
                for (float off : {-8.f, 1.f}) {
                    draw::line(t, {ic.x + off, ic.y - 7.f}, {ic.x + off + 7.f, ic.y}, tint, 2.6f);
                    draw::line(t, {ic.x + off + 7.f, ic.y}, {ic.x + off, ic.y + 7.f}, tint, 2.6f);
                }
                break;
        }

        draw::text(t, f.bold, titles[i], 15, {r.position.x + 80.f, r.position.y + 29.f}, playerTurn ? theme::text : theme::dim);
        draw::text(t, f.regular, subs[i], 12, {r.position.x + 80.f, r.position.y + 51.f}, playerTurn ? theme::muted : theme::dim);
        draw::keycap(t, f.bold, std::to_string(i + 1), {r.position.x + r.size.x - 36.f, r.position.y + 20.f}, 10);
    }
}

void BattleScreen::drawLog(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
    draw::panel(t, kLog);
    const float x = kLog.position.x, y = kLog.position.y;
    draw::sectionLabel(t, f.bold, "COMBAT LOG", {x + 18.f, y + 21.f});

    // Raw contents of the heap's backing array (index 0 is the root).
    std::string heap;
    for (const TurnEvent& ev : b.queue().raw()) {
        if (!heap.empty()) heap += "  ";
        heap += std::string(ev.actor == Actor::Player ? "YOU" : "FOE") + "@" + fmt1(ev.time);
    }
    if (heap.empty()) heap = "empty";
    draw::text(t, f.mono, "heap [ " + heap + " ]", 11, {x + kLog.size.x - 18.f, y + 21.f}, theme::teal, Align::Right);
    draw::rect(t, {{x + 18.f, y + 38.f}, {kLog.size.x - 36.f, 1.f}}, theme::border);

    const DynamicArray<LogLine>& log = b.log();
    const int rows = log.size() < 7 ? log.size() : 7;
    const int start = log.size() - rows;
    for (int row = 0; row < rows; ++row) {
        const LogLine& line = log[start + row];
        const int age = rows - 1 - row;  // 0 = newest
        const int alpha = age == 0 ? 255 : age == 1 ? 200 : age == 2 ? 160 : 120;
        const float ly = y + 58.f + row * 25.f;
        draw::circle(t, {x + 24.f, ly}, 3.f, draw::withAlpha(Hud::logColor(line.kind), alpha));
        draw::text(t, f.regular, line.text, 13, {x + 38.f, ly}, draw::withAlpha(age == 0 ? theme::text : theme::muted, alpha));
    }
}

void BattleScreen::drawResult(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
    const float k = easeOut(overTimer_ / 0.4f);
    std::string title, sub;
    sf::Color accent = theme::gold;
    const std::string enemy = b.enemy()->name();
    switch (b.phase()) {
        case BattlePhase::Won:
            title = "VICTORY";
            sub = "The " + enemy + " falls.  +" + std::to_string(enemyInfo(b.enemy()->kind).xp) + " XP";
            break;
        case BattlePhase::Escaped:
            title = "ESCAPED";
            sub = "You slip away. The " + enemy + " still lurks in the dark.";
            accent = theme::teal;
            break;
        default:
            title = "DEFEATED";
            sub = "The " + enemy + " strikes you down.";
            accent = theme::red;
            break;
    }

    draw::roundedRect(t, {{kArena.position.x + 1.f, kArena.position.y + 1.f}, {kArena.size.x - 2.f, kArena.size.y - 2.f}}, 10.f,
                      sf::Color(6, 8, 13, static_cast<std::uint8_t>(120.f * k)));
    const float cy = 340.f + (1.f - k) * 16.f;
    const int a = static_cast<int>(255.f * k);
    sf::FloatRect r{{410.f, cy - 84.f}, {520.f, 168.f}};
    draw::roundedRect(t, {{r.position.x, r.position.y + 6.f}, r.size}, 14.f, sf::Color(0, 0, 0, static_cast<std::uint8_t>(110.f * k)));
    draw::roundedRect(t, r, 14.f, sf::Color(19, 23, 34, static_cast<std::uint8_t>(248.f * k)), draw::withAlpha(accent, a), 1.5f);
    draw::text(t, f.title, title, 38, {670.f, cy - 36.f}, draw::withAlpha(accent, a), Align::Center, 3.f);
    draw::text(t, f.regular, sub, 14, {670.f, cy + 8.f}, draw::withAlpha(theme::muted, a), Align::Center);

    if (overTimer_ > 0.45f) {
        const float lw = draw::textWidth(f.regular, "Continue", 13);
        const float kw = draw::textWidth(f.bold, "Enter", 11) + 14.f;
        const float left = 670.f - (kw + 8.f + lw) / 2.f;
        draw::keycap(t, f.bold, "Enter", {left, cy + 50.f}, 11);
        draw::text(t, f.regular, "Continue", 13, {left + kw + 8.f, cy + 50.f}, theme::text);
    }
}
