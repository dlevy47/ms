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
    if (child == nullptr || child->kind != wz::Property::INT32) {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " is missing or not int32";
    }

    *x = child->int32;
    return Error();
}

Error OpenedFile::Node::childvector(
        const wchar_t* n,
        int32_t* x,
        int32_t* y) const {
    const OpenedFile::Node* child = find(n);
    if (child == nullptr || child->kind != wz::Property::VECTOR) {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " is missing or not vector";
    }

    *x = child->vector[0];
    *y = child->vector[1];
    return Error();
}

Error OpenedFile::Node::childstring(
        const wchar_t* n,
        const wchar_t** x) const {
    const OpenedFile::Node* child = find(n);
    if (child == nullptr || child->kind != wz::Property::STRING) {
        return error_new(Error::PROPERTYTYPEMISMATCH)
            << "child node " << n << " is missing or not string";
    }

    *x = child->string;
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

        switch (p.kind) {
            case wz::Property::VOID:
            case wz::Property::UINT16:
            case wz::Property::INT32:
            case wz::Property::FLOAT32:
            case wz::Property::FLOAT64:
            case wz::Property::VECTOR:
            case wz::Property::SOUND:
                break;
            case wz::Property::STRING:
                sizes->strings += p.string.len + 1;
                break;
            case wz::Property::UOL:
                sizes->strings += p.uol.len + 1;
                break;
            case wz::Property::CANVAS:
                {
                    sizes->images += p.canvas.image.rawsize();
                    CHECK(container_computesizes(wz, sizes, p.canvas.children),
                            Error::FILEOPENFAILED) << "failed to compute canvas child sizes";
                    ++sizes->nodes;
                } break;
            case wz::Property::CONTAINER:
                {
                    CHECK(container_computesizes(wz, sizes, p.container),
                            Error::FILEOPENFAILED) << "failed to compute container sizes";
                    ++sizes->nodes;
                } break;
            case wz::Property::NAMEDCONTAINER:
                {
                    CHECK(container_computesizes(wz, sizes, p.named_container),
                            Error::FILEOPENFAILED) << "failed to compute named container sizes";
                    ++sizes->nodes;
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
    node.kind = p->kind;

    CHECK(p->name.decrypt(cursor->string),
            Error::FILEOPENFAILED) << "failed to decrypt property name";
    node.name = cursor->string;
    cursor->string += p->name.len + 1;

    switch (p->kind) {
        case wz::Property::VOID:
            break;
        case wz::Property::STRING:
            {
                CHECK(p->string.decrypt(cursor->string),
                        Error::FILEOPENFAILED) << "failed to decrypt property string";
                node.string = cursor->string;
                cursor->string += p->string.len + 1;
            } break;
        case wz::Property::UOL:
            {
                CHECK(p->uol.decrypt(cursor->string),
                        Error::FILEOPENFAILED) << "failed to decrypt property string";
                node.uol = cursor->string;
                cursor->string += p->uol.len + 1;
            } break;
        case wz::Property::CANVAS:
            {
                CHECK(p->canvas.image.pixels(cursor->image),
                        Error::FILEOPENFAILED) << "failed to retrieve image pixels of property ";
                node.canvas.image = p->canvas.image;
                node.canvas.image_data = cursor->image;
                cursor->image += p->canvas.image.rawsize();
            } break;
        case wz::Property::UINT16:
            node.uint16 = p->uint16;
            break;
        case wz::Property::INT32:
            node.int32 = p->int32;
            break;
        case wz::Property::FLOAT32:
            node.float32 = p->float32;
            break;
        case wz::Property::FLOAT64:
            node.float64 = p->float64;
            break;
        case wz::Property::VECTOR:
            node.vector[0] = p->vector[0];
            node.vector[1] = p->vector[1];
            break;
        case wz::Property::SOUND:
            // TODO
            break;
        case wz::Property::CONTAINER:
        case wz::Property::NAMEDCONTAINER:
            // These are handled elsewhere.
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

        switch (p.kind) {
            case wz::Property::CANVAS:
                {
                    CHECK(OpenedFile_open_container(
                                cursor, wz, of, &of->nodes[children_start], p.canvas.children, children_start, f),
                            Error::FILEOPENFAILED) << "failed to open canvas container";
                } break;
            case wz::Property::CONTAINER:
                {
                    CHECK(OpenedFile_open_container(
                                cursor, wz, of, &of->nodes[children_start], p.container, children_start, f),
                            Error::FILEOPENFAILED) << "failed to open container";
                } break;
            case wz::Property::NAMEDCONTAINER:
                {
                    CHECK(OpenedFile_open_container(
                                cursor, wz, of, &of->nodes[children_start], p.named_container, children_start, f),
                            Error::FILEOPENFAILED) << "failed to open named container";
                } break;
            default:
                break;
        }
    }

    return Error();
}

Error OpenedFile::open(
        const wz::Wz* wz,
        OpenedFile* of,
        const wz::File* f) {
    Sizes sizes = {0};
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

    if (self->kind == Vfs::Node::FILE)
        return nullptr;

    std::wstring_view this_path = path;
    std::wstring_view next_path;

    size_t next_slash = path.find(L'/');
    if (next_slash != std::wstring_view::npos) {
        this_path = path.substr(0, next_slash);
        next_path = path.substr(next_slash + 1);
    }

    for (size_t i = 0, l = self->directory.size(); i < l; ++i) {
        if (self->directory[i].name == this_path) {
            return Vfs_Node_find(&self->directory[i], next_path);
        }
    }

    return nullptr;
}

static Error Vfs_open_fromdirectory(
        Vfs* vfs,
        Vfs::Node* at,
        const wz::Directory* dir) {
    at->directory.resize(dir->children.count);

    size_t i = 0;
    auto it = dir->children.iterator(vfs->wz);
    while (it) {
        wz::Entry entry;
        CHECK(it.next(&entry),
                Error::BADREAD) << "failed to read directory entry";

        Vfs::Node& child = at->directory[i];

        switch (entry.kind) {
            case wz::Entry::UNKNOWN:
                break;
            case wz::Entry::FILE:
                {
                    std::wstring name(entry.file.name.len, L'\0');
                    CHECK(entry.file.name.decrypt(name.data()),
                            Error::BADREAD) << "failed to decrypt file name";

                    child.name = name;
                    child.kind = Vfs::Node::FILE;
                    child.file.wz = vfs->wz.get();
                    child.file.file = entry.file.file;
                } break;
            case wz::Entry::SUBDIRECTORY:
                {
                    std::wstring name(entry.subdirectory.name.len, L'\0');
                    CHECK(entry.subdirectory.name.decrypt(name.data()),
                            Error::BADREAD) << "failed to decrypt subdirectory name";

                    child.name = name;
                    child.kind = Vfs::Node::DIRECTORY;

                    CHECK(Vfs_open_fromdirectory(vfs, &child, &entry.subdirectory.directory),
                            Error::OPENFAILED) << "failed to open directory " << name;
                } break;
        }

        ++i;
    }

    return Error();
}

Error Vfs::open(
        Vfs* vfs,
        const wz::Wz* wz) {
    vfs->wz = wz;
    vfs->root.kind = Vfs::Node::DIRECTORY;

    CHECK(Vfs_open_fromdirectory(vfs, &vfs->root, &vfs->wz->root),
            Error::OPENFAILED) << "failed to open vfs";

    return Error();
}

Vfs::Node* Vfs::find(const wchar_t* path) {
    return Vfs_Node_find(&root, path);
}

};
