// ============================================================================
//  Dungeonbound: The Lost Realms
//  A turn-based dungeon crawler built on hand-written data structures.
//
//  Usage:
//    dungeonbound                 play the game
//    dungeonbound --shots <dir>   render every screen to PNG files and exit
//
//  The whole program lives in this one file. It is split into numbered
//  sections; each one only uses the sections above it.
//
//   PART A - GAME LOGIC (no graphics, so the tests can build it on its own)
//     1. DynamicArray      growable array, our replacement for std::vector
//     2. BinaryHeap        priority queue used by A* and by the battle turn order
//     3. Graph             adjacency-list graph of the dungeon tiles
//     4. Rng               seeded random numbers (same seed = same dungeon)
//     5. Items and entities
//     6. Inventory
//     7. Dungeon           the tile grid
//     8. DungeonGenerator  maze + rooms + loops, then entity placement
//     9. Pathfinder        A* search
//    10. BattleSystem      turn-based combat
//    11. GameSession       all the rules for one run
//
//   PART B - SFML GAME (skipped when DUNGEONBOUND_NO_GAME is defined)
//    12. Theme             colours and layout sizes
//    13. Fonts
//    14. Drawing helpers   shapes, text, bars, buttons, icons
//    15. Input             keys -> commands
//    16. Map renderer      tiles, fog of war, entities, debug overlays
//    17. HUD               top bar, sidebar cards, journal
//    18. Inventory panel
//    19. Battle screen
//    20. Menus and full-screen pages
//    21. Game              window, screen state machine, main loop
//    22. main()
//
//   PART C - TESTS (only when DUNGEONBOUND_TESTS is defined; never in the game)
//    23. Test framework    TEST / CHECK macros and the test runner
//    24. Tests             35 tests of Part A
//    25. Test main()
//
//  Builds (see CMakeLists.txt):
//    dungeonbound.exe   = Part A + Part B   (no flags)
//    dungeon_tests.exe  = Part A + Part C   (DUNGEONBOUND_NO_GAME + DUNGEONBOUND_TESTS)
// ============================================================================

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

// ############################################################################
//  PART A - GAME LOGIC
// ############################################################################

// ============================================================================
//  1. DynamicArray<T>
//  A growable array that owns a buffer allocated with new[]. Used for every
//  "list of things" in the project. Indexing is bounds-checked.
// ============================================================================
template <typename T>
class DynamicArray {
public:
    DynamicArray() = default;

    explicit DynamicArray(int count, const T& value = T()) {
        if (count < 0) throw std::invalid_argument("DynamicArray: negative size");
        reserve(count);
        for (int i = 0; i < count; ++i) data_[i] = value;
        size_ = count;
    }

    DynamicArray(const DynamicArray& other) {
        reserve(other.size_);
        for (int i = 0; i < other.size_; ++i) data_[i] = other.data_[i];
        size_ = other.size_;
    }

    DynamicArray(DynamicArray&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            // Copy first, then swap: if the copy throws, *this is unchanged.
            DynamicArray copy(other);
            swap(copy);
        }
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~DynamicArray() { delete[] data_; }

    void swap(DynamicArray& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    void reserve(int capacity) {
        if (capacity <= capacity_) return;
        T* bigger = new T[capacity];
        for (int i = 0; i < size_; ++i) bigger[i] = std::move(data_[i]);
        delete[] data_;
        data_ = bigger;
        capacity_ = capacity;
    }

    void resize(int count, const T& value = T()) {
        if (count < 0) throw std::invalid_argument("DynamicArray: negative size");
        reserve(count);
        for (int i = size_; i < count; ++i) data_[i] = value;
        size_ = count;
    }

    void push_back(const T& value) {
        // Copy first: `value` may point into our own buffer, which grow() frees.
        T copy = value;
        if (size_ == capacity_) grow();
        data_[size_] = std::move(copy);
        ++size_;
    }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("DynamicArray::pop_back on empty array");
        --size_;
        data_[size_] = T();
    }

    void insertAt(int index, const T& value) {
        if (index < 0 || index > size_) throw std::out_of_range("DynamicArray::insertAt");
        T copy = value;
        if (size_ == capacity_) grow();
        // Shift everything after `index` one place to the right.
        for (int i = size_; i > index; --i) data_[i] = std::move(data_[i - 1]);
        data_[index] = std::move(copy);
        ++size_;
    }

    void removeAt(int index) {
        if (index < 0 || index >= size_) throw std::out_of_range("DynamicArray::removeAt");
        // Shift everything after `index` one place to the left.
        for (int i = index; i < size_ - 1; ++i) data_[i] = std::move(data_[i + 1]);
        --size_;
        data_[size_] = T();
    }

    void clear() { size_ = 0; }

    void reverse() {
        int i = 0;
        int j = size_ - 1;
        while (i < j) {
            std::swap(data_[i], data_[j]);
            ++i;
            --j;
        }
    }

    T& operator[](int index) {
        if (index < 0 || index >= size_) throw std::out_of_range("DynamicArray index out of range");
        return data_[index];
    }
    const T& operator[](int index) const {
        if (index < 0 || index >= size_) throw std::out_of_range("DynamicArray index out of range");
        return data_[index];
    }

    T& back() { return (*this)[size_ - 1]; }
    const T& back() const { return (*this)[size_ - 1]; }
    T& front() { return (*this)[0]; }
    const T& front() const { return (*this)[0]; }

    int size() const { return size_; }
    int capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    // Plain pointers are enough for range-based for loops.
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    // Doubling keeps push_back O(1) on average.
    void grow() { reserve(capacity_ == 0 ? 8 : capacity_ * 2); }

    T* data_ = nullptr;
    int size_ = 0;
    int capacity_ = 0;
};

// ============================================================================
//  2. BinaryHeap<T, Compare>
//  Array-backed binary heap. Compare(a, b) returns true when `a` must come
//  out before `b`, so a "less than" comparator gives a min-heap.
//  push / pop are O(log n), top is O(1).
//  For element i: parent = (i - 1) / 2, children = 2i + 1 and 2i + 2.
// ============================================================================
template <typename T, typename Compare>
class BinaryHeap {
public:
    explicit BinaryHeap(Compare compare = Compare()) : compare_(compare) {}

    void push(const T& value) {
        data_.push_back(value);
        siftUp(data_.size() - 1);
    }

    const T& top() const {
        if (data_.empty()) throw std::out_of_range("BinaryHeap::top on empty heap");
        return data_[0];
    }

    T pop() {
        if (data_.empty()) throw std::out_of_range("BinaryHeap::pop on empty heap");
        T result = data_[0];
        // Move the last element to the root, then let it sink to its place.
        data_[0] = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return result;
    }

    // Like pop(), but returns false instead of throwing when empty.
    bool tryPop(T& out) {
        if (data_.empty()) return false;
        out = pop();
        return true;
    }

    int size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }

    // The backing array in heap order (index 0 is the root). Read-only.
    const DynamicArray<T>& raw() const { return data_; }

    // Checks the heap property for every parent/child pair (used by tests).
    bool isValid() const {
        for (int i = 1; i < data_.size(); ++i) {
            int parent = (i - 1) / 2;
            if (compare_(data_[i], data_[parent])) return false;
        }
        return true;
    }

private:
    void siftUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (!compare_(data_[index], data_[parent])) break;
            std::swap(data_[index], data_[parent]);
            index = parent;
        }
    }

    void siftDown(int index) {
        const int n = data_.size();
        while (true) {
            int left = 2 * index + 1;
            int right = left + 1;
            int best = index;
            if (left < n && compare_(data_[left], data_[best])) best = left;
            if (right < n && compare_(data_[right], data_[best])) best = right;
            if (best == index) break;
            std::swap(data_[index], data_[best]);
            index = best;
        }
    }

    DynamicArray<T> data_;
    Compare compare_;
};

// ============================================================================
//  3. Graph
//  Undirected weighted graph. Each vertex has a singly linked list of edges.
//  In the dungeon every tile is a vertex, and two side-by-side walkable
//  tiles share an edge of weight 1. Walls are vertices with no edges.
// ============================================================================
struct GraphEdge {
    int to = -1;
    int weight = 1;
    GraphEdge* next = nullptr;
};

class Graph {
public:
    explicit Graph(int vertexCount = 0) { reset(vertexCount); }

    Graph(const Graph& other) { copyFrom(other); }

    Graph(Graph&& other) noexcept : heads_(std::move(other.heads_)), edgeCount_(other.edgeCount_) {
        other.edgeCount_ = 0;
    }

    Graph& operator=(const Graph& other) {
        if (this != &other) {
            freeAll();
            copyFrom(other);
        }
        return *this;
    }

    Graph& operator=(Graph&& other) noexcept {
        if (this != &other) {
            freeAll();
            heads_ = std::move(other.heads_);
            edgeCount_ = other.edgeCount_;
            other.edgeCount_ = 0;
        }
        return *this;
    }

    ~Graph() { freeAll(); }

    // Throws away every edge and leaves `vertexCount` isolated vertices.
    void reset(int vertexCount) {
        freeAll();  // leaves heads_ empty
        if (vertexCount > 0) heads_.resize(vertexCount, nullptr);
    }

    // Adds a new isolated vertex and returns its id.
    int addVertex() {
        heads_.push_back(nullptr);
        return heads_.size() - 1;
    }

    // Adds an undirected edge. Rejects bad ids, self loops and duplicates.
    bool addEdge(int u, int v, int weight = 1) {
        if (!isValidVertex(u) || !isValidVertex(v)) return false;
        if (u == v || weight < 0) return false;
        if (hasEdge(u, v)) return false;
        // An undirected edge is stored once in each endpoint's list.
        addDirected(u, v, weight);
        addDirected(v, u, weight);
        ++edgeCount_;
        return true;
    }

    bool removeEdge(int u, int v) {
        if (!isValidVertex(u) || !isValidVertex(v)) return false;
        if (!removeDirected(u, v)) return false;
        removeDirected(v, u);
        --edgeCount_;
        return true;
    }

    bool hasEdge(int u, int v) const {
        if (!isValidVertex(u) || !isValidVertex(v)) return false;
        for (const GraphEdge* e = heads_[u]; e != nullptr; e = e->next) {
            if (e->to == v) return true;
        }
        return false;
    }

    bool isValidVertex(int v) const { return v >= 0 && v < heads_.size(); }
    int vertexCount() const { return heads_.size(); }
    int edgeCount() const { return edgeCount_; }  // undirected edges

    int degree(int v) const {
        if (!isValidVertex(v)) return 0;
        int count = 0;
        for (const GraphEdge* e = heads_[v]; e != nullptr; e = e->next) ++count;
        return count;
    }

    // First edge in v's list (nullptr if none or if v is invalid).
    const GraphEdge* edges(int v) const {
        if (!isValidVertex(v)) return nullptr;
        return heads_[v];
    }

    DynamicArray<int> neighbors(int v) const {
        DynamicArray<int> result;
        for (const GraphEdge* e = edges(v); e != nullptr; e = e->next) result.push_back(e->to);
        return result;
    }

    // Breadth-first search. Returns the number of steps from `source` to
    // every vertex, or -1 for vertices that cannot be reached.
    DynamicArray<int> bfsDistances(int source) const {
        DynamicArray<int> dist(vertexCount(), -1);
        if (!isValidVertex(source)) return dist;

        // A FIFO queue on top of an array: `head` moves forward instead of
        // erasing from the front, which would be O(n) per pop.
        DynamicArray<int> queue;
        queue.reserve(vertexCount());
        int head = 0;
        dist[source] = 0;
        queue.push_back(source);
        while (head < queue.size()) {
            int u = queue[head];
            ++head;
            for (const GraphEdge* e = heads_[u]; e != nullptr; e = e->next) {
                if (dist[e->to] == -1) {
                    dist[e->to] = dist[u] + 1;
                    queue.push_back(e->to);
                }
            }
        }
        return dist;
    }

    bool isConnected(int a, int b) const {
        if (!isValidVertex(a) || !isValidVertex(b)) return false;
        if (a == b) return true;
        return bfsDistances(a)[b] != -1;
    }

    // How many vertices (including `source`) can be reached from `source`.
    int reachableCount(int source) const {
        if (!isValidVertex(source)) return 0;
        DynamicArray<int> dist = bfsDistances(source);
        int count = 0;
        for (int d : dist) {
            if (d != -1) ++count;
        }
        return count;
    }

private:
    void addDirected(int u, int v, int weight) {
        // Insert at the head of u's list: O(1).
        GraphEdge* edge = new GraphEdge{v, weight, heads_[u]};
        heads_[u] = edge;
    }

    bool removeDirected(int u, int v) {
        GraphEdge* prev = nullptr;
        GraphEdge* e = heads_[u];
        while (e != nullptr) {
            if (e->to == v) {
                if (prev != nullptr) prev->next = e->next;
                else heads_[u] = e->next;
                delete e;
                return true;
            }
            prev = e;
            e = e->next;
        }
        return false;
    }

    void freeAll() {
        for (int i = 0; i < heads_.size(); ++i) {
            GraphEdge* e = heads_[i];
            while (e != nullptr) {
                GraphEdge* next = e->next;
                delete e;
                e = next;
            }
            heads_[i] = nullptr;
        }
        heads_.clear();
        edgeCount_ = 0;
    }

    void copyFrom(const Graph& other) {
        heads_.resize(other.heads_.size(), nullptr);
        for (int i = 0; i < other.heads_.size(); ++i) {
            // Append at the tail so the copy keeps the same edge order.
            GraphEdge* tail = nullptr;
            for (const GraphEdge* e = other.heads_[i]; e != nullptr; e = e->next) {
                GraphEdge* copy = new GraphEdge{e->to, e->weight, nullptr};
                if (tail != nullptr) tail->next = copy;
                else heads_[i] = copy;
                tail = copy;
            }
        }
        edgeCount_ = other.edgeCount_;
    }

    DynamicArray<GraphEdge*> heads_;
    int edgeCount_ = 0;
};

// ============================================================================
//  4. Rng
//  std::mt19937 is fully defined by the C++ standard and the range mapping
//  below is our own, so one seed gives the same dungeon on every compiler.
// ============================================================================
class Rng {
public:
    explicit Rng(std::uint32_t seed = 1) : engine_(seed) {}

    void reseed(std::uint32_t seed) { engine_.seed(seed); }

    std::uint32_t next() { return static_cast<std::uint32_t>(engine_()); }

    // Whole number in [lo, hi], both ends included.
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        std::uint32_t span = static_cast<std::uint32_t>(hi - lo) + 1u;
        return lo + static_cast<int>(next() % span);
    }

    // Real number in [0, 1).
    double real() { return next() / 4294967296.0; }

    bool chance(double probability) { return real() < probability; }

private:
    std::mt19937 engine_;
};

// Manhattan distance between two tiles given as y * width + x.
int gridDistance(int a, int b, int gridWidth) {
    int dx = a % gridWidth - b % gridWidth;
    int dy = a / gridWidth - b / gridWidth;
    return std::abs(dx) + std::abs(dy);
}

// ============================================================================
//  5. Items and entities
// ============================================================================
enum class ItemType { HealthPotion, Elixir, FireBomb, StrengthTonic, IronSkin, SmokeBomb, Count };

struct ItemInfo {
    const char* name;
    const char* description;
    const char* effect;
    bool battleOnly;
};

const ItemInfo& itemInfo(ItemType type) {
    static const ItemInfo items[] = {
        {"Health Potion", "A ruby draught brewed by the realm's healers.", "Restores 35 HP", false},
        {"Elixir of Vigor", "Liquid sunlight, sealed in a gilded vial.", "Restores all HP", false},
        {"Fire Bomb", "A clay sphere packed with volatile embers.", "Deals 30 damage, ignores armour", true},
        {"Strength Tonic", "Bitter, but your arms feel like iron.", "Permanently +3 ATK", false},
        {"Ironskin Draught", "Your skin hardens like old bark.", "Permanently +2 DEF", false},
        {"Smoke Bomb", "A burst of grey smoke covers your retreat.", "Guaranteed escape (not vs. bosses)", true},
    };
    int i = static_cast<int>(type);
    if (i < 0 || i >= static_cast<int>(ItemType::Count)) i = 0;
    return items[i];
}

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

struct Stats {
    int maxHp = 1;
    int hp = 1;
    int attack = 1;
    int defense = 0;
    int speed = 1;  // higher speed = acts more often in battle
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

const EnemyInfo& enemyInfo(EnemyKind kind) {
    //  name        title                   maxHp hp atk def spd   xp  special          chance mult
    static const EnemyInfo enemies[] = {
        {"Slime",    "Gelatinous Lurker",    {30, 30, 7, 1, 7},   10, "Acid Splash",   0.15, 1.3},
        {"Goblin",   "Tunnel Cutthroat",     {34, 34, 9, 2, 13},  15, "Quick Stab",    0.20, 1.25},
        {"Skeleton", "Restless Sentinel",    {42, 42, 11, 3, 9},  22, "Marrow Strike", 0.18, 1.4},
        {"Orc",      "Brute of the Deep",    {58, 58, 13, 4, 8},  32, "Savage Swing",  0.22, 1.55},
        {"Warden",   "Keeper of the Portal", {95, 95, 14, 5, 9},  80, "Crushing Slam", 0.25, 1.7},
    };
    int i = static_cast<int>(kind);
    if (i < 0 || i >= static_cast<int>(EnemyKind::Count)) i = 0;
    return enemies[i];
}

struct Enemy {
    EnemyKind kind = EnemyKind::Slime;
    int tile = -1;
    Stats stats;
    bool alive = true;
    bool boss = false;
    int uid = 0;

    const char* name() const { return enemyInfo(kind).name; }
};

Enemy makeEnemy(EnemyKind kind, int tile, int uid) {
    Enemy e;
    e.kind = kind;
    e.tile = tile;
    e.stats = enemyInfo(kind).stats;
    e.boss = (kind == EnemyKind::Warden);
    e.uid = uid;
    return e;
}

struct Player {
    Stats stats;
    int tile = -1;
    int level = 1;
    int xp = 0;

    int xpToNext() const { return 25 + level * 20; }

    // Adds experience and returns how many levels were gained.
    int gainXp(int amount) {
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
};

Player makePlayer(int tile) {
    Player p;
    p.tile = tile;
    p.stats = {100, 100, 12, 4, 10};
    return p;
}

// ============================================================================
//  6. Inventory
// ============================================================================
class Inventory {
public:
    static constexpr int kDefaultCapacity = 8;

    explicit Inventory(int capacity = kDefaultCapacity) : capacity_(capacity < 1 ? 1 : capacity) {}

    // Returns false when the pack is full.
    bool add(const Item& item) {
        if (isFull()) return false;
        items_.push_back(item);
        return true;
    }

    // Returns false for an invalid index.
    bool remove(int index) {
        if (!isValidIndex(index)) return false;
        items_.removeAt(index);
        return true;
    }

    bool isValidIndex(int index) const { return index >= 0 && index < items_.size(); }
    const Item& at(int index) const { return items_[index]; }

    int size() const { return items_.size(); }
    int capacity() const { return capacity_; }
    bool isFull() const { return items_.size() >= capacity_; }
    bool isEmpty() const { return items_.empty(); }
    void clear() { items_.clear(); }

    int countOf(ItemType type) const {
        int count = 0;
        for (const Item& item : items_) {
            if (item.type == type) ++count;
        }
        return count;
    }

private:
    DynamicArray<Item> items_;
    int capacity_;
};

struct ItemUseResult {
    bool success = false;   // the effect was applied
    bool consumed = false;  // remove the item from the pack
    bool escape = false;    // the battle should end as an escape
    int healed = 0;
    int damage = 0;
    std::string message;
};

// Heals up to `amount` without going over max HP; returns the HP gained.
int healBy(Stats& s, int amount) {
    int before = s.hp;
    s.hp += amount;
    if (s.hp > s.maxHp) s.hp = s.maxHp;
    return s.hp - before;
}

// Applies an item's effect. `enemy` is nullptr outside of battle.
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
            int amount = 35;
            if (type == ItemType::Elixir) amount = player.stats.maxHp;
            r.healed = healBy(player.stats, amount);
            r.message = "You drink the " + std::string(info.name) + " and recover " + std::to_string(r.healed) + " HP.";
            break;
        }
        case ItemType::FireBomb:
            r.damage = 30;
            enemy->hp -= r.damage;
            if (enemy->hp < 0) enemy->hp = 0;
            r.message = "The Fire Bomb bursts for " + std::to_string(r.damage) + " damage!";
            break;
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

// ============================================================================
//  7. Dungeon
//  A grid of tiles plus its graph. Tile (x, y) has index y * width + x,
//  which is also its vertex id in the graph.
// ============================================================================
enum class Tile : unsigned char { Wall, Floor, Exit };

struct Room {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }

    // True if the two rooms overlap once each is grown by `margin` tiles.
    bool overlapsWithMargin(const Room& o, int margin) const {
        bool overlapX = x - margin < o.x + o.w && x + w + margin > o.x;
        bool overlapY = y - margin < o.y + o.h && y + h + margin > o.y;
        return overlapX && overlapY;
    }
};

class Dungeon {
public:
    Dungeon() = default;
    Dungeon(int width, int height)
        : width_(width < 1 ? 1 : width), height_(height < 1 ? 1 : height), tiles_(width_ * height_, Tile::Wall) {}

    int width() const { return width_; }
    int height() const { return height_; }
    int tileCount() const { return width_ * height_; }

    int index(int x, int y) const { return y * width_ + x; }
    int xOf(int idx) const { return idx % width_; }
    int yOf(int idx) const { return idx / width_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    bool isValidIndex(int idx) const { return idx >= 0 && idx < tileCount(); }

    // Anything outside the grid counts as wall.
    Tile tile(int x, int y) const { return inBounds(x, y) ? tiles_[index(x, y)] : Tile::Wall; }
    Tile tileAt(int idx) const { return isValidIndex(idx) ? tiles_[idx] : Tile::Wall; }
    void setTile(int x, int y, Tile t) {
        if (inBounds(x, y)) tiles_[index(x, y)] = t;
    }

    bool isWalkable(int x, int y) const { return tile(x, y) != Tile::Wall; }
    bool isWalkableIdx(int idx) const { return tileAt(idx) != Tile::Wall; }

    int walkableCount() const {
        int count = 0;
        for (Tile t : tiles_) {
            if (t != Tile::Wall) ++count;
        }
        return count;
    }

    // Rebuilds the graph from the current tiles.
    void buildGraph() {
        graph_.reset(tileCount());
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                if (!isWalkable(x, y)) continue;
                // Only look right and down, so each edge is added once.
                if (isWalkable(x + 1, y)) graph_.addEdge(index(x, y), index(x + 1, y), 1);
                if (isWalkable(x, y + 1)) graph_.addEdge(index(x, y), index(x, y + 1), 1);
            }
        }
    }
    const Graph& graph() const { return graph_; }

    // Index of the room containing (x, y), or -1 if it is not in a room.
    int roomAt(int x, int y) const {
        for (int i = 0; i < rooms_.size(); ++i) {
            if (rooms_[i].contains(x, y)) return i;
        }
        return -1;
    }
    DynamicArray<Room>& rooms() { return rooms_; }
    const DynamicArray<Room>& rooms() const { return rooms_; }

    int start = -1;
    int exit = -1;
    std::uint32_t seed = 0;

private:
    int width_ = 0;
    int height_ = 0;
    DynamicArray<Tile> tiles_;
    DynamicArray<Room> rooms_;
    Graph graph_;
};

// ============================================================================
//  8. DungeonGenerator
//   1. Recursive backtracking carves a perfect maze through every odd cell.
//   2. Rooms that do not touch each other are carved on top of the maze.
//   3. A few walls are knocked out so there are loops (more than one route).
//   4. Short dead ends are filled back in.
//   5. The graph is built; the exit goes on the tile farthest from the start.
//   6. Enemies and items are placed on reachable, separate tiles.
//  If a layout fails validation, it is thrown away and tried with a new seed.
// ============================================================================
struct GeneratorConfig {
    int width = 37;   // forced odd, at least 11
    int height = 23;  // forced odd, at least 11
    int roomAttempts = 60;
    int maxRooms = 7;
    int loopCount = 14;
    int prunePasses = 4;
    int enemyCount = 8;  // not counting the Warden
    int itemCount = 9;
};

struct GeneratedLevel {
    Dungeon dungeon;
    DynamicArray<Enemy> enemies;
    DynamicArray<FloorItem> items;
    int attempts = 0;  // more than 1 means a layout was rejected and redone
};

// Up, right, down, left.
const int kStepX[4] = {0, 1, 0, -1};
const int kStepY[4] = {-1, 0, 1, 0};

// Fisher-Yates shuffle.
template <typename T>
void shuffle(DynamicArray<T>& arr, Rng& rng) {
    for (int i = arr.size() - 1; i > 0; --i) {
        int j = rng.range(0, i);
        T tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

class DungeonGenerator {
public:
    explicit DungeonGenerator(GeneratorConfig config = GeneratorConfig()) : config_(config) {
        if (config_.width < 11) config_.width = 11;
        if (config_.height < 11) config_.height = 11;
        // The maze walks on odd cells, so both sizes must be odd.
        if (config_.width % 2 == 0) config_.width += 1;
        if (config_.height % 2 == 0) config_.height += 1;
    }

    GeneratedLevel generate(std::uint32_t seed) const {
        GeneratedLevel level;
        // Validate-and-regenerate: each retry uses a different derived seed.
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

    // Full structural check; used after generation and by the tests.
    static bool validate(const GeneratedLevel& level, std::string* reason = nullptr) {
        auto fail = [reason](const char* why) {
            if (reason) *reason = why;
            return false;
        };
        const Dungeon& d = level.dungeon;
        if (d.width() < 3 || d.height() < 3) return fail("dungeon too small");
        if (!d.isValidIndex(d.start) || !d.isWalkableIdx(d.start)) return fail("invalid start");
        if (!d.isValidIndex(d.exit) || d.tileAt(d.exit) != Tile::Exit) return fail("invalid exit");
        if (d.start == d.exit) return fail("start equals exit");

        // The outer border must be solid wall.
        for (int x = 0; x < d.width(); ++x) {
            if (d.isWalkable(x, 0) || d.isWalkable(x, d.height() - 1)) return fail("open border");
        }
        for (int y = 0; y < d.height(); ++y) {
            if (d.isWalkable(0, y) || d.isWalkable(d.width() - 1, y)) return fail("open border");
        }

        // Rooms must be inside the border and must not overlap.
        const DynamicArray<Room>& rooms = d.rooms();
        for (int i = 0; i < rooms.size(); ++i) {
            const Room& r = rooms[i];
            if (r.x < 1 || r.y < 1 || r.x + r.w > d.width() - 1 || r.y + r.h > d.height() - 1) return fail("room out of bounds");
            for (int j = i + 1; j < rooms.size(); ++j) {
                if (r.overlapsWithMargin(rooms[j], 0)) return fail("rooms overlap");
            }
        }

        // The graph must match the grid, and every floor tile must be reachable.
        if (d.graph().vertexCount() != d.tileCount()) return fail("graph size mismatch");
        if (d.graph().reachableCount(d.start) != d.walkableCount()) return fail("dungeon not fully connected");
        if (!d.graph().isConnected(d.start, d.exit)) return fail("exit unreachable");

        // Every entity must be on its own walkable tile.
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

    const GeneratorConfig& config() const { return config_; }

private:
    bool tryGenerate(std::uint32_t seed, GeneratedLevel& out) const {
        Rng rng(seed);
        out = GeneratedLevel();
        out.dungeon = Dungeon(config_.width, config_.height);
        Dungeon& d = out.dungeon;

        // Step 1: maze, starting from a random odd cell.
        DynamicArray<bool> visited(d.tileCount(), false);
        int cellsX = (d.width() - 1) / 2;
        int cellsY = (d.height() - 1) / 2;
        int startX = 1 + 2 * rng.range(0, cellsX - 1);
        int startY = 1 + 2 * rng.range(0, cellsY - 1);
        carveMaze(d, visited, startX, startY, rng);

        // Steps 2-4.
        placeRooms(d, rng);
        addLoops(d, rng);
        pruneDeadEnds(d);

        // Step 5: graph, then start in the middle of the first room.
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

        // The exit is the tile with the largest BFS distance from the start.
        DynamicArray<int> dist = d.graph().bfsDistances(d.start);
        int farthest = d.start;
        for (int i = 0; i < dist.size(); ++i) {
            if (dist[i] > dist[farthest]) farthest = i;
        }
        if (farthest == d.start) return false;
        d.exit = farthest;
        d.setTile(d.xOf(farthest), d.yOf(farthest), Tile::Exit);

        // Every floor tile must be reachable from the start.
        if (d.graph().reachableCount(d.start) != d.walkableCount()) return false;

        // Step 6.
        placeEntities(out, dist, rng);
        return true;
    }

    // Recursive backtracker: open this cell, then visit the unvisited cells two
    // tiles away in random order, opening the wall in between. When no
    // neighbour is left the call returns - that return is the "backtrack".
    void carveMaze(Dungeon& d, DynamicArray<bool>& visited, int x, int y, Rng& rng) const {
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
            int nx = x + kStepX[dir] * 2;
            int ny = y + kStepY[dir] * 2;
            // Never carve into the outer border.
            if (nx < 1 || ny < 1 || nx > d.width() - 2 || ny > d.height() - 2) continue;
            if (visited[d.index(nx, ny)]) continue;
            d.setTile(x + kStepX[dir], y + kStepY[dir], Tile::Floor);
            carveMaze(d, visited, nx, ny, rng);
        }
    }

    void placeRooms(Dungeon& d, Rng& rng) const {
        for (int attempt = 0; attempt < config_.roomAttempts; ++attempt) {
            if (d.rooms().size() >= config_.maxRooms) break;
            Room r;
            r.w = 2 * rng.range(1, 3) + 1;  // 3, 5 or 7
            r.h = 2 * rng.range(1, 2) + 1;  // 3 or 5
            int maxX = d.width() - 1 - r.w;
            int maxY = d.height() - 1 - r.h;
            if (maxX < 1 || maxY < 1) continue;
            // Odd positions line rooms up with the maze cells.
            r.x = 1 + 2 * rng.range(0, (maxX - 1) / 2);
            r.y = 1 + 2 * rng.range(0, (maxY - 1) / 2);

            // Rooms never touch each other (1-tile gap).
            bool clash = false;
            for (const Room& other : d.rooms()) {
                if (r.overlapsWithMargin(other, 1)) {
                    clash = true;
                    break;
                }
            }
            if (clash) continue;

            for (int y = r.y; y < r.y + r.h; ++y) {
                for (int x = r.x; x < r.x + r.w; ++x) d.setTile(x, y, Tile::Floor);
            }
            d.rooms().push_back(r);
        }
    }

    // A wall is a loop candidate if it separates two floor tiles in a
    // straight line (left/right or up/down) and is solid on the other axis.
    static bool isThinWall(const Dungeon& d, int x, int y) {
        bool left = d.isWalkable(x - 1, y), right = d.isWalkable(x + 1, y);
        bool up = d.isWalkable(x, y - 1), down = d.isWalkable(x, y + 1);
        bool horizontal = left && right && !up && !down;
        bool vertical = up && down && !left && !right;
        return horizontal || vertical;
    }

    void addLoops(Dungeon& d, Rng& rng) const {
        DynamicArray<int> candidates;
        for (int y = 1; y < d.height() - 1; ++y) {
            for (int x = 1; x < d.width() - 1; ++x) {
                if (d.isWalkable(x, y)) continue;
                if (isThinWall(d, x, y)) candidates.push_back(d.index(x, y));
            }
        }
        shuffle(candidates, rng);
        int opened = 0;
        for (int i = 0; i < candidates.size() && opened < config_.loopCount; ++i) {
            int x = d.xOf(candidates[i]);
            int y = d.yOf(candidates[i]);
            // Check again: opening an earlier candidate may have changed this one.
            if (!isThinWall(d, x, y)) continue;
            d.setTile(x, y, Tile::Floor);
            ++opened;
        }
    }

    void pruneDeadEnds(Dungeon& d) const {
        for (int pass = 0; pass < config_.prunePasses; ++pass) {
            DynamicArray<int> deadEnds;
            for (int y = 1; y < d.height() - 1; ++y) {
                for (int x = 1; x < d.width() - 1; ++x) {
                    if (!d.isWalkable(x, y) || d.roomAt(x, y) != -1) continue;
                    int openSides = 0;
                    for (int k = 0; k < 4; ++k) {
                        if (d.isWalkable(x + kStepX[k], y + kStepY[k])) ++openSides;
                    }
                    if (openSides == 1) deadEnds.push_back(d.index(x, y));
                }
            }
            if (deadEnds.empty()) break;
            // A tile with only one open side can never be the only link
            // between two areas, so filling it keeps the dungeon connected.
            for (int idx : deadEnds) d.setTile(d.xOf(idx), d.yOf(idx), Tile::Wall);
        }
    }

    void placeEntities(GeneratedLevel& level, const DynamicArray<int>& dist, Rng& rng) const {
        Dungeon& d = level.dungeon;
        int maxDist = dist[d.exit];
        int nextUid = 1;

        DynamicArray<bool> occupied(d.tileCount(), false);
        occupied[d.start] = true;
        occupied[d.exit] = true;

        // The Warden stands on the tile just before the exit on a shortest path.
        for (const GraphEdge* e = d.graph().edges(d.exit); e != nullptr; e = e->next) {
            if (dist[e->to] == maxDist - 1 && e->to != d.start) {
                level.enemies.push_back(makeEnemy(EnemyKind::Warden, e->to, nextUid));
                ++nextUid;
                occupied[e->to] = true;
                break;
            }
        }

        // Regular enemies: at least 6 steps from the start and spread out.
        DynamicArray<int> candidates;
        for (int i = 0; i < d.tileCount(); ++i) {
            if (d.tileAt(i) == Tile::Floor && dist[i] >= 6 && !occupied[i]) candidates.push_back(i);
        }
        shuffle(candidates, rng);
        for (int i = 0; i < candidates.size() && level.enemies.size() < config_.enemyCount + 1; ++i) {
            int tile = candidates[i];
            bool tooClose = false;
            for (const Enemy& other : level.enemies) {
                if (gridDistance(tile, other.tile, d.width()) < 4) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            // Deeper into the dungeon means tougher enemies.
            double depth = 0.0;
            if (maxDist > 0) depth = static_cast<double>(dist[tile]) / maxDist;
            EnemyKind kind;
            if (depth < 0.35) kind = rng.chance(0.6) ? EnemyKind::Slime : EnemyKind::Goblin;
            else if (depth < 0.7) kind = rng.chance(0.5) ? EnemyKind::Goblin : EnemyKind::Skeleton;
            else kind = rng.chance(0.55) ? EnemyKind::Skeleton : EnemyKind::Orc;

            level.enemies.push_back(makeEnemy(kind, tile, nextUid));
            ++nextUid;
            occupied[tile] = true;
        }

        // Items: not on an enemy, and spread out.
        candidates.clear();
        for (int i = 0; i < d.tileCount(); ++i) {
            if (d.tileAt(i) == Tile::Floor && dist[i] >= 2 && !occupied[i]) candidates.push_back(i);
        }
        shuffle(candidates, rng);
        for (int i = 0; i < candidates.size() && level.items.size() < config_.itemCount; ++i) {
            int tile = candidates[i];
            bool tooClose = false;
            for (const FloorItem& other : level.items) {
                if (gridDistance(tile, other.tile, d.width()) < 3) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            ItemType type;
            if (level.items.size() < 2) {
                type = ItemType::HealthPotion;  // there are always at least two potions
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
            fi.item = Item{type, nextUid};
            ++nextUid;
            fi.tile = tile;
            level.items.push_back(fi);
            occupied[tile] = true;
        }
    }

    GeneratorConfig config_;
};

// ============================================================================
//  9. Pathfinder (A*)
//  f = g + h, where g is the steps taken so far and h is the Manhattan
//  distance to the goal. Manhattan never overestimates on a 4-way grid, so
//  the first time the goal is taken off the heap its path is the shortest.
// ============================================================================
struct PathResult {
    bool found = false;
    DynamicArray<int> path;      // start .. goal inclusive; empty when not found
    DynamicArray<int> expanded;  // order tiles were closed, for the debug overlay
    int cost = -1;
    int heapPushes = 0;
    double millis = 0.0;
};

struct OpenNode {
    int f = 0;
    int h = 0;
    int id = -1;
};

// Lowest f first. Ties go to the node nearer the goal, then the lower id,
// so the result never depends on heap order.
struct OpenNodeCompare {
    bool operator()(const OpenNode& a, const OpenNode& b) const {
        if (a.f != b.f) return a.f < b.f;
        if (a.h != b.h) return a.h < b.h;
        return a.id < b.id;
    }
};

class Pathfinder {
public:
    static int manhattan(int a, int b, int gridWidth) { return gridDistance(a, b, gridWidth); }

    // `passable` (optional) marks the tiles the search may enter; the start is
    // always allowed. Pass nullptr to search the whole graph.
    static PathResult findPath(const Graph& graph, int gridWidth, int start, int goal,
                               const DynamicArray<bool>* passable = nullptr) {
        const auto startTime = std::chrono::steady_clock::now();
        PathResult result = search(graph, gridWidth, start, goal, passable);
        const auto endTime = std::chrono::steady_clock::now();
        result.millis = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return result;
    }

private:
    static PathResult search(const Graph& graph, int gridWidth, int start, int goal, const DynamicArray<bool>* passable) {
        PathResult result;
        if (gridWidth <= 0 || !graph.isValidVertex(start) || !graph.isValidVertex(goal)) return result;
        if (start == goal) {
            result.found = true;
            result.cost = 0;
            result.path.push_back(start);
            result.expanded.push_back(start);
            return result;
        }
        if (passable && !(*passable)[goal]) return result;
        if (graph.degree(goal) == 0) return result;  // a wall or an isolated tile

        const int infinity = 1 << 29;
        const int n = graph.vertexCount();
        DynamicArray<int> gScore(n, infinity);
        DynamicArray<int> parent(n, -1);
        DynamicArray<bool> closed(n, false);

        BinaryHeap<OpenNode, OpenNodeCompare> open;
        gScore[start] = 0;
        int h0 = manhattan(start, goal, gridWidth);
        open.push({h0, h0, start});
        result.heapPushes = 1;

        OpenNode current;
        while (open.tryPop(current)) {
            int u = current.id;
            // A tile can be pushed more than once; skip the older, worse copies.
            if (closed[u]) continue;
            closed[u] = true;
            result.expanded.push_back(u);

            if (u == goal) {
                result.found = true;
                result.cost = gScore[goal];
                // Walk the parent links back from the goal, then flip the order.
                for (int v = goal; v != -1; v = parent[v]) result.path.push_back(v);
                result.path.reverse();
                return result;
            }

            for (const GraphEdge* e = graph.edges(u); e != nullptr; e = e->next) {
                int v = e->to;
                if (closed[v]) continue;
                if (passable && !(*passable)[v]) continue;
                int tentative = gScore[u] + e->weight;
                if (tentative < gScore[v]) {
                    gScore[v] = tentative;
                    parent[v] = u;
                    int h = manhattan(v, goal, gridWidth);
                    open.push({tentative + h, h, v});
                    ++result.heapPushes;
                }
            }
        }
        return result;  // the open set ran out: there is no path
    }
};

// ============================================================================
//  10. BattleSystem
//  Each fighter acts every 100 / SPD time units. The turn queue is a min-heap
//  keyed on each fighter's next action time, so faster fighters come up more
//  often. On a tie, whoever was scheduled first goes first.
// ============================================================================
enum class BattleAction { Attack, Defend, UseItem, Escape };
enum class BattlePhase { Idle, PlayerTurn, EnemyTurn, Won, Lost, Escaped };
enum class Actor { Player = 0, Enemy = 1 };

enum class LogKind { Info, PlayerHit, EnemyHit, Heal, Warning, Success, Danger, Arcane };

struct LogLine {
    std::string text;
    LogKind kind = LogKind::Info;
};

struct TurnEvent {
    double time = 0.0;
    int seq = 0;
    Actor actor = Actor::Player;
};

struct TurnEventCompare {
    bool operator()(const TurnEvent& a, const TurnEvent& b) const {
        if (a.time != b.time) return a.time < b.time;
        return a.seq < b.seq;
    }
};

// Tells the UI what to animate (damage numbers, flashes).
struct BattleFx {
    enum class Type { Damage, Heal, Guard, Miss, Buff };
    Type type = Type::Damage;
    Actor target = Actor::Enemy;
    int amount = 0;
    bool critical = false;
};

struct ActionResult {
    bool accepted = false;  // false = rejected, and the turn is NOT used up
    std::string feedback;
};

class BattleSystem {
public:
    using TurnQueue = BinaryHeap<TurnEvent, TurnEventCompare>;

    void begin(Player* player, Enemy* enemy, Inventory* inventory, Rng* rng) {
        player_ = player;
        enemy_ = enemy;
        inventory_ = inventory;
        rng_ = rng;
        queue_.clear();
        log_.clear();
        fx_.clear();
        now_ = 0.0;
        seq_ = 0;
        turnsTaken_ = 0;
        playerDefending_ = false;
        enemyDefending_ = false;
        damageDealt_ = 0;
        damageTaken_ = 0;
        itemsUsed_ = 0;

        const EnemyInfo& info = enemyInfo(enemy_->kind);
        addLog(std::string(info.name) + ", " + info.title + ", blocks your way!", enemy_->boss ? LogKind::Danger : LogKind::Warning);

        // Each fighter's first turn comes one interval after time 0.
        schedule(Actor::Player, interval(player_->stats.speed));
        schedule(Actor::Enemy, interval(enemy_->stats.speed));
        advance();
    }

    BattlePhase phase() const { return phase_; }
    bool isOver() const { return phase_ == BattlePhase::Won || phase_ == BattlePhase::Lost || phase_ == BattlePhase::Escaped; }
    bool active() const { return phase_ != BattlePhase::Idle; }

    ActionResult playerAct(BattleAction action, int itemIndex = -1) {
        ActionResult r;
        if (phase_ != BattlePhase::PlayerTurn) {
            r.feedback = "It is not your turn.";
            return r;
        }

        switch (action) {
            case BattleAction::Attack: {
                bool crit = false;
                int dmg = rollDamage(player_->stats, enemy_->stats, enemyDefending_, 1.0, 0.12, crit);
                enemy_->stats.hp -= dmg;
                if (enemy_->stats.hp < 0) enemy_->stats.hp = 0;
                damageDealt_ += dmg;
                pushFx(BattleFx::Type::Damage, Actor::Enemy, dmg, crit);
                std::string prefix = crit ? "Critical hit! " : "";
                addLog(prefix + "You strike the " + enemy_->name() + " for " + std::to_string(dmg) + " damage.", LogKind::PlayerHit);
                break;
            }
            case BattleAction::Defend:
                playerDefending_ = true;
                pushFx(BattleFx::Type::Guard, Actor::Player, 0);
                addLog("You raise your guard. Incoming damage is halved.", LogKind::Info);
                break;
            case BattleAction::UseItem: {
                if (!inventory_ || inventory_->isEmpty()) {
                    r.feedback = "Your pack is empty. Choose another action.";
                    return r;
                }
                if (!inventory_->isValidIndex(itemIndex)) {
                    r.feedback = "Select a valid item first.";
                    return r;
                }
                ItemType type = inventory_->at(itemIndex).type;
                ItemUseResult use = applyItemEffect(type, *player_, &enemy_->stats, enemy_->boss);
                if (!use.success) {
                    r.feedback = use.message;
                    return r;
                }
                if (use.consumed) inventory_->remove(itemIndex);
                ++itemsUsed_;
                if (use.healed > 0) pushFx(BattleFx::Type::Heal, Actor::Player, use.healed);
                if (use.damage > 0) {
                    damageDealt_ += use.damage;
                    pushFx(BattleFx::Type::Damage, Actor::Enemy, use.damage);
                }
                // Stat boosts have no number to show, so show "power up".
                if (use.healed == 0 && use.damage == 0 && !use.escape) pushFx(BattleFx::Type::Buff, Actor::Player, 0);
                addLog(use.message, use.healed > 0 ? LogKind::Heal : LogKind::Arcane);
                if (use.escape) {
                    ++turnsTaken_;
                    phase_ = BattlePhase::Escaped;
                    r.accepted = true;
                    return r;
                }
                break;
            }
            case BattleAction::Escape: {
                if (!canEscape()) {
                    r.feedback = "The Warden blocks every exit. There is no escape!";
                    return r;
                }
                if (rng_->chance(escapeChance())) {
                    ++turnsTaken_;
                    addLog("You slip away into the shadows.", LogKind::Success);
                    phase_ = BattlePhase::Escaped;
                    r.accepted = true;
                    return r;
                }
                pushFx(BattleFx::Type::Miss, Actor::Player, 0);
                addLog("You try to flee, but the " + std::string(enemy_->name()) + " cuts you off!", LogKind::Warning);
                break;
            }
        }

        ++turnsTaken_;
        r.accepted = true;
        schedule(Actor::Player, now_ + interval(player_->stats.speed));
        advance();
        return r;
    }

    // Call while phase() == EnemyTurn.
    void enemyAct() {
        if (phase_ != BattlePhase::EnemyTurn) return;
        const EnemyInfo& info = enemyInfo(enemy_->kind);

        // Below 35% HP the enemy sometimes guards instead of attacking.
        bool lowHealth = enemy_->stats.hp * 100 < enemy_->stats.maxHp * 35;
        if (lowHealth && rng_->chance(0.2)) {
            enemyDefending_ = true;
            pushFx(BattleFx::Type::Guard, Actor::Enemy, 0);
            addLog(std::string("The ") + enemy_->name() + " braces itself.", LogKind::Info);
        } else {
            bool special = rng_->chance(info.specialChance);
            double mult = special ? info.specialMultiplier : 1.0;
            bool crit = false;
            int dmg = rollDamage(enemy_->stats, player_->stats, playerDefending_, mult, 0.06, crit);
            player_->stats.hp -= dmg;
            if (player_->stats.hp < 0) player_->stats.hp = 0;
            damageTaken_ += dmg;
            pushFx(BattleFx::Type::Damage, Actor::Player, dmg, crit || special);

            std::string verb = " hits you for ";
            if (special) verb = std::string(" unleashes ") + info.specialName + " for ";
            std::string ending = playerDefending_ ? " (guarded)." : ".";
            addLog(std::string("The ") + enemy_->name() + verb + std::to_string(dmg) + " damage" + ending, LogKind::EnemyHit);
        }

        ++turnsTaken_;
        schedule(Actor::Enemy, now_ + interval(enemy_->stats.speed));
        advance();
    }

    // The next `count` turns, worked out on a copy of the heap so the real
    // queue is untouched. The fighter acting now is listed first.
    DynamicArray<TurnEvent> upcoming(int count) const {
        DynamicArray<TurnEvent> result;
        if (!player_ || !enemy_ || count <= 0) return result;
        if (phase_ != BattlePhase::PlayerTurn && phase_ != BattlePhase::EnemyTurn) return result;

        TurnQueue sim = queue_;
        int seq = seq_;
        result.push_back({now_, -1, current_});
        sim.push({now_ + interval(speedOf(current_)), seq, current_});
        ++seq;

        while (result.size() < count && !sim.empty()) {
            TurnEvent ev = sim.pop();
            result.push_back(ev);
            sim.push({ev.time + interval(speedOf(ev.actor)), seq, ev.actor});
            ++seq;
        }
        return result;
    }

    bool canEscape() const { return enemy_ && !enemy_->boss; }

    // 50%, plus 4% per point of speed you have over the enemy, kept in 20..90%.
    double escapeChance() const {
        if (!canEscape()) return 0.0;
        double c = 0.5 + (player_->stats.speed - enemy_->stats.speed) * 0.04;
        if (c < 0.2) c = 0.2;
        if (c > 0.9) c = 0.9;
        return c;
    }

    // The lowest and highest damage a normal hit can do (shown on the button).
    void damageRange(const Stats& attacker, const Stats& defender, int& lo, int& hi) const {
        lo = attacker.attack - 2 - defender.defense;
        hi = attacker.attack + 3 - defender.defense;
        if (lo < 1) lo = 1;
        if (hi < 1) hi = 1;
    }

    bool playerDefending() const { return playerDefending_; }
    bool enemyDefending() const { return enemyDefending_; }
    int round() const { return turnsTaken_; }
    int damageDealt() const { return damageDealt_; }
    int damageTaken() const { return damageTaken_; }
    int itemsUsed() const { return itemsUsed_; }
    double now() const { return now_; }
    const TurnQueue& queue() const { return queue_; }

    const Player* player() const { return player_; }
    const Enemy* enemy() const { return enemy_; }
    const DynamicArray<LogLine>& log() const { return log_; }

    // Takes the oldest pending effect; returns false when there is none.
    bool popFx(BattleFx& out) {
        if (fx_.empty()) return false;
        out = fx_[0];
        fx_.removeAt(0);
        return true;
    }

    static double interval(int speed) {
        if (speed < 1) speed = 1;
        return 100.0 / speed;
    }

private:
    int speedOf(Actor actor) const { return actor == Actor::Player ? player_->stats.speed : enemy_->stats.speed; }

    void schedule(Actor actor, double atTime) {
        queue_.push({atTime, seq_, actor});
        ++seq_;
    }

    // Ends the battle if someone is down; otherwise starts the next turn.
    void advance() {
        if (player_->stats.hp <= 0) {
            phase_ = BattlePhase::Lost;
            addLog("You collapse to the cold stone floor...", LogKind::Danger);
            return;
        }
        if (enemy_->stats.hp <= 0) {
            phase_ = BattlePhase::Won;
            addLog(std::string("The ") + enemy_->name() + " is defeated!", LogKind::Success);
            return;
        }
        TurnEvent next;
        if (!queue_.tryPop(next)) {
            // Should never happen; give the player a turn rather than hang.
            schedule(Actor::Player, now_ + interval(player_->stats.speed));
            next = queue_.pop();
        }
        now_ = next.time;
        current_ = next.actor;
        // A guard lasts until that fighter's next turn begins.
        if (current_ == Actor::Player) {
            playerDefending_ = false;
            phase_ = BattlePhase::PlayerTurn;
        } else {
            enemyDefending_ = false;
            phase_ = BattlePhase::EnemyTurn;
        }
    }

    // damage = (ATK + random -2..+3) * multiplier * crit - DEF, halved when
    // guarding, and always at least 1.
    int rollDamage(const Stats& attacker, const Stats& defender, bool defending, double multiplier, double critChance, bool& crit) {
        double raw = attacker.attack + rng_->range(-2, 3);
        crit = rng_->chance(critChance);
        double critMult = crit ? 1.6 : 1.0;
        raw *= multiplier * critMult;
        double dmg = raw - defender.defense;
        if (defending) dmg *= 0.5;
        int result = static_cast<int>(std::lround(dmg));
        if (result < 1) result = 1;
        return result;
    }

    void addLog(const std::string& text, LogKind kind) {
        log_.push_back({text, kind});
        if (log_.size() > 40) log_.removeAt(0);
    }

    void pushFx(BattleFx::Type type, Actor target, int amount, bool crit = false) {
        BattleFx fx;
        fx.type = type;
        fx.target = target;
        fx.amount = amount;
        fx.critical = crit;
        fx_.push_back(fx);
    }

    Player* player_ = nullptr;
    Enemy* enemy_ = nullptr;
    Inventory* inventory_ = nullptr;
    Rng* rng_ = nullptr;

    TurnQueue queue_;
    BattlePhase phase_ = BattlePhase::Idle;
    Actor current_ = Actor::Player;
    double now_ = 0.0;
    int seq_ = 0;
    int turnsTaken_ = 0;
    bool playerDefending_ = false;
    bool enemyDefending_ = false;
    int damageDealt_ = 0;
    int damageTaken_ = 0;
    int itemsUsed_ = 0;

    DynamicArray<LogLine> log_;
    DynamicArray<BattleFx> fx_;
};

// ============================================================================
//  11. GameSession
//  All the rules for one run, with no drawing code. The SFML part reads
//  this state and calls these methods when the player does something.
// ============================================================================
enum class SessionPhase { Exploring, Battle, Victory, Defeat };

enum class MoveResult {
    Moved,
    Blocked,        // wall
    OutOfBounds,
    PickedUp,       // moved and picked up an item
    InventoryFull,  // moved onto an item but the pack is full
    BattleStarted,  // walked into an enemy
    ReachedExit,
    NotAllowed      // not exploring right now
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

    explicit GameSession(GeneratorConfig config = GeneratorConfig()) : generator_(config) {}

    void newRun(std::uint32_t seed) {
        seed_ = seed;
        level_ = generator_.generate(seed);
        // Combat uses its own random stream, derived from the seed.
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

    bool hasRun() const { return hasRun_; }
    std::uint32_t seed() const { return seed_; }
    SessionPhase phase() const { return phase_; }

    // ---- Exploration ---------------------------------------------------------
    MoveResult move(int dx, int dy) {
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

    // Plans an A* route to `tile` through explored ground. Returns false and
    // sets `feedback` when there is no route.
    bool planPath(int tile, std::string* feedback = nullptr) {
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

        // Only search explored tiles, and walk around enemies you can see
        // (unless the enemy is the target itself).
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

    bool planPathToExit(std::string* feedback = nullptr) {
        if (!exitDiscovered()) {
            if (feedback) *feedback = "You have not found the exit portal yet.";
            return false;
        }
        return planPath(level_.dungeon.exit, feedback);
    }

    bool hasPlannedPath() const { return plannedPath_.found && plannedPath_.path.size() > 1; }
    const PathResult& lastSearch() const { return plannedPath_; }
    int pathCursor() const { return pathCursor_; }
    int pathTarget() const { return hasPlannedPath() ? plannedPath_.path.back() : -1; }

    // Takes one step along the planned route.
    MoveResult followPathStep() {
        if (phase_ != SessionPhase::Exploring || !hasPlannedPath()) return MoveResult::NotAllowed;
        if (pathCursor_ + 1 >= plannedPath_.path.size()) {
            clearPath();
            return MoveResult::NotAllowed;
        }
        const Dungeon& d = level_.dungeon;
        int next = plannedPath_.path[pathCursor_ + 1];
        int dx = d.xOf(next) - d.xOf(player_.tile);
        int dy = d.yOf(next) - d.yOf(player_.tile);
        // The next tile should be right next to us; if not, stop safely.
        if (std::abs(dx) + std::abs(dy) != 1) {
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

    void clearPath() {
        // Only the route is dropped. `found`, the cost and the expanded tiles
        // stay so the overlay and sidebar can still show the last search.
        plannedPath_.path.clear();
        pathCursor_ = 0;
    }

    // ---- Inventory -----------------------------------------------------------
    ItemUseResult useItem(int index) {
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

    bool discardItem(int index, std::string* feedback = nullptr) {
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

    // ---- Battle --------------------------------------------------------------
    BattleSystem& battle() { return battle_; }
    const BattleSystem& battle() const { return battle_; }
    int battleEnemyIndex() const { return battleEnemy_; }

    // Applies the result of a finished battle (XP, enemy removal, defeat).
    // Returns the XP gained (0 unless the battle was won).
    int finishBattle(int* levelsGained = nullptr) {
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

    // ---- Queries -------------------------------------------------------------
    const Dungeon& dungeon() const { return level_.dungeon; }
    const Player& player() const { return player_; }
    const Inventory& inventory() const { return inventory_; }
    const DynamicArray<Enemy>& enemies() const { return level_.enemies; }
    const DynamicArray<FloorItem>& floorItems() const { return level_.items; }

    // Index into enemies() of the living enemy on `tile`, or -1.
    int enemyAt(int tile) const {
        for (int i = 0; i < level_.enemies.size(); ++i) {
            if (level_.enemies[i].alive && level_.enemies[i].tile == tile) return i;
        }
        return -1;
    }

    // Index into floorItems() of the item still lying on `tile`, or -1.
    int itemAt(int tile) const {
        for (int i = 0; i < level_.items.size(); ++i) {
            if (!level_.items[i].taken && level_.items[i].tile == tile) return i;
        }
        return -1;
    }

    bool isExplored(int tile) const { return explored_[tile]; }
    bool isVisible(int tile) const { return visible_[tile]; }
    bool exitDiscovered() const { return explored_[level_.dungeon.exit]; }

    int enemiesRemaining() const {
        int n = 0;
        for (const Enemy& e : level_.enemies) {
            if (e.alive) ++n;
        }
        return n;
    }

    int exploredPercent() const {
        const Dungeon& d = level_.dungeon;
        int walkable = 0;
        int seen = 0;
        for (int i = 0; i < d.tileCount(); ++i) {
            if (!d.isWalkableIdx(i)) continue;
            ++walkable;
            if (explored_[i]) ++seen;
        }
        if (walkable == 0) return 0;
        return (seen * 100) / walkable;
    }

    int generationAttempts() const { return level_.attempts; }
    int lastDirection() const { return lastDir_; }
    const RunStats& stats() const { return stats_; }
    const DynamicArray<LogLine>& messages() const { return messages_; }

    void log(const std::string& text, LogKind kind = LogKind::Info) {
        messages_.push_back({text, kind});
        if (messages_.size() > 60) messages_.removeAt(0);
    }

    // Marks every tile as explored (demo aid, bound to F2).
    void revealAll() {
        for (int i = 0; i < explored_.size(); ++i) explored_[i] = true;
    }

    // For tests and the screenshot tool.
    Player& mutablePlayer() { return player_; }
    Inventory& mutableInventory() { return inventory_; }
    DynamicArray<Enemy>& mutableEnemies() { return level_.enemies; }

private:
    MoveResult stepOnto(int tile) {
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
            const char* name = itemInfo(fi.item.type).name;
            if (inventory_.add(fi.item)) {
                fi.taken = true;
                ++stats_.itemsCollected;
                log(std::string("Picked up ") + name + ".", LogKind::Success);
                return MoveResult::PickedUp;
            }
            log(std::string("Your pack is full. The ") + name + " stays on the floor.", LogKind::Warning);
            return MoveResult::InventoryFull;
        }
        return MoveResult::Moved;
    }

    void startBattle(int enemyIndex) {
        clearPath();
        battleEnemy_ = enemyIndex;
        phase_ = SessionPhase::Battle;
        battle_.begin(&player_, &level_.enemies[enemyIndex], &inventory_, &rng_);
    }

    // Bresenham line from (x0, y0) to (x1, y1). Any wall in between blocks
    // the view; the end tile itself may be a wall (so walls can be seen).
    bool lineOfSight(int x0, int y0, int x1, int y1) const {
        const Dungeon& d = level_.dungeon;
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;
        int x = x0;
        int y = y0;
        while (!(x == x1 && y == y1)) {
            bool isStart = (x == x0 && y == y0);
            if (!isStart && !d.isWalkable(x, y)) return false;
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

    // Recomputes which tiles the player can see; seen tiles become explored.
    void updateVisibility() {
        const Dungeon& d = level_.dungeon;
        for (int i = 0; i < visible_.size(); ++i) visible_[i] = false;
        int px = d.xOf(player_.tile);
        int py = d.yOf(player_.tile);
        const int r = kSightRadius;
        for (int y = py - r; y <= py + r; ++y) {
            for (int x = px - r; x <= px + r; ++x) {
                if (!d.inBounds(x, y)) continue;
                int ddx = x - px;
                int ddy = y - py;
                // "+ r" makes the circle a little rounder at the edges.
                if (ddx * ddx + ddy * ddy > r * r + r) continue;
                if (!lineOfSight(px, py, x, y)) continue;
                int idx = d.index(x, y);
                visible_[idx] = true;
                explored_[idx] = true;
            }
        }
    }

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

// ############################################################################
//  PART B - SFML GAME
//  Everything below draws the game and handles input. The rules all live in
//  GameSession above; this part only reads them and calls them.
// ############################################################################
#ifndef DUNGEONBOUND_NO_GAME

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>

// ============================================================================
//  12. Theme: window layout and colour palette
// ============================================================================
namespace theme {

constexpr unsigned kWindowW = 1340;
constexpr unsigned kWindowH = 780;
const float kW = static_cast<float>(kWindowW);
const float kH = static_cast<float>(kWindowH);

constexpr float kTile = 26.f;  // map tile size in pixels
constexpr float kMapX = 16.f;  // top-left corner of the map
constexpr float kMapY = 72.f;

constexpr float kTopBarH = 56.f;
constexpr float kSideX = 996.f;  // sidebar position and width
constexpr float kSideW = 334.f;

const sf::Color bg{10, 12, 18};
const sf::Color bgTop{15, 18, 27};
const sf::Color void_{13, 15, 22};
const sf::Color panel{19, 23, 34};
const sf::Color panelHi{26, 31, 45};
const sf::Color panelHover{32, 38, 55};
const sf::Color border{40, 47, 66};
const sf::Color borderHi{66, 76, 100};

const sf::Color text{232, 234, 240};
const sf::Color muted{150, 158, 178};
const sf::Color dim{94, 102, 122};

const sf::Color gold{226, 182, 92};
const sf::Color goldDeep{150, 116, 52};
const sf::Color teal{79, 200, 196};
const sf::Color violet{160, 126, 255};
const sf::Color red{229, 86, 78};
const sf::Color green{90, 204, 138};
const sf::Color blue{108, 166, 240};
const sf::Color orange{240, 152, 72};

const sf::Color hpHigh{92, 206, 132};
const sf::Color hpMid{232, 186, 80};
const sf::Color hpLow{228, 84, 76};
const sf::Color xp{122, 150, 255};

const sf::Color floorCorridor{33, 39, 54};
const sf::Color floorRoom{40, 46, 63};
const sf::Color wallTop{70, 77, 102};
const sf::Color wallFace{42, 47, 66};

}  // namespace theme

constexpr float kPi = 3.14159265f;

float clamp01(float v) {
    if (v < 0.f) return 0.f;
    if (v > 1.f) return 1.f;
    return v;
}

// Starts fast and slows down at the end (k goes from 0 to 1).
float easeOut(float k) {
    float rest = 1.f - clamp01(k);
    return 1.f - rest * rest * rest;
}

// Feedback text fades out during its last half second.
int fadeOutAlpha(float timeLeft) {
    if (timeLeft < 0.5f) return static_cast<int>(255 * timeLeft / 0.5f);
    return 255;
}

std::string toUpper(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return s;
}

std::string formatNumber(const char* format, double value) {
    char buf[64];
    std::snprintf(buf, sizeof buf, format, value);
    return buf;
}

// Seconds as "mm:ss".
std::string formatTime(float seconds) {
    int s = static_cast<int>(seconds);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

sf::Color logColor(LogKind kind) {
    switch (kind) {
        case LogKind::PlayerHit: return theme::gold;
        case LogKind::EnemyHit: return theme::red;
        case LogKind::Heal: return theme::green;
        case LogKind::Warning: return theme::orange;
        case LogKind::Success: return theme::green;
        case LogKind::Danger: return theme::red;
        case LogKind::Arcane: return theme::violet;
        default: return theme::blue;
    }
}

// ============================================================================
//  13. Fonts
// ============================================================================
struct Fonts {
    sf::Font regular;
    sf::Font bold;
    sf::Font title;
    sf::Font mono;

    // Looks for assets/fonts next to the executable, then in parent folders,
    // and finally falls back to Windows system fonts.
    bool load(const std::string& exeDir, std::string* error = nullptr) {
        namespace fs = std::filesystem;
        const fs::path base = exeDir.empty() ? fs::current_path() : fs::path(exeDir);
        const fs::path folders[] = {
            base / "assets" / "fonts",
            base / ".." / "assets" / "fonts",
            base / ".." / ".." / "assets" / "fonts",
            fs::current_path() / "assets" / "fonts",
        };
        for (const fs::path& dir : folders) {
            if (tryOpen(regular, dir / "DejaVuSans.ttf") && tryOpen(bold, dir / "DejaVuSans-Bold.ttf") &&
                tryOpen(title, dir / "DejaVuSerif-Bold.ttf") && tryOpen(mono, dir / "DejaVuSansMono.ttf")) {
                return true;
            }
        }

        const fs::path sys = "C:/Windows/Fonts";
        if (tryOpen(regular, sys / "segoeui.ttf") || tryOpen(regular, sys / "arial.ttf")) {
            if (!tryOpen(bold, sys / "segoeuib.ttf") && !tryOpen(bold, sys / "arialbd.ttf")) bold = regular;
            if (!tryOpen(title, sys / "georgiab.ttf")) title = bold;
            if (!tryOpen(mono, sys / "consola.ttf")) mono = regular;
            return true;
        }

        if (error) *error = "Could not find assets/fonts (DejaVuSans.ttf). Run the game from the build folder.";
        return false;
    }

private:
    static bool tryOpen(sf::Font& font, const std::filesystem::path& path) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) return false;
        return font.openFromFile(path);
    }
};

// ============================================================================
//  14. Drawing helpers
//  Small functions for shapes, text and widgets. All art is drawn with
//  vector shapes; there are no image files.
// ============================================================================
namespace draw {

enum class Align { Left, Center, Right };

sf::String utf8(const std::string& s) { return sf::String::fromUtf8(s.begin(), s.end()); }

sf::Color withAlpha(sf::Color c, int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    c.a = static_cast<std::uint8_t>(alpha);
    return c;
}

// Multiplies the alpha by `alpha` (0..1).
sf::Color fade(sf::Color c, float alpha) {
    c.a = static_cast<std::uint8_t>(c.a * clamp01(alpha));
    return c;
}

// Brightens (f > 1) or darkens (f < 1) a colour.
sf::Color scale(sf::Color c, float f) {
    auto channel = [f](std::uint8_t v) {
        float r = v * f;
        if (r < 0.f) r = 0.f;
        if (r > 255.f) r = 255.f;
        return static_cast<std::uint8_t>(r);
    };
    return {channel(c.r), channel(c.g), channel(c.b), c.a};
}

// Blends from a (t = 0) to b (t = 1).
sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = clamp01(t);
    auto blend = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(x + (y - x) * t); };
    return {blend(a.r, b.r), blend(a.g, b.g), blend(a.b, b.b), blend(a.a, b.a)};
}

// Green at full health, yellow at half, red when low.
sf::Color hpColor(float fraction) {
    if (fraction > 0.5f) return mix(theme::hpMid, theme::hpHigh, (fraction - 0.5f) * 2.f);
    return mix(theme::hpLow, theme::hpMid, fraction * 2.f);
}

void rect(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill) {
    sf::RectangleShape s(r.size);
    s.setPosition(r.position);
    s.setFillColor(fill);
    t.draw(s);
}

void roundedRect(sf::RenderTarget& t, sf::FloatRect r, float radius, sf::Color fill,
                 sf::Color outline = sf::Color::Transparent, float thickness = 0.f) {
    float maxRadius = std::min(r.size.x, r.size.y) * 0.5f;
    if (radius > maxRadius) radius = maxRadius;
    if (radius <= 0.5f) {
        sf::RectangleShape s(r.size);
        s.setPosition(r.position);
        s.setFillColor(fill);
        if (thickness > 0.f) {
            s.setOutlineThickness(-thickness);  // negative = drawn inside the box
            s.setOutlineColor(outline);
        }
        t.draw(s);
        return;
    }
    // Build the outline from four quarter circles, 6 points each.
    const int seg = 6;
    sf::ConvexShape s(static_cast<std::size_t>(seg * 4));
    const sf::Vector2f corners[4] = {
        {r.position.x + r.size.x - radius, r.position.y + radius},
        {r.position.x + r.size.x - radius, r.position.y + r.size.y - radius},
        {r.position.x + radius, r.position.y + r.size.y - radius},
        {r.position.x + radius, r.position.y + radius},
    };
    std::size_t idx = 0;
    for (int c = 0; c < 4; ++c) {
        float startAngle = -kPi / 2.f + c * (kPi / 2.f);
        for (int i = 0; i < seg; ++i) {
            float a = startAngle + (kPi / 2.f) * (static_cast<float>(i) / (seg - 1));
            s.setPoint(idx, {corners[c].x + std::cos(a) * radius, corners[c].y + std::sin(a) * radius});
            ++idx;
        }
    }
    s.setFillColor(fill);
    if (thickness > 0.f) {
        s.setOutlineThickness(-thickness);
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

// A rounded box with a dark copy drawn `drop` pixels below it as a shadow.
void raisedBox(sf::RenderTarget& t, sf::FloatRect r, float radius, float drop, sf::Color shadow, sf::Color fill,
               sf::Color outline = sf::Color::Transparent, float thickness = 0.f) {
    roundedRect(t, {{r.position.x, r.position.y + drop}, r.size}, radius, shadow);
    roundedRect(t, r, radius, fill, outline, thickness);
}

// Two triangles, with colours given per corner.
void colorQuad(sf::RenderTarget& t, sf::FloatRect r, sf::Color topLeft, sf::Color topRight, sf::Color bottomRight,
               sf::Color bottomLeft) {
    const sf::Vector2f p = r.position, s = r.size;
    sf::Vertex v[6];
    v[0] = {{p.x, p.y}, topLeft};
    v[1] = {{p.x + s.x, p.y}, topRight};
    v[2] = {{p.x + s.x, p.y + s.y}, bottomRight};
    v[3] = {{p.x, p.y}, topLeft};
    v[4] = {{p.x + s.x, p.y + s.y}, bottomRight};
    v[5] = {{p.x, p.y + s.y}, bottomLeft};
    t.draw(v, 6, sf::PrimitiveType::Triangles);
}

void gradientRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color top, sf::Color bottom) { colorQuad(t, r, top, top, bottom, bottom); }

void hGradientRect(sf::RenderTarget& t, sf::FloatRect r, sf::Color left, sf::Color right) { colorQuad(t, r, left, right, right, left); }

// A thick line, drawn as a thin rectangle along a -> b.
void line(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b, sf::Color c, float thickness = 1.f) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.001f) return;
    sf::Vector2f n{-d.y / len * thickness * 0.5f, d.x / len * thickness * 0.5f};
    sf::Vertex v[6];
    v[0] = {a + n, c};
    v[1] = {b + n, c};
    v[2] = {b - n, c};
    v[3] = {a + n, c};
    v[4] = {b - n, c};
    v[5] = {a - n, c};
    t.draw(v, 6, sf::PrimitiveType::Triangles);
}

void circle(sf::RenderTarget& t, sf::Vector2f c, float radius, sf::Color fill, sf::Color outline = sf::Color::Transparent,
            float thickness = 0.f, std::size_t points = 32) {
    sf::CircleShape s(radius, points);
    s.setOrigin({radius, radius});
    s.setPosition(c);
    s.setFillColor(fill);
    if (thickness > 0.f) {
        s.setOutlineThickness(-thickness);
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

void ring(sf::RenderTarget& t, sf::Vector2f c, float radius, float thickness, sf::Color color) {
    circle(t, c, radius, sf::Color::Transparent, color, thickness, 48);
}

// An ellipse is a unit circle stretched to rx by ry.
void ellipse(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, sf::Color fill, sf::Color outline = sf::Color::Transparent,
             float thickness = 0.f) {
    sf::CircleShape s(1.f, 36);
    s.setOrigin({1.f, 1.f});
    s.setScale({rx, ry});
    s.setPosition(c);
    s.setFillColor(fill);
    if (thickness > 0.f) {
        // The outline gets stretched too, so divide by the smaller radius.
        s.setOutlineThickness(thickness / std::min(rx, ry));
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

void polygon(sf::RenderTarget& t, const sf::Vector2f* points, std::size_t n, sf::Color fill,
             sf::Color outline = sf::Color::Transparent, float thickness = 0.f) {
    sf::ConvexShape s(n);
    for (std::size_t i = 0; i < n; ++i) s.setPoint(i, points[i]);
    s.setFillColor(fill);
    if (thickness > 0.f) {
        s.setOutlineThickness(thickness);
        s.setOutlineColor(outline);
    }
    t.draw(s);
}

// Soft light: several see-through circles added on top of each other.
void glow(sf::RenderTarget& t, sf::Vector2f c, float radius, sf::Color color, int layers = 6) {
    sf::RenderStates states;
    states.blendMode = sf::BlendAdd;
    for (int i = 0; i < layers; ++i) {
        float k = 1.f - static_cast<float>(i) / layers;
        sf::CircleShape s(radius * k, 40);
        s.setOrigin({radius * k, radius * k});
        s.setPosition(c);
        s.setFillColor(withAlpha(color, static_cast<int>(color.a * 0.16f)));
        t.draw(s, states);
    }
}

void shadow(sf::RenderTarget& t, sf::Vector2f c, float rx, float ry, int alpha = 90) {
    ellipse(t, c, rx, ry, sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha)));
}

// Draws one line of text. `pos.y` is the vertical middle of the line.
sf::FloatRect text(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, sf::Color color,
                   Align align = Align::Left, float spacing = 1.f) {
    sf::Text tx(f, utf8(s), size);
    tx.setFillColor(color);
    tx.setLetterSpacing(spacing);
    sf::FloatRect b = tx.getLocalBounds();
    float originX = 0.f;
    if (align == Align::Center) originX = b.position.x + b.size.x / 2.f;
    else if (align == Align::Right) originX = b.position.x + b.size.x;
    // Rounding to whole pixels keeps the text sharp.
    tx.setOrigin({std::round(originX), std::round(size * 0.62f)});
    tx.setPosition({std::round(pos.x), std::round(pos.y)});
    t.draw(tx);
    return tx.getGlobalBounds();
}

float textWidth(const sf::Font& f, const std::string& s, unsigned size, float spacing = 1.f) {
    sf::Text tx(f, utf8(s), size);
    tx.setLetterSpacing(spacing);
    sf::FloatRect b = tx.getLocalBounds();
    return b.position.x + b.size.x;
}

// Word-wrapped text. `pos` is the top of the first line. Returns the height used.
float wrapped(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, float maxWidth,
              sf::Color color, float lineHeight, Align align = Align::Left) {
    std::string lineText;
    std::string word;
    float y = pos.y + lineHeight / 2.f;
    int lines = 0;
    auto flushLine = [&]() {
        float x = pos.x;
        if (align == Align::Center) x = pos.x + maxWidth / 2.f;
        text(t, f, lineText, size, {x, y}, color, align);
        y += lineHeight;
        ++lines;
        lineText.clear();
    };
    // Read one word at a time; the extra loop step adds a final space.
    for (std::size_t i = 0; i <= s.size(); ++i) {
        char ch = i < s.size() ? s[i] : ' ';
        if (ch != ' ' && ch != '\n') {
            word += ch;
            continue;
        }
        if (!word.empty()) {
            std::string candidate = lineText.empty() ? word : lineText + " " + word;
            if (!lineText.empty() && textWidth(f, candidate, size) > maxWidth) {
                flushLine();
                lineText = word;
            } else {
                lineText = candidate;
            }
            word.clear();
        }
        if (ch == '\n' && !lineText.empty()) flushLine();
    }
    if (!lineText.empty()) flushLine();
    return lines * lineHeight;
}

// The standard card: soft shadow, rounded body, border, top highlight.
void panel(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill = theme::panel) {
    raisedBox(t, r, 10.f, 3.f, sf::Color(0, 0, 0, 70), fill, theme::border, 1.f);
    line(t, {r.position.x + 10.f, r.position.y + 1.5f}, {r.position.x + r.size.x - 10.f, r.position.y + 1.5f}, sf::Color(255, 255, 255, 14), 1.f);
}

// Small, widely spaced caps label at the top of a card.
void sectionLabel(sf::RenderTarget& t, const sf::Font& f, const std::string& s, sf::Vector2f pos, sf::Color color = theme::muted) {
    text(t, f, s, 11, pos, color, Align::Left, 2.2f);
}

// A rounded progress bar. `ghost` (optional) shows a pale trail behind the
// fill, used to show damage just taken.
void bar(sf::RenderTarget& t, sf::FloatRect r, float fraction, sf::Color fill, sf::Color back, float ghost = -1.f) {
    fraction = clamp01(fraction);
    float radius = r.size.y / 2.f;
    roundedRect(t, r, radius, back);
    if (ghost > fraction) {
        if (ghost > 1.f) ghost = 1.f;
        roundedRect(t, {r.position, {r.size.x * ghost, r.size.y}}, radius, sf::Color(255, 255, 255, 60));
    }
    if (fraction > 0.f) {
        float w = std::max(r.size.x * fraction, r.size.y);
        roundedRect(t, {r.position, {w, r.size.y}}, radius, fill);
        // A shiny strip on the upper part of the bar.
        if (r.size.y >= 6.f) {
            roundedRect(t, {{r.position.x + 2.f, r.position.y + 1.f}, {w - 4.f, r.size.y * 0.38f}}, r.size.y * 0.2f,
                        sf::Color(255, 255, 255, 38));
        }
    }
}

// Width of a keycap for `label`, so callers can lay things out before drawing.
float keycapWidth(const sf::Font& f, const std::string& label, unsigned size) { return std::max(textWidth(f, label, size) + 14.f, 22.f); }

// A little keyboard key with `label` on it. `leftCenter` is the middle of its
// left edge. Returns the width.
float keycap(sf::RenderTarget& t, const sf::Font& f, const std::string& label, sf::Vector2f leftCenter, unsigned size = 12) {
    float w = keycapWidth(f, label, size);
    float h = size + 10.f;
    sf::FloatRect r{{leftCenter.x, leftCenter.y - h / 2.f}, {w, h}};
    raisedBox(t, r, 5.f, 2.f, sf::Color(0, 0, 0, 90), theme::panelHi, theme::borderHi, 1.f);
    text(t, f, label, size, {leftCenter.x + w / 2.f, leftCenter.y}, theme::text, Align::Center);
    return w;
}

// A keycap followed by a short label, e.g. [Esc] Pause. Moves `x` past it,
// leaving `gapAfter` pixels of space. An empty label draws the key only.
void keyHint(sf::RenderTarget& t, const sf::Font& keyFont, const sf::Font& labelFont, float& x, float y, const std::string& key,
             const std::string& label, float gapAfter = 16.f) {
    x += keycap(t, keyFont, key, {x, y}, 10) + 6.f;
    if (label.empty()) return;
    text(t, labelFont, label, 12, {x, y}, theme::muted);
    x += textWidth(labelFont, label, 12) + gapAfter;
}

struct ButtonState {
    bool hovered = false;
    bool selected = false;
    bool enabled = true;
};

// A button with a centred label and an optional keycap to its left.
void button(sf::RenderTarget& t, const sf::Font& font, sf::FloatRect r, const std::string& label, ButtonState state,
            const std::string& key = "", sf::Color accent = theme::gold, unsigned size = 13) {
    sf::Color fill = theme::panelHi;
    sf::Color edge = theme::border;
    sf::Color labelColor = theme::text;
    if (!state.enabled) {
        fill = sf::Color(17, 20, 30);
        labelColor = theme::dim;
    } else if (state.selected) {
        fill = mix(theme::panelHi, accent, 0.16f);
        edge = accent;
    } else if (state.hovered) {
        fill = theme::panelHover;
        edge = theme::borderHi;
    }
    raisedBox(t, r, 8.f, 2.f, sf::Color(0, 0, 0, 70), fill, edge, 1.f);

    float cy = r.position.y + r.size.y / 2.f;
    float labelW = textWidth(font, label, size);
    float keyW = 0.f;
    if (!key.empty()) keyW = keycapWidth(font, key, 10) + 8.f;
    float x = r.position.x + (r.size.x - labelW - keyW) / 2.f;
    if (!key.empty()) {
        keycap(t, font, key, {x, cy}, 10);
        x += keyW;
    }
    text(t, font, label, size, {x, cy}, labelColor);
}

float tagWidth(const sf::Font& font, const std::string& label) { return textWidth(font, label, 10, 1.6f) + 22.f; }

// A small pill-shaped tag centred on `center`. Returns its width.
float tag(sf::RenderTarget& t, const sf::Font& font, const std::string& label, sf::Vector2f center, sf::Color color) {
    float w = tagWidth(font, label);
    sf::FloatRect r{{center.x - w / 2.f, center.y - 10.f}, {w, 20.f}};
    roundedRect(t, r, 10.f, withAlpha(color, 34), withAlpha(color, 150), 1.f);
    text(t, font, label, 10, {center.x, center.y}, color, Align::Center, 1.6f);
    return w;
}

// A tag whose right edge is at `right`. Returns its width.
float tagRight(sf::RenderTarget& t, const sf::Font& font, const std::string& label, float right, float y, sf::Color color) {
    float w = tagWidth(font, label);
    tag(t, font, label, {right - w / 2.f, y}, color);
    return w;
}

// Darkens the whole screen behind a pop-up panel.
void scrim(sf::RenderTarget& t, int alpha) {
    sf::Vector2f size = t.getView().getSize();
    rect(t, {{0.f, 0.f}, size}, withAlpha(sf::Color(6, 8, 13), alpha));
}

sf::Color itemColor(ItemType type) {
    switch (type) {
        case ItemType::HealthPotion: return {226, 72, 86};
        case ItemType::Elixir: return {244, 204, 96};
        case ItemType::FireBomb: return {244, 128, 56};
        case ItemType::StrengthTonic: return {236, 120, 190};
        case ItemType::IronSkin: return {128, 168, 214};
        case ItemType::SmokeBomb: return {168, 174, 190};
        default: return theme::text;
    }
}

sf::Color enemyColor(EnemyKind kind) {
    switch (kind) {
        case EnemyKind::Slime: return {108, 206, 120};
        case EnemyKind::Goblin: return {196, 176, 74};
        case EnemyKind::Skeleton: return {222, 216, 200};
        case EnemyKind::Orc: return {112, 150, 88};
        case EnemyKind::Warden: return {170, 116, 255};
        default: return theme::text;
    }
}

// Icons are about 18 px across at scale s = 1.
void itemIcon(sf::RenderTarget& t, ItemType type, sf::Vector2f c, float s, float alpha = 1.f) {
    const sf::Color outline = fade(sf::Color(8, 10, 16, 220), alpha);
    const sf::Color col = fade(itemColor(type), alpha);
    const float th = 1.2f * (s > 1.5f ? s * 0.7f : 1.f);

    switch (type) {
        case ItemType::HealthPotion:
        case ItemType::Elixir:
        case ItemType::StrengthTonic: {
            // Flask: neck, cork, body, highlight.
            rect(t, {{c.x - 2.2f * s, c.y - 7.5f * s}, {4.4f * s, 4.5f * s}}, fade(sf::Color(190, 200, 220), alpha));
            rect(t, {{c.x - 2.8f * s, c.y - 9.f * s}, {5.6f * s, 2.4f * s}}, fade(sf::Color(150, 100, 60), alpha));
            if (type == ItemType::StrengthTonic) {
                sf::Vector2f tri[3] = {{c.x, c.y - 4.f * s}, {c.x + 6.5f * s, c.y + 6.5f * s}, {c.x - 6.5f * s, c.y + 6.5f * s}};
                polygon(t, tri, 3, col, outline, th);
            } else {
                circle(t, {c.x, c.y + 2.f * s}, 6.2f * s, col, outline, th);
                circle(t, {c.x, c.y + 3.2f * s}, 4.2f * s, fade(scale(itemColor(type), 0.78f), alpha));
            }
            circle(t, {c.x - 2.2f * s, c.y + 0.5f * s}, 1.5f * s, fade(sf::Color(255, 255, 255, 170), alpha));
            if (type == ItemType::Elixir) glow(t, {c.x, c.y + 2.f * s}, 11.f * s, fade(sf::Color(244, 204, 96, 140), alpha), 4);
            break;
        }
        case ItemType::FireBomb:
            circle(t, {c.x, c.y + 1.5f * s}, 6.5f * s, fade(sf::Color(58, 60, 72), alpha), outline, th);
            circle(t, {c.x - 2.f * s, c.y - 0.5f * s}, 1.8f * s, fade(sf::Color(255, 255, 255, 60), alpha));
            line(t, {c.x + 3.f * s, c.y - 4.f * s}, {c.x + 5.5f * s, c.y - 7.5f * s}, fade(sf::Color(200, 170, 120), alpha), 1.4f * s);
            circle(t, {c.x + 6.f * s, c.y - 8.f * s}, 1.8f * s, col);
            glow(t, {c.x + 6.f * s, c.y - 8.f * s}, 6.f * s, fade(sf::Color(255, 150, 60, 200), alpha), 4);
            break;
        case ItemType::IronSkin: {
            sf::Vector2f shield[5] = {{c.x - 6.5f * s, c.y - 7.f * s}, {c.x + 6.5f * s, c.y - 7.f * s}, {c.x + 6.f * s, c.y + 1.f * s},
                                      {c.x, c.y + 8.f * s},          {c.x - 6.f * s, c.y + 1.f * s}};
            polygon(t, shield, 5, col, outline, th);
            sf::Vector2f shine[4] = {{c.x, c.y - 5.f * s}, {c.x + 4.f * s, c.y - 5.f * s}, {c.x + 3.6f * s, c.y + 1.f * s}, {c.x, c.y + 5.f * s}};
            polygon(t, shine, 4, fade(sf::Color(255, 255, 255, 70), alpha));
            break;
        }
        case ItemType::SmokeBomb:
            circle(t, {c.x - 3.5f * s, c.y + 2.f * s}, 4.5f * s, fade(scale(itemColor(type), 0.8f), alpha), outline, th);
            circle(t, {c.x + 3.5f * s, c.y + 2.f * s}, 4.5f * s, fade(scale(itemColor(type), 0.9f), alpha), outline, th);
            circle(t, {c.x, c.y - 2.5f * s}, 5.f * s, col, outline, th);
            break;
        default:
            break;
    }
}

void enemyIcon(sf::RenderTarget& t, EnemyKind kind, sf::Vector2f c, float s, float time, float alpha = 1.f) {
    const sf::Color outline = fade(sf::Color(8, 10, 16, 230), alpha);
    const sf::Color col = fade(enemyColor(kind), alpha);
    const sf::Color dark = fade(sf::Color(14, 14, 20), alpha);
    const float th = 1.3f * (s > 1.5f ? s * 0.6f : 1.f);

    switch (kind) {
        case EnemyKind::Slime: {
            float squish = 1.f + 0.06f * std::sin(time * 4.f);
            ellipse(t, {c.x, c.y + 2.f * s}, 8.f * s * squish, 6.5f * s / squish, col, outline, th);
            ellipse(t, {c.x - 3.f * s, c.y - 1.f * s}, 2.2f * s, 1.4f * s, fade(sf::Color(255, 255, 255, 110), alpha));
            circle(t, {c.x - 2.6f * s, c.y + 1.5f * s}, 1.4f * s, dark);
            circle(t, {c.x + 2.6f * s, c.y + 1.5f * s}, 1.4f * s, dark);
            break;
        }
        case EnemyKind::Goblin: {
            sf::Vector2f earL[3] = {{c.x - 5.f * s, c.y - 2.f * s}, {c.x - 11.f * s, c.y - 5.f * s}, {c.x - 4.f * s, c.y + 2.f * s}};
            sf::Vector2f earR[3] = {{c.x + 5.f * s, c.y - 2.f * s}, {c.x + 11.f * s, c.y - 5.f * s}, {c.x + 4.f * s, c.y + 2.f * s}};
            polygon(t, earL, 3, col, outline, th);
            polygon(t, earR, 3, col, outline, th);
            circle(t, c, 6.8f * s, col, outline, th);
            circle(t, {c.x - 2.5f * s, c.y - 0.5f * s}, 1.4f * s, fade(sf::Color(255, 70, 50), alpha));
            circle(t, {c.x + 2.5f * s, c.y - 0.5f * s}, 1.4f * s, fade(sf::Color(255, 70, 50), alpha));
            line(t, {c.x - 2.5f * s, c.y + 3.2f * s}, {c.x + 2.5f * s, c.y + 3.2f * s}, dark, 1.f * s);
            break;
        }
        case EnemyKind::Skeleton:
            roundedRect(t, {{c.x - 4.f * s, c.y + 2.f * s}, {8.f * s, 5.5f * s}}, 1.5f * s, col, outline, th);
            circle(t, {c.x, c.y - 1.5f * s}, 7.f * s, col, outline, th);
            circle(t, {c.x - 2.7f * s, c.y - 1.f * s}, 2.f * s, dark);
            circle(t, {c.x + 2.7f * s, c.y - 1.f * s}, 2.f * s, dark);
            for (int i = -1; i <= 1; ++i) line(t, {c.x + i * 2.f * s, c.y + 4.f * s}, {c.x + i * 2.f * s, c.y + 7.f * s}, dark, 0.8f * s);
            break;
        case EnemyKind::Orc: {
            roundedRect(t, {{c.x - 8.f * s, c.y - 7.f * s}, {16.f * s, 15.f * s}}, 4.f * s, col, outline, th);
            rect(t, {{c.x - 8.f * s + th, c.y - 7.f * s + th}, {16.f * s - 2 * th, 4.f * s}}, fade(scale(enemyColor(kind), 0.7f), alpha));
            circle(t, {c.x - 3.f * s, c.y - 1.f * s}, 1.5f * s, fade(sf::Color(255, 60, 40), alpha));
            circle(t, {c.x + 3.f * s, c.y - 1.f * s}, 1.5f * s, fade(sf::Color(255, 60, 40), alpha));
            sf::Vector2f tuskL[3] = {{c.x - 4.5f * s, c.y + 3.f * s}, {c.x - 2.f * s, c.y + 3.f * s}, {c.x - 3.6f * s, c.y - 0.5f * s}};
            sf::Vector2f tuskR[3] = {{c.x + 2.f * s, c.y + 3.f * s}, {c.x + 4.5f * s, c.y + 3.f * s}, {c.x + 3.6f * s, c.y - 0.5f * s}};
            polygon(t, tuskL, 3, fade(sf::Color(245, 240, 225), alpha));
            polygon(t, tuskR, 3, fade(sf::Color(245, 240, 225), alpha));
            break;
        }
        case EnemyKind::Warden: {
            std::uint8_t pulseAlpha = static_cast<std::uint8_t>(160 + 40 * std::sin(time * 3.f));
            glow(t, c, 15.f * s, fade(sf::Color(170, 100, 255, pulseAlpha), alpha), 5);
            sf::Vector2f body[4] = {{c.x, c.y - 9.f * s}, {c.x + 8.5f * s, c.y}, {c.x, c.y + 9.f * s}, {c.x - 8.5f * s, c.y}};
            polygon(t, body, 4, col, outline, th);
            sf::Vector2f crown[7] = {{c.x - 5.f * s, c.y - 9.f * s}, {c.x - 5.5f * s, c.y - 14.f * s}, {c.x - 2.5f * s, c.y - 11.5f * s},
                                     {c.x, c.y - 15.f * s},          {c.x + 2.5f * s, c.y - 11.5f * s}, {c.x + 5.5f * s, c.y - 14.f * s},
                                     {c.x + 5.f * s, c.y - 9.f * s}};
            // The crown is not convex, so draw it as triangles from its base.
            for (int i = 0; i < 6; ++i) {
                sf::Vector2f tri[3] = {{c.x, c.y - 8.5f * s}, crown[i], crown[i + 1]};
                polygon(t, tri, 3, fade(theme::gold, alpha));
            }
            circle(t, {c.x, c.y}, 2.6f * s, fade(sf::Color(255, 240, 255), alpha));
            circle(t, {c.x, c.y}, 1.2f * s, fade(sf::Color(120, 40, 200), alpha));
            break;
        }
        default:
            break;
    }
}

// `dir`: 0 up, 1 right, 2 down, 3 left.
void playerIcon(sf::RenderTarget& t, sf::Vector2f c, float s, int dir, float alpha = 1.f) {
    const sf::Color outline = fade(sf::Color(60, 40, 12, 240), alpha);
    circle(t, c, 8.f * s, fade(theme::gold, alpha), outline, 1.6f * (s > 1.5f ? s * 0.6f : 1.f));
    circle(t, c, 5.f * s, fade(sf::Color(255, 232, 170), alpha));

    // A small arrow shows which way the player faces.
    const float angles[4] = {-kPi / 2.f, 0.f, kPi / 2.f, kPi};
    if (dir < 0 || dir > 3) dir = 2;
    float a = angles[dir];
    sf::Vector2f forward{std::cos(a), std::sin(a)};
    sf::Vector2f side{-forward.y, forward.x};
    sf::Vector2f tip = c + forward * (11.5f * s);
    sf::Vector2f base1 = c + forward * (7.f * s) + side * (3.6f * s);
    sf::Vector2f base2 = c + forward * (7.f * s) - side * (3.6f * s);
    sf::Vector2f tri[3] = {tip, base1, base2};
    polygon(t, tri, 3, fade(sf::Color(255, 240, 200), alpha), outline, 1.f);
    circle(t, c - forward * (0.5f * s), 2.f * s, fade(sf::Color(70, 50, 20), alpha));
}

void portalIcon(sf::RenderTarget& t, sf::Vector2f c, float s, float time, float alpha = 1.f) {
    float pulse = 0.5f + 0.5f * std::sin(time * 2.6f);
    glow(t, c, 16.f * s, fade(sf::Color(150, 110, 255, static_cast<std::uint8_t>(150 + 80 * pulse)), alpha), 6);
    circle(t, c, 8.5f * s, fade(sf::Color(40, 22, 80, 230), alpha));
    circle(t, c, (4.f + 1.5f * pulse) * s, fade(sf::Color(200, 180, 255, 220), alpha));
    ring(t, c, 9.f * s, 1.8f * s, fade(theme::violet, alpha));
    // Six sparks circling the portal.
    for (int i = 0; i < 6; ++i) {
        float a = time * 1.6f + i * (kPi / 3.f);
        circle(t, {c.x + std::cos(a) * 11.f * s, c.y + std::sin(a) * 11.f * s}, 1.3f * s, fade(sf::Color(220, 205, 255), alpha));
    }
}

// The game logo bar across the top of the exploring and battle screens.
void brandBar(sf::RenderTarget& t, const Fonts& f, float time) {
    draw::gradientRect(t, {{0.f, 0.f}, {theme::kW, theme::kTopBarH}}, sf::Color(20, 24, 36), sf::Color(14, 17, 26));
    draw::rect(t, {{0.f, theme::kTopBarH - 1.f}, {theme::kW, 1.f}}, theme::border);
    draw::portalIcon(t, {32.f, 28.f}, 0.85f, time);
    draw::text(t, f.title, "DUNGEONBOUND", 20, {56.f, 27.f}, theme::gold, Align::Left, 1.5f);
    float titleW = draw::textWidth(f.title, "DUNGEONBOUND", 20, 1.5f);
    draw::text(t, f.regular, "The Lost Realms", 13, {66.f + titleW, 28.f}, theme::muted);
}

// The dark gradient behind every screen.
void backdrop(sf::RenderTarget& t) { gradientRect(t, {{0.f, 0.f}, {theme::kW, theme::kH}}, theme::bgTop, theme::bg); }

}  // namespace draw

using draw::Align;

// ============================================================================
//  15. Input: keys -> commands
// ============================================================================
enum class Command {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Confirm,
    Step,    // Space: one step along the route (acts as Confirm in menus)
    Reveal,  // F2: reveal the whole map (demo aid)
    Back,
    Inventory,
    Use,
    Discard,
    Help,
    Overlay,
    PathToExit,
    Restart,
    Digit
};

struct Input {
    Command cmd = Command::None;
    int digit = -1;  // 1..9 when cmd == Digit
};

namespace input {

Input translate(sf::Keyboard::Key key) {
    using K = sf::Keyboard::Key;
    Input in;
    switch (key) {
        case K::W: case K::Up: in.cmd = Command::MoveUp; break;
        case K::S: case K::Down: in.cmd = Command::MoveDown; break;
        case K::A: case K::Left: in.cmd = Command::MoveLeft; break;
        case K::D: case K::Right: in.cmd = Command::MoveRight; break;
        case K::Enter: in.cmd = Command::Confirm; break;
        case K::Space: in.cmd = Command::Step; break;
        case K::F2: in.cmd = Command::Reveal; break;
        case K::Escape: case K::Backspace: in.cmd = Command::Back; break;
        case K::I: case K::B: in.cmd = Command::Inventory; break;
        case K::U: case K::E: in.cmd = Command::Use; break;
        case K::X: case K::Delete: in.cmd = Command::Discard; break;
        case K::H: case K::F1: in.cmd = Command::Help; break;
        case K::Tab: in.cmd = Command::Overlay; break;
        case K::P: in.cmd = Command::PathToExit; break;
        case K::R: in.cmd = Command::Restart; break;
        default: break;
    }
    // Number keys on the main row and on the number pad.
    int k = static_cast<int>(key);
    int row1 = static_cast<int>(K::Num1), row9 = static_cast<int>(K::Num9);
    int pad1 = static_cast<int>(K::Numpad1), pad9 = static_cast<int>(K::Numpad9);
    if (k >= row1 && k <= row9) {
        in.cmd = Command::Digit;
        in.digit = k - row1 + 1;
    } else if (k >= pad1 && k <= pad9) {
        in.cmd = Command::Digit;
        in.digit = k - pad1 + 1;
    }
    return in;
}

bool isMove(Command c) { return c == Command::MoveUp || c == Command::MoveDown || c == Command::MoveLeft || c == Command::MoveRight; }

int dx(Command c) {
    if (c == Command::MoveLeft) return -1;
    if (c == Command::MoveRight) return 1;
    return 0;
}

int dy(Command c) {
    if (c == Command::MoveUp) return -1;
    if (c == Command::MoveDown) return 1;
    return 0;
}

// True while a key for this move command is held down.
bool isHeld(Command c) {
    using K = sf::Keyboard::Key;
    auto down = [](K k) { return sf::Keyboard::isKeyPressed(k); };
    switch (c) {
        case Command::MoveUp: return down(K::W) || down(K::Up);
        case Command::MoveDown: return down(K::S) || down(K::Down);
        case Command::MoveLeft: return down(K::A) || down(K::Left);
        case Command::MoveRight: return down(K::D) || down(K::Right);
        default: return false;
    }
}

}  // namespace input

// ============================================================================
//  16. Map renderer
//  Draws the dungeon, fog of war, entities and the planned route, plus the
//  debug overlays (A* search tiles or the graph's edges).
// ============================================================================
enum class OverlayMode { Off, Search, Graph };

struct MapView {
    sf::Vector2f origin{0.f, 0.f};
    float tile = 26.f;
    bool fog = true;
    bool showPlayer = true;
    OverlayMode overlay = OverlayMode::Off;
    int hoverTile = -1;
    sf::Vector2f playerPos{0.f, 0.f};  // in tiles, smoothed for animation
    float time = 0.f;
    float dimAll = 1.f;
};

// A fixed pseudo-random number per tile, so floor texture never flickers.
unsigned hashTile(int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

// Adds a rectangle (two triangles) to a vertex batch.
void addQuad(sf::VertexArray& va, float x, float y, float w, float h, sf::Color c) {
    va.append({{x, y}, c});
    va.append({{x + w, y}, c});
    va.append({{x + w, y + h}, c});
    va.append({{x, y}, c});
    va.append({{x + w, y + h}, c});
    va.append({{x, y + h}, c});
}

class Renderer {
public:
    void drawMap(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
        drawTiles(t, s, v);
        drawOverlay(t, s, v);
        drawRoute(t, s, v);
        drawEntities(t, s, v);
        drawHoverCursor(t, s, v);
    }

    void drawHoverTooltip(sf::RenderTarget& t, const Fonts& fonts, const GameSession& s, const MapView& v, sf::Vector2f mouse) const {
        if (v.hoverTile < 0 || !s.isExplored(v.hoverTile)) return;
        std::string title, sub;
        sf::Color accent = theme::text;
        int e = s.enemyAt(v.hoverTile);
        int it = s.itemAt(v.hoverTile);
        if (e != -1) {
            const Enemy& en = s.enemies()[e];
            const EnemyInfo& info = enemyInfo(en.kind);
            title = std::string(info.name) + (en.boss ? "  ·  BOSS" : "");
            sub = "HP " + std::to_string(en.stats.hp) + "/" + std::to_string(en.stats.maxHp) + "   ATK " + std::to_string(en.stats.attack) +
                  "   DEF " + std::to_string(en.stats.defense) + "   SPD " + std::to_string(en.stats.speed);
            accent = en.boss ? theme::violet : theme::red;
        } else if (it != -1) {
            ItemType type = s.floorItems()[it].item.type;
            title = itemInfo(type).name;
            sub = itemInfo(type).effect;
            accent = draw::itemColor(type);
        } else if (v.hoverTile == s.dungeon().exit) {
            title = "Exit Portal";
            sub = "Step through to escape the Lost Realms";
            accent = theme::violet;
        } else {
            return;  // nothing worth describing on this tile
        }

        float w = std::max(draw::textWidth(fonts.bold, title, 14), draw::textWidth(fonts.regular, sub, 12)) + 28.f;
        float h = 52.f;
        // Below-right of the cursor, flipped if it would leave the map area.
        sf::Vector2f p{mouse.x + 16.f, mouse.y + 16.f};
        if (p.x + w > theme::kSideX - 8.f) p.x = mouse.x - w - 12.f;
        if (p.y + h > theme::kWindowH - 8.f) p.y = mouse.y - h - 12.f;
        sf::FloatRect r{p, {w, h}};
        draw::raisedBox(t, r, 8.f, 4.f, sf::Color(0, 0, 0, 110), sf::Color(24, 28, 41, 248), theme::borderHi, 1.f);
        draw::rect(t, {{p.x + 1, p.y + 10}, {3.f, h - 20}}, accent);
        draw::text(t, fonts.bold, title, 14, {p.x + 14, p.y + 17}, theme::text);
        draw::text(t, fonts.regular, sub, 12, {p.x + 14, p.y + 36}, theme::muted);
    }

private:
    // Brightness of a tile: 0 = never seen, dim = seen before, bright = in view.
    float lightAt(const GameSession& s, const MapView& v, int x, int y) const {
        if (!v.fog) return 0.82f;
        int idx = s.dungeon().index(x, y);
        if (!s.isExplored(idx)) return 0.f;
        if (!s.isVisible(idx)) return 0.34f;
        // In view: brightest next to the player, darker toward the edge of sight.
        float dx = x - v.playerPos.x;
        float dy = y - v.playerPos.y;
        float d = std::sqrt(dx * dx + dy * dy) / static_cast<float>(GameSession::kSightRadius);
        float light = 1.f - 0.5f * std::pow(std::min(d, 1.f), 1.4f);
        return std::max(light, 0.42f);
    }

    sf::Vector2f tileCenter(const Dungeon& d, const MapView& v, int idx) const {
        return {v.origin.x + (d.xOf(idx) + 0.5f) * v.tile, v.origin.y + (d.yOf(idx) + 0.5f) * v.tile};
    }

    // All tiles go into one vertex array and are drawn with a single call.
    void drawTiles(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
        const Dungeon& d = s.dungeon();
        const float T = v.tile;
        const float gap = T >= 20.f ? 1.f : 0.f;
        sf::VertexArray tiles(sf::PrimitiveType::Triangles);

        for (int y = 0; y < d.height(); ++y) {
            for (int x = 0; x < d.width(); ++x) {
                float px = v.origin.x + x * T;
                float py = v.origin.y + y * T;

                if (d.isWalkable(x, y)) {
                    float light = lightAt(s, v, x, y) * v.dimAll;
                    if (light <= 0.f) continue;
                    unsigned h = hashTile(x, y);
                    float vary = 0.93f + (h % 100) / 100.f * 0.12f;
                    sf::Color base = d.roomAt(x, y) != -1 ? theme::floorRoom : theme::floorCorridor;
                    addQuad(tiles, px + gap, py + gap, T - 2 * gap, T - 2 * gap, draw::scale(base, vary * light * 1.15f));
                    // A speckle on some tiles for texture.
                    if (h % 7 == 0) {
                        addQuad(tiles, px + (h % 13) / 13.f * (T - 6) + 2, py + (h % 11) / 11.f * (T - 6) + 2, 2, 2,
                                draw::scale(base, 1.35f * light));
                    }
                    continue;
                }

                // Walls are only drawn next to floor the player has seen;
                // everything else stays empty.
                float edgeLight = 0.f;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!d.isWalkable(x + ox, y + oy)) continue;
                        edgeLight = std::max(edgeLight, lightAt(s, v, x + ox, y + oy));
                    }
                }
                if (edgeLight <= 0.f) continue;
                edgeLight *= v.dimAll;
                // A wall with visible floor below shows a darker front face.
                bool faceBelow = d.isWalkable(x, y + 1) && lightAt(s, v, x, y + 1) > 0.f;
                float faceH = faceBelow ? T * 0.3f : 0.f;
                addQuad(tiles, px, py, T, T - faceH, draw::scale(theme::wallTop, edgeLight));
                addQuad(tiles, px, py, T, 2.f, draw::scale(theme::wallTop, edgeLight * 1.25f));
                if (faceBelow) addQuad(tiles, px, py + T - faceH, T, faceH, draw::scale(theme::wallFace, edgeLight));
            }
        }
        t.draw(tiles);
    }

    void drawOverlay(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
        const Dungeon& d = s.dungeon();
        const float T = v.tile;
        if (v.overlay == OverlayMode::Search) {
            // Tiles the last A* search closed, coloured blue (first) to teal (last).
            const PathResult& search = s.lastSearch();
            sf::VertexArray va(sf::PrimitiveType::Triangles);
            int n = search.expanded.size();
            for (int i = 0; i < n; ++i) {
                int idx = search.expanded[i];
                float k = n > 1 ? static_cast<float>(i) / (n - 1) : 0.f;
                sf::Color c = draw::withAlpha(draw::mix(theme::blue, theme::teal, k), 70);
                addQuad(va, v.origin.x + d.xOf(idx) * T + 2, v.origin.y + d.yOf(idx) * T + 2, T - 4, T - 4, c);
            }
            t.draw(va);
        } else if (v.overlay == OverlayMode::Graph) {
            const Graph& g = d.graph();
            sf::VertexArray lines(sf::PrimitiveType::Lines);
            for (int u = 0; u < g.vertexCount(); ++u) {
                if (v.fog && !s.isExplored(u)) continue;
                for (const GraphEdge* e = g.edges(u); e != nullptr; e = e->next) {
                    // Each edge is stored twice; draw it once (from the lower id).
                    if (e->to < u) continue;
                    if (v.fog && !s.isExplored(e->to)) continue;
                    lines.append({tileCenter(d, v, u), draw::withAlpha(theme::teal, 120)});
                    lines.append({tileCenter(d, v, e->to), draw::withAlpha(theme::teal, 120)});
                }
            }
            t.draw(lines);
            for (int u = 0; u < g.vertexCount(); ++u) {
                if (g.degree(u) == 0 || (v.fog && !s.isExplored(u))) continue;
                draw::circle(t, tileCenter(d, v, u), 2.f, draw::withAlpha(theme::teal, 200), sf::Color::Transparent, 0.f, 8);
            }
        }
    }

    void drawRoute(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
        if (!s.hasPlannedPath()) return;
        const Dungeon& d = s.dungeon();
        const float unit = v.tile / 26.f;
        const DynamicArray<int>& path = s.lastSearch().path;
        int startAt = s.pathCursor();
        for (int i = startAt; i + 1 < path.size(); ++i) {
            draw::line(t, tileCenter(d, v, path[i]), tileCenter(d, v, path[i + 1]), draw::withAlpha(theme::teal, 90), 3.f * unit);
        }
        // Dots along the route, with a pulse running toward the goal.
        for (int i = startAt + 1; i < path.size() - 1; ++i) {
            float phase = std::fmod(v.time * 8.f - i * 0.6f, 6.f);
            float k = phase < 1.f ? 1.f - phase : 0.f;
            sf::Color c = draw::withAlpha(draw::mix(theme::teal, sf::Color::White, k), 230);
            draw::circle(t, tileCenter(d, v, path[i]), (2.2f + 1.6f * k) * unit, c);
        }
        sf::Vector2f goal = tileCenter(d, v, path.back());
        float pulse = 0.5f + 0.5f * std::sin(v.time * 5.f);
        draw::ring(t, goal, (8.f + 2.f * pulse) * unit, 2.f * unit, draw::withAlpha(theme::teal, 230));
        draw::glow(t, goal, 14.f * unit, draw::withAlpha(theme::teal, 120), 4);
    }

    // How see-through to draw something on `tile`: full when in view,
    // `rememberedAlpha` when only seen before.
    float entityAlpha(const GameSession& s, const MapView& v, int tile, float rememberedAlpha) const {
        if (v.fog && !s.isVisible(tile)) return rememberedAlpha;
        return 1.f;
    }

    void drawEntities(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
        const Dungeon& d = s.dungeon();
        const float T = v.tile;
        const float unit = T / 26.f;

        // Exit portal.
        if (!v.fog || s.isExplored(d.exit)) {
            float a = entityAlpha(s, v, d.exit, 0.55f);
            draw::portalIcon(t, tileCenter(d, v, d.exit), unit * 1.05f, v.time, a * v.dimAll);
        }

        // Items, bobbing up and down.
        for (const FloorItem& fi : s.floorItems()) {
            if (fi.taken || (v.fog && !s.isExplored(fi.tile))) continue;
            float a = entityAlpha(s, v, fi.tile, 0.45f);
            sf::Vector2f c = tileCenter(d, v, fi.tile);
            float bob = std::sin(v.time * 3.f + fi.tile) * 1.5f * unit;
            draw::shadow(t, {c.x, c.y + 8.f * unit}, 6.f * unit, 2.f * unit, static_cast<int>(90 * a));
            if (a > 0.9f) draw::glow(t, {c.x, c.y + bob}, 12.f * unit, draw::withAlpha(draw::itemColor(fi.item.type), 70), 3);
            draw::itemIcon(t, fi.item.type, {c.x, c.y - 1.f * unit + bob}, 0.82f * unit, a * v.dimAll);
        }

        // Enemies, with a small health bar once they are hurt.
        for (const Enemy& e : s.enemies()) {
            if (!e.alive || (v.fog && !s.isExplored(e.tile))) continue;
            float a = entityAlpha(s, v, e.tile, 0.45f);
            sf::Vector2f c = tileCenter(d, v, e.tile);
            draw::shadow(t, {c.x, c.y + 9.f * unit}, 8.f * unit, 2.5f * unit, static_cast<int>(100 * a));
            float bob = std::sin(v.time * 2.2f + e.uid) * 1.f * unit;
            float size = (e.boss ? 0.86f : 0.78f) * unit;
            draw::enemyIcon(t, e.kind, {c.x, c.y + bob}, size, v.time + e.uid, a * v.dimAll);
            if (e.stats.hp < e.stats.maxHp) {
                float f = static_cast<float>(e.stats.hp) / e.stats.maxHp;
                draw::bar(t, {{c.x - 10.f * unit, c.y - 15.f * unit}, {20.f * unit, 3.f * unit}}, f, draw::hpColor(f), sf::Color(0, 0, 0, 170));
            }
        }

        // Player, drawn at the smoothed position.
        if (v.showPlayer) {
            sf::Vector2f pc{v.origin.x + (v.playerPos.x + 0.5f) * T, v.origin.y + (v.playerPos.y + 0.5f) * T};
            draw::glow(t, pc, 40.f * unit, sf::Color(255, 196, 110, 60), 6);
            draw::shadow(t, {pc.x, pc.y + 9.f * unit}, 8.f * unit, 2.5f * unit, 110);
            draw::playerIcon(t, pc, 0.82f * unit, s.lastDirection());
        }
    }

    // Teal box on a tile you can route to, red box on one you cannot.
    void drawHoverCursor(sf::RenderTarget& t, const GameSession& s, const MapView& v) const {
        const Dungeon& d = s.dungeon();
        if (v.hoverTile < 0 || !d.isValidIndex(v.hoverTile)) return;
        const float T = v.tile;
        bool reachable = s.isExplored(v.hoverTile) && d.isWalkableIdx(v.hoverTile);
        sf::Color c = reachable ? draw::withAlpha(theme::teal, 220) : draw::withAlpha(theme::red, 170);
        sf::FloatRect r{{v.origin.x + d.xOf(v.hoverTile) * T + 1, v.origin.y + d.yOf(v.hoverTile) * T + 1}, {T - 2, T - 2}};
        draw::roundedRect(t, r, 4.f, draw::withAlpha(c, 28), c, 1.5f);
    }
};

// ============================================================================
//  17. HUD: top bar, map frame, sidebar cards, journal
// ============================================================================
struct HudState {
    float displayHp = 0.f;  // animated HP value
    float ghostHp = 0.f;    // trails behind displayHp after taking damage
    float time = 0.f;
    float playSeconds = 0.f;
    OverlayMode overlay = OverlayMode::Off;
    bool walking = false;
};

// A rounded chip with a label and a value. `rightCenter` is the middle of
// its right edge. Returns its width.
float statChip(sf::RenderTarget& t, const Fonts& f, const std::string& label, const std::string& value, sf::Vector2f rightCenter,
               sf::Color valueColor = theme::text) {
    float labelW = draw::textWidth(f.bold, label, 10, 1.8f);
    float valueW = draw::textWidth(f.mono, value, 13);
    float w = labelW + valueW + 30.f;
    sf::FloatRect r{{rightCenter.x - w, rightCenter.y - 14.f}, {w, 28.f}};
    draw::roundedRect(t, r, 14.f, theme::panel, theme::border, 1.f);
    draw::text(t, f.bold, label, 10, {r.position.x + 12.f, rightCenter.y}, theme::dim, Align::Left, 1.8f);
    draw::text(t, f.mono, value, 13, {r.position.x + w - 12.f, rightCenter.y}, valueColor, Align::Right);
    return w;
}

const sf::FloatRect kPackCard{{theme::kSideX, 254.f}, {theme::kSideW, 128.f}};

class Hud {
public:
    // The box around the map; the journal sits just below it.
    static sf::FloatRect mapFrame(const GameSession& s) {
        float w = s.dungeon().width() * theme::kTile;
        float h = s.dungeon().height() * theme::kTile;
        return {{theme::kMapX - 6.f, theme::kMapY - 6.f}, {w + 12.f, h + 12.f}};
    }

    void drawTopBar(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
        draw::brandBar(t, f, h.time);
        // Chips are laid out from the right edge toward the left.
        float x = theme::kW - 16.f;
        x -= statChip(t, f, "EXPLORED", std::to_string(s.exploredPercent()) + "%", {x, 28.f}) + 8.f;
        x -= statChip(t, f, "TIME", formatTime(h.playSeconds), {x, 28.f}) + 8.f;
        x -= statChip(t, f, "STEPS", std::to_string(s.stats().steps), {x, 28.f}) + 8.f;
        x -= statChip(t, f, "SEED", std::to_string(s.seed()), {x, 28.f}, theme::gold) + 8.f;
        if (h.overlay != OverlayMode::Off) {
            statChip(t, f, "OVERLAY", h.overlay == OverlayMode::Search ? "A* SEARCH" : "GRAPH", {x, 28.f}, theme::teal);
        } else if (h.walking) {
            statChip(t, f, "AUTO", "WALKING", {x, 28.f}, theme::teal);
        }
    }

    void drawMapFrame(sf::RenderTarget& t, const GameSession& s) const {
        draw::raisedBox(t, mapFrame(s), 12.f, 4.f, sf::Color(0, 0, 0, 90), theme::void_, theme::border, 1.f);
    }

    void drawSidebar(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
        drawHeroCard(t, f, s, h);
        drawPackCard(t, f, s);
        drawObjectiveCard(t, f, s, h);
        drawAlgorithmsCard(t, f, s, h);
        drawControlsCard(t, f);
    }

    // The message log strip under the map; shows the newest three lines.
    void drawJournal(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
        sf::FloatRect frame = mapFrame(s);
        float top = frame.position.y + frame.size.y + 12.f;
        sf::FloatRect r{{frame.position.x, top}, {frame.size.x, theme::kH - 12.f - top}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::text(t, f.bold, "JOURNAL", 11, {x + 18.f, y + r.size.y / 2.f}, theme::dim, Align::Left, 2.2f);
        draw::rect(t, {{x + 104.f, y + 14.f}, {1.f, r.size.y - 28.f}}, theme::border);

        const DynamicArray<LogLine>& log = s.messages();
        const int lines = 3;
        float lineH = (r.size.y - 16.f) / lines;
        int start = std::max(log.size() - lines, 0);
        for (int i = start; i < log.size(); ++i) {
            int row = i - start;
            int age = log.size() - 1 - i;  // 0 = newest
            int alpha = 110;
            if (age == 0) alpha = 255;
            else if (age == 1) alpha = 175;
            float ly = y + 8.f + lineH * (row + 0.5f);
            draw::circle(t, {x + 124.f, ly}, 3.f, draw::withAlpha(logColor(log[i].kind), alpha));
            draw::text(t, f.regular, log[i].text, 13, {x + 136.f, ly}, draw::withAlpha(age == 0 ? theme::text : theme::muted, alpha));
        }
    }

private:
    void drawHeroCard(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
        const float W = theme::kSideW;
        const Player& p = s.player();
        sf::FloatRect r{{theme::kSideX, 66.f}, {W, 176.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::sectionLabel(t, f.bold, "ADVENTURER", {x + 18.f, y + 22.f});
        draw::text(t, f.bold, "LEVEL " + std::to_string(p.level), 12, {x + W - 18.f, y + 22.f}, theme::gold, Align::Right, 1.5f);

        sf::Vector2f portrait{x + 46.f, y + 76.f};
        draw::circle(t, portrait, 28.f, theme::panelHi, theme::borderHi, 1.f);
        draw::glow(t, portrait, 30.f, sf::Color(255, 190, 100, 50), 4);
        draw::playerIcon(t, portrait, 1.75f, 2);

        float bx = x + 88.f, bw = W - 106.f;
        float hpFrac = p.stats.maxHp > 0 ? h.displayHp / p.stats.maxHp : 0.f;
        float ghost = p.stats.maxHp > 0 ? h.ghostHp / p.stats.maxHp : 0.f;
        draw::text(t, f.bold, "HP", 11, {bx, y + 52.f}, theme::muted, Align::Left, 1.5f);
        draw::text(t, f.mono, std::to_string(p.stats.hp) + " / " + std::to_string(p.stats.maxHp), 13, {bx + bw, y + 52.f}, theme::text, Align::Right);
        draw::bar(t, {{bx, y + 64.f}, {bw, 12.f}}, hpFrac, draw::hpColor(hpFrac), sf::Color(8, 10, 16), ghost);

        float xpFrac = static_cast<float>(p.xp) / p.xpToNext();
        draw::text(t, f.bold, "XP", 11, {bx, y + 92.f}, theme::muted, Align::Left, 1.5f);
        draw::text(t, f.mono, std::to_string(p.xp) + " / " + std::to_string(p.xpToNext()), 12, {bx + bw, y + 92.f}, theme::muted, Align::Right);
        draw::bar(t, {{bx, y + 103.f}, {bw, 6.f}}, xpFrac, theme::xp, sf::Color(8, 10, 16));

        // Three small boxes for ATK / DEF / SPD.
        const char* labels[3] = {"ATK", "DEF", "SPD"};
        int values[3] = {p.stats.attack, p.stats.defense, p.stats.speed};
        sf::Color tints[3] = {theme::orange, theme::blue, theme::teal};
        float cw = (W - 36.f - 16.f) / 3.f;
        for (int i = 0; i < 3; ++i) {
            sf::FloatRect c{{x + 18.f + i * (cw + 8.f), y + 122.f}, {cw, 40.f}};
            draw::roundedRect(t, c, 7.f, theme::panelHi, theme::border, 1.f);
            draw::rect(t, {{c.position.x + 10.f, c.position.y + 12.f}, {3.f, 16.f}}, tints[i]);
            draw::text(t, f.bold, labels[i], 10, {c.position.x + 20.f, c.position.y + 20.f}, theme::muted, Align::Left, 1.5f);
            draw::text(t, f.bold, std::to_string(values[i]), 17, {c.position.x + cw - 12.f, c.position.y + 20.f}, theme::text, Align::Right);
        }
    }

    // Eight item slots in two rows of four. Clicking this card opens the pack.
    void drawPackCard(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
        const float W = theme::kSideW;
        const sf::FloatRect r = kPackCard;
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        const Inventory& inv = s.inventory();
        draw::sectionLabel(t, f.bold, "PACK", {x + 18.f, y + 22.f});
        draw::text(t, f.mono, std::to_string(inv.size()) + " / " + std::to_string(inv.capacity()), 12, {x + W - 44.f, y + 22.f},
                   inv.isFull() ? theme::orange : theme::muted, Align::Right);
        draw::keycap(t, f.bold, "I", {x + W - 36.f, y + 22.f}, 10);
        float slotW = (W - 36.f - 3 * 8.f) / 4.f;
        for (int i = 0; i < inv.capacity(); ++i) {
            int col = i % 4;
            int row = i / 4;
            sf::FloatRect c{{x + 18.f + col * (slotW + 8.f), y + 40.f + row * 42.f}, {slotW, 36.f}};
            bool filled = i < inv.size();
            draw::roundedRect(t, c, 6.f, filled ? theme::panelHi : sf::Color(15, 18, 27), theme::border, 1.f);
            if (filled) draw::itemIcon(t, inv.at(i).type, {c.position.x + c.size.x / 2.f, c.position.y + 19.f}, 1.05f);
        }
    }

    void drawObjectiveCard(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
        sf::FloatRect r{{theme::kSideX, 394.f}, {theme::kSideW, 104.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::sectionLabel(t, f.bold, "OBJECTIVE", {x + 18.f, y + 22.f});
        bool found = s.exitDiscovered();
        draw::portalIcon(t, {x + 32.f, y + 56.f}, 0.75f, h.time, found ? 1.f : 0.45f);
        draw::text(t, f.bold, found ? "Exit portal located" : "Find the exit portal", 14, {x + 54.f, y + 49.f}, theme::text);
        if (found) {
            float kw = draw::keycap(t, f.bold, "P", {x + 54.f, y + 70.f}, 10);
            draw::text(t, f.regular, "plot an A* route to the portal", 12, {x + 60.f + kw, y + 70.f}, theme::muted);
        } else {
            draw::text(t, f.regular, "Explore to reveal the dungeon", 12, {x + 54.f, y + 70.f}, theme::muted);
        }
        std::string foes = std::to_string(s.enemiesRemaining()) + " of " + std::to_string(s.enemies().size()) + " foes remain  ·  The Warden guards the exit";
        draw::text(t, f.regular, foes, 11, {x + 18.f, y + 90.f}, theme::dim);
    }

    // Live numbers from the data structures, for showing how they work.
    void drawAlgorithmsCard(sf::RenderTarget& t, const Fonts& f, const GameSession& s, const HudState& h) const {
        const float W = theme::kSideW;
        sf::FloatRect r{{theme::kSideX, 510.f}, {W, 158.f}};
        draw::panel(t, r);
        float x = r.position.x, y = r.position.y;
        draw::sectionLabel(t, f.bold, "ALGORITHMS", {x + 18.f, y + 22.f});

        const char* overlayName = "OFF";
        if (h.overlay == OverlayMode::Search) overlayName = "A*";
        else if (h.overlay == OverlayMode::Graph) overlayName = "GRAPH";
        float nameW = draw::textWidth(f.mono, overlayName, 11);
        draw::text(t, f.mono, overlayName, 11, {x + W - 18.f, y + 22.f}, h.overlay == OverlayMode::Off ? theme::dim : theme::teal, Align::Right);
        draw::keycap(t, f.bold, "Tab", {x + W - 30.f - nameW - 32.f, y + 22.f}, 10);

        const Dungeon& d = s.dungeon();
        const PathResult& search = s.lastSearch();
        const bool searched = s.stats().pathsPlanned > 0;
        std::string astar = "No search yet";
        sf::Color astarColor = theme::dim;
        if (searched) {
            if (search.found || s.hasPlannedPath()) {
                astar = std::to_string(search.cost) + " steps · " + std::to_string(search.expanded.size()) + " nodes";
                astarColor = theme::teal;
            } else {
                astar = "No path · " + std::to_string(search.expanded.size()) + " nodes";
                astarColor = theme::orange;
            }
        }
        std::string heap = "—";
        if (searched) heap = std::to_string(search.heapPushes) + formatNumber(" · %.2f ms", search.millis);

        struct Row {
            const char* label;
            std::string value;
            sf::Color color;
        };
        const Row rows[4] = {
            {"Graph · adjacency list", std::to_string(d.walkableCount()) + " V · " + std::to_string(d.graph().edgeCount()) + " E", theme::text},
            {"Recursive backtracking", "seed " + std::to_string(s.seed()), theme::text},
            {"A* · Manhattan", astar, astarColor},
            {"Binary heap pushes", heap, theme::text},
        };
        for (int i = 0; i < 4; ++i) {
            float ry = y + 50.f + i * 27.f;
            if (i > 0) draw::rect(t, {{x + 18.f, ry - 13.5f}, {W - 36.f, 1.f}}, sf::Color(255, 255, 255, 10));
            draw::text(t, f.regular, rows[i].label, 12, {x + 18.f, ry}, theme::muted);
            draw::text(t, f.mono, rows[i].value, 12, {x + W - 18.f, ry}, rows[i].color, Align::Right);
        }
    }

    void drawControlsCard(sf::RenderTarget& t, const Fonts& f) const {
        sf::FloatRect r{{theme::kSideX, 680.f}, {theme::kSideW, 88.f}};
        draw::panel(t, r);
        const float left = r.position.x + 18.f;
        const float row1 = r.position.y + 26.f;
        const float row2 = r.position.y + 60.f;
        float x = left;
        draw::keyHint(t, f.bold, f.regular, x, row1, "WASD", "Move", 14.f);
        draw::keyHint(t, f.bold, f.regular, x, row1, "Click", "Route", 14.f);
        draw::keyHint(t, f.bold, f.regular, x, row1, "Enter", "Walk", 14.f);
        x = left;
        draw::keyHint(t, f.bold, f.regular, x, row2, "I", "Pack", 14.f);
        draw::keyHint(t, f.bold, f.regular, x, row2, "H", "Help", 14.f);
        draw::keyHint(t, f.bold, f.regular, x, row2, "Esc", "Pause", 14.f);
        draw::keyHint(t, f.bold, f.regular, x, row2, "RMB", "Clear", 14.f);
    }
};

// ============================================================================
//  18. Inventory panel
//  A pop-up for looking at, using and discarding items. The same panel is
//  the item picker in battle. It only tracks the cursor and messages; what
//  an item actually does is decided by the game rules.
// ============================================================================
enum class InventoryMode { Explore, Battle };
enum class InventoryAction { None, Close, Use, Discard };

class InventoryPanel {
public:
    void open(InventoryMode mode) {
        mode_ = mode;
        selected_ = 0;
        hoverRow_ = -1;
        hoverButton_ = -1;
        discardArmed_ = -1;
        feedback_.clear();
        feedbackTimer_ = 0.f;
    }

    InventoryMode mode() const { return mode_; }
    int selected() const { return selected_; }

    InventoryAction handleInput(const Input& in, const Inventory& inv) {
        // Any other key cancels a half-finished discard.
        if (in.cmd != Command::Discard) discardArmed_ = -1;
        switch (in.cmd) {
            case Command::Back:
            case Command::Inventory:
                return InventoryAction::Close;
            case Command::MoveUp:
                if (selected_ > 0) --selected_;
                break;
            case Command::MoveDown:
                if (selected_ + 1 < inv.size()) ++selected_;
                break;
            case Command::Digit:
                if (in.digit >= 1 && in.digit <= inv.size()) selected_ = in.digit - 1;
                else setFeedback("Slot " + std::to_string(in.digit) + " is empty.", false);
                break;
            case Command::Use:
            case Command::Confirm:
            case Command::Step:
                return InventoryAction::Use;
            case Command::Discard:
                return requestDiscard(inv);
            default:
                break;
        }
        return InventoryAction::None;
    }

    InventoryAction handleClick(sf::Vector2f mouse, const Inventory& inv) {
        if (!kPanel.contains(mouse)) return InventoryAction::Close;  // clicked outside
        for (int i = 0; i < inv.size(); ++i) {
            if (rowRect(i).contains(mouse)) {
                selected_ = i;
                discardArmed_ = -1;
                return InventoryAction::None;
            }
        }
        if (buttonRect(kUseButton).contains(mouse)) {
            discardArmed_ = -1;
            return InventoryAction::Use;
        }
        if (buttonRect(kDiscardButton).contains(mouse)) return requestDiscard(inv);
        if (buttonRect(kCloseButton).contains(mouse)) return InventoryAction::Close;
        return InventoryAction::None;
    }

    void handleHover(sf::Vector2f mouse) {
        hoverRow_ = -1;
        hoverButton_ = -1;
        for (int i = 0; i < Inventory::kDefaultCapacity; ++i) {
            if (rowRect(i).contains(mouse)) hoverRow_ = i;
        }
        for (int i = 0; i < kButtonCount; ++i) {
            if (buttonRect(i).contains(mouse)) hoverButton_ = i;
        }
    }

    void update(float dt) {
        if (feedbackTimer_ > 0.f) feedbackTimer_ -= dt;
    }

    void setFeedback(const std::string& message, bool positive) {
        feedback_ = message;
        feedbackPositive_ = positive;
        feedbackTimer_ = message.empty() ? 0.f : 3.2f;
    }

    // Keeps the cursor on a real slot after an item was used or discarded.
    void clampSelection(const Inventory& inv) {
        if (selected_ >= inv.size()) selected_ = inv.size() - 1;
        if (selected_ < 0) selected_ = 0;
        discardArmed_ = -1;
    }

    void draw(sf::RenderTarget& t, const Fonts& f, const Inventory& inv, const Player& player) const {
        const float x = kPanel.position.x, y = kPanel.position.y, w = kPanel.size.x;
        const bool battle = mode_ == InventoryMode::Battle;

        draw::scrim(t, 185);
        draw::raisedBox(t, kPanel, 14.f, 8.f, sf::Color(0, 0, 0, 120), theme::panel, theme::borderHi, 1.f);

        // Header.
        draw::text(t, f.title, battle ? "CHOOSE AN ITEM" : "INVENTORY", 22, {x + 24.f, y + 38.f}, theme::gold, Align::Left, 1.5f);
        std::string capacity = std::to_string(inv.size()) + " / " + std::to_string(inv.capacity()) + " slots";
        draw::text(t, f.mono, capacity, 12, {x + w - 24.f, y + 38.f}, inv.isFull() ? theme::orange : theme::muted, Align::Right);
        if (battle) {
            float capacityW = draw::textWidth(f.mono, capacity, 12);
            draw::tagRight(t, f.bold, "USING AN ITEM TAKES YOUR TURN", x + w - 24.f - capacityW - 16.f, y + 38.f, theme::orange);
        }
        draw::rect(t, {{x + 24.f, y + 62.f}, {w - 48.f, 1.f}}, theme::border);

        for (int i = 0; i < inv.capacity(); ++i) drawSlot(t, f, inv, i);
        drawDetails(t, f, inv, player);
        drawFooter(t, f, inv, battle);
    }

private:
    static constexpr int kUseButton = 0;
    static constexpr int kDiscardButton = 1;
    static constexpr int kCloseButton = 2;
    static constexpr int kButtonCount = 3;
    static inline const sf::FloatRect kPanel{{250.f, 112.f}, {840.f, 556.f}};

    static sf::FloatRect rowRect(int i) { return {{kPanel.position.x + 24.f, kPanel.position.y + 78.f + i * 48.f}, {384.f, 44.f}}; }
    static sf::FloatRect detailRect() { return {{kPanel.position.x + 432.f, kPanel.position.y + 78.f}, {384.f, 380.f}}; }
    static sf::FloatRect buttonRect(int i) { return {{kPanel.position.x + 432.f + i * 131.f, kPanel.position.y + 482.f}, {122.f, 44.f}}; }

    // Discarding needs two presses so an item is not thrown away by accident.
    InventoryAction requestDiscard(const Inventory& inv) {
        if (mode_ == InventoryMode::Battle) {
            setFeedback("Items cannot be discarded during battle.", false);
            return InventoryAction::None;
        }
        // Empty pack or bad slot: let the game rules give the message.
        if (!inv.isValidIndex(selected_)) return InventoryAction::Discard;
        if (discardArmed_ == selected_) {
            discardArmed_ = -1;
            return InventoryAction::Discard;
        }
        discardArmed_ = selected_;
        setFeedback(std::string("Press X again to discard ") + itemInfo(inv.at(selected_).type).name + ".", false);
        return InventoryAction::None;
    }

    void drawSlot(sf::RenderTarget& t, const Fonts& f, const Inventory& inv, int i) const {
        const sf::FloatRect r = rowRect(i);
        const bool filled = i < inv.size();
        const bool selected = filled && i == selected_;
        const bool hovered = filled && i == hoverRow_;
        const float cy = r.position.y + r.size.y / 2.f;

        sf::Color fill = filled ? theme::panelHi : sf::Color(15, 18, 27);
        sf::Color edge = theme::border;
        if (selected) {
            fill = draw::mix(theme::panelHi, theme::gold, 0.12f);
            edge = theme::gold;
        } else if (hovered) {
            fill = theme::panelHover;
            edge = theme::borderHi;
        }
        draw::roundedRect(t, r, 8.f, fill, edge, 1.f);
        if (selected) draw::rect(t, {{r.position.x + 1.f, r.position.y + 10.f}, {3.f, r.size.y - 20.f}}, theme::gold);

        draw::text(t, f.mono, std::to_string(i + 1), 11, {r.position.x + 20.f, cy}, selected ? theme::gold : theme::dim, Align::Center);
        if (!filled) {
            draw::text(t, f.regular, "Empty slot", 12, {r.position.x + 42.f, cy}, sf::Color(70, 77, 96));
            return;
        }
        const ItemType type = inv.at(i).type;
        const ItemInfo& info = itemInfo(type);
        draw::itemIcon(t, type, {r.position.x + 54.f, cy}, 1.15f);
        draw::text(t, f.bold, info.name, 14, {r.position.x + 80.f, r.position.y + 15.f}, selected ? theme::text : sf::Color(205, 209, 220));
        draw::text(t, f.regular, info.effect, 11, {r.position.x + 80.f, r.position.y + 31.f}, theme::muted);
        if (info.battleOnly) draw::tag(t, f.bold, "BATTLE", {r.position.x + r.size.x - 44.f, cy}, theme::orange);
    }

    // Right-hand pane: a big picture of the selected item and what it does.
    void drawDetails(sf::RenderTarget& t, const Fonts& f, const Inventory& inv, const Player& player) const {
        const sf::FloatRect d = detailRect();
        draw::roundedRect(t, d, 10.f, sf::Color(15, 18, 27), theme::border, 1.f);
        const float centerX = d.position.x + d.size.x / 2.f;
        const float dx = d.position.x, dy = d.position.y;

        if (inv.isValidIndex(selected_)) {
            const ItemType type = inv.at(selected_).type;
            const ItemInfo& info = itemInfo(type);
            const sf::Vector2f iconPos{centerX, dy + 76.f};
            draw::glow(t, iconPos, 74.f, draw::withAlpha(draw::itemColor(type), 70), 5);
            draw::circle(t, iconPos, 46.f, theme::panelHi, theme::borderHi, 1.f, 48);
            draw::itemIcon(t, type, {iconPos.x, iconPos.y + 1.f}, 3.0f);

            draw::text(t, f.title, info.name, 20, {centerX, dy + 152.f}, theme::text, Align::Center);
            if (info.battleOnly) draw::tag(t, f.bold, "BATTLE ONLY", {centerX, dy + 180.f}, theme::orange);
            else draw::tag(t, f.bold, "USE ANYWHERE", {centerX, dy + 180.f}, theme::green);
            draw::wrapped(t, f.regular, info.description, 13, {dx + 32.f, dy + 200.f}, d.size.x - 64.f, theme::muted, 20.f, Align::Center);

            draw::rect(t, {{dx + 28.f, dy + 256.f}, {d.size.x - 56.f, 1.f}}, theme::border);
            draw::sectionLabel(t, f.bold, "EFFECT", {dx + 28.f, dy + 278.f});
            draw::text(t, f.bold, info.effect, 15, {dx + 28.f, dy + 302.f}, draw::itemColor(type));
        } else {
            const sf::Vector2f iconPos{centerX, dy + 110.f};
            draw::circle(t, iconPos, 46.f, theme::panelHi, theme::border, 1.f, 48);
            draw::itemIcon(t, ItemType::HealthPotion, iconPos, 2.6f, 0.22f);
            draw::text(t, f.bold, "Your pack is empty", 16, {centerX, dy + 190.f}, theme::text, Align::Center);
            draw::wrapped(t, f.regular, "Walk over potions and relics in the dungeon to collect them.", 13, {dx + 56.f, dy + 208.f},
                          d.size.x - 112.f, theme::muted, 20.f, Align::Center);
        }

        const float hpFrac = player.stats.maxHp > 0 ? static_cast<float>(player.stats.hp) / player.stats.maxHp : 0.f;
        draw::sectionLabel(t, f.bold, "YOUR HEALTH", {dx + 28.f, dy + 336.f});
        draw::text(t, f.mono, std::to_string(player.stats.hp) + " / " + std::to_string(player.stats.maxHp), 12,
                   {dx + d.size.x - 28.f, dy + 336.f}, theme::text, Align::Right);
        draw::bar(t, {{dx + 28.f, dy + 350.f}, {d.size.x - 56.f, 10.f}}, hpFrac, draw::hpColor(hpFrac), sf::Color(8, 10, 16));
    }

    // Bottom row: the last message (or key hints), then the three buttons.
    void drawFooter(sf::RenderTarget& t, const Fonts& f, const Inventory& inv, bool battle) const {
        const float x = kPanel.position.x;
        const float fy = kPanel.position.y + 504.f;
        if (feedbackTimer_ > 0.f && !feedback_.empty()) {
            int alpha = fadeOutAlpha(feedbackTimer_);
            sf::Color dot = draw::withAlpha(feedbackPositive_ ? theme::green : theme::orange, alpha);
            draw::circle(t, {x + 30.f, fy}, 3.5f, dot);
            // Long messages wrap onto two lines, so start them higher.
            float top = fy - (draw::textWidth(f.regular, feedback_, 13) > 360.f ? 19.f : 10.f);
            draw::wrapped(t, f.regular, feedback_, 13, {x + 42.f, top}, 362.f, draw::withAlpha(theme::text, alpha), 19.f);
        } else {
            float hx = x + 24.f;
            draw::keyHint(t, f.bold, f.regular, hx, fy, "W", "");
            hx += 4.f;
            draw::keyHint(t, f.bold, f.regular, hx, fy, "S", "Select");
            draw::keyHint(t, f.bold, f.regular, hx, fy, "1-8", "Jump to slot");
        }

        const bool canAct = inv.isValidIndex(selected_);
        const bool armed = discardArmed_ >= 0;
        draw::button(t, f.bold, buttonRect(kUseButton), "Use", {hoverButton_ == kUseButton, false, canAct}, "U", theme::gold);
        draw::button(t, f.bold, buttonRect(kDiscardButton), armed ? "Confirm" : "Discard", {hoverButton_ == kDiscardButton, armed, canAct && !battle},
                     "X", theme::red);
        draw::button(t, f.bold, buttonRect(kCloseButton), battle ? "Back" : "Close", {hoverButton_ == kCloseButton, false, true}, "Esc", theme::gold);
    }

    InventoryMode mode_ = InventoryMode::Explore;
    int selected_ = 0;
    int hoverRow_ = -1;
    int hoverButton_ = -1;
    int discardArmed_ = -1;  // slot waiting for a second "discard" press
    std::string feedback_;
    bool feedbackPositive_ = false;
    float feedbackTimer_ = 0.f;
};

// ============================================================================
//  19. Battle screen
//  Turns key presses and clicks into BattleAction requests, waits a moment
//  before each enemy turn so it can be read, and animates the results. The
//  strip at the top is a live view of the binary-heap turn queue.
// ============================================================================
enum class BattleUiEvent { None, OpenItems, Finished };

class BattleScreen {
public:
    void begin(const GameSession& s) {
        selected_ = 0;
        hover_ = -1;
        time_ = 0.f;
        intro_ = 0.f;
        lock_ = 0.f;
        enemyTimer_ = 0.f;
        overTimer_ = 0.f;
        for (int i = 0; i < 2; ++i) {
            shake_[i] = 0.f;
            flash_[i] = 0.f;
            lunge_[i] = 0.f;
        }
        feedback_.clear();
        feedbackTimer_ = 0.f;
        floaters_.clear();

        const BattleSystem& b = s.battle();
        hpShown_[0] = b.player() ? static_cast<float>(b.player()->stats.hp) : 0.f;
        hpShown_[1] = b.enemy() ? static_cast<float>(b.enemy()->stats.hp) : 0.f;
        hpGhost_[0] = hpShown_[0];
        hpGhost_[1] = hpShown_[1];
    }

    BattleUiEvent handleInput(const Input& in, GameSession& s) {
        if (s.battle().isOver()) {
            bool confirm = in.cmd == Command::Confirm || in.cmd == Command::Step;
            if (confirm && canLeave()) return BattleUiEvent::Finished;
            return BattleUiEvent::None;
        }
        // The four actions sit in a 2 x 2 grid: 0 1 / 2 3.
        switch (in.cmd) {
            case Command::MoveLeft:
                if (selected_ % 2 == 1) --selected_;
                break;
            case Command::MoveRight:
                if (selected_ % 2 == 0) ++selected_;
                break;
            case Command::MoveUp:
                if (selected_ >= 2) selected_ -= 2;
                break;
            case Command::MoveDown:
                if (selected_ < 2) selected_ += 2;
                break;
            case Command::Digit:
                if (in.digit >= 1 && in.digit <= 4) {
                    selected_ = in.digit - 1;
                    return activate(selected_, s);
                }
                break;
            case Command::Confirm:
            case Command::Step:
                return activate(selected_, s);
            case Command::Inventory:
                selected_ = kItem;
                return activate(kItem, s);
            default:
                break;
        }
        return BattleUiEvent::None;
    }

    BattleUiEvent handleClick(sf::Vector2f mouse, GameSession& s) {
        if (s.battle().isOver()) return canLeave() ? BattleUiEvent::Finished : BattleUiEvent::None;
        for (int i = 0; i < 4; ++i) {
            if (actionRect(i).contains(mouse)) {
                selected_ = i;
                return activate(i, s);
            }
        }
        return BattleUiEvent::None;
    }

    void handleHover(sf::Vector2f mouse) {
        hover_ = -1;
        for (int i = 0; i < 4; ++i) {
            if (actionRect(i).contains(mouse)) hover_ = i;
        }
    }

    void update(float dt, GameSession& s) {
        time_ += dt;
        intro_ = clamp01(intro_ + dt * 2.6f);
        if (lock_ > 0.f) lock_ -= dt;
        if (feedbackTimer_ > 0.f) feedbackTimer_ -= dt;
        for (int i = 0; i < 2; ++i) {
            if (shake_[i] > 0.f) shake_[i] -= dt;
            if (flash_[i] > 0.f) flash_[i] -= dt;
            if (lunge_[i] > 0.f) lunge_[i] -= dt;
        }
        for (int i = floaters_.size() - 1; i >= 0; --i) {
            floaters_[i].age += dt;
            if (floaters_[i].age >= floaters_[i].life) floaters_.removeAt(i);
        }

        BattleSystem& b = s.battle();
        if (!b.active() || !b.player() || !b.enemy()) return;
        consumeFx(b);

        // Health bars slide toward the real value; a pale "ghost" trails behind.
        const float target[2] = {static_cast<float>(b.player()->stats.hp), static_cast<float>(b.enemy()->stats.hp)};
        const float maxHp[2] = {static_cast<float>(b.player()->stats.maxHp), static_cast<float>(b.enemy()->stats.maxHp)};
        for (int i = 0; i < 2; ++i) {
            hpShown_[i] += (target[i] - hpShown_[i]) * clamp01(dt * 12.f);
            if (hpGhost_[i] > hpShown_[i]) {
                hpGhost_[i] -= dt * maxHp[i] * 0.4f;
                if (hpGhost_[i] < hpShown_[i]) hpGhost_[i] = hpShown_[i];
            } else {
                hpGhost_[i] = hpShown_[i];
            }
        }

        if (b.isOver()) {
            overTimer_ += dt;
        } else if (b.phase() == BattlePhase::EnemyTurn) {
            // The enemy acts on its own after a short pause.
            enemyTimer_ += dt;
            if (enemyTimer_ >= kEnemyDelay && lock_ <= 0.f) {
                enemyTimer_ = 0.f;
                b.enemyAct();
                lock_ = 0.3f;
            }
        } else {
            enemyTimer_ = 0.f;
        }
    }

    void draw(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
        const BattleSystem& b = s.battle();
        draw::backdrop(t);
        if (!b.player() || !b.enemy()) return;

        drawTopBar(t, f, b);
        drawTurnOrder(t, f, b);
        drawArena(t, f, b);
        drawActions(t, f, s);
        drawLog(t, f, b);
        if (b.isOver()) drawResult(t, f, b);
    }

    void setFeedback(const std::string& message) {
        feedback_ = message;
        feedbackTimer_ = message.empty() ? 0.f : 3.f;
    }

    // Call after any accepted player action (including items used from the pack).
    void onPlayerActed() {
        lock_ = 0.4f;
        enemyTimer_ = 0.f;
        feedbackTimer_ = 0.f;
    }

private:
    struct Floater {  // a floating damage number or word
        sf::Vector2f pos{0.f, 0.f};
        std::string text;
        sf::Color color = sf::Color::White;
        float age = 0.f;
        float life = 1.f;
        unsigned size = 22;
    };

    static constexpr int kAttack = 0, kDefend = 1, kItem = 2, kEscape = 3;
    // Animation lengths in seconds.
    static constexpr float kLunge = 0.32f;
    static constexpr float kShake = 0.36f;
    static constexpr float kFlash = 0.26f;
    static constexpr float kEnemyDelay = 0.85f;
    // The result card must be visible this long before it can be dismissed.
    static constexpr float kLeaveDelay = 0.45f;

    static inline const sf::FloatRect kTurns{{16.f, 68.f}, {1308.f, 74.f}};
    static inline const sf::FloatRect kArena{{16.f, 154.f}, {1308.f, 372.f}};
    static inline const sf::FloatRect kActions{{16.f, 538.f}, {660.f, 230.f}};
    static inline const sf::FloatRect kLog{{688.f, 538.f}, {636.f, 230.f}};
    // Where the player (0) and the enemy (1) stand.
    static inline const sf::Vector2f kAnchor[2] = {{360.f, 398.f}, {980.f, 398.f}};

    static sf::FloatRect actionRect(int i) { return {{34.f + (i % 2) * 318.f, 578.f + (i / 2) * 90.f}, {306.f, 78.f}}; }

    static sf::FloatRect plateRect(int who) {
        if (who == 0) return {{40.f, 172.f}, {420.f, 96.f}};
        return {{880.f, 172.f}, {420.f, 96.f}};
    }

    bool canLeave() const { return overTimer_ > kLeaveDelay; }

    BattleUiEvent activate(int action, GameSession& s) {
        BattleSystem& b = s.battle();
        if (b.isOver()) return canLeave() ? BattleUiEvent::Finished : BattleUiEvent::None;
        if (b.phase() != BattlePhase::PlayerTurn) {
            setFeedback("Wait for your turn.");
            return BattleUiEvent::None;
        }
        if (lock_ > 0.f) return BattleUiEvent::None;  // still animating

        ActionResult r;
        switch (action) {
            case kAttack: r = b.playerAct(BattleAction::Attack); break;
            case kDefend: r = b.playerAct(BattleAction::Defend); break;
            case kItem:
                if (!s.inventory().isEmpty()) return BattleUiEvent::OpenItems;
                // With an empty pack the battle rules reject the action
                // without using up the turn.
                r = b.playerAct(BattleAction::UseItem, -1);
                break;
            case kEscape: r = b.playerAct(BattleAction::Escape); break;
            default: return BattleUiEvent::None;
        }
        if (r.accepted) onPlayerActed();
        else setFeedback(r.feedback);
        return BattleUiEvent::None;
    }

    void spawnFloater(int who, const std::string& text, sf::Color color, unsigned size) {
        Floater fl;
        // Stack up to three at different heights so they do not overlap.
        float offsetX = who == 0 ? 34.f : -34.f;
        fl.pos = {kAnchor[who].x + offsetX, kAnchor[who].y - 84.f - 18.f * (floaters_.size() % 3)};
        fl.text = text;
        fl.color = color;
        fl.size = size;
        fl.life = 1.15f;
        floaters_.push_back(fl);
    }

    // Turns the battle's effect queue into animations.
    void consumeFx(BattleSystem& b) {
        BattleFx fx;
        while (b.popFx(fx)) {
            const int who = fx.target == Actor::Player ? 0 : 1;
            switch (fx.type) {
                case BattleFx::Type::Damage: {
                    sf::Color color = who == 0 ? theme::red : theme::text;
                    if (fx.critical) color = theme::gold;
                    spawnFloater(who, "-" + std::to_string(fx.amount), color, fx.critical ? 34u : 26u);
                    shake_[who] = kShake;
                    flash_[who] = kFlash;
                    lunge_[1 - who] = kLunge;  // the attacker jumps forward
                    break;
                }
                case BattleFx::Type::Heal: spawnFloater(who, "+" + std::to_string(fx.amount), theme::green, 26u); break;
                case BattleFx::Type::Guard: spawnFloater(who, "GUARD", theme::blue, 18u); break;
                case BattleFx::Type::Miss: spawnFloater(who, "NO ESCAPE", theme::orange, 18u); break;
                case BattleFx::Type::Buff: spawnFloater(who, "POWER UP", theme::violet, 18u); break;
            }
        }
    }

    void drawTopBar(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
        draw::brandBar(t, f, time_);
        const Enemy& e = *b.enemy();
        const EnemyInfo& info = enemyInfo(e.kind);
        const std::string label = e.boss ? "BOSS BATTLE" : "ENCOUNTER";
        float tagW = draw::tagRight(t, f.bold, label, theme::kW - 16.f, 28.f, e.boss ? theme::violet : theme::red);
        draw::text(t, f.regular, std::string(info.name) + "  ·  " + info.title, 13, {theme::kW - 28.f - tagW, 28.f}, theme::muted, Align::Right);
    }

    void drawTurnOrder(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
        draw::panel(t, kTurns);
        draw::sectionLabel(t, f.bold, "TURN ORDER", {34.f, 94.f});
        draw::text(t, f.regular, "Binary min-heap  ·  key = next action time", 11, {34.f, 118.f}, theme::dim);

        DynamicArray<TurnEvent> order = b.upcoming(8);
        if (order.empty()) {
            draw::text(t, f.regular, "The turn queue is empty: the battle is over.", 13, {300.f, 105.f}, theme::muted);
            return;
        }
        const EnemyKind kind = b.enemy()->kind;
        const std::string enemyName = toUpper(b.enemy()->name());
        for (int i = 0; i < order.size(); ++i) {
            sf::FloatRect r{{286.f + i * 128.f, 82.f}, {118.f, 46.f}};
            const bool isPlayer = order[i].actor == Actor::Player;
            const bool actingNow = i == 0;
            const sf::Color accent = isPlayer ? theme::gold : draw::enemyColor(kind);
            const float cy = r.position.y + r.size.y / 2.f;

            sf::Color fill = actingNow ? draw::mix(theme::panelHi, accent, 0.2f) : theme::panelHi;
            draw::roundedRect(t, r, 8.f, fill, actingNow ? accent : theme::border, 1.f);
            if (i > 0) {  // a ">" between chips
                const float ax = r.position.x - 7.f;
                draw::line(t, {ax - 2.f, cy - 4.f}, {ax + 2.f, cy}, theme::dim, 1.5f);
                draw::line(t, {ax + 2.f, cy}, {ax - 2.f, cy + 4.f}, theme::dim, 1.5f);
            }
            float iconAlpha = actingNow ? 1.f : 0.75f;
            if (isPlayer) draw::playerIcon(t, {r.position.x + 23.f, cy}, 0.8f, 1, iconAlpha);
            else draw::enemyIcon(t, kind, {r.position.x + 23.f, cy + 1.f}, 0.62f, time_, iconAlpha);

            draw::text(t, f.bold, isPlayer ? "YOU" : enemyName, 11, {r.position.x + 44.f, r.position.y + 16.f}, actingNow ? theme::text : theme::muted,
                       Align::Left, 1.2f);
            std::string when = actingNow ? "ACTING" : "t = " + formatNumber("%.1f", order[i].time);
            draw::text(t, f.mono, when, 10, {r.position.x + 44.f, r.position.y + 32.f}, actingNow ? accent : theme::dim);
        }
    }

    // Name, HP bar and stats for the player (who = 0) or the enemy (who = 1).
    void drawNameplate(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b, int who) const {
        const sf::FloatRect r = plateRect(who);
        const Stats& st = who == 0 ? b.player()->stats : b.enemy()->stats;
        const bool guarding = who == 0 ? b.playerDefending() : b.enemyDefending();
        const sf::Color accent = who == 0 ? theme::gold : draw::enemyColor(b.enemy()->kind);
        const float x = r.position.x, y = r.position.y, w = r.size.x;

        draw::raisedBox(t, r, 10.f, 3.f, sf::Color(0, 0, 0, 80), sf::Color(19, 23, 34, 240), theme::border, 1.f);
        draw::rect(t, {{x + 1.f, y + 14.f}, {3.f, r.size.y - 28.f}}, accent);

        const std::string name = who == 0 ? "Adventurer" : b.enemy()->name();
        const std::string sub = who == 0 ? "Level " + std::to_string(b.player()->level) : enemyInfo(b.enemy()->kind).title;
        draw::text(t, f.bold, name, 16, {x + 20.f, y + 23.f}, theme::text);
        draw::text(t, f.regular, sub, 12, {x + 30.f + draw::textWidth(f.bold, name, 16), y + 24.f}, theme::muted);
        draw::text(t, f.mono, std::to_string(st.hp) + " / " + std::to_string(st.maxHp), 13, {x + w - 20.f, y + 23.f}, theme::text, Align::Right);

        const float frac = st.maxHp > 0 ? hpShown_[who] / st.maxHp : 0.f;
        const float ghost = st.maxHp > 0 ? hpGhost_[who] / st.maxHp : 0.f;
        draw::bar(t, {{x + 20.f, y + 41.f}, {w - 40.f, 14.f}}, frac, draw::hpColor(frac), sf::Color(8, 10, 16), ghost);

        std::string statLine = "ATK " + std::to_string(st.attack) + "    DEF " + std::to_string(st.defense) + "    SPD " + std::to_string(st.speed);
        draw::text(t, f.mono, statLine, 11, {x + 20.f, y + 76.f}, theme::muted);
        // Tags line up from the right edge.
        float right = x + w - 20.f;
        if (guarding) right -= draw::tagRight(t, f.bold, "GUARDING", right, y + 76.f, theme::blue) + 8.f;
        if (who == 1 && b.enemy()->boss) draw::tagRight(t, f.bold, "BOSS", right, y + 76.f, theme::violet);
    }

    void drawArena(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
        draw::panel(t, kArena, sf::Color(14, 17, 26));
        const float ax = kArena.position.x + 4.f, ay = kArena.position.y + 4.f;
        const float aw = kArena.size.x - 8.f, ah = kArena.size.y - 8.f;
        const float horizon = 432.f;
        draw::gradientRect(t, {{ax, ay}, {aw, horizon - ay}}, sf::Color(14, 17, 27), sf::Color(22, 26, 41));
        draw::gradientRect(t, {{ax, horizon}, {aw, ay + ah - horizon}}, sf::Color(30, 35, 52), sf::Color(18, 21, 32));
        draw::rect(t, {{ax, horizon}, {aw, 1.f}}, sf::Color(255, 255, 255, 14));

        const EnemyKind kind = b.enemy()->kind;
        const bool dead[2] = {b.player()->stats.hp <= 0, b.enemy()->stats.hp <= 0};
        const bool guarding[2] = {b.playerDefending(), b.enemyDefending()};
        const float enter = easeOut(intro_);

        draw::glow(t, {kAnchor[0].x, kAnchor[0].y - 20.f}, 190.f, sf::Color(255, 190, 110, 34), 6);
        draw::glow(t, {kAnchor[1].x, kAnchor[1].y - 20.f}, 210.f, draw::withAlpha(draw::enemyColor(kind), 40), 6);

        // "VS" and the turn counter in the middle.
        draw::text(t, f.title, "VS", 26, {670.f, 208.f}, sf::Color(74, 82, 104), Align::Center, 2.f);
        int turnNumber = b.round() + (b.isOver() ? 0 : 1);
        draw::text(t, f.mono, "TURN " + std::to_string(turnNumber), 11, {670.f, 238.f}, theme::dim, Align::Center);

        for (int who = 0; who < 2; ++who) {
            const float facing = who == 0 ? 1.f : -1.f;  // the player faces right
            const sf::Vector2f base = kAnchor[who];
            sf::Vector2f pos = base;
            pos.x -= facing * (1.f - enter) * 140.f;  // slide in from the side
            if (lunge_[who] > 0.f) pos.x += facing * 76.f * std::sin(kPi * (1.f - lunge_[who] / kLunge));
            if (shake_[who] > 0.f) pos.x += std::sin(time_ * 85.f) * 9.f * (shake_[who] / kShake);
            float alpha = enter * (dead[who] ? 0.22f : 1.f);
            if (dead[who]) pos.y += 14.f;

            // Platform under the fighter.
            draw::ellipse(t, {base.x, base.y + 62.f}, 104.f, 22.f, sf::Color(0, 0, 0, 120));
            draw::ellipse(t, {base.x, base.y + 58.f}, 96.f, 18.f, sf::Color(40, 46, 66));
            draw::ellipse(t, {base.x, base.y + 56.f}, 88.f, 15.f, sf::Color(52, 59, 82));

            if (who == 0) {
                draw::playerIcon(t, pos, 4.8f, 1, alpha);
            } else {
                const float bob = dead[1] ? 0.f : std::sin(time_ * 2.2f) * 4.f;
                const bool boss = b.enemy()->boss;
                draw::enemyIcon(t, kind, {pos.x, pos.y + bob - (boss ? 0.f : 4.f)}, boss ? 4.6f : 5.0f, time_, alpha);
            }
            if (flash_[who] > 0.f) {
                draw::glow(t, pos, 96.f, sf::Color(255, 255, 255, static_cast<std::uint8_t>(210.f * flash_[who] / kFlash)), 5);
            }
            if (guarding[who] && !dead[who]) {
                const float pulse = 0.5f + 0.5f * std::sin(time_ * 4.f);
                draw::ring(t, pos, 70.f + 3.f * pulse, 3.f, draw::withAlpha(theme::blue, 150 + static_cast<int>(70 * pulse)));
                draw::ring(t, pos, 78.f + 3.f * pulse, 1.5f, draw::withAlpha(theme::blue, 70));
            }
            drawNameplate(t, f, b, who);
        }

        // Floating numbers rise, then fade out over the last 35% of their life.
        for (const Floater& fl : floaters_) {
            const float k = fl.age / fl.life;
            const float y = fl.pos.y - 64.f * easeOut(k);
            const int alpha = k < 0.65f ? 255 : static_cast<int>(255.f * (1.f - (k - 0.65f) / 0.35f));
            draw::text(t, f.bold, fl.text, fl.size, {fl.pos.x, y + 2.f}, sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha * 0.7f)), Align::Center);
            draw::text(t, f.bold, fl.text, fl.size, {fl.pos.x, y}, draw::withAlpha(fl.color, alpha), Align::Center);
        }
    }

    // The small picture on each action button.
    void drawActionIcon(sf::RenderTarget& t, int action, sf::Vector2f c, sf::Color tint, float alpha) const {
        switch (action) {
            case kAttack:  // sword
                draw::line(t, {c.x - 9.f, c.y + 9.f}, {c.x + 10.f, c.y - 10.f}, tint, 3.2f);
                draw::line(t, {c.x - 9.f, c.y + 1.f}, {c.x - 1.f, c.y + 9.f}, tint, 3.2f);
                break;
            case kDefend: draw::itemIcon(t, ItemType::IronSkin, {c.x, c.y - 1.f}, 1.3f, alpha); break;
            case kItem: draw::itemIcon(t, ItemType::HealthPotion, {c.x, c.y + 1.f}, 1.25f, alpha); break;
            default:  // escape: double arrow
                for (float off : {-8.f, 1.f}) {
                    draw::line(t, {c.x + off, c.y - 7.f}, {c.x + off + 7.f, c.y}, tint, 2.6f);
                    draw::line(t, {c.x + off + 7.f, c.y}, {c.x + off, c.y + 7.f}, tint, 2.6f);
                }
                break;
        }
    }

    void drawActions(sf::RenderTarget& t, const Fonts& f, const GameSession& s) const {
        const BattleSystem& b = s.battle();
        const bool playerTurn = b.phase() == BattlePhase::PlayerTurn;
        draw::panel(t, kActions);

        const float headerY = kActions.position.y + 21.f;
        if (b.isOver()) {
            draw::sectionLabel(t, f.bold, "BATTLE OVER", {34.f, headerY}, theme::muted);
        } else if (playerTurn) {
            draw::sectionLabel(t, f.bold, "YOUR MOVE", {34.f, headerY}, theme::gold);
        } else {
            const std::string dots(1 + static_cast<int>(time_ * 3.f) % 3, '.');
            draw::sectionLabel(t, f.bold, toUpper(b.enemy()->name()) + " IS ACTING " + dots, {34.f, headerY}, theme::red);
        }
        const float right = kActions.position.x + kActions.size.x - 18.f;
        if (feedbackTimer_ > 0.f && !feedback_.empty()) {
            draw::text(t, f.regular, feedback_, 12, {right, headerY}, draw::withAlpha(theme::orange, fadeOutAlpha(feedbackTimer_)), Align::Right);
        } else if (!b.isOver()) {
            draw::text(t, f.regular, "1-4 or arrows + Enter", 11, {right, headerY}, theme::dim, Align::Right);
        }

        // Subtitles under each action.
        int lo = 0, hi = 0;
        b.damageRange(b.player()->stats, b.enemy()->stats, lo, hi);
        if (b.enemyDefending()) {
            lo = std::max(lo / 2, 1);
            hi = std::max(hi / 2, 1);
        }
        std::string attackSub = "Deal " + std::to_string(lo) + "-" + std::to_string(hi) + " damage";
        if (b.enemyDefending()) attackSub += " (enemy guarding)";

        const int items = s.inventory().size();
        std::string itemSub = "Your pack is empty";
        if (items == 1) itemSub = "1 item in your pack";
        else if (items > 1) itemSub = std::to_string(items) + " items in your pack";

        std::string escapeSub = "Impossible in a boss fight";
        if (b.canEscape()) escapeSub = std::to_string(static_cast<int>(b.escapeChance() * 100.0 + 0.5)) + "% chance to flee";

        const char* titles[4] = {"Attack", "Defend", "Item", "Escape"};
        const std::string subs[4] = {attackSub, "Halve damage until your next turn", itemSub, escapeSub};
        const sf::Color tints[4] = {theme::orange, theme::blue, theme::red, theme::teal};

        for (int i = 0; i < 4; ++i) {
            const sf::FloatRect r = actionRect(i);
            const bool selected = playerTurn && i == selected_;
            const bool hovered = playerTurn && i == hover_;
            const float cy = r.position.y + r.size.y / 2.f;

            sf::Color fill = theme::panelHi;
            sf::Color edge = theme::border;
            if (!playerTurn) {
                fill = sf::Color(17, 20, 30);
            } else if (selected) {
                fill = draw::mix(theme::panelHi, theme::gold, 0.14f);
                edge = theme::gold;
            } else if (hovered) {
                fill = theme::panelHover;
                edge = theme::borderHi;
            }
            draw::raisedBox(t, r, 9.f, 2.f, sf::Color(0, 0, 0, 70), fill, edge, 1.f);

            // Dimmed while it is not your turn.
            const float a = playerTurn ? 1.f : 0.4f;
            const sf::Vector2f iconPos{r.position.x + 42.f, cy};
            draw::circle(t, iconPos, 25.f, sf::Color(13, 16, 24), draw::withAlpha(tints[i], static_cast<int>(110 * a)), 1.f, 40);
            drawActionIcon(t, i, iconPos, draw::withAlpha(tints[i], static_cast<int>(255 * a)), a);

            draw::text(t, f.bold, titles[i], 15, {r.position.x + 80.f, r.position.y + 29.f}, playerTurn ? theme::text : theme::dim);
            draw::text(t, f.regular, subs[i], 12, {r.position.x + 80.f, r.position.y + 51.f}, playerTurn ? theme::muted : theme::dim);
            draw::keycap(t, f.bold, std::to_string(i + 1), {r.position.x + r.size.x - 36.f, r.position.y + 20.f}, 10);
        }
    }

    void drawLog(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
        draw::panel(t, kLog);
        const float x = kLog.position.x, y = kLog.position.y;
        draw::sectionLabel(t, f.bold, "COMBAT LOG", {x + 18.f, y + 21.f});

        // The heap's backing array exactly as stored (index 0 is the root).
        std::string heap;
        for (const TurnEvent& ev : b.queue().raw()) {
            if (!heap.empty()) heap += "  ";
            heap += std::string(ev.actor == Actor::Player ? "YOU" : "FOE") + "@" + formatNumber("%.1f", ev.time);
        }
        if (heap.empty()) heap = "empty";
        draw::text(t, f.mono, "heap [ " + heap + " ]", 11, {x + kLog.size.x - 18.f, y + 21.f}, theme::teal, Align::Right);
        draw::rect(t, {{x + 18.f, y + 38.f}, {kLog.size.x - 36.f, 1.f}}, theme::border);

        // The newest 7 lines; older lines are fainter.
        const DynamicArray<LogLine>& log = b.log();
        const int rows = std::min(log.size(), 7);
        const int start = log.size() - rows;
        const int alphaByAge[4] = {255, 200, 160, 120};
        for (int row = 0; row < rows; ++row) {
            const LogLine& line = log[start + row];
            const int age = rows - 1 - row;  // 0 = newest
            const int alpha = alphaByAge[std::min(age, 3)];
            const float ly = y + 58.f + row * 25.f;
            draw::circle(t, {x + 24.f, ly}, 3.f, draw::withAlpha(logColor(line.kind), alpha));
            draw::text(t, f.regular, line.text, 13, {x + 38.f, ly}, draw::withAlpha(age == 0 ? theme::text : theme::muted, alpha));
        }
    }

    // The VICTORY / ESCAPED / DEFEATED card over the arena.
    void drawResult(sf::RenderTarget& t, const Fonts& f, const BattleSystem& b) const {
        const float k = easeOut(overTimer_ / 0.4f);  // fades and slides in
        std::string title, sub;
        sf::Color accent = theme::gold;
        const std::string enemy = b.enemy()->name();
        switch (b.phase()) {
            case BattlePhase::Won:
                title = "VICTORY";
                sub = "The " + enemy + " falls.  +" + std::to_string(enemyInfo(b.enemy()->kind).xp) + " XP";
                break;
            case BattlePhase::Escaped:
                title = "ESCAPED";
                sub = "You slip away. The " + enemy + " still lurks in the dark.";
                accent = theme::teal;
                break;
            default:
                title = "DEFEATED";
                sub = "The " + enemy + " strikes you down.";
                accent = theme::red;
                break;
        }

        sf::FloatRect arenaInner{{kArena.position.x + 1.f, kArena.position.y + 1.f}, {kArena.size.x - 2.f, kArena.size.y - 2.f}};
        draw::roundedRect(t, arenaInner, 10.f, sf::Color(6, 8, 13, static_cast<std::uint8_t>(120.f * k)));
        const float cy = 340.f + (1.f - k) * 16.f;
        const int a = static_cast<int>(255.f * k);
        sf::FloatRect r{{410.f, cy - 84.f}, {520.f, 168.f}};
        draw::raisedBox(t, r, 14.f, 6.f, sf::Color(0, 0, 0, static_cast<std::uint8_t>(110.f * k)), sf::Color(19, 23, 34, static_cast<std::uint8_t>(248.f * k)),
                        draw::withAlpha(accent, a), 1.5f);
        draw::text(t, f.title, title, 38, {670.f, cy - 36.f}, draw::withAlpha(accent, a), Align::Center, 3.f);
        draw::text(t, f.regular, sub, 14, {670.f, cy + 8.f}, draw::withAlpha(theme::muted, a), Align::Center);

        if (canLeave()) {
            const float labelW = draw::textWidth(f.regular, "Continue", 13);
            const float keyW = draw::textWidth(f.bold, "Enter", 11) + 14.f;
            const float left = 670.f - (keyW + 8.f + labelW) / 2.f;
            draw::keycap(t, f.bold, "Enter", {left, cy + 50.f}, 11);
            draw::text(t, f.regular, "Continue", 13, {left + keyW + 8.f, cy + 50.f}, theme::text);
        }
    }

    int selected_ = kAttack;
    int hover_ = -1;
    float time_ = 0.f;
    float intro_ = 0.f;       // 0..1 entrance animation
    float lock_ = 0.f;        // ignore input while an action animates
    float enemyTimer_ = 0.f;  // pause before the enemy acts
    float overTimer_ = 0.f;   // time since the battle ended
    // Per fighter (0 = player, 1 = enemy):
    float shake_[2] = {0.f, 0.f};
    float flash_[2] = {0.f, 0.f};
    float lunge_[2] = {0.f, 0.f};
    float hpShown_[2] = {0.f, 0.f};
    float hpGhost_[2] = {0.f, 0.f};
    std::string feedback_;
    float feedbackTimer_ = 0.f;
    DynamicArray<Floater> floaters_;
};

// ============================================================================
//  20. Menus and full-screen pages
//  A menu widget that works with keyboard and mouse, and the title, pause,
//  help and victory / defeat screens built around it.
// ============================================================================
namespace menu_id {
enum Id { NewGame, PlaySeed, HowToPlay, Quit, Resume, Restart, NewDungeon, MainMenu };
}

struct MenuItem {
    std::string label;
    std::string hint;
    int id = 0;
};

class Menu {
public:
    void clear() {
        items_.clear();
        selected_ = 0;
    }

    void add(const std::string& label, const std::string& hint, int id) { items_.push_back({label, hint, id}); }

    void setLayout(sf::Vector2f topLeft, float width, float itemHeight = 46.f, float gap = 8.f) {
        origin_ = topLeft;
        width_ = width;
        itemHeight_ = itemHeight;
        gap_ = gap;
    }

    void setHint(int id, const std::string& hint) {
        int i = indexOf(id);
        if (i >= 0) items_[i].hint = hint;
    }

    void select(int index) {
        if (index >= 0 && index < items_.size()) selected_ = index;
    }

    // Each of these returns the id of the chosen item, or -1.
    int handleInput(const Input& in) {
        if (items_.empty()) return -1;
        const int n = items_.size();
        switch (in.cmd) {
            case Command::MoveUp:
                selected_ = (selected_ + n - 1) % n;  // wraps to the bottom
                break;
            case Command::MoveDown:
                selected_ = (selected_ + 1) % n;  // wraps to the top
                break;
            case Command::Confirm:
            case Command::Step:
                return items_[selected_].id;
            default:
                break;
        }
        return -1;
    }

    int handleClick(sf::Vector2f mouse) const {
        for (int i = 0; i < items_.size(); ++i) {
            if (itemRect(i).contains(mouse)) return items_[i].id;
        }
        return -1;
    }

    void handleHover(sf::Vector2f mouse) {
        for (int i = 0; i < items_.size(); ++i) {
            if (itemRect(i).contains(mouse)) selected_ = i;
        }
    }

    void draw(sf::RenderTarget& t, const Fonts& f, sf::Color accent) const {
        for (int i = 0; i < items_.size(); ++i) {
            const sf::FloatRect r = itemRect(i);
            const bool selected = i == selected_;
            const float cy = r.position.y + r.size.y / 2.f;
            sf::Color fill = selected ? draw::mix(theme::panelHi, accent, 0.15f) : sf::Color(19, 23, 34, 235);
            draw::raisedBox(t, r, 9.f, 2.f, sf::Color(0, 0, 0, 60), fill, selected ? accent : theme::border, 1.f);
            if (selected) draw::rect(t, {{r.position.x + 1.f, r.position.y + 12.f}, {3.f, r.size.y - 24.f}}, accent);
            draw::text(t, f.bold, items_[i].label, 15, {r.position.x + 22.f, cy}, selected ? theme::text : theme::muted);
            if (!items_[i].hint.empty()) {
                draw::text(t, f.regular, items_[i].hint, 12, {r.position.x + r.size.x - 18.f, cy}, selected ? theme::muted : theme::dim, Align::Right);
            }
        }
    }

    sf::FloatRect itemRect(int index) const { return {{origin_.x, origin_.y + index * (itemHeight_ + gap_)}, {width_, itemHeight_}}; }

    int indexOf(int id) const {
        for (int i = 0; i < items_.size(); ++i) {
            if (items_[i].id == id) return i;
        }
        return -1;
    }

    int size() const { return items_.size(); }

private:
    DynamicArray<MenuItem> items_;
    int selected_ = 0;
    sf::Vector2f origin_{0.f, 0.f};
    float width_ = 320.f;
    float itemHeight_ = 46.f;
    float gap_ = 8.f;
};

namespace screens {

// Project credits from the PRD. Change them here to change them everywhere.
const char* const kCredits = "Isbah Rafay (25K-0664)  ·  Maheen Kamran (25K-0598)  ·  BCS-3F  ·  FAST-NUCES Karachi";

// The big pop-up card used by the pause, help and end screens.
void card(sf::RenderTarget& t, sf::FloatRect r, sf::Color edge = theme::borderHi) {
    draw::raisedBox(t, r, 16.f, 8.f, sf::Color(0, 0, 0, 120), theme::panel, edge, 1.f);
}

void setupTitleMenu(Menu& menu) {
    menu.clear();
    menu.add("New Game", "Random dungeon", menu_id::NewGame);
    menu.add("Play a Seed", "Replay a layout", menu_id::PlaySeed);
    menu.add("How to Play", "Controls & legend", menu_id::HowToPlay);
    menu.add("Quit", "", menu_id::Quit);
    menu.setLayout({96.f, 404.f}, 400.f, 48.f, 8.f);
}

void setupPauseMenu(Menu& menu) {
    menu.clear();
    menu.add("Resume", "Esc", menu_id::Resume);
    menu.add("Restart Dungeon", "Same seed", menu_id::Restart);
    menu.add("New Dungeon", "Random seed", menu_id::NewDungeon);
    menu.add("How to Play", "", menu_id::HowToPlay);
    menu.add("Main Menu", "", menu_id::MainMenu);
    menu.setLayout({494.f, 288.f}, 352.f, 46.f, 8.f);
}

void setupEndMenu(Menu& menu) {
    menu.clear();
    menu.add("New Dungeon", "Random seed", menu_id::NewDungeon);
    menu.add("Retry This Dungeon", "R", menu_id::Restart);
    menu.add("Main Menu", "", menu_id::MainMenu);
    menu.setLayout({490.f, 494.f}, 360.f, 44.f, 8.f);
}

// Drawn over a dungeon backdrop that the caller has already rendered.
void drawTitle(sf::RenderTarget& t, const Fonts& f, const Menu& menu, bool seedEntry, const std::string& seedText, float time) {
    const float W = theme::kW, H = theme::kH;
    // Darken the backdrop, most of all on the left where the text is.
    draw::hGradientRect(t, {{0.f, 0.f}, {W, H}}, sf::Color(8, 10, 16, 252), sf::Color(8, 10, 16, 120));
    draw::gradientRect(t, {{0.f, 0.f}, {W, 180.f}}, sf::Color(8, 10, 16, 210), sf::Color(8, 10, 16, 0));
    draw::gradientRect(t, {{0.f, H - 190.f}, {W, 190.f}}, sf::Color(8, 10, 16, 0), sf::Color(8, 10, 16, 240));

    const float x = 96.f;
    draw::portalIcon(t, {x + 24.f, 138.f}, 1.6f, time);
    draw::tag(t, f.bold, "DATA STRUCTURES & ALGORITHMS  ·  LAB PROJECT", {x + 66.f + 168.f, 138.f}, theme::teal);

    draw::text(t, f.title, "DUNGEONBOUND", 64, {x - 3.f, 228.f}, theme::gold, Align::Left, 1.6f);
    draw::text(t, f.bold, "THE LOST REALMS", 16, {x + 2.f, 286.f}, theme::text, Align::Left, 7.f);
    draw::rect(t, {{x + 2.f, 310.f}, {72.f, 2.f}}, theme::gold);
    draw::wrapped(t, f.regular,
                  "Explore a procedurally generated dungeon, gather relics and outwit its guardians. "
                  "Every system runs on hand-built data structures.",
                  14, {x + 2.f, 326.f}, 500.f, theme::muted, 22.f);

    menu.draw(t, f, theme::gold);

    // The seed text box sits on top of the "Play a Seed" row.
    const int seedRow = menu.indexOf(menu_id::PlaySeed);
    if (seedEntry && seedRow >= 0) {
        const sf::FloatRect row = menu.itemRect(seedRow);
        const sf::FloatRect field{{row.position.x + row.size.x - 186.f, row.position.y + 8.f}, {174.f, row.size.y - 16.f}};
        const float cy = field.position.y + field.size.y / 2.f;
        draw::roundedRect(t, field, 6.f, sf::Color(10, 12, 18), theme::gold, 1.f);
        const bool caretOn = std::fmod(time, 1.f) < 0.55f;  // blinking cursor
        if (seedText.empty()) {
            draw::text(t, f.regular, "type a number", 12, {field.position.x + 12.f, cy}, theme::dim);
            if (caretOn) draw::rect(t, {{field.position.x + 8.f, cy - 9.f}, {1.5f, 18.f}}, theme::gold);
        } else {
            draw::text(t, f.mono, seedText, 15, {field.position.x + 12.f, cy}, theme::text);
            float caretX = field.position.x + 14.f + draw::textWidth(f.mono, seedText, 15);
            if (caretOn) draw::rect(t, {{caretX, cy - 9.f}, {1.5f, 18.f}}, theme::gold);
        }
    }

    float hx = x;
    const float hy = menu.itemRect(menu.size() - 1).position.y + 48.f + 34.f;
    if (seedEntry) {
        draw::keyHint(t, f.bold, f.regular, hx, hy, "0-9", "Type a seed");
        draw::keyHint(t, f.bold, f.regular, hx, hy, "Enter", "Start");
        draw::keyHint(t, f.bold, f.regular, hx, hy, "Esc", "Cancel");
    } else {
        draw::keyHint(t, f.bold, f.regular, hx, hy, "W", "");
        hx -= 2.f;
        draw::keyHint(t, f.bold, f.regular, hx, hy, "S", "Navigate");
        draw::keyHint(t, f.bold, f.regular, hx, hy, "Enter", "Select");
        draw::keyHint(t, f.bold, f.regular, hx, hy, "Mouse", "Click");
    }

    // Footer: credits on the left, the four main data structures on the right.
    draw::text(t, f.regular, kCredits, 11, {x, H - 38.f}, theme::dim);
    const char* tags[4] = {"BINARY HEAP", "A* PATHFINDING", "RECURSIVE BACKTRACKING", "GRAPH"};
    float right = W - 40.f;
    for (const char* label : tags) right -= draw::tagRight(t, f.bold, label, right, H - 38.f, theme::muted) + 8.f;
}

void drawPause(sf::RenderTarget& t, const Fonts& f, const Menu& menu, const GameSession& s, float playSeconds) {
    draw::scrim(t, 175);
    const sf::FloatRect c{{470.f, 170.f}, {400.f, 440.f}};
    card(t, c);
    const float cx = c.position.x + c.size.x / 2.f;
    draw::text(t, f.title, "PAUSED", 26, {cx, c.position.y + 44.f}, theme::gold, Align::Center, 3.f);
    std::string info = "Seed " + std::to_string(s.seed()) + "  ·  " + formatTime(playSeconds) + "  ·  " + std::to_string(s.exploredPercent()) + "% explored";
    draw::text(t, f.mono, info, 12, {cx, c.position.y + 76.f}, theme::muted, Align::Center);
    draw::rect(t, {{c.position.x + 24.f, c.position.y + 98.f}, {c.size.x - 48.f, 1.f}}, theme::border);
    menu.draw(t, f, theme::gold);
    float hx = cx - 92.f;
    const float hy = c.position.y + c.size.y - 30.f;
    draw::keyHint(t, f.bold, f.regular, hx, hy, "Enter", "Select");
    draw::keyHint(t, f.bold, f.regular, hx, hy, "Esc", "Resume");
}

struct KeyRow {
    const char* key;
    const char* desc;
};

// Draws a column of "[key] description" rows starting at y; returns the new y.
float drawKeyRows(sf::RenderTarget& t, const Fonts& f, const KeyRow* rows, int count, float x, float y) {
    for (int i = 0; i < count; ++i) {
        draw::keycap(t, f.bold, rows[i].key, {x, y}, 10);
        draw::text(t, f.regular, rows[i].desc, 13, {x + 72.f, y}, theme::text);
        y += 30.f;
    }
    return y;
}

void drawHelp(sf::RenderTarget& t, const Fonts& f, float time) {
    draw::scrim(t, 195);
    const sf::FloatRect p{{150.f, 70.f}, {1040.f, 640.f}};
    card(t, p);
    const float px = p.position.x, py = p.position.y;

    draw::text(t, f.title, "HOW TO PLAY", 24, {px + 32.f, py + 40.f}, theme::gold, Align::Left, 2.f);
    draw::text(t, f.regular, "Reach the exit portal alive. The Warden guards it.", 13, {px + 250.f, py + 42.f}, theme::muted);
    float closeX = px + p.size.x - 110.f;
    draw::keyHint(t, f.bold, f.regular, closeX, py + 40.f, "Esc", "Close");
    draw::rect(t, {{px + 32.f, py + 68.f}, {p.size.x - 64.f, 1.f}}, theme::border);

    // Column 1: controls.
    const float col1 = px + 32.f;
    draw::sectionLabel(t, f.bold, "EXPLORING", {col1, py + 96.f}, theme::gold);
    const KeyRow exploreKeys[] = {
        {"WASD", "Move (arrow keys also work)"},
        {"Click", "Plot an A* route to a tile"},
        {"Enter", "Walk or stop the plotted route"},
        {"Space", "Take a single step on the route"},
        {"P", "Plot a route to the exit portal"},
        {"RMB", "Clear the route"},
        {"I", "Open the inventory"},
        {"Tab", "Cycle overlays: A* search, graph"},
        {"F2", "Reveal the whole map (demo aid)"},
        {"H", "Show this help"},
        {"Esc", "Pause or go back"},
    };
    float y = drawKeyRows(t, f, exploreKeys, 11, col1, py + 126.f);
    y += 8.f;
    draw::sectionLabel(t, f.bold, "INVENTORY & BATTLE", {col1, y}, theme::gold);
    y += 30.f;
    const KeyRow otherKeys[] = {
        {"U", "Use the selected item"},
        {"X", "Discard it (press twice)"},
        {"1-4", "Attack, Defend, Item, Escape"},
        {"Enter", "Confirm the highlighted action"},
    };
    drawKeyRows(t, f, otherKeys, 4, col1, y);

    // Column 2: what everything on the map is.
    const float col2 = px + 384.f;
    draw::rect(t, {{col2 - 24.f, py + 88.f}, {1.f, p.size.y - 120.f}}, theme::border);
    draw::sectionLabel(t, f.bold, "LEGEND", {col2, py + 96.f}, theme::gold);
    y = py + 128.f;
    auto legendRow = [&](const std::string& name, const std::string& detail) {
        draw::text(t, f.bold, name, 13, {col2 + 36.f, y - 7.f}, theme::text);
        draw::text(t, f.regular, detail, 11, {col2 + 36.f, y + 9.f}, theme::muted);
        y += 37.f;
    };
    draw::playerIcon(t, {col2 + 12.f, y}, 0.9f, 1);
    legendRow("You", "The marker shows the way you face");
    draw::portalIcon(t, {col2 + 12.f, y}, 0.7f, time);
    legendRow("Exit portal", "Step in to win the run");
    for (int k = 0; k < static_cast<int>(EnemyKind::Count); ++k) {
        const EnemyKind kind = static_cast<EnemyKind>(k);
        const EnemyInfo& info = enemyInfo(kind);
        const bool boss = kind == EnemyKind::Warden;
        draw::enemyIcon(t, kind, {col2 + 12.f, y + (boss ? 3.f : 0.f)}, boss ? 0.72f : 0.8f, time);
        std::string detail = std::to_string(info.stats.maxHp) + " HP  ·  " + std::to_string(info.stats.attack) + " ATK  ·  " +
                             std::to_string(info.stats.speed) + " SPD" + (boss ? "  ·  boss" : "");
        legendRow(info.name, detail);
    }
    for (int k = 0; k < static_cast<int>(ItemType::Count); ++k) {
        const ItemType type = static_cast<ItemType>(k);
        draw::itemIcon(t, type, {col2 + 12.f, y}, 0.95f);
        legendRow(itemInfo(type).name, itemInfo(type).effect);
    }

    // Column 3: the algorithms.
    const float col3 = px + 706.f;
    draw::rect(t, {{col3 - 24.f, py + 88.f}, {1.f, p.size.y - 120.f}}, theme::border);
    draw::sectionLabel(t, f.bold, "UNDER THE HOOD", {col3, py + 96.f}, theme::gold);
    struct Blurb {
        const char* title;
        const char* body;
    };
    const Blurb blurbs[] = {
        {"Graph  ·  adjacency list",
         "Every walkable tile is a vertex. Neighbouring tiles share an edge, stored in hand-written linked lists. Press Tab twice to see it."},
        {"Recursive backtracking",
         "The generator carves a maze cell by cell and backtracks at dead ends. Rooms and loops are added, then a graph search proves "
         "every tile is reachable."},
        {"A* pathfinding",
         "Click a tile to find the shortest route using a Manhattan-distance heuristic. Press Tab to see every node the search expanded."},
        {"Binary heap", "One custom heap orders the A* open set by cost and the battle turn queue by each fighter's next action time."},
    };
    y = py + 126.f;
    for (const Blurb& b : blurbs) {
        draw::rect(t, {{col3, y - 7.f}, {3.f, 14.f}}, theme::teal);
        draw::text(t, f.bold, b.title, 13, {col3 + 12.f, y}, theme::text);
        y += 14.f;
        y += draw::wrapped(t, f.regular, b.body, 12, {col3 + 12.f, y}, 290.f, theme::muted, 19.f) + 22.f;
    }
}

// The summary shown after winning or dying.
void drawEnd(sf::RenderTarget& t, const Fonts& f, const Menu& menu, const GameSession& s, bool victory, float playSeconds, float time) {
    draw::scrim(t, victory ? 165 : 190);
    if (!victory) draw::rect(t, {{0.f, 0.f}, {theme::kW, theme::kH}}, sf::Color(90, 14, 14, 46));  // red tint

    const sf::Color accent = victory ? theme::gold : theme::red;
    const sf::FloatRect c{{340.f, 88.f}, {660.f, 604.f}};
    card(t, c, draw::withAlpha(accent, 150));
    const float cx = c.position.x + c.size.x / 2.f;
    const float top = c.position.y;

    if (victory) {
        draw::portalIcon(t, {cx, top + 68.f}, 2.1f, time);
    } else {
        draw::glow(t, {cx, top + 68.f}, 60.f, sf::Color(229, 86, 78, 90), 5);
        draw::enemyIcon(t, EnemyKind::Skeleton, {cx, top + 68.f}, 2.5f, time);
    }
    draw::text(t, f.title, victory ? "VICTORY" : "DEFEATED", 40, {cx, top + 142.f}, accent, Align::Center, 4.f);
    const char* subtitle = victory ? "You stepped through the portal and escaped the Lost Realms." : "The dungeon claims another adventurer.";
    draw::text(t, f.regular, subtitle, 14, {cx, top + 180.f}, theme::muted, Align::Center);

    // Eight stat boxes in two rows of four.
    const RunStats& st = s.stats();
    struct Cell {
        const char* label;
        std::string value;
    };
    const Cell cells[8] = {
        {"TIME", formatTime(playSeconds)},
        {"STEPS", std::to_string(st.steps)},
        {"LEVEL", std::to_string(s.player().level)},
        {"EXPLORED", std::to_string(s.exploredPercent()) + "%"},
        {"FOES SLAIN", std::to_string(st.kills) + " / " + std::to_string(s.enemies().size())},
        {"ITEMS FOUND", std::to_string(st.itemsCollected)},
        {"DAMAGE DEALT", std::to_string(st.damageDealt)},
        {"DAMAGE TAKEN", std::to_string(st.damageTaken)},
    };
    const float gridX = c.position.x + 32.f, gridY = top + 212.f, gap = 10.f;
    const float cellW = (c.size.x - 64.f - 3.f * gap) / 4.f, cellH = 64.f;
    for (int i = 0; i < 8; ++i) {
        const sf::FloatRect r{{gridX + (i % 4) * (cellW + gap), gridY + (i / 4) * (cellH + gap)}, {cellW, cellH}};
        draw::roundedRect(t, r, 8.f, theme::panelHi, theme::border, 1.f);
        draw::text(t, f.bold, cells[i].label, 9, {r.position.x + 14.f, r.position.y + 19.f}, theme::dim, Align::Left, 1.6f);
        draw::text(t, f.bold, cells[i].value, 19, {r.position.x + 14.f, r.position.y + 43.f}, theme::text);
    }
    std::string seedLine = "Dungeon seed " + std::to_string(s.seed()) + "  ·  enter it on the title screen to replay this layout";
    draw::text(t, f.mono, seedLine, 11, {cx, top + 374.f}, theme::dim, Align::Center);

    menu.draw(t, f, accent);
}

}  // namespace screens

// ============================================================================
//  21. Game: window, screen state machine and main loop
//
//    Title      -> Exploring
//    Exploring <-> Inventory | Paused
//    Exploring  -> Battle <-> BattleItems (item picker)
//    Battle     -> Exploring (won / escaped) | Defeat (player died)
//    Exploring  -> Victory (exit reached)
//    Help can be opened over Title, Exploring and Paused.
//
//  The rules are all in GameSession; this class routes input, runs the
//  animations and decides which screen to draw.
// ============================================================================
class Game {
public:
    explicit Game(std::string exeDir) : exeDir_(std::move(exeDir)) {}

    bool init(std::string* error = nullptr) {
        if (!fonts_.load(exeDir_, error)) return false;
        screens::setupTitleMenu(titleMenu_);
        screens::setupPauseMenu(pauseMenu_);
        screens::setupEndMenu(endMenu_);
        // A fixed, fully revealed dungeon scrolls behind the title screen.
        backdrop_.newRun(kBackdropSeed);
        backdrop_.revealAll();
        view_ = sf::View(sf::FloatRect({0.f, 0.f}, {theme::kW, theme::kH}));
        return true;
    }

    int run() {
        // On small screens shrink the window; the layout itself never changes.
        const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        float fitW = (static_cast<float>(desktop.size.x) - 40.f) / theme::kW;
        float fitH = (static_cast<float>(desktop.size.y) - 110.f) / theme::kH;
        float scale = std::min({1.f, fitW, fitH});
        if (scale < 0.5f) scale = 0.5f;
        const sf::Vector2u size{static_cast<unsigned>(theme::kW * scale), static_cast<unsigned>(theme::kH * scale)};

        sf::ContextSettings settings;
        settings.antiAliasingLevel = 8;
        window_.create(sf::VideoMode(size), "Dungeonbound: The Lost Realms", sf::Style::Default, sf::State::Windowed, settings);
        window_.setFramerateLimit(60);
        window_.setKeyRepeatEnabled(false);  // we do our own hold-to-move
        applyLetterbox(window_.getSize());
        setWindowIcon();

        sf::Clock clock;
        while (window_.isOpen()) {
            while (const std::optional event = window_.pollEvent()) handleEvent(*event);
            if (!window_.isOpen()) break;

            float dt = clock.restart().asSeconds();
            if (dt > 0.05f) dt = 0.05f;  // no huge jump after a stall or a window drag
            update(dt);

            window_.clear(sf::Color(6, 8, 13));
            window_.setView(view_);
            render(window_);
            window_.display();
        }
        return 0;
    }

    // Renders every screen to PNG files in `outDir` without opening a window.
    // Used for the README screenshots and as a quick visual check.
    int captureScreens(const std::string& outDir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(outDir, ec);

        sf::ContextSettings settings;
        settings.antiAliasingLevel = 8;
        sf::RenderTexture target;
        if (!target.resize({theme::kWindowW, theme::kWindowH}, settings)) return 1;

        int failures = 0;
        auto tick = [this](int frames) {
            for (int i = 0; i < frames; ++i) update(1.f / 60.f);
        };
        auto shot = [&](const char* name) {
            tick(45);  // let the animations settle
            fade_ = 0.f;
            target.clear(sf::Color(6, 8, 13));
            render(target);
            target.display();
            if (!target.getTexture().copyToImage().saveToFile(fs::path(outDir) / name)) ++failures;
        };
        auto attackWith = [this](int digit) {
            Input in;
            in.cmd = Command::Digit;
            in.digit = digit;
            battleEvent(battle_.handleInput(in, session_));
        };

        // Title.
        shot("01-title.png");
        seedEntry_ = true;
        seedText_ = "12345";
        titleMenu_.select(1);
        shot("02-title-seed.png");

        // Exploring: pick up some items and visit a few rooms.
        startRun(12345);
        const Dungeon& d = session_.dungeon();
        const int home = d.start;
        for (int i = 0; i < 4 && i < session_.floorItems().size(); ++i) debugWalkTo(session_.floorItems()[i].tile);
        for (int i = 1; i < d.rooms().size() && i < 4; ++i) {
            const Room& room = d.rooms()[i];
            debugWalkTo(d.index(room.x + room.w / 2, room.y + room.h / 2));
        }
        session_.mutablePlayer().stats.hp = 71;
        std::string feedback;
        if (session_.planPath(home, &feedback)) toast(feedback + " Press Enter or click again to walk.", LogKind::Success);
        // Hover the mouse over a visible enemy to show its tooltip.
        for (const Enemy& e : session_.enemies()) {
            if (e.alive && session_.isVisible(e.tile)) mouse_ = tileCenterOnScreen(e.tile);
        }
        shot("03-explore.png");
        mouse_ = {-100.f, -100.f};
        toast_.ttl = 0.f;
        overlay_ = OverlayMode::Search;
        shot("04-overlay-astar.png");
        overlay_ = OverlayMode::Graph;
        shot("05-overlay-graph.png");
        overlay_ = OverlayMode::Off;

        // Inventory.
        inventory_.open(InventoryMode::Explore);
        setScreen(Screen::Inventory, false);
        shot("06-inventory.png");
        setScreen(Screen::Exploring, false);

        // Battle: walk into the first regular enemy we can reach.
        for (const Enemy& e : session_.enemies()) {
            if (e.alive && !e.boss && session_.phase() == SessionPhase::Exploring) {
                session_.clearPath();
                debugWalkTo(e.tile);
            }
        }
        if (screen_ == Screen::Battle) {
            tick(90);
            for (int round = 0; round < 2 && !session_.battle().isOver(); ++round) {
                attackWith(round == 0 ? 1 : 2);  // attack, then defend
                tick(110);
            }
            shot("07-battle.png");
            if (!session_.battle().isOver() && !session_.inventory().isEmpty()) {
                inventory_.open(InventoryMode::Battle);
                setScreen(Screen::BattleItems, false);
                shot("08-battle-items.png");
                setScreen(Screen::Battle, false);
            }
            for (int guard = 0; guard < 80 && !session_.battle().isOver(); ++guard) {
                attackWith(1);
                tick(70);
            }
            tick(40);
            shot("09-battle-result.png");
            leaveBattle();
        }

        // Pause, help and the end screens.
        if (screen_ == Screen::Exploring) {
            pauseMenu_.select(0);
            setScreen(Screen::Paused, false);
            shot("10-pause.png");
            setScreen(Screen::Exploring, false);
            openHelp();
            shot("11-help.png");
            screen_ = Screen::Exploring;
        }
        endMenu_.select(0);
        setScreen(Screen::Victory, false);
        shot("12-victory.png");
        endMenu_.select(1);
        setScreen(Screen::Defeat, false);
        shot("13-defeat.png");
        return failures == 0 ? 0 : 1;
    }

private:
    enum class Screen { Title, Exploring, Inventory, Battle, BattleItems, Paused, Help, Victory, Defeat };

    struct Toast {  // a short message that pops up over the map
        std::string text;
        LogKind kind = LogKind::Info;
        float ttl = 0.f;
    };

    static constexpr float kRepeatDelay = 0.2f;   // hold to move: wait before repeating...
    static constexpr float kRepeatRate = 0.085f;  // ...then step this often
    static constexpr float kWalkRate = 0.07f;     // auto-walk step interval
    static constexpr std::uint32_t kBackdropSeed = 20261018u;

    // ---- Flow ------------------------------------------------------------------
    // Six digits: short enough to read out, type back in and share.
    static std::uint32_t randomSeed() {
        std::random_device device;
        auto ticks = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        return 100000u + (device() ^ ticks) % 900000u;
    }

    void startRun(std::uint32_t seed) {
        session_.newRun(seed);
        playerVis_ = playerTilePosition();
        hudState_ = HudState();
        hudState_.displayHp = static_cast<float>(session_.player().stats.hp);
        hudState_.ghostHp = hudState_.displayHp;
        overlay_ = OverlayMode::Off;
        walking_ = false;
        heldMove_ = Command::None;
        hoverTile_ = -1;
        playSeconds_ = 0.f;
        seedEntry_ = false;
        toast_ = Toast();
        setScreen(Screen::Exploring);
    }

    // `fade` briefly fades the new screen in from black.
    void setScreen(Screen screen, bool fade = true) {
        screen_ = screen;
        if (fade) fade_ = 1.f;
    }

    void openHelp() {
        helpReturn_ = screen_;
        screen_ = Screen::Help;
    }

    void openInventory() {
        walking_ = false;
        inventory_.open(InventoryMode::Explore);
        setScreen(Screen::Inventory, false);
    }

    void onMenu(int id) {
        switch (id) {
            case menu_id::NewGame:
            case menu_id::NewDungeon:
                startRun(randomSeed());
                break;
            case menu_id::PlaySeed:
                seedEntry_ = true;
                seedText_.clear();
                break;
            case menu_id::HowToPlay:
                openHelp();
                break;
            case menu_id::Quit:
                window_.close();
                break;
            case menu_id::Resume:
                setScreen(Screen::Exploring, false);
                break;
            case menu_id::Restart:
                startRun(session_.seed());
                break;
            case menu_id::MainMenu:
                titleMenu_.select(0);
                seedEntry_ = false;
                setScreen(Screen::Title);
                break;
            default:
                break;
        }
    }

    void toast(const std::string& text, LogKind kind) {
        toast_.text = text;
        toast_.kind = kind;
        toast_.ttl = text.empty() ? 0.f : 2.6f;
    }

    // ---- Input -----------------------------------------------------------------
    void handleEvent(const sf::Event& event) {
        if (event.is<sf::Event::Closed>()) {
            window_.close();
        } else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
            applyLetterbox(resized->size);
        } else if (event.is<sf::Event::FocusLost>()) {
            focused_ = false;
            heldMove_ = Command::None;
        } else if (event.is<sf::Event::FocusGained>()) {
            focused_ = true;
        } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            onKey(key->code);
        } else if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
            onText(text->unicode);
        } else if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
            onMouseMove(window_.mapPixelToCoords(moved->position, view_));
        } else if (const auto* button = event.getIf<sf::Event::MouseButtonPressed>()) {
            const sf::Vector2f p = window_.mapPixelToCoords(button->position, view_);
            onMouseMove(p);
            if (button->button == sf::Mouse::Button::Left) onMouseDown(p, true);
            else if (button->button == sf::Mouse::Button::Right) onMouseDown(p, false);
        }
    }

    // Typed digits for the seed box arrive here (not as key presses).
    void onText(char32_t unicode) {
        if (screen_ != Screen::Title || !seedEntry_) return;
        if (unicode >= U'0' && unicode <= U'9' && seedText_.size() < 9) seedText_ += static_cast<char>(unicode);
    }

    void onKey(sf::Keyboard::Key key) {
        // While typing a seed, only Esc, Backspace and Enter do anything.
        if (screen_ == Screen::Title && seedEntry_) {
            if (key == sf::Keyboard::Key::Escape) {
                seedEntry_ = false;
            } else if (key == sf::Keyboard::Key::Backspace) {
                if (!seedText_.empty()) seedText_.pop_back();
            } else if (key == sf::Keyboard::Key::Enter && !seedText_.empty()) {
                startRun(static_cast<std::uint32_t>(std::stoul(seedText_)));
            }
            return;
        }

        const Input in = input::translate(key);
        if (in.cmd == Command::None) return;

        switch (screen_) {
            case Screen::Title:
                if (in.cmd == Command::Help) openHelp();
                else onMenu(titleMenu_.handleInput(in));
                break;
            case Screen::Exploring:
                exploreInput(in);
                break;
            case Screen::Inventory:
            case Screen::BattleItems:
                inventoryAct(inventory_.handleInput(in, session_.inventory()));
                break;
            case Screen::Battle:
                battleEvent(battle_.handleInput(in, session_));
                break;
            case Screen::Paused:
                if (in.cmd == Command::Back) setScreen(Screen::Exploring, false);
                else if (in.cmd == Command::Help) openHelp();
                else onMenu(pauseMenu_.handleInput(in));
                break;
            case Screen::Help: {
                bool close = in.cmd == Command::Back || in.cmd == Command::Help || in.cmd == Command::Confirm || in.cmd == Command::Step;
                if (close) screen_ = helpReturn_;
                break;
            }
            case Screen::Victory:
            case Screen::Defeat:
                if (in.cmd == Command::Restart) startRun(session_.seed());
                else onMenu(endMenu_.handleInput(in));
                break;
        }
    }

    void cycleOverlay() {
        switch (overlay_) {
            case OverlayMode::Off:
                overlay_ = OverlayMode::Search;
                toast("Overlay: nodes expanded by the last A* search.", LogKind::Info);
                break;
            case OverlayMode::Search:
                overlay_ = OverlayMode::Graph;
                toast("Overlay: dungeon graph (vertices and edges).", LogKind::Info);
                break;
            case OverlayMode::Graph:
                overlay_ = OverlayMode::Off;
                toast("Overlay off.", LogKind::Info);
                break;
        }
    }

    void exploreInput(const Input& in) {
        if (input::isMove(in.cmd)) {
            tryMove(in.cmd);
            heldMove_ = in.cmd;
            repeatTimer_ = kRepeatDelay;
            return;
        }
        switch (in.cmd) {
            case Command::Confirm:  // start or stop walking the route
                if (session_.hasPlannedPath()) {
                    walking_ = !walking_;
                    walkTimer_ = 0.f;
                } else {
                    toast("Click a tile to plot a route first.", LogKind::Info);
                }
                break;
            case Command::Step:  // one step on the route
                if (session_.hasPlannedPath()) {
                    walking_ = false;
                    onMoveResult(session_.followPathStep());
                } else {
                    toast("Click a tile to plot a route first.", LogKind::Info);
                }
                break;
            case Command::Back:  // first clears the route, then pauses
                if (walking_ || session_.hasPlannedPath()) {
                    walking_ = false;
                    session_.clearPath();
                } else {
                    pauseMenu_.select(0);
                    setScreen(Screen::Paused, false);
                }
                break;
            case Command::Inventory:
                openInventory();
                break;
            case Command::Help:
                walking_ = false;
                openHelp();
                break;
            case Command::Overlay:
                cycleOverlay();
                break;
            case Command::PathToExit: {
                std::string feedback;
                const bool ok = session_.planPathToExit(&feedback);
                walking_ = false;
                if (ok) toast(feedback + " Press Enter to walk.", LogKind::Success);
                else toast(feedback, LogKind::Warning);
                break;
            }
            case Command::Reveal:
                session_.revealAll();
                toast("Map revealed (demo aid).", LogKind::Arcane);
                break;
            default:
                break;
        }
    }

    void tryMove(Command c) {
        // Moving by hand cancels any plotted route.
        walking_ = false;
        session_.clearPath();
        onMoveResult(session_.move(input::dx(c), input::dy(c)));
    }

    void onMoveResult(MoveResult r) {
        switch (r) {
            case MoveResult::BattleStarted:
                heldMove_ = Command::None;
                walking_ = false;
                battle_.begin(session_);
                setScreen(Screen::Battle);
                break;
            case MoveResult::ReachedExit:
                heldMove_ = Command::None;
                walking_ = false;
                endMenu_.select(0);
                setScreen(Screen::Victory);
                break;
            case MoveResult::PickedUp:
                toast(session_.messages().back().text, LogKind::Success);
                break;
            case MoveResult::InventoryFull:
                toast(session_.messages().back().text, LogKind::Warning);
                break;
            default:
                break;
        }
    }

    void inventoryAct(InventoryAction action) {
        const bool inBattle = screen_ == Screen::BattleItems;
        switch (action) {
            case InventoryAction::Close:
                setScreen(inBattle ? Screen::Battle : Screen::Exploring, false);
                break;
            case InventoryAction::Use:
                if (inBattle) {
                    // A rejected item shows a message and keeps your turn.
                    const ActionResult r = session_.battle().playerAct(BattleAction::UseItem, inventory_.selected());
                    if (r.accepted) {
                        battle_.onPlayerActed();
                        setScreen(Screen::Battle, false);
                    } else {
                        inventory_.setFeedback(r.feedback, false);
                    }
                } else {
                    const ItemUseResult r = session_.useItem(inventory_.selected());
                    inventory_.setFeedback(r.message, r.success);
                    inventory_.clampSelection(session_.inventory());
                }
                break;
            case InventoryAction::Discard: {
                std::string feedback;
                const bool ok = session_.discardItem(inventory_.selected(), &feedback);
                inventory_.setFeedback(feedback, ok);
                inventory_.clampSelection(session_.inventory());
                break;
            }
            case InventoryAction::None:
                break;
        }
    }

    void battleEvent(BattleUiEvent event) {
        if (event == BattleUiEvent::OpenItems) {
            inventory_.open(InventoryMode::Battle);
            setScreen(Screen::BattleItems, false);
        } else if (event == BattleUiEvent::Finished) {
            leaveBattle();
        }
    }

    void leaveBattle() {
        const BattlePhase outcome = session_.battle().phase();
        int levels = 0;
        const int xp = session_.finishBattle(&levels);
        if (session_.phase() == SessionPhase::Defeat) {
            endMenu_.select(1);  // "Retry This Dungeon"
            setScreen(Screen::Defeat);
            return;
        }
        setScreen(Screen::Exploring);
        if (outcome == BattlePhase::Won) {
            if (levels > 0) {
                toast("+" + std::to_string(xp) + " XP  ·  Level up! You are now level " + std::to_string(session_.player().level) + ".", LogKind::Arcane);
            } else {
                toast("+" + std::to_string(xp) + " XP", LogKind::Success);
            }
        } else if (outcome == BattlePhase::Escaped) {
            toast("You got away.", LogKind::Info);
        }
    }

    void onMouseMove(sf::Vector2f p) {
        mouse_ = p;
        switch (screen_) {
            case Screen::Title:
                if (!seedEntry_) titleMenu_.handleHover(p);
                break;
            case Screen::Paused: pauseMenu_.handleHover(p); break;
            case Screen::Victory:
            case Screen::Defeat: endMenu_.handleHover(p); break;
            case Screen::Inventory:
            case Screen::BattleItems: inventory_.handleHover(p); break;
            case Screen::Battle: battle_.handleHover(p); break;
            default: break;
        }
    }

    void onMouseDown(sf::Vector2f p, bool left) {
        if (screen_ == Screen::Exploring) {
            exploreClick(p, left);
            return;
        }
        if (!left) return;  // the right button only matters while exploring
        switch (screen_) {
            case Screen::Title:
                if (!seedEntry_) onMenu(titleMenu_.handleClick(p));
                break;
            case Screen::Inventory:
            case Screen::BattleItems: inventoryAct(inventory_.handleClick(p, session_.inventory())); break;
            case Screen::Battle: battleEvent(battle_.handleClick(p, session_)); break;
            case Screen::Paused: onMenu(pauseMenu_.handleClick(p)); break;
            case Screen::Help: screen_ = helpReturn_; break;
            case Screen::Victory:
            case Screen::Defeat: onMenu(endMenu_.handleClick(p)); break;
            default: break;
        }
    }

    void exploreClick(sf::Vector2f p, bool left) {
        if (!left) {  // right click clears the route
            walking_ = false;
            session_.clearPath();
            return;
        }
        if (kPackCard.contains(p)) {  // the pack card opens the inventory
            openInventory();
            return;
        }
        const int tile = tileAtPoint(p);
        if (tile < 0) return;
        if (session_.hasPlannedPath() && tile == session_.pathTarget()) {
            walking_ = true;  // clicking the destination again starts walking
            walkTimer_ = 0.f;
            return;
        }
        std::string feedback;
        walking_ = false;
        const bool ok = session_.planPath(tile, &feedback);
        if (ok) toast(feedback + " Press Enter or click again to walk.", LogKind::Success);
        else toast(feedback, LogKind::Warning);
    }

    // The map tile under a screen point, or -1.
    int tileAtPoint(sf::Vector2f p) const {
        const Dungeon& d = session_.dungeon();
        const float fx = (p.x - theme::kMapX) / theme::kTile;
        const float fy = (p.y - theme::kMapY) / theme::kTile;
        if (fx < 0.f || fy < 0.f) return -1;
        const int x = static_cast<int>(fx);
        const int y = static_cast<int>(fy);
        return d.inBounds(x, y) ? d.index(x, y) : -1;
    }

    sf::Vector2f tileCenterOnScreen(int tile) const {
        const Dungeon& d = session_.dungeon();
        return {theme::kMapX + (d.xOf(tile) + 0.5f) * theme::kTile, theme::kMapY + (d.yOf(tile) + 0.5f) * theme::kTile};
    }

    sf::Vector2f playerTilePosition() const {
        const Dungeon& d = session_.dungeon();
        const int tile = session_.player().tile;
        return {static_cast<float>(d.xOf(tile)), static_cast<float>(d.yOf(tile))};
    }

    // ---- Frame update ------------------------------------------------------------
    void update(float dt) {
        time_ += dt;
        if (fade_ > 0.f) fade_ -= dt * 3.4f;
        if (toast_.ttl > 0.f) toast_.ttl -= dt;

        // The play clock only runs while you are in the dungeon.
        switch (screen_) {
            case Screen::Exploring:
                playSeconds_ += dt;
                updateExplore(dt);
                break;
            case Screen::Inventory:
                playSeconds_ += dt;
                inventory_.update(dt);
                break;
            case Screen::Battle:
                playSeconds_ += dt;
                battle_.update(dt, session_);
                break;
            case Screen::BattleItems:
                playSeconds_ += dt;
                inventory_.update(dt);
                battle_.update(dt, session_);
                break;
            default:
                break;
        }

        if (!session_.hasRun()) return;

        // Slide the player marker and the HP bar toward their real values.
        const sf::Vector2f target = playerTilePosition();
        playerVis_ += (target - playerVis_) * std::min(1.f, dt * 22.f);

        const float hp = static_cast<float>(session_.player().stats.hp);
        hudState_.displayHp += (hp - hudState_.displayHp) * std::min(1.f, dt * 10.f);
        if (hudState_.ghostHp > hudState_.displayHp) {
            hudState_.ghostHp -= dt * 28.f;
            if (hudState_.ghostHp < hudState_.displayHp) hudState_.ghostHp = hudState_.displayHp;
        } else {
            hudState_.ghostHp = hudState_.displayHp;
        }
        hudState_.time = time_;
        hudState_.playSeconds = playSeconds_;
        hudState_.overlay = overlay_;
        hudState_.walking = walking_;
    }

    void updateExplore(float dt) {
        // Holding a direction keeps moving.
        if (heldMove_ != Command::None) {
            if (!focused_ || !input::isHeld(heldMove_)) {
                heldMove_ = Command::None;
            } else {
                repeatTimer_ -= dt;
                if (repeatTimer_ <= 0.f) {
                    repeatTimer_ = kRepeatRate;
                    tryMove(heldMove_);
                }
            }
        }
        if (screen_ != Screen::Exploring) return;  // that step may have started a battle

        // Follow the planned A* route one tile at a time.
        if (walking_) {
            if (!session_.hasPlannedPath()) {
                walking_ = false;
            } else {
                walkTimer_ -= dt;
                if (walkTimer_ <= 0.f) {
                    walkTimer_ = kWalkRate;
                    onMoveResult(session_.followPathStep());
                    if (!session_.hasPlannedPath()) walking_ = false;
                }
            }
        }
        hoverTile_ = tileAtPoint(mouse_);
    }

    // ---- Rendering -----------------------------------------------------------------
    void render(sf::RenderTarget& t) {
        switch (screen_) {
            case Screen::Title:
                renderTitle(t);
                break;
            case Screen::Exploring:
                renderExplore(t, true, false);
                break;
            case Screen::Inventory:
                renderExplore(t, false, false);
                inventory_.draw(t, fonts_, session_.inventory(), session_.player());
                break;
            case Screen::Battle:
                battle_.draw(t, fonts_, session_);
                break;
            case Screen::BattleItems:
                battle_.draw(t, fonts_, session_);
                inventory_.draw(t, fonts_, session_.inventory(), session_.player());
                break;
            case Screen::Paused:
                renderExplore(t, false, false);
                screens::drawPause(t, fonts_, pauseMenu_, session_, playSeconds_);
                break;
            case Screen::Help:
                if (helpReturn_ == Screen::Title) renderTitle(t);
                else renderExplore(t, false, false);
                screens::drawHelp(t, fonts_, time_);
                break;
            case Screen::Victory:
            case Screen::Defeat:
                // Show the whole dungeon behind the summary.
                renderExplore(t, false, true);
                screens::drawEnd(t, fonts_, endMenu_, session_, screen_ == Screen::Victory, playSeconds_, time_);
                break;
        }
        if (fade_ > 0.f) {
            std::uint8_t alpha = static_cast<std::uint8_t>(255.f * std::min(1.f, fade_));
            draw::rect(t, {{0.f, 0.f}, {theme::kW, theme::kH}}, sf::Color(6, 8, 13, alpha));
        }
    }

    void renderTitle(sf::RenderTarget& t) {
        draw::rect(t, {{0.f, 0.f}, {theme::kW, theme::kH}}, theme::bg);
        MapView v;
        v.tile = 36.4f;  // 37 tiles fill the window width
        v.origin = {-3.f, -28.f + 26.f * std::sin(time_ * 0.16f)};  // slow drift up and down
        v.fog = false;
        v.showPlayer = false;
        v.time = time_;
        v.dimAll = 0.9f;
        renderer_.drawMap(t, backdrop_, v);
        screens::drawTitle(t, fonts_, titleMenu_, seedEntry_, seedText_, time_);
    }

    // `interactive` adds the hover cursor, tooltip and toast; `reveal` turns off the fog.
    void renderExplore(sf::RenderTarget& t, bool interactive, bool reveal) {
        draw::backdrop(t);
        hud_.drawTopBar(t, fonts_, session_, hudState_);
        hud_.drawMapFrame(t, session_);

        MapView v;
        v.origin = {theme::kMapX, theme::kMapY};
        v.tile = theme::kTile;
        v.fog = !reveal;
        v.overlay = overlay_;
        v.hoverTile = interactive ? hoverTile_ : -1;
        v.playerPos = playerVis_;
        v.time = time_;
        renderer_.drawMap(t, session_, v);

        hud_.drawSidebar(t, fonts_, session_, hudState_);
        hud_.drawJournal(t, fonts_, session_);
        if (interactive) {
            renderToast(t);
            renderer_.drawHoverTooltip(t, fonts_, session_, v, mouse_);
        }
    }

    void renderToast(sf::RenderTarget& t) {
        if (toast_.ttl <= 0.f || toast_.text.empty()) return;
        const float k = toast_.ttl < 0.4f ? toast_.ttl / 0.4f : 1.f;  // fades out at the end
        const int a = static_cast<int>(255.f * k);
        const sf::FloatRect frame = Hud::mapFrame(session_);
        const float w = draw::textWidth(fonts_.regular, toast_.text, 13) + 50.f;
        const float cx = frame.position.x + frame.size.x / 2.f;
        const float cy = frame.position.y + frame.size.y - 34.f + (1.f - k) * 8.f;
        const sf::FloatRect r{{cx - w / 2.f, cy - 17.f}, {w, 34.f}};
        draw::raisedBox(t, r, 17.f, 4.f, sf::Color(0, 0, 0, static_cast<std::uint8_t>(110.f * k)), sf::Color(24, 28, 41, static_cast<std::uint8_t>(246.f * k)),
                        draw::withAlpha(theme::borderHi, a), 1.f);
        draw::circle(t, {r.position.x + 20.f, cy}, 4.f, draw::withAlpha(logColor(toast_.kind), a));
        draw::text(t, fonts_.regular, toast_.text, 13, {r.position.x + 34.f, cy}, draw::withAlpha(theme::text, a));
    }

    // Keeps the 1340x780 layout's shape when the window is resized; the
    // spare area stays dark.
    void applyLetterbox(sf::Vector2u windowSize) {
        if (windowSize.x == 0 || windowSize.y == 0) return;
        const float sx = static_cast<float>(windowSize.x) / theme::kW;
        const float sy = static_cast<float>(windowSize.y) / theme::kH;
        const float s = std::min(sx, sy);
        const float vw = theme::kW * s / static_cast<float>(windowSize.x);
        const float vh = theme::kH * s / static_cast<float>(windowSize.y);
        view_.setViewport(sf::FloatRect({(1.f - vw) / 2.f, (1.f - vh) / 2.f}, {vw, vh}));
    }

    // The window icon is the exit portal, drawn with the same art as the game.
    void setWindowIcon() {
        sf::RenderTexture icon;
        if (!icon.resize({64u, 64u})) return;
        icon.clear(sf::Color::Transparent);
        draw::portalIcon(icon, {32.f, 32.f}, 2.3f, 0.6f);
        icon.display();
        const sf::Image image = icon.getTexture().copyToImage();
        window_.setIcon(image.getSize(), image.getPixelsPtr());
    }

    // Screenshot helper: walks straight to `tile`, ignoring fog of war.
    bool debugWalkTo(int tile) {
        const Dungeon& d = session_.dungeon();
        if (!d.isValidIndex(tile) || session_.phase() != SessionPhase::Exploring) return false;
        DynamicArray<bool> passable(d.tileCount(), true);
        for (const Enemy& e : session_.enemies()) {
            if (e.alive && e.tile != tile) passable[e.tile] = false;
        }
        if (tile != d.exit) passable[d.exit] = false;
        const PathResult route = Pathfinder::findPath(d.graph(), d.width(), session_.player().tile, tile, &passable);
        if (!route.found) return false;
        for (int i = 1; i < route.path.size(); ++i) {
            const int from = route.path[i - 1];
            const int to = route.path[i];
            const MoveResult r = session_.move(d.xOf(to) - d.xOf(from), d.yOf(to) - d.yOf(from));
            if (r == MoveResult::BattleStarted || r == MoveResult::ReachedExit) {
                onMoveResult(r);
                return true;
            }
        }
        return true;
    }

    std::string exeDir_;
    sf::RenderWindow window_;
    sf::View view_;
    Fonts fonts_;

    GameSession session_;
    GameSession backdrop_;  // the dungeon behind the title screen
    Renderer renderer_;
    Hud hud_;
    HudState hudState_;
    InventoryPanel inventory_;
    BattleScreen battle_;
    Menu titleMenu_, pauseMenu_, endMenu_;

    Screen screen_ = Screen::Title;
    Screen helpReturn_ = Screen::Title;  // where closing help goes back to
    OverlayMode overlay_ = OverlayMode::Off;
    bool seedEntry_ = false;
    std::string seedText_;

    sf::Vector2f mouse_{-100.f, -100.f};
    sf::Vector2f playerVis_{0.f, 0.f};  // smoothed player position, in tiles
    int hoverTile_ = -1;
    bool focused_ = true;
    bool walking_ = false;
    float walkTimer_ = 0.f;
    Command heldMove_ = Command::None;
    float repeatTimer_ = 0.f;

    float time_ = 0.f;
    float playSeconds_ = 0.f;
    float fade_ = 0.f;
    Toast toast_;
};

// ============================================================================
//  22. main()
// ============================================================================
int main(int argc, char** argv) {
    // Fonts are looked up next to the executable.
    std::string exeDir;
    if (argc > 0 && argv[0]) {
        std::error_code ec;
        const std::filesystem::path exe = std::filesystem::absolute(argv[0], ec);
        if (!ec) exeDir = exe.parent_path().string();
    }

    Game game(exeDir);
    std::string error;
    if (!game.init(&error)) {
        // Release builds have no console on Windows, so also write a log file.
        std::fprintf(stderr, "Dungeonbound could not start: %s\n", error.c_str());
        std::ofstream log("dungeonbound-error.log");
        log << "Dungeonbound could not start: " << error << "\n";
        return 1;
    }

    if (argc >= 3 && std::string(argv[1]) == "--shots") return game.captureScreens(argv[2]);
    return game.run();
}

#endif  // DUNGEONBOUND_NO_GAME

// ############################################################################
//  PART C - TESTS (only built when DUNGEONBOUND_TESTS is defined)
//  The game build never defines it, so none of this ends up in the game.
//  CMake builds dungeon_tests.exe from this same file with both
//  DUNGEONBOUND_NO_GAME and DUNGEONBOUND_TESTS defined: Part A + Part C.
//
//    23. Test framework  TEST / CHECK / CHECK_THROWS macros and the runner
//    24. Tests           35 tests of the data structures and game rules
//    25. Test main()
// ############################################################################
#ifdef DUNGEONBOUND_TESTS

#ifndef DUNGEONBOUND_NO_GAME
#error "DUNGEONBOUND_TESTS needs DUNGEONBOUND_NO_GAME too (the tests have their own main())"
#endif

// ============================================================================
//  23. Test framework
//  TEST(name) { ... } defines a test; each one adds itself to a list when the
//  program starts. CHECK(expr) records a failure (with file and line) if
//  expr is false. CHECK_THROWS(expr) expects expr to throw an exception.
// ============================================================================
namespace testfw {

using TestFn = void (*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

inline TestCase* registry() {
    static TestCase tests[256];
    return tests;
}
inline int& registryCount() {
    static int count = 0;
    return count;
}
inline int& failures() {
    static int f = 0;
    return f;
}
inline int& checks() {
    static int c = 0;
    return c;
}
inline bool& currentFailed() {
    static bool b = false;
    return b;
}

struct Registrar {
    Registrar(const char* name, TestFn fn) {
        if (registryCount() < 256) registry()[registryCount()++] = {name, fn};
    }
};

inline void report(bool ok, const char* expr, const char* file, int line) {
    ++checks();
    if (!ok) {
        ++failures();
        currentFailed() = true;
        std::printf("    FAILED: %s  (%s:%d)\n", expr, file, line);
    }
}

inline int runAll() {
    int passed = 0;
    std::printf("Dungeonbound test suite: %d tests\n\n", registryCount());
    for (int i = 0; i < registryCount(); ++i) {
        currentFailed() = false;
        try {
            registry()[i].fn();
        } catch (...) {
            ++failures();
            currentFailed() = true;
            std::printf("    FAILED: unexpected exception\n");
        }
        std::printf("  [%s] %s\n", currentFailed() ? "FAIL" : " OK ", registry()[i].name);
        if (!currentFailed()) ++passed;
    }
    std::printf("\n%d/%d tests passed, %d checks, %d failed checks\n", passed, registryCount(), checks(), failures());
    return failures() == 0 ? 0 : 1;
}

}  // namespace testfw

#define TF_CONCAT2(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT2(a, b)

#define TEST(name)                                                              \
    static void name();                                                         \
    static testfw::Registrar TF_CONCAT(registrar_, name)(#name, &name);         \
    static void name()

#define CHECK(expr) testfw::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define CHECK_THROWS(expr)                                                      \
    do {                                                                        \
        bool threw_ = false;                                                    \
        try {                                                                   \
            (void)(expr);                                                       \
        } catch (...) {                                                         \
            threw_ = true;                                                      \
        }                                                                       \
        testfw::report(threw_, "throws: " #expr, __FILE__, __LINE__);           \
    } while (0)

// ============================================================================
//  24. Tests
// ============================================================================
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

// ============================================================================
//  25. Test main(): runs every test; returns 0 only if all of them pass.
// ============================================================================
int main() { return testfw::runAll(); }

#endif  // DUNGEONBOUND_TESTS
