#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LL_INSERT(item, list) do {      \
    item->prev = NULL;      \
    item->next = list;      \
    if (list != NULL) list->prev = item;        \
    list = item;        \
} while (0)

#define LL_REMOVE(item, list) do {      \
    if (item->prev != NULL) item->prev->next = item->next;        \
    if (item->next != NULL) item->next->prev = item->prev;        \
    if (list == item) list = item->next;        \
    item->prev = item->next = NULL;     \
} while (0)

typedef struct NWORKER {
    pthread_t id;
    int terminate;
    struct NMANAGER* pool;

    struct NWORKER* prev;
    struct NWORKER* next;
} nworker;

typedef struct NTASK {
    void (*task_func)(void* arg);
    void* user_data;

    struct NTASK* prev;
    struct NTASK* next;
} njob;

typedef struct NMANAGER {
    struct NWORKER* workers;
    struct NTASK* tasks;

    pthread_mutex_t mtx;
    pthread_cond_t cond;
} nthreadpool_t;

static void thread_callback(void* arg) {
    nworker* worker = (nworker*)arg;
    while(1) {
        pthread_mutex_lock(&worker->pool->mtx);
        while (worker->pool->tasks == NULL) {
            pthread_cond_wait(&worker->pool->cond, &worker->pool->mtx);
            if (worker->terminate) break;
        }
        if (worker->terminate) {
            pthread_mutex_unlock(&worker->pool->mtx);
            break;
        }

        njob* task = worker->pool->tasks;
        if (task) {
            LL_REMOVE(task, worker->pool->tasks);
        }

        pthread_mutex_unlock(&worker->pool->mtx);

        task->task_func(task);
    }
}

void thread_pool_push(nthreadpool_t* pool, njob* task) {
    pthread_mutex_lock(&pool->mtx);

    LL_INSERT(task, pool->tasks);
    pthread_cond_signal(&pool->cond);

    pthread_mutex_unlock(&pool->mtx);
}

void thread_pool_destroy(nthreadpool_t* pool) {
    for (nworker* worker = pool->workers; worker != NULL; worker = worker->next) {
        worker->terminate = 1;
    }
    pthread_mutex_lock(&pool->mtx);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);
}

int thread_pool_create(nthreadpool_t* pool, int nthread) {
    if (pool == NULL) return -1;
    if (nthread < 1) nthread = 1;

    memset(pool, 0, sizeof(nthreadpool_t));

    pthread_mutex_t blank_mtx = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mtx, &blank_mtx, sizeof(blank_mtx));

    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond, &blank_cond, sizeof(blank_cond));

    int idx = 0;

    for (idx = 0; idx < nthread; idx ++) {
        nworker* worker = (nworker*)malloc(sizeof(nworker));
        if (worker == NULL) {
            perror("malloc");
            exit(1);
        }
        memset(worker, 0, sizeof(nworker));
        worker->pool = pool;
        worker->terminate = 0;
        int ret = pthread_create(&worker->id, NULL, thread_callback, worker);
        if (ret) {
            perror("pthread_create");
            free(worker);
            exit(1);
        }
        LL_INSERT(worker, pool->workers);
    }

    return idx;
}

void counter(njob* job) {
    printf("idx : %d\n", *(int*)(job->user_data));
}

int main () {
    struct NMANAGER pool;
    int nthread = 20;

    int ret = thread_pool_create(&pool, nthread);

    const int TASK_COUNT = 1000;

    for (int i = 0; i < TASK_COUNT; i ++) {
        njob* job = (njob*)malloc(sizeof(njob));
        if (job == NULL) exit(1);
        job->task_func = counter;
        job->user_data = malloc(sizeof(int));
        *(int*)job->user_data = i;
        thread_pool_push(&pool, job);
    }

    return 0;
}