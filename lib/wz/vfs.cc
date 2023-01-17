#include "wz/vfs.hh"

#include <string_view>

namespace wz {

static const OpenedFile::Node* OpenedFile_Node_find(
    const OpenedFile::Node* self,
    const std::wstring_view& path) {
    if (path.size() == 0)
        return self;

    if (self->children.count == 0)
        return nullptr;

    std::wstring_view this_path = path;
    std::wstring_view next_path;

    size_t next_slash = path.find(L'/');
    if (next_slash != std::wstring_view::npos) {
        this_path = path.substr(0, next_slash);
        next_path = path.substr(next_slash + 1);
    }

    // Special case for ...
    if (this_path == L"..") {
        // Only the root node has no parent.
        if (!self->parent)
            return nullptr;

        return OpenedFile_Node_find(
            self->parent,
            next_path);
    }

    for (uint32_t i = 0, l = self->children.count; i < l; ++i) {
        if (this_path == self->children.start[i].name) {
            return OpenedFile_Node_find(
                &self->children.start[i],
                next_path);
        }
    }

    return nullptr;
}

const OpenedFile::Node* OpenedFile::Node::find(
    const wchar_t* path) const {
    return OpenedFile_Node_find(
        this,
        path);
}

Error OpenedFile::Node::childint32(
    const wchar_t* n,
    int32_t* x) const {
    const OpenedFile::Node* child = find(n);
    if (!child) {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " does not exist";
    }

    if (const int32_t* ip = std::get_if<int32_t>(&child->value)) {
        *x = *ip;
    } else {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " is not int32";
    }

    return Error();
}

Error OpenedFile::Node::childvector(
    const wchar_t* n,
    int32_t* x,
    int32_t* y) const {
    const OpenedFile::Node* child = find(n);
    if (!child) {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " does not exist";
    }

    if (const Vector* v = std::get_if<Vector>(&child->value)) {
        *x = v->x;
        *y = v->y;
    } else {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " is not vector";
    }

    return Error();
}

Error OpenedFile::Node::childstring(
    const wchar_t* n,
    const wchar_t** x) const {
    const OpenedFile::Node* child = find(n);
    if (!child) {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " does not exist";
    }

    if (const String* s = std::get_if<String>(&child->value)) {
        *x = s->string;
    } else {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " is not string";
    }

    return Error();
}

struct Sizes {
    size_t strings;
    size_t images;
    size_t children;
    size_t nodes;
};

template <typename C>
static Error container_computesizes(
    const wz::Wz* wz,
    Sizes* sizes,
    const C& container) {
    sizes->children += container.count;
    sizes->nodes += container.count;

    auto it = container.iterator(wz);
    while (it) {
        wz::Property p;
        CHECK(it.next(&p),
            Error::BADREAD) << "failed to read property";

        // Extra byte for null terminator.
        sizes->strings += p.name.len + 1;

        switch (p.property.index()) {
        case 5:
        {
            sizes->strings += std::get_if<5>(&p.property)->len + 1;
        } break;
        case 6:
        {
            CHECK(container_computesizes(wz, sizes, *std::get_if<6>(&p.property)),
                Error::FILEOPENFAILED) << "failed to compute container sizes";
            ++sizes->nodes;
        } break;
        case 7:
        {
            CHECK(container_computesizes(wz, sizes, *std::get_if<7>(&p.property)),
                Error::FILEOPENFAILED) << "failed to compute named container sizes";
            ++sizes->nodes;
        } break;
        case 8:
        {
            const Canvas* c = std::get_if<8>(&p.property);
            sizes->images += c->image.rawsize();
            CHECK(container_computesizes(wz, sizes, c->children),
                Error::FILEOPENFAILED) << "failed to compute canvas child sizes";
            ++sizes->nodes;
        } break;
        case 11:
        {
            sizes->strings += std::get_if<11>(&p.property)->uol.len + 1;
        } break;
        }
    }

    return Error();
}

struct Cursor {
    uint32_t node;
    wchar_t* string;
    uint8_t* image;
};

static Error OpenedFile_open_property(
    Cursor* cursor,
    const wz::Wz* wz,
    OpenedFile* of,
    const wz::Property* p,
    const wz::File* f) {
    OpenedFile::Node& node = of->nodes[cursor->node];

    CHECK(p->name.decrypt(cursor->string),
        Error::FILEOPENFAILED) << "failed to decrypt property name";
    node.name = cursor->string;
    cursor->string += p->name.len + 1;

    switch (p->property.index()) {
    case 0:
        node.value = Void{};
        break;
    case 1:
        node.value = *std::get_if<1>(&p->property);
        break;
    case 2:
        node.value = *std::get_if<2>(&p->property);
        break;
    case 3:
        node.value = *std::get_if<3>(&p->property);
        break;
    case 4:
        node.value = *std::get_if<4>(&p->property);
        break;
    case 9:
        node.value = *std::get_if<9>(&p->property);
        break;
    case 5:
    {
        const wz::String* string = std::get_if<5>(&p->property);
        CHECK(string->decrypt(cursor->string),
            Error::FILEOPENFAILED) << "failed to decrypt property string";
        node.value = wz::OpenedFile::String{
            cursor->string,
        };

        cursor->string += string->len + 1;
    } break;
    case 8:
    {
        const wz::Canvas* canvas = std::get_if<8>(&p->property);
        CHECK(canvas->image.pixels(cursor->image),
            Error::FILEOPENFAILED) << "failed to retrieve image pixels of property ";

        wz::OpenedFile::Canvas node_canvas;
        node_canvas.image = canvas->image;
        node_canvas.image_data = cursor->image;
        node.value = node_canvas;

        cursor->image += canvas->image.rawsize();
    } break;
    case 11:
    {
        const wz::Uol* uol = std::get_if<11>(&p->property);
        CHECK(uol->uol.decrypt(cursor->string),
            Error::FILEOPENFAILED) << "failed to decrypt property string";
        node.value = wz::OpenedFile::Uol{
            cursor->string,
        };

        cursor->string += uol->uol.len + 1;
    } break;
    case 10:
        // TODO: Sound
        break;
    case 6:
    case 7:
        // PropertyContainer and NamedPropertyContainer are handled
        // elsewhere.
        break;
    }

    return Error();
}

template <typename C>
static Error OpenedFile_open_container(
    Cursor* cursor,
    const wz::Wz* wz,
    OpenedFile* of,
    OpenedFile::Node* self,
    const C& c,
    uint32_t this_index,
    const wz::File* f) {
    uint32_t children_start = cursor->node;
    self->children.start = &of->nodes[cursor->node + 1];  // + 1 because cursor->node points to self
    self->children.count = c.count;

    // Here, iterate through the container's children twice: the first time, open properties, but
    // don't descend into child containers. The second time, descend into child containers. This
    // ensures that child spans are all contiguous.

    auto it1 = c.iterator(wz);
    while (it1) {
        ++cursor->node;

        wz::Property p;
        CHECK(it1.next(&p),
            Error::FILEOPENFAILED) << "failed to parse child";
        CHECK(OpenedFile_open_property(
            cursor, wz, of, &p, f),
            Error::FILEOPENFAILED) << "failed to open child";
        of->nodes[cursor->node].parent = self;
    }

    // Now, replay the children of this container, except this time descending into containers.
    auto it2 = c.iterator(wz);
    while (it2) {
        ++children_start;

        wz::Property p;
        CHECK(it2.next(&p),
            Error::FILEOPENFAILED) << "failed to parse child";

        if (const wz::Canvas* canvas = std::get_if<wz::Canvas>(&p.property)) {
            CHECK(OpenedFile_open_container(
                cursor, wz, of, &of->nodes[children_start], canvas->children, children_start, f),
                Error::FILEOPENFAILED) << "failed to open canvas container";
        } else if (const wz::PropertyContainer* container = std::get_if<wz::PropertyContainer>(&p.property)) {
            CHECK(OpenedFile_open_container(
                cursor, wz, of, &of->nodes[children_start], *container, children_start, f),
                Error::FILEOPENFAILED) << "failed to open container";
        } else if (const wz::NamedPropertyContainer* container = std::get_if<wz::NamedPropertyContainer>(&p.property)) {
            CHECK(OpenedFile_open_container(
                cursor, wz, of, &of->nodes[children_start], *container, children_start, f),
                Error::FILEOPENFAILED) << "failed to open named container";
        }
    }

    return Error();
}

Error OpenedFile::open(
    const wz::Wz* wz,
    OpenedFile* of,
    const wz::File* f) {
    Sizes sizes = { 0 };
    CHECK(container_computesizes(wz, &sizes, f->root),
        Error::FILEOPENFAILED) << "failed to precompute sizes";

    of->strings.resize(sizes.strings, L'\0');
    of->images.resize(sizes.images);
    of->nodes.resize(sizes.nodes + 1);

    Cursor cursor = {
        .node = 0,
        .string = of->strings.data(),
        .image = of->images.data(),
    };
    CHECK(OpenedFile_open_container(
        &cursor, wz, of, &of->nodes[0], f->root, 0, f),
        Error::FILEOPENFAILED) << "failed to open file";

    return Error();
}

static Vfs::Node* Vfs_Node_find(
    Vfs::Node* self,
    const std::wstring_view& path) {
    if (path.size() == 0)
        return self;

    Vfs::Directory* directory = self->directory();
    if (directory == nullptr)
        return nullptr;

    std::wstring_view this_path = path;
    std::wstring_view next_path;

    size_t next_slash = path.find(L'/');
    if (next_slash != std::wstring_view::npos) {
        this_path = path.substr(0, next_slash);
        next_path = path.substr(next_slash + 1);
    }

    auto it = directory->children.find(this_path);
    if (it != directory->children.end()) {
        return Vfs_Node_find(&it->second, next_path);
    }

    return nullptr;
}

static Error Vfs_open_fromdirectory(
    Vfs* vfs,
    Vfs::Directory* at,
    const wz::Directory* dir) {
    size_t i = 0;
    auto it = dir->children.iterator(vfs->wz);
    while (it) {
        wz::Entry entry;
        CHECK(it.next(&entry),
            Error::BADREAD) << "failed to read directory entry";

        if (auto entry_file = std::get_if<wz::Entry::File>(&entry.entry)) {
            std::wstring name(entry_file->name.len, L'\0');
            CHECK(entry_file->name.decrypt(name.data()),
                Error::BADREAD) << "failed to decrypt file name";

            Vfs::File file;
            file.wz = vfs->wz.get();
            file.file = entry_file->file;

            Vfs::Node child;
            child.name = name;
            child.contents.emplace<1>(std::move(file));

            at->children.emplace(name, std::move(child));
        } else if (auto entry_directory = std::get_if<wz::Entry::Directory>(&entry.entry)) {
            std::wstring name(entry_directory->name.len, L'\0');
            CHECK(entry_directory->name.decrypt(name.data()),
                Error::BADREAD) << "failed to decrypt subdirectory name";

            Vfs::Directory directory;
            Vfs::Node child;
            child.name = name;
            child.contents.emplace<0>(std::move(directory));

            at->children.emplace(name, std::move(child));
            CHECK(Vfs_open_fromdirectory(vfs, at->children[name].directory(), &entry_directory->directory),
                Error::OPENFAILED) << "failed to open directory " << name;
        }

        ++i;
    }

    return Error();
}

Error Vfs::opennamed(
    Vfs* vfs,
    const wz::Wz* wz,
    std::wstring&& name) {
    Vfs::Directory directory;
    vfs->wz = wz;
    vfs->root.name = std::move(name);
    vfs->root.contents.emplace<0>(std::move(directory));

    CHECK(Vfs_open_fromdirectory(vfs, vfs->root.directory(), &vfs->wz->root),
        Error::OPENFAILED) << "failed to open vfs";

    return Error();
}

Vfs::Node* Vfs::find(const wchar_t* path) {
    return Vfs_Node_find(&root, path);
}

const Vfs::Node::Maybe Vfs::Node::child(
    const wchar_t* name) {
    Vfs::Directory* dir = directory();
    if (!dir)
        return Vfs::Node::Maybe{
            .node = nullptr,
    };

    auto it = dir->children.find(name);
    if (it == dir->children.end())
        return Vfs::Node::Maybe{
            .node = nullptr,
    };

    return Vfs::Node::Maybe{
        .node = &it->second,
    };
}

const Vfs::Node::Maybe Vfs::Node::child(
    const Basename& b) {
    Vfs::Directory* dir = directory();
    if (!dir)
        return Vfs::Node::Maybe{
            .node = nullptr,
    };

    auto it = dir->children.find(b);
    if (it == dir->children.end())
        return Vfs::Node::Maybe{
            .node = nullptr,
    };

    return Vfs::Node::Maybe{
        .node = &it->second,
    };
}

};
