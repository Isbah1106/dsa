#pragma once

#include <string>

struct Stats {
    int maxHp = 1;
    int hp = 1;
    int attack = 1;
    int defense = 0;
    int speed = 1;  // initiative: higher speed = acts more often (heap priority)
};

enum class EnemyKind { Slime, Goblin, Skeleton, Orc, Warden, Count };

struct EnemyInfo {
    const char* name;
    const char* title;
    Stats stats;
    int xp;
    const char* specialName;
    double specialChance;
    double specialMultiplier;
};

const EnemyInfo& enemyInfo(EnemyKind kind);

struct Enemy {
    EnemyKind kind = EnemyKind::Slime;
    int tile = -1;
    Stats stats;
    bool alive = true;
    bool boss = false;
    int uid = 0;

    const char* name() const { return enemyInfo(kind).name; }
};

Enemy makeEnemy(EnemyKind kind, int tile, int uid);

struct Player {
    Stats stats;
    int tile = -1;
    int level = 1;
    int xp = 0;

    int xpToNext() const { return 25 + level * 20; }

    // Adds experience; returns the number of levels gained.
    int gainXp(int amount);
};

Player makePlayer(int tile);
