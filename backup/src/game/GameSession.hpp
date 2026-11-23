#pragma once
// GameSession — all game rules for one run, with no rendering dependency.
// The SFML layer reads this state and calls its methods in response to input.

#include "../core/DynamicArray.hpp"
#include "BattleSystem.hpp"
#include "Dungeon.hpp"
#include "DungeonGenerator.hpp"
#include "Entity.hpp"
#include "Inventory.hpp"
#include "Item.hpp"
#include "Pathfinder.hpp"
#include "Rng.hpp"

#include <cstdint>
#include <string>

enum class SessionPhase { Exploring, Battle, Victory, Defeat };

enum class MoveResult {
    Moved,
    Blocked,        // wall
    OutOfBounds,
    PickedUp,       // moved and collected an item
    InventoryFull,  // moved onto an item but the pack is full
    BattleStarted,  // bumped into an enemy
    ReachedExit,
    NotAllowed      // not in exploration
};

struct RunStats {
    int steps = 0;
    int kills = 0;
    int battlesFled = 0;
    int itemsCollected = 0;
    int itemsUsed = 0;
    int damageDealt = 0;
    int damageTaken = 0;
    int pathsPlanned = 0;
};

class GameSession {
public:
    static constexpr int kSightRadius = 6;

    explicit GameSession(GeneratorConfig config = GeneratorConfig());

    void newRun(std::uint32_t seed);
    bool hasRun() const { return hasRun_; }
    std::uint32_t seed() const { return seed_; }
    SessionPhase phase() const { return phase_; }

    // --- Exploration --------------------------------------------------
    MoveResult move(int dx, int dy);

    // Plans an A* route to `tile` through explored ground. Returns false and
    // sets `feedback` when no route exists (FR-A2).
    bool planPath(int tile, std::string* feedback = nullptr);
    bool planPathToExit(std::string* feedback = nullptr);
    bool hasPlannedPath() const { return plannedPath_.found && plannedPath_.path.size() > 1; }
    const PathResult& lastSearch() const { return plannedPath_; }
    int pathCursor() const { return pathCursor_; }
    int pathTarget() const { return hasPlannedPath() ? plannedPath_.path.back() : -1; }
    MoveResult followPathStep();  // one step along the planned route
    void clearPath();

    // --- Inventory ----------------------------------------------------
    ItemUseResult useItem(int index);
    bool discardItem(int index, std::string* feedback = nullptr);

    // --- Battle -------------------------------------------------------
    BattleSystem& battle() { return battle_; }
    const BattleSystem& battle() const { return battle_; }
    int battleEnemyIndex() const { return battleEnemy_; }
    // Applies the outcome of a finished battle (XP, removal, defeat...).
    // Returns XP gained (0 if not a win); `levelsGained` is optional.
    int finishBattle(int* levelsGained = nullptr);

    // --- Queries ------------------------------------------------------
    const Dungeon& dungeon() const { return level_.dungeon; }
    const Player& player() const { return player_; }
    const Inventory& inventory() const { return inventory_; }
    const DynamicArray<Enemy>& enemies() const { return level_.enemies; }
    const DynamicArray<FloorItem>& floorItems() const { return level_.items; }
    int enemyAt(int tile) const;  // index into enemies(), alive only
    int itemAt(int tile) const;   // index into floorItems(), not taken only
    bool isExplored(int tile) const { return explored_[tile]; }
    bool isVisible(int tile) const { return visible_[tile]; }
    bool exitDiscovered() const { return explored_[level_.dungeon.exit]; }
    int enemiesRemaining() const;
    int exploredPercent() const;
    int generationAttempts() const { return level_.attempts; }
    int lastDirection() const { return lastDir_; }
    const RunStats& stats() const { return stats_; }
    const DynamicArray<LogLine>& messages() const { return messages_; }
    void log(const std::string& text, LogKind kind = LogKind::Info);

    // Marks every tile as explored (demo / grading aid, bound to F2).
    void revealAll();

    // Testing hooks.
    Player& mutablePlayer() { return player_; }
    Inventory& mutableInventory() { return inventory_; }
    DynamicArray<Enemy>& mutableEnemies() { return level_.enemies; }

private:
    MoveResult stepOnto(int tile);
    void updateVisibility();
    bool lineOfSight(int x0, int y0, int x1, int y1) const;
    void startBattle(int enemyIndex);

    DungeonGenerator generator_;
    GeneratedLevel level_;
    Player player_;
    Inventory inventory_;
    BattleSystem battle_;
    Rng rng_;

    SessionPhase phase_ = SessionPhase::Exploring;
    std::uint32_t seed_ = 0;
    bool hasRun_ = false;
    int battleEnemy_ = -1;
    int lastDir_ = 2;  // 0 up, 1 right, 2 down, 3 left

    DynamicArray<bool> explored_;
    DynamicArray<bool> visible_;
    PathResult plannedPath_;
    int pathCursor_ = 0;

    RunStats stats_;
    DynamicArray<LogLine> messages_;
};
