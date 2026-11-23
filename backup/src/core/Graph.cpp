#include "Graph.hpp"

Graph::Graph(int vertexCount) { reset(vertexCount); }

Graph::Graph(const Graph& other) { copyFrom(other); }

Graph::Graph(Graph&& other) noexcept : heads_(std::move(other.heads_)), edgeCount_(other.edgeCount_) {
    other.edgeCount_ = 0;
}

Graph& Graph::operator=(const Graph& other) {
    if (this != &other) {
        freeAll();
        copyFrom(other);
    }
    return *this;
}

Graph& Graph::operator=(Graph&& other) noexcept {
    if (this != &other) {
        freeAll();
        heads_ = std::move(other.heads_);
        edgeCount_ = other.edgeCount_;
        other.edgeCount_ = 0;
    }
    return *this;
}

Graph::~Graph() { freeAll(); }

void Graph::reset(int vertexCount) {
    freeAll();
    if (vertexCount < 0) vertexCount = 0;
    heads_.resize(vertexCount, nullptr);
}

int Graph::addVertex() {
    heads_.push_back(nullptr);
    return heads_.size() - 1;
}

bool Graph::addEdge(int u, int v, int weight) {
    if (!isValidVertex(u) || !isValidVertex(v) || u == v || weight < 0) return false;
    if (hasEdge(u, v)) return false;
    addDirected(u, v, weight);
    addDirected(v, u, weight);
    ++edgeCount_;
    return true;
}

bool Graph::removeEdge(int u, int v) {
    if (!isValidVertex(u) || !isValidVertex(v)) return false;
    if (!removeDirected(u, v)) return false;
    removeDirected(v, u);
    --edgeCount_;
    return true;
}

bool Graph::hasEdge(int u, int v) const {
    if (!isValidVertex(u) || !isValidVertex(v)) return false;
    for (const GraphEdge* e = heads_[u]; e; e = e->next) {
        if (e->to == v) return true;
    }
    return false;
}

int Graph::degree(int v) const {
    if (!isValidVertex(v)) return 0;
    int count = 0;
    for (const GraphEdge* e = heads_[v]; e; e = e->next) ++count;
    return count;
}

const GraphEdge* Graph::edges(int v) const { return isValidVertex(v) ? heads_[v] : nullptr; }

DynamicArray<int> Graph::neighbors(int v) const {
    DynamicArray<int> result;
    for (const GraphEdge* e = edges(v); e; e = e->next) result.push_back(e->to);
    return result;
}

DynamicArray<int> Graph::bfsDistances(int source) const {
    DynamicArray<int> dist(vertexCount(), -1);
    if (!isValidVertex(source)) return dist;

    // Array-backed FIFO queue: `head` advances instead of erasing from the front.
    DynamicArray<int> queue;
    queue.reserve(vertexCount());
    int head = 0;
    dist[source] = 0;
    queue.push_back(source);
    while (head < queue.size()) {
        int u = queue[head++];
        for (const GraphEdge* e = heads_[u]; e; e = e->next) {
            if (dist[e->to] == -1) {
                dist[e->to] = dist[u] + 1;
                queue.push_back(e->to);
            }
        }
    }
    return dist;
}

bool Graph::isConnected(int a, int b) const {
    if (!isValidVertex(a) || !isValidVertex(b)) return false;
    if (a == b) return true;
    return bfsDistances(a)[b] != -1;
}

int Graph::reachableCount(int source) const {
    if (!isValidVertex(source)) return 0;
    DynamicArray<int> dist = bfsDistances(source);
    int count = 0;
    for (int d : dist) {
        if (d != -1) ++count;
    }
    return count;
}

bool Graph::addDirected(int u, int v, int weight) {
    // Prepend: O(1) insertion at the head of u's list.
    GraphEdge* edge = new GraphEdge{v, weight, heads_[u]};
    heads_[u] = edge;
    return true;
}

bool Graph::removeDirected(int u, int v) {
    GraphEdge* prev = nullptr;
    for (GraphEdge* e = heads_[u]; e; prev = e, e = e->next) {
        if (e->to == v) {
            if (prev) prev->next = e->next;
            else heads_[u] = e->next;
            delete e;
            return true;
        }
    }
    return false;
}

void Graph::freeAll() {
    for (int i = 0; i < heads_.size(); ++i) {
        GraphEdge* e = heads_[i];
        while (e) {
            GraphEdge* next = e->next;
            delete e;
            e = next;
        }
        heads_[i] = nullptr;
    }
    heads_.clear();
    edgeCount_ = 0;
}

void Graph::copyFrom(const Graph& other) {
    heads_.resize(other.heads_.size(), nullptr);
    for (int i = 0; i < other.heads_.size(); ++i) {
        // Append at the tail to preserve the source's edge order.
        GraphEdge* tail = nullptr;
        for (const GraphEdge* e = other.heads_[i]; e; e = e->next) {
            GraphEdge* copy = new GraphEdge{e->to, e->weight, nullptr};
            if (tail) tail->next = copy;
            else heads_[i] = copy;
            tail = copy;
        }
    }
    edgeCount_ = other.edgeCount_;
}
