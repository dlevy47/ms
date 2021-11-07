#include <stdint.h>
#include <windows.h>

int _wz_openfileforread(
        int* handle_out,
        size_t* size_out,
        const char* filename) {
	HANDLE fh = CreateFileA(
		filename,
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}

	LARGE_INTEGER size = {0};
	if (GetFileSizeEx(fh, &size) == 0) {
		DWORD e = GetLastError();
		CloseHandle(fh);
		return e;
	}

	*handle_out = (int) fh;
	*size_out = (size_t) size.QuadPart;
	return 0;
}

int _wz_closefile(
        int handle) {
    if (CloseHandle((HANDLE) handle) == 0) {
        return GetLastError();
    }

    return 0;
}

int _wz_mapfile(
        const void** addr_out,
        int handle,
        size_t length) {
	DWORD size_low = length & 0xFFFFFFFF;
	DWORD size_high = (length & 0xFFFFFFFF00000000) >> 32;
	HANDLE mapping = CreateFileMappingA(
		(HANDLE) handle,
		NULL,
		PAGE_READONLY,
		size_high,
		size_low,
		NULL);
	if (mapping == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}

	LPVOID addr = MapViewOfFile(
		mapping,
		FILE_MAP_READ,
		0,
		0,
		length);
	if (addr == NULL) {
		return GetLastError();
	}

    *addr_out = addr;
    return 0;
}

int _wz_unmapfile(
        const void* addr,
        size_t length) {
	if (UnmapViewOfFile(addr) == 0) {
		return GetLastError();
	}

    return 0;
}
