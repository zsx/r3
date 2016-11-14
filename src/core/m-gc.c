//
//  File: %m-gc.c
//  Summary: "main memory garbage collection"
//  Section: memory
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The garbage collector is based on a conventional "mark and sweep":
//
//     https://en.wikipedia.org/wiki/Tracing_garbage_collection
//
// From an optimization perspective, there is an attempt to not incur
// function call overhead just to check if a GC-aware item has its
// SERIES_FLAG_MARK flag set.  So the flag is checked by a macro before making
// any calls to process the references inside of an item.
//
// "Shallow" marking only requires setting the flag, and is suitable for
// series like strings (which are not containers for other REBVALs).  In
// debug builds shallow marking is done with a function anyway, to give
// a place to put assertion code or set breakpoints to catch when a
// shallow mark is set (when that is needed).
//
// "Deep" marking was originally done with recursion, and the recursion
// would stop whenever a mark was hit.  But this meant deeply nested
// structures could quickly wind up overflowing the C stack.  Consider:
//
//     a: copy []
//     loop 200'000 [a: append/only copy [] a]
//     recycle
//
// The simple solution is that when an unmarked item is hit that it is
// marked and put into a queue for processing (instead of recursed on the
// spot.  This queue is then handled as soon as the marking stack is
// exited, and the process repeated until no more items are queued.
//
// Regarding the two stages:
//
// MARK -  Mark all series and gobs ("collectible values")
//         that can be found in:
//
//         Root Block: special structures and buffers
//         Task Block: special structures and buffers per task
//         Data Stack: current state of evaluation
//         Safe Series: saves the last N allocations
//
// SWEEP - Free all collectible values that were not marked.
//
// GC protection methods:
//
// KEEP flag - protects an individual series from GC, but
//     does not protect its contents (if it holds values).
//     Reserved for non-block system series.
//
// Root_Vars - protects all series listed. This list is
//     used by Sweep as the root of the in-use memory tree.
//     Reserved for important system series only.
//
// Task_Vars - protects all series listed. This list is
//     the same as Root, but per the current task context.
//
// Save_Series - protects temporary series. Used with the
//     SAVE_SERIES and UNSAVE_SERIES macros. Throws and errors
//     must roll back this series to avoid "stuck" memory.
//
// Safe_Series - protects last MAX_SAFE_SERIES series from GC.
//     Can only be used if no deeply allocating functions are
//     called within the scope of its protection. Not affected
//     by throws and errors.
//
// Data_Stack - all values in the data stack that are below
//     the TOP (DSP) are automatically protected. This is a
//     common protection method used by native functions.
//
// DONE flag - do not scan the series; it has no links.
//

#define REN_C_STDIO_OK
#include "sys-core.h"

#include "mem-pools.h" // low-level memory pool access
#include "mem-series.h" // low-level series memory access

#include "reb-evtypes.h"

//-- For Serious Debugging:
#ifdef WATCH_GC_VALUE
REBSER *Watcher = 0;
REBVAL *WatchVar = 0;
REBVAL *GC_Break_Point(REBVAL *val) {return val;}
#endif

// This can be put below
#ifdef WATCH_GC_VALUE
            if (Watcher && ser == Watcher)
                GC_Break_Point(val);

        // for (n = 0; n < depth * 2; n++) Prin_Str(" ");
        // Mark_Count++;
        // Print("Mark: %s %x", TYPE_NAME(val), val);
#endif

enum mem_dump_kind {
    REB_KIND_SERIES = REB_MAX + 4,
    REB_KIND_ARRAY,
    REB_KIND_CONTEXT,
    REB_KIND_KEYLIST,
    REB_KIND_VARLIST,
    REB_KIND_FIELD,
    REB_KIND_STU,
    REB_KIND_HASH,
    REB_KIND_CHUNK,
    REB_KIND_CALL,
    REB_KIND_ROUTINE_INFO,
    REB_KIND_DEV,
    REB_KIND_MAX
};

struct Reb_Mem_Dump {
    void *parent;
    FILE *out;
};

struct mark_stack_elem {
    REBARR *array;
    const REBARR *key_list;
    REBMDP *dump;
#ifndef NDEBUG
    int *guard;
#endif
};

struct mem_dump_entry {
    const void *addr;
    const char *name;
    const void *parent;
    const char *edge; /* name of the edge from parent to this ndoe */
    int kind;
    REBCNT size;
};

static void Dump_Mem_Entry(REBMDP *dump,
    const struct mem_dump_entry *entry)
{
    char n[8];
    if (!dump || !dump->out) return;
    if (entry->addr == entry->parent) return;
    if (entry->parent == NULL) {
        // Windows prints 00000 for NULL
        fprintf(dump->out, "%p,(nil),%d,%d,%s,%s\n",
            entry->addr,
            entry->kind,
            entry->size,
            entry->edge == NULL ? "(null)" : entry->edge,
            entry->name == NULL ? "(null)" : entry->name);
    }
    else {
        fprintf(dump->out, "%p,%p,%d,%d,%s,%s\n",
            entry->addr,
            entry->parent,
            entry->kind,
            entry->size,
            entry->edge == NULL? "(null)" : entry->edge,
            entry->name == NULL? "(null)" : entry->name);
    }
}

static void Dump_Mem_Comment(REBMDP *dump, const char *s)
{
    if (!dump) return;
    fprintf(dump->out, "#%s\n", s);
}

// was static, but exported for Ren/C
/* static void Queue_Mark_Value_Deep(const REBVAL *val, const void *parent, REBMDP *dump);*/

static void Push_Array_Marked_Deep(REBARR *array, const REBARR *key_listr, REBMDP *dump);

#ifndef NDEBUG
static void Mark_Series_Only_Debug_Core(REBSER *ser);
#endif

#ifndef NDEBUG
static void Panic_Mark_Stack(struct mark_stack_elem *elem)
{
    /* reference the freed guard to cause a crash and a backtrace */
    int i = *elem->guard;
}
#endif

//
//  Push_Array_Marked_Deep: C
// 
// Note: Call Mark_Array_Deep or Queue_Mark_Array_Deep instead!
// 
// Submits the block into the deferred stack to be processed later
// with Propagate_All_GC_Marks().  We have already set this series
// mark as it's now "spoken for".  (Though we haven't marked its
// dependencies yet, we want to prevent it from being wastefully
// submitted multiple times by another reference that would still
// see it as "unmarked".)
// 
// The data structure used for this processing is a stack and not
// a queue (for performance reasons).  But when you use 'queue'
// as a verb it has more leeway than as the CS noun, and can just
// mean "put into a list for later processing", hence macro names.
//
static void Push_Array_Marked_Deep(REBARR *array, const REBARR *key_list, REBMDP *dump)
{
    struct mark_stack_elem *elem;

#if !defined(NDEBUG)
    if (!IS_ARRAY_MANAGED(array)) {
        Debug_Fmt("Link to non-MANAGED item reached by GC");
        Panic_Array(array);
    }
#endif

    assert(GET_ARR_FLAG(array, SERIES_FLAG_ARRAY));

    if (GET_ARR_FLAG(array, CONTEXT_FLAG_STACK)) {
        //
        // If the array's storage was on the stack and that stack level has
        // been popped, its data has been nulled out, and the series only
        // exists to keep words or objects holding it from crashing.
        //
        if (!GET_ARR_FLAG(array, SERIES_FLAG_ACCESSIBLE))
            return;
    }

    // !!! Are there actually any "external" series that are value-bearing?
    // e.g. a REBSER node which has a ->data pointer to REBVAL[...] and
    // expects this to be managed with GC, even though if the REBSER is
    // GC'd it shouldn't free that data?
    //
    assert(!GET_ARR_FLAG(array, SERIES_FLAG_EXTERNAL));

    // set by calling macro (helps catch direct calls of this function)
    assert(IS_REBSER_MARKED(ARR_SERIES(array)));

    // Add series to the end of the mark stack series and update terminator

    if (SER_FULL(GC_Mark_Stack)) Extend_Series(GC_Mark_Stack, 8);

    elem = SER_AT(struct mark_stack_elem, GC_Mark_Stack, SER_LEN(GC_Mark_Stack));
    elem->array = array;
    elem->key_list = key_list;
    elem->dump = dump;
#ifndef NDEBUG
    elem->guard = cast(int*, malloc(sizeof(int)));
    free(elem->guard);
#endif

    SET_SERIES_LEN(GC_Mark_Stack, SER_LEN(GC_Mark_Stack) + 1);

    elem = SER_AT(struct mark_stack_elem, GC_Mark_Stack, SER_LEN(GC_Mark_Stack));
    elem->array = NULL;
    elem->key_list = NULL;
#ifndef NDEBUG
    elem->guard = NULL;
#endif
}


static void Propagate_All_GC_Marks(REBMDP *dump);

#ifndef NDEBUG
    static REBOOL in_mark = FALSE;
#endif


// Deferred form for marking series that prevents potentially overflowing the
// C execution stack.

inline static void Queue_Mark_Array_Deep_Full(REBARR *a,
    const char *name,
    const void *parent,
    const char *edge,
    const REBARR *keylist,
    int kind,
    REBMDP *dump) {
    if (kind != REB_KIND_KEYLIST) {
        struct mem_dump_entry tmp_entry = {
            a, name, parent, edge, kind, sizeof(REBARR) /* size is counted in the contained REBVALs */
        };
        Dump_Mem_Entry(dump, &tmp_entry);
    }
    else {
        struct mem_dump_entry tmp_entry = {
            a, name, parent, edge, kind, cast(REBCNT, sizeof(REBARR) + ARR_LEN(a) * sizeof(REBVAL))
        };
        Dump_Mem_Entry(dump, &tmp_entry);
    }
    if (NOT(IS_REBSER_MARKED(ARR_SERIES(a)))) {
        MARK_REBSER(ARR_SERIES(a));
        Push_Array_Marked_Deep(a, keylist, (kind == REB_KIND_KEYLIST) ? NULL : dump);
    }
}

inline static void Queue_Mark_Named_Array_Deep(REBARR *a,
    const char *name,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    Queue_Mark_Array_Deep_Full(a, name, parent, edge, NULL, REB_KIND_ARRAY, dump);
}

inline static void Queue_Mark_Array_Deep(REBARR *a,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    Queue_Mark_Named_Array_Deep(a, NULL, parent, edge, dump);
}

inline static void Queue_Mark_Named_Context_Deep(REBCTX *c,
    const char *name,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    struct mem_dump_entry tmp_entry = {
        c, name, parent, edge, REB_KIND_CONTEXT, 0
    };
    assert(GET_ARR_FLAG(CTX_VARLIST(c), ARRAY_FLAG_VARLIST));
    Queue_Mark_Array_Deep_Full(CTX_KEYLIST(c), NULL, c, "<keylist>", CTX_KEYLIST(c), REB_KIND_KEYLIST, dump);
    Queue_Mark_Array_Deep(CTX_VARLIST(c), c, "<varlist>", dump);
}

inline static void Queue_Mark_Context_Deep(REBCTX *c,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    Queue_Mark_Named_Context_Deep(c, NULL, parent, edge, dump);
}

inline static void Queue_Mark_Routine_Deep(REBRIN *r, const void *parent, const char *edge, REBMDP *dump);

inline static void Queue_Mark_Function_Deep(REBFUN *f,
    const void *parent,
    const char *edge,
    REBMDP *dump) {

    struct mem_dump_entry tmp_entry = {
        f, NULL, parent, edge, REB_FUNCTION, sizeof(REBARR) /* size is counted in the contained REBVALs */
    };
    Dump_Mem_Entry(dump, &tmp_entry);
    // Need to queue the mark of the array for the body--as trying
    // to mark the "singular" value directly could infinite loop.
    //
    Queue_Mark_Array_Deep(FUNC_VALUE(f)->payload.function.body_holder, f, "<body_holder>", dump);

    if (FUNC_META(f) != NULL)
        Queue_Mark_Context_Deep(FUNC_META(f), f, "<meta>", dump);

    // Of all the function types, only the routines and callbacks use
    // HANDLE! and must be explicitly pointed out in the body.
    //
    if (IS_FUNCTION_RIN(FUNC_VALUE(f)))
        Queue_Mark_Routine_Deep(VAL_FUNC_ROUTINE(FUNC_VALUE(f)), f, "<routine>", dump);
}

inline static void Queue_Mark_Named_Anything_Deep(REBSER *s,
    const char *name,
    const void *parent,
    const char *edge,
    REBMDP *dump) {

    struct mem_dump_entry tmp_entry = {
        s, name, parent, edge, REB_KIND_SERIES, cast(REBCNT, sizeof(REBSER) + SER_TOTAL(s))
    };
    Dump_Mem_Entry(dump, &tmp_entry);

    if (IS_REBSER_MARKED(s))
        return;

    // !!! Temporary: Does not support functions yet, so don't use a function
    // as a GC root!

    if (GET_SER_FLAG(s, ARRAY_FLAG_VARLIST))
        Queue_Mark_Context_Deep(AS_CONTEXT(s), s, "<context>", dump);
  /*  else if (GET_SER_FLAG(s, ARRAY_FLAG_PARAMLIST))
        Queue_Mark_Function_Deep(AS_FUNC(s));
    else */ if (Is_Array_Series(s))
        Queue_Mark_Array_Deep(AS_ARRAY(s), s, "<array>", dump);
    else
        MARK_REBSER(s);
}

inline static void Queue_Mark_Anything_Deep(REBSER *s,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    Queue_Mark_Named_Anything_Deep(s, NULL, parent, edge, dump);
}

// Non-Queued form for marking blocks.  Used for marking a *root set item*,
// don't recurse from within Mark_Value/Mark_Gob/Mark_Array_Deep/etc.

inline static void Mark_Array_Deep_Full(REBARR *a,
    const char *name,
    const void *parent,
    const char *edge,
    const REBARR *keylist,
    int kind,
    REBMDP *dump) {
    assert(!in_mark);
    Queue_Mark_Array_Deep_Full(a, name, parent, edge, keylist, kind, dump);
    Propagate_All_GC_Marks(dump);
}

inline static void Mark_Named_Array_Deep(REBARR *a,
    const char *name,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    Mark_Array_Deep_Full(a, name, parent, edge, NULL, REB_KIND_ARRAY, dump);
}

inline static void Mark_Array_Deep(REBARR *a,
    const void *parent,
    const char *edge,
    REBMDP *dump) {
    Mark_Named_Array_Deep(a, NULL, parent, edge, dump);
}

inline static void Mark_Context_Deep(REBCTX *c,
    const void *parent,
    const char *edge,
    REBMDP *dump)
{
    assert(!in_mark);
    Queue_Mark_Context_Deep(c, parent, edge, dump);
    Propagate_All_GC_Marks(dump);
}

// Non-Deep form of mark, to be used on non-BLOCK! series or a block series
// for which deep marking is known to be unnecessary.
//
inline static void Mark_Series_Only_Full(
    REBSER *s,
    const char *name,
    const void *parent,
    const char *edge,
    int kind,
    REBMDP *dump)
{
    struct mem_dump_entry tmp_entry = {
        s, name, parent, edge, kind,
        cast(REBCNT, GET_SER_FLAG(s, SERIES_FLAG_HAS_DYNAMIC) ?
            SER_TOTAL(s) + sizeof(REBSER) : sizeof(REBSER))
    };
    Dump_Mem_Entry(dump, &tmp_entry);
#if !defined(NDEBUG)
    if (NOT(IS_SERIES_MANAGED(s))) {
        Debug_Fmt("Link to non-MANAGED item reached by GC");
        Panic_Series(s);
    }
#endif
    if (NOT(IS_REBSER_MARKED(s))) {
        s->header.bits |= REBSER_REBVAL_FLAG_MARK;
    }
}

inline static void Mark_Named_Series_Only(
    REBSER *s,
    const char *name,
    const void *parent,
    const char *edge,
    REBMDP *dump)
{
    Mark_Series_Only_Full(s, NULL, parent, edge, REB_KIND_SERIES, dump);
}

inline static void Mark_Series_Only(
    REBSER *s,
    const void *parent,
    const char *edge,
    REBMDP *dump)
{
    Mark_Named_Series_Only(s, NULL, parent, edge, dump);
}

// Assertion for making sure that all the deferred marks have been propagated

#define ASSERT_NO_GC_MARKS_PENDING() \
    assert(SER_LEN(GC_Mark_Stack) == 0)


//
//  Queue_Mark_Gob_Deep: C
// 
// 'Queue' refers to the fact that after calling this routine,
// one will have to call Propagate_All_GC_Marks() to have the
// deep transitive closure be guaranteed fully marked.
// 
// Note: only referenced blocks are queued, the GOB structure
// itself is processed via recursion.  Deeply nested GOBs could
// in theory overflow the C stack.
//
static void Queue_Mark_Gob_Deep(REBGOB *gob, const char *name, const void *parent, const char *edge, REBMDP *dump)
{
    REBGOB **pane;
    REBCNT i;

    struct mem_dump_entry entry = {gob, name, parent, edge, REB_GOB, sizeof(REBGOB)};

    Dump_Mem_Entry(dump, &entry);

    if (IS_GOB_MARK(gob)) return;

    MARK_GOB(gob);

    if (GOB_PANE(gob)) {
        Mark_Series_Only(GOB_PANE(gob), gob, "<pane>", dump);
        pane = GOB_HEAD(gob);
        for (i = 0; i < GOB_LEN(gob); i++, pane++)
            Queue_Mark_Gob_Deep(*pane, NULL, GOB_PANE(gob), "<has>", dump);
    }

    if (GOB_PARENT(gob)) Queue_Mark_Gob_Deep(GOB_PARENT(gob), NULL, gob, "<parent>", dump);

    if (GOB_CONTENT(gob)) {
        const char *edge = "<content>";
        if (GOB_TYPE(gob) >= GOBT_IMAGE && GOB_TYPE(gob) <= GOBT_STRING)
            Mark_Series_Only_Full(GOB_CONTENT(gob), NULL, gob, edge, GOB_TYPE(gob) + REB_KIND_MAX, dump);
        else if (GOB_TYPE(gob) >= GOBT_DRAW && GOB_TYPE(gob) <= GOBT_EFFECT)
            Queue_Mark_Array_Deep_Full(AS_ARRAY(GOB_CONTENT(gob)), NULL, gob, edge, NULL, GOB_TYPE(gob) + REB_KIND_MAX, dump);
    }

    if (GOB_DATA(gob)) {
        enum Reb_Kind kind = REB_BLANK;
        const char *edge = "<gob-data>";
        struct mem_dump_entry entry = {GOB_DATA(gob), NULL, gob, edge, kind, sizeof(REBVAL)};
        switch (GOB_DTYPE(gob)) {
        case GOBD_INTEGER:
            kind = REB_INTEGER;
            // fall through
        case GOBD_NONE:
            kind = REB_BLANK;
            // fall through
        default:
            entry.kind = kind;
            Dump_Mem_Entry(dump, &entry);
            break;
        case GOBD_OBJECT:
            Queue_Mark_Context_Deep(AS_CONTEXT(GOB_DATA(gob)), gob, edge, dump);
            break;
        case GOBD_STRING:
        case GOBD_BINARY:
            Mark_Series_Only(GOB_DATA(gob), gob, edge, dump);
            break;
        case GOBD_BLOCK:
            Queue_Mark_Array_Deep(AS_ARRAY(GOB_DATA(gob)), gob, edge, dump);
        }
    }
}

inline static void Queue_Mark_Value_Deep(const RELVAL *val, const void *parent, const char *edge, REBMDP *dump);

//
//  Queue_Mark_Field_Deep: C
// 
// 'Queue' refers to the fact that after calling this routine,
// one will have to call Propagate_All_GC_Marks() to have the
// deep transitive closure be guaranteed fully marked.
// 
// Note: only referenced blocks are queued, fields that are structs
// will be processed via recursion.  Deeply nested structs could
// in theory overflow the C stack.
//
static void Queue_Mark_Field_Deep(
    struct Struct_Field *field,
    REBSER *data_bin,
    REBCNT offset,
    const void *parent,
    REBMDP *dump
) {
    struct mem_dump_entry entry = { field, cast(const char*, STR_HEAD(field->name)), parent, "<field>", REB_KIND_FIELD, 0 /* counted in fields already */ };
    Dump_Mem_Entry(dump, &entry);
    if (field->is_rebval) {
        //
        // !!! The FFI apparently can tunnel REBVALs through to callbacks.
        // They would generally appear as raw sizeof(REBVAL) blobs to the
        // C routines processing them.  The GC considers the REBVAL* to be
        // "live", and there may be an array of them...so they are marked
        // much as a REBARR would.
        //
        assert(field->type == FFI_TYPE_POINTER);
        assert(field->dimension % 4 == 0);
        assert(field->size == sizeof(REBVAL));

        REBCNT i;
        for (i = 0; i < field->dimension; i += 4) {
            RELVAL *value = cast(REBVAL*,
                SER_AT(
                    REBYTE,
                    data_bin,
                    offset + field->offset + i * field->size
                )
            );

            /* This could lead to an infinite recursive call to Queue_Mark_Field_Deep if this value refers back to this struct */
            if (field->done)
                Queue_Mark_Value_Deep(value, field, "<value>", dump);
        }
    }
    else if (field->type == FFI_TYPE_STRUCT) {
        assert(!field->is_rebval);
        Mark_Series_Only(field->fields, field, "<fields>", dump);
        Queue_Mark_Array_Deep(field->spec, field, "<spec>", dump);

        REBCNT i;
        for (i = 0; i < SER_LEN(field->fields); ++i) {
            struct Struct_Field *subfield
                = SER_AT(struct Struct_Field, field->fields, i);

            // !!! If offset doesn't reflect the actual offset of this field
            // inside the structure this will have to be revisited (it should
            // be because you need to be able to reuse schemas
            //
            assert(subfield->offset >= offset);

            Queue_Mark_Field_Deep(subfield, data_bin, subfield->offset, field, dump);
        }
    }
    else {
        // ignore primitive datatypes
    }

    if (field->name != NULL)
        Mark_Series_Only_Full(field->name, cast(const char*, STR_HEAD(field->name)), field, "<name>", REB_STRING, dump);
}

//
//  Queue_Mark_Routine_Deep: C
// 
// 'Queue' refers to the fact that after calling this routine,
// one will have to call Propagate_All_GC_Marks() to have the
// deep transitive closure completely marked.
// 
// Note: only referenced blocks are queued, the routine's RValue
// is processed via recursion.  Deeply nested RValue structs could
// in theory overflow the C stack.
//
static void Queue_Mark_Named_Routine_Deep(REBRIN *r, const char *name, const void *parent, const char *edge, REBMDP *dump)
{
    SET_RIN_FLAG(r, ROUTINE_FLAG_MARK);

    struct mem_dump_entry entry = { r, name, parent, "<routine>", REB_KIND_ROUTINE_INFO, sizeof(REBRIN)};
    Dump_Mem_Entry(dump, &entry);

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_MARK)) return;

    // Mark the descriptions for the return type and argument types.
    //
    // !!! This winds up being a bit convoluted, because an OBJECT!-like thing
    // is being implemented as a HANDLE! to a series, in order to get the
    // behavior of multiple references and GC'd when the last goes away.
    // This "schema" concept also allows the `ffi_type` descriptive structures
    // to be garbage collected.  Replace with OBJECT!s in the future.

    if (IS_HANDLE(&r->ret_schema)) {
        REBSER *schema = cast(REBSER*, VAL_HANDLE_DATA(&r->ret_schema));
        Mark_Series_Only(schema, r, "<ret-schema>", dump);
        Queue_Mark_Field_Deep(
            *SER_HEAD(struct Struct_Field*, schema), NULL, 0, schema, dump
        );
    }
    else // special, allows NONE (e.g. void return)
        assert(IS_INTEGER(&r->ret_schema) || IS_BLANK(&r->ret_schema));

    Queue_Mark_Array_Deep(r->args_schemas, r, "<args-schemas>", dump);
    REBCNT n;
    for (n = 0; n < ARR_LEN(r->args_schemas); ++n) {
        if (IS_HANDLE(ARR_AT(r->args_schemas, n))) {
            REBSER *schema
                = cast(REBSER*, VAL_HANDLE_DATA(ARR_AT(r->args_schemas, n)));
            Mark_Series_Only(schema, r->args_schemas, "<schema>", dump);
            Queue_Mark_Field_Deep(
                *SER_HEAD(struct Struct_Field*, schema), NULL, 0, schema, dump
            );
        }
        else
            assert(IS_INTEGER(ARR_AT(r->args_schemas, n)));
    }

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
        assert(r->cif == NULL);
        assert(r->args_fftypes == NULL);
    }
    else {
        // !!! r->cif should always be set to something in non-variadic
        // routines, but currently the implementation has to tolerate partially
        // formed routines...because evaluations are called during make-routine
        // before the CIF is ready to be created or not.
        //
        if (r->cif)
            Mark_Series_Only(r->cif, r, "<cif>", dump);
        if (r->args_fftypes)
            Mark_Series_Only(r->args_fftypes, r, "<args-fftypes>", dump);
    }

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_CALLBACK)) {
        REBFUN *cb_func = RIN_CALLBACK_FUNC(r);
        if (cb_func) {
            // Should take care of spec, body, etc.
            Queue_Mark_Array_Deep(FUNC_PARAMLIST(cb_func), r, "<callback>", dump);
        }
        else {
            // !!! There is a call during MAKE_Routine that does an evaluation
            // while creating a callback function, before the CALLBACK_FUNC
            // has been set.  If the garbage collector is invoked at that
            // time, this will happen.  This should be reviewed to see if
            // it can be done another way--e.g. by not making the relevant
            // series visible to the garbage collector via MANAGE_SERIES()
            // until fully constructed.
        }
    } else {
        if (RIN_LIB(r))
            Queue_Mark_Array_Deep(RIN_LIB(r), r, "<library>", dump);
        else {
            // may be null if called before the routine is fully constructed
            // !!! Review if this can be made not possible
        }
    }
    Dump_Mem_Comment(dump, "Done dumping Routine/Callback");
}

inline static void Queue_Mark_Routine_Deep(REBRIN *r, const void *parent, const char *edge, REBMDP *dump)
{
    Queue_Mark_Named_Routine_Deep(r, NULL, parent, edge, dump);
}

//
//  Queue_Mark_Event_Deep: C
// 
// 'Queue' refers to the fact that after calling this routine,
// one will have to call Propagate_All_GC_Marks() to have the
// deep transitive closure completely marked.
//
static void Queue_Mark_Event_Deep(const RELVAL *value, REBMDP *dump)
{
    REBREQ *req;

    if (
        IS_EVENT_MODEL(value, EVM_PORT)
        || IS_EVENT_MODEL(value, EVM_OBJECT)
        || (
            VAL_EVENT_TYPE(value) == EVT_DROP_FILE
            && GET_FLAG(VAL_EVENT_FLAGS(value), EVF_COPIED)
        )
    ) {
        // !!! Comment says void* ->ser field of the REBEVT is a "port or
        // object" but it also looks to store maps.  (?)
        //
        Queue_Mark_Array_Deep(AS_ARRAY(VAL_EVENT_SER(m_cast(RELVAL*, value))), value, "<port/object/ser>", dump);
    }

    if (IS_EVENT_MODEL(value, EVM_DEVICE)) {
        // In the case of being an EVM_DEVICE event type, the port! will
        // not be in VAL_EVENT_SER of the REBEVT structure.  It is held
        // indirectly by the REBREQ ->req field of the event, which
        // in turn possibly holds a singly linked list of other requests.
        req = VAL_EVENT_REQ(value);

        while (req) {
            // Comment says void* ->port is "link back to REBOL port object"
            if (req->port)
                Queue_Mark_Context_Deep(AS_CONTEXT(cast(REBSER*, req->port)), value, "<port>", dump);
            req = req->next;
        }
    }
}


//
//  Mark_Devices_Deep: C
// 
// Mark all devices. Search for pending requests.
// 
// This should be called at the top level, and as it is not
// 'Queued' it guarantees that the marks have been propagated.
//
static void Mark_Devices_Deep(REBMDP *dump)
{
    REBDEV **devices = Host_Lib->devices;

    int d;
    for (d = 0; d < RDI_MAX; d++) {
        REBREQ *req;
        REBDEV *dev = devices[d];
        if (!dev)
            continue;

        struct mem_dump_entry entry = { dev, NULL, NULL, "<dev>", REB_KIND_DEV, sizeof(REBDEV) };
        Dump_Mem_Entry(dump, &entry);

        for (req = dev->pending; req; req = req->next)
            if (req->port)
                Queue_Mark_Context_Deep(AS_CONTEXT(cast(REBSER*, req->port)), dev, "<req-port>", dump);
    }
}


//
//  Mark_Frame_Stack_Deep: C
// 
// Mark values being kept live by all call frames.  If a function is running,
// then this will keep the function itself live, as well as the arguments.
// There is also an "out" slot--which may point to an arbitrary REBVAL cell
// on the C stack.  The out slot is initialized to an END marker at the
// start of every function call, so that it won't be uninitialized bits
// which would crash the GC...but it must be turned into a value (or a void)
// by the time the function is finished running.
//
// Since function argument slots are not pre-initialized, how far the function
// has gotten in its fulfillment must be taken into account.  Only those
// argument slots through points of fulfillment may be GC protected.
// 
// This should be called at the top level, and not from inside a
// Propagate_All_GC_Marks().  All marks will be propagated.
//
static void Mark_Frame_Stack_Deep(REBMDP *dump)
{
    REBFRM *f = TG_Frame_Stack;
    // The GC must consider all entries, not just those that have been pushed
    // into active evaluation.
    //
    struct mem_dump_entry entry;
    
    entry.addr = f;
    entry.name = "TG_Do_Stack";
    entry.parent = NULL;
    entry.kind = REB_KIND_CALL;
    entry.edge = NULL,
    entry.size = 0; // on the stack
    Dump_Mem_Entry(dump, &entry);

    for (; f != NULL; f = f->prior) {
        assert(f->eval_type <= REB_MAX_VOID);

        // Should have taken care of reifying all the VALIST on the stack
        // earlier in the recycle process (don't want to create new arrays
        // once the recycling has started...)
        //
        assert(f->pending != VA_LIST_PENDING);

        ASSERT_ARRAY_MANAGED(f->source.array);
        Queue_Mark_Array_Deep(f->source.array, f, "<source-array>", dump);

        // END is possible, because the frame could be sitting at the end of
        // a block when a function runs, e.g. `do [zero-arity]`.  That frame
        // will stay on the stack while the zero-arity function is running.
        // The array still might be used in an error, so can't GC it.
        //
        if (f->value && NOT_END(f->value) && Is_Value_Managed(f->value))
            Queue_Mark_Value_Deep(f->value, f, "<value>", dump);

        if (f->specifier != SPECIFIED)
            Queue_Mark_Context_Deep(f->specifier, f, "<specifier>", dump);

        // For uniformity of assumption, f->out is always maintained as GC safe
        //
        if (!IS_END(f->out) && !IS_VOID_OR_SAFE_TRASH(f->out))
            Queue_Mark_Value_Deep(f->out, f, "<out>", dump); // never NULL

        if (NOT(Is_Any_Function_Frame(f))) {
            //
            // Consider something like `eval copy quote (recycle)`, because
            // while evaluating the group it has no anchor anywhere in the
            // root set and could be GC'd.  The Reb_Frame's array ref is it.
            //
            continue;
        }

        if (!IS_END(&f->cell) && !IS_VOID_OR_SAFE_TRASH(&f->cell))
            Queue_Mark_Value_Deep(&f->cell, f, "<cell>", dump);

        Queue_Mark_Array_Deep(FUNC_PARAMLIST(f->func), f, "<func>", dump); // never NULL

        // Need to keep the label symbol alive for error messages/stacktraces
        //
        Mark_Series_Only(f->label, f, "<label>", dump); // also never NULL

        // The subfeed may be in use by VARARGS!, and it may be either a
        // context or a single element array.  It will only be valid during
        // the function's actual running.
        //
        if (!Is_Function_Frame_Fulfilling(f)) {
            if (f->special->header.bits & NOT_END_MASK) {
                REBARR *subfeed = cast(REBARR*, f->special);

                if (GET_ARR_FLAG(subfeed, ARRAY_FLAG_VARLIST))
                    Queue_Mark_Context_Deep(AS_CONTEXT(subfeed), f, "<subfeed>", dump);
                else {
                    assert(ARR_LEN(subfeed) == 1);
                    Queue_Mark_Array_Deep(subfeed, f, "<subfeed>", dump);
                }
            }

            assert(IS_END(f->param)); // indicates function is running

            if (
                f->refine // currently allowed to be NULL
                && !IS_END(f->refine)
                && !IS_VOID_OR_SAFE_TRASH(f->refine)
                && Is_Value_Managed(f->refine)
            ) {
                Queue_Mark_Value_Deep(f->refine, f, "<refine>", dump);
            }
        }

        // We need to GC protect the values in the args no matter what,
        // but it might not be managed yet (e.g. could still contain garbage
        // during argument fulfillment).  But if it is managed, then it needs
        // to be handed to normal GC.
        //
        if (f->varlist != NULL && IS_ARRAY_MANAGED(f->varlist)) {
            assert(!IS_TRASH_DEBUG(ARR_AT(f->varlist, 0)));
            assert(GET_ARR_FLAG(f->varlist, ARRAY_FLAG_VARLIST));
            Queue_Mark_Context_Deep(AS_CONTEXT(f->varlist), f, "<varlist>", dump);
        }

        // (Although the above will mark the varlist, it may not mark the
        // values...because it may be a single element array that merely
        // points at the stackvars.  Queue_Mark_Context expects stackvars
        // to be marked separately.)

        // The slots may be stack based or dynamic.  Mark in use but only
        // as far as parameter filling has gotten (may be garbage bits
        // past that).  Note END values are possible in the course of
        // frame fulfillment in the middle of the args, so we go by the
        // END parameter.
        //
        // Refinements need special treatment, and also consideration
        // of if this is the "doing pickups" or not.  If doing pickups
        // then skip the cells for pending refinement arguments.
        //
        REBVAL *param = FUNC_PARAMS_HEAD(f->underlying);
        REBVAL *arg = f->args_head; // may be stack or dynamic
        while (NOT_END(param)) {
            if (!IS_END(arg) && !IS_VOID_OR_SAFE_TRASH(arg))
                Queue_Mark_Value_Deep(arg, f, "<arg>", dump);

            if (param == f->param && !f->doing_pickups)
                break; // protect arg for current param, but no further

            ++param;
            ++arg;
        }
        assert(IS_END(param) ? IS_END(arg) : TRUE); // may not enforce

        Propagate_All_GC_Marks(dump);
    }
}


//
//  Queue_Mark_Named_Value_Deep: C
// 
// This routine is not marked `static` because it is needed by
// Ren/C++ in order to implement its GC_Mark_Hook.
//
void Queue_Mark_Named_Value_Deep(const RELVAL *val, const char *name, const void *parent, const char *edge, REBMDP *dump)
{
    REBSER *ser = NULL;
    struct mem_dump_entry entry;
    enum Reb_Kind kind;

    // If this happens, it means somehow Recycle() got called between
    // when an `if (Do_XXX_Throws())` branch was taken and when the throw
    // should have been caught up the stack (before any more calls made).
    //
    assert(!THROWN(val));

    kind = VAL_TYPE(val);

    entry.addr = val;
    entry.name = name;
    entry.parent = parent;
    entry.kind = kind;
    entry.edge = edge;
    entry.size = sizeof(REBVAL);

    if (name == NULL && ANY_WORD(val)) {
        entry.name = cast(const char*, STR_HEAD(VAL_WORD_SPELLING(val)));
    }
    Dump_Mem_Entry(dump, &entry);


    switch (VAL_TYPE(val)) {
        case REB_0:
            //
            // Should not be possible, REB_0 instances should not exist or
            // be filtered out by caller.
            //
            panic (Error(RE_MISC));

        case REB_MAX_VOID:
            //
            // Critical error; the only array that can handle unsets are the
            // varlists of contexts, and they must do so before getting here.
            //
            panic (Error(RE_MISC));

        case REB_TYPESET:
            //
            // Not all typesets have symbols--only those that serve as the
            // keys of objects (or parameters of functions)
            //
            if (val->extra.key_spelling != NULL)
                Mark_Series_Only(val->extra.key_spelling, val, "<spelling>", dump);
            break;

        case REB_HANDLE:
            //
            // See %sys-handle.h for an explanation of the two different types
            // of HANDLE!; one uses a singular REBSER to participate in GC
            // and another is just an opaque pointer with no GC hook.
            //
            if (val->extra.singular) {
            #if !defined(NDEBUG)
                assert(ARR_LEN(val->extra.singular) == 1);
                RELVAL *h = ARR_HEAD(val->extra.singular);
                assert(IS_HANDLE(h));
                assert(h->extra.singular == val->extra.singular);
            #endif

                Mark_Series_Only(ARR_SERIES(val->extra.singular), val, "<handle>", dump);
            }
            break;

        case REB_DATATYPE:
            // Type spec is allowed to be NULL.  See %typespec.r file
            if (VAL_TYPE_SPEC(val))
                Queue_Mark_Array_Deep(VAL_TYPE_SPEC(val), val, "<spec>", dump);
            break;

        case REB_OBJECT:
        case REB_MODULE:
        case REB_PORT:
        case REB_FRAME:
        case REB_ERROR: {
            REBCTX *context = VAL_CONTEXT(val);

            assert(CTX_TYPE(context) == VAL_TYPE(val));
            assert(VAL_CONTEXT(CTX_VALUE(context)) == context);
            assert(VAL_CONTEXT_META(CTX_VALUE(context)) == CTX_META(context));

            Queue_Mark_Context_Deep(context, val, "<context>", dump);

            // !!! Currently a FRAME! has a keylist which is storing a non-
            // context block spec.  This will be changed to be compatible
            // with the meta on object keylists.
            //
            if (!IS_FRAME(val) && VAL_CONTEXT_META(val))
                Queue_Mark_Context_Deep(VAL_CONTEXT_META(val), val, "<meta>", dump);

            // For VAL_CONTEXT_FRAME, the FRM_CALL is either on the stack
            // (in which case it's already taken care of for marking) or it
            // has gone bad, in which case it should be ignored.

            break;
        }

        case REB_FUNCTION: {
            REBVAL *archetype = FUNC_VALUE(VAL_FUNC(val));

            assert(VAL_FUNC_PARAMLIST(val) == VAL_FUNC_PARAMLIST(archetype));
            assert(VAL_FUNC_BODY(val) == VAL_FUNC_BODY(archetype));
            Queue_Mark_Function_Deep(VAL_FUNC(val), val, "<func>", dump);
            if (VAL_BINDING(val) != NULL)
                Queue_Mark_Anything_Deep(ARR_SERIES(VAL_BINDING(val)), val, "<binding>", dump);

            // !!! Needs to mark the exit/binding...
            break;
        }

        case REB_VARARGS: {
            if (GET_VAL_FLAG(val, VARARGS_FLAG_NO_FRAME)) {
                //
                // A single-element shared series node is kept between
                // instances of the same vararg that was created with
                // MAKE ARRAY! - which fits compactly in a REBSER.
                //
                Queue_Mark_Array_Deep(VAL_VARARGS_ARRAY1(val), val, "<varargs-array1>", dump);
            }
            else {
                //
                // VARARGS! can wind up holding a pointer to a frame that is
                // not managed, because arguments are still being fulfilled
                // in the frame where the varargs lives.  This is a bit snakey,
                // but if that's the state it's in, then it need not worry
                // about GC protecting the frame...because it protects itself
                // so long as the function is running.  (If it tried to
                // protect it, then it could hit unfinished/corrupt arg cells)
                //
                REBARR *varlist = VAL_BINDING(val);
                if (GET_ARR_FLAG(varlist, ARRAY_FLAG_VARLIST)) {
                    if (IS_ARRAY_MANAGED(varlist)) {
                        REBCTX *context = AS_CONTEXT(varlist);
                        Queue_Mark_Context_Deep(context, val, "<binding>", dump);
                    }
                }
            }
            break;
        }

        case REB_WORD:  // (and also used for function STACK backtrace frame)
        case REB_SET_WORD:
        case REB_GET_WORD:
        case REB_LIT_WORD:
        case REB_REFINEMENT:
        case REB_ISSUE: {
            REBSTR *spelling = val->payload.any_word.spelling;
            const char *edge = "<bound-to>";

            // A word marks the specific spelling it uses, but not the canon
            // value.  That's because if the canon value gets GC'd, then
            // another value might become the new canon during that sweep.
            //
            Mark_Series_Only(spelling, val, "<spelling>", dump);

            // A GC cannot run during a binding process--which is the only
            // time a canon word's "index" field is allowed to be nonzero.
            //
            assert(
                !GET_SER_FLAG(spelling, STRING_FLAG_CANON)
                || (
                    spelling->misc.bind_index.high == 0
                    && spelling->misc.bind_index.low == 0
                )
            );

            // All bound words should keep their contexts from being GC'd...
            // even stack-relative contexts for functions.
            //
            if (GET_VAL_FLAG(val, VALUE_FLAG_RELATIVE)) {
                //
                // Marking the function's paramlist should be enough to
                // mark all the function's properties (there is an embedded
                // function value...)
                //
                REBFUN* func = VAL_WORD_FUNC(val);
                assert(GET_VAL_FLAG(val, WORD_FLAG_BOUND)); // should be set
                Queue_Mark_Array_Deep(FUNC_PARAMLIST(func), val, edge, dump);
            }
            else if (GET_VAL_FLAG(val, WORD_FLAG_BOUND)) {
                if (IS_SPECIFIC(val)) {
                    REBCTX* context = VAL_WORD_CONTEXT(const_KNOWN(val));
                    Queue_Mark_Context_Deep(context, val, edge, dump);
                }
                else {
                    // We trust that if a relative word's context needs to make
                    // it into the transitive closure, that will be taken care
                    // of by the array reference that holds it.
                    //
                    REBFUN* func = VAL_WORD_FUNC(val);
                    Queue_Mark_Array_Deep(FUNC_PARAMLIST(func), val, edge, dump);
                }
            }
            else if (GET_VAL_FLAG(val, WORD_FLAG_PICKUP)) {
                //
                // Special word class that might be seen on the stack during
                // a GC that's used by argument fulfillment when searching
                // for out-of-order refinements.  It holds two REBVAL*s
                // (for the parameter and argument of the refinement) and
                // both should be covered for GC already, because the
                // paramlist and arg variables are "in progress" for a call.
            }
            else {
                // The word is unbound...make sure index is 0 in debug build.
                //
            #if !defined(NDEBUG)
                assert(val->payload.any_word.index == 0);
            #endif
            }
            break; }

        case REB_BLANK:
        case REB_BAR:
        case REB_LIT_BAR:
        case REB_LOGIC:
        case REB_INTEGER:
        case REB_DECIMAL:
        case REB_PERCENT:
        case REB_MONEY:
        case REB_TIME:
        case REB_DATE:
        case REB_CHAR:
            break;

        case REB_PAIR: {
            REBVAL *key = PAIRING_KEY(val->payload.pair);
            Init_Header_Aliased( // will be read via REBSER
                &key->header,
                key->header.bits | REBSER_REBVAL_FLAG_MARK
            );
            break; }

        case REB_TUPLE:
            break;

        case REB_STRING:
        case REB_BINARY:
        case REB_FILE:
        case REB_EMAIL:
        case REB_URL:
        case REB_TAG:
        case REB_BITSET:
            ser = VAL_SERIES(val);
            assert(SER_WIDE(ser) <= sizeof(REBUNI));
            Mark_Series_Only(ser, val, "<series>", dump);
            break;

        case REB_IMAGE:
            Mark_Series_Only(VAL_SERIES(val), val, "<series>", dump);
            break;

        case REB_VECTOR:
            Mark_Series_Only(VAL_SERIES(val), val, "<series>", dump);
            break;

        case REB_BLOCK:
        case REB_GROUP:
        case REB_PATH:
        case REB_SET_PATH:
        case REB_GET_PATH:
        case REB_LIT_PATH: {
            if (IS_SPECIFIC(val)) {
                REBCTX *context = VAL_SPECIFIER(const_KNOWN(val));
                if (context != SPECIFIED)
                    Queue_Mark_Context_Deep(context, val, "<bound-to>", dump);
            }
            else {
                // We trust that if a relative array's context needs to make
                // it into the transitive closure, that will be taken care
                // of by a higher-up array reference that holds it.
                //
                REBFUN* func = VAL_RELATIVE(val);
                Queue_Mark_Array_Deep(FUNC_PARAMLIST(func), val, "<bound-to>", dump);
            }

            Queue_Mark_Array_Deep(VAL_ARRAY(val), val, "<contains>", dump);
            break;
        }

        case REB_MAP: {
            REBMAP* map = VAL_MAP(val);
            Queue_Mark_Array_Deep(MAP_PAIRLIST(map), val, "<contains>", dump);
            if (MAP_HASHLIST(map))
                Mark_Series_Only(MAP_HASHLIST(map), val, "<hash>", dump);
            break;
        }

        case REB_LIBRARY: {
            Queue_Mark_Array_Deep(VAL_LIBRARY(val), val, "<contains>", dump);
            REBCTX *meta = VAL_LIBRARY_META(val);
            if (meta != NULL)
                Queue_Mark_Context_Deep(meta, val, "<meta>", dump);
            break; }

        case REB_STRUCT:
            {
            // The struct gets its GC'able identity and is passable by one
            // pointer from the fact that it is a single-element array that
            // contains the REBVAL of the struct itself.  (Because it is
            // "singular" it is only a REBSER node--no data allocation.)
            //
            Queue_Mark_Array_Deep(VAL_STRUCT(val), val, "<contains>", dump);

            // Though the REBVAL payload carries the data series and offset
            // position of this struct into that data, the hierarchical
            // description of the structure's fields is stored in another
            // single element series--the "schema"--which is held in the
            // miscellaneous slot of the main array.
            //
            Mark_Series_Only(ARR_SERIES(VAL_STRUCT(val))->link.schema, val, "<link-schema>", dump);

            // The data series needs to be marked.  It needs to be marked
            // even for structs that aren't at the 0 offset--because their
            // lifetime can be longer than the struct which they represent
            // a "slice" out of.
            //
            Mark_Series_Only(VAL_STRUCT_DATA_BIN(val), val, "<data-bin>", dump);

            // The symbol needs to be GC protected, but only fields have them

            assert(VAL_STRUCT_SCHEMA(val)->name == NULL);

            // These series are backing stores for the `ffi_type` data that
            // is needed to use the struct with the FFI api.
            //
            Mark_Series_Only(VAL_STRUCT_SCHEMA(val)->fftype, val, "<fftype>", dump);
            Mark_Series_Only(VAL_STRUCT_SCHEMA(val)->fields_fftype_ptrs, val, "<fields-fftype-ptrs>", dump);

            // Recursively mark the schema and any nested structures (or
            // REBVAL-typed fields, specially recognized by the interface)
            //
            Queue_Mark_Field_Deep(
                VAL_STRUCT_SCHEMA(val),
                VAL_STRUCT_DATA_BIN(val),
                VAL_STRUCT_OFFSET(val),
                val,
                dump
            );
            }
            break;

        case REB_GOB:
            Queue_Mark_Gob_Deep(VAL_GOB(val), NULL, val, "<REBGOB>", dump);
            break;

        case REB_EVENT:
            Queue_Mark_Event_Deep(val, dump);
            break;

        default:
            panic (Error_Invalid_Datatype(VAL_TYPE(val)));
    }
}

inline static void Queue_Mark_Value_Deep(const RELVAL *val, const void *parent, const char *edge, REBMDP *dump)
{
    Queue_Mark_Named_Value_Deep(val, NULL, parent, edge, dump);
}

//
//  Mark_Array_Deep_Core: C
// 
// Mark all series reachable from the array.
//
// !!! At one time there was a notion of a "bare series" which would be marked
// to escape needing to be checked for GC--for instance because it only
// contained symbol words.  However skipping over the values is a limited
// optimization.  (For instance: symbols may become GC'd, and need to see the
// symbol references inside the values...or typesets might be expanded to
// contain dynamically allocated arrays of user types).
//
// !!! A more global optimization would be if there was a flag that was
// maintained about whether there might be any GC'able values in an array.
// It could start out saying there may be...but then if it did a visit and
// didn't see any mark it as not needing GC.  Modifications dirty that bit.
//
static void Mark_Array_Deep_Core(struct mark_stack_elem *elem, REBMDP *dump)
{
    REBCNT len;
    RELVAL *value, *key = NULL;
    REBARR *array = elem->array;
    const REBARR *keylist = elem->key_list;

    //printf("Marking array at %p\n", array);

#if !defined(NDEBUG)
    //
    // We should have marked this series at queueing time to keep it from
    // being doubly added before the queue had a chance to be processed
    //
    if (!IS_REBSER_MARKED(ARR_SERIES(array))) Panic_Array(array);

    // Make sure that a context's varlist wasn't marked without also marking
    // its keylist.  This could happen if Queue_Mark_Array_Deep is used on a
    // context instead of Queue_Mark_Context_Deep.
    //
    if (GET_ARR_FLAG(array, ARRAY_FLAG_VARLIST))
        assert(IS_REBSER_MARKED(ARR_SERIES(CTX_KEYLIST(AS_CONTEXT(array)))));
#endif

#ifdef HEAVY_CHECKS
    //
    // The GC is a good general hook point that all series which have been
    // managed will go through, so it's a good time to assert properties
    // about the array.
    //
    ASSERT_ARRAY(array);
#else
    //
    // For a lighter check, make sure it's marked as a value-bearing array
    // and that it hasn't been freed.
    //
    assert(GET_ARR_FLAG(array, SERIES_FLAG_ARRAY));
    assert(!IS_FREE_NODE(ARR_SERIES(array)));
#endif

    value = ARR_HEAD(array);
    if (keylist != NULL) {
        assert(ARR_LEN(array) == ARR_LEN(m_cast(REBARR*, keylist)));
        key = ARR_HEAD(m_cast(REBARR*, keylist));
    }

    for (; NOT_END(value); value++) {
        const char *name = NULL;
        if (IS_VOID_OR_SAFE_TRASH(value)) {
            //
            // Voids are illegal in most arrays, but the varlist of a context
            // uses void values to denote that the variable is not set.  Also
            // reified C va_lists as Do_Core() sources can have them.
            //
            assert(
                GET_ARR_FLAG(array, ARRAY_FLAG_VARLIST)
                || GET_ARR_FLAG(array, ARRAY_FLAG_VOIDS_LEGAL)
            );
        }
        else {
            if (dump && key != NULL) {
                switch (VAL_TYPE(key)) {
                case REB_TYPESET:
                case REB_WORD:
                    name = cast(const char*, VAL_WORD_SPELLING(key));
                    break;
                default:
                    if (key != ARR_HEAD(m_cast(REBARR*, keylist))) {// the first element could be function!, native!, etc for FRAMEs
                        printf("unexpected type: %d\n", VAL_TYPE(key));
                        fclose(dump->out);
                        Panic_Mark_Stack(elem);
                        //Panic_Array(array);
                    }
                }
                key++;
            }

            Queue_Mark_Named_Value_Deep(value, name, array, "<has>", dump);
        }
    }
}


//
//  Sweep_Series: C
// 
// Scans all series nodes (REBSER structs) in all segments that are part of
// the SER_POOL.  If a series had its lifetime management delegated to the
// garbage collector with MANAGE_SERIES(), then if it didn't get "marked" as
// live during the marking phase then free it.
//
ATTRIBUTE_NO_SANITIZE_ADDRESS static REBCNT Sweep_Series(void)
{
    REBCNT count = 0;

    // Optimization here depends on SWITCH of the concrete values of bits.
    //
    assert(
        (NOT_END_MASK == 0x1)
        && (CELL_MASK == 0x2)
        && (REBSER_REBVAL_FLAG_MANAGED == 0x4)
    );

    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg != NULL; seg = seg->next) {
        REBSER *s = cast(REBSER*, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[SER_POOL].units; n > 0; --n, ++s) {
            switch (s->header.bits & 0x7) {
            case 0:
                // Marked as an end, but not marked as a cell.  Only way this
                // should be able to happen is if this is a free node with
                // all header bits set to 0.
                //
                assert(IS_FREE_NODE(s));
                break;

            case 1:
                // Doesn't have CELL_MASK set, but not marked as an END.  This
                // is the state series start out in as unmanaged, where the
                // not end bit is merely indicating "not free". 
                //
                assert(!IS_SERIES_MANAGED(s));
                break;

            case 2:
                // CELL_MASK set and it's an END, REBSER_REBVAL_FLAG_MANAGED
                // is not set.  That's an "unmanaged pairing" whose key is
                // an END, which occurs in some API tracking cases.  It's a
                // REBSER node, but *not* a "series".
                //
                assert(!IS_SERIES_MANAGED(s));
                break;

            case 3:
                // CELL_MASK set and it's not an end, and also not managed.
                // So this is a pairing with some value key that is not
                // GC managed.  Skip it.
                //
                assert(!IS_SERIES_MANAGED(s));
                break;
            
            case 4:
                // A managed REBSER which has no cell mask and is marked as
                // an END.  This currently doesn't happen, because the not end
                // bit is set on series at creation time so the header isn't
                // all zero bits (which would be free).  But this could signal
                // some special condition in the future. 
                //
                assert(FALSE);
                break;

            case 5:
                // A managed REBSER which has no cell mask and is marked as
                // *not* an END.  This is the typical signature of what one
                // would call an "ordinary managed REBSER".  If it's marked,
                // leave it alone...else kill it.
                //
                assert(IS_SERIES_MANAGED(s));
                if (IS_REBSER_MARKED(s))
                    UNMARK_REBSER(s);
                else {
                    GC_Kill_Series(s);
                    ++count;
                }
                break;

            case 6:
                // The CELL_MASK is set, and it's an END, and it's managed.
                // Assume this is impossible until a case is found.
                //
                assert(FALSE);
                break;

            case 7:
                // CELL_MASK is set, so it's a pairing...and the key is not
                // an END, and it's managed.  Mark bit should be heeded. 
                //
                assert(IS_SERIES_MANAGED(s));
                if (IS_REBSER_MARKED(s))
                    UNMARK_REBSER(s);
                else {
                    Free_Node(SER_POOL, s); // Free_Pairing is for manuals
                    ++count;
                }
                break;
            }
        }
    }

    return count;
}

//
//  Snapshot_All_Functions: C
//
// This routine can be used to get a list of all the functions in the system
// at a given moment in time.  Be sure to protect this array from GC when
// enumerating if there is any chance the GC might run (e.g. if user code
// is called to process the function list)
//
ATTRIBUTE_NO_SANITIZE_ADDRESS REBARR *Snapshot_All_Functions(void)
{
    REBDSP dsp_orig = DSP;

    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg != NULL; seg = seg->next) {
        REBSER *s = cast(REBSER*, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[SER_POOL].units; n > 0; --n, ++s) {
            switch (s->header.bits & 0x7) {
            case 5:
                // A managed REBSER which has no cell mask and is marked as
                // *not* an END.  This is the typical signature of what one
                // would call an "ordinary managed REBSER".  (For the meanings
                // of other bits, see Sweep_Series.)
                //
                assert(IS_SERIES_MANAGED(s));
                if (
                    Is_Array_Series(s)
                    && GET_SER_FLAG(s, ARRAY_FLAG_PARAMLIST)
                ){
                    REBVAL *v = KNOWN(ARR_HEAD(AS_ARRAY(s)));
                    assert(IS_FUNCTION(v));
                    DS_PUSH(v);
                }
                break;
            }
        }
    }

    return Pop_Stack_Values(dsp_orig);
}

//
//  Mark_Root_Series: C
//
// In Ren-C, there is a concept of there being an open number of GC roots.
// Through the API, each cell held by a "paired" which is under GC management
// is considered to be a root.
//
// There is also a special ability of a paired, such that if the "key" is
// a frame with a certain bit set, then it will tie its lifetime to the
// lifetime of that frame on the stack.  (Not to the lifetime of the FRAME!
// value itself, which could be indefinite.)
//
ATTRIBUTE_NO_SANITIZE_ADDRESS static void Mark_Root_Series(REBMDP *dump)
{
    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg; seg = seg->next) {
        REBSER *s = cast(REBSER *, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[SER_POOL].units; n > 0; --n, ++s) {
            if (IS_FREE_NODE(s))
                continue;

            if (IS_REBSER_MARKED(s))
                continue;

            if (NOT(s->header.bits & REBSER_REBVAL_FLAG_ROOT))
                continue;

            // If something is marked as a root, then it has its contents
            // GC managed...even if it is not itself a candidate for GC.

            if (s->header.bits & CELL_MASK) {
                //
                // There is a special feature of root paired series, which
                // is that if the "key" is a frame marked in a certain way,
                // it will tie its lifetime to that of the execution of that
                // frame.  When the frame is done executing, it will no
                // longer preserve the paired.
                //
                // (Note: This does not have anything to do with the lifetime
                // of the FRAME! value itself, which could be indefinite.)
                //
                REBVAL *key = cast(REBVAL*, s);
                REBVAL *pairing = key + 1;
                if (
                    IS_FRAME(key)
                    && GET_VAL_FLAG(key, ANY_CONTEXT_FLAG_OWNS_PAIRED)
                    /*&& !Is_Context_Running_Or_Pending(VAL_CONTEXT(key))*/
                ){
                    Free_Pairing(key); // don't consider a root
                    continue;
                }

                // It's alive and a root.  Pick up its dependencies deeply.
                // Note that ENDs are allowed because for instance, a DO
                // might be executed with the pairing as the OUT slot (since
                // it is memory guaranteed not to relocate)
                //
                MARK_REBSER(s);
                Queue_Mark_Value_Deep(key, NULL, "<key>", dump);
                if (!IS_END(pairing))
                    Queue_Mark_Value_Deep(pairing, NULL, "<pairing>", dump);
            }
            else {
                // We have to do the queueing based on whatever type of series
                // this is.  So if it's a context, we have to get the
                // keylist...etc.
                //
                Queue_Mark_Anything_Deep(s, NULL, "<has>", dump);
            }
        }
    }
}


//
//  Sweep_Gobs: C
// 
// Free all unmarked gobs.
// 
// Scans all gobs in all segments that are part of the
// GOB_POOL. Free gobs that have not been marked.
//
ATTRIBUTE_NO_SANITIZE_ADDRESS static REBCNT Sweep_Gobs(void)
{
    REBSEG  *seg;
    REBGOB  *gob;
    REBCNT  n;
    REBCNT  count = 0;

    for (seg = Mem_Pools[GOB_POOL].segs; seg; seg = seg->next) {
        gob = (REBGOB *) (seg + 1);
        for (n = Mem_Pools[GOB_POOL].units; n > 0; --n, ++gob) {
            if (IS_FREE_NODE(gob)) // unused REBNOD
                continue;

            if (IS_GOB_MARK(gob))
                UNMARK_GOB(gob);
            else {
                Free_Gob(gob);
                count++;
            }
        }
    }

    return count;
}


//
//  Sweep_Routines: C
// 
// Free all unmarked routines.
// 
// Scans all routines in all segments that are part of the
// RIN_POOL. Free routines that have not been marked.
//
ATTRIBUTE_NO_SANITIZE_ADDRESS static REBCNT Sweep_Routines(void)
{
    REBCNT count = 0;

    REBSEG *seg;
    for (seg = Mem_Pools[RIN_POOL].segs; seg; seg = seg->next) {
        REBRIN *rin = cast(REBRIN*, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[RIN_POOL].units; n > 0; n--) {
            if (IS_FREE_NODE(rin))
                continue; // not used

            assert(GET_RIN_FLAG(rin, ROUTINE_FLAG_USED)); // redundant?
            if (GET_RIN_FLAG(rin, ROUTINE_FLAG_MARK))
                CLEAR_RIN_FLAG(rin, ROUTINE_FLAG_MARK);
            else {
                Free_Routine(rin);
                ++count;
            }

            ++rin;
        }
    }

    return count;
}


//
//  Propagate_All_GC_Marks: C
// 
// The Mark Stack is a series containing series pointers.  They
// have already had their SERIES_FLAG_MARK set to prevent being added
// to the stack multiple times, but the items they can reach
// are not necessarily marked yet.
// 
// Processing continues until all reachable items from the mark
// stack are known to be marked.
//
static void Propagate_All_GC_Marks(REBMDP *dump)
{
    assert(!in_mark);

    Dump_Mem_Comment(dump, "Progagate all GC marks");

    while (SER_LEN(GC_Mark_Stack) != 0) {
        SET_SERIES_LEN(GC_Mark_Stack, SER_LEN(GC_Mark_Stack) - 1); // still ok

        // Data pointer may change in response to an expansion during
        // Mark_Array_Deep_Core(), so must be refreshed on each loop.
        //
        REBARR *array;
        struct mark_stack_elem *elem, *last;

        elem = SER_AT(struct mark_stack_elem, GC_Mark_Stack, SER_LEN(GC_Mark_Stack));

        // Drop the series we are processing off the tail, as we could be
        // queuing more of them (hence increasing the tail).
        //

        last = elem + 1;

        last->array = NULL;
        last->key_list = NULL;

        Mark_Array_Deep_Core(elem, elem->dump);
    }
}

//
//  Dump_Memory_Usage: C
//
// Dump detailed memory usage to a file
//
void Dump_Memory_Usage(const REBCHR *path)
{
    REBMDP dump;
#ifdef TO_WINDOWS
    dump.out = _wfopen(cast(const wchar_t*, path), L"w");
#else
    dump.out = fopen(cast(const char*, path), "w");
#endif
    if (dump.out == NULL) {
        return;
    }
    dump.parent = NULL;
    Dump_Mem_Comment(&dump, "Addr,parent,type,size,name");

    Recycle_Core(FALSE, &dump);

    fclose(dump.out);
}

//
//  Recycle_Core: C
// 
// Recycle memory no longer needed.
//
REBCNT Recycle_Core(REBOOL shutdown, REBMDP *dump)
{
    REBINT n;
    REBCNT count;

    //Debug_Num("GC", GC_Disabled);

    ASSERT_NO_GC_MARKS_PENDING();

    // If disabled, exit now but set the pending flag.
    if (GC_Disabled || !GC_Active) {
        SET_SIGNAL(SIG_RECYCLE);
        //Print("pending");
        return 0;
    }

    // Some of the call stack frames may have been invoked with a C function
    // call that took a comma-separated list of REBVAL (the way printf works,
    // a variadic va_list).  These call frames have no REBARR series behind
    // them, but still need to be enumerated to protect the values coming up
    // in the later DO/NEXTs.  But enumerating a C va_list can't be undone;
    // the information were be lost if it weren't saved.  We "reify" the
    // va_list into a REBARR before we start the GC (as it makes new series).
    //
    {
        REBFRM *f = FS_TOP;
        for (; f != NULL; f = f->prior) {
            const REBOOL truncated = TRUE;
            if (f->flags.bits & DO_FLAG_VA_LIST)
                Reify_Va_To_Array_In_Frame(f, truncated); // see function
        }
    }

    if (Reb_Opts->watch_recycle) Debug_Str(cs_cast(BOOT_STR(RS_WATCH, 0)));

    GC_Disabled = 1;

#if !defined(NDEBUG)
    PG_Reb_Stats->Recycle_Counter++;
    PG_Reb_Stats->Recycle_Series = Mem_Pools[SER_POOL].free;

    PG_Reb_Stats->Mark_Count = 0;
#endif

    // WARNING: These terminate existing open blocks. This could
    // be a problem if code is building a new value at the tail,
    // but has not yet updated the TAIL marker.
    //
    TERM_ARRAY_LEN(BUF_EMIT, ARR_LEN(BUF_EMIT));
    TERM_ARRAY_LEN(BUF_COLLECT, ARR_LEN(BUF_COLLECT));

    // MARKING PHASE: the "root set" from which we determine the liveness
    // (or deadness) of a series.  If we are shutting down, we are freeing
    // *all* of the series that are managed by the garbage collector, so
    // we don't mark anything as live.
    //
    // !!! Should a root set be "frozen" that's not cleaned out until shutdown
    // time?  There used to be a SERIES_FLAG_KEEP, which was removed due
    // to its usages being bad (and having few flags at the time).  It could
    // be reintroduced in a more limited fashion for this purpose.

    if (!shutdown) {
        //
        // The data stack logic is that it is contiguous values that has no
        // REB_ENDs in it except at the series end.  Bumping up against that
        // END signal is how the stack knows when it needs to grow.  But every
        // drop of the stack doesn't clean up the value dropped--because the
        // values are not END markers, they are considered fine as far as the
        // stack is concerned to indicate unused capacity.  However, the GC
        // doesn't want to mark these "marker-only" values live.
        //
        REBVAL *stackval = DS_TOP;
        assert(IS_TRASH_DEBUG(&DS_Movable_Base[0]));
        while (stackval != &DS_Movable_Base[0]) {
            if (NOT(IS_VOID_OR_SAFE_TRASH(stackval)))
                Queue_Mark_Value_Deep(stackval, NULL, "<stackval>", dump);
            --stackval;
        }
        Propagate_All_GC_Marks(dump);

        REBSER **sp;
        REBVAL **vp;
        REBINT n;

        struct mem_dump_entry entry;


        // Mark symbol series.  These canon words for SYM_XXX are the only ones
        // that are never candidates for GC.  All other symbol series may
        // go away if no words, parameters, object keys, etc. refer to them.
        {
            REBSTR **canon = SER_HEAD(REBSTR*, PG_Symbol_Canons);
            assert(*canon == NULL); // SYM_0 is for all non-builtin words
            ++canon;
            for (; *canon != NULL; ++canon)
                Mark_Series_Only(*canon, NULL, "<symbol-canons>", dump);
        }

        // Mark all natives
        entry.name = "Natives";
        entry.edge = NULL;
        entry.addr = Natives;
        entry.parent = NULL;
        entry.kind = REB_KIND_ARRAY;
        entry.size = sizeof(Natives);
        Dump_Mem_Entry(dump, &entry);
        {
            REBCNT n;
            for (n = 0; n < NUM_NATIVES; ++n)
                Mark_Array_Deep(VAL_FUNC_PARAMLIST(&Natives[n]), Natives, "<has>", dump);
        }

        // Mark series that have been temporarily protected from garbage
        // collection with PUSH_GUARD_SERIES.  We have to check if the
        // series is a context (so the keylist gets marked) or an array (so
        // the values are marked), or if it's just a data series which
        // should just be marked shallow.
        //
        sp = SER_HEAD(REBSER*, GC_Series_Guard);
        entry.name = "GC_Series_Guard";
        entry.edge = NULL;
        entry.addr = GC_Series_Guard;
        entry.parent = NULL;
        entry.kind = REB_KIND_SERIES;
        entry.size = SER_TOTAL(GC_Series_Guard);
        Dump_Mem_Entry(dump, &entry);

        for (n = SER_LEN(GC_Series_Guard); n > 0; n--, sp++) {
            if (GET_SER_FLAG(*sp, ARRAY_FLAG_VARLIST))
                Mark_Context_Deep(AS_CONTEXT(*sp), GC_Series_Guard, "<has>", dump);
            else if (Is_Array_Series(*sp))
                Mark_Array_Deep(AS_ARRAY(*sp), GC_Series_Guard, "<has>", dump);
            else
                Mark_Series_Only(*sp, GC_Series_Guard, "<has>", dump);
        }

        // Mark value stack (temp-saved values):
        vp = SER_HEAD(REBVAL*, GC_Value_Guard);
        entry.name = "GC_Value_Guard";
        entry.addr = GC_Value_Guard;
        entry.parent = NULL;
        entry.edge = NULL;
        entry.kind = REB_KIND_SERIES;
        entry.size = SER_TOTAL(GC_Value_Guard);
        Dump_Mem_Entry(dump, &entry);

        for (n = SER_LEN(GC_Value_Guard); n > 0; n--, vp++) {
            if (NOT_END(*vp) && !IS_VOID_OR_SAFE_TRASH(*vp))
                Queue_Mark_Value_Deep(*vp, GC_Value_Guard, "<has>", dump);
            Propagate_All_GC_Marks(dump);
        }

        // Mark all root series:
        //
        Mark_Root_Series(dump);

        // Mark potential error object from callback!
        if (!IS_VOID_OR_SAFE_TRASH(&Callback_Error)) {
            assert(NOT(GET_VAL_FLAG(&Callback_Error, VALUE_FLAG_RELATIVE)));
            Queue_Mark_Value_Deep(&Callback_Error, "Callback-error", NULL, dump);
        }
        Propagate_All_GC_Marks(dump);

        // Mark all devices:
        Dump_Mem_Comment(dump, "Dumping all devices!");
        Mark_Devices_Deep(dump);
        Propagate_All_GC_Marks(dump);

        // Mark function call frames:
        Dump_Mem_Comment(dump, "Dumping function call frames");
        Mark_Frame_Stack_Deep(dump);
        Propagate_All_GC_Marks(dump);
    }

    // SWEEPING PHASE

    // this needs to run before Sweep_Series(), because Routine has series
    // with pointers, which can't be simply discarded by Sweep_Series
    count = Sweep_Routines();

    count += Sweep_Series();
    count += Sweep_Gobs();

    CHECK_MEMORY(4);

#if !defined(NDEBUG)
    // Compute new stats:
    PG_Reb_Stats->Recycle_Series = Mem_Pools[SER_POOL].free - PG_Reb_Stats->Recycle_Series;
    PG_Reb_Stats->Recycle_Series_Total += PG_Reb_Stats->Recycle_Series;
    PG_Reb_Stats->Recycle_Prior_Eval = Eval_Cycles;
#endif

    // Do not adjust task variables or boot strings in shutdown when they
    // are being freed.
    //
    if (!shutdown) {
        //
        // !!! This code was added by Atronix to deal with frequent garbage
        // collection, but the logic is not correct.  The issue has been
        // raised and is commented out pending a correct solution.
        //
        // https://github.com/zsx/r3/issues/32
        //
        /*if (GC_Ballast <= VAL_INT32(TASK_BALLAST) / 2
            && VAL_INT64(TASK_BALLAST) < MAX_I32) {
            //increasing ballast by half
            VAL_INT64(TASK_BALLAST) /= 2;
            VAL_INT64(TASK_BALLAST) *= 3;
        } else if (GC_Ballast >= VAL_INT64(TASK_BALLAST) * 2) {
            //reduce ballast by half
            VAL_INT64(TASK_BALLAST) /= 2;
        }

        // avoid overflow
        if (
            VAL_INT64(TASK_BALLAST) < 0
            || VAL_INT64(TASK_BALLAST) >= MAX_I32
        ) {
            VAL_INT64(TASK_BALLAST) = MAX_I32;
        }*/

        GC_Ballast = VAL_INT32(TASK_BALLAST);
        GC_Disabled = 0;

        if (Reb_Opts->watch_recycle)
            Debug_Fmt(cs_cast(BOOT_STR(RS_WATCH, 1)), count);
    }

    ASSERT_NO_GC_MARKS_PENDING();

    return count;
}


//
//  Recycle: C
// 
// Recycle memory no longer needed.
//
REBCNT Recycle(void)
{
    //printf("Recycle begins\n");
    // Default to not passing the `shutdown` flag.
    return Recycle_Core(FALSE, NULL);
}


//
//  Guard_Series_Core: C
//
void Guard_Series_Core(REBSER *series)
{
    // It would seem there isn't any reason to save a series from being
    // garbage collected if it is already invisible to the garbage
    // collector.  But some kind of "saving" feature which added a
    // non-managed series in as if it were part of the root set would
    // be useful.  That would be for cases where you are building a
    // series up from constituent values but might want to abort and
    // manually free it.  For the moment, we don't have that feature.
    ASSERT_SERIES_MANAGED(series);

    if (SER_FULL(GC_Series_Guard)) Extend_Series(GC_Series_Guard, 8);

    *SER_AT(
        REBSER*,
        GC_Series_Guard,
        SER_LEN(GC_Series_Guard)
    ) = series;

    SET_SERIES_LEN(GC_Series_Guard, SER_LEN(GC_Series_Guard) + 1);
}


//
//  Guard_Value_Core: C
//
void Guard_Value_Core(const RELVAL *value)
{
    // Cheap check; require that the value already contain valid data when
    // the guard call is made (even if GC isn't necessarily going to happen
    // immediately, and value could theoretically become valid before then.)
    //
    assert(
        IS_END(value)
        || IS_VOID_OR_SAFE_TRASH(value)
        || VAL_TYPE(value) < REB_MAX
    );

#ifdef STRESS_CHECK_GUARD_VALUE_POINTER
    //
    // Technically we should never call this routine to guard a value that
    // lives inside of a series.  Not only would we have to guard the
    // containing series, we would also have to lock the series from
    // being able to resize and reallocate the data pointer.  But this is
    // a somewhat expensive check, so it's only feasible to run occasionally.
    //
    ASSERT_NOT_IN_SERIES_DATA(value);
#endif

    if (SER_FULL(GC_Value_Guard)) Extend_Series(GC_Value_Guard, 8);

    *SER_AT(
        const RELVAL*,
        GC_Value_Guard,
        SER_LEN(GC_Value_Guard)
    ) = value;

    SET_SERIES_LEN(GC_Value_Guard, SER_LEN(GC_Value_Guard) + 1);
}


//
//  Init_GC: C
// 
// Initialize garbage collector.
//
void Init_GC(void)
{
    // TRUE when recycle is enabled (set by RECYCLE func)
    //
    GC_Active = FALSE;

    // GC disabled counter for critical sections.  Used liberally in R3-Alpha.
    // But with Ren-C's introduction of the idea that an allocated series is
    // not seen by the GC until such time as it gets the SERIES_FLAG_MANAGED flag
    // set, there are fewer legitimate justifications to disabling the GC.
    //
    GC_Disabled = 0;

    GC_Ballast = MEM_BALLAST;

    // Temporary series protected from GC. Holds series pointers.
    GC_Series_Guard = Make_Series(15, sizeof(REBSER *), MKS_NONE);

    // Temporary values protected from GC. Holds value pointers.
    GC_Value_Guard = Make_Series(15, sizeof(REBVAL *), MKS_NONE);

    // The marking queue used in lieu of recursion to ensure that deeply
    // nested structures don't cause the C stack to overflow.
    GC_Mark_Stack = Make_Series(100, sizeof(struct mark_stack_elem), MKS_NONE);
    TERM_SEQUENCE(GC_Mark_Stack);
}


//
//  Shutdown_GC: C
//
void Shutdown_GC(void)
{
    Free_Series(GC_Series_Guard);
    Free_Series(GC_Value_Guard);
    Free_Series(GC_Mark_Stack);
}
