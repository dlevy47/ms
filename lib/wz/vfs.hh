#pragma once

#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "logger.hh"
#include "p.hh"
#include "util/error.hh"
#include "wz/directory.hh"
#include "wz/property.hh"
#include "wz/wz.hh"

namespace wz {

struct Basename {
    const std::wstring_view name;
};

struct OpenedFile {
    struct String {
        wchar_t* string;
    };

    struct Uol {
        wchar_t* uol;
    };

    struct Canvas {
        wz::Image image;
        uint8_t* image_data;
    };

    struct Node {
        struct Maybe {
            const Node* node;

            const Maybe child(
                const wchar_t* name) const {
                if (node)
                    return node->child(name);

                return *this;
            }

            const Maybe child(
                const std::wstring& name) const {
                return child(name.c_str());
            }
        };

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

        // parent is a pointer to this Node's parent in the nodes arena.
        Node* parent;

        // name is a pointer to the null-terminated name of this node, internal
        // to the containing OpenedFile's strings arena.
        wchar_t* name;

        // children is a span of node indices in the containing OpenedFile's children
        // arena.
        struct {
            uint32_t count{ 0 };
            Node* start;
        } children;

        std::variant<
            Void,
            uint16_t,
            int32_t,
            float,
            double,
            String,
            Vector,
            uint8_t*,
            Uol,
            Canvas> value;

        const String* string() const {
            return std::get_if<String>(&value);
        }

        const Canvas* canvas() const {
            return std::get_if<Canvas>(&value);
        }

        Iterator iterator() const {
            Iterator it = {
                .node = this,
                .next_index = 0,
            };

            return it;
        }

        const Node* find(
            const wchar_t* path) const;

        const Maybe child(
            const wchar_t* name) const;
        const Maybe child(
            const std::wstring& name) const {
            return child(name.c_str());
        }

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
            bool found[count] = { false };

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
            if (const String* s = std::get_if<String>(&node->value)) {
                *destination = s->string;
            } else {
                return error_new(Error::PROPERTYTYPEMISMATCH)
                    << "node is not string";
            }

            return Error();
        }

        static Error deserialize_into(
            const wz::OpenedFile::Node* node,
            int32_t* destination) {
            if (const int32_t* i = std::get_if<int32_t>(&node->value)) {
                *destination = *i;
            } else {
                return error_new(Error::PROPERTYTYPEMISMATCH)
                    << "node is not int32";
            }

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

// Vfs is a wrapper around a Wz that provides quicker, random-ish access to
// the Files inside.
struct Vfs {
    struct Node;

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

            operator const OpenedFile* () const {
                return opened_file.get();
            }

            ~Handle() {
                if (file)
                    file->close();
            }

            Handle& operator=(Handle&) = delete;
            Handle& operator=(Handle&& rhs) = default;
            Handle() = default;
            Handle(Handle&&) = default;
            Handle(const Handle&) = delete;
        };

        Error open(Handle* h) {
            if (rc == (uint32_t)0) {
                opened.reset(new OpenedFile());
                CHECK(OpenedFile::open(wz.get(), opened.get(), &file),
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
            if (rc == (uint32_t)0)
                throw "double closed file";

            --rc;
            if (rc == (uint32_t)0)
                opened.reset();
        }

        ~File() {
            if (rc > 0) {
                LOG(Logger::ERROR)
                    << "closing file with " << rc << " open handles!";
            }
        }

        File() = default;
        File(File&& rhs) = default;
        File(const File&) = delete;
    };

    struct Directory {
        struct Hash
        {
            using hash_type = std::hash<std::wstring_view>;
            using is_transparent = void;

            std::size_t operator()(const wchar_t* str) const { return operator()(std::wstring_view{ str }); }
            std::size_t operator()(std::wstring const& str) const { return operator()((std::wstring_view)str); }
            std::size_t operator()(std::wstring_view str) const {
                if (str.ends_with(L".img")) {
                    str.remove_suffix(4);
                    return operator()(Basename{ str });
                }

                return hash_type{}(str);
            }
            std::size_t operator()(const Basename& basename) const {
                hash_type hasher;
                const std::size_t kMul = 0x9ddfea08eb382d69ULL;

                std::size_t seed = hasher(basename.name);

                std::size_t a = (hasher(L".img") ^ seed) * kMul;
                a ^= (a >> 47);

                std::size_t b = (seed ^ a) * kMul;
                b ^= (b >> 47);

                return b * kMul;
            }
        };

        struct Equal {
            using is_transparent = void;

            bool operator()(const Basename& l, const Basename& r) const {
                return l.name == r.name;
            }
            
            bool operator()(const Basename& l, const std::wstring& r) const {
                return this->operator()(r, l);
            }
            bool operator()(const std::wstring& l, const Basename& r) const {
                return (l.size() == r.name.size() + 4) && l.starts_with(r.name) && l.ends_with(L".img");
            }
            
            bool operator()(const std::wstring_view& r, const std::wstring& l) const {
                return l == r;
            }
            bool operator()(const std::wstring& l, const std::wstring_view& r) const {
                return l == r;
            }
            
            bool operator()(const std::wstring& l, const std::wstring& r) const {
                return ::wcscmp(l.c_str(), r.c_str()) == 0;
            }
            bool operator()(const wchar_t* l, const std::wstring& r) const {
                return this->operator()(r, l);
            }
            
            bool operator()(const std::wstring& l, const wchar_t* r) const {
                return ::wcscmp(l.c_str(), r) == 0;
            }
        };

        std::unordered_map<std::wstring, Node, Hash, Equal> children;

        Directory() = default;
        Directory(Directory&&) = default;
        Directory(const Directory&) = delete;
    };

    struct Node {
        struct Maybe {
            Node* node;

            template <typename... Ts>
            auto child(Ts... args) const {
                if (node)
                    return node->child(args...);

                return *this;
            }

            Directory* directory() const {
                if (node)
                    return node->directory();

                return nullptr;
            }

            File* file() const {
                if (node)
                    return node->file();

                return nullptr;
            }
        };

        P<Node> parent;
        std::wstring name;

        std::variant<
            Directory,
            File> contents;

        const Maybe child(
            const wchar_t* name);
        const Maybe child(
            const std::wstring& name) {
            return child(name.c_str());
        }
        const Maybe child(
            const Basename& b);

        Directory* directory() {
            return std::get_if<Directory>(&contents);
        }

        const Directory* directory() const {
            return std::get_if<Directory>(&contents);
        }

        File* file() {
            return std::get_if<File>(&contents);
        }

        const File* file() const {
            return std::get_if<File>(&contents);
        }

        Node() = default;
        Node(Node&&) = default;
        Node(const Node&) = delete;
    };

    P<const wz::Wz> wz;
    Node root;

    static Error opennamed(
        Vfs* vfs,
        const wz::Wz* wz,
        std::wstring&& name);

    static Error open(
        Vfs* vfs,
        const wz::Wz* wz) {
        return opennamed(
            vfs,
            wz,
            L"");
    }

    Node* find(const wchar_t* path);

    const Node::Maybe child(
        const wchar_t* name) {
        return root.child(name);
    }
    const Node::Maybe child(
        const std::wstring& name) {
        return root.child(name);
    }
    const Node::Maybe child(
        const Basename& b) {
        return root.child(b);
    }

    Vfs() = default;
    Vfs(Vfs&&) = default;
    Vfs(const Vfs&) = delete;
};

}
