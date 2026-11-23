#include "Item.hpp"

namespace {
const ItemInfo kItems[] = {
    {"Health Potion", "A ruby draught brewed by the realm's healers.", "Restores 35 HP", false},
    {"Elixir of Vigor", "Liquid sunlight, sealed in a gilded vial.", "Restores all HP", false},
    {"Fire Bomb", "A clay sphere packed with volatile embers.", "Deals 30 damage, ignores armour", true},
    {"Strength Tonic", "Bitter, but your arms feel like iron.", "Permanently +3 ATK", false},
    {"Ironskin Draught", "Your skin hardens like old bark.", "Permanently +2 DEF", false},
    {"Smoke Bomb", "A burst of grey smoke covers your retreat.", "Guaranteed escape (not vs. bosses)", true},
};
}

const ItemInfo& itemInfo(ItemType type) {
    int i = static_cast<int>(type);
    if (i < 0 || i >= static_cast<int>(ItemType::Count)) i = 0;
    return kItems[i];
}
