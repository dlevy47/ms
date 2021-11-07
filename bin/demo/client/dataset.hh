#pragma once

#include <filesystem>

#include "util/error.hh"
#include "wz/vfs.hh"
#include "wz/wz.hh"

namespace client {

struct Wz {
    wz::Wz wz;
    wz::Vfs vfs;

    Wz():
        wz(),
        vfs() {}

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

    Dataset():
        base(),
        character(),
        effect(),
        etc(),
        item(),
        map(),
        mob(),
        morph(),
        npc(),
        quest(),
        reactor(),
        skill(),
        sound(),
        string(),
        taming_mob(),
        ui() {}

    Dataset(const Dataset&) = delete;
};

}
