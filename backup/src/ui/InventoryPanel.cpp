#include "InventoryPanel.hpp"

#include "Draw.hpp"
#include "Theme.hpp"

using draw::Align;

namespace {
const sf::FloatRect kPanel{{250.f, 112.f}, {840.f, 556.f}};
constexpr int kButtonCount = 3;  // 0 = use, 1 = discard, 2 = close

sf::FloatRect rowRect(int i) { return {{kPanel.position.x + 24.f, kPanel.position.y + 78.f + i * 48.f}, {384.f, 44.f}}; }

sf::FloatRect detailRect() { return {{kPanel.position.x + 432.f, kPanel.position.y + 78.f}, {384.f, 380.f}}; }

sf::FloatRect buttonRect(int i) { return {{kPanel.position.x + 432.f + i * 131.f, kPanel.position.y + 482.f}, {122.f, 44.f}}; }
}  // namespace

void InventoryPanel::open(InventoryMode mode) {
    mode_ = mode;
    selected_ = 0;
    hoverRow_ = hoverButton_ = -1;
    discardArmed_ = -1;
    feedback_.clear();
    feedbackTimer_ = 0.f;
}

void InventoryPanel::setFeedback(const std::string& message, bool positive) {
    feedback_ = message;
    feedbackPositive_ = positive;
    feedbackTimer_ = message.empty() ? 0.f : 3.2f;
}

void InventoryPanel::clampSelection(const Inventory& inv) {
    if (selected_ >= inv.size()) selected_ = inv.size() - 1;
    if (selected_ < 0) selected_ = 0;
    discardArmed_ = -1;
}

void InventoryPanel::update(float dt) {
    if (feedbackTimer_ > 0.f) feedbackTimer_ -= dt;
}

InventoryAction InventoryPanel::requestDiscard(const Inventory& inv) {
    if (mode_ == InventoryMode::Battle) {
        setFeedback("Items cannot be discarded during battle.", false);
        return InventoryAction::None;
    }
    // Empty pack / invalid slot: let the game logic produce the feedback.
    if (!inv.isValidIndex(selected_)) return InventoryAction::Discard;
    if (discardArmed_ == selected_) {
        discardArmed_ = -1;
        return InventoryAction::Discard;
    }
    discardArmed_ = selected_;
    setFeedback(std::string("Press X again to discard ") + itemInfo(inv.at(selected_).type).name + ".", false);
    return InventoryAction::None;
}

InventoryAction InventoryPanel::handleInput(const Input& in, const Inventory& inv) {
    if (in.cmd != Command::Discard) discardArmed_ = -1;
    switch (in.cmd) {
        case Command::Back:
        case Command::Inventory:
            return InventoryAction::Close;
        case Command::MoveUp:
            if (selected_ > 0) --selected_;
            break;
        case Command::MoveDown:
            if (selected_ + 1 < inv.size()) ++selected_;
            break;
        case Command::Digit:
            if (in.digit >= 1 && in.digit <= inv.size()) selected_ = in.digit - 1;
            else setFeedback("Slot " + std::to_string(in.digit) + " is empty.", false);
            break;
        case Command::Use:
        case Command::Confirm:
        case Command::Step:
            return InventoryAction::Use;
        case Command::Discard:
            return requestDiscard(inv);
        default:
            break;
    }
    return InventoryAction::None;
}

void InventoryPanel::handleHover(sf::Vector2f mouse) {
    hoverRow_ = hoverButton_ = -1;
    for (int i = 0; i < Inventory::kDefaultCapacity; ++i) {
        if (rowRect(i).contains(mouse)) hoverRow_ = i;
    }
    for (int i = 0; i < kButtonCount; ++i) {
        if (buttonRect(i).contains(mouse)) hoverButton_ = i;
    }
}

InventoryAction InventoryPanel::handleClick(sf::Vector2f mouse, const Inventory& inv) {
    if (!kPanel.contains(mouse)) return InventoryAction::Close;
    for (int i = 0; i < inv.size(); ++i) {
        if (rowRect(i).contains(mouse)) {
            selected_ = i;
            discardArmed_ = -1;
            return InventoryAction::None;
        }
    }
    if (buttonRect(0).contains(mouse)) {
        discardArmed_ = -1;
        return InventoryAction::Use;
    }
    if (buttonRect(1).contains(mouse)) return requestDiscard(inv);
    if (buttonRect(2).contains(mouse)) return InventoryAction::Close;
    return InventoryAction::None;
}

void InventoryPanel::draw(sf::RenderTarget& t, const Fonts& f, const Inventory& inv, const Player& player) const {
    const float x = kPanel.position.x, y = kPanel.position.y, w = kPanel.size.x;
    const bool battle = mode_ == InventoryMode::Battle;

    draw::scrim(t, 185);
    draw::roundedRect(t, {{x, y + 8.f}, kPanel.size}, 14.f, sf::Color(0, 0, 0, 120));
    draw::roundedRect(t, kPanel, 14.f, theme::panel, theme::borderHi, 1.f);

    // ---- Header -------------------------------------------------------------------
    draw::text(t, f.title, battle ? "CHOOSE AN ITEM" : "INVENTORY", 22, {x + 24.f, y + 38.f}, theme::gold, Align::Left, 1.5f);
    std::string cap = std::to_string(inv.size()) + " / " + std::to_string(inv.capacity()) + " slots";
    draw::text(t, f.mono, cap, 12, {x + w - 24.f, y + 38.f}, inv.isFull() ? theme::orange : theme::muted, Align::Right);
    if (battle) {
        const std::string note = "USING AN ITEM TAKES YOUR TURN";
        float tw = draw::textWidth(f.bold, note, 10, 1.6f) + 22.f;
        float capW = draw::textWidth(f.mono, cap, 12);
        draw::tag(t, f.bold, note, {x + w - 24.f - capW - 16.f - tw / 2.f, y + 38.f}, theme::orange);
    }
    draw::rect(t, {{x + 24.f, y + 62.f}, {w - 48.f, 1.f}}, theme::border);

    // ---- Slot list ----------------------------------------------------------------
    for (int i = 0; i < inv.capacity(); ++i) {
        sf::FloatRect r = rowRect(i);
        const bool has = i < inv.size();
        const bool sel = has && i == selected_;
        const bool hov = has && i == hoverRow_;
        const float cy = r.position.y + r.size.y / 2.f;

        sf::Color fill = has ? theme::panelHi : sf::Color(15, 18, 27);
        sf::Color border = theme::border;
        if (sel) {
            fill = draw::mix(theme::panelHi, theme::gold, 0.12f);
            border = theme::gold;
        } else if (hov) {
            fill = theme::panelHover;
            border = theme::borderHi;
        }
        draw::roundedRect(t, r, 8.f, fill, border, 1.f);
        if (sel) draw::rect(t, {{r.position.x + 1.f, r.position.y + 10.f}, {3.f, r.size.y - 20.f}}, theme::gold);

        draw::text(t, f.mono, std::to_string(i + 1), 11, {r.position.x + 20.f, cy}, sel ? theme::gold : theme::dim, Align::Center);
        if (!has) {
            draw::text(t, f.regular, "Empty slot", 12, {r.position.x + 42.f, cy}, sf::Color(70, 77, 96));
            continue;
        }
        const ItemType type = inv.at(i).type;
        const ItemInfo& info = itemInfo(type);
        draw::itemIcon(t, type, {r.position.x + 54.f, cy}, 1.15f);
        draw::text(t, f.bold, info.name, 14, {r.position.x + 80.f, r.position.y + 15.f}, sel ? theme::text : sf::Color(205, 209, 220));
        draw::text(t, f.regular, info.effect, 11, {r.position.x + 80.f, r.position.y + 31.f}, theme::muted);
        if (info.battleOnly) draw::tag(t, f.bold, "BATTLE", {r.position.x + r.size.x - 44.f, cy}, theme::orange);
    }

    // ---- Detail pane (inspect) ------------------------------------------------------
    sf::FloatRect d = detailRect();
    draw::roundedRect(t, d, 10.f, sf::Color(15, 18, 27), theme::border, 1.f);
    const float dcx = d.position.x + d.size.x / 2.f;
    const float dx = d.position.x, dy = d.position.y;

    if (inv.isValidIndex(selected_)) {
        const ItemType type = inv.at(selected_).type;
        const ItemInfo& info = itemInfo(type);
        const sf::Vector2f ic{dcx, dy + 76.f};
        draw::glow(t, ic, 74.f, draw::withAlpha(draw::itemColor(type), 70), 5);
        draw::circle(t, ic, 46.f, theme::panelHi, theme::borderHi, 1.f, 48);
        draw::itemIcon(t, type, {ic.x, ic.y + 1.f}, 3.0f);

        draw::text(t, f.title, info.name, 20, {dcx, dy + 152.f}, theme::text, Align::Center);
        draw::tag(t, f.bold, info.battleOnly ? "BATTLE ONLY" : "USE ANYWHERE", {dcx, dy + 180.f}, info.battleOnly ? theme::orange : theme::green);
        draw::wrapped(t, f.regular, info.description, 13, {dx + 32.f, dy + 200.f}, d.size.x - 64.f, theme::muted, 20.f, Align::Center);

        draw::rect(t, {{dx + 28.f, dy + 256.f}, {d.size.x - 56.f, 1.f}}, theme::border);
        draw::sectionLabel(t, f.bold, "EFFECT", {dx + 28.f, dy + 278.f});
        draw::text(t, f.bold, info.effect, 15, {dx + 28.f, dy + 302.f}, draw::itemColor(type));
    } else {
        const sf::Vector2f ic{dcx, dy + 110.f};
        draw::circle(t, ic, 46.f, theme::panelHi, theme::border, 1.f, 48);
        draw::itemIcon(t, ItemType::HealthPotion, ic, 2.6f, 0.22f);
        draw::text(t, f.bold, "Your pack is empty", 16, {dcx, dy + 190.f}, theme::text, Align::Center);
        draw::wrapped(t, f.regular, "Walk over potions and relics in the dungeon to collect them.", 13, {dx + 56.f, dy + 208.f},
                      d.size.x - 112.f, theme::muted, 20.f, Align::Center);
    }

    const float hpFrac = player.stats.maxHp > 0 ? static_cast<float>(player.stats.hp) / player.stats.maxHp : 0.f;
    draw::sectionLabel(t, f.bold, "YOUR HEALTH", {dx + 28.f, dy + 336.f});
    draw::text(t, f.mono, std::to_string(player.stats.hp) + " / " + std::to_string(player.stats.maxHp), 12,
               {dx + d.size.x - 28.f, dy + 336.f}, theme::text, Align::Right);
    draw::bar(t, {{dx + 28.f, dy + 350.f}, {d.size.x - 56.f, 10.f}}, hpFrac, draw::hpColor(hpFrac), sf::Color(8, 10, 16));

    // ---- Footer: feedback or key hints, then buttons ---------------------------------
    const float fy = y + 504.f;
    if (feedbackTimer_ > 0.f && !feedback_.empty()) {
        int alpha = feedbackTimer_ < 0.5f ? static_cast<int>(255 * feedbackTimer_ / 0.5f) : 255;
        sf::Color c = draw::withAlpha(feedbackPositive_ ? theme::green : theme::orange, alpha);
        draw::circle(t, {x + 30.f, fy}, 3.5f, c);
        draw::wrapped(t, f.regular, feedback_, 13, {x + 42.f, fy - (draw::textWidth(f.regular, feedback_, 13) > 360.f ? 19.f : 10.f)},
                      362.f, draw::withAlpha(theme::text, alpha), 19.f);
    } else {
        float cx = x + 24.f;
        auto hint = [&](const std::string& key, const std::string& label) {
            cx += draw::keycap(t, f.bold, key, {cx, fy}, 10) + 6.f;
            draw::text(t, f.regular, label, 12, {cx, fy}, theme::muted);
            cx += draw::textWidth(f.regular, label, 12) + 16.f;
        };
        hint("W", "");
        cx -= 12.f;
        hint("S", "Select");
        hint("1-8", "Jump to slot");
    }

    const bool canAct = inv.isValidIndex(selected_);
    draw::button(t, f.bold, buttonRect(0), "Use", {hoverButton_ == 0, false, canAct}, "U", theme::gold);
    draw::button(t, f.bold, buttonRect(1), discardArmed_ >= 0 ? "Confirm" : "Discard",
                 {hoverButton_ == 1, discardArmed_ >= 0, canAct && !battle}, "X", theme::red);
    draw::button(t, f.bold, buttonRect(2), battle ? "Back" : "Close", {hoverButton_ == 2, false, true}, "Esc", theme::gold);
}
