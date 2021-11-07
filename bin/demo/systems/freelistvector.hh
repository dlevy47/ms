#pragma once

#include <deque>
#include <optional>
#include <vector>

namespace systems {

template <typename T>
struct FreelistVector {
    std::vector<std::optional<T>> items;
    std::deque<size_t> free;

    size_t insert(T&& x) {
        if (!free.empty()) {
            size_t at = free.front();
            free.pop_front();
            items[at] = std::move(x);

            return at;
        }

        items.emplace_back(std::move(x));
        return items.size() - 1;
    }

    void remove(size_t index) {
        items[index].reset();
        free.push_front(index);
    }
};

}
