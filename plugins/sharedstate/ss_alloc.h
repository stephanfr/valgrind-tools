/*
 * ss_alloc.h -- Allocation tracker interface.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#ifndef SS_ALLOC_H
#define SS_ALLOC_H

#include "ss_types.h"

/* Initialise the allocation tracker */
void ss_alloc_init(void);

/* Record a heap allocation */
void ss_alloc_new_block(Addr base, SizeT size, ThreadId tid);

/* Record a heap free */
void ss_alloc_free_block(Addr base);

/* Look up the allocation containing addr.
 * Returns NULL if no known allocation contains it. */
SSAlloc* ss_alloc_find(Addr addr);

/* Iterate over all tracked allocations.
 * callback returns True to continue, False to stop. */
void ss_alloc_iterate(Bool (*callback)(SSAlloc* alloc, void* opaque),
                      void* opaque);

/* Clean up */
void ss_alloc_destroy(void);

#endif /* SS_ALLOC_H */
