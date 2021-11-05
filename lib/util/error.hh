#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Error {
    enum Kind {
        NONE,
        BADREAD,
        INVALIDOFFSET,
        OPENFAILED,
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
};

#define error_push(e, k) \
    e.push(k, __FILE__, __LINE__)

#define error_new(k) \
    Error(k, __FILE__, __LINE__)

#define CHECK(x, k) \
    if (Error __error__ = (x)) return error_push(__error__, k)
