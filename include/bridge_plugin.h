/*
 * bridge_plugin.h -- Plugin callback interface for the Valgrind Bridge SDK.
 *
 * A plugin implements bridge_plugin_t and registers it via
 * BRIDGE_PLUGIN_REGISTER(). The bridge tool dispatches high-level
 * events to the plugin, completely hiding VEX IR from plugin authors.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef BRIDGE_PLUGIN_H
#define BRIDGE_PLUGIN_H

#include "pub_tool_basics.h"  /* Addr, SizeT, ThreadId, Bool, HChar */

/*--------------------------------------------------------------------*/
/*--- Plugin descriptor structure                                  ---*/
/*--------------------------------------------------------------------*/

typedef struct {
    /* Identity */
    const HChar* name;
    const HChar* version;
    const HChar* description;

    /* Lifecycle */
    void (*init)(void);
    void (*post_clo_init)(void);
    void (*finish)(Int exitcode);

    /* Command-line options */
    Bool (*process_cmd_line_option)(const HChar* arg);
    void (*print_usage)(void);
    void (*print_debug_usage)(void);

    /*
     * Memory access callback.
     *   tid      - thread performing the access
     *   addr     - guest virtual address
     *   size     - number of bytes accessed
     *   is_write - True for stores, False for loads
     */
    void (*on_mem_access)(ThreadId tid, Addr addr, SizeT size, Bool is_write);

    /*
     * Thread lifecycle callbacks.
     */
    void (*on_thread_create)(ThreadId parent, ThreadId child);
    void (*on_thread_exit)(ThreadId tid);

    /*
     * Client-request-based monitoring scope.
     * Called when the target program executes BRIDGE_START / BRIDGE_STOP.
     *   tid   - thread issuing the request
     *   label - user-supplied label string (only for start)
     */
    void (*on_monitor_start)(ThreadId tid, const HChar* label);
    void (*on_monitor_stop)(ThreadId tid);

} bridge_plugin_t;

/*--------------------------------------------------------------------*/
/*--- Plugin registration macro                                    ---*/
/*--------------------------------------------------------------------*/

/*
 * Every plugin must invoke this exactly once, at file scope, passing
 * a pointer to a static bridge_plugin_t.  The bridge tool's bg_main.c
 * picks this up via the well-known symbol.
 */
extern const bridge_plugin_t* bg_active_plugin;

#define BRIDGE_PLUGIN_REGISTER(plugin_ptr) \
    const bridge_plugin_t* bg_active_plugin = (plugin_ptr);

#endif /* BRIDGE_PLUGIN_H */
