/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#ifndef MPIDU_THREAD_FALLBACK_H_INCLUDED
#define MPIDU_THREAD_FALLBACK_H_INCLUDED

/*
=== BEGIN_MPI_T_CVAR_INFO_BLOCK ===

cvars:
    - name        : MPIR_CVAR_ENABLE_HEAVY_YIELD
      category    : THREADS
      type        : boolean
      default     : 0
      class       : none
      verbosity   : MPI_T_VERBOSITY_USER_BASIC
      scope       : MPI_T_SCOPE_LOCAL
      description : >-
        If enabled, use nanosleep to ensure other threads have a chance to grab the lock.
        Note: this may not work with some thread runtimes, e.g. non-preemptive user-level
        threads.

=== END_MPI_T_CVAR_INFO_BLOCK ===
*/

/* some important critical section names:
 *   GLOBAL - entered/exited at beginning/end of (nearly) every MPI_ function
 *   INIT - entered before MPID_Init and exited near the end of MPI_Init(_thread)
 * See the analysis of the MPI routines for thread usage properties.  Those
 * routines considered "Access Only" do not require GLOBAL.  That analysis
 * was very general; in MPICH, some routines may have internal shared
 * state that isn't required by the MPI specification.  Perhaps the
 * best example of this is the MPI_ERROR_STRING routine, where the
 * instance-specific error messages make use of shared state, and hence
 * must be accessed in a thread-safe fashion (e.g., require an GLOBAL
 * critical section).  With such routines removed, the set of routines
 * that (probably) do not require GLOBAL include:
 *
 * MPI_CART_COORDS, MPI_CART_GET, MPI_CART_MAP, MPI_CART_RANK, MPI_CART_SHIFT,
 * MPI_CART_SUB, MPI_CARTDIM_GET, MPI_COMM_GET_NAME,
 * MPI_COMM_RANK, MPI_COMM_REMOTE_SIZE,
 * MPI_COMM_SET_NAME, MPI_COMM_SIZE, MPI_COMM_TEST_INTER, MPI_ERROR_CLASS,
 * MPI_FILE_GET_AMODE, MPI_FILE_GET_ATOMICITY, MPI_FILE_GET_BYTE_OFFSET,
 * MPI_FILE_GET_POSITION, MPI_FILE_GET_POSITION_SHARED, MPI_FILE_GET_SIZE
 * MPI_FILE_GET_TYPE_EXTENT, MPI_FILE_SET_SIZE,
g * MPI_FINALIZED, MPI_GET_COUNT, MPI_GET_ELEMENTS, MPI_GRAPH_GET,
 * MPI_GRAPH_MAP, MPI_GRAPH_NEIGHBORS, MPI_GRAPH_NEIGHBORS_COUNT,
 * MPI_GRAPHDIMS_GET, MPI_GROUP_COMPARE, MPI_GROUP_RANK,
 * MPI_GROUP_SIZE, MPI_GROUP_TRANSLATE_RANKS, MPI_INITIALIZED,
 * MPI_PACK, MPI_PACK_EXTERNAL, MPI_PACK_SIZE, MPI_TEST_CANCELLED,
 * MPI_TOPO_TEST, MPI_TYPE_EXTENT, MPI_TYPE_GET_ENVELOPE,
 * MPI_TYPE_GET_EXTENT, MPI_TYPE_GET_NAME, MPI_TYPE_GET_TRUE_EXTENT,
 * MPI_TYPE_LB, MPI_TYPE_SET_NAME, MPI_TYPE_SIZE, MPI_TYPE_UB, MPI_UNPACK,
 * MPI_UNPACK_EXTERNAL, MPI_WIN_GET_NAME, MPI_WIN_SET_NAME
 *
 * Some of the routines that could be read-only, but internally may
 * require access or updates to shared data include
 * MPI_COMM_COMPARE (creation of group sets)
 * MPI_COMM_SET_ERRHANDLER (reference count on errhandler)
 * MPI_COMM_CALL_ERRHANDLER (actually ok, but risk high, usage low)
 * MPI_FILE_CALL_ERRHANDLER (ditto)
 * MPI_WIN_CALL_ERRHANDLER (ditto)
 * MPI_ERROR_STRING (access to instance-specific string, which could
 *                   be overwritten by another thread)
 * MPI_FILE_SET_VIEW (setting view a big deal)
 * MPI_TYPE_COMMIT (could update description of type internally,
 *                  including creating a new representation.  Should
 *                  be ok, but, like call_errhandler, low usage)
 *
 * Note that other issues may force a routine to include the GLOBAL
 * critical section, such as debugging information that requires shared
 * state.  Such situations should be avoided where possible.
 */

typedef struct {
    MPL_thread_mutex_t mutex;
    MPL_thread_id_t owner;
    int count;
} MPIDU_Thread_mutex_t;
typedef MPL_thread_cond_t MPIDU_Thread_cond_t;

typedef MPL_thread_id_t MPIDU_Thread_id_t;
typedef MPL_thread_func_t MPIDU_Thread_func_t;

/*M MPIDU_THREAD_CS_ENTER - Enter a named critical section

  Input Parameters:
+ _name - name of the critical section
- _context - A context (typically an object) of the critical section

M*/

/*M MPIDU_THREAD_CS_EXIT - Exit a named critical section

  Input Parameters:
+ _name - name of the critical section
- _context - A context (typically an object) of the critical section

M*/

/*M MPIDU_THREAD_CS_YIELD - Temporarily release a critical section and yield
    to other threads

  Input Parameters:
+ _name - name of the critical section
- _context - A context (typically an object) of the critical section

M*/

/*M MPIDU_THREAD_ASSERT_IN_CS - Assert whether the code is inside a critical section

  Input Parameters:
+ _name - name of the critical section
- _context - A context (typically an object) of the critical section

M*/

#if defined(MPICH_IS_THREADED)
#define MPIDU_THREAD_CS_ENTER(name, ...) MPIDUI_THREAD_CS_ENTER_##name(__VA_ARGS__)
#define MPIDU_THREAD_CS_EXIT(name, ...) MPIDUI_THREAD_CS_EXIT_##name(__VA_ARGS__)
#define MPIDU_THREAD_CS_YIELD(name, ...) MPIDUI_THREAD_CS_YIELD_##name(__VA_ARGS__)
#define MPIDU_THREAD_ASSERT_IN_CS(name, ...) MPIDUI_THREAD_ASSERT_IN_CS_##name(__VA_ARGS__)

#else
#define MPIDU_THREAD_CS_ENTER(name, ...)        /* NOOP */
#define MPIDU_THREAD_CS_EXIT(name, ...) /* NOOP */
#define MPIDU_THREAD_CS_YIELD(name, ...)        /* NOOP */
#define MPIDU_THREAD_ASSERT_IN_CS(name, ...)    /* NOOP */

#endif

/* ***************************************** */
#if defined(MPICH_IS_THREADED)

#define MPIDUI_THREAD_CS_ENTER(mutex)                                   \
    do {                                                                \
        if (MPIR_ThreadInfo.isThreaded) {                               \
            int equal_ = 0;                                             \
            MPL_thread_id_t self_, owner_;                              \
            MPL_thread_self(&self_);                                    \
            owner_ = mutex.owner;                                       \
            MPL_thread_same(&self_, &owner_, &equal_);                  \
            if (!equal_) {                                              \
                int err_ = 0;                                           \
                MPL_DBG_MSG_P(MPIR_DBG_THREAD,VERBOSE,"enter MPIDU_Thread_mutex_lock %p", &mutex); \
                MPIDU_Thread_mutex_lock(&mutex, &err_, MPL_THREAD_PRIO_HIGH);\
                MPL_DBG_MSG_P(MPIR_DBG_THREAD,VERBOSE,"exit MPIDU_Thread_mutex_lock %p", &mutex); \
                MPIR_Assert(err_ == 0);                                 \
                MPIR_Assert(mutex.count == 0);                          \
                MPL_thread_self(&mutex.owner);                          \
            } else {                                                    \
                /* assert all recursive usage */                        \
                MPIR_Assert(0);                                         \
            }                                                           \
            mutex.count++;                                              \
        }                                                               \
    } while (0)

#define MPIDUI_THREAD_CS_EXIT(mutex)                                    \
    do {                                                                \
        if (MPIR_ThreadInfo.isThreaded) {                               \
            mutex.count--;                                              \
            MPIR_Assert(mutex.count >= 0);                              \
            if (mutex.count == 0) {                                     \
                mutex.owner = 0;                                        \
                int err_ = 0;                                           \
                MPL_DBG_MSG_P(MPIR_DBG_THREAD,VERBOSE,"MPIDU_Thread_mutex_unlock %p", &mutex); \
                MPIDU_Thread_mutex_unlock(&mutex, &err_);               \
                MPIR_Assert(err_ == 0);                                 \
            }                                                           \
        }                                                               \
    } while (0)

#define MPIDUI_THREAD_CS_YIELD(mutex)                                   \
    do {                                                                \
        if (MPIR_ThreadInfo.isThreaded) {                               \
            int err_ = 0, equal_ = 0;                                   \
            MPL_thread_id_t self_;                                      \
            MPL_thread_self(&self_);                                    \
            MPL_thread_same(&self_, &mutex.owner, &equal_);             \
            MPIR_Assert(equal_ && mutex.count > 0);                     \
            MPL_DBG_MSG_P(MPIR_DBG_THREAD,VERBOSE,"enter MPIDUI_THREAD_CS_YIELD %p", &mutex); \
            int saved_count_ = mutex.count;                             \
            MPL_thread_id_t saved_owner_ = mutex.owner;                 \
            MPIR_Assert(saved_count_ > 0);                              \
            mutex.count = 0;                                            \
            mutex.owner = 0;                                            \
            MPIDU_Thread_mutex_unlock(&mutex, &err_);                   \
            MPIR_Assert(err_ == 0);                                     \
            MPL_thread_yield();                                         \
            MPIDU_Thread_mutex_lock(&mutex, &err_, MPL_THREAD_PRIO_LOW);\
            MPIR_Assert(mutex.count == 0);                              \
            mutex.count = saved_count_;                                 \
            mutex.owner = saved_owner_;                                 \
            MPIR_Assert(err_ == 0);                                     \
            MPL_DBG_MSG_P(MPIR_DBG_THREAD,VERBOSE,"exit MPIDUI_THREAD_CS_YIELD %p", &mutex); \
            MPIR_Assert(err_ == 0);                                     \
        }                                                               \
    } while (0)

/* debug macros */

/* NOTE this macro is only available with VCI granularity */
#define MPIDUI_THREAD_ASSERT_IN_CS(mutex) \
    do { \
        if (MPIR_ThreadInfo.isThreaded) {  \
            int equal_ = 0;                                             \
            MPL_thread_id_t self_;                                      \
            MPL_thread_self(&self_);                                    \
            MPL_thread_same(&self_, &mutex.owner, &equal_);             \
            MPIR_Assert(equal_ && mutex.count >= 1); \
        } \
    } while (0)

/* MPICH_THREAD_GRANULARITY (set via `--enable-thread-cs=...`) activates one set of locks */

/* GLOBAL is only enabled with MPICH_THREAD_GRANULARITY__GLOBAL */
#if MPICH_THREAD_GRANULARITY == MPICH_THREAD_GRANULARITY__GLOBAL
#define MPIDUI_THREAD_CS_ENTER_GLOBAL(mutex)  MPIDUI_THREAD_CS_ENTER(mutex)
#define MPIDUI_THREAD_CS_EXIT_GLOBAL(mutex)   MPIDUI_THREAD_CS_EXIT(mutex)
#define MPIDUI_THREAD_CS_YIELD_GLOBAL(mutex)  MPIDUI_THREAD_CS_YIELD(mutex)
#else
#define MPIDUI_THREAD_CS_ENTER_GLOBAL(mutex)    /* NOOP */
#define MPIDUI_THREAD_CS_EXIT_GLOBAL(mutex)     /* NOOP */
#define MPIDUI_THREAD_CS_YIELD_GLOBAL(mutex)    /* NOOP */
#endif

/* VCI is only enabled with MPICH_THREAD_GRANULARITY__VCI */
#if MPICH_THREAD_GRANULARITY == MPICH_THREAD_GRANULARITY__VCI

#if defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)

#undef VCIEXP_LOCK_PTHREADS_COND_OR_FALSE
#if defined(VCIEXP_LOCK_PTHREADS)
#define VCIEXP_LOCK_PTHREADS_COND_OR_FALSE(cond) (cond)
#else
#define VCIEXP_LOCK_PTHREADS_COND_OR_FALSE(...) 0
#endif

void MPIDUI_Thread_cs_vci_check(MPIDU_Thread_mutex_t * p_mutex, int mutex_id, const char *mutex_str,
                                const char *function, const char *file, int line);

void MPIDUI_Thread_cs_vci_print(MPIDU_Thread_mutex_t * p_mutex, int mutex_id, const char *msg,
                                const char *mutex_str, const char *function, const char *file,
                                int line);

#if defined(VCIEXP_LOCK_ARGOBOTS)
/* Argobots-only data structures and functions. */
typedef struct MPIDUI_Thread_abt_tls_t {
    uint64_t vci_history;
    ABT_pool original_pool;
} MPIDUI_Thread_abt_tls_t;
ABT_pool MPIDUI_Thread_cs_get_target_pool(int mutex_id);

static inline void MPIDUI_Thread_cs_update_history(MPIDUI_Thread_abt_tls_t * p_tls, int mutex_id)
{
    /* Update the history. */
    p_tls->vci_history = (((uint64_t) (mutex_id)) | (p_tls->vci_history << (uint64_t) 8));
}

/* Return true if this ULT should migrate. */
static inline
    bool MPIDUI_Thread_cs_update_history_and_decide(MPIDUI_Thread_abt_tls_t * p_tls, int mutex_id)
{
    uint64_t vci_history = p_tls->vci_history;
    /* Update the history. */
    p_tls->vci_history = (((uint64_t) (mutex_id)) | (vci_history << (uint64_t) 8));
#define MPIDUI_THREAD_CS_CHECK_EQ_TMP(i) \
        (mutex_id == ((vci_history >> (uint64_t)(i * 8)) & (uint64_t)0xFF) ? 1 : 0)
    int count = MPIDUI_THREAD_CS_CHECK_EQ_TMP(0) + MPIDUI_THREAD_CS_CHECK_EQ_TMP(1)
        + MPIDUI_THREAD_CS_CHECK_EQ_TMP(2) + MPIDUI_THREAD_CS_CHECK_EQ_TMP(3)
        + MPIDUI_THREAD_CS_CHECK_EQ_TMP(4) + MPIDUI_THREAD_CS_CHECK_EQ_TMP(5)
        + MPIDUI_THREAD_CS_CHECK_EQ_TMP(6) + MPIDUI_THREAD_CS_CHECK_EQ_TMP(7);
#undef MPIDUI_THREAD_CS_CHECK_EQ_TMP
    /* Currently, if this thread is offloaded to the same thread N out of 8 times (including
     * this), migration happens. */
    const int N = 5;
    return count >= N;
}
#endif

static inline
    void MPIDUI_Thread_cs_enter_vci_impl(MPIDU_Thread_mutex_t * p_mutex, int mutex_id,
                                         bool recursive, int print_level, const char *mutex_str,
                                         const char *function, const char *file, int line)
{
    if (mutex_id <= 0 || VCIEXP_LOCK_PTHREADS_COND_OR_FALSE(!g_MPIU_exp_data.no_lock)) {
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, recursive ? "racquire" : "acquire",
                                           mutex_str, function, file, line);
        }
        if (recursive) {
            MPIDUI_THREAD_CS_ENTER_REC((*p_mutex));
        } else {
            MPIDUI_THREAD_CS_ENTER((*p_mutex));
        }
    } else {
#if defined(VCIEXP_LOCK_ARGOBOTS)
        /* Schedule this ULT on a certain execution stream. */
        MPL_thread_id_t owner_id;
        MPIDUI_Thread_abt_tls_t *p_tls =
            (MPIDUI_Thread_abt_tls_t *) MPL_thread_get_tls_ptr_and_self_fast(&owner_id);
        while (1) {
            if (!((((uint64_t) 1) << (uint64_t) (mutex_id - 1)) & l_MPIU_exp_data.vci_mask)) {
                /* Check if we should "migrate" this thread, not "offload". */
                if (MPIDUI_Thread_cs_update_history_and_decide(p_tls, mutex_id)) {
                    /* Migration should happen. */
                    if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
                        if (g_MPIU_exp_data.print_enabled >= print_level)
                            MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "resched-acquire-nomig",
                                                       mutex_str, function, file, line);
                    }
                } else {
                    /* Migration should not happen. */
                    if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
                        if (g_MPIU_exp_data.print_enabled >= print_level)
                            MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "resched-acquire-mig",
                                                       mutex_str, function, file, line);
                    }
                    p_tls->original_pool = ABTX_FAST_SELF_GET_ASSOCIATED_POOL();
                }
                ABT_pool target_pool = MPIDUI_Thread_cs_get_target_pool(mutex_id);
                int ret = ABTX_FAST_SET_ASSOCIATED_POOL_AND_YIELD(target_pool);
                MPIR_Assert(ret == ABT_SUCCESS);
            } else {
                /* This VCI operation can be done on this execution stream. */
                MPIDUI_Thread_cs_update_history(p_tls, mutex_id);
            }
            if (likely(p_mutex->count == 0)) {
                /* This thread becomes an owner. */
                p_mutex->owner = owner_id;
                p_mutex->count = 1;
                break;
            } else if (recursive) {
                /* If this thread is the owner, it's fine. */
                if (owner_id == p_mutex->owner) {
                    p_mutex->count++;
                    break;
                }
            }
            /* It seems that another ULT is taking this lock. */
            if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
                if (g_MPIU_exp_data.print_enabled >= print_level)
                    MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "retry-acquire", mutex_str,
                                               function, file, line);
            }
            /* Try again after yield. */
            int ret = ABT_self_yield();
            MPIR_Assert(ret == ABT_SUCCESS);
        }
        /* fallthrough */
#endif /* defined(VCIEXP_LOCK_ARGOBOTS) */
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "empty-acquire", mutex_str, function,
                                           file, line);
            MPIDUI_Thread_cs_vci_check(p_mutex, mutex_id, mutex_str, function, file, line);
        }
    }
}

static inline
    void MPIDUI_Thread_cs_enter_or_skip_vci_impl(MPIDU_Thread_mutex_t * p_mutex, int mutex_id,
                                                 int *p_skip, int print_level,
                                                 const char *mutex_str, const char *function,
                                                 const char *file, int line)
{
    if (mutex_id <= 0 || VCIEXP_LOCK_PTHREADS_COND_OR_FALSE(!g_MPIU_exp_data.no_lock)) {
        if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "acquire", mutex_str, function, file,
                                           line);
        }
        MPIDUI_THREAD_CS_ENTER((*p_mutex));
        *p_skip = 0;
    } else if ((((uint64_t) 1) << (uint64_t) (mutex_id - 1)) & l_MPIU_exp_data.vci_mask) {
#if defined(VCIEXP_LOCK_ARGOBOTS)
        /* Since multiple ULTs can be associated with a single execution stream, we need to check
         * the owner. */
        if (unlikely(p_mutex->count != 0)) {
            /* Someone has taken this lock (NOTE: this function does not take a recursive lock). */
            if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
                if (g_MPIU_exp_data.print_enabled >= print_level)
                    MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "skip-empty-acquire", mutex_str,
                                               function, file, line);
            }
            *p_skip = 1;
            return;
        } else {
            /* To take a lock, set owner and count. */
            p_mutex->count = 1;
            p_mutex->owner = MPL_thread_get_self_fast();
        }
        /* fallthrough */
#endif
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "acquire", mutex_str, function, file,
                                           line);
        }
        /* This VCI should be checked without lock. */
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "empty-acquire", mutex_str,
                                           function, file, line);
            MPIDUI_Thread_cs_vci_check(p_mutex, mutex_id, mutex_str, function, file, line);
        }
        *p_skip = 0;
    } else {
        /* This VCI is not associated with it. */
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "skip-acquire", mutex_str, function,
                                           file, line);
        }
        *p_skip = 1;
    }
}

static inline
    void MPIDUI_Thread_cs_exit_vci_impl(MPIDU_Thread_mutex_t * p_mutex, int mutex_id,
                                        int print_level, const char *mutex_str,
                                        const char *function, const char *file, int line)
{
    if (mutex_id <= 0 || VCIEXP_LOCK_PTHREADS_COND_OR_FALSE(!g_MPIU_exp_data.no_lock)) {
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "release", mutex_str, function, file,
                                           line);
        }
        MPIDUI_THREAD_CS_EXIT((*p_mutex));
    } else {
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "empty-release", mutex_str, function,
                                           file, line);
            MPIDUI_Thread_cs_vci_check(p_mutex, mutex_id, mutex_str, function, file, line);
        }
#if defined(VCIEXP_LOCK_ARGOBOTS)
        if (likely(p_mutex->count == 1)) {
            p_mutex->count = 0;
            MPIDUI_Thread_abt_tls_t *p_tls =
                (MPIDUI_Thread_abt_tls_t *) ABTX_FAST_SELF_GET_TLS_PTR();
            ABT_pool original_pool = p_tls->original_pool;
            if (original_pool) {
                /* This ULT was offloaded. Go back to the original pool. */
                p_tls->original_pool = NULL;
                int ret = ABTX_FAST_SET_ASSOCIATED_POOL_AND_YIELD(original_pool);
                MPIR_Assert(ret == ABT_SUCCESS);
            }
        } else {
            p_mutex->count -= 1;
        }
#endif
    }
}

static inline
    void MPIDUI_Thread_cs_yield_vci_impl(MPIDU_Thread_mutex_t * p_mutex, int mutex_id,
                                         int print_level, const char *mutex_str,
                                         const char *function, const char *file, int line)
{
    if (mutex_id <= 0 || VCIEXP_LOCK_PTHREADS_COND_OR_FALSE(!g_MPIU_exp_data.no_lock)) {
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "yield", mutex_str, function, file,
                                           line);
        }
        MPIDUI_THREAD_CS_YIELD((*p_mutex));
    } else {
        if (unlikely(g_MPIU_exp_data.debug_enabled)) {
            if (g_MPIU_exp_data.print_enabled >= print_level)
                MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "empty-yield", mutex_str, function,
                                           file, line);
            MPIDUI_Thread_cs_vci_check(p_mutex, mutex_id, mutex_str, function, file, line);
        }
#if defined(VCIEXP_LOCK_ARGOBOTS)
        if (p_mutex->count == 1) {
            MPL_thread_id_t self = p_mutex->owner;
            p_mutex->owner = 0;
            p_mutex->count = 0;
            /* This yield is not very desirable since there's no guarantee of progress while it is
             * yielding.  This routine should not be called very often. */
            ABT_pool target_pool = MPIDUI_Thread_cs_get_target_pool(mutex_id);
            while (1) {
                int ret = ABTX_FAST_SET_ASSOCIATED_POOL_AND_YIELD(target_pool);
                MPIR_Assert(ret == ABT_SUCCESS);
                if (p_mutex->owner == 0) {
                    p_mutex->owner = self;
                    p_mutex->count = 1;
                    break;
                } else {
                    if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
                        if (g_MPIU_exp_data.print_enabled >= print_level)
                            MPIDUI_Thread_cs_vci_print(p_mutex, mutex_id, "retry-yacquire",
                                                       mutex_str, function, file, line);
                    }
                }
            }
        } else {
            /* Yielding recursive lock does not make sense. */
            MPIR_Assert(0);
        }
        if (MPIDUI_THREAD_CHECK_ERROR && unlikely(g_MPIU_exp_data.debug_enabled)) {
            /* Check the VCI association again. */
            MPIDUI_Thread_cs_vci_check(p_mutex, mutex_id, mutex_str, function, file, line);
        }
#endif
    }
}

#define MPIDUI_THREAD_CS_ENTER_VCI(_mutex, _mutex_id) \
        MPIDUI_Thread_cs_enter_vci_impl(&(_mutex), _mutex_id, false, 1, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_CS_ENTER_OR_SKIP_VCI(_mutex, _mutex_id, _p_skip) \
        MPIDUI_Thread_cs_enter_or_skip_vci_impl(&(_mutex), _mutex_id, _p_skip, 1, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_CS_EXIT_VCI(_mutex, _mutex_id) \
        MPIDUI_Thread_cs_exit_vci_impl(&(_mutex), _mutex_id, 1, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_CS_YIELD_VCI(_mutex, _mutex_id) \
        MPIDUI_Thread_cs_yield_vci_impl(&(_mutex), _mutex_id, 1, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_CS_ENTER_VCI_NOPRINT(_mutex, _mutex_id) \
        MPIDUI_Thread_cs_enter_vci_impl(&(_mutex), _mutex_id, false, 2, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_CS_ENTER_OR_SKIP_VCI_NOPRINT(_mutex, _mutex_id, _p_skip) \
        MPIDUI_Thread_cs_enter_or_skip_vci_impl(&(_mutex), _mutex_id, _p_skip, 2, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_CS_EXIT_VCI_NOPRINT(_mutex, _mutex_id) \
        MPIDUI_Thread_cs_exit_vci_impl(&(_mutex), _mutex_id, 2, #_mutex, __FUNCTION__, __FILE__, __LINE__)
#define MPIDUI_THREAD_ASSERT_IN_CS_VCI(_mutex, _mutex_id) do {} while (0)       /* no-op */

#else /* !(defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)) */

#define MPIDUI_THREAD_CS_ENTER_VCI(mutex, mutex_id) MPIDUI_THREAD_CS_ENTER(mutex)
#define MPIDUI_THREAD_CS_ENTER_OR_SKIP_VCI(mutex, mutex_id, p_skip) \
    do {                                                            \
        MPIDUI_THREAD_CS_ENTER_VCI(mutex, mutex_id);                \
        *(p_skip) = 0;                                              \
    } while (0)
#define MPIDUI_THREAD_CS_EXIT_VCI(mutex, mutex_id) MPIDUI_THREAD_CS_EXIT(mutex)
#define MPIDUI_THREAD_CS_ENTER_VCI_NOPRINT(mutex, mutex_id) MPIDUI_THREAD_CS_ENTER(mutex)
#define MPIDUI_THREAD_CS_ENTER_OR_SKIP_VCI_NOPRINT(mutex, mutex_id, p_skip) \
    do {                                                            \
        MPIDUI_THREAD_CS_ENTER_VCI(mutex, mutex_id);                \
        *(p_skip) = 0;                                              \
    } while (0)
#define MPIDUI_THREAD_CS_EXIT_VCI_NOPRINT(mutex, mutex_id) MPIDUI_THREAD_CS_EXIT(mutex)
#define MPIDUI_THREAD_CS_YIELD_VCI(mutex, mutex_id) MPIDUI_THREAD_CS_YIELD(mutex)
#define MPIDUI_THREAD_ASSERT_IN_CS_VCI(mutex, mutex_id) MPIDUI_THREAD_ASSERT_IN_CS(mutex)

#endif /* !(defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)) */

#else
#define MPIDUI_THREAD_CS_ENTER_VCI(mutex, mutex_id)     /* NOOP */
#define MPIDUI_THREAD_CS_ENTER_OR_SKIP_VCI(mutex, mutex_id, p_skip)     \
    do {                                                                \
        *(p_skip) = 0;                                                  \
    } while (0)
#define MPIDUI_THREAD_CS_EXIT_VCI(mutex, mutex_id)      /* NOOP */
#define MPIDUI_THREAD_CS_ENTER_VCI_NOPRINT(mutex, mutex_id)     /* NOOP */
#define MPIDUI_THREAD_CS_EXIT_VCI_NOPRINT(mutex, mutex_id)      /* NOOP */
#define MPIDUI_THREAD_CS_ENTER_OR_SKIP_VCI_NOPRINT(mutex, mutex_id, p_skip) \
    do {                                                                    \
        *(p_skip) = 0;                                                      \
    } while (0)
#define MPIDUI_THREAD_CS_YIELD_VCI(mutex, mutex_id)     /* NOOP */
#define MPIDUI_THREAD_ASSERT_IN_CS_VCI(mutex, mutex_id) /* NOOP */
#endif

#endif /* MPICH_IS_THREADED */

/* ***************************************** */
#define MPIDU_Thread_init         MPL_thread_init
#define MPIDU_Thread_finalize     MPL_thread_finalize
#define MPIDU_Thread_create       MPL_thread_create
#define MPIDU_Thread_exit         MPL_thread_exit
#define MPIDU_Thread_self         MPL_thread_self
#define MPIDU_Thread_join       MPL_thread_join
#define MPIDU_Thread_same       MPL_thread_same

#define MPIDU_Thread_yield() \
    do { \
        if (MPIR_CVAR_ENABLE_HEAVY_YIELD) { \
            /* note: sleep time may be rounded up to the granularity of the underlying clock */ \
            struct timespec t; \
            t.tv_sec = 0; \
            t.tv_nsec = 1; \
            nanosleep(&t, NULL); \
        } else { \
            MPL_thread_yield(); \
        } \
    } while (0)


/*
 *    Mutexes
 */

/*@
  MPIDU_Thread_mutex_create - create a new mutex

  Output Parameters:
+ mutex - mutex
- err - error code (non-zero indicates an error has occurred)
@*/
#define MPIDU_Thread_mutex_create(mutex_ptr_, err_ptr_)                 \
    do {                                                                \
        (mutex_ptr_)->owner = 0;                                        \
        (mutex_ptr_)->count = 0;                                        \
        MPL_thread_mutex_create(&(mutex_ptr_)->mutex, err_ptr_);     \
        MPL_DBG_MSG_P(MPIR_DBG_THREAD,TYPICAL,"Created MPL_thread_mutex %p", (mutex_ptr_)); \
    } while (0)

/*@
  MPIDU_Thread_mutex_destroy - destroy an existing mutex

  Input Parameter:
. mutex - mutex

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
#define MPIDU_Thread_mutex_destroy(mutex_ptr_, err_ptr_)                \
    do {                                                                \
        MPL_DBG_MSG_P(MPIR_DBG_THREAD,TYPICAL,"About to destroy MPL_thread_mutex %p", (mutex_ptr_)); \
        MPL_thread_mutex_destroy(&(mutex_ptr_)->mutex, err_ptr_);       \
    } while (0)

/*@
  MPIDU_Thread_lock - acquire a mutex

  Input Parameter:
. mutex - mutex
@*/
#define MPIDU_Thread_mutex_lock(mutex_ptr_, err_ptr_, prio_)            \
    do {                                                                \
        MPL_thread_mutex_lock(&(mutex_ptr_)->mutex, err_ptr_, prio_);\
        MPIR_Assert(*err_ptr_ == 0);                                    \
    } while (0)

/*@
  MPIDU_Thread_unlock - release a mutex

  Input Parameter:
. mutex - mutex
@*/
#define MPIDU_Thread_mutex_unlock(mutex_ptr_, err_ptr_)                 \
    do {                                                                \
        MPL_thread_mutex_unlock(&(mutex_ptr_)->mutex, err_ptr_);        \
        MPIR_Assert(*err_ptr_ == 0);                                    \
    } while (0)

/*
 * Condition Variables
 */

/*@
  MPIDU_Thread_cond_create - create a new condition variable

  Output Parameters:
+ cond - condition variable
- err - location to store the error code; pointer may be NULL; error is zero for success, non-zero if a failure occurred
@*/
#define MPIDU_Thread_cond_create(cond_ptr_, err_ptr_)                   \
    do {                                                                \
        MPL_thread_cond_create(cond_ptr_, err_ptr_);                    \
        MPIR_Assert(*err_ptr_ == 0);                                    \
        MPL_DBG_MSG_P(MPIR_DBG_THREAD,TYPICAL,"Created MPL_thread_cond %p", (cond_ptr_)); \
    } while (0)

/*@
  MPIDU_Thread_cond_destroy - destroy an existinga condition variable

  Input Parameter:
. cond - condition variable

  Output Parameter:
. err - location to store the error code; pointer may be NULL; error is zero
        for success, non-zero if a failure occurred
@*/
#define MPIDU_Thread_cond_destroy(cond_ptr_, err_ptr_)                  \
    do {                                                                \
        MPL_DBG_MSG_P(MPIR_DBG_THREAD,TYPICAL,"About to destroy MPL_thread_cond %p", (cond_ptr_)); \
        MPL_thread_cond_destroy(cond_ptr_, err_ptr_);                   \
        MPIR_Assert(*err_ptr_ == 0);                                    \
    } while (0)

/*@
  MPIDU_Thread_cond_wait - wait (block) on a condition variable

  Input Parameters:
+ cond - condition variable
- mutex - mutex

  Notes:
  This function may return even though another thread has not requested that a
  thread be released.  Therefore, the calling
  program must wrap the function in a while loop that verifies program state
  has changed in a way that warrants letting the
  thread proceed.
@*/
#define MPIDU_Thread_cond_wait(cond_ptr_, mutex_ptr_, err_ptr_)         \
    do {                                                                \
        int saved_count_ = (mutex_ptr_)->count;                         \
        MPL_thread_id_t saved_owner_ = (mutex_ptr_)->owner;              \
        (mutex_ptr_)->count = 0;                                        \
        (mutex_ptr_)->owner = 0;                                        \
        MPL_DBG_MSG_FMT(MPIR_DBG_THREAD,TYPICAL,(MPL_DBG_FDEST,"Enter cond_wait on cond=%p mutex=%p",(cond_ptr_),&(mutex_ptr_)->mutex)); \
        MPL_thread_cond_wait(cond_ptr_, &(mutex_ptr_)->mutex, err_ptr_); \
        MPIR_Assert_fmt_msg(*((int *) err_ptr_) == 0,                   \
                            ("cond_wait failed, err=%d (%s)", *((int *) err_ptr_), strerror(*((int *) err_ptr_)))); \
        MPL_DBG_MSG_FMT(MPIR_DBG_THREAD,TYPICAL,(MPL_DBG_FDEST,"Exit cond_wait on cond=%p mutex=%p",(cond_ptr_),&(mutex_ptr_)->mutex)); \
        (mutex_ptr_)->count = saved_count_;                             \
        (mutex_ptr_)->owner = saved_owner_;                             \
    } while (0)

/*@
  MPIDU_Thread_cond_broadcast - release all threads currently waiting on a condition variable

  Input Parameter:
. cond - condition variable
@*/
#define MPIDU_Thread_cond_broadcast(cond_ptr_, err_ptr_)                \
    do {                                                                \
        MPL_DBG_MSG_P(MPIR_DBG_THREAD,TYPICAL,"About to cond_broadcast on MPL_thread_cond %p", (cond_ptr_)); \
        MPL_thread_cond_broadcast(cond_ptr_, err_ptr_);                 \
        MPIR_Assert_fmt_msg(*((int *) err_ptr_) == 0,                   \
                            ("cond_broadcast failed, err=%d (%s)", *((int *) err_ptr_), strerror(*((int *) err_ptr_)))); \
    } while (0)

/*@
  MPIDU_Thread_cond_signal - release one thread currently waitng on a condition variable

  Input Parameter:
. cond - condition variable
@*/
#define MPIDU_Thread_cond_signal(cond_ptr_, err_ptr_)                   \
    do {                                                                \
        MPL_DBG_MSG_P(MPIR_DBG_THREAD,TYPICAL,"About to cond_signal on MPL_thread_cond %p", (cond_ptr_)); \
        MPL_thread_cond_signal(cond_ptr_, err_ptr_);                    \
        MPIR_Assert_fmt_msg(*((int *) err_ptr_) == 0,                   \
                            ("cond_signal failed, err=%d (%s)", *((int *) err_ptr_), strerror(*((int *) err_ptr_)))); \
    } while (0)

#endif /* MPIDU_THREAD_FALLBACK_H_INCLUDED */
