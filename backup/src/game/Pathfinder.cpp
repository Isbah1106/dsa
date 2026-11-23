#include "Pathfinder.hpp"

#include "../core/BinaryHeap.hpp"

#include <chrono>

namespace {
struct OpenNode {
    int f = 0;
    int h = 0;
    int id = -1;
};

// Lowest f first; ties prefer the node closer to the goal, then lower id
// (keeps results deterministic).
struct OpenNodeCompare {
    bool operator()(const OpenNode& a, const OpenNode& b) const {
        if (a.f != b.f) return a.f < b.f;
        if (a.h != b.h) return a.h < b.h;
        return a.id < b.id;
    }
};

const int kInfinity = 1 << 29;
}  // namespace

int Pathfinder::manhattan(int a, int b, int gridWidth) {
    int dx = a % gridWidth - b % gridWidth;
    int dy = a / gridWidth - b / gridWidth;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

PathResult Pathfinder::findPath(const Graph& graph, int gridWidth, int start, int goal, const DynamicArray<bool>* passable) {
    auto t0 = std::chrono::steady_clock::now();
    PathResult result;
    auto finish = [&]() {
        auto t1 = std::chrono::steady_clock::now();
        result.millis = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return result;
    };

    if (gridWidth <= 0 || !graph.isValidVertex(start) || !graph.isValidVertex(goal)) return finish();
    if (start == goal) {
        result.found = true;
        result.cost = 0;
        result.path.push_back(start);
        result.expanded.push_back(start);
        return finish();
    }
    if (passable && !(*passable)[goal]) return finish();
    if (graph.degree(goal) == 0) return finish();  // walls / isolated tiles

    const int n = graph.vertexCount();
    DynamicArray<int> gScore(n, kInfinity);
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
        if (closed[u]) continue;  // stale entry (lazy deletion)
        closed[u] = true;
        result.expanded.push_back(u);

        if (u == goal) {
            result.found = true;
            result.cost = gScore[goal];
            for (int v = goal; v != -1; v = parent[v]) result.path.push_back(v);
            result.path.reverse();
            return finish();
        }

        for (const GraphEdge* e = graph.edges(u); e; e = e->next) {
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
    return finish();  // open set exhausted: no path (FR-A2)
}
