#include "Draw.hpp"

#include "Theme.hpp"

#include <cmath>

namespace draw {

namespace {
constexpr float kPi = 3.14159265f;

sf::Color fade(sf::Color c, float alpha) {
    c.a = static_cast<std::uint8_t>(c.a * (alpha < 0.f ? 0.f : alpha > 1.f ? 1.f : alpha));
    return c;
}

void ellipse(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, sf::Color fill,
             sf::Color outline = sf::Color::Transparent, float thick = 0.f) {
    sf::CircleShape s(1.f, 36);
    s.setOrigin({1.f, 1.f});
    s.setScale({rx, ry});
    s.setPosition(c);
    s.setFillColor(fill);
    if (thick > 0.f) {
        // Outline thickness is scaled too; compensate using the smaller radius.
        s.setOutlineThickness(thick / (rx < ry ? rx : ry));
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

void polygon(sf::RenderTarget& t, const sf::Vector2f* pts, std::size_t n, sf::Color fill,
             sf::Color outline = sf::Color::Transparent, float thick = 0.f) {
    sf::ConvexShape s(n);
    for (std::size_t i = 0; i < n; ++i) s.setPoint(i, pts[i]);
    s.setFillColor(fill);
    if (thick > 0.f) {
        s.setOutlineThickness(thick);
        s.setOutlineColor(outline);
    }
    t.draw(s);
}
}  // namespace

sf::String utf8(const std::string& s) { return sf::String::fromUtf8(s.begin(), s.end()); }

sf::Color withAlpha(sf::Color c, int alpha) {
    c.a = static_cast<std::uint8_t>(alpha < 0 ? 0 : alpha > 255 ? 255 : alpha);
    return c;
}

sf::Color scale(sf::Color c, float f) {
    auto ch = [f](std::uint8_t v) {
        float r = v * f;
        return static_cast<std::uint8_t>(r < 0.f ? 0.f : r > 255.f ? 255.f : r);
    };
    return {ch(c.r), ch(c.g), ch(c.b), c.a};
}

sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(x + (y - x) * t); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

sf::Color hpColor(float f) {
    if (f > 0.5f) return mix(theme::hpMid, theme::hpHigh, (f - 0.5f) * 2.f);
    return mix(theme::hpLow, theme::hpMid, f * 2.f);
}

void rect(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill) {
    sf::RectangleShape s(r.size);
    s.setPosition(r.position);
    s.setFillColor(fill);
    t.draw(s);
}

void roundedRect(sf::RenderTarget& t, sf::FloatRect r, float radius, sf::Color fill, sf::Color outline, float thickness) {
    float maxR = (r.size.x < r.size.y ? r.size.x : r.size.y) * 0.5f;
    if (radius > maxR) radius = maxR;
    if (radius <= 0.5f) {
        sf::RectangleShape s(r.size);
        s.setPosition(r.position);
        s.setFillColor(fill);
        if (thickness > 0.f) {
            s.setOutlineThickness(-thickness);
            s.setOutlineColor(outline);
        }
        t.draw(s);
        return;
    }
    const int seg = 6;
    sf::ConvexShape s(static_cast<std::size_t>(seg * 4));
    const sf::Vector2f centers[4] = {
        {r.position.x + r.size.x - radius, r.position.y + radius},
        {r.position.x + r.size.x - radius, r.position.y + r.size.y - radius},
        {r.position.x + radius, r.position.y + r.size.y - radius},
        {r.position.x + radius, r.position.y + radius},
    };
    std::size_t idx = 0;
    for (int c = 0; c < 4; ++c) {
        float start = -kPi / 2.f + c * (kPi / 2.f);
        for (int i = 0; i < seg; ++i) {
            float a = start + (kPi / 2.f) * (static_cast<float>(i) / (seg - 1));
            s.setPoint(idx++, {centers[c].x + std::cos(a) * radius, centers[c].y + std::sin(a) * radius});
        }
    }
    s.setFillColor(fill);
    if (thickness > 0.f) {
        s.setOutlineThickness(-thickness);
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

void gradientRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color top, sf::Color bottom) {
    sf::Vertex v[6];
    sf::Vector2f p = r.position, s = r.size;
    v[0] = {{p.x, p.y}, top};
    v[1] = {{p.x + s.x, p.y}, top};
    v[2] = {{p.x + s.x, p.y + s.y}, bottom};
    v[3] = {{p.x, p.y}, top};
    v[4] = {{p.x + s.x, p.y + s.y}, bottom};
    v[5] = {{p.x, p.y + s.y}, bottom};
    t.draw(v, 6, sf::PrimitiveType::Triangles);
}

void hGradientRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color left, sf::Color right) {
    sf::Vertex v[6];
    sf::Vector2f p = r.position, s = r.size;
    v[0] = {{p.x, p.y}, left};
    v[1] = {{p.x + s.x, p.y}, right};
    v[2] = {{p.x + s.x, p.y + s.y}, right};
    v[3] = {{p.x, p.y}, left};
    v[4] = {{p.x + s.x, p.y + s.y}, right};
    v[5] = {{p.x, p.y + s.y}, left};
    t.draw(v, 6, sf::PrimitiveType::Triangles);
}

void line(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b, sf::Color c, float thickness) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.001f) return;
    sf::Vector2f n{-d.y / len * thickness * 0.5f, d.x / len * thickness * 0.5f};
    sf::Vertex v[6];
    v[0] = {a + n, c};
    v[1] = {b + n, c};
    v[2] = {b - n, c};
    v[3] = {a + n, c};
    v[4] = {b - n, c};
    v[5] = {a - n, c};
    t.draw(v, 6, sf::PrimitiveType::Triangles);
}

void circle(sf::RenderTarget& t, sf::Vector2f c, float radius, sf::Color fill, sf::Color outline, float thickness, std::size_t points) {
    sf::CircleShape s(radius, points);
    s.setOrigin({radius, radius});
    s.setPosition(c);
    s.setFillColor(fill);
    if (thickness > 0.f) {
        s.setOutlineThickness(-thickness);
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

void ring(sf::RenderTarget& t, sf::Vector2f c, float radius, float thickness, sf::Color color) {
    circle(t, c, radius, sf::Color::Transparent, color, thickness, 48);
}

void oval(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, sf::Color fill) { ellipse(t, c, rx, ry, fill); }

void glow(sf::RenderTarget& t, sf::Vector2f c, float radius, sf::Color color, int layers) {
    sf::RenderStates states;
    states.blendMode = sf::BlendAdd;
    for (int i = 0; i < layers; ++i) {
        float k = 1.f - static_cast<float>(i) / layers;
        sf::CircleShape s(radius * k, 40);
        s.setOrigin({radius * k, radius * k});
        s.setPosition(c);
        s.setFillColor(withAlpha(color, static_cast<int>(color.a * 0.16f)));
        t.draw(s, states);
    }
}

sf::FloatRect text(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos,
                   sf::Color color, Align align, float spacing) {
    sf::Text tx(f, utf8(s), size);
    tx.setFillColor(color);
    tx.setLetterSpacing(spacing);
    sf::FloatRect b = tx.getLocalBounds();
    float ox = 0.f;
    if (align == Align::Center) ox = b.position.x + b.size.x / 2.f;
    else if (align == Align::Right) ox = b.position.x + b.size.x;
    tx.setOrigin({std::round(ox), std::round(size * 0.62f)});
    tx.setPosition({std::round(pos.x), std::round(pos.y)});
    t.draw(tx);
    return tx.getGlobalBounds();
}

float textWidth(const sf::Font& f, const std::string& s, unsigned size, float spacing) {
    sf::Text tx(f, utf8(s), size);
    tx.setLetterSpacing(spacing);
    sf::FloatRect b = tx.getLocalBounds();
    return b.position.x + b.size.x;
}

float wrapped(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, float maxWidth,
              sf::Color color, float lineHeight, Align align) {
    std::string lineText, word;
    float y = pos.y + lineHeight / 2.f;
    int lines = 0;
    auto flush = [&]() {
        float x = pos.x;
        if (align == Align::Center) x = pos.x + maxWidth / 2.f;
        text(t, f, lineText, size, {x, y}, color, align);
        y += lineHeight;
        ++lines;
        lineText.clear();
    };
    for (std::size_t i = 0; i <= s.size(); ++i) {
        char ch = i < s.size() ? s[i] : ' ';
        if (ch == ' ' || ch == '\n') {
            if (!word.empty()) {
                std::string candidate = lineText.empty() ? word : lineText + " " + word;
                if (!lineText.empty() && textWidth(f, candidate, size) > maxWidth) {
                    flush();
                    lineText = word;
                } else {
                    lineText = candidate;
                }
                word.clear();
            }
            if (ch == '\n' && !lineText.empty()) flush();
        } else {
            word += ch;
        }
    }
    if (!lineText.empty()) flush();
    return lines * lineHeight;
}

void panel(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill) {
    // Soft drop shadow, body, then a 1px inner border and a top highlight.
    roundedRect(t, {{r.position.x, r.position.y + 3.f}, r.size}, 10.f, sf::Color(0, 0, 0, 70));
    roundedRect(t, r, 10.f, fill, theme::border, 1.f);
    line(t, {r.position.x + 10.f, r.position.y + 1.5f}, {r.position.x + r.size.x - 10.f, r.position.y + 1.5f},
         sf::Color(255, 255, 255, 14), 1.f);
}

void sectionLabel(sf::RenderTarget& t, const sf::Font& f, const std::string& s, sf::Vector2f pos, sf::Color color) {
    text(t, f, s, 11, pos, color, Align::Left, 2.2f);
}

void bar(sf::RenderTarget& t, sf::FloatRect r, float fraction, sf::Color fill, sf::Color back, float ghost) {
    fraction = fraction < 0.f ? 0.f : fraction > 1.f ? 1.f : fraction;
    float radius = r.size.y / 2.f;
    roundedRect(t, r, radius, back);
    if (ghost > fraction) {
        if (ghost > 1.f) ghost = 1.f;
        roundedRect(t, {r.position, {r.size.x * ghost, r.size.y}}, radius, sf::Color(255, 255, 255, 60));
    }
    if (fraction > 0.f) {
        float w = r.size.x * fraction;
        if (w < r.size.y) w = r.size.y;
        roundedRect(t, {r.position, {w, r.size.y}}, radius, fill);
        // Gloss highlight on the upper half.
        if (r.size.y >= 6.f)
            roundedRect(t, {{r.position.x + 2.f, r.position.y + 1.f}, {w - 4.f, r.size.y * 0.38f}}, r.size.y * 0.2f,
                        sf::Color(255, 255, 255, 38));
    }
}

float keycap(sf::RenderTarget& t, const sf::Font& f, const std::string& label, sf::Vector2f lc, unsigned size) {
    float w = textWidth(f, label, size) + 14.f;
    if (w < 22.f) w = 22.f;
    float h = size + 10.f;
    sf::FloatRect r{{lc.x, lc.y - h / 2.f}, {w, h}};
    roundedRect(t, {{r.position.x, r.position.y + 2.f}, r.size}, 5.f, sf::Color(0, 0, 0, 90));
    roundedRect(t, r, 5.f, theme::panelHi, theme::borderHi, 1.f);
    text(t, f, label, size, {lc.x + w / 2.f, lc.y}, theme::text, Align::Center);
    return w;
}

void shadow(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, int alpha) {
    ellipse(t, c, rx, ry, sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha)));
}

void button(sf::RenderTarget& t, const sf::Font& font, sf::FloatRect r, const std::string& label, ButtonState st,
            const std::string& key, sf::Color accent, unsigned size) {
    sf::Color fill = theme::panelHi, border = theme::border, txt = theme::text;
    if (!st.enabled) {
        fill = sf::Color(17, 20, 30);
        txt = theme::dim;
    } else if (st.selected) {
        fill = mix(theme::panelHi, accent, 0.16f);
        border = accent;
    } else if (st.hovered) {
        fill = theme::panelHover;
        border = theme::borderHi;
    }
    roundedRect(t, {{r.position.x, r.position.y + 2.f}, r.size}, 8.f, sf::Color(0, 0, 0, 70));
    roundedRect(t, r, 8.f, fill, border, 1.f);

    float cy = r.position.y + r.size.y / 2.f;
    float lw = textWidth(font, label, size);
    float kw = 0.f;
    if (!key.empty()) {
        kw = textWidth(font, key, 10) + 14.f;
        if (kw < 22.f) kw = 22.f;
        kw += 8.f;
    }
    float x = r.position.x + (r.size.x - lw - kw) / 2.f;
    if (!key.empty()) {
        keycap(t, font, key, {x, cy}, 10);
        x += kw;
    }
    text(t, font, label, size, {x, cy}, txt);
}

float tag(sf::RenderTarget& t, const sf::Font& font, const std::string& label, sf::Vector2f center, sf::Color color) {
    float w = textWidth(font, label, 10, 1.6f) + 22.f;
    sf::FloatRect r{{center.x - w / 2.f, center.y - 10.f}, {w, 20.f}};
    roundedRect(t, r, 10.f, withAlpha(color, 34), withAlpha(color, 150), 1.f);
    text(t, font, label, 10, {center.x, center.y}, color, Align::Center, 1.6f);
    return w;
}

void scrim(sf::RenderTarget& t, int alpha) {
    sf::Vector2f size = t.getView().getSize();
    rect(t, {{0.f, 0.f}, size}, sf::Color(6, 8, 13, static_cast<std::uint8_t>(alpha < 0 ? 0 : alpha > 255 ? 255 : alpha)));
}

sf::Color itemColor(ItemType type) {
    switch (type) {
        case ItemType::HealthPotion: return {226, 72, 86};
        case ItemType::Elixir: return {244, 204, 96};
        case ItemType::FireBomb: return {244, 128, 56};
        case ItemType::StrengthTonic: return {236, 120, 190};
        case ItemType::IronSkin: return {128, 168, 214};
        case ItemType::SmokeBomb: return {168, 174, 190};
        default: return theme::text;
    }
}

sf::Color enemyColor(EnemyKind kind) {
    switch (kind) {
        case EnemyKind::Slime: return {108, 206, 120};
        case EnemyKind::Goblin: return {196, 176, 74};
        case EnemyKind::Skeleton: return {222, 216, 200};
        case EnemyKind::Orc: return {112, 150, 88};
        case EnemyKind::Warden: return {170, 116, 255};
        default: return theme::text;
    }
}

void itemIcon(sf::RenderTarget& t, ItemType type, sf::Vector2f c, float s, float alpha) {
    const sf::Color outline = fade(sf::Color(8, 10, 16, 220), alpha);
    const sf::Color col = fade(itemColor(type), alpha);
    const float th = 1.2f * (s > 1.5f ? s * 0.7f : 1.f);

    switch (type) {
        case ItemType::HealthPotion:
        case ItemType::Elixir:
        case ItemType::StrengthTonic: {
            // Flask: neck, cork, round body, highlight.
            rect(t, {{c.x - 2.2f * s, c.y - 7.5f * s}, {4.4f * s, 4.5f * s}}, fade(sf::Color(190, 200, 220), alpha));
            rect(t, {{c.x - 2.8f * s, c.y - 9.f * s}, {5.6f * s, 2.4f * s}}, fade(sf::Color(150, 100, 60), alpha));
            if (type == ItemType::StrengthTonic) {
                sf::Vector2f tri[3] = {{c.x, c.y - 4.f * s}, {c.x + 6.5f * s, c.y + 6.5f * s}, {c.x - 6.5f * s, c.y + 6.5f * s}};
                polygon(t, tri, 3, col, outline, th);
            } else {
                circle(t, {c.x, c.y + 2.f * s}, 6.2f * s, col, outline, th);
                circle(t, {c.x, c.y + 3.2f * s}, 4.2f * s, fade(scale(itemColor(type), 0.78f), alpha));
            }
            circle(t, {c.x - 2.2f * s, c.y + 0.5f * s}, 1.5f * s, fade(sf::Color(255, 255, 255, 170), alpha));
            if (type == ItemType::Elixir) glow(t, {c.x, c.y + 2.f * s}, 11.f * s, fade(sf::Color(244, 204, 96, 140), alpha), 4);
            break;
        }
        case ItemType::FireBomb: {
            circle(t, {c.x, c.y + 1.5f * s}, 6.5f * s, fade(sf::Color(58, 60, 72), alpha), outline, th);
            circle(t, {c.x - 2.f * s, c.y - 0.5f * s}, 1.8f * s, fade(sf::Color(255, 255, 255, 60), alpha));
            line(t, {c.x + 3.f * s, c.y - 4.f * s}, {c.x + 5.5f * s, c.y - 7.5f * s}, fade(sf::Color(200, 170, 120), alpha), 1.4f * s);
            circle(t, {c.x + 6.f * s, c.y - 8.f * s}, 1.8f * s, col);
            glow(t, {c.x + 6.f * s, c.y - 8.f * s}, 6.f * s, fade(sf::Color(255, 150, 60, 200), alpha), 4);
            break;
        }
        case ItemType::IronSkin: {
            sf::Vector2f sh[5] = {{c.x - 6.5f * s, c.y - 7.f * s}, {c.x + 6.5f * s, c.y - 7.f * s}, {c.x + 6.f * s, c.y + 1.f * s},
                                  {c.x, c.y + 8.f * s}, {c.x - 6.f * s, c.y + 1.f * s}};
            polygon(t, sh, 5, col, outline, th);
            sf::Vector2f in[4] = {{c.x - 0.f, c.y - 5.f * s}, {c.x + 4.f * s, c.y - 5.f * s}, {c.x + 3.6f * s, c.y + 1.f * s}, {c.x, c.y + 5.f * s}};
            polygon(t, in, 4, fade(sf::Color(255, 255, 255, 70), alpha));
            break;
        }
        case ItemType::SmokeBomb: {
            circle(t, {c.x - 3.5f * s, c.y + 2.f * s}, 4.5f * s, fade(scale(itemColor(type), 0.8f), alpha), outline, th);
            circle(t, {c.x + 3.5f * s, c.y + 2.f * s}, 4.5f * s, fade(scale(itemColor(type), 0.9f), alpha), outline, th);
            circle(t, {c.x, c.y - 2.5f * s}, 5.f * s, col, outline, th);
            break;
        }
        default:
            break;
    }
}

void enemyIcon(sf::RenderTarget& t, EnemyKind kind, sf::Vector2f c, float s, float time, float alpha) {
    const sf::Color outline = fade(sf::Color(8, 10, 16, 230), alpha);
    const sf::Color col = fade(enemyColor(kind), alpha);
    const sf::Color dark = fade(sf::Color(14, 14, 20), alpha);
    const float th = 1.3f * (s > 1.5f ? s * 0.6f : 1.f);

    switch (kind) {
        case EnemyKind::Slime: {
            float squish = 1.f + 0.06f * std::sin(time * 4.f);
            ellipse(t, {c.x, c.y + 2.f * s}, 8.f * s * squish, 6.5f * s / squish, col, outline, th);
            ellipse(t, {c.x - 3.f * s, c.y - 1.f * s}, 2.2f * s, 1.4f * s, fade(sf::Color(255, 255, 255, 110), alpha));
            circle(t, {c.x - 2.6f * s, c.y + 1.5f * s}, 1.4f * s, dark);
            circle(t, {c.x + 2.6f * s, c.y + 1.5f * s}, 1.4f * s, dark);
            break;
        }
        case EnemyKind::Goblin: {
            sf::Vector2f earL[3] = {{c.x - 5.f * s, c.y - 2.f * s}, {c.x - 11.f * s, c.y - 5.f * s}, {c.x - 4.f * s, c.y + 2.f * s}};
            sf::Vector2f earR[3] = {{c.x + 5.f * s, c.y - 2.f * s}, {c.x + 11.f * s, c.y - 5.f * s}, {c.x + 4.f * s, c.y + 2.f * s}};
            polygon(t, earL, 3, col, outline, th);
            polygon(t, earR, 3, col, outline, th);
            circle(t, c, 6.8f * s, col, outline, th);
            circle(t, {c.x - 2.5f * s, c.y - 0.5f * s}, 1.4f * s, fade(sf::Color(255, 70, 50), alpha));
            circle(t, {c.x + 2.5f * s, c.y - 0.5f * s}, 1.4f * s, fade(sf::Color(255, 70, 50), alpha));
            line(t, {c.x - 2.5f * s, c.y + 3.2f * s}, {c.x + 2.5f * s, c.y + 3.2f * s}, dark, 1.f * s);
            break;
        }
        case EnemyKind::Skeleton: {
            roundedRect(t, {{c.x - 4.f * s, c.y + 2.f * s}, {8.f * s, 5.5f * s}}, 1.5f * s, col, outline, th);
            circle(t, {c.x, c.y - 1.5f * s}, 7.f * s, col, outline, th);
            circle(t, {c.x - 2.7f * s, c.y - 1.f * s}, 2.f * s, dark);
            circle(t, {c.x + 2.7f * s, c.y - 1.f * s}, 2.f * s, dark);
            for (int i = -1; i <= 1; ++i)
                line(t, {c.x + i * 2.f * s, c.y + 4.f * s}, {c.x + i * 2.f * s, c.y + 7.f * s}, dark, 0.8f * s);
            break;
        }
        case EnemyKind::Orc: {
            roundedRect(t, {{c.x - 8.f * s, c.y - 7.f * s}, {16.f * s, 15.f * s}}, 4.f * s, col, outline, th);
            rect(t, {{c.x - 8.f * s + th, c.y - 7.f * s + th}, {16.f * s - 2 * th, 4.f * s}}, fade(scale(enemyColor(kind), 0.7f), alpha));
            circle(t, {c.x - 3.f * s, c.y - 1.f * s}, 1.5f * s, fade(sf::Color(255, 60, 40), alpha));
            circle(t, {c.x + 3.f * s, c.y - 1.f * s}, 1.5f * s, fade(sf::Color(255, 60, 40), alpha));
            sf::Vector2f tL[3] = {{c.x - 4.5f * s, c.y + 3.f * s}, {c.x - 2.f * s, c.y + 3.f * s}, {c.x - 3.6f * s, c.y - 0.5f * s}};
            sf::Vector2f tR[3] = {{c.x + 2.f * s, c.y + 3.f * s}, {c.x + 4.5f * s, c.y + 3.f * s}, {c.x + 3.6f * s, c.y - 0.5f * s}};
            polygon(t, tL, 3, fade(sf::Color(245, 240, 225), alpha));
            polygon(t, tR, 3, fade(sf::Color(245, 240, 225), alpha));
            break;
        }
        case EnemyKind::Warden: {
            glow(t, c, 15.f * s, fade(sf::Color(170, 100, 255, static_cast<std::uint8_t>(160 + 40 * std::sin(time * 3.f))), alpha), 5);
            sf::Vector2f body[4] = {{c.x, c.y - 9.f * s}, {c.x + 8.5f * s, c.y}, {c.x, c.y + 9.f * s}, {c.x - 8.5f * s, c.y}};
            polygon(t, body, 4, col, outline, th);
            sf::Vector2f crown[7] = {{c.x - 5.f * s, c.y - 9.f * s}, {c.x - 5.5f * s, c.y - 14.f * s}, {c.x - 2.5f * s, c.y - 11.5f * s},
                                     {c.x, c.y - 15.f * s},        {c.x + 2.5f * s, c.y - 11.5f * s}, {c.x + 5.5f * s, c.y - 14.f * s},
                                     {c.x + 5.f * s, c.y - 9.f * s}};
            // Crown is concave: draw as a fan of triangles from its base centre.
            for (int i = 0; i < 6; ++i) {
                sf::Vector2f tri[3] = {{c.x, c.y - 8.5f * s}, crown[i], crown[i + 1]};
                polygon(t, tri, 3, fade(theme::gold, alpha));
            }
            circle(t, {c.x, c.y}, 2.6f * s, fade(sf::Color(255, 240, 255), alpha));
            circle(t, {c.x, c.y}, 1.2f * s, fade(sf::Color(120, 40, 200), alpha));
            break;
        }
        default:
            break;
    }
}

void playerIcon(sf::RenderTarget& t, sf::Vector2f c, float s, int dir, float alpha) {
    const sf::Color outline = fade(sf::Color(60, 40, 12, 240), alpha);
    circle(t, c, 8.f * s, fade(theme::gold, alpha), outline, 1.6f * (s > 1.5f ? s * 0.6f : 1.f));
    circle(t, c, 5.f * s, fade(sf::Color(255, 232, 170), alpha));
    // Facing marker.
    const float ang[4] = {-kPi / 2.f, 0.f, kPi / 2.f, kPi};
    float a = ang[dir < 0 || dir > 3 ? 2 : dir];
    sf::Vector2f dirV{std::cos(a), std::sin(a)};
    sf::Vector2f perp{-dirV.y, dirV.x};
    sf::Vector2f tip = c + dirV * (11.5f * s);
    sf::Vector2f b1 = c + dirV * (7.f * s) + perp * (3.6f * s);
    sf::Vector2f b2 = c + dirV * (7.f * s) - perp * (3.6f * s);
    sf::Vector2f tri[3] = {tip, b1, b2};
    polygon(t, tri, 3, fade(sf::Color(255, 240, 200), alpha), outline, 1.f);
    circle(t, c - dirV * (0.5f * s), 2.f * s, fade(sf::Color(70, 50, 20), alpha));
}

void portalIcon(sf::RenderTarget& t, sf::Vector2f c, float s, float time, float alpha) {
    float pulse = 0.5f + 0.5f * std::sin(time * 2.6f);
    glow(t, c, 16.f * s, fade(sf::Color(150, 110, 255, static_cast<std::uint8_t>(150 + 80 * pulse)), alpha), 6);
    circle(t, c, 8.5f * s, fade(sf::Color(40, 22, 80, 230), alpha));
    circle(t, c, (4.f + 1.5f * pulse) * s, fade(sf::Color(200, 180, 255, 220), alpha));
    ring(t, c, 9.f * s, 1.8f * s, fade(theme::violet, alpha));
    for (int i = 0; i < 6; ++i) {
        float a = time * 1.6f + i * (kPi / 3.f);
        circle(t, {c.x + std::cos(a) * 11.f * s, c.y + std::sin(a) * 11.f * s}, 1.3f * s,
               fade(sf::Color(220, 205, 255), alpha));
    }
}

}  // namespace draw
