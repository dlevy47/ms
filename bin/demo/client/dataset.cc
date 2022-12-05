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

static void openfiles(
        std::vector<std::wstring>* names,
        std::wstring prefix,
        const wz::Vfs::Directory* dir) {
    for (const auto& it : dir->children) {
        const wz::Vfs::Node* child = &it.second;
        if (const wz::Vfs::File* file = child->file()) {
            if (file->rc) {
                names->push_back(prefix + L"/" + it.first);
            }
        } else if (const wz::Vfs::Directory* directory = child->directory()) {
            openfiles(
                    names,
                    prefix + L"/" + it.first,
                    directory);
        }
    }
}

std::vector<std::wstring> Dataset::openfiles() const {
    struct ToOpen {
        const Wz* into;
        const wchar_t* basename;
    };
    const ToOpen to_open[] = {
        {
            .into = &base,
            .basename = L"Base.wz",
        },
        {
            .into = &character,
            .basename = L"Character.wz",
        },
        {
            .into = &effect,
            .basename = L"Effect.wz",
        },
        {
            .into = &etc,
            .basename = L"Etc.wz",
        },
        {
            .into = &item,
            .basename = L"Item.wz",
        },
        {
            .into = &map,
            .basename = L"Map.wz",
        },
        {
            .into = &mob,
            .basename = L"Mob.wz",
        },
        {
            .into = &morph,
            .basename = L"Morph.wz",
        },
        {
            .into = &npc,
            .basename = L"Npc.wz",
        },
        {
            .into = &quest,
            .basename = L"Quest.wz",
        },
        {
            .into = &reactor,
            .basename = L"Reactor.wz",
        },
        {
            .into = &skill,
            .basename = L"Skill.wz",
        },
        {
            .into = &sound,
            .basename = L"Sound.wz",
        },
        {
            .into = &string,
            .basename = L"String.wz",
        },
        {
            .into = &taming_mob,
            .basename = L"TamingMob.wz",
        },
        {
            .into = &ui,
            .basename = L"UI.wz",
        },
    };

    std::vector<std::wstring> names;
    for (size_t i = 0; i < sizeof(to_open) / sizeof(*to_open); ++i) {
        ::client::openfiles(
                &names,
                to_open[i].basename,
                to_open[i].into->vfs.root.directory());
    }

    return names;
}

}
