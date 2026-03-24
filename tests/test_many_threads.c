/*
 * test_many_threads.c -- Stress test with many threads and allocations.
 *
 * Expected: the shared-state report should show the shared_array
 * accessed by all threads.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "bridge_client.h"

#define NUM_THREADS 8
#define ARRAY_SIZE  128

static int shared_array[ARRAY_SIZE];

static void* worker(void* arg)
{
    int id = *(int*)arg;

    BRIDGE_MONITOR_START("stress_test");

    /* Each thread reads and writes different indices, but they
     * all touch the same global array. */
    for (int i = 0; i < ARRAY_SIZE; i++) {
        int idx = (id * 16 + i) % ARRAY_SIZE;
        shared_array[idx] += id;
    }

    BRIDGE_MONITOR_STOP();
    return NULL;
}

int main(void)
{
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    for (int i = 0; i < ARRAY_SIZE; i++)
        shared_array[i] = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("shared_array[0] = %d\n", shared_array[0]);
    return 0;
}
