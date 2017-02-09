#ifndef __VECTOR_H
#define __VECTOR_H
#include <stdint.h>

/*
 * Simple vector of void pointers that grows dynamically. If it runs out of
 * space, the capacity is doubled.
 */

struct vector_t {
    void **items;
    uint32_t capacity;
    uint32_t length;
};

struct vector_t *vector_init();
void vector_add(struct vector_t *vec, void *item);
void vector_insert(struct vector_t *vec, void *item, uint32_t index);
void vector_del(struct vector_t *vec, uint32_t index);
void vector_foreach(struct vector_t *vec, void (*foreach_f)(void *item));
void vector_free(struct vector_t *vec);

#endif
