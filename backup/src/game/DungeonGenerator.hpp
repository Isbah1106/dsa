#pragma once
// DungeonGenerator — procedural level creation (FR-P1 .. FR-P5).
//
//  1. Recursive backtracking carves a perfect maze through every odd cell.
//  2. Non-overlapping, in-bounds rooms are carved on top of the maze.
//  3. A few walls are knocked out to create loops (multiple routes).
//  4. Short dead ends are pruned.
//  5. The adjacency-list graph is built and connectivity is validated;
//     the exit is placed on the tile farthest from the start (BFS distance).
//  6. Enemies and items are placed on reachable, non-overlapping tiles.

#include "../core/DynamicArray.hpp"
#include "Dungeon.hpp"
#include "Entity.hpp"
#include "Item.hpp"
#include "Rng.hpp"

#include <cstdint>
#include <string>

struct GeneratorConfig {
    int width = 37;   // forced odd, >= 11
    int height = 23;  // forced odd, >= 11
    int roomAttempts = 60;
    int maxRooms = 7;
    int loopCount = 14;
    int prunePasses = 4;
    int enemyCount = 8;  // in addition to the Warden
    int itemCount = 9;
};

struct GeneratedLevel {
    Dungeon dungeon;
    DynamicArray<Enemy> enemies;
    DynamicArray<FloorItem> items;
    int attempts = 0;  // > 1 means a layout was rejected and regenerated
};

class DungeonGenerator {
public:
    explicit DungeonGenerator(GeneratorConfig config = GeneratorConfig());

    GeneratedLevel generate(std::uint32_t seed) const;

    // Full structural validation; used after generation and by the tests.
    static bool validate(const GeneratedLevel& level, std::string* reason = nullptr);

    const GeneratorConfig& config() const { return config_; }

private:
    bool tryGenerate(std::uint32_t seed, GeneratedLevel& out) const;
    void carveMaze(Dungeon& d, DynamicArray<bool>& visited, int x, int y, Rng& rng) const;
    void placeRooms(Dungeon& d, Rng& rng) const;
    void addLoops(Dungeon& d, Rng& rng) const;
    void pruneDeadEnds(Dungeon& d) const;
    void placeEntities(GeneratedLevel& level, const DynamicArray<int>& dist, Rng& rng) const;

    GeneratorConfig config_;
};
