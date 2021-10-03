#ifndef INCLUDED_UTIL_VEC_H
#define INCLUDED_UTIL_VEC_H

#include <stdint.h>

#define vec(ty) vec_ ## ty
#define vec_iterator(ty) vec_ ## ty ## _iterator

#define vec_push(ty) vec_ ## ty ## _push
#define vec_pop(ty) vec_ ## ty ## _pop
#define vec_iterator_next(ty) vec_ ## ty ## _iterator_next
#define vec_free(ty) vec_ ## ty ## _free

#define vec_declare(ty) \
struct vec_ ## ty { \
	uint32_t len; \
	uint32_t cap; \
	struct ty* items; \
}; \
\
struct vec_ ## ty ## _iterator; \
\
void vec_ ## ty ## _push( \
	struct vec_ ## ty* vec, \
	struct ty item); \
struct ty* vec_ ## ty ## _pop( \
	struct vec_ ## ty* vec); \
void vec_ ## ty ## _trim( \
	struct vec_ ## ty* vec); \
struct vec_ ## ty ## _iterator vec_ ## ty ## _iterator( \
	struct vec_ ## ty* vec); \
void vec_ ## ty ## _free( \
	struct vec_ ## ty* vec); \
\
struct vec_ ## ty ## _iterator { \
	uint32_t i; \
	struct vec_ ## ty* vec; \
}; \
\
struct ty* vec_ ## ty ## _iterator_next( \
	struct vec_ ## ty ## _iterator* it)

#define vec_define(ty) \
static void vec_ ## ty ## _growtofit( \
	struct vec_ ## ty* vec, \
	uint32_t new_cap) { \
	if (new_cap < vec->cap) \
		return; \
	while (new_cap < vec->cap) { \
		if (new_cap == 0) new_cap = 1; \
		if (new_cap < 1024) new_cap *= 2; \
		else new_cap += 1024; \
	} \
	vec->items = realloc(vec->items, new_cap * sizeof(*vec->items)); \
	vec->cap = new_cap; \
} \
void vec_ ## ty ## _push( \
	struct vec_ ## ty* vec, \
	struct ty item) { \
	vec_ ## ty ## _growtofit(vec, vec->len + 1); \
	vec->items[vec->len] = item; \
	++vec->len; \
} \
struct ty* vec_ ## ty ## _pop( \
	struct vec_ ## ty* vec) { \
	if (vec->len == 0) return NULL; \
	struct ty* ret = &vec->items[vec->len - 1]; \
	--vec->len; \
	return ret; \
} \
void vec_ ## ty ## _trim( \
	struct vec_ ## ty* vec) { \
	vec->items = realloc(vec->items, vec->len * sizeof(*vec->items)); \
	vec->cap = vec->len; \
} \
struct vec_ ## ty ## _iterator vec_ ## ty ## _iterator( \
	struct vec_ ## ty* vec) { \
	struct vec_ ## ty ## _iterator it = { \
		.i = 0, \
		.vec = vec, \
	}; \
	return it; \
} \
void vec_ ## ty ## _free( \
	struct vec_ ## ty* vec) { \
	free(vec->items); \
} \
\
struct ty* vec_ ## ty ## _iterator_next( \
	struct vec_ ## ty ## _iterator* it) { \
	if (it->i >= it->vec->len) return NULL; \
	struct ty* ret = &it->vec->items[it->i]; \
	++it->i; \
	return ret; \
}

#endif
