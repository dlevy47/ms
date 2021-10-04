#include <stdio.h>

#include "wz/wz.h"
#include "wz-vfs/wz-vfs.h"
#include "util/map.h"
#include "util/vec.h"

#include "lodepng/lodepng.h"

struct zmap_index {
	uint32_t index;
};

str_map_declare(zmap_index);
str_map_define(zmap_index);

struct dataset {
	struct wz base;
	struct str_map(zmap_index) zmap;
};

static struct error dataset_openwz(
	struct dataset* dataset,
	struct wz* wz,
	const char* dir,
	const char* filename) {
	size_t dir_len = strlen(dir);
	size_t filename_len = strlen(filename);
	char* full_name = calloc(dir_len + filename_len + 1 + 1, sizeof(*full_name));

	strcpy(full_name, dir);
	full_name[dir_len] = '/';
	strcpy(full_name + 2, filename);

	struct error err = wz_open(wz, full_name);

	free(full_name);
	return err;
}

static struct error dataset_open(
	struct dataset* dataset,
	const char* dir) {
	struct error err = {0};

	CHECK(dataset_openwz(dataset, &dataset->base, dir, "Base.wz"),
		ERROR_KIND_BADREAD, L"failed to open Base.wz");

	return err;
}

static void print_indent(
        size_t indent,
        FILE* outf) {
    for (size_t i = 0; i < indent; ++i) {
        fprintf(outf, " ");
    }
}

static void dump(
	const struct wz_vfs_openedfile* of,
	const struct wz_vfs_openedfile_node* node,
	size_t indent) {
	print_indent(indent, stdout);
	printf("%ls = ", node->name);

	switch (node->kind) {
    case WZ_PROPERTY_KIND_NULL:
        printf("NULL\n");
        break;
    case WZ_PROPERTY_KIND_UINT16:
        printf("[u16] %hu\n", node->uint16);
        break;
    case WZ_PROPERTY_KIND_INT32:
        printf("[i32] %d\n", node->int32);
        break;
    case WZ_PROPERTY_KIND_FLOAT32:
        printf("[f32] %f\n", node->float32);
        break;
    case WZ_PROPERTY_KIND_FLOAT64:
        printf("[f64] %lf\n", node->float64);
        break;
    case WZ_PROPERTY_KIND_STRING:
    	printf("[string] %ls\n", node->string);
    	break;
    case WZ_PROPERTY_KIND_VECTOR:
        printf("[vector] %d, %d\n", node->vector[0], node->vector[1]);
        break;
    case WZ_PROPERTY_KIND_SOUND:
        printf("[sound] @%p\n", node->sound);
        break;
    case WZ_PROPERTY_KIND_UOL:
    	printf("[uol] %ls\n", node->uol);
    	break;
    case WZ_PROPERTY_KIND_CANVAS:
        {
            int encrypted = (node->canvas.image.data[0] != 0x78 ||
                    node->canvas.image.data[1] != 0x9C);
            printf("[canvas] %u x %u (%d %d) (%s) ",
                    node->canvas.image.width, node->canvas.image.height,
                    node->canvas.image.format, node->canvas.image.format2,
                    (encrypted ? "encrypted" : "not encrypted"));
        }
    case WZ_PROPERTY_KIND_CONTAINER:
    case WZ_PROPERTY_KIND_NAMEDCONTAINER:
    	{
    		printf("[container] %u children\n", node->children.count);
		    for (uint32_t i = 0; i < node->children.count; ++i) {
		    	const struct wz_vfs_openedfile_node* child = &of->nodes[node->children.start[i]];
		    	dump(of, child, indent + 2);
		    }
    	} break;
    default:
    	printf("\n");
        break;
    }
}

struct pathcomponent {
	wchar_t* name;
};

vec_declare(pathcomponent);
vec_define(pathcomponent);

static void print_path(struct vec(pathcomponent)* path) {
	struct vec_iterator(pathcomponent) it = vec_iterator(pathcomponent)(path);
	struct pathcomponent* c = NULL;
	while ((c = vec_iterator_next(pathcomponent)(&it))) {
		printf("/%ls", c->name);
	}
	printf("/");
}

void tokenize(
	wchar_t** token_out,
	size_t* token_len_out,
	wchar_t* s,
	wchar_t* delimiters) {
	wchar_t* next = s;
	while (1) {
		next = wcspbrk(s, delimiters);
		if (next == NULL) {
			*token_len_out = wcslen(s);
			*token_out = s;
			return;
		}

		if (next != s) {
			break;
		}

		++s;
	}

	*token_len_out =  next - s;
	*token_out = s;
}

struct error main_(
	int argc,
	char* argv[]) {
	struct error err = {0};

	struct wz wz = {0};
	CHECK(wz_open(&wz, argv[1]),
		ERROR_KIND_BADREAD, L"failed to open wz file %s", argv[1]);

    struct wz_vfs vfs = {0};
    CHECK(wz_vfs_from(&vfs, wz),
    	ERROR_KIND_VFSOPENFAILED, L"failed to open vfs for wz %s", argv[1]);

    struct wz_vfs_node* cd = &vfs.root;
    struct vec(pathcomponent) path = {0};
    while (1) {
    	print_path(&path);
    	printf("> ");

    	enum { entry_len = 1024 };
    	wchar_t entry[entry_len] = {0};
    	fgetws(entry, entry_len, stdin);

    	size_t command_len = 0;
    	wchar_t* command = NULL;
    	tokenize(
    		&command,
    		&command_len,
    		entry,
    		L" \n");
    	command[command_len] = 0;

    	if (wcscmp(command, L"q") == 0) {
    		break;
    	} else if (wcscmp(command, L"ls") == 0) {
    		if (cd->kind == WZ_VFS_NODE_KIND_DIRECTORY) {
    			for (uint32_t i = 0; i < cd->directory.count; ++i) {
    				struct wz_vfs_node* child = &cd->directory.children[i];

    				printf("%ls", child->name);
    				if (child->kind == WZ_VFS_NODE_KIND_DIRECTORY)
    					printf("/");
    				printf("\n");
    			}
    		}
    	} else if (wcscmp(command, L"cd") == 0) {
    		size_t arg_len = 0;
    		wchar_t* arg = NULL;
    		tokenize(
    			&arg,
    			&arg_len,
    			command + command_len + 1,
    			L" \n");

    		if (arg == NULL || wcslen(arg) == 0) {
    			printf("specify path\n");
    			continue;
    		}
    		arg[arg_len] = 0;

    		struct wz_vfs_node* next = NULL;
    		wz_vfs_wfind(&next, cd, arg);
    		if (next == NULL) {
    			printf("404\n");
    			continue;
    		}

    		cd = next;

    		if (wcscmp(arg, L"..") == 0) {
    			vec_pop(pathcomponent)(&path);
    		} else if (wcscmp(arg, L".") != 0) {
	    		struct pathcomponent pc = {
	    			.name = cd->name,
	    		};
	    		vec_push(pathcomponent)(&path, pc);
	    	}
    	} else if (wcscmp(command, L"dump") == 0) {
    		if (cd->kind != WZ_VFS_NODE_KIND_FILE) {
    			printf("can only dump files\n");
    			continue;
    		}

    		CHECK(wz_vfs_file_open(&vfs, &cd->file),
    			ERROR_KIND_FILEOPENFAILED, L"failed to open file");
    		dump(&cd->file.opened, cd->file.opened.nodes, 0);
    		wz_vfs_file_close(&cd->file);
    	} else if (wcscmp(command, L"save") == 0) {
    		if (cd->kind != WZ_VFS_NODE_KIND_FILE) {
    			printf("can only save from files\n");
    			continue;
    		}

    		CHECK(wz_vfs_file_open(&vfs, &cd->file),
    			ERROR_KIND_FILEOPENFAILED, L"failed to open file");

    		size_t wfilename_len = 0;
    		wchar_t* wfilename = NULL;
    		tokenize(
    			&wfilename,
    			&wfilename_len,
    			command + command_len + 1,
    			L" \n");

    		if (wfilename == NULL || wcslen(wfilename) == 0) {
    			printf("specify filename\n");
    			wz_vfs_file_close(&cd->file);
    			continue;
    		}
    		wfilename[wfilename_len] = 0;

    		size_t propertyname_len = 0;
    		wchar_t* propertyname = NULL;
    		tokenize(
    			&propertyname,
    			&propertyname_len,
    			wfilename + wfilename_len + 1,
    			L" \n");

    		if (propertyname == NULL || wcslen(propertyname) == 0) {
    			printf("specify property name\n");
    			wz_vfs_file_close(&cd->file);
    			continue;
    		}
    		propertyname[propertyname_len] = 0;

    		struct wz_vfs_openedfile_node* property = NULL;
    		wz_vfs_openedfile_node_wfind(
    			&property,
    			&cd->file.opened,
    			cd->file.opened.nodes,
    			propertyname);
    		if (property == NULL) {
    			printf("failed to find property\n");
    			wz_vfs_file_close(&cd->file);
    			continue;
    		}

    		if (property->kind != WZ_PROPERTY_KIND_CANVAS) {
    			printf("can only save canvas properties\n");
    			wz_vfs_file_close(&cd->file);
    			continue;
    		}

    		char filename[entry_len] = {0};
    		snprintf(filename, entry_len, "%ls", wfilename);

    		FILE* outf = fopen(filename, "wb");
    		if (outf == NULL) {
    			printf("failed to open output file %s\n", filename);
    			wz_vfs_file_close(&cd->file);
    			continue;
    		}

    		uint8_t* encoded = NULL;
    		size_t encoded_size = 0;
    		unsigned ret = lodepng_encode32(
    			&encoded,
    			&encoded_size,
    			property->canvas.image_data,
    			property->canvas.image.width,
    			property->canvas.image.height);
    		if (ret) {
    			printf("failed to encode image: %u\n", ret);
    			fclose(outf);
    			wz_vfs_file_close(&cd->file);
    			continue;
    		}

    		size_t write_ret = fwrite(encoded, encoded_size, 1, outf);
    		free(encoded);
    		fclose(outf);
    		wz_vfs_file_close(&cd->file);
    		if (write_ret != 1) {
    			printf("failed to write encoded image\n");
    			continue;
    		}
    	}
    }

    vec_free(pathcomponent)(&path);
    wz_vfs_free(&vfs);
	return err;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s wz-file\n", argv[0]);
		return 1;
	}

	struct error err = main_(argc, argv);
	if (error_top(&err)) {
		error_printto(&err, stderr);
		fprintf(stderr, "\n");
		return 1;
	}

	return 0;
}
