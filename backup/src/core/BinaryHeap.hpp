#pragma once
// BinaryHeap<T, Compare> — a team-implemented array-backed binary heap.
//
// `Compare(a, b)` returns true when `a` must leave the heap before `b`.
// Supplying a "less than" comparator gives a min-heap; "greater than" a max-heap.
//
// Used by:
//   * the A* pathfinder (open set, lowest f-cost first), and
//   * the battle system (turn queue, earliest next-action time first).
//
// Complexity: push / pop O(log n), top O(1).

#include "DynamicArray.hpp"

#include <stdexcept>
#include <utility>

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
        data_[0] = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return result;
    }

    // Non-throwing extract: returns false if the heap was empty.
    bool tryPop(T& out) {
        if (data_.empty()) return false;
        out = pop();
        return true;
    }

    int size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }

    // Underlying array in heap order (index 0 is the root). Read-only.
    const DynamicArray<T>& raw() const { return data_; }

    // Verifies the heap property for every parent/child pair (used by tests).
    bool isValid() const {
        for (int i = 1; i < data_.size(); ++i) {
            if (compare_(data_[i], data_[(i - 1) / 2])) return false;
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
