#include "GameSession.hpp"

GameSession::GameSession(GeneratorConfig config) : generator_(config) {}

void GameSession::newRun(std::uint32_t seed) {
    seed_ = seed;
    level_ = generator_.generate(seed);
    rng_.reseed(seed ^ 0xA5A5F00Du);
    player_ = makePlayer(level_.dungeon.start);
    inventory_ = Inventory(Inventory::kDefaultCapacity);
    battle_ = BattleSystem();
    phase_ = SessionPhase::Exploring;
    battleEnemy_ = -1;
    lastDir_ = 2;
    stats_ = RunStats();
    messages_.clear();
    plannedPath_ = PathResult();
    pathCursor_ = 0;

    int n = level_.dungeon.tileCount();
    explored_ = DynamicArray<bool>(n, false);
    visible_ = DynamicArray<bool>(n, false);
    updateVisibility();
    hasRun_ = true;

    log("You descend into the Lost Realms. Find the exit portal.", LogKind::Arcane);
}

void GameSession::log(const std::string& text, LogKind kind) {
    messages_.push_back({text, kind});
    if (messages_.size() > 60) messages_.removeAt(0);
}

void GameSession::revealAll() {
    for (int i = 0; i < explored_.size(); ++i) explored_[i] = true;
}

int GameSession::enemyAt(int tile) const {
    for (int i = 0; i < level_.enemies.size(); ++i) {
        if (level_.enemies[i].alive && level_.enemies[i].tile == tile) return i;
    }
    return -1;
}

int GameSession::itemAt(int tile) const {
    for (int i = 0; i < level_.items.size(); ++i) {
        if (!level_.items[i].taken && level_.items[i].tile == tile) return i;
    }
    return -1;
}

int GameSession::enemiesRemaining() const {
    int n = 0;
    for (const Enemy& e : level_.enemies) n += e.alive ? 1 : 0;
    return n;
}

int GameSession::exploredPercent() const {
    const Dungeon& d = level_.dungeon;
    int walk = 0, seen = 0;
    for (int i = 0; i < d.tileCount(); ++i) {
        if (!d.isWalkableIdx(i)) continue;
        ++walk;
        if (explored_[i]) ++seen;
    }
    return walk ? (seen * 100) / walk : 0;
}

MoveResult GameSession::move(int dx, int dy) {
    if (phase_ != SessionPhase::Exploring) return MoveResult::NotAllowed;
    if (dy < 0) lastDir_ = 0;
    else if (dx > 0) lastDir_ = 1;
    else if (dy > 0) lastDir_ = 2;
    else if (dx < 0) lastDir_ = 3;

    const Dungeon& d = level_.dungeon;
    int nx = d.xOf(player_.tile) + dx;
    int ny = d.yOf(player_.tile) + dy;
    if (!d.inBounds(nx, ny)) return MoveResult::OutOfBounds;
    if (!d.isWalkable(nx, ny)) return MoveResult::Blocked;
    return stepOnto(d.index(nx, ny));
}

MoveResult GameSession::stepOnto(int tile) {
    const Dungeon& d = level_.dungeon;

    int enemy = enemyAt(tile);
    if (enemy != -1) {
        startBattle(enemy);
        return MoveResult::BattleStarted;
    }

    player_.tile = tile;
    ++stats_.steps;
    updateVisibility();

    if (tile == d.exit) {
        phase_ = SessionPhase::Victory;
        clearPath();
        log("You step through the portal and escape the Lost Realms!", LogKind::Success);
        return MoveResult::ReachedExit;
    }

    int item = itemAt(tile);
    if (item != -1) {
        FloorItem& fi = level_.items[item];
        if (inventory_.add(fi.item)) {
            fi.taken = true;
            ++stats_.itemsCollected;
            log(std::string("Picked up ") + itemInfo(fi.item.type).name + ".", LogKind::Success);
            return MoveResult::PickedUp;
        }
        log(std::string("Your pack is full. The ") + itemInfo(fi.item.type).name + " stays on the floor.", LogKind::Warning);
        return MoveResult::InventoryFull;
    }
    return MoveResult::Moved;
}

void GameSession::startBattle(int enemyIndex) {
    clearPath();
    battleEnemy_ = enemyIndex;
    phase_ = SessionPhase::Battle;
    battle_.begin(&player_, &level_.enemies[enemyIndex], &inventory_, &rng_);
}

int GameSession::finishBattle(int* levelsGained) {
    if (levelsGained) *levelsGained = 0;
    if (phase_ != SessionPhase::Battle || !battle_.isOver()) return 0;

    stats_.damageDealt += battle_.damageDealt();
    stats_.damageTaken += battle_.damageTaken();
    stats_.itemsUsed += battle_.itemsUsed();
    Enemy& enemy = level_.enemies[battleEnemy_];
    int xp = 0;

    switch (battle_.phase()) {
        case BattlePhase::Won: {
            enemy.alive = false;
            ++stats_.kills;
            xp = enemyInfo(enemy.kind).xp;
            int levels = player_.gainXp(xp);
            if (levelsGained) *levelsGained = levels;
            log(std::string("Defeated the ") + enemy.name() + ". +" + std::to_string(xp) + " XP.", LogKind::Success);
            if (levels > 0) log("Level up! You are now level " + std::to_string(player_.level) + ".", LogKind::Arcane);
            phase_ = SessionPhase::Exploring;
            break;
        }
        case BattlePhase::Escaped:
            ++stats_.battlesFled;
            log(std::string("You escaped from the ") + enemy.name() + ".", LogKind::Info);
            phase_ = SessionPhase::Exploring;
            break;
        case BattlePhase::Lost:
            log("You have fallen in the Lost Realms.", LogKind::Danger);
            phase_ = SessionPhase::Defeat;
            break;
        default:
            break;
    }
    battle_ = BattleSystem();
    battleEnemy_ = -1;
    return xp;
}

bool GameSession::planPath(int tile, std::string* feedback) {
    auto say = [feedback](const std::string& s) {
        if (feedback) *feedback = s;
    };
    const Dungeon& d = level_.dungeon;
    if (phase_ != SessionPhase::Exploring) {
        say("You cannot plan a route right now.");
        return false;
    }
    if (!d.isValidIndex(tile)) {
        say("That location is outside the dungeon.");
        return false;
    }
    if (!explored_[tile]) {
        say("No path: that area is unexplored.");
        plannedPath_ = PathResult();
        return false;
    }
    if (!d.isWalkableIdx(tile)) {
        say("No path: that tile is solid rock.");
        plannedPath_ = PathResult();
        return false;
    }
    if (tile == player_.tile) {
        say("You are already standing there.");
        plannedPath_ = PathResult();
        return false;
    }

    // Search explored tiles only, steering around visible enemies (except the target).
    DynamicArray<bool> passable(d.tileCount(), false);
    for (int i = 0; i < d.tileCount(); ++i) passable[i] = explored_[i] && d.isWalkableIdx(i);
    for (const Enemy& e : level_.enemies) {
        if (e.alive && e.tile != tile && visible_[e.tile]) passable[e.tile] = false;
    }

    plannedPath_ = Pathfinder::findPath(d.graph(), d.width(), player_.tile, tile, &passable);
    pathCursor_ = 0;
    ++stats_.pathsPlanned;
    if (!plannedPath_.found) {
        say("No path: the route is blocked.");
        return false;
    }
    say("Route found: " + std::to_string(plannedPath_.path.size() - 1) + " steps.");
    return true;
}

bool GameSession::planPathToExit(std::string* feedback) {
    if (!exitDiscovered()) {
        if (feedback) *feedback = "You have not found the exit portal yet.";
        return false;
    }
    return planPath(level_.dungeon.exit, feedback);
}

MoveResult GameSession::followPathStep() {
    if (phase_ != SessionPhase::Exploring || !hasPlannedPath()) return MoveResult::NotAllowed;
    if (pathCursor_ + 1 >= plannedPath_.path.size()) {
        clearPath();
        return MoveResult::NotAllowed;
    }
    const Dungeon& d = level_.dungeon;
    int next = plannedPath_.path[pathCursor_ + 1];
    int dx = d.xOf(next) - d.xOf(player_.tile);
    int dy = d.yOf(next) - d.yOf(player_.tile);
    if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) != 1) {  // desynced: stop safely
        clearPath();
        return MoveResult::NotAllowed;
    }
    MoveResult r = move(dx, dy);
    if (r == MoveResult::Moved || r == MoveResult::PickedUp || r == MoveResult::InventoryFull) {
        ++pathCursor_;
        if (pathCursor_ + 1 >= plannedPath_.path.size()) clearPath();
    }
    return r;
}

void GameSession::clearPath() {
    // Keep `found`, cost and the expanded set so the overlay/HUD can still
    // show the last search; only the walkable route is dropped.
    plannedPath_.path.clear();
    pathCursor_ = 0;
}

ItemUseResult GameSession::useItem(int index) {
    ItemUseResult r;
    if (inventory_.isEmpty()) {
        r.message = "Your pack is empty.";
        return r;
    }
    if (!inventory_.isValidIndex(index)) {
        r.message = "Select a valid item first.";
        return r;
    }
    r = applyItemEffect(inventory_.at(index).type, player_, nullptr, false);
    if (r.success) {
        if (r.consumed) inventory_.remove(index);
        ++stats_.itemsUsed;
        log(r.message, r.healed > 0 ? LogKind::Heal : LogKind::Arcane);
    }
    return r;
}

bool GameSession::discardItem(int index, std::string* feedback) {
    if (!inventory_.isValidIndex(index)) {
        if (feedback) *feedback = inventory_.isEmpty() ? "Your pack is empty." : "Select a valid item first.";
        return false;
    }
    std::string name = itemInfo(inventory_.at(index).type).name;
    inventory_.remove(index);
    if (feedback) *feedback = "Discarded " + name + ".";
    log("Discarded " + name + ".", LogKind::Info);
    return true;
}

bool GameSession::lineOfSight(int x0, int y0, int x1, int y1) const {
    // Bresenham line; intermediate wall tiles block sight.
    const Dungeon& d = level_.dungeon;
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (!(x == x1 && y == y1)) {
        if (!(x == x0 && y == y0) && !d.isWalkable(x, y)) return false;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    return true;
}

void GameSession::updateVisibility() {
    const Dungeon& d = level_.dungeon;
    for (int i = 0; i < visible_.size(); ++i) visible_[i] = false;
    int px = d.xOf(player_.tile);
    int py = d.yOf(player_.tile);
    const int r = kSightRadius;
    for (int y = py - r; y <= py + r; ++y) {
        for (int x = px - r; x <= px + r; ++x) {
            if (!d.inBounds(x, y)) continue;
            int ddx = x - px, ddy = y - py;
            if (ddx * ddx + ddy * ddy > r * r + r) continue;
            if (!lineOfSight(px, py, x, y)) continue;
            int idx = d.index(x, y);
            visible_[idx] = true;
            explored_[idx] = true;
        }
    }
}
