#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "vector.h"

inline static void vector_resize(struct vector_t *vec) {
    if (vec->length == vec->capacity) {
        vec->capacity *= 2;
        vec->items = realloc(vec->items, sizeof(void *) * vec->capacity);
    }
}

struct vector_t *vector_init() {
    struct vector_t *new_vec = malloc(sizeof(struct vector_t));
    if (!new_vec) {
        return NULL;
    }

    new_vec->capacity = 16;
    new_vec->length = 0;
    new_vec->items = malloc(sizeof(void *) * new_vec->capacity);
    if (!new_vec->items) {
        free(new_vec);
        return NULL;
    }

    return new_vec;
}

void vector_add(struct vector_t *vec, void *item) {
    assert(vec);
    vector_resize(vec);
    vec->items[vec->length] = item;
    vec->length++;
}

void vector_insert(struct vector_t *vec, void *item, uint32_t index) {
    assert(vec && (index <= vec->length));
    vector_resize(vec);
    memmove(&vec->items[index + 1], &vec->items[index],
            sizeof(void *) * (vec->length - index));
    vec->items[index] = item;
    vec->length++;
}

void vector_del(struct vector_t *vec, uint32_t index) {
    assert(vec && (vec->length > 0) && (index < vec->length));
    vec->length--;
    memmove(&vec->items[index], &vec->items[index + 1],
            sizeof(void *) * (vec->length - index));
}

void vector_foreach(struct vector_t *vec, void (*foreach_f)(void *item)) {
    assert(vec && foreach_f);
    for (uint32_t i = 0; i < vec->length; i++) {
        foreach_f(vec->items[i]);
    }
}

void vector_free(struct vector_t *vec) {
    assert(vec);
    free(vec->items);
    free(vec);
}
