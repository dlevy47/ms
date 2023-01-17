#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "util/error.hh"
#include "wz/vfs.hh"
#include "wz/wz.hh"

namespace client {

struct Wz {
    wz::Wz wz;
    wz::Vfs vfs;

    Wz() = default;
    // Move and copy are prohibited because vfs contains a pointer to wz.
    Wz(Wz&&) = delete;
    Wz(const Wz&) = delete;
};

struct Dataset {
    Wz base;
    Wz character;
    Wz effect;
    Wz etc;
    Wz item;
    Wz map;
    Wz mob;
    Wz morph;
    Wz npc;
    Wz quest;
    Wz reactor;
    Wz skill;
    Wz sound;
    Wz string;
    Wz taming_mob;
    Wz ui;

    static Error opendirectory(
        Dataset* self,
        const std::filesystem::path& path);

    std::vector<std::wstring> openfiles() const;

    Dataset() = default;
    Dataset(const Dataset&) = delete;
};

}
