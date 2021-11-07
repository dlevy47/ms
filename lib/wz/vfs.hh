#pragma once

#include <type_traits>
#include <vector>

#include "p.hh"
#include "util/error.hh"
#include "wz/directory.hh"
#include "wz/property.hh"
#include "wz/wz.hh"

namespace wz {

struct OpenedFile {
    struct Node {
        struct Iterator {
            const Node* node;
            uint32_t next_index;

            const Node* next() {
                if (next_index >= node->children.count)
                    return nullptr;

                const Node* ret = &node->children.start[next_index];
                ++next_index;
                return ret;
            }
        };

        Property::Kind kind;

        // parent is a pointer to this Node's parent in the nodes arena.
        Node* parent;

        // name is a pointer to the null-terminated name of this node, internal
        // to the containing OpenedFile's strings arena.
        wchar_t* name;

        // children is a span of node indices in the containing OpenedFile's children
        // arena.
        struct {
            uint32_t count;
            Node* start;
        } children;

        union {
            uint16_t uint16;
            int32_t int32;
            float float32;
            double float64;
            wchar_t* string;
            int32_t vector[2];
            uint8_t* sound;
            wchar_t* uol;
            struct {
                wz::Image image;
                uint8_t* image_data;
            } canvas;
        };

        Iterator iterator() const {
            Iterator it = {
                .node = this,
                .next_index = 0,
            };

            return it;
        }

        const Node* find(
                const wchar_t* path) const;

        Error childint32(
                const wchar_t* n,
                int32_t* x) const;
        Error childvector(
                const wchar_t* n,
                int32_t* x,
                int32_t* y) const;
        Error childstring(
                const wchar_t* n,
                const wchar_t** x) const;

        template <typename S, typename T, typename... Args>
            Error deserialize(
                    S name,
                    T destination,
                    Args... rest) const {
                static_assert(std::is_same<S, const wchar_t*>::value, "name must be const wchar_t*");

                constexpr size_t count = (sizeof...(rest) / 2) + 1;
                bool found[count] = {false};

                for (uint32_t i = 0, l = children.count; i < l; ++i) {
                    if (Error e = (deserialize_match<S, T, Args...>(
                                    found,
                                    &children.start[i],
                                    name,
                                    destination,
                                    rest...))) {
                        return error_push(e, Error::WZ_DESERIALIZE_FAILED)
                            << "failed to deserialize node value";
                    }
                }

                for (size_t i = 0; i < count; ++i) {
                    if (!found[i]) {
                        return error_new(Error::WZ_DESERIALIZE_FAILED)
                            << "not all node values were deserialized";
                    }
                }

                return Error();
            }

        Node():
            kind(Property::VOID),
            parent(0),
            name(nullptr) {
                children.count = 0;
                children.start = nullptr;
            }

        private:

        template <typename... Args>
            static Error deserialize_match(
                    bool* found,
                    const wz::OpenedFile::Node* child) {
                return Error();
            }

        template <typename S, typename T, typename... Args>
            static Error deserialize_match(
                    bool* found,
                    const wz::OpenedFile::Node* child,
                    S name,
                    T destination,
                    Args... rest) {
                static_assert(std::is_same<S, const wchar_t*>::value, "name must be const wchar_t*");

                if (wcscmp(name, child->name) == 0) {
                    CHECK(deserialize_into(
                                child,
                                destination),
                            Error::WZ_DESERIALIZE_FAILED)
                        << "failed to deserialize node value: " << name;

                    *found = true;
                    return Error();
                }

                return deserialize_match<Args...>(
                        found + 1,
                        child,
                        rest...);
            }

        static Error deserialize_into(
                const wz::OpenedFile::Node* node,
                const wchar_t** destination) {
            if (node->kind != wz::Property::STRING) {
                return error_new(Error::PROPERTYTYPEMISMATCH)
                    << "node is not string";
            }

            *destination = node->string;
            return Error();
        }

        static Error deserialize_into(
                const wz::OpenedFile::Node* node,
                int32_t* destination) {
            if (node->kind != wz::Property::INT32) {
                return error_new(Error::PROPERTYTYPEMISMATCH)
                    << "node is not int32";
            }

            *destination = node->int32;
            return Error();
        }
    };

    // strings is an arena containing null-terminated decrypted strings in this file.
    std::vector<wchar_t> strings;

    // images is an arena containing decompressed image information. This is not
    // an atlas; image data here is one after the other.
    std::vector<uint8_t> images;

    // nodes is an arena containing the Nodes in this file.
    std::vector<Node> nodes;

    static Error open(
            const wz::Wz* wz,
            OpenedFile* of,
            const wz::File* f);

    Node::Iterator iterator() const {
        return nodes[0].iterator();
    }

    const Node* find(
            const wchar_t* path) const {
        return nodes[0].find(path);
    }

    OpenedFile() = default;
    OpenedFile(OpenedFile&&) = default;
    OpenedFile(const OpenedFile&) = delete;
};

struct Vfs {
    struct File {
        P<const wz::Wz> wz;
        wz::File file;
        N<uint32_t> rc;

        std::unique_ptr<OpenedFile> opened;

        struct Handle {
            P<File> file;
            P<const OpenedFile> opened_file;

            Handle clone() {
                Handle h;

                h.file = file.get();
                h.opened_file = opened_file.get();
                ++(file->rc);
                return h;
            }

            const OpenedFile& operator*() const {
                return *opened_file;
            }

            const OpenedFile* operator->() const {
                return opened_file.get();
            }

            operator const OpenedFile*() const {
                return opened_file.get();
            }

            ~Handle() {
                if (file)
                    file->close();
            }

            Handle& operator=(Handle&& rhs) = default;
            Handle() = default;
            Handle(Handle&&) = default;
            Handle(const Handle&) = delete;
        };

        Error open(Handle* h) {
            if (rc == (uint32_t) 0) {
                opened.reset(new OpenedFile());
                CHECK(OpenedFile::open(wz, opened.get(), &file),
                        Error::OPENFAILED) << "failed to open file";
            }

            if (h) {
                h->file = this;
                h->opened_file = opened.get();
            }
            ++rc;
            return Error();
        }

        void close() {
            if (rc == (uint32_t) 0)
                return;

            --rc;
            if (rc == (uint32_t) 0)
                opened.reset();
        }

        File() = default;
        File(File&& rhs) = default;
        File(const File&) = delete;
    };

    struct Node {
        enum Kind {
            DIRECTORY,
            FILE,
        };

        P<Node> parent;
        std::wstring name;
        Kind kind;

        // directory and file are mutually exclusive, but are not in a union for c++ reasons.
        std::vector<Node> directory;
        File file;

        Node() = default;
        Node(Node&&) = default;
        Node(const Node&) = delete;
    };

    P<const wz::Wz> wz;
    Node root;

    static Error open(
            Vfs* vfs,
            const wz::Wz* wz);

    Node* find(const wchar_t* path);

    Vfs() = default;
    Vfs(Vfs&&) = default;
    Vfs(const Vfs&) = delete;
};

}
