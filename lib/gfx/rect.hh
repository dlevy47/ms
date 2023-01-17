#pragma once

#include <cmath>
#include <iostream>
#include <ostream>
#include <stdexcept>

#include "gfx/vector.hh"

namespace gfx {

template <typename T>
struct Rect {
    Vector<T> topleft;
    Vector<T> bottomright;

    template <typename U>
    bool contains(Vector<U> p) const {
        return (
            (p.x >= topleft.x && p.x <= bottomright.x) &&
            (p.y >= topleft.y && p.y <= bottomright.y));
    }

    template <typename U>
    bool contains(Rect<U> r) const {
        return contains(r.topleft) && contains(r.bottomright);
    }

    Vector<T> center() const {
        Vector<T> center = {
            .x = (bottomright.x + topleft.x) / 2,
            .y = (bottomright.y + topleft.y) / 2,
        };
        return center;
    }

    T width() const {
        return bottomright.x - topleft.x;
    }

    T height() const {
        return bottomright.y - topleft.y;
    }

    template <typename U>
    void translate(Vector<U> amount) {
        topleft += amount;
        bottomright += amount;
    }

    // scale changes the size of a rectangle such that the area is multiplied
    // by amount. I.e., if amount is 2, the area will be doubled. Scaling is
    // done around the center of the rectangle.
    template <typename U>
    void scale(U amount) {
        if (amount <= 0) {
            throw std::out_of_range("amount");
        }

        U factor = std::sqrt(amount);

        T width_change = (bottomright.x - topleft.x) * (factor - 1);
        T height_change = (bottomright.y - topleft.y) * (factor - 1);

        // If amount is positive, that means the rectangle grows, so topleft
        // has to move more top and more left. Vice/versa for bottomright.
        topleft.x -= width_change / 2;
        topleft.y -= height_change / 2;
        bottomright.x += width_change / 2;
        bottomright.y += height_change / 2;
    }

    template <typename U>
    void scale_about(U amount, Vector<T> p) {
        if (!contains(p)) {
            throw std::out_of_range("p");
        }

        Vector<T> distance = center() - p;
        U factor = std::sqrt(amount) - 1;
        distance *= factor;

        scale(amount);
        translate(distance);
    }

    template <typename U>
    explicit operator Rect<U>() const {
        return Rect<U> {
            .topleft = static_cast<Vector<U>>(topleft),
                .bottomright = static_cast<Vector<U>>(bottomright),
        };
    }

    T area() const {
        return (bottomright.x - topleft.x) * (bottomright.y - topleft.y);
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const Rect<T>& r) {
    os << "[" << r.topleft.x << ", " << r.topleft.y << "; "
        << r.bottomright.x << ", " << r.bottomright.y << "]";
    return os;
}

}
