#pragma once
// Graph — undirected weighted graph stored as an adjacency list.
//
// Each vertex owns a singly linked list of GraphEdge nodes (team-implemented,
// no std::list). In Dungeonbound every walkable tile is a vertex and every pair
// of orthogonally adjacent walkable tiles is joined by an edge of weight 1.

#include "DynamicArray.hpp"

struct GraphEdge {
    int to = -1;
    int weight = 1;
    GraphEdge* next = nullptr;
};

class Graph {
public:
    explicit Graph(int vertexCount = 0);
    Graph(const Graph& other);
    Graph(Graph&& other) noexcept;
    Graph& operator=(const Graph& other);
    Graph& operator=(Graph&& other) noexcept;
    ~Graph();

    // Resets the graph to `vertexCount` isolated vertices.
    void reset(int vertexCount);

    // Appends a new isolated vertex and returns its id.
    int addVertex();

    // Adds an undirected edge. Rejects invalid ids, self loops and duplicates.
    bool addEdge(int u, int v, int weight = 1);
    bool removeEdge(int u, int v);
    bool hasEdge(int u, int v) const;

    bool isValidVertex(int v) const { return v >= 0 && v < heads_.size(); }
    int vertexCount() const { return heads_.size(); }
    int edgeCount() const { return edgeCount_; }  // undirected edges
    int degree(int v) const;

    // Head of the adjacency list for `v` (nullptr if none or invalid).
    const GraphEdge* edges(int v) const;
    DynamicArray<int> neighbors(int v) const;

    // Breadth-first search helpers used for connectivity validation.
    DynamicArray<int> bfsDistances(int source) const;  // -1 = unreachable
    bool isConnected(int a, int b) const;
    int reachableCount(int source) const;

private:
    bool addDirected(int u, int v, int weight);
    bool removeDirected(int u, int v);
    void freeAll();
    void copyFrom(const Graph& other);

    DynamicArray<GraphEdge*> heads_;
    int edgeCount_ = 0;
};
