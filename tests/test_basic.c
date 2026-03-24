/*
 * test_basic.c -- Two threads sharing a global variable.
 *
 * Expected: the shared-state report should show g_counter
 * accessed by two threads.
 */

#include <pthread.h>
#include <stdio.h>
#include "bridge_client.h"

static int g_counter = 0;

static void* worker(void* arg)
{
    BRIDGE_MONITOR_START("basic_test");

    for (int i = 0; i < 100; i++)
        g_counter++;

    BRIDGE_MONITOR_STOP();
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("g_counter = %d\n", g_counter);
    return 0;
}
