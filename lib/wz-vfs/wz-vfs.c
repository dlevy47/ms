#include "wz-vfs/wz-vfs.h"

static int limited_wcsncmp(
        const wchar_t* name,
        uint32_t name_len,
        const wchar_t* right) {
    uint32_t i = 0;
    for (i = 0; *right && i < name_len; ++i) {
        if (name[i] != *right) {
            return 1;
        }
        ++right;
    }

    // Only consider these equal if we've reached the ends.
    if (i == name_len && *right == 0)
        return 0;
    return 1;
}

void wz_vfs_openedfile_node_wfind(
	struct wz_vfs_openedfile_node** node_out,
	struct wz_vfs_openedfile* of,
	struct wz_vfs_openedfile_node* at,
	const wchar_t* name) {
	struct wz_vfs_openedfile_node* cur = at;
	while (cur) {
		uint32_t name_len = 0;
		while (name[name_len] && name[name_len] != '/')
			++name_len;

		// Base case.
		if (name_len == 0)
			break;

		// Special cases for '.' and '..'.
		if (limited_wcsncmp(name, name_len, L".") == 0) {
			name = name + name_len + 1;
			continue;
		}
		if (limited_wcsncmp(name, name_len, L"..") == 0) {
			if (!cur->parent)
				return;

			cur = of->nodes + cur->parent;
			name = name + name_len + 1;
			continue;
		}

		// Look in cur's children for a node with this name.
		int found = 0;
		for (uint32_t i = 0; i < cur->children.count; ++i) {
			struct wz_vfs_openedfile_node* child = &of->nodes[cur->children.start[i]];

			if (limited_wcsncmp(name, name_len, child->name) == 0) {
				// This matches, so descend.
				cur = child;

				// Plus 1 to point past the slash.
				name = name + name_len + 1;

				found = 1;
			}
		}

		if (!found) {
			// We didn't find a child with this name.
			return;
		}
	}

	*node_out = cur;
}

struct wz_vfs_openedfile_sizes {
	// children is the number of entries in the children array.
	uint32_t children;

	// nodes is the number of entries in the nodes array.
	uint32_t nodes;

	// strings is the number of wchar_t's to allocate in strings, including
	// null terminators.
	uint32_t strings;

	// images is the size, in bytes, to allocate for images.
	uint32_t images;
};

static struct error wz_vfs_openedfile_precomputesizes_namedcontainer(
	struct wz_vfs_openedfile_sizes* sizes,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f,
	struct wz_namedpropertycontainer* c);

static struct error wz_vfs_openedfile_precomputesizes_container(
	struct wz_vfs_openedfile_sizes* sizes,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f,
	struct wz_propertycontainer* c);

static struct error wz_vfs_openedfile_precomputesizes_property(
	struct wz_vfs_openedfile_sizes* sizes,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f,
	struct wz_property* property) {
	struct error err = {0};

    sizes->strings += property->name.len + 1;

    switch (property->kind) {
    case WZ_PROPERTY_KIND_STRING:
    	{
    		sizes->strings += property->string.len + 1;
    	} break;
    case WZ_PROPERTY_KIND_UOL:
    	{
    		sizes->strings += property->uol.len + 1;
    	} break;
    case WZ_PROPERTY_KIND_CANVAS:
    	{
    		sizes->images += wz_image_rawsize(&property->canvas.image);
    		CHECK(wz_vfs_openedfile_precomputesizes_container(
    			sizes, wz, of, f, &property->canvas.children),
    			ERROR_KIND_FILEOPENFAILED, L"failed to precompute canvas container");
    		++sizes->nodes;
    	} break;
    case WZ_PROPERTY_KIND_CONTAINER:
    	{
    		CHECK(wz_vfs_openedfile_precomputesizes_container(
    			sizes, wz, of, f, &property->container),
    			ERROR_KIND_FILEOPENFAILED, L"failed to precompute container");
    		++sizes->nodes;
    	} break;
    case WZ_PROPERTY_KIND_NAMEDCONTAINER:
    	{
    		CHECK(wz_vfs_openedfile_precomputesizes_namedcontainer(
    			sizes, wz, of, f, &property->named_container),
    			ERROR_KIND_FILEOPENFAILED, L"failed to precompute named container");
    		++sizes->nodes;
    	} break;
    default:
        // Other property kinds don't contribute to the sizes.
        break;
    }

    return err;
}

static struct error wz_vfs_openedfile_precomputesizes_namedcontainer(
	struct wz_vfs_openedfile_sizes* sizes,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f,
	struct wz_namedpropertycontainer* c) {
	struct error err = {0};

	sizes->children += c->count;
	sizes->nodes += c->count;

	uint8_t* off = c->first;
	for (uint32_t i = 0; i < c->count; ++i) {
        struct wz_property property = {0};
        CHECK(wz_property_namedfrom(wz, f, &property, &off),
        	ERROR_KIND_FILEOPENFAILED, L"failed to parse child %u", i);
        CHECK(wz_vfs_openedfile_precomputesizes_property(
        	sizes, wz, of, f, &property),
        	ERROR_KIND_FILEOPENFAILED, L"failed to precompute child %u", i);
	}

	return err;
}

static struct error wz_vfs_openedfile_precomputesizes_container(
	struct wz_vfs_openedfile_sizes* sizes,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f,
	struct wz_propertycontainer* c) {
	struct error err = {0};

	sizes->children += c->count;
	sizes->nodes += c->count;

	uint8_t* off = c->first;
	for (uint32_t i = 0; i < c->count; ++i) {
        struct wz_property property = {0};
        CHECK(wz_property_from(wz, f, &property, &off),
        	ERROR_KIND_FILEOPENFAILED, L"failed to parse child %u", i);
        CHECK(wz_vfs_openedfile_precomputesizes_property(
        	sizes, wz, of, f, &property),
        	ERROR_KIND_FILEOPENFAILED, L"failed to precompute child %u", i);
	}

	return err;
}

struct wz_vfs_openedfile_openfrom_cursor {
	uint32_t* child;
	uint32_t node;
	wchar_t* string;
	uint8_t* image;
};

static struct error wz_vfs_openedfile_openfrom_property(
	struct wz_vfs_openedfile_openfrom_cursor* cursor,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f,
	struct wz_property* property) {
	struct error err = {0};

	of->nodes[cursor->node].kind = property->kind;

	CHECK(wz_encryptedstring_decrypt(
		wz, &property->name, cursor->string),
		ERROR_KIND_FILEOPENFAILED, L"failed to decrypt property name");
	of->nodes[cursor->node].name = cursor->string;
	cursor->string += property->name.len + 1;

    switch (property->kind) {
    case WZ_PROPERTY_KIND_STRING:
    	{
    		CHECK(wz_encryptedstring_decrypt(
				wz, &property->string, cursor->string),
				ERROR_KIND_FILEOPENFAILED, L"failed to decrypt property string value");
    		of->nodes[cursor->node].string = cursor->string;
			cursor->string += property->string.len + 1;
    	} break;
    case WZ_PROPERTY_KIND_UOL:
    	{
    		CHECK(wz_encryptedstring_decrypt(
				wz, &property->uol, cursor->string),
				ERROR_KIND_FILEOPENFAILED, L"failed to decrypt property uol value");
    		of->nodes[cursor->node].uol = cursor->string;
			cursor->string += property->uol.len + 1;
    	} break;
    case WZ_PROPERTY_KIND_CANVAS:
    	{
    		CHECK(wz_image_pixels(
    			wz, &property->canvas.image, cursor->image),
    			ERROR_KIND_FILEOPENFAILED, L"failed to retrieve property image pixels");
    		of->nodes[cursor->node].canvas.image = property->canvas.image;
    		of->nodes[cursor->node].canvas.image_data = cursor->image;
    		cursor->image += wz_image_rawsize(&property->canvas.image);
    	} break;
    case WZ_PROPERTY_KIND_UINT16:
    	of->nodes[cursor->node].uint16 = property->uint16;
    	break;
    case WZ_PROPERTY_KIND_INT32:
    	of->nodes[cursor->node].int32 = property->int32;
    	break;
    case WZ_PROPERTY_KIND_FLOAT32:
    	of->nodes[cursor->node].float32 = property->float32;
    	break;
    case WZ_PROPERTY_KIND_FLOAT64:
    	of->nodes[cursor->node].float64 = property->float64;
    	break;
    case WZ_PROPERTY_KIND_VECTOR:
    	of->nodes[cursor->node].vector[0] = property->vector[0];
    	of->nodes[cursor->node].vector[1] = property->vector[1];
    	break;
    case WZ_PROPERTY_KIND_SOUND:
    	// TODO
    	break;
    case WZ_PROPERTY_KIND_CONTAINER:
    case WZ_PROPERTY_KIND_NAMEDCONTAINER:
    	// These are handled elsewhere.
    	break;
    default:
        break;
    }

    return err;
}

// wz_vfs_openedfile_openfrom_container (and _namedcontainer) perform a breadth-first traversal
// of the wz data so that node children ranges are contiguous.
static struct error wz_vfs_openedfile_openfrom_container(
	struct wz_vfs_openedfile_openfrom_cursor* cursor,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_vfs_openedfile_node* this,
	uint32_t this_index,
	struct wz_file* f,
	struct wz_propertycontainer* c);

static struct error wz_vfs_openedfile_openfrom_namedcontainer(
	struct wz_vfs_openedfile_openfrom_cursor* cursor,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_vfs_openedfile_node* this,
	uint32_t this_index,
	struct wz_file* f,
	struct wz_namedpropertycontainer* c);

static struct error wz_vfs_openedfile_openfrom_container(
	struct wz_vfs_openedfile_openfrom_cursor* cursor,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_vfs_openedfile_node* this,
	uint32_t this_index,
	struct wz_file* f,
	struct wz_propertycontainer* c) {
	struct error err = {0};

	uint32_t children_start = cursor->node;
	this->children.start = cursor->child;
	this->children.count = c->count;

	uint8_t* off = c->first;
	for (uint32_t i = 0; i < c->count; ++i) {
		// Store this child index.
		++cursor->node;
		*(cursor->child) = cursor->node;
		++cursor->child;

        struct wz_property property = {0};
        CHECK(wz_property_from(wz, f, &property, &off),
        	ERROR_KIND_FILEOPENFAILED, L"failed to parse child %u", i);
        CHECK(wz_vfs_openedfile_openfrom_property(
        	cursor, wz, of, f, &property),
        	ERROR_KIND_FILEOPENFAILED, L"failed to open child %u", i);
        of->nodes[cursor->node].parent = this_index;
	}

	// Now, replay the children of this container, except this time, descending into containers.
	off = c->first;
	for (uint32_t i = 0; i < c->count; ++i) {
		++children_start;

        struct wz_property property = {0};
        CHECK(wz_property_from(wz, f, &property, &off),
        	ERROR_KIND_FILEOPENFAILED, L"failed to parse child %u", i);

        switch (property.kind) {
        case WZ_PROPERTY_KIND_CANVAS:
        	{
	    		CHECK(wz_vfs_openedfile_openfrom_container(
	    			cursor, wz, of, &of->nodes[children_start], children_start, f, &property.canvas.children),
	    			ERROR_KIND_FILEOPENFAILED, L"failed to open canvas container");
        	} break;
        case WZ_PROPERTY_KIND_CONTAINER:
        	{
	    		CHECK(wz_vfs_openedfile_openfrom_container(
	    			cursor, wz, of, &of->nodes[children_start], children_start, f, &property.container),
	    			ERROR_KIND_FILEOPENFAILED, L"failed to open property container");
        	} break;
        case WZ_PROPERTY_KIND_NAMEDCONTAINER:
        	{
	    		CHECK(wz_vfs_openedfile_openfrom_namedcontainer(
	    			cursor, wz, of, &of->nodes[children_start], children_start, f, &property.named_container),
	    			ERROR_KIND_FILEOPENFAILED, L"failed to open named property container");
        	} break;
        default:
        	break;
        }
	}

	return err;
}

static struct error wz_vfs_openedfile_openfrom_namedcontainer(
	struct wz_vfs_openedfile_openfrom_cursor* cursor,
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_vfs_openedfile_node* this,
	uint32_t this_index,
	struct wz_file* f,
	struct wz_namedpropertycontainer* c) {
	struct error err = {0};

	uint32_t children_start = cursor->node;
	this->children.start = cursor->child;
	this->children.count = c->count;

	uint8_t* off = c->first;
	for (uint32_t i = 0; i < c->count; ++i) {
		// Store this child index.
		++cursor->node;
		*(cursor->child) = cursor->node;
		++cursor->child;

        struct wz_property property = {0};
        CHECK(wz_property_namedfrom(wz, f, &property, &off),
        	ERROR_KIND_FILEOPENFAILED, L"failed to parse child %u", i);
        CHECK(wz_vfs_openedfile_openfrom_property(
        	cursor, wz, of, f, &property),
        	ERROR_KIND_FILEOPENFAILED, L"failed to open child %u", i);
        of->nodes[cursor->node].parent = this_index;
	}

	// Now, replay the children of this container, except this time, descending into containers.
	off = c->first;
	for (uint32_t i = 0; i < c->count; ++i) {
		++children_start;

        struct wz_property property = {0};
        CHECK(wz_property_namedfrom(wz, f, &property, &off),
        	ERROR_KIND_FILEOPENFAILED, L"failed to parse child %u", i);

        switch (property.kind) {
        case WZ_PROPERTY_KIND_CANVAS:
        	{
	    		CHECK(wz_vfs_openedfile_openfrom_container(
	    			cursor, wz, of, &of->nodes[children_start], children_start, f, &property.canvas.children),
	    			ERROR_KIND_FILEOPENFAILED, L"failed to open canvas container");
        	} break;
        case WZ_PROPERTY_KIND_CONTAINER:
        	{
	    		CHECK(wz_vfs_openedfile_openfrom_container(
	    			cursor, wz, of, &of->nodes[children_start], children_start, f, &property.container),
	    			ERROR_KIND_FILEOPENFAILED, L"failed to open property container");
        	} break;
        case WZ_PROPERTY_KIND_NAMEDCONTAINER:
        	{
	    		CHECK(wz_vfs_openedfile_openfrom_namedcontainer(
	    			cursor, wz, of, &of->nodes[children_start], children_start, f, &property.named_container),
	    			ERROR_KIND_FILEOPENFAILED, L"failed to open named property container");
        	} break;
        default:
        	break;
        }
	}

	return err;
}

struct error wz_vfs_openedfile_openfrom(
	struct wz* wz,
	struct wz_vfs_openedfile* of,
	struct wz_file* f) {
	struct error err = {0};

	// First, precompute the required sizes of the string and image
	// arenas.
	struct wz_vfs_openedfile_sizes sizes = {0};
	CHECK(wz_vfs_openedfile_precomputesizes_container(
		&sizes, wz, of, f, &f->root),
		ERROR_KIND_FILEOPENFAILED, L"failed to precompute arena sizes");

	if (sizes.children)
		of->children = calloc(sizes.children, sizeof(*of->children));
	if (sizes.nodes) {
		// 1 additional node for the root.
		of->nodes = calloc(sizes.nodes + 1, sizeof(*of->nodes));
	}
	if (sizes.strings)
		of->strings = calloc(sizes.strings, sizeof(*of->strings));
	if (sizes.images)
		of->images = calloc(sizes.images, sizeof(*of->images));

	// Now, actually open the file.
	struct wz_vfs_openedfile_openfrom_cursor cursor = {
		.child = of->children,
		.node = 0,
		.string = of->strings,
		.image = of->images,
	};
	err = wz_vfs_openedfile_openfrom_container(
		&cursor, wz, of, of->nodes, 0, f, &f->root);
	if (error_top(&err)) {
		error_push(&err, ERROR_KIND_FILEOPENFAILED, L"failed to open file");
		wz_vfs_openedfile_free(of);
		return err;
	}
	of->nodes->kind = WZ_PROPERTY_KIND_CONTAINER;

	return err;
}

void wz_vfs_openedfile_free(
	struct wz_vfs_openedfile* of) {
	free(of->images);
	free(of->strings);
	free(of->nodes);
	free(of->children);
	memset(of, 0, sizeof(*of));
}

struct error wz_vfs_file_open(
	struct wz_vfs* vfs,
	struct wz_vfs_file* f) {
	struct error err = {0};

	// Open this file, if required.
	if (f->rc == 0) {
		CHECK(wz_vfs_openedfile_openfrom(
			&vfs->wz,
			&f->opened,
			&f->file),
			ERROR_KIND_FILEOPENFAILED, L"failed to open file");
	}

	++f->rc;
	return err;
}

void wz_vfs_file_close(
	struct wz_vfs_file* f) {
	if (f->rc == 0) {
		// TODO: error of some kind here?
		return;
	}
	--f->rc;

	if (f->rc == 0)
		wz_vfs_openedfile_free(&f->opened);
}

void wz_vfs_node_free(
	struct wz_vfs_node* node) {
	free(node->name);

	switch (node->kind) {
	case WZ_VFS_NODE_KIND_DIRECTORY:
		{
			for (uint32_t i = 0; i < node->directory.count; ++i) {
				wz_vfs_node_free(&node->directory.children[i]);
			}

			free(node->directory.children);
		} break;
	case WZ_VFS_NODE_KIND_FILE:
		{
			if (node->file.rc > 0) {
				wz_vfs_openedfile_free(&node->file.opened);
			}
		} break;
	}
}

static struct error wz_vfs_fromdirectory(
	struct wz_vfs* vfs,
	struct wz_vfs_node* at,
	struct wz_directory* dir) {
	struct error err = {0};

	at->directory.count = dir->count;
	at->directory.children = calloc(dir->count, sizeof(*at->directory.children));

	uint8_t* entry_addr = dir->first_entry;
	for (uint32_t i = 0; i < dir->count; ++i) {
		struct wz_vfs_node* child = at->directory.children + i;
		child->parent = at;

		struct wz_directoryentry entry = {0};
		CHECK(wz_directoryentry_from(&vfs->wz, &entry, &entry_addr),
			ERROR_KIND_BADREAD, L"failed to parse directory entry %u", i);

		switch (entry.kind) {
		case WZ_DIRECTORYENTRY_KIND_UNKNOWN:
        default:
			break;
		case WZ_DIRECTORYENTRY_KIND_FILE:
			{
				child->name = calloc(entry.file.name.len + 1, sizeof(*child->name));
				CHECK(wz_encryptedstring_decrypt(
					&vfs->wz, &entry.file.name, child->name),
					ERROR_KIND_BADREAD, L"failed to decrypt file node name");

				child->kind = WZ_VFS_NODE_KIND_FILE;
				child->file.file = entry.file.file;
			} break;
		case WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY:
			{
				child->name = calloc(entry.subdirectory.name.len + 1, sizeof(*child->name));
				CHECK(wz_encryptedstring_decrypt(
					&vfs->wz, &entry.subdirectory.name, child->name),
					ERROR_KIND_BADREAD, L"failed to decrypt subdirectory node name");

				child->kind = WZ_VFS_NODE_KIND_DIRECTORY;
				CHECK(wz_vfs_fromdirectory(
					vfs, child, &entry.subdirectory.subdirectory),
					ERROR_KIND_VFSOPENFAILED, L"failed to open child entry %u", i);
			} break;
		}
	}

	return err;
}

struct error wz_vfs_from(
	struct wz_vfs* vfs,
	struct wz wz) {
	vfs->wz = wz;
	vfs->root.kind = WZ_VFS_NODE_KIND_DIRECTORY;

	struct error err = wz_vfs_fromdirectory(vfs, &vfs->root, &vfs->wz.root);
	if (error_top(&err)) {
		wz_vfs_free(vfs);
		error_push(&err, ERROR_KIND_VFSOPENFAILED, L"failed to open vfs");
		return err;
	}
	return err;
}

void wz_vfs_wfind(
	struct wz_vfs_node** node_out,
	struct wz_vfs_node* from,
	const wchar_t* name) {
	struct wz_vfs_node* cur = from;
	while (1) {
		uint32_t name_len = 0;
		while (name[name_len] && name[name_len] != '/')
			++name_len;

		// Base case.
		if (name_len == 0)
			break;

		// Special cases for '.' and '..'.
		if (limited_wcsncmp(name, name_len, L".") == 0) {
			name = name + name_len + 1;
			continue;
		}
		if (limited_wcsncmp(name, name_len, L"..") == 0) {
			if (!cur->parent)
				return;

			cur = cur->parent;
			name = name + name_len + 1;
			continue;
		}

		// Only directories have children.
		if (cur->kind != WZ_VFS_NODE_KIND_DIRECTORY)
			return;

		// Look in cur's children for a node with this name.
		int found = 0;
		for (uint32_t i = 0; i < cur->directory.count; ++i) {
			if (limited_wcsncmp(name, name_len, cur->directory.children[i].name) == 0) {
				// This matches, so descend.
				cur = &cur->directory.children[i];

				// Plus 1 to point past the slash.
				name = name + name_len + 1;

				found = 1;
			}
		}

		if (!found) {
			// We didn't find a child with this name.
			return;
		}
	}

	*node_out = cur;
}

struct error wz_vfs_open(
	struct wz_vfs_file** opened_out,
	struct wz_vfs* vfs,
	const char* name) {
	wchar_t* wname = calloc(strlen(name) + 1, sizeof(*wname));
	wchar_t* cur = wname;
	while (*name) {
		*cur = (wchar_t) *name;
		++cur;
		++name;
	}

	struct error err = wz_vfs_wopen(opened_out, vfs, wname);
	free(wname);
	return err;
}

struct error wz_vfs_wopen(
	struct wz_vfs_file** opened_out,
	struct wz_vfs* vfs,
	const wchar_t* name) {
	struct error err = {0};

	struct wz_vfs_node* node = NULL;
	wz_vfs_wfind(
		&node,
		&vfs->root,
		name);
	if (node == NULL) {
		return err;
	}

	if (node->kind == WZ_VFS_NODE_KIND_FILE) {
		CHECK(wz_vfs_file_open(vfs, &node->file),
			ERROR_KIND_FILEOPENFAILED, L"failed to open file");
		*opened_out = &node->file;
	}

	return err;
}

struct error wz_vfs_free(
	struct wz_vfs* vfs) {
	struct error err = {0};

	wz_vfs_node_free(&vfs->root);

	CHECK(wz_close(&vfs->wz),
		ERROR_KIND_CLOSEFAILED, L"failed to close owned wz");
	return err;
}
