/*
 * sharedstate.c -- Shared-State Tracker plugin for the Valgrind Bridge SDK.
 *
 * Monitors memory accesses and builds an inventory of all memory
 * locations accessed by more than one thread.  Produces both a
 * human-readable text report and a JSON file suitable for further
 * analysis and TLA+ model development.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_machine.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_options.h"
#include "pub_tool_debuginfo.h"
#include "bridge_plugin.h"
#include "bridge_utils.h"
#include "ss_types.h"
#include "ss_alloc.h"
#include "ss_report.h"

#define CC "sharedstate"

#define SS_MAX_THREADS 512

/*--------------------------------------------------------------------*/
/*--- Per-thread state                                             ---*/
/*--------------------------------------------------------------------*/

/* Active section label per thread */
static const HChar* ss_thread_label[SS_MAX_THREADS];

/*--------------------------------------------------------------------*/
/*--- Plugin callbacks                                             ---*/
/*--------------------------------------------------------------------*/

static void ss_init(void)
{
    ss_alloc_init();

    UInt i;
    for (i = 0; i < SS_MAX_THREADS; i++)
        ss_thread_label[i] = NULL;
}

static void ss_finish(Int exitcode)
{
    ss_report_text();
    ss_report_json();
    ss_alloc_destroy();
}

static void ss_on_mem_access(ThreadId tid, Addr addr, SizeT size,
                              Bool is_write)
{
    /* Find or create the allocation for this address */
    SSAlloc* a = ss_alloc_find(addr);

    if (!a) {
        /* Unknown allocation (global, stack, or unmapped).
         * Create a pseudo-allocation for the accessed region. */
        a = VG_(malloc)(CC, sizeof(SSAlloc));
        VG_(memset)(a, 0, sizeof(SSAlloc));
        a->key  = (UWord)addr;
        a->base = addr;
        a->size = size;
        a->kind = SS_ALLOC_UNKNOWN;

        /* Try to determine if this is a global or stack address.
         * Simple heuristic: check if it's in the data segment. */
        const HChar* name = NULL;
        PtrdiffT offset;
        if (bridge_get_datasym(addr, &name, &offset)) {
            a->kind = SS_ALLOC_GLOBAL;
        }

        /* Register it so future accesses to the same region match */
        ss_alloc_new_block(a->base, a->size, tid);
        VG_(free)(a);  /* ss_alloc_new_block made its own copy */
        a = ss_alloc_find(addr);
        if (!a) return;  /* shouldn't happen */
        if (name) {
            /* Re-check kind */
            a->kind = SS_ALLOC_GLOBAL;
        }
    }

    /* Update the thread set */
    if (tid < SS_MAX_THREADS_FAST) {
        ULong mask = 1ULL << tid;
        if (!(a->thread_set & mask)) {
            a->thread_set |= mask;
            a->thread_count++;
        }
    }

    /* Set section label if not yet set */
    if (!a->section_label && tid < SS_MAX_THREADS)
        a->section_label = ss_thread_label[tid];

    /* Record the access (subject to per-alloc limit) */
    if (a->access_count < SS_MAX_ACCESSES_PER_ALLOC) {
        SSAccess* acc = VG_(malloc)(CC, sizeof(SSAccess));
        VG_(memset)(acc, 0, sizeof(SSAccess));
        acc->tid      = tid;
        acc->is_write = is_write;
        acc->ip       = bridge_get_ip(tid);
        acc->stacktrace_depth =
            bridge_get_stacktrace(tid, acc->stacktrace,
                                  SS_STACKTRACE_DEPTH);

        /* Prepend to the accesses list */
        acc->next    = a->accesses;
        a->accesses  = acc;
        a->access_count++;
    }
}

static void ss_on_thread_create(ThreadId parent, ThreadId child)
{
    if (child < SS_MAX_THREADS)
        ss_thread_label[child] = NULL;
}

static void ss_on_thread_exit(ThreadId tid)
{
    if (tid < SS_MAX_THREADS)
        ss_thread_label[tid] = NULL;
}

static void ss_on_monitor_start(ThreadId tid, const HChar* label)
{
    if (tid < SS_MAX_THREADS)
        ss_thread_label[tid] = label;
    VG_(umsg)("SharedState: monitoring started on T%u (section: \"%s\")\n",
              tid, label ? label : "");
}

static void ss_on_monitor_stop(ThreadId tid)
{
    VG_(umsg)("SharedState: monitoring stopped on T%u\n", tid);
    if (tid < SS_MAX_THREADS)
        ss_thread_label[tid] = NULL;
}

static Bool ss_process_cmd_line_option(const HChar* arg)
{
    /* No custom options yet */
    return False;
}

static void ss_print_usage(void)
{
    VG_(printf)(
        "    (none)  SharedState Tracker has no custom options yet.\n"
    );
}

static void ss_print_debug_usage(void)
{
    VG_(printf)(
        "    (none)  SharedState Tracker has no debug options.\n"
    );
}

/*--------------------------------------------------------------------*/
/*--- Plugin descriptor                                            ---*/
/*--------------------------------------------------------------------*/

static const bridge_plugin_t ss_plugin = {
    .name        = "bridge",
    .version     = "0.1",
    .description = "SharedState Tracker: inventories multi-threaded shared memory",

    .init                     = ss_init,
    .post_clo_init            = NULL,
    .finish                   = ss_finish,

    .process_cmd_line_option  = ss_process_cmd_line_option,
    .print_usage              = ss_print_usage,
    .print_debug_usage        = ss_print_debug_usage,

    .on_mem_access            = ss_on_mem_access,
    .on_thread_create         = ss_on_thread_create,
    .on_thread_exit           = ss_on_thread_exit,
    .on_monitor_start         = ss_on_monitor_start,
    .on_monitor_stop          = ss_on_monitor_stop,
};

BRIDGE_PLUGIN_REGISTER(&ss_plugin)
