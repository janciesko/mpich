/*
 * Copyright (C) by Argonne National Laboratory
 *     See COPYRIGHT in top-level directory
 */

#include "mpiimpl.h"
#include "datatype.h"

#define COPY_BUFFER_SZ 16384

int MPIR_Localcopy(const void *sendbuf, MPI_Aint sendcount, MPI_Datatype sendtype,
                   void *recvbuf, MPI_Aint recvcount, MPI_Datatype recvtype)
{
    int mpi_errno = MPI_SUCCESS;
    int sendtype_iscontig, recvtype_iscontig;
    MPI_Aint sendsize, recvsize, sdata_sz, rdata_sz, copy_sz;
    MPI_Aint true_extent, sendtype_true_lb, recvtype_true_lb;
    char *buf = NULL;
    MPL_pointer_attr_t send_attr, recv_attr;
    MPIR_CHKLMEM_DECL(1);
    MPIR_FUNC_TERSE_STATE_DECL(MPID_STATE_MPIR_LOCALCOPY);

    MPIR_FUNC_TERSE_ENTER(MPID_STATE_MPIR_LOCALCOPY);

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
    MPIR_Datatype_iscontig(sendtype, &sendtype_iscontig);
    MPIR_Datatype_iscontig(recvtype, &recvtype_iscontig);

    MPIR_Type_get_true_extent_impl(sendtype, &sendtype_true_lb, &true_extent);
    MPIR_Type_get_true_extent_impl(recvtype, &recvtype_true_lb, &true_extent);

    if (sendtype_iscontig) {
        MPI_Aint actual_unpack_bytes;
        MPIR_Typerep_unpack((char *) sendbuf + sendtype_true_lb, copy_sz, recvbuf, recvcount,
                            recvtype, 0, &actual_unpack_bytes);
        MPIR_ERR_CHKANDJUMP(actual_unpack_bytes != copy_sz, mpi_errno, MPI_ERR_TYPE,
                            "**dtypemismatch");
    } else if (recvtype_iscontig) {
        MPI_Aint actual_pack_bytes;
        MPIR_Typerep_pack(sendbuf, sendcount, sendtype, 0, (char *) recvbuf + recvtype_true_lb,
                          copy_sz, &actual_pack_bytes);
        MPIR_ERR_CHKANDJUMP(actual_pack_bytes != copy_sz, mpi_errno, MPI_ERR_TYPE,
                            "**dtypemismatch");
    } else {
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
                              max_pack_bytes, &actual_pack_bytes);
            MPIR_Assert(actual_pack_bytes > 0);

            sfirst += actual_pack_bytes;

            MPI_Aint actual_unpack_bytes;
            MPIR_Typerep_unpack(buf, actual_pack_bytes, recvbuf, recvcount, recvtype,
                                rfirst, &actual_unpack_bytes);
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
    MPIR_FUNC_TERSE_EXIT(MPID_STATE_MPIR_LOCALCOPY);
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

void MPIDUI_Thread_cs_vci_check(MPIDU_Thread_mutex_t *p_mutex, int mutex_id, const char *mutex_str,
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
            assert(0);
            MPIR_Assert(0);
        }
        if ((((uint64_t)1) << (uint64_t)(mutex_id - 1)) & l_MPIU_exp_data.vci_mask) {
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
                       g_MPIU_exp_data.print_rank, tid, mutex_id, (void *)p_mutex,
                       (void *)p_vci_lock, mutex_str, function, file, line);
                fflush(0);
                assert(0);
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
            printf("[%2d:%2d] invalid mutex_id: %d (mask: %" PRIu64 ", %s in %s() %s:%d)\n",
                   g_MPIU_exp_data.print_rank, tid, mutex_id, l_MPIU_exp_data.vci_mask,
                   mutex_str, function, file, line);
            fflush(0);
            assert(0);
            MPIR_Assert(0);
        }
    }
}

void MPIDUI_Thread_cs_vci_print(MPIDU_Thread_mutex_t *p_mutex, int mutex_id, const char *msg,
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
    printf("[%2d:%2d] %s %s (id = %d) (%s() %s:%d, nolock = %d, mask = %" PRIu64 ")\n",
           g_MPIU_exp_data.print_rank, tid, msg, mutex_str, mutex_id, function, file, line, nolock,
           l_MPIU_exp_data.vci_mask);
    fflush(0);
}

#if defined(VCIEXP_LOCK_ARGOBOTS)
#define MAX_XSTREAMS 256
typedef struct {
    char dummy1[64];
    ABT_pool vci_pools[64]; /* private pools. */
    char dummy2[64];
} abt_data_t;

static abt_data_t g_abt_data;

static void update_vci_mask(uint64_t vci_mask)
{
    int ret;
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
    uint64_t vci;
    /* Remove the previous setting. */
    for (vci = 0; vci < 64; vci++) {
        if (g_abt_data.vci_pools[vci] == mypool)
            g_abt_data.vci_pools[vci] = NULL;
    }
    /* Update this setting. */
    for (vci = 0; vci < 64; vci++) {
        if ((((uint64_t)1) << vci) & vci_mask) {
            /* This vci is corresponding to this ES. */
            g_abt_data.vci_pools[vci] = mypool;
        }
    }
}

ABT_pool MPIDUI_Thread_cs_get_target_pool(int mutex_id)
{
    ABT_pool pool = g_abt_data.vci_pools[mutex_id - 1];
    if (pool == NULL) {
        printf("Error: mutex_id = %d\n", mutex_id);
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
        uint64_t vci_mask = *(uint64_t *)val1;
        l_MPIU_exp_data.vci_mask = vci_mask;
#if defined(VCIEXP_LOCK_ARGOBOTS)
        update_vci_mask(vci_mask);
#endif
    }
}

#else /* !(defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)) */

int MPIX_Set_exp_info(int info_type, void *val1, int val2)
{
    /* Ignored. */
}

#endif /* !(defined(VCIEXP_LOCK_PTHREADS) || defined(VCIEXP_LOCK_ARGOBOTS)) */
