#include "ms/mapindex.hh"

#include <cwchar>

#include "logger.hh"

namespace ms {

static bool keyword(
        size_t* at,
        std::wstring* keyword,
        const std::wstring* s) {
    const wchar_t* SPACES = L" \n";
    *at = s->find_first_not_of(SPACES, *at);

    if (*at == std::wstring::npos) {
        return false;
    }

    size_t end = s->find_first_of(SPACES, *at);
    if (end == std::wstring::npos) {
        end = s->size();
    }

    *keyword = s->substr(*at, end - *at);

    *at = end;
    return true;
}

std::vector<ms::Map::ID> MapIndex::search(
        const wchar_t* name) const {
    std::wstring name_s(name);

    struct Metric {
        size_t position { 0 };
        size_t length { 0 };
        size_t name_length { 0 };

        bool operator==(const Metric& rhs) const {
            return position == rhs.position &&
                length == rhs.length;
        }

        bool operator<(const Metric& rhs) const {
            return position < rhs.position ||
                length > rhs.length;
        }

        bool operator>(const Metric& rhs) const {
            return *this < rhs;
        }
    };
    std::unordered_map<ms::Map::ID, Metric, ms::Map::ID::Hash> found;

    // First, look for IDs that have name as a prefix.
    {
        std::vector<ms::Map::ID> ids =
            name_to_id.findprefix(name);

        for (const auto& id : ids) {
            Metric m = {
                .position = 0,
                .length = name_s.size(),
                .name_length = std::wcslen(id_to_name.at(id)),
            };

            found[id] = m;
        }
    }

    // Now, find matches based on keywords.
    // We keep track of keyword matches separately so we can enforce order
    // in the matches. Keyword matches are done on a prefix basis.
    std::vector<std::wstring> keywords;
    {
        size_t at = 0;
        std::wstring k;
        while (keyword(&at, &k, &name_s)) {
            keywords.push_back(k);
        }
    }

    struct KeywordMatch {
        size_t start { 0 };
        size_t last_keyword { 0 };
        size_t length { 0 };
    };

    std::unordered_map<ms::Map::ID, KeywordMatch, ms::Map::ID::Hash> keyword_matches;
    for (size_t i = 0; i < keywords.size(); ++i) {
        std::vector<std::pair<ms::Map::ID, uint32_t>> this_matches =
            keyword_to_id.findprefix(keywords[i].c_str());

        // Filter keyword_matches, removing matches that don't have this keyword.
        if (i > 0) {
            std::unordered_map<ms::Map::ID, uint32_t, ms::Map::ID::Hash> sieve;

            for (const auto it : this_matches) {
                sieve[it.first] = it.second;
            }

            // unordered_map.erase does not invalidate iterators.
            auto it = keyword_matches.begin();
            while (it != keyword_matches.end()) {
                const auto sieve_at = sieve.find(it->first);
                if (sieve_at == sieve.end() || sieve_at->second < it->second.last_keyword) {
                    it = keyword_matches.erase(it);
                    continue;
                }

                it->second.last_keyword = sieve_at->second;
                it->second.length += keywords[i].size();
                ++it;
            }
        } else {
            for (const auto it : this_matches) {
                KeywordMatch match = {
                    .start = it.second,
                    .length = keywords[i].size(),
                };

                keyword_matches[it.first] = match;
            }
        }
    }

    // Now that we have keyword matches, add them to found.
    for (const auto it : keyword_matches) {
        Metric m = {
            .position = it.second.start,
            .length = it.second.length,
        };

        // Only replace a previous find if it's better.
        auto found_it = found.find(it.first);
        if (found_it == found.end() || m > found_it->second) {
            found[it.first] = m;
        }
    }

    // Finally, sort found into the return vector.
    std::vector<ms::Map::ID> ret;
    for (const auto it : found) {
        ret.push_back(it.first);
    }

    std::sort(
            ret.begin(),
            ret.end(),
            [&](const ms::Map::ID& left_id, const ms::Map::ID& right_id) {
            const auto& left = found[left_id];
            const auto& right = found[right_id];

            if (left == right) {
                return left.name_length < right.name_length;
            }

            return left < right;
            });

    return ret;
}

Error MapIndex::load(
        MapIndex* self,
        wz::Vfs::File::Handle&& map_file) {
    self->map_file = std::move(map_file);

    auto realm_it = self->map_file->iterator();

    uint32_t count = 0;
    const wz::OpenedFile::Node* realm = nullptr;
    while ((realm = realm_it.next())) {
        auto map_it = realm->iterator();

        const wz::OpenedFile::Node* map = nullptr;
        while ((map = map_it.next())) {
            ms::Map::ID this_id;
            if (ms::Map::ID::from(
                        &this_id,
                        map->name)) {
                LOG(Logger::WARNING) << "map strings node name is unexpectedly "
                    << "not a map ID: " << realm->name << "/" << map->name;
                continue;
            }

            if (self->id_to_name.contains(this_id)) {
                LOG(Logger::WARNING) << "map strings node " << realm->name <<
                    "/" << map->name << " is repeated";
                continue;
            }

            const wchar_t* map_name = nullptr;
            if (map->childstring(
                        L"mapName",
                        &map_name)) {
                LOG(Logger::WARNING) << "map strings node " << realm->name <<
                    "/" << map->name << " has no mapName";
                continue;
            }

            // Keywords for map names are just space delimited. First, the name
            // needs to be sanitized.
            std::wstring map_name_s;
            {
                const wchar_t* map_name_at = map_name;
                while (*map_name_at) {
                    if (std::iswalnum(*map_name_at) || std::iswspace(*map_name_at)) {
                        map_name_s.push_back(std::towlower(*map_name_at));
                    } else {
                        // To avoid smushing together keywords that should be
                        // separate, add a space if we see a character that
                        // we discard.
                        map_name_s.push_back(L' ');
                    }

                    ++map_name_at;
                }
            }

            // For each keyword, insert the map ID.
            size_t at = 0;
            std::wstring k;
            while (keyword(&at, &k, &map_name_s)) {
                self->keyword_to_id.insert(
                        k.c_str(),
                        std::make_pair(this_id, at));
            }

            self->id_to_name[this_id] = map_name;
            self->name_to_id.insert(map_name, this_id);
            ++count;
        }
    }

    LOG(Logger::INFO) << "found " << count << " map strings";
    return Error();
}

}
