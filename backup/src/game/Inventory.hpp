#pragma once

#include "../core/DynamicArray.hpp"
#include "Entity.hpp"
#include "Item.hpp"

#include <string>

class Inventory {
public:
    static constexpr int kDefaultCapacity = 8;

    explicit Inventory(int capacity = kDefaultCapacity) : capacity_(capacity < 1 ? 1 : capacity) {}

    bool add(const Item& item);      // false when full
    bool remove(int index);          // false on invalid index
    bool isValidIndex(int index) const { return index >= 0 && index < items_.size(); }
    const Item& at(int index) const { return items_[index]; }

    int size() const { return items_.size(); }
    int capacity() const { return capacity_; }
    bool isFull() const { return items_.size() >= capacity_; }
    bool isEmpty() const { return items_.empty(); }
    int countOf(ItemType type) const;
    void clear() { items_.clear(); }

private:
    DynamicArray<Item> items_;
    int capacity_;
};

struct ItemUseResult {
    bool success = false;   // effect applied
    bool consumed = false;  // remove from inventory
    bool escape = false;    // battle should end as an escape
    int healed = 0;
    int damage = 0;
    std::string message;
};

// Applies an item's effect. `enemy` is nullptr outside of battle.
ItemUseResult applyItemEffect(ItemType type, Player& player, Stats* enemy, bool enemyIsBoss);
