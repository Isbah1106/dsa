#pragma once

enum class ItemType {
    HealthPotion,
    Elixir,
    FireBomb,
    StrengthTonic,
    IronSkin,
    SmokeBomb,
    Count
};

struct ItemInfo {
    const char* name;
    const char* description;
    const char* effect;
    bool battleOnly;
};

const ItemInfo& itemInfo(ItemType type);

struct Item {
    ItemType type = ItemType::HealthPotion;
    int uid = 0;
};

// An item lying on a dungeon tile, waiting to be picked up.
struct FloorItem {
    Item item;
    int tile = -1;
    bool taken = false;
};
