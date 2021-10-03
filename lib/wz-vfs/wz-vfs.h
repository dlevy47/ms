#ifndef INCLUDED_WZ_VFS_WZ_VFS_H
#define INCLUDED_WZ_VFS_WZ_VFS_H

#include <wchar.h>

#include "util/error.h"
#include "wz/wz.h"

struct wz_vfs_openedfile;
struct wz_vfs;

struct wz_vfs_openedfile_node {
	wchar_t* name;
	uint32_t parent;
	enum wz_property_kind kind;

	struct {
		uint32_t count;
		uint32_t* start;
	} children;

	union {
        uint16_t uint16;
        uint32_t uint32;
        float float32;
        double float64;
        wchar_t* string;
        uint32_t vector[2];
        uint8_t* sound;
        wchar_t* uol;
        struct {
        	struct wz_image image;
        	uint8_t* image_data;
        } canvas;
    };
};

void wz_vfs_openedfile_node_wfind(
	struct wz_vfs_openedfile_node** node_out,
	struct wz_vfs_openedfile* of,
	struct wz_vfs_openedfile_node* at,
	const wchar_t* name);

// wz_vfs_openedfile represents an opened wz_file, along with arenas for any
// sub properties that required allocation.
struct wz_vfs_openedfile {
	struct wz_vfs_openedfile_node* nodes;

	uint32_t* children;
	wchar_t* strings;
	uint8_t* images;
};

struct error wz_vfs_openedfile_openfrom(
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f);

void wz_vfs_openedfile_free(
	struct wz_vfs_openedfile* of);

enum wz_vfs_node_kind {
	WZ_VFS_NODE_KIND_DIRECTORY,
	WZ_VFS_NODE_KIND_FILE,
};

struct wz_vfs_file {
	uint32_t rc;
	struct wz_vfs_openedfile opened;
	struct wz_file file;
};

struct error wz_vfs_file_open(
	struct wz_vfs* vfs,
	struct wz_vfs_file* f);

void wz_vfs_file_close(
	struct wz_vfs_file* f);

struct wz_vfs_node {
	struct wz_vfs_node* parent;
	wchar_t* name;
	enum wz_vfs_node_kind kind;

	union {
		struct {
			uint32_t count;
			struct wz_vfs_node* children;
		} directory;

		struct wz_vfs_file file;
	};
};

void wz_vfs_node_free(
	struct wz_vfs_node* node);

struct wz_vfs {
	struct wz wz;
	struct wz_vfs_node root;
};

struct error wz_vfs_from(
	struct wz_vfs* vfs,
	struct wz wz);

void wz_vfs_wfind(
	struct wz_vfs_node** node_out,
	struct wz_vfs_node* from,
	const wchar_t* name);

struct error wz_vfs_open(
	struct wz_vfs_file** opened_out,
	struct wz_vfs* vfs,
	const char* name);

struct error wz_vfs_wopen(
	struct wz_vfs_file** opened_out,
	struct wz_vfs* vfs,
	const wchar_t* name);

struct error wz_vfs_free(
	struct wz_vfs* vfs);

#endif
