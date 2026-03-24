/*
 * bg_threads.c -- Thread lifecycle tracking for the Bridge SDK.
 *
 * Uses Valgrind's track_pre_thread_ll_create / track_pre_thread_ll_exit
 * to detect thread creation and exit, and dispatches to the plugin.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_threadstate.h"

#include "bridge_plugin.h"

#define BG_MAX_THREADS 512

/* Per-thread monitoring state — shared with bg_helpers.c / bg_clientreqs.c */
Bool bg_monitoring_active[BG_MAX_THREADS];

/*--------------------------------------------------------------------*/
/*--- Thread callbacks                                             ---*/
/*--------------------------------------------------------------------*/

static void bg_thread_create(ThreadId parent, ThreadId child)
{
    /* New thread starts with monitoring OFF */
    bg_monitoring_active[child] = False;

    if (bg_active_plugin && bg_active_plugin->on_thread_create)
        bg_active_plugin->on_thread_create(parent, child);
}

static void bg_thread_exit(ThreadId tid)
{
    bg_monitoring_active[tid] = False;

    if (bg_active_plugin && bg_active_plugin->on_thread_exit)
        bg_active_plugin->on_thread_exit(tid);
}

/*--------------------------------------------------------------------*/
/*--- Registration (called from bg_pre_clo_init)                   ---*/
/*--------------------------------------------------------------------*/

void bg_threads_init(void)
{
    UInt i;
    for (i = 0; i < BG_MAX_THREADS; i++)
        bg_monitoring_active[i] = False;

    VG_(track_pre_thread_ll_create)(bg_thread_create);
    VG_(track_pre_thread_ll_exit)(bg_thread_exit);
}
