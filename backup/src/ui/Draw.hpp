#pragma once
// Drawing primitives shared by every screen: shapes, text, bars, icons.
// Placeholder vector art is used instead of sprites (FR-U5).

#include "../game/Entity.hpp"
#include "../game/Item.hpp"

#include <SFML/Graphics.hpp>

#include <string>

namespace draw {

enum class Align { Left, Center, Right };

sf::String utf8(const std::string& s);
sf::Color withAlpha(sf::Color c, int alpha);
sf::Color scale(sf::Color c, float f);
sf::Color mix(sf::Color a, sf::Color b, float t);
sf::Color hpColor(float fraction);

void rect(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill);
void roundedRect(sf::RenderTarget& t, sf::FloatRect r, float radius, sf::Color fill,
                 sf::Color outline = sf::Color::Transparent, float thickness = 0.f);
void gradientRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color top, sf::Color bottom);
void hGradientRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color left, sf::Color right);
void line(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b, sf::Color c, float thickness = 1.f);
void circle(sf::RenderTarget& t, sf::Vector2f c, float radius, sf::Color fill,
            sf::Color outline = sf::Color::Transparent, float thickness = 0.f, std::size_t points = 32);
void ring(sf::RenderTarget& t, sf::Vector2f c, float radius, float thickness, sf::Color color);
void oval(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, sf::Color fill);
void glow(sf::RenderTarget& t, sf::Vector2f c, float radius, sf::Color color, int layers = 6);

// Text: `pos.y` is the vertical centre of the line.
sf::FloatRect text(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size,
                   sf::Vector2f pos, sf::Color color, Align align = Align::Left, float spacing = 1.f);
float textWidth(const sf::Font& f, const std::string& s, unsigned size, float spacing = 1.f);
// Word-wrapped text starting at `pos` (top of first line). Returns total height.
float wrapped(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos,
              float maxWidth, sf::Color color, float lineHeight, Align align = Align::Left);

// Composite widgets.
void panel(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill = sf::Color(19, 23, 34));
void sectionLabel(sf::RenderTarget& t, const sf::Font& f, const std::string& s, sf::Vector2f pos,
                  sf::Color color = sf::Color(150, 158, 178));
void bar(sf::RenderTarget& t, sf::FloatRect r, float fraction, sf::Color fill, sf::Color back,
         float ghostFraction = -1.f);
// Small keyboard key chip. Returns width.
float keycap(sf::RenderTarget& t, const sf::Font& f, const std::string& label, sf::Vector2f leftCenter,
             unsigned size = 12);
void shadow(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, int alpha = 90);

struct ButtonState {
    bool hovered = false;
    bool selected = false;
    bool enabled = true;
};
// Centered label with an optional keycap on its left.
void button(sf::RenderTarget& t, const sf::Font& font, sf::FloatRect r, const std::string& label, ButtonState state,
            const std::string& key = "", sf::Color accent = sf::Color(226, 182, 92), unsigned size = 13);
// Small pill-shaped tag. `center` is the middle of the pill. Returns its width.
float tag(sf::RenderTarget& t, const sf::Font& font, const std::string& label, sf::Vector2f center, sf::Color color);
// Full-target translucent scrim used behind modal panels.
void scrim(sf::RenderTarget& t, int alpha);

// Icons (scale 1 is roughly a 18 px glyph).
void itemIcon(sf::RenderTarget& t, ItemType type, sf::Vector2f c, float s, float alpha = 1.f);
void enemyIcon(sf::RenderTarget& t, EnemyKind kind, sf::Vector2f c, float s, float time, float alpha = 1.f);
void playerIcon(sf::RenderTarget& t, sf::Vector2f c, float s, int dir, float alpha = 1.f);
void portalIcon(sf::RenderTarget& t, sf::Vector2f c, float s, float time, float alpha = 1.f);
sf::Color itemColor(ItemType type);
sf::Color enemyColor(EnemyKind kind);

}  // namespace draw
