#ifndef _COLLECTION_H
#define _COLLECTION_H

#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 500L
    #define _XOPEN_SOURCE 500L
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <features.h>

#include "asserts.h"
#include "geometry.h"
#include "logging.h"
#include "macros.h"
#include "refcounting.h"
#include "uuid.h"

struct queue {
    size_t start_index;
    size_t length;
    size_t size;
    void *elements;

    size_t max_queue_size;
    size_t element_size;
};

struct concurrent_queue {
    pthread_mutex_t mutex;
    pthread_cond_t is_dequeueable;
    pthread_cond_t is_enqueueable;
    struct queue queue;
};

struct pointer_set {
    /**
	 * @brief The number of non-NULL pointers currently stored in @ref pointers.
	 */
    size_t count_pointers;

    /**
	 * @brief The current size of the @ref pointers memory block, in pointers.
	 */
    size_t size;

    /**
	 * @brief The actual memory where the pointers are stored.
	 */
    void **pointers;

    /**
	 * @brief The maximum size of the @ref pointers memory block, in pointers.
	 */
    size_t max_size;

    /**
	 * @brief Whether this pointer_set is using static memory.
	 */
    bool is_static;
};

struct concurrent_pointer_set {
    pthread_mutex_t mutex;
    struct pointer_set set;
};

#define QUEUE_DEFAULT_MAX_SIZE 64

#define QUEUE_INITIALIZER(element_type, _max_size)      \
    ((struct queue){ .start_index = 0,                  \
                     .length = 0,                       \
                     .size = 0,                         \
                     .elements = NULL,                  \
                     .max_queue_size = _max_queue_size, \
                     .element_size = sizeof(element_type) })

#define CQUEUE_DEFAULT_MAX_SIZE 64

#define CQUEUE_INITIALIZER(element_type, _max_size)                         \
    ((struct concurrent_queue){ .mutex = PTHREAD_MUTEX_INITIALIZER,         \
                                .is_dequeueable = PTHREAD_COND_INITIALIZER, \
                                .is_enqueueable = PTHREAD_COND_INITIALIZER, \
                                .queue = QUEUE_INITIALIZER(element_type, _max_queue_size) })

#define PSET_DEFAULT_MAX_SIZE 64

#define PSET_INITIALIZER(_max_size) \
    ((struct pointer_set){ .count_pointers = 0, .size = 0, .pointers = NULL, .max_size = _max_size, .is_static = false })

#define PSET_INITIALIZER_STATIC(_storage, _size) \
    ((struct pointer_set){ .count_pointers = 0, .size = _size, .pointers = _storage, .max_size = _size, .is_static = true })

#define CPSET_DEFAULT_MAX_SIZE 64

#define CPSET_INITIALIZER(_max_size)       \
    ((struct concurrent_pointer_set        \
    ){ .mutex = PTHREAD_MUTEX_INITIALIZER, \
       .set = { .count_pointers = 0, .size = 0, .pointers = NULL, .max_size = _max_size, .is_static = false } })

int queue_init(struct queue *queue, size_t element_size, size_t max_queue_size);

int queue_deinit(struct queue *queue);

int queue_enqueue(struct queue *queue, const void *p_element);

int queue_dequeue(struct queue *queue, void *element_out);

int queue_peek(struct queue *queue, void **pelement_out);

int cqueue_init(struct concurrent_queue *queue, size_t element_size, size_t max_queue_size);

int cqueue_deinit(struct concurrent_queue *queue);

static inline int cqueue_lock(struct concurrent_queue *const queue) {
    return pthread_mutex_lock(&queue->mutex);
}

static inline int cqueue_unlock(struct concurrent_queue *const queue) {
    return pthread_mutex_unlock(&queue->mutex);
}

int cqueue_try_enqueue_locked(struct concurrent_queue *queue, const void *p_element);

int cqueue_enqueue_locked(struct concurrent_queue *queue, const void *p_element);

int cqueue_try_enqueue(struct concurrent_queue *queue, const void *p_element);

int cqueue_enqueue(struct concurrent_queue *queue, const void *p_element);

int cqueue_try_dequeue_locked(struct concurrent_queue *queue, void *element_out);

int cqueue_dequeue_locked(struct concurrent_queue *queue, void *element_out);

int cqueue_try_dequeue(struct concurrent_queue *queue, void *element_out);

int cqueue_dequeue(struct concurrent_queue *queue, void *element_out);

int cqueue_peek_locked(struct concurrent_queue *queue, void **pelement_out);

/*
 * pointer set
 */
int pset_init(struct pointer_set *set, size_t max_size);

int pset_init_static(struct pointer_set *set, void **storage, size_t size);

void pset_deinit(struct pointer_set *set);

int pset_put(struct pointer_set *set, void *pointer);

bool pset_contains(const struct pointer_set *set, const void *pointer);

int pset_remove(struct pointer_set *set, const void *pointer);

static inline int pset_get_count_pointers(const struct pointer_set *set) {
    return set->count_pointers;
}

/**
 * @brief Returns the size of the internal storage of set, in pointers.
 */
static inline int pset_get_storage_size(const struct pointer_set *set) {
    return set->size;
}

int pset_copy(const struct pointer_set *src, struct pointer_set *dest);

void pset_intersect(struct pointer_set *src_dest, const struct pointer_set *b);

int pset_union(struct pointer_set *src_dest, const struct pointer_set *b);

int pset_subtract(struct pointer_set *minuend_difference, const struct pointer_set *subtrahend);

void *__pset_next_pointer(struct pointer_set *set, const void *pointer);

#define for_each_pointer_in_pset(set, pointer) \
    for ((pointer) = __pset_next_pointer(set, NULL); (pointer) != NULL; (pointer) = __pset_next_pointer(set, (pointer)))

/*
 * concurrent pointer set
 */
int cpset_init(struct concurrent_pointer_set *set, size_t max_size);

void cpset_deinit(struct concurrent_pointer_set *set);

static inline int cpset_lock(struct concurrent_pointer_set *set) {
    return pthread_mutex_lock(&set->mutex);
}

static inline int cpset_unlock(struct concurrent_pointer_set *set) {
    return pthread_mutex_unlock(&set->mutex);
}

static inline int cpset_put_locked(struct concurrent_pointer_set *set, void *pointer) {
    return pset_put(&set->set, pointer);
}

static inline int cpset_put(struct concurrent_pointer_set *set, void *pointer) {
    int ok;

    cpset_lock(set);
    ok = pset_put(&set->set, pointer);
    cpset_unlock(set);

    return ok;
}

static inline bool cpset_contains_locked(struct concurrent_pointer_set *set, const void *pointer) {
    return pset_contains(&set->set, pointer);
}

static inline bool cpset_contains(struct concurrent_pointer_set *set, const void *pointer) {
    bool result;

    cpset_lock(set);
    result = pset_contains(&set->set, pointer);
    cpset_unlock(set);

    return result;
}

static inline int cpset_remove_locked(struct concurrent_pointer_set *set, const void *pointer) {
    return pset_remove(&set->set, pointer);
}

static inline int cpset_remove(struct concurrent_pointer_set *set, const void *pointer) {
    int ok;

    cpset_lock(set);
    ok = cpset_remove_locked(set, pointer);
    cpset_unlock(set);

    return ok;
}

static inline int cpset_get_count_pointers_locked(const struct concurrent_pointer_set *set) {
    return set->set.count_pointers;
}

/**
 * @brief Returns the size of the internal storage of set, in pointers.
 */
static inline int cpset_get_storage_size_locked(const struct concurrent_pointer_set *set) {
    return set->set.size;
}

static inline int cpset_copy_into_pset_locked(struct concurrent_pointer_set *src, struct pointer_set *dest) {
    return pset_copy(&src->set, dest);
}

static inline void *__cpset_next_pointer_locked(struct concurrent_pointer_set *set, const void *pointer) {
    return __pset_next_pointer(&set->set, pointer);
}

#define for_each_pointer_in_cpset(set, pointer) \
    for ((pointer) = __cpset_next_pointer_locked(set, NULL); (pointer) != NULL; (pointer) = __cpset_next_pointer_locked(set, (pointer)))

static inline void *memdup(const void *restrict src, const size_t n) {
    void *__restrict__ dest;

    if ((src == NULL) || (n == 0))
        return NULL;

    dest = malloc(n);
    if (dest == NULL)
        return NULL;

    return memcpy(dest, src, n);
}

/**
 * @brief Get the current time of the system monotonic clock.
 * @returns time in nanoseconds.
 */
static inline uint64_t get_monotonic_time(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_nsec + time.tv_sec * 1000000000ull;
}

#define DECLARE_LOCK_OPS(obj_name)                     \
    UNUSED void obj_name##_lock(struct obj_name *obj); \
    UNUSED void obj_name##_unlock(struct obj_name *obj);

#define DEFINE_LOCK_OPS(obj_name, mutex_member_name)        \
    UNUSED void obj_name##_lock(struct obj_name *obj) {     \
        int ok;                                             \
        ok = pthread_mutex_lock(&obj->mutex_member_name);   \
        ASSERT_EQUALS_MSG(ok, 0, "Error locking mutex.");   \
        (void) ok;                                          \
    }                                                       \
    UNUSED void obj_name##_unlock(struct obj_name *obj) {   \
        int ok;                                             \
        ok = pthread_mutex_unlock(&obj->mutex_member_name); \
        ASSERT_EQUALS_MSG(ok, 0, "Error unlocking mutex."); \
        (void) ok;                                          \
    }

#define DEFINE_STATIC_LOCK_OPS(obj_name, mutex_member_name)      \
    UNUSED static void obj_name##_lock(struct obj_name *obj) {   \
        int ok;                                                  \
        ok = pthread_mutex_lock(&obj->mutex_member_name);        \
        ASSERT_EQUALS_MSG(ok, 0, "Error locking mutex.");        \
        (void) ok;                                               \
    }                                                            \
    UNUSED static void obj_name##_unlock(struct obj_name *obj) { \
        int ok;                                                  \
        ok = pthread_mutex_unlock(&obj->mutex_member_name);      \
        ASSERT_EQUALS_MSG(ok, 0, "Error unlocking mutex.");      \
        (void) ok;                                               \
    }

#define DEFINE_INLINE_LOCK_OPS(obj_name, mutex_member_name)             \
    UNUSED static inline void obj_name##_lock(struct obj_name *obj) {   \
        int ok;                                                         \
        ok = pthread_mutex_lock(&obj->mutex_member_name);               \
        ASSERT_EQUALS_MSG(ok, 0, "Error locking mutex.");               \
        (void) ok;                                                      \
    }                                                                   \
    UNUSED static inline void obj_name##_unlock(struct obj_name *obj) { \
        int ok;                                                         \
        ok = pthread_mutex_unlock(&obj->mutex_member_name);             \
        ASSERT_EQUALS_MSG(ok, 0, "Error unlocking mutex.");             \
        (void) ok;                                                      \
    }

#define BITCAST(to_type, value) (*((const to_type *) (&(value))))

static inline int32_t uint32_to_int32(const uint32_t v) {
    return BITCAST(int32_t, v);
}

static inline uint32_t int32_to_uint32(const int32_t v) {
    return BITCAST(uint32_t, v);
}

static inline uint64_t int64_to_uint64(const int64_t v) {
    return BITCAST(uint64_t, v);
}

static inline int64_t uint64_to_int64(const uint64_t v) {
    return BITCAST(int64_t, v);
}

static inline int64_t ptr_to_int64(const void *const ptr) {
    union {
        const void *ptr;
        int64_t int64;
    } u;

    u.int64 = 0;
    u.ptr = ptr;
    return u.int64;
}

static inline void *int64_to_ptr(const int64_t v) {
    union {
        void *ptr;
        int64_t int64;
    } u;

    u.int64 = v;
    return u.ptr;
}

static inline int64_t ptr_to_uint32(const void *const ptr) {
    union {
        const void *ptr;
        uint32_t u32;
    } u;

    u.u32 = 0;
    u.ptr = ptr;
    return u.u32;
}

static inline void *uint32_to_ptr(const uint32_t v) {
    union {
        void *ptr;
        uint32_t u32;
    } u;

    u.ptr = NULL;
    u.u32 = v;
    return u.ptr;
}

#define MAX_ALIGNMENT (__alignof__(max_align_t))
#define IS_MAX_ALIGNED(num) ((num) % MAX_ALIGNMENT == 0)

#define DOUBLE_TO_FP1616(v) ((uint32_t) ((v) *65536))
#define DOUBLE_TO_FP1616_ROUNDED(v) (((uint32_t) (v)) << 16)

typedef void (*void_callback_t)(void *userdata);

ATTR_PURE static inline bool streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

const pthread_mutexattr_t *get_default_mutex_attrs();

#endif
