#ifndef INCLUDED_UTIL_MAP_H
#define INCLUDED_UTIL_MAP_H

#include <stdint.h>
#include <wchar.h>

static inline uint32_t wstr_hash(const wchar_t* s) {
	uint32_t hash = 5381;

	while (*s) {
		hash = ((hash << 5) + hash) + *s;
		++s;
	}

	return hash;
}

static inline uint32_t str_hash(const char* s) {
	uint32_t hash = 5381;

	while (*s) {
		hash = ((hash << 5) + hash) + *s;
		++s;
	}

	return hash;
}

static inline int str_compare(const wchar_t* l, const char* r) {
	while (*l && *r) {
		if (*l != (wchar_t) *r)
			return 1;

		++l;
		++r;
	}
	if (*l || *r)
		return 1;
	return 0;
}

#define str_map(ty) str_map_ ## ty

#define str_map_declare(ty) struct str_map_ ## ty ## _node { \
	struct str_map_ ## ty ## _node* next; \
	wchar_t* key; \
	struct ty value; \
}; \
\
void str_map_ ## ty ## _node_free( \
	struct str_map_ ## ty ## _node* n); \
\
struct str_map_ ## ty { \
	struct str_map_ ## ty ## _node buckets[128]; \
}; \
\
struct str_map_ ## ty ## _iterator; \
\
struct ty* str_map_ ## ty ## _winsert( \
	struct str_map_ ## ty* map, \
	const wchar_t* key, \
	struct ty from); \
struct ty* str_map_ ## ty ## _wget( \
	struct str_map_ ## ty* map, \
	const wchar_t* key); \
struct ty* str_map_ ## ty ## _get( \
	struct str_map_ ## ty* map, \
	const char* key); \
struct str_map_ ## ty ## _iterator str_map_ ## ty ## _iterator( \
	struct str_map_ ## ty* map); \
void str_map_ ## ty ## _free( \
	struct str_map_ ## ty* map); \
\
struct str_map_ ## ty ## _iterator { \
	struct str_map_ ## ty* map; \
	size_t bucket; \
	struct str_map_ ## ty ## _node* node; \
}; \
\
struct ty* str_map_ ## ty ## _iterator_next( \
	struct str_map_ ## ty ## _iterator* it)

#define str_map_define(ty) \
void str_map_ ## ty ## _node_free( \
	struct str_map_ ## ty ## _node* n) { \
	free(n->key); \
} \
\
struct ty* str_map_ ## ty ## _winsert( \
	struct str_map_ ## ty* map, \
	const wchar_t* key, \
	struct ty from) { \
	uint32_t hash = wstr_hash(key); \
\
	struct str_map_ ## ty ## _node* bucket = &map->buckets[hash % 128]; \
	struct str_map_ ## ty ## _node* last_head = bucket->next; \
	struct str_map_ ## ty ## _node* new_head = calloc(1, sizeof(*new_head)); \
	new_head->key = calloc(wcslen(key) + 1, sizeof(*new_head->key)); \
	wcscpy(new_head->key, key); \
	new_head->value = from; \
\
	bucket->next = new_head; \
	new_head->next = last_head; \
	return &new_head->value; \
} \
struct ty* str_map_ ## ty ## _wget( \
	struct str_map_ ## ty* map, \
	const wchar_t* key) { \
	uint32_t hash = wstr_hash(key); \
\
	struct str_map_ ## ty ## _node* cur = map->buckets[hash % 128].next; \
	while (cur) { \
		if (wcscmp(key, cur->key) == 0) return &cur->value; \
		cur = cur->next; \
	} \
	return NULL; \
} \
struct ty* str_map_ ## ty ## _get( \
	struct str_map_ ## ty* map, \
	const char* key) { \
	uint32_t hash = str_hash(key); \
\
	struct str_map_ ## ty ## _node* cur = map->buckets[hash % 128].next; \
	while (cur) { \
		if (str_compare(cur->key, key) == 0) return &cur->value; \
		cur = cur->next; \
	} \
	return NULL; \
} \
struct str_map_ ## ty ## _iterator str_map_ ## ty ## _iterator( \
	struct str_map_ ## ty* map) { \
	struct str_map_ ## ty ## _iterator it = { \
		.map = map, \
		.bucket = 0, \
		.node = map->buckets[0].next, \
	}; \
	return it; \
} \
void str_map_ ## ty ## _free( \
	struct str_map_ ## ty* map) { \
	for (size_t i = 0; i < 128; ++i) { \
		struct str_map_ ## ty ## _node* cur = map->buckets[i].next; \
		while (cur) { \
			struct str_map_ ## ty ## _node* next = cur->next; \
			str_map_ ## ty ## _node_free(cur); \
			free(cur); \
			cur = next; \
		} \
	} \
} \
\
struct ty* str_map_ ## ty ## _iterator_next( \
	struct str_map_ ## ty ## _iterator* it) { \
	if (it->node) it->node = it->node->next; \
	while (it->node == NULL) { \
		++it->bucket; \
		if (it->bucket >= 128) break; \
		it->node = it->map->buckets[it->bucket].next; \
	} \
	if (it->node) return &it->node->value; \
	return NULL; \
}

#endif
