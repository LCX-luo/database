#ifndef ARRAY_LIST_H
#define ARRAY_LIST_H

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace minidb {

/**
 * @brief 自定义动态数组容器，替代 std::vector
 * 
 * 任务要求禁止使用 STL 标准容器，故自行实现 ArrayList。
 * 支持动态扩容、迭代器、基本容器操作。
 */
template<typename T>
class ArrayList {
private:
    T* data_;
    size_t capacity_;
    size_t size_;

    void reserve(size_t new_cap) {
        if (new_cap <= capacity_) return;
        T* new_data = new T[new_cap];
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = data_[i];
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_cap;
    }

public:
    // 迭代器
    class Iterator {
    private:
        T* ptr_;
    public:
        explicit Iterator(T* ptr) : ptr_(ptr) {}
        T& operator*() { return *ptr_; }
        T* operator->() { return ptr_; }
        Iterator& operator++() { ++ptr_; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++ptr_; return tmp; }
        bool operator==(const Iterator& other) const { return ptr_ == other.ptr_; }
        bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }
    };

    class ConstIterator {
    private:
        const T* ptr_;
    public:
        explicit ConstIterator(const T* ptr) : ptr_(ptr) {}
        const T& operator*() const { return *ptr_; }
        const T* operator->() const { return ptr_; }
        ConstIterator& operator++() { ++ptr_; return *this; }
        ConstIterator operator++(int) { ConstIterator tmp = *this; ++ptr_; return tmp; }
        bool operator==(const ConstIterator& other) const { return ptr_ == other.ptr_; }
        bool operator!=(const ConstIterator& other) const { return ptr_ != other.ptr_; }
    };

    ArrayList() : data_(nullptr), capacity_(0), size_(0) {}

    explicit ArrayList(size_t initial_cap) 
        : data_(new T[initial_cap]), capacity_(initial_cap), size_(0) {}

    ArrayList(const ArrayList& other) 
        : data_(new T[other.capacity_]), capacity_(other.capacity_), size_(other.size_) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    ArrayList& operator=(const ArrayList& other) {
        if (this != &other) {
            delete[] data_;
            capacity_ = other.capacity_;
            size_ = other.size_;
            data_ = new T[capacity_];
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
        }
        return *this;
    }

    ~ArrayList() {
        delete[] data_;
        data_ = nullptr;
        capacity_ = 0;
        size_ = 0;
    }

    void push_back(const T& value) {
        if (size_ >= capacity_) {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        data_[size_++] = value;
    }

    void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

    T& at(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("ArrayList index out of range");
        }
        return data_[index];
    }

    const T& at(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("ArrayList index out of range");
        }
        return data_[index];
    }

    T& operator[](size_t index) {
        return data_[index];
    }

    const T& operator[](size_t index) const {
        return data_[index];
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    void clear() {
        size_ = 0;
    }

    void resize(size_t new_size) {
        if (new_size > capacity_) {
            reserve(new_size);
        }
        size_ = new_size;
    }

    T* data() { return data_; }
    const T* data() const { return data_; }

    Iterator begin() { return Iterator(data_); }
    Iterator end() { return Iterator(data_ + size_); }
    ConstIterator begin() const { return ConstIterator(data_); }
    ConstIterator end() const { return ConstIterator(data_ + size_); }

    // 查找元素位置，返回索引；未找到返回 -1
    int find(const T& value) const {
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == value) return static_cast<int>(i);
        }
        return -1;
    }

    // 在指定位置插入
    void insert_at(size_t index, const T& value) {
        if (index > size_) {
            throw std::out_of_range("ArrayList insert_at index out of range");
        }
        if (size_ >= capacity_) {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        for (size_t i = size_; i > index; --i) {
            data_[i] = data_[i - 1];
        }
        data_[index] = value;
        ++size_;
    }

    // 删除指定位置元素
    void erase_at(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("ArrayList erase_at index out of range");
        }
        for (size_t i = index; i < size_ - 1; ++i) {
            data_[i] = data_[i + 1];
        }
        --size_;
    }

    // 删除指定元素（第一个匹配）
    void remove(const T& value) {
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == value) {
                erase_at(i);
                return;
            }
        }
    }
};

} // namespace minidb

#endif // ARRAY_LIST_H
