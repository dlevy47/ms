#include "client/dataset.hh"

#include <codecvt>

#include "logger.hh"

namespace client {

template <typename T>
std::string convert(const T* s);

template <>
std::string convert(const char* s) {
    return std::string(s);
}

template<>
std::string convert(const wchar_t* s) {
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(s);
}

Error Dataset::opendirectory(
        Dataset* self,
        const std::filesystem::path& path) {
    struct ToOpen {
        Wz* into;
        const char* basename;
    };
    const ToOpen to_open[] = {
        {
            .into = &self->base,
            .basename = "Base.wz",
        },
        {
            .into = &self->character,
            .basename = "Character.wz",
        },
        {
            .into = &self->effect,
            .basename = "Effect.wz",
        },
        {
            .into = &self->etc,
            .basename = "Etc.wz",
        },
        {
            .into = &self->item,
            .basename = "Item.wz",
        },
        {
            .into = &self->map,
            .basename = "Map.wz",
        },
        {
            .into = &self->mob,
            .basename = "Mob.wz",
        },
        {
            .into = &self->morph,
            .basename = "Morph.wz",
        },
        {
            .into = &self->npc,
            .basename = "Npc.wz",
        },
        {
            .into = &self->quest,
            .basename = "Quest.wz",
        },
        {
            .into = &self->reactor,
            .basename = "Reactor.wz",
        },
        {
            .into = &self->skill,
            .basename = "Skill.wz",
        },
        {
            .into = &self->sound,
            .basename = "Sound.wz",
        },
        {
            .into = &self->string,
            .basename = "String.wz",
        },
        {
            .into = &self->taming_mob,
            .basename = "TamingMob.wz",
        },
        {
            .into = &self->ui,
            .basename = "UI.wz",
        },
    };

    for (size_t i = 0, l = sizeof(to_open) / sizeof(*to_open); i < l; ++i) {
        std::filesystem::path wz_path = path / to_open[i].basename;
        LOG(INFO)
            << "loading " << wz_path;

        std::string path_converted = convert(wz_path.c_str());

        CHECK(wz::Wz::open(&to_open[i].into->wz, path_converted.c_str()),
                Error::OPENFAILED) << "failed to open " << to_open[i].basename;
        CHECK(wz::Vfs::open(&to_open[i].into->vfs, &to_open[i].into->wz),
                Error::OPENFAILED) << "failed to build vfs for " << to_open[i].basename;

        LOG(Logger::INFO)
            << "loaded " << to_open[i].basename;
    }

    return Error();
}

}
