// Tests for the core DSA layer and game logic layer (PRD section 10.1).
// Builds without SFML.

#include "../src/core/BinaryHeap.hpp"
#include "../src/core/DynamicArray.hpp"
#include "../src/core/Graph.hpp"
#include "../src/game/BattleSystem.hpp"
#include "../src/game/DungeonGenerator.hpp"
#include "../src/game/GameSession.hpp"
#include "../src/game/Inventory.hpp"
#include "../src/game/Pathfinder.hpp"
#include "TestFramework.hpp"

#include <string>

namespace {
struct IntLess {
    bool operator()(int a, int b) const { return a < b; }
};
struct IntGreater {
    bool operator()(int a, int b) const { return a > b; }
};

// Builds a grid graph from an ASCII map ('#' = wall).
Graph gridGraph(const char* const* rows, int w, int h) {
    Graph g(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (rows[y][x] == '#') continue;
            if (x + 1 < w && rows[y][x + 1] != '#') g.addEdge(y * w + x, y * w + x + 1);
            if (y + 1 < h && rows[y + 1][x] != '#') g.addEdge(y * w + x, (y + 1) * w + x);
        }
    return g;
}
}  // namespace

// ---------------------------------------------------------------- DynamicArray
TEST(dynamic_array_push_remove_copy) {
    DynamicArray<int> a;
    CHECK(a.empty());
    for (int i = 0; i < 100; ++i) a.push_back(i);
    CHECK(a.size() == 100);
    CHECK(a[99] == 99);
    a.removeAt(0);
    CHECK(a[0] == 1 && a.size() == 99);
    a.insertAt(0, 42);
    CHECK(a[0] == 42);
    DynamicArray<int> b = a;  // deep copy
    b[0] = 7;
    CHECK(a[0] == 42);
    DynamicArray<int> c = std::move(b);
    CHECK(c[0] == 7 && b.size() == 0);
    a.push_back(a[0]);  // aliasing on growth
    CHECK(a.back() == 42);
    CHECK_THROWS(a[1000]);
    CHECK_THROWS(DynamicArray<int>().pop_back());
}

// ----------------------------------------------------------------------- Graph
TEST(graph_add_vertices_and_edges) {
    Graph g(4);
    CHECK(g.vertexCount() == 4);
    CHECK(g.addEdge(0, 1));
    CHECK(g.addEdge(1, 2));
    CHECK(g.edgeCount() == 2);
    CHECK(g.hasEdge(1, 0));  // undirected
    int v = g.addVertex();
    CHECK(v == 4 && g.vertexCount() == 5);
    CHECK(g.addEdge(3, 4));
}

TEST(graph_rejects_duplicate_and_invalid_edges) {
    Graph g(3);
    CHECK(g.addEdge(0, 1));
    CHECK(!g.addEdge(0, 1));
    CHECK(!g.addEdge(1, 0));
    CHECK(!g.addEdge(0, 0));   // self loop
    CHECK(!g.addEdge(-1, 2));  // invalid id
    CHECK(!g.addEdge(0, 99));
    CHECK(g.edgeCount() == 1);
    CHECK(!g.hasEdge(0, 99));
    CHECK(g.degree(99) == 0);
}

TEST(graph_neighbors_and_removal) {
    Graph g(4);
    g.addEdge(0, 1);
    g.addEdge(0, 2);
    g.addEdge(0, 3);
    DynamicArray<int> n = g.neighbors(0);
    CHECK(n.size() == 3);
    CHECK(g.degree(0) == 3 && g.degree(1) == 1);
    CHECK(g.removeEdge(0, 2));
    CHECK(!g.hasEdge(2, 0));
    CHECK(!g.removeEdge(0, 2));
    CHECK(g.edgeCount() == 2);
}

TEST(graph_connectivity_and_copy) {
    Graph g(5);
    g.addEdge(0, 1);
    g.addEdge(1, 2);
    g.addEdge(3, 4);
    CHECK(g.isConnected(0, 2));
    CHECK(!g.isConnected(0, 4));
    CHECK(g.reachableCount(0) == 3);
    CHECK(g.bfsDistances(0)[2] == 2);
    Graph copy = g;
    copy.addEdge(2, 3);
    CHECK(copy.isConnected(0, 4));
    CHECK(!g.isConnected(0, 4));  // original untouched
    Graph assigned;
    assigned = copy;
    CHECK(assigned.edgeCount() == copy.edgeCount());
}

// ------------------------------------------------------------------ BinaryHeap
TEST(heap_extracts_in_order) {
    BinaryHeap<int, IntLess> h;
    int values[] = {5, 3, 9, 1, 7, 2, 8, 6, 4, 0};
    for (int v : values) h.push(v);
    CHECK(h.isValid());
    for (int expect = 0; expect < 10; ++expect) CHECK(h.pop() == expect);
    CHECK(h.empty());
}

TEST(heap_max_heap_and_ties) {
    BinaryHeap<int, IntGreater> h;
    int values[] = {4, 4, 9, 1, 9, 4};
    for (int v : values) h.push(v);
    int expected[] = {9, 9, 4, 4, 4, 1};
    for (int e : expected) CHECK(h.pop() == e);
}

TEST(heap_empty_extract_is_safe) {
    BinaryHeap<int, IntLess> h;
    int out = -1;
    CHECK(!h.tryPop(out));
    CHECK(out == -1);
    CHECK_THROWS(h.pop());
    CHECK_THROWS(h.top());
}

TEST(heap_large_input) {
    BinaryHeap<int, IntLess> h;
    Rng rng(1234);
    for (int i = 0; i < 20000; ++i) h.push(rng.range(-100000, 100000));
    CHECK(h.isValid());
    int prev = h.pop();
    bool sorted = true;
    while (!h.empty()) {
        int v = h.pop();
        if (v < prev) sorted = false;
        prev = v;
    }
    CHECK(sorted);
}

TEST(heap_turn_events_tie_break_by_sequence) {
    BinaryHeap<TurnEvent, TurnEventCompare> q;
    q.push({10.0, 2, Actor::Enemy});
    q.push({10.0, 1, Actor::Player});
    q.push({5.0, 3, Actor::Enemy});
    CHECK(q.pop().time == 5.0);
    CHECK(q.pop().actor == Actor::Player);
    CHECK(q.pop().actor == Actor::Enemy);
}

// ------------------------------------------------------------------ Generation
TEST(generation_many_seeds_are_valid) {
    DungeonGenerator gen;
    int failed = 0;
    std::string firstReason;
    for (std::uint32_t seed = 1; seed <= 500; ++seed) {
        GeneratedLevel level = gen.generate(seed);
        std::string reason;
        if (!DungeonGenerator::validate(level, &reason)) {
            if (failed == 0) firstReason = reason;
            ++failed;
        }
    }
    if (failed) std::printf("    first failure: %s\n", firstReason.c_str());
    CHECK(failed == 0);
}

TEST(generation_start_and_exit_reachable) {
    DungeonGenerator gen;
    for (std::uint32_t seed = 900; seed < 950; ++seed) {
        GeneratedLevel level = gen.generate(seed);
        const Dungeon& d = level.dungeon;
        CHECK(d.graph().isConnected(d.start, d.exit));
        CHECK(d.graph().reachableCount(d.start) == d.walkableCount());
        CHECK(level.enemies.size() >= 1);
        CHECK(level.items.size() >= 2);
    }
}

TEST(generation_is_reproducible_with_seed) {
    DungeonGenerator gen;
    GeneratedLevel a = gen.generate(777);
    GeneratedLevel b = gen.generate(777);
    GeneratedLevel c = gen.generate(778);
    bool same = true, different = false;
    for (int i = 0; i < a.dungeon.tileCount(); ++i) {
        if (a.dungeon.tileAt(i) != b.dungeon.tileAt(i)) same = false;
        if (a.dungeon.tileAt(i) != c.dungeon.tileAt(i)) different = true;
    }
    CHECK(same);
    CHECK(different);
    CHECK(a.dungeon.start == b.dungeon.start && a.dungeon.exit == b.dungeon.exit);
    CHECK(a.enemies.size() == b.enemies.size());
}

TEST(generation_border_and_bounds) {
    DungeonGenerator gen;
    GeneratedLevel level = gen.generate(31337);
    const Dungeon& d = level.dungeon;
    CHECK(d.width() % 2 == 1 && d.height() % 2 == 1);
    CHECK(!d.isWalkable(-1, 0));
    CHECK(!d.isWalkable(d.width(), 0));
    CHECK(d.tile(500, 500) == Tile::Wall);
}

// ---------------------------------------------------------------------- A*
TEST(astar_finds_optimal_path) {
    const char* rows[] = {
        ".....",
        ".###.",
        ".#...",
        ".#.#.",
        "...#.",
    };
    Graph g = gridGraph(rows, 5, 5);
    PathResult r = Pathfinder::findPath(g, 5, 0, 24);
    CHECK(r.found);
    CHECK(r.cost == 8);
    CHECK(r.path.size() == 9);
    CHECK(r.path[0] == 0 && r.path.back() == 24);
    for (int i = 1; i < r.path.size(); ++i) CHECK(g.hasEdge(r.path[i - 1], r.path[i]));
}

TEST(astar_no_path) {
    const char* rows[] = {
        "..#..",
        "..#..",
        "..#..",
    };
    Graph g = gridGraph(rows, 5, 3);
    PathResult r = Pathfinder::findPath(g, 5, 0, 4);
    CHECK(!r.found);
    CHECK(r.path.empty());
}

TEST(astar_start_equals_goal_and_blocked_goal) {
    const char* rows[] = {
        "...",
        ".#.",
        "...",
    };
    Graph g = gridGraph(rows, 3, 3);
    PathResult same = Pathfinder::findPath(g, 3, 0, 0);
    CHECK(same.found && same.cost == 0 && same.path.size() == 1);
    PathResult wall = Pathfinder::findPath(g, 3, 0, 4);  // center is a wall
    CHECK(!wall.found);
    PathResult invalid = Pathfinder::findPath(g, 3, 0, 99);
    CHECK(!invalid.found);
}

TEST(astar_respects_passable_mask) {
    const char* rows[] = {
        "...",
        "...",
        "...",
    };
    Graph g = gridGraph(rows, 3, 3);
    DynamicArray<bool> mask(9, true);
    mask[1] = mask[4] = mask[7] = false;  // block the middle column
    PathResult r = Pathfinder::findPath(g, 3, 0, 2, &mask);
    CHECK(!r.found);
    mask[7] = true;
    r = Pathfinder::findPath(g, 3, 0, 2, &mask);
    CHECK(r.found && r.cost == 6);
}

TEST(astar_matches_bfs_on_generated_dungeons) {
    DungeonGenerator gen;
    for (std::uint32_t seed = 10; seed < 60; ++seed) {
        GeneratedLevel level = gen.generate(seed);
        const Dungeon& d = level.dungeon;
        PathResult r = Pathfinder::findPath(d.graph(), d.width(), d.start, d.exit);
        CHECK(r.found);
        CHECK(r.cost == d.graph().bfsDistances(d.start)[d.exit]);
    }
}

// ------------------------------------------------------------------ Inventory
TEST(inventory_collect_use_discard) {
    Inventory inv(3);
    CHECK(inv.isEmpty());
    CHECK(inv.add({ItemType::HealthPotion, 1}));
    CHECK(inv.add({ItemType::FireBomb, 2}));
    CHECK(inv.add({ItemType::Elixir, 3}));
    CHECK(inv.isFull());
    CHECK(!inv.add({ItemType::SmokeBomb, 4}));  // capacity limit
    CHECK(inv.countOf(ItemType::FireBomb) == 1);
    CHECK(inv.remove(1));
    CHECK(inv.size() == 2 && inv.at(1).type == ItemType::Elixir);
    CHECK(!inv.remove(-1));
    CHECK(!inv.remove(5));
}

TEST(inventory_item_effects) {
    Player p = makePlayer(0);
    p.stats.hp = 50;
    ItemUseResult r = applyItemEffect(ItemType::HealthPotion, p, nullptr, false);
    CHECK(r.success && r.consumed && p.stats.hp == 85);
    r = applyItemEffect(ItemType::Elixir, p, nullptr, false);
    CHECK(r.success && p.stats.hp == p.stats.maxHp);
    r = applyItemEffect(ItemType::HealthPotion, p, nullptr, false);
    CHECK(!r.success && !r.consumed);  // already full
    r = applyItemEffect(ItemType::FireBomb, p, nullptr, false);
    CHECK(!r.success);  // battle only
    int atk = p.stats.attack;
    r = applyItemEffect(ItemType::StrengthTonic, p, nullptr, false);
    CHECK(r.success && p.stats.attack == atk + 3);
    Stats enemy{40, 40, 5, 0, 5};
    r = applyItemEffect(ItemType::FireBomb, p, &enemy, false);
    CHECK(r.success && enemy.hp == 10);
    r = applyItemEffect(ItemType::SmokeBomb, p, &enemy, true);
    CHECK(!r.success);  // cannot flee a boss
}

TEST(session_inventory_edge_cases) {
    GameSession s;
    s.newRun(42);
    ItemUseResult r = s.useItem(0);
    CHECK(!r.success);  // empty
    std::string fb;
    CHECK(!s.discardItem(0, &fb));
    s.mutableInventory().add({ItemType::IronSkin, 99});
    CHECK(!s.useItem(3).success);  // invalid index
    int def = s.player().stats.defense;
    CHECK(s.useItem(0).success);
    CHECK(s.player().stats.defense == def + 2);
    CHECK(s.inventory().isEmpty());
}

// --------------------------------------------------------------------- Combat
namespace {
struct BattleRig {
    Player player = makePlayer(0);
    Enemy enemy = makeEnemy(EnemyKind::Skeleton, 1, 1);
    Inventory inventory;
    Rng rng{99};
    BattleSystem battle;
};
}  // namespace

TEST(combat_turn_order_by_initiative) {
    BattleRig rig;
    rig.player.stats.speed = 10;
    rig.enemy.stats.speed = 20;  // twice as fast
    rig.battle.begin(&rig.player, &rig.enemy, &rig.inventory, &rig.rng);
    CHECK(rig.battle.phase() == BattlePhase::EnemyTurn);  // faster acts first
    DynamicArray<TurnEvent> order = rig.battle.upcoming(6);
    int enemyTurns = 0;
    for (const TurnEvent& e : order) enemyTurns += e.actor == Actor::Enemy ? 1 : 0;
    CHECK(enemyTurns == 4);
    for (int i = 1; i < order.size(); ++i) CHECK(order[i].time >= order[i - 1].time);
}

TEST(combat_damage_and_enemy_death) {
    BattleRig rig;
    rig.player.stats.speed = 50;
    rig.enemy.stats.hp = 1;
    rig.battle.begin(&rig.player, &rig.enemy, &rig.inventory, &rig.rng);
    CHECK(rig.battle.phase() == BattlePhase::PlayerTurn);
    ActionResult r = rig.battle.playerAct(BattleAction::Attack);
    CHECK(r.accepted);
    CHECK(rig.enemy.stats.hp == 0);
    CHECK(rig.battle.phase() == BattlePhase::Won);
}

TEST(combat_defend_halves_damage) {
    // Same RNG seed in both rigs: the defended hit must be about half.
    BattleRig a, b;
    a.player.stats.speed = b.player.stats.speed = 20;
    a.enemy.stats.speed = b.enemy.stats.speed = 19;
    a.battle.begin(&a.player, &a.enemy, &a.inventory, &a.rng);
    b.battle.begin(&b.player, &b.enemy, &b.inventory, &b.rng);
    a.battle.playerAct(BattleAction::Attack);
    b.battle.playerAct(BattleAction::Defend);
    // Make the enemy's next action identical in both rigs.
    a.rng.reseed(5);
    b.rng.reseed(5);
    int hpA = a.player.stats.hp, hpB = b.player.stats.hp;
    CHECK(a.battle.phase() == BattlePhase::EnemyTurn && b.battle.phase() == BattlePhase::EnemyTurn);
    a.battle.enemyAct();
    b.battle.enemyAct();
    int takenA = hpA - a.player.stats.hp;
    int takenB = hpB - b.player.stats.hp;
    CHECK(takenB <= takenA);
    CHECK(takenB >= 1);
}

TEST(combat_invalid_actions_do_not_consume_turn) {
    BattleRig rig;
    rig.player.stats.speed = 30;
    rig.battle.begin(&rig.player, &rig.enemy, &rig.inventory, &rig.rng);
    CHECK(rig.battle.phase() == BattlePhase::PlayerTurn);
    ActionResult r = rig.battle.playerAct(BattleAction::UseItem, 0);  // empty pack
    CHECK(!r.accepted);
    CHECK(rig.battle.phase() == BattlePhase::PlayerTurn);
    CHECK(rig.battle.round() == 0);
    rig.inventory.add({ItemType::HealthPotion, 1});
    r = rig.battle.playerAct(BattleAction::UseItem, 0);  // full health: rejected
    CHECK(!r.accepted);
    CHECK(rig.inventory.size() == 1);
    r = rig.battle.playerAct(BattleAction::UseItem, 7);  // invalid index
    CHECK(!r.accepted);
}

TEST(combat_escape_rules) {
    BattleRig rig;
    rig.enemy = makeEnemy(EnemyKind::Warden, 1, 1);
    rig.player.stats.speed = 30;
    rig.battle.begin(&rig.player, &rig.enemy, &rig.inventory, &rig.rng);
    ActionResult r = rig.battle.playerAct(BattleAction::Escape);
    CHECK(!r.accepted);  // bosses block escape
    CHECK(rig.battle.phase() == BattlePhase::PlayerTurn);

    BattleRig rig2;
    rig2.player.stats.speed = 30;
    rig2.inventory.add({ItemType::SmokeBomb, 1});
    rig2.battle.begin(&rig2.player, &rig2.enemy, &rig2.inventory, &rig2.rng);
    r = rig2.battle.playerAct(BattleAction::UseItem, 0);
    CHECK(r.accepted);
    CHECK(rig2.battle.phase() == BattlePhase::Escaped);
}

TEST(combat_player_death) {
    BattleRig rig;
    rig.player.stats.hp = 1;
    rig.player.stats.defense = 0;
    rig.enemy.stats.speed = 40;
    rig.battle.begin(&rig.player, &rig.enemy, &rig.inventory, &rig.rng);
    for (int i = 0; i < 20 && !rig.battle.isOver(); ++i) {
        if (rig.battle.phase() == BattlePhase::EnemyTurn) rig.battle.enemyAct();
        else rig.battle.playerAct(BattleAction::Defend);
    }
    CHECK(rig.battle.phase() == BattlePhase::Lost);
    CHECK(rig.player.stats.hp == 0);
}

TEST(combat_low_health_heal_and_damage_floor) {
    // Insufficient health: a potion at 1 HP is accepted and consumes the turn.
    BattleRig rig;
    rig.player.stats.hp = 1;
    rig.player.stats.speed = 30;
    rig.inventory.add({ItemType::HealthPotion, 1});
    rig.battle.begin(&rig.player, &rig.enemy, &rig.inventory, &rig.rng);
    CHECK(rig.battle.phase() == BattlePhase::PlayerTurn);
    ActionResult r = rig.battle.playerAct(BattleAction::UseItem, 0);
    CHECK(r.accepted);
    CHECK(rig.player.stats.hp == 36);
    CHECK(rig.inventory.isEmpty());
    CHECK(rig.battle.round() == 1);

    // Damage never drops below 1, even against overwhelming armour.
    BattleRig tank;
    tank.player.stats.speed = 30;
    tank.enemy.stats.defense = 999;
    tank.battle.begin(&tank.player, &tank.enemy, &tank.inventory, &tank.rng);
    int before = tank.enemy.stats.hp;
    tank.battle.playerAct(BattleAction::Attack);
    CHECK(tank.enemy.stats.hp == before - 1);
}

TEST(combat_victory_awards_xp_and_removes_enemy) {
    GameSession s;
    s.newRun(55);
    const Dungeon& d = s.dungeon();
    Enemy& e = s.mutableEnemies()[1];  // index 0 is the Warden
    e.stats.hp = 1;
    int px = d.xOf(s.player().tile), py = d.yOf(s.player().tile);
    const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int k = 0; k < 4; ++k) {
        if (!d.isWalkable(px + dx[k], py + dy[k])) continue;
        e.tile = d.index(px + dx[k], py + dy[k]);
        CHECK(s.move(dx[k], dy[k]) == MoveResult::BattleStarted);
        s.mutablePlayer().stats.hp = 100;
        for (int i = 0; i < 20 && !s.battle().isOver(); ++i) {
            if (s.battle().phase() == BattlePhase::EnemyTurn) s.battle().enemyAct();
            else s.battle().playerAct(BattleAction::Attack);
        }
        CHECK(s.battle().phase() == BattlePhase::Won);
        CHECK(s.finishBattle() > 0);
        CHECK(s.phase() == SessionPhase::Exploring);
        CHECK(!s.mutableEnemies()[1].alive);
        CHECK(s.enemyAt(e.tile) == -1);
        CHECK(s.stats().kills == 1);
        break;
    }
}

// ------------------------------------------------------------------- Movement
TEST(movement_map_boundary) {
    GameSession s;
    s.newRun(9);
    const Dungeon& d = s.dungeon();
    // Force the player onto each edge and try to step off the map.
    s.mutablePlayer().tile = d.index(0, 5);
    CHECK(s.move(-1, 0) == MoveResult::OutOfBounds);
    s.mutablePlayer().tile = d.index(d.width() - 1, 5);
    CHECK(s.move(1, 0) == MoveResult::OutOfBounds);
    s.mutablePlayer().tile = d.index(5, 0);
    CHECK(s.move(0, -1) == MoveResult::OutOfBounds);
    s.mutablePlayer().tile = d.index(5, d.height() - 1);
    CHECK(s.move(0, 1) == MoveResult::OutOfBounds);
    CHECK(s.player().tile == d.index(5, d.height() - 1));
}

TEST(movement_walls_boundaries_and_exit) {
    GameSession s;
    s.newRun(2024);
    const Dungeon& d = s.dungeon();

    // Find a wall next to the player, and check movement into it is blocked.
    int px = d.xOf(s.player().tile), py = d.yOf(s.player().tile);
    const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int k = 0; k < 4; ++k) {
        if (!d.isWalkable(px + dx[k], py + dy[k])) {
            CHECK(s.move(dx[k], dy[k]) == MoveResult::Blocked);
            CHECK(s.player().tile == d.index(px, py));
        }
    }

    // Clear the way, then walk the A* route to the exit.
    for (Enemy& e : s.mutableEnemies()) e.alive = false;
    PathResult route = Pathfinder::findPath(d.graph(), d.width(), d.start, d.exit);
    CHECK(route.found);
    MoveResult last = MoveResult::Moved;
    for (int i = 1; i < route.path.size(); ++i) {
        int from = route.path[i - 1], to = route.path[i];
        last = s.move(d.xOf(to) - d.xOf(from), d.yOf(to) - d.yOf(from));
    }
    CHECK(last == MoveResult::ReachedExit);
    CHECK(s.phase() == SessionPhase::Victory);
    CHECK(s.move(1, 0) == MoveResult::NotAllowed);
}

TEST(movement_enemy_contact_starts_battle) {
    GameSession s;
    s.newRun(55);
    const Dungeon& d = s.dungeon();
    Enemy& e = s.mutableEnemies()[0];
    // Teleport the enemy next to the player onto a walkable tile.
    int px = d.xOf(s.player().tile), py = d.yOf(s.player().tile);
    const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int k = 0; k < 4; ++k) {
        if (d.isWalkable(px + dx[k], py + dy[k])) {
            e.tile = d.index(px + dx[k], py + dy[k]);
            int before = s.player().tile;
            CHECK(s.move(dx[k], dy[k]) == MoveResult::BattleStarted);
            CHECK(s.phase() == SessionPhase::Battle);
            CHECK(s.player().tile == before);
            break;
        }
    }
}

TEST(movement_item_pickup) {
    GameSession s;
    s.newRun(77);
    const Dungeon& d = s.dungeon();
    for (Enemy& e : s.mutableEnemies()) e.alive = false;
    const FloorItem& target = s.floorItems()[0];
    PathResult route = Pathfinder::findPath(d.graph(), d.width(), s.player().tile, target.tile);
    CHECK(route.found);
    MoveResult last = MoveResult::Moved;
    int pickups = 0;
    for (int i = 1; i < route.path.size(); ++i) {
        int from = route.path[i - 1], to = route.path[i];
        last = s.move(d.xOf(to) - d.xOf(from), d.yOf(to) - d.yOf(from));
        if (last == MoveResult::PickedUp) ++pickups;
    }
    CHECK(last == MoveResult::PickedUp);
    CHECK(s.inventory().size() == pickups);
    CHECK(s.floorItems()[0].taken);
}

TEST(session_plan_path_feedback) {
    GameSession s;
    s.newRun(3);
    std::string fb;
    CHECK(!s.planPath(0, &fb));  // corner is unexplored rock
    CHECK(!fb.empty());
    CHECK(!s.planPath(s.player().tile, &fb));  // already there
    CHECK(!s.planPathToExit(&fb) || s.exitDiscovered());
}

int main() { return testfw::runAll(); }
