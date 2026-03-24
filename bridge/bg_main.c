/*
 * bg_main.c -- Bridge tool entry point and VEX IR instrumentation.
 *
 * This is the core of the Bridge SDK.  It registers with Valgrind as
 * a tool, instruments every superblock to insert memory-access
 * callbacks, and dispatches all events to the active plugin.
 *
 * This file is part of the Valgrind Bridge SDK.
 * License: GPL-2.0-or-later
 */

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_machine.h"
#include "pub_tool_options.h"

#include "libvex_ir.h"

#include "bridge_plugin.h"

/* Defined in bg_helpers.c */
extern VG_REGPARM(3) void bg_helper_mem_access(Addr addr, SizeT size,
                                                 UWord is_write);

/* Defined in bg_threads.c */
extern void bg_threads_init(void);

/* Defined in bg_clientreqs.c */
extern Bool bg_handle_client_request(ThreadId tid, UWord* args, UWord* ret);

/*--------------------------------------------------------------------*/
/*--- Lifecycle callbacks                                          ---*/
/*--------------------------------------------------------------------*/

static void bg_post_clo_init(void)
{
    if (bg_active_plugin && bg_active_plugin->post_clo_init)
        bg_active_plugin->post_clo_init();
}

static void bg_fini(Int exitcode)
{
    if (bg_active_plugin && bg_active_plugin->finish)
        bg_active_plugin->finish(exitcode);
}

/*--------------------------------------------------------------------*/
/*--- Command-line option handling                                 ---*/
/*--------------------------------------------------------------------*/

static Bool bg_process_cmd_line_option(const HChar* arg)
{
    if (bg_active_plugin && bg_active_plugin->process_cmd_line_option)
        return bg_active_plugin->process_cmd_line_option(arg);
    return False;
}

static void bg_print_usage(void)
{
    if (bg_active_plugin && bg_active_plugin->print_usage)
        bg_active_plugin->print_usage();
}

static void bg_print_debug_usage(void)
{
    if (bg_active_plugin && bg_active_plugin->print_debug_usage)
        bg_active_plugin->print_debug_usage();
}

/*--------------------------------------------------------------------*/
/*--- Instrumentation (VEX IR → callback insertion)                ---*/
/*--------------------------------------------------------------------*/

/*
 * For each memory load or store in the superblock, insert a call to
 * bg_helper_mem_access(addr, size, is_write).
 *
 * We walk the flat IR statement list:
 *   Ist_WrTmp + Iex_Load  → load  (is_write = 0)
 *   Ist_Store              → store (is_write = 1)
 *   Ist_StoreG             → guarded store
 *   Ist_LoadG              → guarded load
 *   Ist_CAS                → compare-and-swap (both read+write)
 *   Ist_LLSC               → load-linked / store-conditional
 *
 * We skip Ist_Dirty (too complex / internal).
 */

/* Helper: insert a dirty call to bg_helper_mem_access */
static void add_mem_event(IRSB* sbOut, IRExpr* addr, Int size, Bool is_write)
{
    IRDirty* di;
    IRExpr** argv;

    argv = mkIRExprVec_3(
        addr,
        mkIRExpr_HWord((HWord)size),
        mkIRExpr_HWord((HWord)(is_write ? 1 : 0))
    );

    di = unsafeIRDirty_0_N(
        3,  /* regparms */
        "bg_helper_mem_access",
        VG_(fnptr_to_fnentry)(&bg_helper_mem_access),
        argv
    );

    addStmtToIRSB(sbOut, IRStmt_Dirty(di));
}

/* sizeofIRType is provided by libvex_ir.h */

static IRSB* bg_instrument(VgCallbackClosure* closure,
                            IRSB*              sbIn,
                            const VexGuestLayout*  layout,
                            const VexGuestExtents* vge,
                            const VexArchInfo*     archinfo_host,
                            IRType             gWordTy,
                            IRType             hWordTy)
{
    Int   i;
    IRSB* sbOut;
    IRTypeEnv* tyenv = sbIn->tyenv;

    if (gWordTy != hWordTy) {
        /* Cross-arch: punt and return unchanged */
        return sbIn;
    }

    sbOut = deepCopyIRSBExceptStmts(sbIn);

    for (i = 0; i < sbIn->stmts_used; i++) {
        IRStmt* st = sbIn->stmts[i];

        if (!st || st->tag == Ist_NoOp)
            continue;

        switch (st->tag) {

        case Ist_WrTmp: {
            IRExpr* data = st->Ist.WrTmp.data;
            if (data->tag == Iex_Load) {
                Int size = sizeofIRType(data->Iex.Load.ty);
                if (size > 0) {
                    add_mem_event(sbOut, data->Iex.Load.addr, size, False);
                }
            }
            addStmtToIRSB(sbOut, st);
            break;
        }

        case Ist_Store: {
            IRExpr* data = st->Ist.Store.data;
            IRType  ty   = typeOfIRExpr(tyenv, data);
            Int     size = sizeofIRType(ty);
            if (size > 0) {
                add_mem_event(sbOut, st->Ist.Store.addr, size, True);
            }
            addStmtToIRSB(sbOut, st);
            break;
        }

        case Ist_StoreG: {
            IRStoreG* sg  = st->Ist.StoreG.details;
            IRType    ty  = typeOfIRExpr(tyenv, sg->data);
            Int       size = sizeofIRType(ty);
            if (size > 0) {
                add_mem_event(sbOut, sg->addr, size, True);
            }
            addStmtToIRSB(sbOut, st);
            break;
        }

        case Ist_LoadG: {
            IRLoadG* lg   = st->Ist.LoadG.details;
            IRType   ty   = Ity_INVALID;
            switch (lg->cvt) {
                case ILGop_IdentV128: ty = Ity_V128; break;
                case ILGop_Ident64:   ty = Ity_I64;  break;
                case ILGop_Ident32:   ty = Ity_I32;  break;
                case ILGop_16Uto32:   ty = Ity_I16;  break;
                case ILGop_16Sto32:   ty = Ity_I16;  break;
                case ILGop_8Uto32:    ty = Ity_I8;   break;
                case ILGop_8Sto32:    ty = Ity_I8;   break;
                default: break;
            }
            Int size = sizeofIRType(ty);
            if (size > 0) {
                add_mem_event(sbOut, lg->addr, size, False);
            }
            addStmtToIRSB(sbOut, st);
            break;
        }

        case Ist_CAS: {
            IRCAS* cas = st->Ist.CAS.details;
            IRType ty  = typeOfIRExpr(tyenv, cas->dataLo);
            Int    size = sizeofIRType(ty);
            if (cas->dataHi != NULL)
                size *= 2;  /* double-element CAS */
            if (size > 0) {
                /* CAS is read + write */
                add_mem_event(sbOut, cas->addr, size, False);
                add_mem_event(sbOut, cas->addr, size, True);
            }
            addStmtToIRSB(sbOut, st);
            break;
        }

        case Ist_LLSC: {
            /* Load-linked (storedata==NULL) or store-conditional */
            if (st->Ist.LLSC.storedata == NULL) {
                /* Load-linked */
                IRType ty  = typeOfIRExpr(tyenv,
                    IRExpr_RdTmp(st->Ist.LLSC.result));
                Int    size = sizeofIRType(ty);
                if (size > 0)
                    add_mem_event(sbOut, st->Ist.LLSC.addr, size, False);
            } else {
                /* Store-conditional */
                IRType ty  = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
                Int    size = sizeofIRType(ty);
                if (size > 0)
                    add_mem_event(sbOut, st->Ist.LLSC.addr, size, True);
            }
            addStmtToIRSB(sbOut, st);
            break;
        }

        default:
            addStmtToIRSB(sbOut, st);
            break;
        }
    }

    return sbOut;
}

/*--------------------------------------------------------------------*/
/*--- Tool registration (pre_clo_init)                             ---*/
/*--------------------------------------------------------------------*/

static void bg_pre_clo_init(void)
{
    const HChar* plugin_name = "bridge";
    const HChar* plugin_ver  = "0.1";
    const HChar* plugin_desc =
        "Bridge SDK: generic Valgrind plugin framework";

    if (bg_active_plugin) {
        if (bg_active_plugin->name)
            plugin_name = bg_active_plugin->name;
        if (bg_active_plugin->version)
            plugin_ver = bg_active_plugin->version;
        if (bg_active_plugin->description)
            plugin_desc = bg_active_plugin->description;
    }

    VG_(details_name)(plugin_name);
    VG_(details_version)(plugin_ver);
    VG_(details_description)(plugin_desc);
    VG_(details_copyright_author)(
        "Bridge SDK (GPL-2.0-or-later)");
    VG_(details_bug_reports_to)(
        "https://github.com/valgrind-bridge/issues");

    VG_(basic_tool_funcs)(bg_post_clo_init, bg_instrument, bg_fini);

    VG_(needs_command_line_options)(
        bg_process_cmd_line_option,
        bg_print_usage,
        bg_print_debug_usage
    );

    VG_(needs_client_requests)(bg_handle_client_request);

    /* Register thread lifecycle tracking */
    bg_threads_init();

    /* Let the plugin do its own init */
    if (bg_active_plugin && bg_active_plugin->init)
        bg_active_plugin->init();
}

VG_DETERMINE_INTERFACE_VERSION(bg_pre_clo_init)
