#pragma once
// Dungeon — tile grid plus its graph representation (FR-G1).
// Vertex id of tile (x, y) is y * width + x. Walls are isolated vertices.

#include "../core/DynamicArray.hpp"
#include "../core/Graph.hpp"

#include <cstdint>

enum class Tile : unsigned char { Wall, Floor, Exit };

struct Room {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    // True if the rooms overlap or touch (including a 1-tile margin).
    bool overlapsWithMargin(const Room& o, int margin) const {
        return x - margin < o.x + o.w && x + w + margin > o.x && y - margin < o.y + o.h && y + h + margin > o.y;
    }
};

class Dungeon {
public:
    Dungeon() = default;
    Dungeon(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    int tileCount() const { return width_ * height_; }

    int index(int x, int y) const { return y * width_ + x; }
    int xOf(int idx) const { return idx % width_; }
    int yOf(int idx) const { return idx / width_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    bool isValidIndex(int idx) const { return idx >= 0 && idx < tileCount(); }

    Tile tile(int x, int y) const { return inBounds(x, y) ? tiles_[index(x, y)] : Tile::Wall; }
    Tile tileAt(int idx) const { return isValidIndex(idx) ? tiles_[idx] : Tile::Wall; }
    void setTile(int x, int y, Tile t) {
        if (inBounds(x, y)) tiles_[index(x, y)] = t;
    }

    bool isWalkable(int x, int y) const { return tile(x, y) != Tile::Wall; }
    bool isWalkableIdx(int idx) const { return tileAt(idx) != Tile::Wall; }
    int walkableCount() const;

    // Rebuilds the adjacency-list graph from the current tiles.
    void buildGraph();
    const Graph& graph() const { return graph_; }

    int roomAt(int x, int y) const;  // -1 if not in a room
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
