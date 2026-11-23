#pragma once
// DynamicArray<T> — a team-implemented growable array (replacement for std::vector).
// Used as the backing store for the binary heap, the graph's head table, inventory
// storage and every other "list of things" in the project.
//
// Memory: owns a heap buffer allocated with new[]; follows the rule of five.

#include <stdexcept>
#include <utility>

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
        T* fresh = new T[capacity];
        for (int i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
        delete[] data_;
        data_ = fresh;
        capacity_ = capacity;
    }

    void resize(int count, const T& value = T()) {
        if (count < 0) throw std::invalid_argument("DynamicArray: negative size");
        reserve(count);
        for (int i = size_; i < count; ++i) data_[i] = value;
        size_ = count;
    }

    void push_back(const T& value) {
        // Copy first: `value` may alias an element that reserve() is about to move.
        T copy = value;
        if (size_ == capacity_) reserve(capacity_ == 0 ? 8 : capacity_ * 2);
        data_[size_++] = std::move(copy);
    }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("DynamicArray::pop_back on empty array");
        --size_;
        data_[size_] = T();
    }

    void insertAt(int index, const T& value) {
        if (index < 0 || index > size_) throw std::out_of_range("DynamicArray::insertAt");
        T copy = value;
        if (size_ == capacity_) reserve(capacity_ == 0 ? 8 : capacity_ * 2);
        for (int i = size_; i > index; --i) data_[i] = std::move(data_[i - 1]);
        data_[index] = std::move(copy);
        ++size_;
    }

    void removeAt(int index) {
        if (index < 0 || index >= size_) throw std::out_of_range("DynamicArray::removeAt");
        for (int i = index; i < size_ - 1; ++i) data_[i] = std::move(data_[i + 1]);
        --size_;
        data_[size_] = T();
    }

    void clear() { size_ = 0; }

    void reverse() {
        for (int i = 0, j = size_ - 1; i < j; ++i, --j) std::swap(data_[i], data_[j]);
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

    // Raw-pointer iteration so range-based for loops work.
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    T* data_ = nullptr;
    int size_ = 0;
    int capacity_ = 0;
};
