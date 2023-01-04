#pragma once

#include <memory>
#include <utility>

// P is a type that wraps a non-owned reference to a value.
template <typename T>
struct P {
    T* get() {
        return x;
    }

    const T* get() const {
        return x;
    }

    P& operator=(const P<T>& rhs) = delete;

    P& operator=(P<T>&& rhs) {
        x = rhs.x;
        rhs.x = nullptr;
        return *this;
    }

    P& operator=(T* x) {
        this->x = x;
        return *this;
    }

    T* operator->() {
        return x;
    }

    const T* operator->() const {
        return x;
    }

    T& operator*() {
        return *x;
    }

    const T& operator*() const {
        return *x;
    }

    operator bool() const {
        return x != nullptr;
    }

    operator T*() {
        return x;
    }

    operator const T*() const {
        return x;
    }

    T& operator[](size_t i) {
        return x[i];
    }

    const T& operator[](size_t i) const {
        return x[i];
    }

    P():
        x(nullptr) {}

    P(P&& rhs):
        x(std::exchange(rhs.x, nullptr)) {}

    ~P() {}

    P(const P&) = delete;

private:
    T* x;
};

// N is a type that wraps a primitive arithmetic type, providing it with a known value
// (zero) at initialization and after a move.
template <typename T>
struct N {
    N& operator++() {
        ++x;
        return *this;
    }

    N& operator++(int) {
        x++;
        return *this;
    }

    N& operator--() {
        --x;
        return *this;
    }

    N& operator--(int) {
        x--;
        return *this;
    }

    N& operator=(T x) {
        this->x = x;
        return *this;
    }

    N& operator=(N&& rhs) {
        this->x = rhs.x;
        rhs.x = 0;
        return *this;
    }

    bool operator==(T x) {
        return this->x == x;
    }

    operator T() const {
        return x;
    }

    T* operator&() {
        return &x;
    }

    const T* operator&() const {
        return &x;
    }

    N():
        x((T) 0) {}

    N(N&& rhs):
        x(std::exchange(rhs.x, (T) 0)) {}

    N(N&) = default;

private:
    T x;
};
