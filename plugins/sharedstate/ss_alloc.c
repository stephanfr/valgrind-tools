/*
 * ss_alloc.c -- Allocation tracker for the Shared-State Tracker plugin.
 *
 * Maintains a hash table of SSAlloc records, keyed by base address.
 * For a given memory address, we scan the table to find the allocation
 * that contains it (base <= addr < base+size).
 *
 * For globals and stack, we create pseudo-allocations on first access.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_stacktrace.h"

#include "ss_types.h"
#include "ss_alloc.h"

/* We use a VgHashTable keyed by SSAlloc.key = base address */
static VgHashTable* alloc_ht = NULL;

/* A secondary sorted array for range-based lookup */
typedef struct {
    Addr  base;
    SizeT size;
    SSAlloc* alloc;
} AllocRange;

/* Simple dynamic array for range lookups */
static AllocRange* range_array     = NULL;
static UInt        range_count     = 0;
static UInt        range_capacity  = 0;
static Bool        range_sorted    = False;

#define CC "ss_alloc"

/*--------------------------------------------------------------------*/
/*--- Helpers                                                      ---*/
/*--------------------------------------------------------------------*/

static void range_array_add(SSAlloc* alloc)
{
    if (range_count >= range_capacity) {
        UInt new_cap = range_capacity == 0 ? 1024 : range_capacity * 2;
        AllocRange* new_arr = VG_(malloc)(CC, new_cap * sizeof(AllocRange));
        if (range_array) {
            VG_(memcpy)(new_arr, range_array,
                        range_count * sizeof(AllocRange));
            VG_(free)(range_array);
        }
        range_array = new_arr;
        range_capacity = new_cap;
    }
    range_array[range_count].base  = alloc->base;
    range_array[range_count].size  = alloc->size;
    range_array[range_count].alloc = alloc;
    range_count++;
    range_sorted = False;
}

/* (range_cmp reserved for use if we switch to qsort) */

static void ensure_sorted(void)
{
    if (range_sorted) return;
    /* Simple insertion sort — allocations are added incrementally */
    UInt i, j;
    for (i = 1; i < range_count; i++) {
        AllocRange tmp = range_array[i];
        j = i;
        while (j > 0 && range_array[j-1].base > tmp.base) {
            range_array[j] = range_array[j-1];
            j--;
        }
        range_array[j] = tmp;
    }
    range_sorted = True;
}

/*--------------------------------------------------------------------*/
/*--- Public API                                                   ---*/
/*--------------------------------------------------------------------*/

void ss_alloc_init(void)
{
    alloc_ht = VG_(HT_construct)("ss_alloc_ht");
}

void ss_alloc_new_block(Addr base, SizeT size, ThreadId tid)
{
    SSAlloc* a = VG_(malloc)(CC, sizeof(SSAlloc));
    VG_(memset)(a, 0, sizeof(SSAlloc));
    a->key  = (UWord)base;
    a->base = base;
    a->size = size;
    a->kind = SS_ALLOC_HEAP;
    a->alloc_stacktrace_depth =
        VG_(get_StackTrace)(tid, a->alloc_stacktrace,
                            SS_STACKTRACE_DEPTH, NULL, NULL, 0);

    VG_(HT_add_node)(alloc_ht, a);
    range_array_add(a);
}

void ss_alloc_free_block(Addr base)
{
    /* Remove from hash table (but keep the record — we still want it
     * in the report if it was shared before being freed). */
    SSAlloc* a = VG_(HT_remove)(alloc_ht, (UWord)base);
    if (a) {
        /* Don't free the SSAlloc: it may still have shared accesses
         * that we want to report.  Mark it as freed by zeroing size. */
        /* Actually, keep it accessible in range_array too. */
    }
}

SSAlloc* ss_alloc_find(Addr addr)
{
    /* First: exact hash lookup (addr IS the base) */
    SSAlloc* a = VG_(HT_lookup)(alloc_ht, (UWord)addr);
    if (a && addr >= a->base && addr < a->base + a->size)
        return a;

    /* Binary search the range array */
    ensure_sorted();
    if (range_count == 0) return NULL;

    UInt lo = 0, hi = range_count;
    while (lo < hi) {
        UInt mid = lo + (hi - lo) / 2;
        if (range_array[mid].base + range_array[mid].size <= addr)
            lo = mid + 1;
        else if (range_array[mid].base > addr)
            hi = mid;
        else {
            /* range_array[mid].base <= addr
             *   && addr < range_array[mid].base + range_array[mid].size */
            return range_array[mid].alloc;
        }
    }

    return NULL;  /* Unknown allocation — caller may create a pseudo-alloc */
}

void ss_alloc_iterate(Bool (*callback)(SSAlloc* alloc, void* opaque),
                      void* opaque)
{
    UInt i;
    for (i = 0; i < range_count; i++) {
        if (!callback(range_array[i].alloc, opaque))
            break;
    }
}

static void free_alloc_node(void* node)
{
    SSAlloc* a = (SSAlloc*)node;
    SSAccess* acc = a->accesses;
    while (acc) {
        SSAccess* next = acc->next;
        VG_(free)(acc);
        acc = next;
    }
    VG_(free)(a);
}

void ss_alloc_destroy(void)
{
    /* Free the range array (just the index, not the nodes) */
    VG_(free)(range_array);
    range_array    = NULL;
    range_count    = 0;
    range_capacity = 0;

    /* Let HT_destruct walk and free all nodes + their access lists */
    if (alloc_ht) {
        VG_(HT_destruct)(alloc_ht, free_alloc_node);
        alloc_ht = NULL;
    }
}
