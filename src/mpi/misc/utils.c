/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "mpiimpl.h"
#include "datatype.h"

#define COPY_BUFFER_SZ 16384

/* localcopy_kind */
enum {
    LOCALCOPY_BLOCKING,
    LOCALCOPY_NONBLOCKING,
    LOCALCOPY_STREAM,
};

static int do_localcopy(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                        void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype,
                        int localcopy_kind, void *extra_param)
{
    int mpi_errno = MPI_SUCCESS;
    int sendtype_iscontig, recvtype_iscontig;
    MPI_Aint sendsize, recvsize, sdata_sz, rdata_sz, copy_sz;
    MPI_Aint true_extent, sendtype_true_lb, recvtype_true_lb;
    char *buf = NULL;
    MPL_pointer_attr_t send_attr, recv_attr;
    MPIR_CHKLMEM_DECL(1);

    MPIR_FUNC_ENTER;

    MPIR_Datatype_get_size_macro(sendtype, sendsize);
    MPIR_Datatype_get_size_macro(recvtype, recvsize);

    sdata_sz = sendsize * sendcount;
    rdata_sz = recvsize * recvcount;

    send_attr.type = recv_attr.type = MPL_GPU_POINTER_UNREGISTERED_HOST;

    /* if there is no data to copy, bail out */
    if (!sdata_sz || !rdata_sz)
        goto fn_exit;

#if defined(HAVE_ERROR_CHECKING)
    if (sdata_sz > rdata_sz) {
        MPIR_ERR_SET2(mpi_errno, MPI_ERR_TRUNCATE, "**truncate", "**truncate %d %d", sdata_sz,
                      rdata_sz);
        copy_sz = rdata_sz;
    } else
#endif /* HAVE_ERROR_CHECKING */
        copy_sz = sdata_sz;

    /* Builtin types is the common case; optimize for it */
    MPIR_Datatype_is_contig(sendtype, &sendtype_iscontig);
    MPIR_Datatype_is_contig(recvtype, &recvtype_iscontig);

    MPIR_Type_get_true_extent_impl(sendtype, &sendtype_true_lb, &true_extent);
    MPIR_Type_get_true_extent_impl(recvtype, &recvtype_true_lb, &true_extent);

    /* NOTE: actual_unpack_bytes is a local variable. It works because yaksa
     *       updates it at issuing time regardless of nonblocking or stream.
     */
    if (sendtype_iscontig) {
        MPI_Aint actual_unpack_bytes;
        const void *bufptr = MPIR_get_contig_ptr(sendbuf, sendtype_true_lb);
        if (localcopy_kind == LOCALCOPY_NONBLOCKING) {
            MPIR_Typerep_req *typerep_req = extra_param;
            MPIR_Typerep_iunpack(bufptr, copy_sz, recvbuf, recvcount, recvtype, 0,
                                 &actual_unpack_bytes, typerep_req, MPIR_TYPEREP_FLAG_NONE);
        } else if (localcopy_kind == LOCALCOPY_STREAM) {
            void *stream = extra_param;
            MPIR_Typerep_unpack_stream(bufptr, copy_sz, recvbuf, recvcount, recvtype, 0,
                                       &actual_unpack_bytes, stream);
        } else {
            /* LOCALCOPY_BLOCKING */
            MPIR_Typerep_unpack(bufptr, copy_sz, recvbuf, recvcount, recvtype, 0,
                                &actual_unpack_bytes, MPIR_TYPEREP_FLAG_NONE);
        }
        MPIR_ERR_CHKANDJUMP(actual_unpack_bytes != copy_sz, mpi_errno, MPI_ERR_TYPE,
                            "**dtypemismatch");
    } else if (recvtype_iscontig) {
        void *bufptr = MPIR_get_contig_ptr(recvbuf, recvtype_true_lb);
        MPI_Aint actual_pack_bytes;
        if (localcopy_kind == LOCALCOPY_NONBLOCKING) {
            MPIR_Typerep_req *typerep_req = extra_param;
            MPIR_Typerep_ipack(sendbuf, sendcount, sendtype, 0, bufptr, copy_sz,
                               &actual_pack_bytes, typerep_req, MPIR_TYPEREP_FLAG_NONE);
        } else if (localcopy_kind == LOCALCOPY_STREAM) {
            void *stream = extra_param;
            MPIR_Typerep_pack_stream(sendbuf, sendcount, sendtype, 0, bufptr, copy_sz,
                                     &actual_pack_bytes, stream);
        } else {
            /* LOCALCOPY_BLOCKING */
            MPIR_Typerep_pack(sendbuf, sendcount, sendtype, 0, bufptr, copy_sz,
                              &actual_pack_bytes, MPIR_TYPEREP_FLAG_NONE);
        }
        MPIR_ERR_CHKANDJUMP(actual_pack_bytes != copy_sz, mpi_errno, MPI_ERR_TYPE,
                            "**dtypemismatch");
    } else {
        /* Non-contig to non-contig, we allocate a temp buffer of COPY_BUFFER_SZ,
         * unpack to the temp buffer followed with pack to recv buffer.
         *
         * Use blocking version for nonblocking kind, since it is less worth of
         * optimization.
         */

        /* non-contig to non-contig stream enqueue is not supported. */
        MPIR_Assert(localcopy_kind != LOCALCOPY_STREAM);

        intptr_t sfirst;
        intptr_t rfirst;

        MPIR_GPU_query_pointer_attr(sendbuf, &send_attr);
        MPIR_GPU_query_pointer_attr(recvbuf, &recv_attr);

        if (send_attr.type == MPL_GPU_POINTER_DEV && recv_attr.type == MPL_GPU_POINTER_DEV) {
            MPL_gpu_malloc((void **) &buf, COPY_BUFFER_SZ, recv_attr.device);
        } else if (send_attr.type == MPL_GPU_POINTER_DEV || recv_attr.type == MPL_GPU_POINTER_DEV) {
            MPL_gpu_malloc_host((void **) &buf, COPY_BUFFER_SZ);
        } else {
            MPIR_CHKLMEM_MALLOC(buf, char *, COPY_BUFFER_SZ, mpi_errno, "buf", MPL_MEM_BUFFER);
        }

        sfirst = 0;
        rfirst = 0;

        while (1) {
            MPI_Aint max_pack_bytes;
            if (copy_sz - sfirst > COPY_BUFFER_SZ) {
                max_pack_bytes = COPY_BUFFER_SZ;
            } else {
                max_pack_bytes = copy_sz - sfirst;
            }

            MPI_Aint actual_pack_bytes;
            MPIR_Typerep_pack(sendbuf, sendcount, sendtype, sfirst, buf,
                              max_pack_bytes, &actual_pack_bytes, MPIR_TYPEREP_FLAG_NONE);
            MPIR_Assert(actual_pack_bytes > 0);

            sfirst += actual_pack_bytes;

            MPI_Aint actual_unpack_bytes;
            MPIR_Typerep_unpack(buf, actual_pack_bytes, recvbuf, recvcount, recvtype,
                                rfirst, &actual_unpack_bytes, MPIR_TYPEREP_FLAG_NONE);
            MPIR_Assert(actual_unpack_bytes > 0);

            rfirst += actual_unpack_bytes;

            /* everything that was packed from the source type must be
             * unpacked; otherwise we will lose the remaining data in
             * buf in the next iteration. */
            MPIR_ERR_CHKANDJUMP(actual_pack_bytes != actual_unpack_bytes, mpi_errno,
                                MPI_ERR_TYPE, "**dtypemismatch");

            if (rfirst == copy_sz) {
                /* successful completion */
                break;
            }
        }

        if (send_attr.type == MPL_GPU_POINTER_DEV && recv_attr.type == MPL_GPU_POINTER_DEV) {
            MPL_gpu_free(buf);
        } else if (send_attr.type == MPL_GPU_POINTER_DEV || recv_attr.type == MPL_GPU_POINTER_DEV) {
            MPL_gpu_free_host(buf);
        }
    }

  fn_exit:
    MPIR_CHKLMEM_FREEALL();
    MPIR_FUNC_EXIT;
    return mpi_errno;
  fn_fail:
    if (buf) {
        if (send_attr.type == MPL_GPU_POINTER_DEV && recv_attr.type == MPL_GPU_POINTER_DEV) {
            MPL_gpu_free(buf);
        } else if (send_attr.type == MPL_GPU_POINTER_DEV || recv_attr.type == MPL_GPU_POINTER_DEV) {
            MPL_gpu_free_host(buf);
        }
    }
    goto fn_exit;
}

#ifdef MPL_HAVE_GPU
static int do_localcopy_gpu(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                            MPL_pointer_attr_t * send_attr, void *recvbuf, MPI_Aint recvcount,
                            MPI_Datatype recvtype, MPL_pointer_attr_t * recv_attr,
                            MPL_gpu_copy_direction_t dir, MPL_gpu_engine_type_t enginetype,
                            bool commit, MPIR_Typerep_req * typerep_req)
{
    int mpi_errno = MPI_SUCCESS;
    int mpl_errno = MPL_SUCCESS;
    int sendtype_iscontig, recvtype_iscontig;
    MPI_Aint sendsize, recvsize, sdata_sz, rdata_sz, copy_sz;
    MPI_Aint true_extent, sendtype_true_lb, recvtype_true_lb;
    MPL_gpu_request gpu_req;
    int completed = 0;
    int dev_id = -1;

    MPIR_FUNC_ENTER;

    if (typerep_req)
        *typerep_req = MPIR_TYPEREP_REQ_NULL;

    MPIR_Datatype_get_size_macro(sendtype, sendsize);
    MPIR_Datatype_get_size_macro(recvtype, recvsize);

    sdata_sz = sendsize * sendcount;
    rdata_sz = recvsize * recvcount;

    /* if there is no data to copy, bail out */
    if (!sdata_sz || !rdata_sz)
        goto fn_exit;

#if defined(HAVE_ERROR_CHECKING)
    if (sdata_sz > rdata_sz) {
        MPIR_ERR_SET2(mpi_errno, MPI_ERR_TRUNCATE, "**truncate", "**truncate %d %d", sdata_sz,
                      rdata_sz);
        copy_sz = rdata_sz;
    } else
#endif /* HAVE_ERROR_CHECKING */
        copy_sz = sdata_sz;

    /* This case is specific for contig datatypes */
    MPIR_Datatype_iscontig(sendtype, &sendtype_iscontig);
    MPIR_Datatype_iscontig(recvtype, &recvtype_iscontig);

    MPIR_Type_get_true_extent_impl(sendtype, &sendtype_true_lb, &true_extent);
    MPIR_Type_get_true_extent_impl(recvtype, &recvtype_true_lb, &true_extent);

    if (sendtype_iscontig && recvtype_iscontig) {
        /* Remove guard when other backends implement MPL_gpu_imemcpy and MPL_gpu_fast_memcpy */
#ifdef MPL_HAVE_ZE
        if (copy_sz <= MPIR_CVAR_CH4_IPC_GPU_FAST_COPY_MAX_SIZE) {
            mpl_errno = MPL_gpu_fast_memcpy(MPIR_get_contig_ptr(sendbuf, sendtype_true_lb),
                                            send_attr, MPIR_get_contig_ptr(recvbuf,
                                                                           recvtype_true_lb),
                                            recv_attr, copy_sz);
            MPIR_ERR_CHKANDJUMP(mpl_errno != MPL_SUCCESS, mpi_errno, MPI_ERR_OTHER,
                                "**mpl_gpu_fast_memcpy");
        } else {
            if (send_attr == NULL) {
                MPL_pointer_attr_t sendattr;
                MPIR_GPU_query_pointer_attr(sendbuf, &sendattr);
                if (sendattr.type == MPL_GPU_POINTER_DEV) {
                    dev_id = MPL_gpu_get_dev_id_from_attr(&sendattr);
                }
            } else if (send_attr->type == MPL_GPU_POINTER_DEV) {
                dev_id = MPL_gpu_get_dev_id_from_attr(send_attr);
            }

            if (dev_id == -1) {
                if (recv_attr == NULL) {
                    MPL_pointer_attr_t recvattr;
                    MPIR_GPU_query_pointer_attr(recvbuf, &recvattr);
                    if (recvattr.type == MPL_GPU_POINTER_DEV) {
                        dev_id = MPL_gpu_get_dev_id_from_attr(&recvattr);
                    } else {
                        goto fn_fallback;
                    }
                } else if (recv_attr->type == MPL_GPU_POINTER_DEV) {
                    dev_id = MPL_gpu_get_dev_id_from_attr(recv_attr);
                } else {
                    /* fallback to do_localcopy */
                    goto fn_fallback;
                }
            }
            MPIR_ERR_CHKANDJUMP(dev_id == -1, mpi_errno, MPI_ERR_OTHER,
                                "**mpl_gpu_get_dev_id_from_attr");

            mpl_errno = MPL_gpu_imemcpy(MPIR_get_contig_ptr(recvbuf, recvtype_true_lb),
                                        MPIR_get_contig_ptr(sendbuf, sendtype_true_lb), copy_sz,
                                        dev_id, dir, enginetype, &gpu_req, commit);
            MPIR_ERR_CHKANDJUMP(mpl_errno != MPL_SUCCESS, mpi_errno, MPI_ERR_OTHER,
                                "**mpl_gpu_imemcpy");

            while (!completed) {
                mpl_errno = MPL_gpu_test(&gpu_req, &completed);
                MPIR_ERR_CHKANDJUMP(mpl_errno != MPL_SUCCESS, mpi_errno, MPI_ERR_OTHER,
                                    "**mpl_gpu_test");
            }
        }
#else
        /* fallback to do_localcopy */
        goto fn_fallback;
#endif
    } else {
        /* fallback to do_localcopy */
        goto fn_fallback;
    }

  fn_exit:
    MPIR_FUNC_EXIT;
    return mpi_errno;
  fn_fail:
    goto fn_exit;
  fn_fallback:
    mpi_errno =
        do_localcopy(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                     LOCALCOPY_BLOCKING, NULL);
    MPIR_ERR_CHECK(mpi_errno);
    goto fn_exit;
}
#endif

int MPIR_Localcopy(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                   void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype)
{
    int mpi_errno = MPI_SUCCESS;

    MPIR_FUNC_ENTER;

    mpi_errno = do_localcopy(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                             LOCALCOPY_BLOCKING, NULL);
    MPIR_ERR_CHECK(mpi_errno);

  fn_exit:
    MPIR_FUNC_EXIT;
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int MPIR_Ilocalcopy(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                    void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype,
                    MPIR_Typerep_req * typerep_req)
{
    int mpi_errno = MPI_SUCCESS;

    MPIR_FUNC_ENTER;

    mpi_errno = do_localcopy(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                             LOCALCOPY_NONBLOCKING, typerep_req);
    MPIR_ERR_CHECK(mpi_errno);

  fn_exit:
    MPIR_FUNC_EXIT;
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

/* Only single chunk copy (either sendtype or recvtype is contig) works with do_localcopy.
 * For noncontig to noncontig, the copy consists of pacc/unpack to/from an intermediary
 * temporary buffer. Since MPIR_Typerep_(un)pack_stream cannot handle host-host copy, non-
 * contig local copy needs to be separately handled by caller.
 */
int MPIR_Localcopy_stream(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                          void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype, void *stream)
{
    int mpi_errno = MPI_SUCCESS;

    MPIR_FUNC_ENTER;

    mpi_errno = do_localcopy(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                             LOCALCOPY_STREAM, stream);
    MPIR_ERR_CHECK(mpi_errno);

  fn_exit:
    MPIR_FUNC_EXIT;
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

int MPIR_Localcopy_gpu(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                       MPL_pointer_attr_t * sendattr, void *recvbuf, MPI_Aint recvcount,
                       MPI_Datatype recvtype, MPL_pointer_attr_t * recvattr,
                       MPL_gpu_copy_direction_t dir, MPL_gpu_engine_type_t enginetype, bool commit)
{
    int mpi_errno = MPI_SUCCESS;

    MPIR_FUNC_ENTER;

#ifdef MPL_HAVE_GPU
    mpi_errno = do_localcopy_gpu(sendbuf, sendcount, sendtype, sendattr, recvbuf, recvcount,
                                 recvtype, recvattr, dir, enginetype, commit, NULL);
    MPIR_ERR_CHECK(mpi_errno);
#else
    mpi_errno =
        do_localcopy(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, LOCALCOPY_BLOCKING,
                     NULL);
    MPIR_ERR_CHECK(mpi_errno);
#endif

  fn_exit:
    MPIR_FUNC_EXIT;
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

#if defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)

MPIU_exp_data_t g_MPIU_exp_data = {
    "", /* dummy1 */
    0,  /* debug_enabled */
    -1, /* print_rank */
    0,  /* print_enabled */
#if defined(VCIEXP_LOCK_PTHREADS)
    0,  /* no_lock */
#endif
    ""  /* dummy2 */
};

__thread MPIU_exp_data_tls_t l_MPIU_exp_data = {
    "", /* dummy1 */
    0,  /* vci_mask */
#if defined(VCIEXP_LOCK_PTHREADS)
    -1, /* local_tid */
#endif
    ""  /* dummy2 */
};

void MPIDUI_Thread_cs_vci_check(MPIDU_Thread_mutex_t * p_mutex, int mutex_id, const char *mutex_str,
                                const char *function, const char *file, int line)
{
    if (mutex_id <= 0) {
        /* It's okay. */
        return;
    } else {
        /* Check the mask. */
        if (mutex_id > MPIDI_global.n_vcis) {
            int tid = -1;
#if defined(VCIEXP_LOCK_PTHREADS)
            tid = l_MPIU_exp_data.local_tid;
#elif defined(VCIEXP_LOCK_ARGOBOTS)
            int rank;
            if (ABT_self_get_xstream_rank(&rank) == ABT_SUCCESS)
                tid = rank;
#endif
            printf("[%2d:%2d] invalid mutex_id: %d (%s in %s() %s:%d)\n",
                   g_MPIU_exp_data.print_rank, tid, mutex_id, mutex_str, function, file, line);
            fflush(0);
            MPIR_Assert(0);
        }
        if ((1 << mutex_id) & l_MPIU_exp_data.vci_mask) {
            /* It's okay, but check a lock value just in case. */
            MPIDU_Thread_mutex_t *p_vci_lock = &MPIDI_global.vci[mutex_id].vci.lock;
            if (p_mutex != p_vci_lock) {
                int tid = -1;
#if defined(VCIEXP_LOCK_PTHREADS)
                tid = l_MPIU_exp_data.local_tid;
#elif defined(VCIEXP_LOCK_ARGOBOTS)
                int rank;
                if (ABT_self_get_xstream_rank(&rank) == ABT_SUCCESS)
                    tid = rank;
#endif
                printf("[%2d:%2d] invalid mutex_id: %d, %p vs %p (%s in %s() %s:%d)\n",
                       g_MPIU_exp_data.print_rank, tid, mutex_id, (void *) p_mutex,
                       (void *) p_vci_lock, mutex_str, function, file, line);
                fflush(0);
                MPIR_Assert(0);
            }
            return;
        } else {
            /* Not okay. Error. */
            int tid = -1;
#if defined(VCIEXP_LOCK_PTHREADS)
            tid = l_MPIU_exp_data.local_tid;
#elif defined(VCIEXP_LOCK_ARGOBOTS)
            int rank;
            if (ABT_self_get_xstream_rank(&rank) == ABT_SUCCESS)
                tid = rank;
#endif
            printf("[%2d:%2d] invalid mutex_id: %d (mask: %d, %s in %s() %s:%d)\n",
                   g_MPIU_exp_data.print_rank, tid, mutex_id, l_MPIU_exp_data.vci_mask,
                   mutex_str, function, file, line);
            fflush(0);
            MPIR_Assert(0);
        }
    }
}

void MPIDUI_Thread_cs_vci_print(MPIDU_Thread_mutex_t * p_mutex, int mutex_id, const char *msg,
                                const char *mutex_str, const char *function, const char *file,
                                int line)
{
    int tid = -1;
    int nolock = -1;
#if defined(VCIEXP_LOCK_PTHREADS)
    tid = l_MPIU_exp_data.local_tid;
    nolock = g_MPIU_exp_data.no_lock;
#elif defined(VCIEXP_LOCK_ARGOBOTS)
    int rank;
    if (ABT_self_get_xstream_rank(&rank) == ABT_SUCCESS)
        tid = rank;
#endif
    printf("[%2d:%2d] %s %s (id = %d) (%s() %s:%d, nolock = %d, mask = %d)\n",
           g_MPIU_exp_data.print_rank, tid, msg, mutex_str, mutex_id, function, file, line, nolock,
           l_MPIU_exp_data.vci_mask);
    fflush(0);
}

#if defined(VCIEXP_LOCK_ARGOBOTS)
#define MAX_XSTREAMS 256
typedef struct {
    char dummy1[64];
    ABT_pool vci_pools[MPIDI_CH4_MAX_VCIS];     /* private pools. */
    char dummy2[64];
} abt_data_t;

static abt_data_t g_abt_data;

static void update_vci_mask(int vci_mask)
{
    int vci, ret;
    ABT_pool mypool;
    ABT_xstream xstream;

    ret = ABT_self_get_xstream(&xstream);
    MPIR_Assert(ret == ABT_SUCCESS);
    {
        ABT_pool tmp_pools[2];
        ret = ABT_xstream_get_main_pools(xstream, 2, tmp_pools);
        MPIR_Assert(ret == ABT_SUCCESS);
        /* The second pool is what we want. */
        mypool = tmp_pools[1];
    }
    /* Remove the previous setting. */
    for (vci = 0; vci < MPIDI_CH4_MAX_VCIS; vci++) {
        if (g_abt_data.vci_pools[vci] == mypool)
            g_abt_data.vci_pools[vci] = NULL;
    }
    /* Update this setting. */
    for (vci = 0; vci < MPIDI_CH4_MAX_VCIS; vci++) {
        if ((1 << vci) & vci_mask) {
            /* This vci is corresponding to this ES. */
            g_abt_data.vci_pools[vci] = mypool;
        }
    }
}

ABT_pool MPIDUI_Thread_cs_get_target_pool(int mutex_id)
{
    ABT_pool pool = g_abt_data.vci_pools[mutex_id];
    if (pool == NULL) {
        printf("mutex_id = %d\n", mutex_id);
        MPIR_Assert(pool != NULL);
    }
    return pool;
}

#endif

int MPIX_Set_exp_info(int info_type, void *val1, int val2)
{
    if (info_type == MPIX_INFO_TYPE_PRINT_RANK) {
        g_MPIU_exp_data.print_rank = val2;
    } else if (info_type == MPIX_INFO_TYPE_LOCAL_TID) {
#if defined(VCIEXP_LOCK_PTHREADS)
        l_MPIU_exp_data.local_tid = val2;
#else
        if (g_MPIU_exp_data.print_enabled) {
            printf("MPIX_INFO_TYPE_LOCAL_TID: %d is ignored.\n", val2);
        }
#endif
    } else if (info_type == MPIX_INFO_TYPE_DEBUG_ENABLED) {
        g_MPIU_exp_data.debug_enabled = val2;
    } else if (info_type == MPIX_INFO_TYPE_PRINT_ENABLED) {
        g_MPIU_exp_data.print_enabled = val2;
    } else if (info_type == MPIX_INFO_TYPE_NOLOCK) {
#if defined(VCIEXP_LOCK_PTHREADS)
        g_MPIU_exp_data.no_lock = val2;
#else
        if (g_MPIU_exp_data.print_enabled) {
            printf("MPIX_INFO_TYPE_NOLOCK: %d is ignored.\n", val2);
        }
#endif
    } else if (info_type == MPIX_INFO_TYPE_VCIMASK) {
        l_MPIU_exp_data.vci_mask = val2;
#if defined(VCIEXP_LOCK_ARGOBOTS)
        update_vci_mask(val2);
#endif
    }
}

#else /* !(defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)) */

int MPIX_Set_exp_info(int info_type, void *val1, int val2)
{
    /* Ignored. */
}

#endif /* defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS) */
