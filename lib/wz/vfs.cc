#include "wz/vfs.hh"

namespace wz {

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
	uint32_t* child;
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
                cursor->string += p->string.len + 1;
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
    self->children.start = cursor->child;
    self->children.count = c.count;

    auto it1 = c.iterator(wz);
    while (it1) {
        // Store this child index.
        ++cursor->node;
        *(cursor->child) = cursor->node;
        ++cursor->child;

        wz::Property p;
        CHECK(it1.next(&p),
                Error::FILEOPENFAILED) << "failed to parse child";
        CHECK(OpenedFile_open_property(
                    cursor, wz, of, &p, f),
                Error::FILEOPENFAILED) << "failed to open child";
        of->nodes[cursor->node].parent = this_index;
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

    of->strings = new wchar_t[sizes.strings];
    of->images = new uint8_t[sizes.images];
    of->children = new uint32_t[sizes.children];
    of->nodes = new OpenedFile::Node[sizes.nodes + 1]; // One extra for the root node.

    Cursor cursor = {
        .child = of->children,
        .node = 0,
        .string = of->strings,
        .image = of->images,
    };
    CHECK(OpenedFile_open_container(
                &cursor, wz, of, of->nodes, f->root, 0, f),
            Error::FILEOPENFAILED) << "failed to open file";

    return Error();
}

};
