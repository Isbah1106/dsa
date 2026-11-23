#pragma once
// Visual design tokens: palette and layout metrics shared by every screen.

#include <SFML/Graphics/Color.hpp>

namespace theme {

// ---- Window & layout ------------------------------------------------------
constexpr unsigned kWindowW = 1340;
constexpr unsigned kWindowH = 780;

constexpr float kTile = 26.f;
constexpr float kMapX = 16.f;
constexpr float kMapY = 72.f;

constexpr float kTopBarH = 56.f;
constexpr float kSideX = 996.f;
constexpr float kSideW = 334.f;

// ---- Palette ----------------------------------------------------------------
const sf::Color bg{10, 12, 18};
const sf::Color bgTop{15, 18, 27};
const sf::Color void_{13, 15, 22};
const sf::Color panel{19, 23, 34};
const sf::Color panelHi{26, 31, 45};
const sf::Color panelHover{32, 38, 55};
const sf::Color border{40, 47, 66};
const sf::Color borderHi{66, 76, 100};

const sf::Color text{232, 234, 240};
const sf::Color muted{150, 158, 178};
const sf::Color dim{94, 102, 122};

const sf::Color gold{226, 182, 92};
const sf::Color goldDeep{150, 116, 52};
const sf::Color teal{79, 200, 196};
const sf::Color violet{160, 126, 255};
const sf::Color red{229, 86, 78};
const sf::Color green{90, 204, 138};
const sf::Color blue{108, 166, 240};
const sf::Color orange{240, 152, 72};

const sf::Color hpHigh{92, 206, 132};
const sf::Color hpMid{232, 186, 80};
const sf::Color hpLow{228, 84, 76};
const sf::Color xp{122, 150, 255};

const sf::Color floorCorridor{33, 39, 54};
const sf::Color floorRoom{40, 46, 63};
const sf::Color wallTop{70, 77, 102};
const sf::Color wallFace{42, 47, 66};
const sf::Color grout{16, 18, 27};

}  // namespace theme
