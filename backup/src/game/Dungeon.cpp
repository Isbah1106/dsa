#include "Dungeon.hpp"

Dungeon::Dungeon(int width, int height)
    : width_(width < 1 ? 1 : width), height_(height < 1 ? 1 : height), tiles_(width_ * height_, Tile::Wall) {}

int Dungeon::walkableCount() const {
    int count = 0;
    for (Tile t : tiles_) {
        if (t != Tile::Wall) ++count;
    }
    return count;
}

void Dungeon::buildGraph() {
    graph_.reset(tileCount());
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (!isWalkable(x, y)) continue;
            // Only look right and down: each undirected edge is added once.
            if (isWalkable(x + 1, y)) graph_.addEdge(index(x, y), index(x + 1, y), 1);
            if (isWalkable(x, y + 1)) graph_.addEdge(index(x, y), index(x, y + 1), 1);
        }
    }
}

int Dungeon::roomAt(int x, int y) const {
    for (int i = 0; i < rooms_.size(); ++i) {
        if (rooms_[i].contains(x, y)) return i;
    }
    return -1;
}
