/*
 * bg_clientreqs.c -- Client request handler for Bridge monitor start/stop.
 *
 * When the target program calls BRIDGE_MONITOR_START / BRIDGE_MONITOR_STOP
 * (from bridge_client.h), the request arrives here.  We update the
 * per-thread monitoring flag and dispatch to the plugin.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"

#include "bridge_plugin.h"
#include "bridge_client.h"   /* BridgeClientRequest enum values */

#define BG_MAX_THREADS 512

/* Per-thread monitoring state, defined in bg_threads.c */
extern Bool bg_monitoring_active[BG_MAX_THREADS];

Bool bg_handle_client_request(ThreadId tid, UWord* args, UWord* ret)
{
    if (!VG_IS_TOOL_USERREQ('B', 'G', args[0]))
        return False;

    switch (args[0]) {

    case VG_USERREQ__BRIDGE_MONITOR_START: {
        const HChar* label = (const HChar*)(Addr)args[1];
        bg_monitoring_active[tid] = True;
        if (bg_active_plugin && bg_active_plugin->on_monitor_start)
            bg_active_plugin->on_monitor_start(tid, label);
        *ret = 0;
        return True;
    }

    case VG_USERREQ__BRIDGE_MONITOR_STOP: {
        bg_monitoring_active[tid] = False;
        if (bg_active_plugin && bg_active_plugin->on_monitor_stop)
            bg_active_plugin->on_monitor_stop(tid);
        *ret = 0;
        return True;
    }

    default:
        return False;
    }
}
