/*
 * test_no_sharing.c -- Threads with no shared state.
 *
 * Expected: the shared-state report should be empty.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "bridge_client.h"

static void* worker(void* arg)
{
    int local_counter = 0;

    BRIDGE_MONITOR_START("no_sharing_test");

    /* Each thread only touches its own local variable */
    for (int i = 0; i < 100; i++)
        local_counter += i;

    BRIDGE_MONITOR_STOP();

    printf("Thread done: local_counter = %d\n", local_counter);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
