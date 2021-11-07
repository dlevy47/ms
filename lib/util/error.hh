#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Error {
    enum Kind {
        NONE,
        CONVERTFAILED,
        CONVERTFAILED_TRAILINGTEXT,
        BADREAD,
        INVALIDOFFSET,
        OPENFAILED,
        NOTFOUND,
        CLOSEFAILED,
        FILEPARSEERROR1,
        FILEPARSEERROR2,
        FILEPARSEERROR3,
        UNKNOWNDIRECTORYENTRYKIND,
        UNKNOWNPROPERTYKIND,
        UNKNOWNSTRINGOFFSETKIND,
        UNKNOWNPROPERTYKINDNAME,
        UNKNOWNIMAGEFORMAT,
        DECOMPRESSIONFAILED,
        VISITFAILED,
        FILEOPENFAILED,
        GLERROR,
        UIERROR,
        INVALIDUSAGE,
        PROPERTYTYPEMISMATCH,
        FRAMELOADFAILED,
        SPRITELOADFAILED,
        RESOURCELOADFAILED,
        BACKGROUND_LOAD_MISSINGATTRIBUTES,
        BACKGROUND_LOAD_MISSINGFILENAME,
        BACKGROUND_LOAD_MISSINGFILE,
        BACKGROUND_LOAD_MISSINGFRAME,
        BACKGROUND_LOAD_FRAMELOADFAILED,
        TILE_LOAD_MISSINGATTRIBUTES,
        TILESET_LOAD_MISSINGFILE,
        TILESET_LOAD_FRAMELOADFAILED,
        OBJECT_LOAD_MISSINGATTRIBUTES,
        OBJECTSET_LOAD_MISSINGFILE,
        OBJECTSET_LOAD_SPRITELOADFAILED,
        LAYER_LOAD_MISSINGATTRIBUTES,
        MAP_LOAD_FOOTHOLDLOADFAILED,
        MAP_LOAD_HELPERLOADFAILED,
        MAP_LOAD_LADDERLOADFAILED,
        MAP_LOAD_LAYERLOADFAILED,
        MAP_LOAD_MISSINGATTRIBUTES,
        MAP_LOAD_PORTALLOADFAILED,
        WZ_DESERIALIZE_FAILED,
    };

    struct Frame {
        Kind kind;
        const char* file;
        size_t line;
        std::wstringstream message;

        Frame(Kind kind,
                const char* file,
                size_t line):
            kind(kind), file(file), line(line) {}

        Frame(const Frame& rhs):
            kind(rhs.kind), file(rhs.file), line(rhs.line), message(rhs.message.str()) {}
    };

    Kind kind;
    std::vector<Frame> frames;

    template <typename T>
        Error& operator<<(T t) {
            frames.back().message << t;
            return *this;
        }

    Error(
            Kind kind,
            const char* file,
            size_t line): frames() {
        push(kind, file, line);
    }

    Error(): frames() {}

    Error& push(
            Kind kind,
            const char* file,
            size_t line) {
        frames.emplace_back(kind, file, line);
        return *this;
    }

    explicit operator bool() const {
        return frames.size() > 0;
    }

    void print(std::wostream& os) const {
        if (frames.size() == 0) {
            os << "no error";
        }

        for (size_t i = 0, l = frames.size(); i < l; ++i) {
            const Frame& f = frames[i];
            os << f.file << ":" << f.line << ": " << f.message.str() << "\n";
        }
    }

    std::wstring str() const {
        std::wstringstream ss;
        print(ss);
        return ss.str();
    }
};

inline std::wostream& operator<<(std::wostream& os, const Error& e) {
    e.print(os);
    return os;
}

#define error_push(e, k) \
    e.push(k, __FILE__, __LINE__)

#define error_new(k) \
    Error(k, __FILE__, __LINE__)

#define CHECK(x, k) \
    if (Error __error__ = (x)) return error_push(__error__, k)
