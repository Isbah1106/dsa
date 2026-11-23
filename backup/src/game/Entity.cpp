#include "Entity.hpp"

namespace {
//                name        title                      maxHp hp atk def spd  xp  special           chance mult
const EnemyInfo kEnemies[] = {
    {"Slime",     "Gelatinous Lurker",   {30, 30, 7, 1, 7},   10, "Acid Splash",   0.15, 1.3},
    {"Goblin",    "Tunnel Cutthroat",    {34, 34, 9, 2, 13},  15, "Quick Stab",    0.20, 1.25},
    {"Skeleton",  "Restless Sentinel",   {42, 42, 11, 3, 9},  22, "Marrow Strike", 0.18, 1.4},
    {"Orc",       "Brute of the Deep",   {58, 58, 13, 4, 8},  32, "Savage Swing",  0.22, 1.55},
    {"Warden",    "Keeper of the Portal",{95, 95, 14, 5, 9},  80, "Crushing Slam", 0.25, 1.7},
};
}

const EnemyInfo& enemyInfo(EnemyKind kind) {
    int i = static_cast<int>(kind);
    if (i < 0 || i >= static_cast<int>(EnemyKind::Count)) i = 0;
    return kEnemies[i];
}

Enemy makeEnemy(EnemyKind kind, int tile, int uid) {
    Enemy e;
    e.kind = kind;
    e.tile = tile;
    e.stats = enemyInfo(kind).stats;
    e.boss = (kind == EnemyKind::Warden);
    e.uid = uid;
    return e;
}

int Player::gainXp(int amount) {
    if (amount <= 0) return 0;
    xp += amount;
    int levels = 0;
    while (xp >= xpToNext()) {
        xp -= xpToNext();
        ++level;
        ++levels;
        stats.maxHp += 12;
        stats.attack += 2;
        stats.defense += 1;
        stats.hp += 25;
        if (stats.hp > stats.maxHp) stats.hp = stats.maxHp;
    }
    return levels;
}

Player makePlayer(int tile) {
    Player p;
    p.tile = tile;
    p.stats = {100, 100, 12, 4, 10};
    return p;
}
