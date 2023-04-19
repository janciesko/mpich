/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

/* common header includes */
#include "mpl.h"

MPL_SUPPRESS_OSX_HAS_NO_SYMBOLS_WARNING;

#if defined(MPL_THREAD_PACKAGE_NAME) && (MPL_THREAD_PACKAGE_NAME == MPL_THREAD_PACKAGE_QTHREADS)

typedef void (*MPL_thread_func_t) (void *data);

typedef struct {
    MPL_thread_func_t func;
    void *data;
} thread_info;

static void thread_start(void *arg)
{
    thread_info *info = (thread_info *) arg;
    MPL_thread_func_t func = info->func;
    void *data = info->data;

    free(arg);

    func(data);
}

static int thread_create(MPL_thread_func_t func, void *data, MPL_thread_id_t * idp)
{
    if (qthread_initialize() != QTHREAD_SUCCESS) {
        /* Not initialized yet. */
        return MPL_ERR_THREAD;
    }
    int err = qthread_fork(func, data, NULL); /* fix missing update of idp */
    return (err == QTHREAD_SUCCESS) ? MPL_SUCCESS : MPL_ERR_THREAD;
}

/*
 * MPL_thread_create()
 */
void MPL_thread_create(MPL_thread_func_t func, void *data, MPL_thread_id_t * idp, int *errp)
{
    int err = thread_create(func, data, idp);

    if (errp != NULL) {
        *errp = err;
    }
}

#endif
