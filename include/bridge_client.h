/*
 * bridge_client.h -- Client-request macros for target programs (C).
 *
 * Include this header in the program you want to analyse.
 * It works whether or not the program runs under Valgrind.
 *
 * Usage:
 *   BRIDGE_MONITOR_START("my_section");
 *   ... code to monitor ...
 *   BRIDGE_MONITOR_STOP();
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef BRIDGE_CLIENT_H
#define BRIDGE_CLIENT_H

#include "valgrind/valgrind.h"

/*
 * Client-request IDs.  'B' 'G' → Bridge.
 * Must match the values handled in bridge/bg_clientreqs.c.
 */
typedef enum {
    VG_USERREQ__BRIDGE_MONITOR_START =
        VG_USERREQ_TOOL_BASE('B','G'),
    VG_USERREQ__BRIDGE_MONITOR_STOP,
} BridgeClientRequest;

/*
 * BRIDGE_MONITOR_START(label)
 *   Begin monitoring memory accesses on the calling thread.
 *   `label` is a const char* tag for the monitored section.
 */
#define BRIDGE_MONITOR_START(label)                              \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                             \
        VG_USERREQ__BRIDGE_MONITOR_START,                        \
        (label), 0, 0, 0, 0)

/*
 * BRIDGE_MONITOR_STOP()
 *   Stop monitoring memory accesses on the calling thread.
 */
#define BRIDGE_MONITOR_STOP()                                    \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                             \
        VG_USERREQ__BRIDGE_MONITOR_STOP,                         \
        0, 0, 0, 0, 0)

#endif /* BRIDGE_CLIENT_H */
