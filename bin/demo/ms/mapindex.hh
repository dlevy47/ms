#pragma once

#include <string>
#include <unordered_map>

#include "ms/map.hh"
#include "util/trie.hh"

namespace ms {

struct MapIndex {
    wz::Vfs::File::Handle map_file;

    std::unordered_map<ms::Map::ID, const wchar_t*, ms::Map::ID::Hash> id_to_name;

    util::Trie<ms::Map::ID> name_to_id;

    util::Trie<std::pair<ms::Map::ID, uint32_t>> keyword_to_id;

    // name returns the name of the map with the specified ID, if there is one.
    inline const wchar_t* name(
        ms::Map::ID id) const {
        auto it = id_to_name.find(id);
        if (it == id_to_name.end()) {
            return nullptr;
        }

        return it->second;
    }

    // search returns a list of map IDs for the provided search term. The results in the
    // returned vector are ordered in descending "relevance"; i.e. the first result is
    // "most relevant". For now, relevance is defined as the index of the search term in
    // the map name. That is, if the keyword appears at the beginning of the map name, it
    // is considered to be more relevant than if the keyword appears later.
    std::vector<ms::Map::ID> search(
        const wchar_t* name) const;

    static Error load(
        MapIndex* self,
        wz::Vfs::File::Handle&& map_file);

    MapIndex() = default;
    MapIndex(const MapIndex&) = delete;
    MapIndex(MapIndex&&) = default;
};

}
