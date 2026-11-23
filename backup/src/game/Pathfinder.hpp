#pragma once
// Pathfinder — A* search over the dungeon graph (FR-A1 .. FR-A4).
// Heuristic: Manhattan distance on the tile grid (admissible for 4-way moves).
// Open set: the team's BinaryHeap (min-heap on f = g + h).

#include "../core/DynamicArray.hpp"
#include "../core/Graph.hpp"

struct PathResult {
    bool found = false;
    DynamicArray<int> path;      // start .. goal inclusive; empty when not found
    DynamicArray<int> expanded;  // closed-set order, for the debug overlay
    int cost = -1;
    int heapPushes = 0;
    double millis = 0.0;
};

class Pathfinder {
public:
    // `passable` (optional) masks vertices the search may enter; the start is
    // always allowed. Pass nullptr to search the whole graph.
    static PathResult findPath(const Graph& graph, int gridWidth, int start, int goal,
                               const DynamicArray<bool>* passable = nullptr);

    static int manhattan(int a, int b, int gridWidth);
};
