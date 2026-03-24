/*
 * bridge_utils.h -- Utility wrappers for plugin authors.
 *
 * Plugins run inside Valgrind and cannot call libc.  These thin
 * wrappers expose the Valgrind-internal equivalents through a
 * stable, easy-to-use interface.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef BRIDGE_UTILS_H
#define BRIDGE_UTILS_H

#include "pub_tool_basics.h"       /* Addr, SizeT, ThreadId, HChar */
#include "pub_tool_mallocfree.h"   /* VG_(malloc), VG_(free) */
#include "pub_tool_libcprint.h"    /* VG_(umsg), VG_(sprintf) */
#include "pub_tool_libcbase.h"     /* VG_(memset), VG_(strlen), ... */
#include "pub_tool_stacktrace.h"   /* VG_(get_StackTrace) */
#include "pub_tool_machine.h"      /* VG_(get_IP) */
#include "pub_tool_debuginfo.h"    /* VG_(get_fnname), VG_(describe_IP) */
#include "pub_tool_execontext.h"   /* VG_(record_ExeContext) */
#include "pub_tool_libcfile.h"     /* VG_(fd_open), VG_(write), ... */

/*--------------------------------------------------------------------*/
/*--- Allocation helpers (use Valgrind's tool-arena allocator)     ---*/
/*--------------------------------------------------------------------*/

/* cc = cost-centre string for leak-tracking; use the plugin name */
static inline void* bridge_alloc(const HChar* cc, SizeT nbytes) {
    return VG_(malloc)(cc, nbytes);
}

static inline void* bridge_calloc(const HChar* cc, SizeT n, SizeT elem_sz) {
    return VG_(calloc)(cc, n, elem_sz);
}

static inline void* bridge_realloc(const HChar* cc, void* p, SizeT new_sz) {
    return VG_(realloc)(cc, p, new_sz);
}

static inline void bridge_free(void* p) {
    VG_(free)(p);
}

static inline HChar* bridge_strdup(const HChar* cc, const HChar* s) {
    return VG_(strdup)(cc, s);
}

/*--------------------------------------------------------------------*/
/*--- Printing helpers                                             ---*/
/*--------------------------------------------------------------------*/

/* Print a user-facing message (goes to stderr / log file) */
#define bridge_umsg(...) VG_(umsg)(__VA_ARGS__)

/* Print a debug message (only shown at -v or higher) */
#define bridge_dmsg(...) VG_(dmsg)(__VA_ARGS__)

/* sprintf into a buffer */
#define bridge_sprintf  VG_(sprintf)
#define bridge_snprintf VG_(snprintf)

/*--------------------------------------------------------------------*/
/*--- String / memory helpers (no libc!)                           ---*/
/*--------------------------------------------------------------------*/

#define bridge_memset  VG_(memset)
#define bridge_memcpy  VG_(memcpy)
#define bridge_memcmp  VG_(memcmp)
#define bridge_strlen  VG_(strlen)
#define bridge_strcmp   VG_(strcmp)
#define bridge_strncmp VG_(strncmp)
#define bridge_strcpy  VG_(strcpy)

/*--------------------------------------------------------------------*/
/*--- Stack trace helpers                                          ---*/
/*--------------------------------------------------------------------*/

#define BRIDGE_MAX_STACKTRACE 16

/* Get a stack trace for the given thread.
 * Returns the number of valid entries written into ips[]. */
static inline UInt bridge_get_stacktrace(ThreadId tid, Addr* ips,
                                         UInt max_depth)
{
    return VG_(get_StackTrace)(tid, ips, max_depth, NULL, NULL, 0);
}

/* Pretty-print a single IP to a static buffer (returned pointer).
 * The buffer is overwritten on each call. */
static inline const HChar* bridge_describe_ip(Addr ip) {
    return VG_(describe_IP)(VG_(current_DiEpoch)(), ip, NULL);
}

/* Get current instruction pointer for a thread */
static inline Addr bridge_get_ip(ThreadId tid) {
    return VG_(get_IP)(tid);
}

/*--------------------------------------------------------------------*/
/*--- Debug-info helpers                                           ---*/
/*--------------------------------------------------------------------*/

/* Resolve an address to a function name. Returns True on success. */
static inline Bool bridge_get_fnname(Addr a, const HChar** out) {
    return VG_(get_fnname)(VG_(current_DiEpoch)(), a, out);
}

/* Resolve an address to file:line. Returns True on success. */
static inline Bool bridge_get_file_line(Addr a, const HChar** file,
                                        UInt* line)
{
    return VG_(get_filename_linenum)(VG_(current_DiEpoch)(), a,
                                    file, NULL, line);
}

/* Resolve an address to a data symbol + offset. Returns True on success. */
static inline Bool bridge_get_datasym(Addr a, const HChar** name,
                                      PtrdiffT* offset)
{
    return VG_(get_datasym_and_offset)(VG_(current_DiEpoch)(), a,
                                      name, offset);
}

/*--------------------------------------------------------------------*/
/*--- File I/O helpers                                             ---*/
/*--------------------------------------------------------------------*/

/* Open a file.  Returns fd >= 0 on success, < 0 on failure.
 * flags / mode are standard POSIX (VKI_O_WRONLY | VKI_O_CREAT etc.) */
static inline Int bridge_fd_open(const HChar* path, Int flags, Int mode) {
    return VG_(fd_open)(path, flags, mode);
}

static inline void bridge_close(Int fd) {
    VG_(close)(fd);
}

static inline Int bridge_write(Int fd, const void* buf, Int count) {
    return VG_(write)(fd, buf, count);
}

#endif /* BRIDGE_UTILS_H */
