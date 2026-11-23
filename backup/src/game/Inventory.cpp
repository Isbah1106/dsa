#include "Inventory.hpp"

bool Inventory::add(const Item& item) {
    if (isFull()) return false;
    items_.push_back(item);
    return true;
}

bool Inventory::remove(int index) {
    if (!isValidIndex(index)) return false;
    items_.removeAt(index);
    return true;
}

int Inventory::countOf(ItemType type) const {
    int count = 0;
    for (const Item& item : items_) {
        if (item.type == type) ++count;
    }
    return count;
}

namespace {
int heal(Stats& s, int amount) {
    int before = s.hp;
    s.hp += amount;
    if (s.hp > s.maxHp) s.hp = s.maxHp;
    return s.hp - before;
}
}  // namespace

ItemUseResult applyItemEffect(ItemType type, Player& player, Stats* enemy, bool enemyIsBoss) {
    ItemUseResult r;
    const ItemInfo& info = itemInfo(type);
    const bool inBattle = enemy != nullptr;

    if (info.battleOnly && !inBattle) {
        r.message = std::string(info.name) + " can only be used in battle.";
        return r;
    }

    switch (type) {
        case ItemType::HealthPotion:
        case ItemType::Elixir: {
            if (player.stats.hp >= player.stats.maxHp) {
                r.message = "You are already at full health.";
                return r;
            }
            int amount = (type == ItemType::Elixir) ? player.stats.maxHp : 35;
            r.healed = heal(player.stats, amount);
            r.message = "You drink the " + std::string(info.name) + " and recover " + std::to_string(r.healed) + " HP.";
            break;
        }
        case ItemType::FireBomb: {
            r.damage = 30;
            enemy->hp -= r.damage;
            if (enemy->hp < 0) enemy->hp = 0;
            r.message = "The Fire Bomb bursts for " + std::to_string(r.damage) + " damage!";
            break;
        }
        case ItemType::StrengthTonic:
            player.stats.attack += 3;
            r.message = "Strength surges through you. ATK +3.";
            break;
        case ItemType::IronSkin:
            player.stats.defense += 2;
            r.message = "Your skin hardens. DEF +2.";
            break;
        case ItemType::SmokeBomb:
            if (enemyIsBoss) {
                r.message = "The Warden sees through the smoke. You cannot flee a boss.";
                return r;
            }
            r.escape = true;
            r.message = "Smoke fills the chamber and you slip away.";
            break;
        case ItemType::Count:
            r.message = "Unknown item.";
            return r;
    }
    r.success = true;
    r.consumed = true;
    return r;
}
