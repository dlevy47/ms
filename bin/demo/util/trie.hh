#pragma once

#include <cwctype>
#include <unordered_map>
#include <vector>

#include "util/error.hh"

namespace util {

template <typename V>
struct Trie {
    struct Node {
        std::unordered_map<wchar_t, Node> children;

        std::vector<V> vs;

        Node() = default;
        Node(const Node&) = delete;
        Node(Node&&) = default;
    };

    Node root;

    // findprefix returns all values with the specified prefix.
    std::vector<V> findprefix(
            const wchar_t* prefix) const {
        const Node* node = findnode(
                &root,
                prefix);

        std::vector<V> ret;
        if (node == nullptr) {
            return ret;
        }

        accumulate(
                &ret,
                node);
        return ret;
    }

    // find looks for and returns a value with exactly the specified
    // name, if there is one.
    Error find(
            V* x,
            const wchar_t* name) const {
        const Node* node = findnode(
                &root,
                name);

        if (node && node->vs.size() == 1) {
            *x = node->vs[0];
            return Error();
        }

        return error_new(Error::NOTFOUND) << "no vs found for name " << name;
    }

    // findmultiple returns multiple values with exactly the specified
    // name.
    std::vector<V> findmultiple(
            const wchar_t* name) const;

    void insert(
            const wchar_t* name,
            V x) {
        insert(
                &root,
                name,
                x);
    }

    Trie() = default;
    Trie(const Trie&) = delete;
    Trie(Trie&&) = default;

private:
    void accumulate(
            std::vector<V>* ret,
            const Node* node) const {
        ret->insert(
                ret->end(),
                node->vs.begin(),
                node->vs.end());

        for (auto it = node->children.begin(); it != node->children.end(); ++it) {
            accumulate(
                    ret,
                    &it->second);
        }
    }

    const Node* findnode(
            const Node* node,
            const wchar_t* prefix) const {
        if (prefix[0] == L'\0') {
            return node;
        }

        auto it = node->children.find(std::towlower(prefix[0]));
        if (it == node->children.end()) {
            return nullptr;
        }

        return findnode(
                &it->second,
                prefix + 1);
    }

    void insert(
            Node* node,
            const wchar_t* name,
            V x) {
    if (name[0] == L'\0') {
        node->vs.push_back(x);
        return;
    }

    // Using the [] operator on node->children creates the child node,
    // if necessary.
    insert(
            &node->children[std::towlower(name[0])],
            name + 1,
            x);
    }
};

}
