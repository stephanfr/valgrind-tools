/*
 * bg_helpers.c -- Runtime helper functions called from instrumented code.
 *
 * These are "dirty helpers" inserted into the guest code by
 * bg_instrument().  They run on the simulated CPU and must be very
 * fast — the hot path is a single branch on the monitoring flag
 * followed by a dispatch into the plugin callback.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_machine.h"

#include "bridge_plugin.h"

/* Maximum thread ID we support.  Valgrind's VG_N_THREADS is a
 * runtime variable (typically ~500), not a compile-time constant. */
#define BG_MAX_THREADS 512

/* Per-thread monitoring state, managed by bg_clientreqs.c */
extern Bool bg_monitoring_active[BG_MAX_THREADS];

/*
 * bg_helper_mem_access -- called for every load/store in instrumented code.
 *
 * This function is inserted as a dirty call with 3 register-passed args
 * (VG_REGPARM(3)) for maximum speed on x86-64.
 *
 *   addr     - the guest virtual address of the access
 *   size     - number of bytes
 *   is_write - 1 for store, 0 for load
 */
VG_REGPARM(3) void bg_helper_mem_access(Addr addr, SizeT size,
                                         UWord is_write)
{
    ThreadId tid = VG_(get_running_tid)();

    if (tid >= BG_MAX_THREADS)
        return;

    /* Fast path: skip if monitoring is not active for this thread */
    if (!bg_monitoring_active[tid])
        return;

    if (bg_active_plugin && bg_active_plugin->on_mem_access)
        bg_active_plugin->on_mem_access(tid, addr, size,
                                         (Bool)(is_write != 0));
}
