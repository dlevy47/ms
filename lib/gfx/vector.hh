#pragma once

#include <ostream>

namespace gfx {

template <typename T>
struct Vector {
    T x;
    T y;

    template <typename U>
    Vector<T> operator-(const Vector<U>& rhs) const {
        Vector<T> ret = *this;
        ret.x -= rhs.x;
        ret.y -= rhs.y;

        return ret;
    }

    template <typename U>
    Vector<T> operator+(const Vector<U>& rhs) const {
        Vector<T> ret = *this;
        ret.x += rhs.x;
        ret.y += rhs.y;

        return ret;
    }

    template <typename U>
    Vector<T>& operator-=(U rhs) {
        x -= rhs;
        y -= rhs;

        return *this;
    }

    template <typename U>
    Vector<T>& operator+=(const Vector<U>& rhs) {
        x += rhs.x;
        y += rhs.y;

        return *this;
    }

    template <typename U>
    Vector<T> operator*(U scale) const {
        Vector<T> ret = *this;
        ret.x *= scale;
        ret.y *= scale;

        return ret;
    }

    template <typename U>
    Vector<T>& operator*=(U scale) {
        this->x *= scale;
        this->y *= scale;

        return *this;
    }

    template <typename U>
    Vector<T> operator/(U scale) const {
        Vector<T> ret = *this;
        ret.x /= scale;
        ret.y /= scale;

        return ret;
    }

    template <typename U>
    Vector<T>& operator/=(U scale) {
        this->x /= scale;
        this->y /= scale;

        return *this;
    }

    template <typename U>
    explicit operator Vector<U>() const {
        return Vector<U> {
            .x = static_cast<U>(x),
                .y = static_cast<U>(y),
        };
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const Vector<T>& v) {
    os << "[" << v.x << ", " << v.y << "]";
    return os;
}

}
