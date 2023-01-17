#pragma once

#include <cwchar>

#include "util/error.hh"

namespace util {

static inline Error convert(
    const wchar_t* s,
    int32_t* x) {
    wchar_t* end = nullptr;
    *x = std::wcstol(s, &end, 10);

    if (end == s) {
        return error_new(Error::CONVERTFAILED);
    }
    if (*end != '\0') {
        return error_new(Error::CONVERTFAILED_TRAILINGTEXT);
    }

    return Error();
}

static inline Error convert(
    const wchar_t* s,
    uint32_t* x) {
    wchar_t* end = nullptr;
    *x = std::wcstol(s, &end, 10);

    if (end == s) {
        return error_new(Error::CONVERTFAILED);
    }
    if (*end != '\0') {
        return error_new(Error::CONVERTFAILED_TRAILINGTEXT);
    }

    return Error();
}

}
