/*
 * test_heap.c -- Multiple threads sharing a heap-allocated buffer.
 *
 * Expected: the shared-state report should show the buffer
 * accessed by all worker threads.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "bridge_client.h"

#define NUM_THREADS 4
#define BUF_SIZE    64

static int* shared_buf = NULL;

static void* writer(void* arg)
{
    int id = *(int*)arg;

    BRIDGE_MONITOR_START("heap_test");

    for (int i = 0; i < BUF_SIZE; i++)
        shared_buf[i] = id;

    BRIDGE_MONITOR_STOP();
    return NULL;
}

int main(void)
{
    shared_buf = (int*)malloc(BUF_SIZE * sizeof(int));

    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, writer, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("shared_buf[0] = %d\n", shared_buf[0]);
    free(shared_buf);
    return 0;
}
