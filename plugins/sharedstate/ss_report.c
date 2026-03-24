/*
 * ss_report.c -- Text and JSON report generation for the Shared-State Tracker.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_execontext.h"
#include "pub_tool_vki.h"

#include "ss_types.h"
#include "ss_alloc.h"
#include "ss_report.h"
#include "bridge_utils.h"

#define CC "ss_report"

/*--------------------------------------------------------------------*/
/*--- Helpers                                                      ---*/
/*--------------------------------------------------------------------*/

static const HChar* alloc_kind_str(SSAllocKind kind)
{
    switch (kind) {
        case SS_ALLOC_HEAP:    return "heap";
        case SS_ALLOC_GLOBAL:  return "global";
        case SS_ALLOC_STACK:   return "stack";
        case SS_ALLOC_UNKNOWN: return "unknown";
    }
    return "unknown";
}

/* Format an IP as "file:line (function)" or just "0xADDR" */
static void format_ip(Addr ip, HChar* buf, Int buflen)
{
    const HChar* file = NULL;
    const HChar* fn   = NULL;
    UInt line = 0;
    DiEpoch ep = VG_(current_DiEpoch)();

    Bool have_fn   = VG_(get_fnname)(ep, ip, &fn);
    Bool have_file = VG_(get_filename_linenum)(ep, ip, &file, NULL, &line);

    if (have_file && have_fn)
        VG_(snprintf)(buf, buflen, "%s:%u (%s)", file, line, fn);
    else if (have_fn)
        VG_(snprintf)(buf, buflen, "0x%lx (%s)", ip, fn);
    else if (have_file)
        VG_(snprintf)(buf, buflen, "%s:%u", file, line);
    else
        VG_(snprintf)(buf, buflen, "0x%lx", ip);
}

/*--------------------------------------------------------------------*/
/*--- Collecting shared allocations                                ---*/
/*--------------------------------------------------------------------*/

typedef struct {
    SSAlloc** items;
    UInt      count;
    UInt      capacity;
    UInt      total_threads;
} SharedCollector;

static Bool collect_shared(SSAlloc* alloc, void* opaque)
{
    SharedCollector* sc = (SharedCollector*)opaque;

    /* Skip if accessed by fewer than 2 threads */
    if (alloc->thread_count < 2)
        return True;

    if (sc->count >= sc->capacity) {
        UInt new_cap = sc->capacity == 0 ? 256 : sc->capacity * 2;
        SSAlloc** new_items = VG_(malloc)(CC, new_cap * sizeof(SSAlloc*));
        if (sc->items) {
            VG_(memcpy)(new_items, sc->items, sc->count * sizeof(SSAlloc*));
            VG_(free)(sc->items);
        }
        sc->items = new_items;
        sc->capacity = new_cap;
    }
    sc->items[sc->count++] = alloc;
    return True;
}

/* Count total unique threads across all shared allocations */
static UInt count_total_threads(SharedCollector* sc)
{
    ULong all_threads = 0;
    UInt i;
    for (i = 0; i < sc->count; i++)
        all_threads |= sc->items[i]->thread_set;

    UInt count = 0;
    while (all_threads) {
        count += all_threads & 1;
        all_threads >>= 1;
    }
    return count;
}

/*--------------------------------------------------------------------*/
/*--- Text report                                                  ---*/
/*--------------------------------------------------------------------*/

void ss_report_text(void)
{
    SharedCollector sc = { NULL, 0, 0, 0 };
    ss_alloc_iterate(collect_shared, &sc);

    if (sc.count == 0) {
        VG_(umsg)("\n");
        VG_(umsg)("=== Shared State Inventory ===\n");
        VG_(umsg)("No shared state detected.\n");
        VG_(umsg)("\n");
        if (sc.items) VG_(free)(sc.items);
        return;
    }

    sc.total_threads = count_total_threads(&sc);

    VG_(umsg)("\n");
    VG_(umsg)("=== Shared State Inventory ===\n");
    VG_(umsg)("\n");

    UInt i;
    for (i = 0; i < sc.count; i++) {
        SSAlloc* a = sc.items[i];
        HChar loc_buf[256];

        /* Header */
        if (a->alloc_stacktrace_depth > 0) {
            format_ip(a->alloc_stacktrace[0], loc_buf, sizeof(loc_buf));
        } else {
            VG_(snprintf)(loc_buf, sizeof(loc_buf), "unknown");
        }

        VG_(umsg)("[%u] %s: 0x%lx (%lu bytes, allocated at %s)\n",
                  i + 1,
                  alloc_kind_str(a->kind),
                  a->base, a->size, loc_buf);

        if (a->section_label)
            VG_(umsg)("    Section: \"%s\"\n", a->section_label);

        /* Thread list */
        VG_(umsg)("    Threads:");
        UInt t;
        for (t = 0; t < SS_MAX_THREADS_FAST; t++) {
            if (a->thread_set & (1ULL << t))
                VG_(umsg)(" T%u", t);
        }
        VG_(umsg)("\n");

        /* Accesses */
        VG_(umsg)("    Accesses:\n");
        SSAccess* acc = a->accesses;
        UInt acc_num = 0;
        while (acc && acc_num < 20) {  /* limit output */
            format_ip(acc->ip, loc_buf, sizeof(loc_buf));
            VG_(umsg)("      T%u %s at %s\n",
                      acc->tid,
                      acc->is_write ? "WRITE" : "READ ",
                      loc_buf);

            /* Stack trace */
            UInt s;
            for (s = 0; s < acc->stacktrace_depth && s < 6; s++) {
                HChar st_buf[256];
                format_ip(acc->stacktrace[s], st_buf, sizeof(st_buf));
                VG_(umsg)("        %s %s\n",
                          s == 0 ? " " : " ",
                          st_buf);
            }

            acc = acc->next;
            acc_num++;
        }
        if (acc)
            VG_(umsg)("      ... and more accesses (truncated)\n");

        VG_(umsg)("\n");
    }

    VG_(umsg)("Summary: %u shared objects found across %u threads\n",
              sc.count, sc.total_threads);
    VG_(umsg)("\n");

    if (sc.items) VG_(free)(sc.items);
}

/*--------------------------------------------------------------------*/
/*--- JSON report                                                  ---*/
/*--------------------------------------------------------------------*/

/* Write a string to an fd, properly escaping JSON special chars */
static void json_write(Int fd, const HChar* s)
{
    VG_(write)(fd, s, VG_(strlen)(s));
}

static void json_write_escaped_string(Int fd, const HChar* s)
{
    json_write(fd, "\"");
    while (*s) {
        switch (*s) {
            case '"':  json_write(fd, "\\\""); break;
            case '\\': json_write(fd, "\\\\"); break;
            case '\n': json_write(fd, "\\n");  break;
            case '\r': json_write(fd, "\\r");  break;
            case '\t': json_write(fd, "\\t");  break;
            default: {
                HChar c[2] = { *s, 0 };
                json_write(fd, c);
            }
        }
        s++;
    }
    json_write(fd, "\"");
}

void ss_report_json(void)
{
    SharedCollector sc = { NULL, 0, 0, 0 };
    ss_alloc_iterate(collect_shared, &sc);

    Int fd = VG_(fd_open)("bridge.out.json",
                          VKI_O_WRONLY | VKI_O_CREAT | VKI_O_TRUNC,
                          0644);
    if (fd < 0) {
        VG_(umsg)("Warning: could not create bridge.out.json\n");
        if (sc.items) VG_(free)(sc.items);
        return;
    }

    sc.total_threads = count_total_threads(&sc);

    json_write(fd, "{\n");
    json_write(fd, "  \"shared_objects\": [\n");

    UInt i;
    HChar buf[512];
    for (i = 0; i < sc.count; i++) {
        SSAlloc* a = sc.items[i];

        json_write(fd, "    {\n");

        VG_(snprintf)(buf, sizeof(buf), "      \"id\": %u,\n", i + 1);
        json_write(fd, buf);

        json_write(fd, "      \"type\": ");
        json_write_escaped_string(fd, alloc_kind_str(a->kind));
        json_write(fd, ",\n");

        VG_(snprintf)(buf, sizeof(buf),
                      "      \"base_addr\": \"0x%lx\",\n", a->base);
        json_write(fd, buf);

        VG_(snprintf)(buf, sizeof(buf),
                      "      \"size\": %lu,\n", a->size);
        json_write(fd, buf);

        /* Section label */
        if (a->section_label) {
            json_write(fd, "      \"section\": ");
            json_write_escaped_string(fd, a->section_label);
            json_write(fd, ",\n");
        }

        /* Alloc stack trace */
        json_write(fd, "      \"alloc_stacktrace\": [");
        UInt s;
        for (s = 0; s < a->alloc_stacktrace_depth; s++) {
            if (s > 0) json_write(fd, ", ");
            format_ip(a->alloc_stacktrace[s], buf, sizeof(buf));
            json_write_escaped_string(fd, buf);
        }
        json_write(fd, "],\n");

        /* Thread list */
        json_write(fd, "      \"threads\": [");
        Bool first = True;
        UInt t;
        for (t = 0; t < SS_MAX_THREADS_FAST; t++) {
            if (a->thread_set & (1ULL << t)) {
                if (!first) json_write(fd, ", ");
                VG_(snprintf)(buf, sizeof(buf), "%u", t);
                json_write(fd, buf);
                first = False;
            }
        }
        json_write(fd, "],\n");

        /* Accesses */
        json_write(fd, "      \"accesses\": [\n");
        SSAccess* acc = a->accesses;
        UInt acc_num = 0;
        while (acc) {
            if (acc_num > 0) json_write(fd, ",\n");

            json_write(fd, "        {\n");

            VG_(snprintf)(buf, sizeof(buf),
                          "          \"thread_id\": %u,\n", acc->tid);
            json_write(fd, buf);

            VG_(snprintf)(buf, sizeof(buf),
                          "          \"is_write\": %s,\n",
                          acc->is_write ? "true" : "false");
            json_write(fd, buf);

            format_ip(acc->ip, buf, sizeof(buf));
            json_write(fd, "          \"source\": ");
            json_write_escaped_string(fd, buf);
            json_write(fd, ",\n");

            json_write(fd, "          \"stacktrace\": [");
            for (s = 0; s < acc->stacktrace_depth; s++) {
                if (s > 0) json_write(fd, ", ");
                format_ip(acc->stacktrace[s], buf, sizeof(buf));
                json_write_escaped_string(fd, buf);
            }
            json_write(fd, "]\n");

            json_write(fd, "        }");
            acc = acc->next;
            acc_num++;
        }
        json_write(fd, "\n      ]\n");

        if (i + 1 < sc.count)
            json_write(fd, "    },\n");
        else
            json_write(fd, "    }\n");
    }

    json_write(fd, "  ],\n");

    VG_(snprintf)(buf, sizeof(buf),
                  "  \"summary\": {\n"
                  "    \"shared_object_count\": %u,\n"
                  "    \"thread_count\": %u\n"
                  "  }\n",
                  sc.count, sc.total_threads);
    json_write(fd, buf);
    json_write(fd, "}\n");

    VG_(close)(fd);
    VG_(umsg)("JSON report written to bridge.out.json\n");

    if (sc.items) VG_(free)(sc.items);
}
