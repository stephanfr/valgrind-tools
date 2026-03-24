/*
 * test_scoped.cpp -- C++ RAII BridgeMonitor usage.
 *
 * Expected: the shared-state report should show g_shared
 * accessed by both threads.
 */

#include <pthread.h>
#include <cstdio>
#include "bridge_client.hpp"

static int g_shared = 0;

static void* worker(void* arg)
{
    {
        BridgeMonitor mon("scoped_test");

        for (int i = 0; i < 50; i++)
            g_shared += 1;
    }
    // monitoring stops here when `mon` goes out of scope

    return nullptr;
}

int main()
{
    pthread_t t1, t2;

    pthread_create(&t1, nullptr, worker, nullptr);
    pthread_create(&t2, nullptr, worker, nullptr);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);

    std::printf("g_shared = %d\n", g_shared);
    return 0;
}
