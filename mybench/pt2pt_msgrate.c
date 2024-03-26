
#include <mpi.h>
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

int g_comm_size;
int g_comm_rank;
int g_benchmark_type;
int g_num_entities;
int g_num_comms;
int g_debug_mode = 0;
int g_nolock_mode = 0;
int g_num_repeats = 1;
int g_num_threads = 1;
#define COMM_TYPE_PT2PT 0
#define COMM_TYPE_PUT 1
#define COMM_TYPE_GET 2
int g_comm_type = COMM_TYPE_PT2PT;

size_t g_num_messages;
size_t g_window_size;
size_t g_message_size;

barrier_handle_t g_barrier;

typedef struct {
    int tid;
    MPI_Comm comm;
    double elapsed_time;
    thread_handle_t thread;
} thread_arg_t;

void thread_func(void *arg)
{
    double elapsed_time = 0.0;
    int ret;

    thread_arg_t *p_thread_arg = (thread_arg_t *) arg;
    const size_t win_posts = g_num_messages / g_window_size + 1;
    const size_t message_size = g_message_size;
    const size_t window_size = g_window_size;
    const int comm_type = g_comm_type;
    MPI_Comm comm = p_thread_arg->comm;
    const int tid = p_thread_arg->tid;

    MPI_Request *requests;
    MPI_Status *statuses;
    char *buf;
    const size_t buf_size = window_size * message_size * sizeof(char) + 4096;
    MPI_Win win;
    if (comm_type == COMM_TYPE_PT2PT) {
        requests = (MPI_Request *) malloc(sizeof(MPI_Request) * window_size + 128);
        statuses = (MPI_Status *) malloc(sizeof(MPI_Status) * window_size + 128);
        ret = posix_memalign((void **) &buf, 128, buf_size);
        assert(ret == 0);
    } else {
        for (int i = 0; i < g_num_threads; i++) {
            thread_barrier_wait(&g_barrier);
            if (i != tid)
                continue;
            ret = MPI_Win_allocate(buf_size, 1, MPI_INFO_NULL, comm, (void *) &buf, &win);
            assert(ret == 0);
            ret = MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
            assert(ret == 0);
        }
        thread_barrier_wait(&g_barrier);
    }
    memset(buf, 0, buf_size);

    /* Benchmark */
    for (int i = 0; i < 1 + g_num_repeats; i++) {
        if (tid == 0)
            MPI_Barrier(MPI_COMM_WORLD);
        thread_barrier_wait(&g_barrier);
        double start_time = MPI_Wtime();
        if (tid == 0)
            MPI_Barrier(MPI_COMM_WORLD);
        thread_barrier_wait(&g_barrier);

        int target;
        target = g_comm_rank ^ 1;
        if (g_debug_mode > 0) {
            char processor_name[256];
            int len = 256;
            MPI_Get_processor_name(processor_name, &len);
            printf("[%s:%d] target rank: %d\n", processor_name, g_comm_rank, target);
        }
#if defined(USE_PTHREADS) || defined(USE_ARGOBOTS)
        if (g_debug_mode > 0) {
            MPIX_Set_exp_info(MPIX_INFO_TYPE_PRINT_RANK, NULL, g_comm_rank);
            MPIX_Set_exp_info(MPIX_INFO_TYPE_LOCAL_TID, NULL, tid);
            MPIX_Set_exp_info(MPIX_INFO_TYPE_DEBUG_ENABLED, NULL, g_debug_mode ? 1 : 0);
            MPIX_Set_exp_info(MPIX_INFO_TYPE_PRINT_ENABLED, NULL, g_debug_mode - 1);
        }
        if (g_nolock_mode) {
            MPIX_Set_exp_info(MPIX_INFO_TYPE_NOLOCK, NULL, 1);
            uint64_t vci_mask = ((uint64_t) 1) << ((uint64_t) tid);
            MPIX_Set_exp_info(MPIX_INFO_TYPE_VCIMASK, &vci_mask, 0);
            thread_barrier_wait(&g_barrier);
        }
#endif
        /* The core kernel. */
        for (size_t win_post_i = 0; win_post_i < win_posts; win_post_i++) {
            int is_sender = (g_comm_rank & 1) ? (win_post_i % 2) : (1 - win_post_i % 2);
            if (comm_type == COMM_TYPE_PT2PT) {
                for (size_t win_i = 0; win_i < window_size; win_i++) {
                    if (is_sender) {
                        ret = MPI_Isend(&buf[win_i * message_size], message_size, MPI_CHAR, target,
                                        tid, comm, &requests[win_i]);
                        assert(ret == 0);
                    } else {
                        ret = MPI_Irecv(&buf[win_i * message_size], message_size, MPI_CHAR, target,
                                        tid, comm, &requests[win_i]);
                        assert(ret == 0);
                    }
                }
                ret = MPI_Waitall(window_size, requests, statuses);
                assert(ret == 0);
            } else if (comm_type == COMM_TYPE_PUT && is_sender) {
                for (size_t win_i = 0; win_i < window_size; win_i++) {
                    ret = MPI_Put(&buf[win_i * message_size], message_size,
                                  MPI_CHAR, target, win_i * message_size,
                                  message_size, MPI_CHAR, win);
                    assert(ret == 0);
                }
                ret = MPI_Win_flush(target, win);
                assert(ret == 0);
            } else if (comm_type == COMM_TYPE_GET && !is_sender) {
                for (size_t win_i = 0; win_i < window_size; win_i++) {
                    ret = MPI_Get(&buf[win_i * message_size], message_size,
                                  MPI_CHAR, target, win_i * message_size,
                                  message_size, MPI_CHAR, win);
                    assert(ret == 0);
                }
                ret = MPI_Win_flush(target, win);
                assert(ret == 0);
            }
        }

#if defined(USE_PTHREADS) || defined(USE_ARGOBOTS)
        if (g_nolock_mode) {
            thread_barrier_wait(&g_barrier);
            MPIX_Set_exp_info(MPIX_INFO_TYPE_NOLOCK, NULL, 0);
        }
        if (g_debug_mode) {
            MPIX_Set_exp_info(MPIX_INFO_TYPE_DEBUG_ENABLED, NULL, 0);
            MPIX_Set_exp_info(MPIX_INFO_TYPE_PRINT_ENABLED, NULL, 0);
        }
#endif
        if (tid == 0)
            MPI_Barrier(MPI_COMM_WORLD);
        thread_barrier_wait(&g_barrier);
        double end_time = MPI_Wtime();
        if (i >= 1) {
            elapsed_time += end_time - start_time;
            if (tid == 0 && g_comm_rank == 0){
             //   printf("%f[ms]\n", (end_time - start_time) * 1.0e3);
            }
        }
    }
    if (comm_type == COMM_TYPE_PT2PT) {
        free(requests);
        free(buf);
    } else {
        for (int i = 0; i < g_num_threads; i++) {
            thread_barrier_wait(&g_barrier);
            if (i != tid)
                continue;
            ret = MPI_Win_unlock_all(win);
            assert(ret == 0);
            ret = MPI_Win_free(&win);
            assert(ret == 0);
        }
    }
    p_thread_arg->elapsed_time = elapsed_time / g_num_repeats;
}

typedef struct {
    int *p_argc;
    char ***p_argv;
} main_thread_arg_t;

void main_thread_func(void *arg)
{
    main_thread_arg_t *p_arg = (main_thread_arg_t *) arg;
    int num_threads;
    if (g_benchmark_type == 0) {
        /* MPI everywhere */
        int provided;
        MPI_Init_thread(p_arg->p_argc, p_arg->p_argv, MPI_THREAD_SINGLE, &provided);
        assert(provided == MPI_THREAD_SINGLE);
        num_threads = 1;
    } else {
        /* Multithreaded. */
        int provided;
        MPI_Init_thread(p_arg->p_argc, p_arg->p_argv, MPI_THREAD_MULTIPLE, &provided);
        assert(provided == MPI_THREAD_MULTIPLE);
        num_threads = g_num_entities;
    }
    g_num_threads = num_threads;

    MPI_Comm_rank(MPI_COMM_WORLD, &g_comm_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_comm_size);

    if (g_benchmark_type == 0) {
        /* MPI everywhere */
        assert(g_comm_size == g_num_entities * 2);
    } else {
        /* Multithreaded. */
        //printf("g_comm_size = %d\n", g_comm_size);
        assert(g_comm_size == 2);
    }

    thread_barrier_init(num_threads, &g_barrier);
    thread_arg_t *thread_args = (thread_arg_t *) malloc(sizeof(thread_arg_t) * num_threads);
    /* Set up arguments. */
    for (int i = 0; i < num_threads; i++) {
        /* Thread ID */
        thread_args[i].tid = i;
        if (g_comm_type == COMM_TYPE_PT2PT) {
            /* Create a communicator per thread */
            if (i < g_num_comms) {
                MPI_Info info;
                MPI_Info_create(&info);
                MPI_Comm_dup_with_info(MPI_COMM_WORLD, info, &thread_args[i].comm);
                MPI_Info_free(&info);
            } else {
                /* Reuse communicators */
                thread_args[i].comm = thread_args[i % g_num_comms].comm;
            }
        } else {
            /* MPI_Win internally creates a communicator. */
            thread_args[i].comm = MPI_COMM_WORLD;
        }
    }

    /* Run tests. */
    for (int i = 1; i < num_threads; i++) {
        thread_create(thread_func, &thread_args[i], &thread_args[i].thread);
    }
    thread_func(&thread_args[0]);
    for (int i = 1; i < num_threads; i++) {
        thread_join(&thread_args[i].thread);
    }

    if (g_comm_rank == 0) {
        if(g_benchmark_type == 0){
            printf("mpi-everywhere,");
        }
        else {
            printf("mpi+");
            #if defined(USE_PTHREADS)
            if(g_nolock_mode)
            printf("pthreads-nolock,");
            else
            printf("pthreads,");
            #elif defined (USE_ARGOBOTS)
            if(g_nolock_mode)
            printf("argobots-nolock,");
            else
            printf("argobots,");
            #else
            printf("unknown,");
            #endif
        }

        if (g_comm_type == COMM_TYPE_PT2PT) {
            printf("p2p,");
        } else if (g_comm_type == COMM_TYPE_PUT) {
            printf("put,");
        } else if (g_comm_type == COMM_TYPE_GET) {
            printf("get,");
        }
        printf(/*"Number of messages: */"%lld,", (long long) g_num_messages);
        printf(/*"Message size: */"%lld,", (long long) g_message_size);
        printf(/*"Window size:*/ "%lld,", (long long) g_window_size);
        printf(/*"Number of entities:*/"%d,", g_num_entities);
        printf(/*"Number of communicators: */"%d,", g_num_comms);
        printf(/*"Number of repetitions: */"%d,", g_num_repeats);
        const size_t win_posts = g_num_messages / g_window_size + 1;
        double total_num_messages = ((double) win_posts) * g_window_size * g_num_entities;
        printf(/*"Total message rates: */"%.4f,",
               total_num_messages / thread_args[0].elapsed_time * 1.0e-6);
        double total_send_bytes = total_num_messages * g_message_size;
        printf(/*"Total bandwidth:*/ "%.4f\n",
               total_send_bytes / thread_args[0].elapsed_time * 1.0e-6);
    }

    for (int i = 0; i < num_threads; i++) {
        if (g_comm_type == COMM_TYPE_PT2PT) {
            if (i >= g_num_comms)
                break;
            MPI_Comm_free(&thread_args[i].comm);
        }
    }
    MPI_Finalize();

    free(thread_args);
    thread_barrier_destroy(&g_barrier);
}

int main(int argc, char *argv[])
{
    if (argc != 7) {
        printf("Usage: ./bench.out BENCHMARK_TYPE NUM_ENTITIES NUM_COMMS "
               "NUM_MSGS WINDOW_SIZE MSG_SIZE\n");
        printf("BENCHMARK_TYPE = 0 : MPI everywhere / 2 * NUM_ENTITIES " "processes\n");
        printf("               = 1 : MPI+Threads   / 2 processes, NUM_ENTITIES "
               "threads per process\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    g_benchmark_type = atoi(argv[1]);
    g_num_entities = atoi(argv[2]);
    g_num_comms = atoi(argv[3]);
    g_num_messages = atol(argv[4]);
    g_window_size = atol(argv[5]);
    g_message_size = atol(argv[6]);
    if (getenv("DEBUG")) {
        g_debug_mode = atoi(getenv("DEBUG"));
    }
    if (getenv("NOLOCK")) {
        g_nolock_mode = atoi(getenv("NOLOCK"));
    }
    if (getenv("NUM_REPEATS")) {
        g_num_repeats = atoi(getenv("NUM_REPEATS"));
    }
    if (getenv("COMM_TYPE")) {
        g_comm_type = atoi(getenv("COMM_TYPE"));
    }
    
    if (getenv("PROGMASK")) {
        int user_progmask = atoi(getenv("PROGMASK"));
#if defined(USE_PTHREADS) || defined(USE_ARGOBOTS)
        int progmask = 2;
        while (progmask < user_progmask + 1) {
            progmask *= 2;
        }
        MPIX_Set_exp_info(MPIX_INFO_TYPE_PROGMASK, NULL, progmask - 1);
#else
        (void) user_progmask;
#endif
    }

    thread_library_init();
    main_thread_arg_t arg;
    thread_handle_t main_thread_handle;
    arg.p_argc = &argc;
    arg.p_argv = &argv;
    thread_create(main_thread_func, &arg, &main_thread_handle);
    thread_join(&main_thread_handle);
    thread_library_finalize();

    return 0;
}
