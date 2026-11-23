#include "DungeonGenerator.hpp"

namespace {
const int kDx[4] = {0, 1, 0, -1};
const int kDy[4] = {-1, 0, 1, 0};

template <typename T>
void shuffle(DynamicArray<T>& arr, Rng& rng) {
    for (int i = arr.size() - 1; i > 0; --i) {
        int j = rng.range(0, i);
        T tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

int manhattan(const Dungeon& d, int a, int b) {
    int dx = d.xOf(a) - d.xOf(b);
    int dy = d.yOf(a) - d.yOf(b);
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

int makeOdd(int v) { return (v % 2 == 0) ? v + 1 : v; }
}  // namespace

DungeonGenerator::DungeonGenerator(GeneratorConfig config) : config_(config) {
    if (config_.width < 11) config_.width = 11;
    if (config_.height < 11) config_.height = 11;
    config_.width = makeOdd(config_.width);
    config_.height = makeOdd(config_.height);
}

GeneratedLevel DungeonGenerator::generate(std::uint32_t seed) const {
    GeneratedLevel level;
    // Validate-and-regenerate loop (risk mitigation from the PRD, section 13).
    for (int attempt = 0; attempt < 16; ++attempt) {
        std::uint32_t s = seed + static_cast<std::uint32_t>(attempt) * 0x9E3779B9u;
        if (tryGenerate(s, level) && validate(level)) {
            level.attempts = attempt + 1;
            level.dungeon.seed = seed;
            return level;
        }
    }
    level.attempts = 16;
    level.dungeon.seed = seed;
    return level;
}

bool DungeonGenerator::tryGenerate(std::uint32_t seed, GeneratedLevel& out) const {
    Rng rng(seed);
    out = GeneratedLevel();
    out.dungeon = Dungeon(config_.width, config_.height);
    Dungeon& d = out.dungeon;

    // 1. Recursive backtracking maze over odd cells.
    DynamicArray<bool> visited(d.tileCount(), false);
    int cellsX = (d.width() - 1) / 2;
    int cellsY = (d.height() - 1) / 2;
    int sx = 1 + 2 * rng.range(0, cellsX - 1);
    int sy = 1 + 2 * rng.range(0, cellsY - 1);
    carveMaze(d, visited, sx, sy, rng);

    // 2-4. Rooms, loops, pruning.
    placeRooms(d, rng);
    addLoops(d, rng);
    pruneDeadEnds(d);

    // 5. Graph + start/exit.
    d.buildGraph();
    if (!d.rooms().empty()) {
        const Room& r = d.rooms()[0];
        d.start = d.index(r.x + r.w / 2, r.y + r.h / 2);
    } else {
        for (int i = 0; i < d.tileCount(); ++i) {
            if (d.isWalkableIdx(i)) {
                d.start = i;
                break;
            }
        }
    }
    if (!d.isWalkableIdx(d.start)) return false;

    DynamicArray<int> dist = d.graph().bfsDistances(d.start);
    int far = d.start;
    for (int i = 0; i < dist.size(); ++i) {
        if (dist[i] > dist[far]) far = i;
    }
    if (far == d.start) return false;
    d.exit = far;
    d.setTile(d.xOf(far), d.yOf(far), Tile::Exit);

    // Connectivity check through the graph (FR-P2).
    if (d.graph().reachableCount(d.start) != d.walkableCount()) return false;

    // 6. Entities.
    placeEntities(out, dist, rng);
    return true;
}

// Recursive backtracker: carve the current cell, then visit unvisited neighbours
// two tiles away in random order, opening the wall between. When no neighbour
// is left the call returns, which is the "backtrack" step (FR-P1).
void DungeonGenerator::carveMaze(Dungeon& d, DynamicArray<bool>& visited, int x, int y, Rng& rng) const {
    visited[d.index(x, y)] = true;
    d.setTile(x, y, Tile::Floor);

    int order[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; --i) {
        int j = rng.range(0, i);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }

    for (int k = 0; k < 4; ++k) {
        int dir = order[k];
        int nx = x + kDx[dir] * 2;
        int ny = y + kDy[dir] * 2;
        // Bounds rule (FR-P3): the outer border always stays solid wall.
        if (nx < 1 || ny < 1 || nx > d.width() - 2 || ny > d.height() - 2) continue;
        if (visited[d.index(nx, ny)]) continue;
        d.setTile(x + kDx[dir], y + kDy[dir], Tile::Floor);
        carveMaze(d, visited, nx, ny, rng);
    }
}

void DungeonGenerator::placeRooms(Dungeon& d, Rng& rng) const {
    for (int attempt = 0; attempt < config_.roomAttempts && d.rooms().size() < config_.maxRooms; ++attempt) {
        Room r;
        r.w = 2 * rng.range(1, 3) + 1;  // 3, 5 or 7
        r.h = 2 * rng.range(1, 2) + 1;  // 3 or 5
        int maxX = d.width() - 1 - r.w;
        int maxY = d.height() - 1 - r.h;
        if (maxX < 1 || maxY < 1) continue;
        r.x = 1 + 2 * rng.range(0, (maxX - 1) / 2);
        r.y = 1 + 2 * rng.range(0, (maxY - 1) / 2);

        // Overlap rule (FR-P3): rooms never touch each other.
        bool clash = false;
        for (const Room& other : d.rooms()) {
            if (r.overlapsWithMargin(other, 1)) {
                clash = true;
                break;
            }
        }
        if (clash) continue;

        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x) d.setTile(x, y, Tile::Floor);
        d.rooms().push_back(r);
    }
}

void DungeonGenerator::addLoops(Dungeon& d, Rng& rng) const {
    DynamicArray<int> candidates;
    for (int y = 1; y < d.height() - 1; ++y) {
        for (int x = 1; x < d.width() - 1; ++x) {
            if (d.isWalkable(x, y)) continue;
            bool horizontal = d.isWalkable(x - 1, y) && d.isWalkable(x + 1, y) && !d.isWalkable(x, y - 1) && !d.isWalkable(x, y + 1);
            bool vertical = d.isWalkable(x, y - 1) && d.isWalkable(x, y + 1) && !d.isWalkable(x - 1, y) && !d.isWalkable(x + 1, y);
            if (horizontal || vertical) candidates.push_back(d.index(x, y));
        }
    }
    shuffle(candidates, rng);
    int opened = 0;
    for (int i = 0; i < candidates.size() && opened < config_.loopCount; ++i) {
        int x = d.xOf(candidates[i]);
        int y = d.yOf(candidates[i]);
        bool horizontal = d.isWalkable(x - 1, y) && d.isWalkable(x + 1, y) && !d.isWalkable(x, y - 1) && !d.isWalkable(x, y + 1);
        bool vertical = d.isWalkable(x, y - 1) && d.isWalkable(x, y + 1) && !d.isWalkable(x - 1, y) && !d.isWalkable(x + 1, y);
        if (!horizontal && !vertical) continue;
        d.setTile(x, y, Tile::Floor);
        ++opened;
    }
}

void DungeonGenerator::pruneDeadEnds(Dungeon& d) const {
    for (int pass = 0; pass < config_.prunePasses; ++pass) {
        DynamicArray<int> deadEnds;
        for (int y = 1; y < d.height() - 1; ++y) {
            for (int x = 1; x < d.width() - 1; ++x) {
                if (!d.isWalkable(x, y) || d.roomAt(x, y) != -1) continue;
                int open = 0;
                for (int k = 0; k < 4; ++k) open += d.isWalkable(x + kDx[k], y + kDy[k]) ? 1 : 0;
                if (open == 1) deadEnds.push_back(d.index(x, y));
            }
        }
        if (deadEnds.empty()) break;
        // Removing a degree-1 tile can never disconnect the remaining floor.
        for (int idx : deadEnds) d.setTile(d.xOf(idx), d.yOf(idx), Tile::Wall);
    }
}

void DungeonGenerator::placeEntities(GeneratedLevel& level, const DynamicArray<int>& dist, Rng& rng) const {
    Dungeon& d = level.dungeon;
    int maxDist = dist[d.exit];
    int nextUid = 1;

    DynamicArray<bool> occupied(d.tileCount(), false);
    occupied[d.start] = true;
    occupied[d.exit] = true;

    // The Warden stands on the tile right before the exit along a shortest path.
    for (const GraphEdge* e = d.graph().edges(d.exit); e; e = e->next) {
        if (dist[e->to] == maxDist - 1 && e->to != d.start) {
            level.enemies.push_back(makeEnemy(EnemyKind::Warden, e->to, nextUid++));
            occupied[e->to] = true;
            break;
        }
    }

    // Regular enemies: reachable, at least 6 steps from the start, spread out.
    DynamicArray<int> candidates;
    for (int i = 0; i < d.tileCount(); ++i) {
        if (d.tileAt(i) == Tile::Floor && dist[i] >= 6 && !occupied[i]) candidates.push_back(i);
    }
    shuffle(candidates, rng);
    for (int i = 0; i < candidates.size() && level.enemies.size() < config_.enemyCount + 1; ++i) {
        int tile = candidates[i];
        bool tooClose = false;
        for (const Enemy& other : level.enemies) {
            if (manhattan(d, tile, other.tile) < 4) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        double depth = maxDist > 0 ? static_cast<double>(dist[tile]) / maxDist : 0.0;
        EnemyKind kind;
        if (depth < 0.35) kind = rng.chance(0.6) ? EnemyKind::Slime : EnemyKind::Goblin;
        else if (depth < 0.7) kind = rng.chance(0.5) ? EnemyKind::Goblin : EnemyKind::Skeleton;
        else kind = rng.chance(0.55) ? EnemyKind::Skeleton : EnemyKind::Orc;

        level.enemies.push_back(makeEnemy(kind, tile, nextUid++));
        occupied[tile] = true;
    }

    // Items: reachable, not on an enemy, spread out.
    candidates.clear();
    for (int i = 0; i < d.tileCount(); ++i) {
        if (d.tileAt(i) == Tile::Floor && dist[i] >= 2 && !occupied[i]) candidates.push_back(i);
    }
    shuffle(candidates, rng);
    for (int i = 0; i < candidates.size() && level.items.size() < config_.itemCount; ++i) {
        int tile = candidates[i];
        bool tooClose = false;
        for (const FloorItem& other : level.items) {
            if (manhattan(d, tile, other.tile) < 3) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        ItemType type;
        if (level.items.size() < 2) {
            type = ItemType::HealthPotion;  // always at least two potions
        } else {
            int roll = rng.range(0, 99);
            if (roll < 34) type = ItemType::HealthPotion;
            else if (roll < 44) type = ItemType::Elixir;
            else if (roll < 60) type = ItemType::FireBomb;
            else if (roll < 73) type = ItemType::StrengthTonic;
            else if (roll < 85) type = ItemType::IronSkin;
            else type = ItemType::SmokeBomb;
        }
        FloorItem fi;
        fi.item = Item{type, nextUid++};
        fi.tile = tile;
        level.items.push_back(fi);
        occupied[tile] = true;
    }
}

bool DungeonGenerator::validate(const GeneratedLevel& level, std::string* reason) {
    auto fail = [reason](const char* why) {
        if (reason) *reason = why;
        return false;
    };
    const Dungeon& d = level.dungeon;
    if (d.width() < 3 || d.height() < 3) return fail("dungeon too small");
    if (!d.isValidIndex(d.start) || !d.isWalkableIdx(d.start)) return fail("invalid start");
    if (!d.isValidIndex(d.exit) || d.tileAt(d.exit) != Tile::Exit) return fail("invalid exit");
    if (d.start == d.exit) return fail("start equals exit");

    // Border must be solid.
    for (int x = 0; x < d.width(); ++x)
        if (d.isWalkable(x, 0) || d.isWalkable(x, d.height() - 1)) return fail("open border");
    for (int y = 0; y < d.height(); ++y)
        if (d.isWalkable(0, y) || d.isWalkable(d.width() - 1, y)) return fail("open border");

    // Rooms in bounds and non-overlapping.
    const DynamicArray<Room>& rooms = d.rooms();
    for (int i = 0; i < rooms.size(); ++i) {
        const Room& r = rooms[i];
        if (r.x < 1 || r.y < 1 || r.x + r.w > d.width() - 1 || r.y + r.h > d.height() - 1) return fail("room out of bounds");
        for (int j = i + 1; j < rooms.size(); ++j)
            if (r.overlapsWithMargin(rooms[j], 0)) return fail("rooms overlap");
    }

    // Graph consistency and connectivity.
    if (d.graph().vertexCount() != d.tileCount()) return fail("graph size mismatch");
    if (d.graph().reachableCount(d.start) != d.walkableCount()) return fail("dungeon not fully connected");
    if (!d.graph().isConnected(d.start, d.exit)) return fail("exit unreachable");

    // Entities on valid, distinct, walkable tiles.
    DynamicArray<bool> used(d.tileCount(), false);
    used[d.start] = true;
    used[d.exit] = true;
    for (const Enemy& e : level.enemies) {
        if (!d.isValidIndex(e.tile) || !d.isWalkableIdx(e.tile)) return fail("enemy on invalid tile");
        if (used[e.tile]) return fail("enemy overlaps another entity");
        used[e.tile] = true;
    }
    for (const FloorItem& it : level.items) {
        if (!d.isValidIndex(it.tile) || !d.isWalkableIdx(it.tile)) return fail("item on invalid tile");
        if (used[it.tile]) return fail("item overlaps another entity");
        used[it.tile] = true;
    }
    return true;
}
