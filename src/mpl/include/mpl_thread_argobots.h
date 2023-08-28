/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

/* This file is used when configured with (MPICH_THREAD_PACKAGE_NAME ==
 * MPICH_THREAD_PACKAGE_ARGOBOTS) */

#ifndef MPL_THREAD_ARGOBOTS_H_INCLUDED
#define MPL_THREAD_ARGOBOTS_H_INCLUDED

#include "mpl.h"
#include "abt.h"

#include <errno.h>
#include <assert.h>

typedef ABT_mutex MPL_thread_mutex_t;
typedef ABT_cond MPL_thread_cond_t;
typedef uintptr_t MPL_thread_id_t;
typedef ABT_key MPL_thread_tls_key_t;

/* ======================================================================
 *    Creation and misc
 * ======================================================================*/

/* MPL_thread_init()/MPL_thread_finalize() can be called in a nested manner
 * (e.g., MPI_T_init_thread() and MPI_Init_thread()), but Argobots internally
 * maintains a counter so it is okay. */
#define MPL_thread_init(err_ptr_)                                             \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_init(0, NULL);                                            \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_init", err__,                  \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_finalize(err_ptr_)                                         \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_finalize();                                               \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_finalize", err__,              \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

/* MPL_thread_create() defined in mpiu_thread_argobots.c */
typedef void (*MPL_thread_func_t) (void *data);
void MPL_thread_create(MPL_thread_func_t func, void *data, MPL_thread_id_t * idp, int *errp);

#define MPL_thread_exit()
#define MPL_thread_self(idp_)                                                 \
    do {                                                                      \
        ABT_thread self_thread_tmp_ = ABT_THREAD_NULL;                        \
        ABT_thread_self(&self_thread_tmp_);                                   \
        uintptr_t id_tmp_;                                                    \
        if (self_thread_tmp_ == ABT_THREAD_NULL) {                            \
            /* It seems that an external thread calls this function. */       \
            /* Use Pthreads ID instead. */                                    \
            id_tmp_ = (uintptr_t)pthread_self();                              \
            /* Set a bit to the last bit.                                     \
             * Note that the following shifts bits first because pthread_t    \
             * might use the last bit if, for example, pthread_t saves an ID  \
             * starting from zero; overwriting the last bit can cause a       \
             * conflict.  The last bit that is shifted out is less likely to  \
             * be significant. */                                             \
            id_tmp_ = (id_tmp_ << 1) | (uintptr_t)0x1;                        \
        } else {                                                              \
            id_tmp_ = (uintptr_t)self_thread_tmp_;                            \
            /* If ID is that of Argobots, the last bit is not set because     \
             * ABT_thread points to an aligned memory region.  Since          \
             * ABT_thread is not modified, this ID can be directly used by    \
             * MPL_thread_join().  Let's check it. */                         \
            assert(!(id_tmp_ & (uintptr_t)0x1));                              \
        }                                                                     \
        *(idp_) = id_tmp_;                                                    \
    } while (0)
#define MPL_thread_join(id_) ABT_thread_free((ABT_thread *)&(id_))
#define MPL_thread_same(idp1_, idp2_, same_)                                  \
    do {                                                                      \
        /*                                                                    \
         * TODO: strictly speaking, Pthread-Pthread and Pthread-Argobots IDs  \
         * are not arithmetically comparable, while it is okay on most        \
         * platforms.  This should be fixed.  Note that Argobots-Argobots ID  \
         * comparison is okay.                                                \
         */                                                                   \
        *(same_) = (*(idp1_) == *(idp2_)) ? TRUE : FALSE;                     \
    } while (0)

/* See mpl_thread_posix.h for interface description. */
void MPL_thread_set_affinity(MPL_thread_id_t thread, int *affinity_arr, int affinity_size,
                             int *err);

/* ======================================================================
 *    Scheduling
 * ======================================================================*/

#define MPL_thread_yield ABT_thread_yield

/* ======================================================================
 *    Mutexes
 * ======================================================================*/
#define MPL_thread_mutex_create(mutex_ptr_, err_ptr_)                         \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_mutex_create(mutex_ptr_);                                 \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_mutex_create", err__,          \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_mutex_destroy(mutex_ptr_, err_ptr_)                        \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_mutex_free(mutex_ptr_);                                   \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_mutex_free", err__,            \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_mutex_lock(mutex_ptr_, err_ptr_, prio_)                    \
    do {                                                                      \
        int err__;                                                            \
        if (prio_ == MPL_THREAD_PRIO_HIGH) {                                  \
            err__ = ABT_mutex_lock(*mutex_ptr_);                              \
        } else {                                                              \
            assert(prio_ == MPL_THREAD_PRIO_LOW);                             \
            err__ = ABT_mutex_lock_low(*mutex_ptr_);                          \
        }                                                                     \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_mutex_lock", err__,            \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_mutex_unlock(mutex_ptr_, err_ptr_)                         \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_mutex_unlock(*mutex_ptr_);                                \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_mutex_unlock", err__,          \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_mutex_unlock_se(mutex_ptr_, err_ptr_)                      \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_mutex_unlock_se(*mutex_ptr_);                             \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_mutex_unlock_se", err__,       \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

/* ======================================================================
 *    Condition Variables
 * ======================================================================*/

#define MPL_thread_cond_create(cond_ptr_, err_ptr_)                           \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_cond_create((cond_ptr_));                                 \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_cond_create", err__,           \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_cond_destroy(cond_ptr_, err_ptr_)                          \
    do {                                                                      \
        int err__;                                                            \
        err__ = ABT_cond_free(cond_ptr_);                                     \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_cond_free", err__,             \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_cond_wait(cond_ptr_, mutex_ptr_, err_ptr_)                   \
    do {                                                                        \
        int err__;                                                              \
        MPL_DBG_MSG_FMT(THREAD,TYPICAL,                                         \
                        (MPL_DBG_FDEST,                                         \
                         "Enter cond_wait on cond=%p mutex=%p",                 \
                         (cond_ptr_),(mutex_ptr_)));                            \
        do {                                                                    \
            err__ = ABT_cond_wait((*cond_ptr_), *mutex_ptr_);                   \
        } while (err__ == EINTR);                                               \
        *(int *)(err_ptr_) = err__;                                             \
        if (unlikely(err__))                                                    \
            MPL_internal_sys_error_printf("ABT_cond_free", err__,                 \
                   "    %s:%d error in cond_wait on cond=%p mutex=%p err__=%d", \
                   __FILE__, __LINE__);       \
        MPL_DBG_MSG_FMT(THREAD,TYPICAL,(MPL_DBG_FDEST,                          \
                                        "Exit cond_wait on cond=%p mutex=%p",   \
                                        (cond_ptr_),(mutex_ptr_)));             \
    } while (0)

#define MPL_thread_cond_broadcast(cond_ptr_, err_ptr_)                        \
    do {                                                                      \
        int err__;                                                            \
        MPL_DBG_MSG_P(THREAD,TYPICAL,                                         \
                      "About to cond_broadcast on MPL_thread_cond %p",        \
                      (cond_ptr_));                                           \
        err__ = ABT_cond_broadcast((*cond_ptr_));                             \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_cond_broadcast", err__,        \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

#define MPL_thread_cond_signal(cond_ptr_, err_ptr_)                           \
    do {                                                                      \
        int err__;                                                            \
        MPL_DBG_MSG_P(THREAD,TYPICAL,                                         \
                      "About to cond_signal on MPL_thread_cond %p",           \
                      (cond_ptr_));                                           \
        err__ = ABT_cond_signal((*cond_ptr_));                                \
        if (unlikely(err__))                                                  \
            MPL_internal_sys_error_printf("ABT_cond_signal", err__,           \
                                          "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                           \
    } while (0)

/* ======================================================================
 *    Thread Local Storage
 * ======================================================================*/

#define MPL_NO_COMPILER_TLS     /* Cannot use compiler tls with argobots */

#define MPL_thread_tls_create(exit_func_ptr_, tls_ptr_, err_ptr_)         \
    do {                                                                  \
        int err__;                                                        \
        err__ = ABT_key_create((exit_func_ptr_), (tls_ptr_));             \
        if (unlikely(err__))                                              \
        MPL_internal_sys_error_printf("ABT_key_create", err__,            \
                                      "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = 0;                                           \
    } while (0)

#define MPL_thread_tls_destroy(tls_ptr_, err_ptr_)                        \
    do {                                                                  \
        int err__;                                                        \
        err__ = ABT_key_free(tls_ptr_);                                   \
        if (unlikely(err__))                                              \
        MPL_internal_sys_error_printf("ABT_key_free", err__,              \
                                      "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                       \
    } while (0)

#define MPL_thread_tls_set(tls_ptr_, value_, err_ptr_)                    \
    do {                                                                  \
        int err__;                                                        \
        err__ = ABT_key_set(*(tls_ptr_), (value_));                       \
        if (unlikely(err__))                                              \
        MPL_internal_sys_error_printf("ABT_key_set", err__,               \
                                      "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                       \
    } while (0)

#define MPL_thread_tls_get(tls_ptr_, value_ptr_, err_ptr_)                \
    do {                                                                  \
        int err__;                                                        \
        err__ = ABT_key_get(*(tls_ptr_), (value_ptr_));                   \
        if (unlikely(err__))                                              \
        MPL_internal_sys_error_printf("ABT_key_get", err__,               \
                                      "    %s:%d\n", __FILE__, __LINE__); \
        *(int *)(err_ptr_) = err__;                                       \
    } while (0)

/*
 * Utility functions that are natively supported by the experimental version of Argobots.
 */

#ifndef ABTX_FAST_SELF_GET_TLS_PTR
typedef struct {
    char dummy1[64];
    void *abt_tls_key;
    char dummy2[64];
} MPL_global_abt_info;
extern MPL_global_abt_info g_abt_info;

#define ABTX_FAST_TLS_SIZE 1024
static void ABTX_FAST_SELF_GET_TLS_PTR_destructor(void *ptr)
{
    MPL_free(ptr);
}

static inline void *ABTX_FAST_SELF_GET_TLS_PTR(void)
{
    int ret;
    ABT_key tls_key = (ABT_key) __atomic_load_n(&g_abt_info.abt_tls_key, __ATOMIC_RELAXED);
    /* Atomically create the TLS key. */
    while (unlikely(tls_key == 0)) {
        ABT_key new_key;
        ret = ABT_key_create(ABTX_FAST_SELF_GET_TLS_PTR_destructor, &new_key);
        assert(ret == ABT_SUCCESS);
        void *expected = NULL;
        if (__atomic_compare_exchange_n((void **) &g_abt_info.abt_tls_key, &expected,
                                        (void *) new_key, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            /* Successfully set the key. */
            tls_key = new_key;
            break;
        } else {
            ABT_key_free(&new_key);
            tls_key = (ABT_key) __atomic_load_n(&g_abt_info.abt_tls_key, __ATOMIC_RELAXED);
        }
    }
    /* Anyway, let's use this key. */
    void *p_tls;
    ret = ABT_self_get_specific(tls_key, (void **) &p_tls);
    assert(ret == ABT_SUCCESS);
    if (unlikely(!p_tls)) {
        /* p_tls is not created. */
        p_tls = (void *) MPL_malloc(ABTX_FAST_TLS_SIZE, MPL_MEM_OTHER);
        /* Must be 0-initialized. */
        memset(p_tls, 0, ABTX_FAST_TLS_SIZE);
        ret = ABT_self_set_specific(tls_key, p_tls);
        assert(ret == ABT_SUCCESS);
    }
    /* Use that p_tls. */
    return p_tls;
}

#endif /* ABTX_FAST_SELF_GET_TLS_PTR */

#ifndef ABTX_FAST_SELF_GET_ASSOCIATED_POOL
static inline ABT_pool ABTX_FAST_SELF_GET_ASSOCIATED_POOL(void)
{
    int ret;
    ABT_thread self_thread;
    ret = ABT_self_get_thread(&self_thread);
    assert(ret == ABT_SUCCESS);
    ABT_pool pool;
    ret = ABT_thread_get_last_pool(self_thread, &pool);
    assert(ret == ABT_SUCCESS);
    return pool;
}
#endif /* ABTX_FAST_SELF_GET_ASSOCIATED_POOL */

#ifndef ABTX_FAST_SELF_GET_THREAD
static inline ABT_thread ABTX_FAST_SELF_GET_THREAD(void)
{
    int ret;
    ABT_thread self_thread;
    ret = ABT_self_get_thread(&self_thread);
    assert(ret == ABT_SUCCESS);
    return self_thread;
}
#endif /* ABTX_FAST_SELF_GET_THREAD */

static inline MPL_thread_id_t MPL_thread_get_self_fast(void)
{
    return (uintptr_t) ((void *) ABTX_FAST_SELF_GET_THREAD());
}

#ifndef ABTX_FAST_SELF_GET_TLS_PTR
static inline void *MPL_thread_get_tls_ptr_and_self_fast(MPL_thread_id_t * p_thread_id)
{
    *p_thread_id = MPL_thread_get_self_fast();
    return ABTX_FAST_SELF_GET_TLS_PTR();
}
#else
static inline void *MPL_thread_get_tls_ptr_and_self_fast(MPL_thread_id_t * p_thread_id)
{
    void *base_tls_ptr = ABTX_fast_self_get_base_tls_ptr();
    void **pp_thread = (void **) base_tls_ptr;
    *p_thread_id = (uintptr_t) * pp_thread;
    return (void *) (((char *) base_tls_ptr) + sizeof(void *));;
}
#endif /* ABTX_FAST_SELF_GET_TLS_PTR */

#ifndef ABTX_FAST_SET_ASSOCIATED_POOL_AND_YIELD
static inline int ABTX_FAST_SET_ASSOCIATED_POOL_AND_YIELD(ABT_pool pool)
{
    int ret;
    ABT_thread self_thread;
    ret = ABT_self_get_thread(&self_thread);
    assert(ret == ABT_SUCCESS);
    ret = ABT_thread_set_associated_pool(self_thread, pool);
    assert(ret == ABT_SUCCESS);
    ret = ABT_self_yield();
    assert(ret == ABT_SUCCESS);
    return ABT_SUCCESS;
}
#endif /* ABTX_FAST_SET_ASSOCIATED_POOL_AND_YIELD */

#endif /* MPL_THREAD_ARGOBOTS_H_INCLUDED */
