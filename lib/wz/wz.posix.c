#include "wz/internal.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int _wz_openfileforread(
        int* handle_out,
        size_t* size_out,
        const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return errno;
    }

    struct stat stat = {0};
    if (fstat(fd, &stat) == -1) {
        close(fd);
        return errno;
    }

    *handle_out = fd;
    *size_out = (size_t) stat.st_size;
    return 0;
}

int _wz_closefile(
        int handle) {
    if (close(handle) == -1) {
        return errno;
    }

    return 0;
}

int _wz_mapfile(
        void** addr_out,
        int handle,
        size_t length) {
    void* addr = mmap(
            NULL,
            length,
            PROT_READ,
            MAP_PRIVATE,
            handle,
            0);
    if (addr == MAP_FAILED) {
        return errno;
    }

    *addr_out = addr;
    return 0;
}

int _wz_unmapfile(
        void* addr,
        size_t length) {
    if (munmap(addr, length) == -1) {
        return errno;
    }

    return 0;
}
