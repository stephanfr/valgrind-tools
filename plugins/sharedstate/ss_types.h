/*
 * ss_types.h -- Core data types for the Shared-State Tracker plugin.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef SS_TYPES_H
#define SS_TYPES_H

#include "pub_tool_basics.h"
#include "bridge_utils.h"

/* Maximum threads we can track in a thread-set bitmask.
 * Valgrind's VG_N_THREADS is around 500; we use 64 for the bitmask
 * and store an overflow list if needed. */
#define SS_MAX_THREADS_FAST 64

/* Maximum stack trace depth per access record */
#define SS_STACKTRACE_DEPTH 12

/* Maximum number of recorded accesses per allocation before we stop
 * recording (to bound memory usage). */
#define SS_MAX_ACCESSES_PER_ALLOC 256

/*--------------------------------------------------------------------*/
/*--- Access record                                                ---*/
/*--------------------------------------------------------------------*/

typedef struct _SSAccess {
    ThreadId tid;
    Bool     is_write;
    Addr     ip;                               /* instruction pointer */
    Addr     stacktrace[SS_STACKTRACE_DEPTH];  /* IPs */
    UInt     stacktrace_depth;                 /* valid entries */
    struct _SSAccess* next;
} SSAccess;

/*--------------------------------------------------------------------*/
/*--- Allocation record                                            ---*/
/*--------------------------------------------------------------------*/

typedef enum {
    SS_ALLOC_HEAP,
    SS_ALLOC_GLOBAL,
    SS_ALLOC_STACK,
    SS_ALLOC_UNKNOWN
} SSAllocKind;

typedef struct _SSAlloc {
    struct _SSAlloc* ht_next;   /* Hash table linkage (must be first for VgHashTable) */
    UWord   key;                /* = base address (for hash table lookup) */

    Addr    base;
    SizeT   size;
    SSAllocKind kind;

    /* Thread set — threads that have accessed this allocation */
    ULong   thread_set;         /* Bitmask for TIDs 0-63 */
    UInt    thread_count;

    /* Allocation-site stack trace (for heap blocks) */
    Addr    alloc_stacktrace[SS_STACKTRACE_DEPTH];
    UInt    alloc_stacktrace_depth;

    /* Access records (linked list) */
    SSAccess* accesses;
    UInt      access_count;

    /* Section label active when first accessed */
    const HChar* section_label;

} SSAlloc;

/*--------------------------------------------------------------------*/
/*--- Monitoring section record                                    ---*/
/*--------------------------------------------------------------------*/

typedef struct _SSSection {
    const HChar* label;
    struct _SSSection* next;
} SSSection;

#endif /* SS_TYPES_H */
