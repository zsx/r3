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
// Copyright 2012-2017 Rebol Open Source Contributors
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
// Today's garbage collector is based on a conventional "mark and sweep",
// of REBSER "nodes", which is how it was done in R3-Alpha:
//
//     https://en.wikipedia.org/wiki/Tracing_garbage_collection
//
// A REBVAL's "payload" and "extra" field may or may not contain pointers to
// REBSERs that the GC needs to be aware of.  Some small values like LOGIC!
// or INTEGER! don't, because they can fit the entirety of their data into the
// REBVAL's 4*sizeof(void) cell...though this would change if INTEGER! added
// support for arbitrary-sized-numbers.
//
// Some REBVALs embed REBSER pointers even when the payload would technically
// fit inside their cell.  They do this in order to create a level of
// indirection so that their data can be shared among copies of that REBVAL.
// For instance, HANDLE! does this.
//
// "Deep" marking in R3-Alpha was originally done with recursion, and the
// recursion would stop whenever a mark was hit.  But this meant deeply nested
// structures could quickly wind up overflowing the C stack.  Consider:
//
//     a: copy []
//     loop 200'000 [a: append/only copy [] a]
//     recycle
//
// The simple solution is that when an unmarked array is hit that it is
// marked and put into a queue for processing (instead of recursed on the
// spot).  This queue is then handled as soon as the marking call is exited,
// and the process repeated until no more items are queued.
//
// !!! There is actually not a specific list of roots of the garbage collect,
// so a first pass of all the REBSER nodes must be done to find them.  This is
// because with the redesigned "RL_API" in Ren-C, ordinary REBSER nodes do
// double duty as lifetime-managed containers for REBVALs handed out by the
// API--without requiring a separate series data allocation.  These could be
// in their own "pool", but that would prevent mingling and reuse among REBSER
// nodes used for other purposes.  Review in light of any new garbage collect
// approaches used.
//

#include "sys-core.h"

#include "mem-pools.h" // low-level memory pool access
#include "mem-series.h" // low-level series memory access

#include "sys-int-funcs.h"


//
// !!! In R3-Alpha, the core included specialized structures which required
// their own GC participation.  This is because rather than store their
// properties in conventional Rebol types (like an OBJECT!) they wanted to
// compress their data into a tighter bit pattern than that would allow.
//
// Ren-C has attempted to be increasingly miserly about bytes, and also
// added the ability for C extensions to hook the GC for a cleanup callback
// relating to HANDLE! for any non-Rebol types.  Hopefully this will reduce
// the desire to hook the core garbage collector more deeply.  If a tighter
// structure is desired, that can be done with a HANDLE! or BINARY!, so long
// as any Rebol series/arrays/contexts/functions are done with full values.
//
// Events, Devices, and Gobs are slated to be migrated to structures that
// lean less heavily on C structs and raw C pointers, and leverage higher
// level Rebol services.  So ultimately their implementations would not
// require including specialized code in the garbage collector.  For the
// moment, they still need the hook.
//

#include "reb-evtypes.h"
static void Queue_Mark_Event_Deep(const RELVAL *value);

#define IS_GOB_MARK(g) \
    GET_GOB_FLAG((g), GOBF_MARK)
#define MARK_GOB(g) \
    SET_GOB_FLAG((g), GOBF_MARK)
#define UNMARK_GOB(g) \
    CLR_GOB_FLAG((g), GOBF_MARK)
static void Queue_Mark_Gob_Deep(REBGOB *gob);
static REBCNT Sweep_Gobs(void);

static void Mark_Devices_Deep(void);


#ifndef NDEBUG
    static REBOOL in_mark = FALSE; // needs to be per-GC thread
#endif

#define ASSERT_NO_GC_MARKS_PENDING() \
    assert(SER_LEN(GC_Mark_Stack) == 0)


// Private routines for dealing with the GC mark bit.  Note that not all
// REBSERs are actually series at the present time, because some are
// "pairings".  Plus the name Mark_Rebser_Only helps drive home that it's
// not actually marking an "any_series" type (like array) deeply.
//
static inline void Mark_Rebser_Only(REBSER *s)
{
#if !defined(NDEBUG)
    if (NOT(IS_SERIES_MANAGED(s))) {
        printf("Link to non-MANAGED item reached by GC\n");
        panic (s);
    }
#endif
    assert(NOT_SER_FLAG(s, SERIES_FLAG_ARRAY));

    if (s->header.bits & SERIES_FLAG_FILE_LINE)
        s->link.filename->header.bits |= NODE_FLAG_MARKED;
    s->header.bits |= NODE_FLAG_MARKED;
}

static inline REBOOL Is_Rebser_Marked_Or_Pending(REBSER *rebser) {
    return LOGICAL(rebser->header.bits & NODE_FLAG_MARKED);
}

static inline REBOOL Is_Rebser_Marked(REBSER *rebser) {
    // ASSERT_NO_GC_MARKS_PENDING(); // overkill check, but must be true
    return LOGICAL(rebser->header.bits & NODE_FLAG_MARKED);
}

static inline void Unmark_Rebser(REBSER *rebser) {
    rebser->header.bits &= ~cast(REBUPT, NODE_FLAG_MARKED);
}


//
//  Queue_Mark_Array_Subclass_Deep: C
//
// Submits the array into the deferred stack to be processed later with
// Propagate_All_GC_Marks().  If it were not queued and just used recursion
// (as R3-Alpha did) then deeply nested arrays could overflow the C stack.
//
// Although there are subclasses of REBARR which have ->link and ->misc
// and other properties that must be marked, the subclass processing is done
// during the propagation.  This is to prevent recursion from within the
// subclass queueing routine itself.  Hence this routine is the workhorse for
// the subclasses, but there are type-checked specializations for clarity
// if you have a REBFUN*, REBCTX*, etc.
//
// (Note: The data structure used for this processing is a "stack" and not
// a "queue".  But when you use 'queue' as a verb, it has more leeway than as
// the CS noun, and can just mean "put into a list for later processing".)
//
static void Queue_Mark_Array_Subclass_Deep(REBARR *a)
{
#if !defined(NDEBUG)
    if (IS_FREE_NODE(a))
        panic (a);

    if (NOT_SER_FLAG(a, SERIES_FLAG_ARRAY))
        panic (a);

    if (!IS_ARRAY_MANAGED(a))
        panic (a);
#endif

    // A marked array doesn't necessarily mean all references reached from it
    // have been marked yet--it could still be waiting in the queue.  But we
    // don't want to wastefully submit it to the queue multiple times.
    //
    if (Is_Rebser_Marked_Or_Pending(SER(a)))
        return;

    SER(a)->header.bits |= NODE_FLAG_MARKED; // the up-front marking

    // Add series to the end of the mark stack series.  The length must be
    // maintained accurately to know when the stack needs to grow.
    //
    // !!! Should this use a "bumping a NULL at the end" technique to grow,
    // like the data stack?
    //
    if (SER_FULL(GC_Mark_Stack))
        Extend_Series(GC_Mark_Stack, 8);
    *SER_AT(REBARR*, GC_Mark_Stack, SER_LEN(GC_Mark_Stack)) = a;
    SET_SERIES_LEN(GC_Mark_Stack, SER_LEN(GC_Mark_Stack) + 1); // unterminated
}

inline static void Queue_Mark_Array_Deep(REBARR *a) {
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_VARLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PARAMLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PAIRLIST));

    if (GET_SER_FLAG(a, SERIES_FLAG_FILE_LINE))
        SER(a)->link.filename->header.bits |= NODE_FLAG_MARKED;

    Queue_Mark_Array_Subclass_Deep(a);
}

inline static void Queue_Mark_Context_Deep(REBCTX *c) {
    REBARR *a = CTX_VARLIST(c);
    assert(GET_SER_FLAG(a, ARRAY_FLAG_VARLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PARAMLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PAIRLIST));
    assert(NOT_SER_FLAG(a, SERIES_FLAG_FILE_LINE));

    Queue_Mark_Array_Subclass_Deep(a);

    // Further handling is in Propagate_All_GC_Marks() for ARRAY_FLAG_VARLIST
    // where it can safely call Queue_Mark_Context_Deep() again without it
    // being a recursion.  (e.g. marking the context for this context's meta)
}

inline static void Queue_Mark_Function_Deep(REBFUN *f) {
    REBARR *a = FUNC_PARAMLIST(f);
    assert(GET_SER_FLAG(a, ARRAY_FLAG_PARAMLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_VARLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PAIRLIST));
    assert(NOT_SER_FLAG(a, SERIES_FLAG_FILE_LINE));

    Queue_Mark_Array_Subclass_Deep(a);

    // Further handling is in Propagate_All_GC_Marks() for ARRAY_FLAG_PARAMLIST
    // where it can safely call Queue_Mark_Function_Deep() again without it
    // being a recursion.  (e.g. marking underlying function for this function)
}

inline static void Queue_Mark_Map_Deep(REBMAP *m) {
    REBARR *a = MAP_PAIRLIST(m);
    assert(GET_SER_FLAG(a, ARRAY_FLAG_PAIRLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PARAMLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_VARLIST));
    assert(NOT_SER_FLAG(a, SERIES_FLAG_FILE_LINE));


    Queue_Mark_Array_Subclass_Deep(a);

    // Further handling is in Propagate_All_GC_Marks() for ARRAY_FLAG_PAIRLIST
    // where it can safely call Queue_Mark_Map_Deep() again without it
    // being a recursion.  (e.g. marking underlying function for this function)
}


static void Queue_Mark_Opt_Value_Deep(const RELVAL *v);

// A singular array, if you know it to be singular, can be marked a little
// faster by avoiding a queue step for the array node or walk.
//
inline static void Queue_Mark_Singular_Array(REBARR *a) {
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PAIRLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_PARAMLIST));
    assert(NOT_SER_FLAG(a, ARRAY_FLAG_VARLIST));
    assert(NOT_SER_FLAG(a, SERIES_FLAG_FILE_LINE));

    assert(NOT_SER_INFO(a, SERIES_INFO_HAS_DYNAMIC));

    SER(a)->header.bits |= NODE_FLAG_MARKED;
    Queue_Mark_Opt_Value_Deep(ARR_HEAD(a));
}


//
//  Queue_Mark_Opt_Value_Deep: C
//
// This queues *optional* values, which may include void cells.  If a slot is
// not supposed to allow a void, use Queue_Mark_Value_Deep()
//
static void Queue_Mark_Opt_Value_Deep(const RELVAL *v)
{
    assert(NOT(in_mark));

    // If this happens, it means somehow Recycle() got called between
    // when an `if (Do_XXX_Throws())` branch was taken and when the throw
    // should have been caught up the stack (before any more calls made).
    //
    assert(NOT(v->header.bits & VALUE_FLAG_THROWN));

#if !defined(NDEBUG)
    if (IS_UNREADABLE_IF_DEBUG(v))
        return;
#endif

#if !defined(NDEBUG)
    in_mark = TRUE;
#endif

    // This switch is done via contiguous REB_XXX values, in order to
    // facilitate use of a "jump table optimization":
    //
    // http://stackoverflow.com/questions/17061967/c-switch-and-jump-tables
    //
    enum Reb_Kind kind = VAL_TYPE(v);
    switch (kind) {
    case REB_0:
        //
        // Should not be possible, REB_0 instances should not exist or
        // be filtered out by caller.
        //
        panic (v);

    case REB_FUNCTION: {
        REBFUN *func = VAL_FUNC(v);
        Queue_Mark_Function_Deep(func);

        if (VAL_BINDING(v) != NULL)
            Queue_Mark_Array_Subclass_Deep(VAL_BINDING(v));

    #if !defined(NDEBUG)
        //
        // Make sure the [0] slot of the paramlist holds an archetype that is
        // consistent with the paramlist itself.
        //
        REBVAL *archetype = FUNC_VALUE(func);
        assert(FUNC_PARAMLIST(func) == VAL_FUNC_PARAMLIST(archetype));
        assert(FUNC_BODY(func) == VAL_FUNC_BODY(archetype));

        // It would be prohibitive to do validity checks on the facade of
        // a function on each call to FUNC_FACADE, so it is checked here.
        //
        // Though a facade *may* be a paramlist, it could just be an array
        // that *looks* like a paramlist, holding the underlying function the
        // facade is "fronting for" in the head slot.  The facade must always
        // hold the same number of parameters as the underlying function.
        //
        REBARR *facade = SER(FUNC_PARAMLIST(func))->misc.facade;
        assert(IS_FUNCTION(ARR_HEAD(facade)));
        REBARR *underlying = ARR_HEAD(facade)->payload.function.paramlist;
        if (underlying != facade) {
            assert(NOT_SER_FLAG(facade, ARRAY_FLAG_PARAMLIST));
            assert(GET_SER_FLAG(underlying, ARRAY_FLAG_PARAMLIST));
            assert(ARR_LEN(facade) == ARR_LEN(underlying));
        }
    #endif
        break; }

    case REB_BAR:
    case REB_LIT_BAR:
        break;

    case REB_WORD:
    case REB_SET_WORD:
    case REB_GET_WORD:
    case REB_LIT_WORD:
    case REB_REFINEMENT:
    case REB_ISSUE: {
        REBSTR *spelling = v->payload.any_word.spelling;

        // A word marks the specific spelling it uses, but not the canon
        // value.  That's because if the canon value gets GC'd, then
        // another value might become the new canon during that sweep.
        //
        Mark_Rebser_Only(spelling);

        // A GC cannot run during a binding process--which is the only
        // time a canon word's "index" field is allowed to be nonzero.
        //
        assert(
            NOT_SER_INFO(spelling, STRING_INFO_CANON)
            || (
                spelling->misc.bind_index.high == 0
                && spelling->misc.bind_index.low == 0
            )
        );

        if (GET_VAL_FLAG(v, WORD_FLAG_BOUND)) {
            assert(v->payload.any_word.index != 0);

            if (GET_VAL_FLAG(v, VALUE_FLAG_RELATIVE)) {
                // Bound relative to a function, keep that function alive.
                //
                // (To turn a relative binding into a specific one, a
                // frame is needed from the BLOCK!/GROUP!/PATH! etc. that
                // holds a word instance.  So those frames are kept alive
                // by the REB_BLOCK/REB_GROUP/REB_PATH lines in this switch
                // where they mark their "binding" field.)
                //
                REBFUN* func = VAL_WORD_FUNC(v);
                Queue_Mark_Function_Deep(func);
            }
            else {
                // Bound to a specific context, keep that context alive.
                //
                REBCTX* context = VAL_WORD_CONTEXT(const_KNOWN(v));
                Queue_Mark_Context_Deep(context);
            }
        }
        else {
            // The word is unbound...make sure index is 0 in debug build.
            // (it can be left uninitialized in release builds, for now)
            //
            assert(!GET_VAL_FLAG(v, VALUE_FLAG_RELATIVE));
        #if !defined(NDEBUG)
            assert(v->payload.any_word.index == 0);
        #endif
        }
        break; }

    case REB_PATH:
    case REB_SET_PATH:
    case REB_GET_PATH:
    case REB_LIT_PATH:
    case REB_BLOCK:
    case REB_GROUP: {
        if (IS_SPECIFIC(v)) {
            REBSPC *specifier = VAL_SPECIFIER(const_KNOWN(v));
            if (specifier != SPECIFIED)
                Queue_Mark_Context_Deep(CTX(specifier));
        }
        else {
            // We trust that if a relative array's context needs to make
            // it into the transitive closure, that will be taken care
            // of by a higher-up array reference that holds it.
            //
            REBFUN* func = VAL_RELATIVE(v);
            Queue_Mark_Function_Deep(func);
        }

        Queue_Mark_Array_Deep(VAL_ARRAY(v));
        break; }

    case REB_BINARY:
    case REB_STRING:
    case REB_FILE:
    case REB_EMAIL:
    case REB_URL:
    case REB_TAG:
    case REB_BITSET: {
        REBSER *series = VAL_SERIES(v);
        assert(SER_WIDE(series) <= sizeof(REBUNI));
        Mark_Rebser_Only(series);
        break; }

    case REB_HANDLE: { // See %sys-handle.h
        REBARR *singular = v->extra.singular;
        if (singular == NULL) {
            //
            // This HANDLE! was created with Init_Handle_Simple.  There is
            // no GC interaction.
        }
        else {
            // Handle was created with Init_Handle_Managed.  It holds a
            // REBSER node that contains exactly one handle, and the actual
            // data for the handle lives in that shared location.  There is
            // nothing the GC needs to see inside a handle.
            //
            SER(singular)->header.bits |= NODE_FLAG_MARKED;

        #if !defined(NDEBUG)
            assert(ARR_LEN(singular) == 1);
            RELVAL *single = ARR_HEAD(singular);
            assert(IS_HANDLE(single));
            assert(single->extra.singular == v->extra.singular);
            if (v != single) {
                //
                // In order to make it clearer that individual handles do not
                // hold the shared data (there'd be no way to update all the
                // references at once), the data pointers in all but the
                // shared singular value are NULL.
                //
                if (GET_VAL_FLAG(v, HANDLE_FLAG_CFUNC))
                    assert(
                        IS_CFUNC_TRASH_DEBUG(v->payload.handle.data.cfunc)
                    );
                else
                    assert(
                        IS_POINTER_TRASH_DEBUG(v->payload.handle.data.pointer)
                    );
            }
        #endif
        }
        break; }

    case REB_IMAGE:
        Mark_Rebser_Only(VAL_SERIES(v));
        break;

    case REB_VECTOR:
        Mark_Rebser_Only(VAL_SERIES(v));
        break;

    case REB_BLANK:
    case REB_LOGIC:
    case REB_INTEGER:
    case REB_DECIMAL:
    case REB_PERCENT:
    case REB_MONEY:
    case REB_CHAR:
        break;

    case REB_PAIR: {
        //
        // Ren-C's PAIR! uses a special kind of REBSER that does no additional
        // memory allocation, but embeds two REBVALs in the REBSER itself.
        // A REBVAL has a REBUPT-sized header at the beginning of its struct,
        // just like a REBSER, and the NODE_FLAG_MARKED bit is a 0
        // if unmarked...so it can stealthily participate in the marking
        // process, as long as the bit is cleared at the end.
        //
        REBSER *pairing = cast(REBSER*, PAIRING_KEY(v->payload.pair));
        pairing->header.bits |= NODE_FLAG_MARKED; // read via REBSER
        break; }

    case REB_TUPLE:
    case REB_TIME:
    case REB_DATE:
        break;

    case REB_MAP: {
        REBMAP* map = VAL_MAP(v);
        Queue_Mark_Map_Deep(map);
        break;
    }

    case REB_DATATYPE:
        // Type spec is allowed to be NULL.  See %typespec.r file
        if (VAL_TYPE_SPEC(v))
            Queue_Mark_Array_Deep(VAL_TYPE_SPEC(v));
        break;

    case REB_TYPESET:
        //
        // Not all typesets have symbols--only those that serve as the
        // keys of objects (or parameters of functions)
        //
        if (v->extra.key_spelling != NULL)
            Mark_Rebser_Only(v->extra.key_spelling);
        break;

    case REB_VARARGS: {
        //
        // Binding may be NULL if the varargs was a MAKE VARARGS! and hasn't
        // been passed through any parameter.  Otherwise it is the frame
        // context where the param and arg live (possibly expired).
        //
        REBARR *binding = v->extra.binding;
        if (binding != NULL) {
            if (IS_ARRAY_MANAGED(binding))
                Queue_Mark_Context_Deep(CTX(v->extra.binding));
            else {
                // !!! Should assert that the binding is to a frame that is
                // in mid-fulfillment on the stack
            }
        }

        // The data feed is either a frame context or a singular block which
        // holds the shared index among all same varargs into that array.
        //
        REBARR *feed = v->payload.varargs.feed;
        assert(GET_SER_FLAG(feed, ARRAY_FLAG_VARLIST) || ARR_LEN(feed) == 1);
        if (IS_ARRAY_MANAGED(feed))
            Queue_Mark_Array_Subclass_Deep(feed);
        else {
            // !!! Should also assert that this is a frame in mid-fulfillment
            // on the stack.
            //
            assert(GET_SER_FLAG(feed, ARRAY_FLAG_VARLIST));
        }
        break; }

    case REB_OBJECT:
    case REB_FRAME:
    case REB_MODULE:
    case REB_ERROR:
    case REB_PORT: {
        REBCTX *context = VAL_CONTEXT(v);
        Queue_Mark_Context_Deep(context);

        // Currently the "binding" in a context is only used by FRAME! to
        // preserve the binding of the FUNCTION! value that spawned that
        // frame.  Currently that binding is typically NULL inside of a
        // function's REBVAL unless it is a definitional RETURN or LEAVE.
        //
        // !!! Expanded usages may be found in other situations that mix an
        // archetype with an instance (e.g. an archetypal function body that
        // could apply to any OBJECT!, but the binding cheaply makes it
        // a method for that object.)
        //
        REBARR *binding = VAL_BINDING(v);
        if (binding != NULL) {
            assert(CTX_TYPE(context) == REB_FRAME);

        #if !defined(NDEBUG)
            if (CTX_VARS_UNAVAILABLE(context)) {
                //
                // !!! It seems a bit wasteful to keep alive the binding of a
                // stack frame you can no longer get values out of.  But
                // However, FUNCTION-OF still works on a FRAME! value after
                // the function is finished, if the FRAME! value was kept.
                // And that needs to give back a correct binding.
                //
            }
            else {
                struct Reb_Frame *f = CTX_FRAME_IF_ON_STACK(context);
                if (f != NULL) // comes from execution, not MAKE FRAME!
                    assert(binding == f->binding);
            }
        #endif

            Queue_Mark_Array_Subclass_Deep(binding);
        }

        REBFUN *phase = v->payload.any_context.phase;
        if (phase != NULL) {
            if (CTX_TYPE(context) != REB_FRAME)
                panic (context);
            Queue_Mark_Function_Deep(phase);
        }

    #if !defined(NDEBUG)
        REBVAL *archetype = CTX_VALUE(context);
        assert(CTX_TYPE(context) == VAL_TYPE(v));
        assert(VAL_CONTEXT(archetype) == context);
        assert(VAL_CONTEXT_META(archetype) == CTX_META(context));
    #endif

        // Note: for VAL_CONTEXT_FRAME, the FRM_CALL is either on the stack
        // (in which case it's already taken care of for marking) or it
        // has gone bad, in which case it should be ignored.

        break; }

    case REB_GOB:
        Queue_Mark_Gob_Deep(VAL_GOB(v));
        break;

    case REB_EVENT:
        Queue_Mark_Event_Deep(v);
        break;

    case REB_STRUCT: {
        //
        // !!! The ultimate goal for STRUCT! is that it be part of the FFI
        // extension and fall into the category of a "user defined type".
        // This essentially means it would be an opaque variant of a context.
        // User-defined types aren't fully designed, so struct is achieved
        // through a hacky set of hooks for now...but it does use arrays in
        // a fairly conventional way that should translate to the user
        // defined type system once it exists.
        //
        // The struct gets its GC'able identity and is passable by one
        // pointer from the fact that it is a single-element array that
        // contains the REBVAL of the struct itself.  (Because it is
        // "singular" it is only a REBSER node--no data allocation.)
        //
        REBSTU *stu = v->payload.structure.stu;
        Queue_Mark_Array_Deep(stu);

        // The schema is the hierarchical description of the struct.
        //
        REBFLD *schema = SER(stu)->link.schema;
        Queue_Mark_Array_Deep(schema);

        // The data series needs to be marked.  It needs to be marked
        // even for structs that aren't at the 0 offset--because their
        // lifetime can be longer than the struct which they represent
        // a "slice" out of.
        //
        // Note this may be a singular array handle, or it could be a BINARY!
        //
        Mark_Rebser_Only(v->payload.structure.data);
        break; }

    case REB_LIBRARY: {
        Queue_Mark_Array_Deep(VAL_LIBRARY(v));
        REBCTX *meta = VAL_LIBRARY_META(v);
        if (meta != NULL)
            Queue_Mark_Context_Deep(meta);
        break; }

    case REB_MAX_VOID:
        //
        // Not an actual ANY-VALUE! "value", just a void cell.  Instead of
        // this "Opt"ional routine, use Queue_Mark_Value_Deep() on slots
        // that should not be void.
        //
        break;

    default:
        panic (v);
    }

#if !defined(NDEBUG)
    in_mark = FALSE;
#endif
}

inline static void Queue_Mark_Value_Deep(const RELVAL *v)
{
#if !defined(NDEBUG)
    if (IS_VOID(v))
        panic (v);
#endif
    Queue_Mark_Opt_Value_Deep(v);
}


//
//  Propagate_All_GC_Marks: C
//
// The Mark Stack is a series containing series pointers.  They have already
// had their SERIES_FLAG_MARK set to prevent being added to the stack multiple
// times, but the items they can reach are not necessarily marked yet.
//
// Processing continues until all reachable items from the mark stack are
// known to be marked.
//
static void Propagate_All_GC_Marks(void)
{
    assert(!in_mark);

    while (SER_LEN(GC_Mark_Stack) != 0) {
        SET_SERIES_LEN(GC_Mark_Stack, SER_LEN(GC_Mark_Stack) - 1); // still ok

        // Data pointer may change in response to an expansion during
        // Mark_Array_Deep_Core(), so must be refreshed on each loop.
        //
        REBARR *a = *SER_AT(REBARR*, GC_Mark_Stack, SER_LEN(GC_Mark_Stack));

        // Termination is not required in the release build (the length is
        // enough to know where it ends).  But overwrite with trash in debug.
        //
        TRASH_POINTER_IF_DEBUG(
            *SER_AT(REBARR*, GC_Mark_Stack, SER_LEN(GC_Mark_Stack))
        );

        // We should have marked this series at queueing time to keep it from
        // being doubly added before the queue had a chance to be processed
         //
        assert(Is_Rebser_Marked(SER(a)));

    #ifdef HEAVY_CHECKS
        //
        // The GC is a good general hook point that all series which have been
        // managed will go through, so it's a good time to assert properties
        // about the array.
        //
        ASSERT_ARRAY(a);
    #else
        //
        // For a lighter check, make sure it's marked as a value-bearing array
        // and that it hasn't been freed.
        //
        assert(GET_SER_FLAG(a, SERIES_FLAG_ARRAY));
        assert(!IS_FREE_NODE(SER(a)));
    #endif

        RELVAL *v = ARR_HEAD(a);

        if (GET_SER_FLAG(a, ARRAY_FLAG_PARAMLIST)) {
            //
            // These queueings cannot be done in Queue_Mark_Function_Deep
            // because of the potential for overflowing the C stack with calls
            // to Queue_Mark_Function_Deep.

            REBARR *body_holder = v->payload.function.body_holder;
            Queue_Mark_Singular_Array(body_holder);

            REBCTX *exemplar = SER(body_holder)->link.exemplar;
            if (exemplar != NULL)
                Queue_Mark_Context_Deep(exemplar);

            REBCTX *meta = SER(a)->link.meta;
            if (meta != NULL)
                Queue_Mark_Context_Deep(meta);

            REBARR *facade = SER(a)->misc.facade;
            Queue_Mark_Array_Subclass_Deep(facade);

            assert(IS_FUNCTION(v));
            assert(v->extra.binding == NULL); // archetypes have no binding
            ++v; // function archetype completely marked by this process
        }
        else if (GET_SER_FLAG(a, ARRAY_FLAG_VARLIST)) {
            //
            // These queueings cannot be done in Queue_Mark_Context_Deep
            // because of the potential for overflowing the C stack with calls
            // to Queue_Mark_Context_Deep.

            REBARR *keylist = SER(a)->link.keylist;
            assert(keylist == CTX_KEYLIST_RAW(CTX(a)));
            Queue_Mark_Array_Subclass_Deep(keylist); // might be paramlist

            REBCTX *meta = SER(keylist)->link.meta;
            if (meta != NULL)
                Queue_Mark_Context_Deep(meta);

            assert(ANY_CONTEXT(v));

            // Currently only FRAME! uses binding
            //
            assert(v->extra.binding == NULL || VAL_TYPE(v) == REB_FRAME);

            ++v; // context archtype completely marked by this process
        }
        else if (GET_SER_FLAG(a, ARRAY_FLAG_PAIRLIST)) {
            //
            // There was once a "small map" optimization that wouldn't
            // produce a hashlist for small maps and just did linear search.
            // @giuliolunati deleted that for the time being because it
            // seemed to be a source of bugs, but it may be added again...in
            // which case the hashlist may be NULL.
            //
            REBSER *hashlist = SER(a)->link.hashlist;
            assert(hashlist != NULL);

            Mark_Rebser_Only(hashlist);
        }

        if (GET_SER_INFO(a, SERIES_INFO_INACCESSIBLE)) {
            //
            // At present the only inaccessible arrays are expired frames of
            // functions with stack-bound arg and local lifetimes.  They are
            // just singular REBARRs with the FRAME! archetype value.
            //
            assert(GET_SER_FLAG(a, ARRAY_FLAG_VARLIST));
            assert(IS_FRAME(ARR_HEAD(a)));
            assert(GET_SER_INFO(a, CONTEXT_INFO_STACK));
            continue;
        }

        for (; NOT_END(v); ++v) {
            Queue_Mark_Opt_Value_Deep(v);
            //
        #if !defined(NDEBUG)
            //
            // Voids are illegal in most arrays, but the varlist of a context
            // uses void values to denote that the variable is not set.  Also
            // reified C va_lists as Do_Core() sources can have them.
            //
            if (NOT(IS_BLANK_RAW(v)) && IS_VOID(v)) {
                if(
                    !GET_SER_FLAG(a, ARRAY_FLAG_VARLIST)
                    && !GET_SER_FLAG(a, ARRAY_FLAG_VOIDS_LEGAL)
                )
                    panic(a);
            }
        #endif
        }
    }
}


//
//  Reify_Any_C_Valist_Frames: C
//
// Some of the call stack frames may have been invoked with a C function call
// that took a comma-separated list of REBVAL (the way printf works, a
// variadic "va_list").
//
// http://en.cppreference.com/w/c/variadic
//
// Although it's a list of REBVAL*, these call frames have no REBARR series
// behind.  Yet they still need to be enumerated to protect the values coming
// up in the later DO/NEXTs.  But enumerating a C va_list can't be undone.
// The REBVAL* is lost if it isn't saved, and these frames may be in
// mid-evaluation.
//
// Hence, the garbage collector has to "reify" the remaining portion of the
// va_list into a REBARR before starting the GC.  Then the rest of the
// evaluation happens on that array.
//
static void Reify_Any_C_Valist_Frames(void)
{
    // IMPORTANT: This must be done *before* any of the mark/sweep logic
    // begins, because it creates new arrays.  In the future it may be
    // possible to introduce new series in mid-garbage collection (which would
    // be necessary for an incremental garbage collector), but for now the
    // feature is not supported.
    //
    ASSERT_NO_GC_MARKS_PENDING();

    REBFRM *f = FS_TOP;
    for (; f != NULL; f = f->prior) {
        if (f->flags.bits & DO_FLAG_VA_LIST) {
            const REBOOL truncated = TRUE;
            Reify_Va_To_Array_In_Frame(f, truncated);
        }
    }
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
static void Mark_Root_Series(void)
{
    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg; seg = seg->next) {
        REBSER *s = cast(REBSER *, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[SER_POOL].units; n > 0; --n, ++s) {
            if (IS_FREE_NODE(s))
                continue;

            assert(NOT(Is_Rebser_Marked(s))); // can't be marked yet

            if (NOT(s->header.bits & NODE_FLAG_ROOT))
                continue;

            // If something is marked as a root, then it has its contents
            // GC managed...even if it is not itself a candidate for GC.

            if (s->header.bits & NODE_FLAG_CELL) {
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
                // !!! Does it need to check for pending?  Could it be set
                // up such that you can't make an owning frame that's in
                // a pending state?
                //
                REBVAL *key = cast(REBVAL*, s);
                REBVAL *paired = key + 1;
                if (
                    IS_FRAME(key)
                    && GET_VAL_FLAG(key, ANY_CONTEXT_FLAG_OWNS_PAIRED)
                    && !Is_Context_Running_Or_Pending(VAL_CONTEXT(key))
                ){
                    Free_Pairing(paired); // don't consider a root
                    continue;
                }

                // It's alive and a root.  Pick up its dependencies deeply.
                // Note that ENDs are allowed because for instance, a DO
                // might be executed with the pairing as the OUT slot (since
                // it is memory guaranteed not to relocate)
                //
                Mark_Rebser_Only(s);
                Queue_Mark_Value_Deep(key);
                if (NOT_END(paired))
                    Queue_Mark_Value_Deep(paired);
            }
            else {
                // We have to do the queueing based on whatever type of series
                // this is.  So if it's a context, we have to get the
                // keylist...etc.
                //
                if (GET_SER_FLAG(s, SERIES_FLAG_ARRAY))
                    Queue_Mark_Array_Subclass_Deep(ARR(s));
                else
                    Mark_Rebser_Only(s);
            }
        }
    }

    Propagate_All_GC_Marks();
}


//
//  Mark_Data_Stack: C
//
// The data stack logic is that it is contiguous values with no END markers
// except at the array end.  Bumping up against that END signal is how the
// stack knows when it needs to grow.
//
// But every drop of the stack doesn't overwrite the dropped value.  Since the
// values are not END markers, they are considered fine as far as a NOT_END()
// test is concerned to indicate unused capacity.  So the values are good
// for the testing purpose, yet the GC doesn't want to consider those to be
// "live" references.  So rather than to a full Queue_Mark_Array_Deep() on
// the capacity of the data stack's underlying array, it begins at DS_TOP.
//
static void Mark_Data_Stack(void)
{
    assert(IS_UNREADABLE_IF_DEBUG(&DS_Movable_Base[0]));

    REBVAL *stackval = DS_TOP;
    for (; stackval != &DS_Movable_Base[0]; --stackval) {
        //
        // During path evaluation, function refinements are pushed to the
        // data stack as WORD!.  If the order of definition of refinements
        // in the function spec doesn't match the order of usage, then the
        // refinement will need to be revisited.  The WORD! is converted
        // into a "pickup" which stores the parameter and argument position.
        // These are only legal on the data stack, and are skipped by the GC.
        //
        if (VAL_TYPE(stackval) == REB_0_PICKUP)
            continue;

        Queue_Mark_Value_Deep(stackval);
    }

    Propagate_All_GC_Marks();
}


//
//  Mark_Symbol_Series: C
//
// Mark symbol series.  These canon words for SYM_XXX are the only ones that
// are never candidates for GC (until shutdown).  All other symbol series may
// go away if no words, parameters, object keys, etc. refer to them.
//
static void Mark_Symbol_Series(void)
{
    REBSTR **canon = SER_HEAD(REBSTR*, PG_Symbol_Canons);
    assert(IS_POINTER_TRASH_DEBUG(*canon)); // SYM_0 is for all non-builtin words
    ++canon;
    for (; *canon != NULL; ++canon)
        Mark_Rebser_Only(*canon);

    ASSERT_NO_GC_MARKS_PENDING(); // doesn't ues any queueing
}


//
//  Mark_Natives: C
//
// For each native C implemenation, a REBVAL is created during init to
// represent it as a FUNCTION!.  These are kept in a global array and are
// protected from GC.  It might not technically be necessary to do so for
// all natives, but at least some have their paramlists referenced by the
// core code (such as RETURN).
//
static void Mark_Natives(void)
{
    REBCNT n;
    for (n = 0; n < NUM_NATIVES; ++n)
        Queue_Mark_Value_Deep(&Natives[n]);

   Propagate_All_GC_Marks();
}


//
//  Mark_Guarded_Nodes: C
//
// Mark series and values that have been temporarily protected from garbage
// collection with PUSH_GUARD_SERIES and PUSH_GUARD_VALUE.
//
// Note: If the REBSER is actually a REBCTX, REBFUN, or REBARR then the
// reachable values for the series will be guarded appropriate to its type.
// (e.g. guarding a REBSER of an array will mark the values in that array,
// not just shallow mark the REBSER node)
//
static void Mark_Guarded_Nodes(void)
{
    REBNOD **np = SER_HEAD(REBNOD*, GC_Guarded);
    REBCNT n = SER_LEN(GC_Guarded);
    for (; n > 0; --n, ++np) {
        REBNOD *node = *np;
        if (node->header.bits & NODE_FLAG_CELL) { // a value cell
            if (NOT(node->header.bits & NODE_FLAG_END))
                Queue_Mark_Opt_Value_Deep(cast(REBVAL*, node));
        }
        else { // a series
            REBSER *s = cast(REBSER*, node);
            if (GET_SER_FLAG(s, SERIES_FLAG_ARRAY))
                Queue_Mark_Array_Subclass_Deep(ARR(s));
            else
                Mark_Rebser_Only(s);
        }
        Propagate_All_GC_Marks();
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
static void Mark_Frame_Stack_Deep(void)
{
    REBFRM *f = TG_Frame_Stack;

    for (; f != NULL; f = f->prior) {
        assert(f->eval_type <= REB_MAX_VOID);

        // Should have taken care of reifying all the VALIST on the stack
        // earlier in the recycle process (don't want to create new arrays
        // once the recycling has started...)
        //
        assert(f->pending != VA_LIST_PENDING);

        ASSERT_ARRAY_MANAGED(f->source.array);
        Queue_Mark_Array_Deep(f->source.array);

        // END is possible, because the frame could be sitting at the end of
        // a block when a function runs, e.g. `do [zero-arity]`.  That frame
        // will stay on the stack while the zero-arity function is running.
        // The array still might be used in an error, so can't GC it.
        //
        if (f->value && NOT_END(f->value) && Is_Value_Managed(f->value))
            Queue_Mark_Value_Deep(f->value);

        if (f->specifier != SPECIFIED)
            Queue_Mark_Context_Deep(CTX(f->specifier));

        if (NOT_END(f->out)) // never NULL, always initialized bit pattern
            Queue_Mark_Opt_Value_Deep(f->out);

        if (NOT(Is_Any_Function_Frame(f))) {
            //
            // Consider something like `eval copy quote (recycle)`, because
            // while evaluating the group it has no anchor anywhere in the
            // root set and could be GC'd.  The Reb_Frame's array ref is it.
            //
            continue;
        }

        if (NOT_END(&f->cell))
            Queue_Mark_Opt_Value_Deep(&f->cell);

        Queue_Mark_Function_Deep(f->phase); // never NULL
        Mark_Rebser_Only(f->label); // also never NULL

        if (!Is_Function_Frame_Fulfilling(f)) {
            assert(IS_END(f->param)); // indicates function is running

            // refine and special can be used to GC protect an arbitrary
            // value while a function is running, currently.  (A more
            // important purpose may come up...)

            if (NOT_END(f->refine) && Is_Value_Managed(f->refine))
                Queue_Mark_Opt_Value_Deep(f->refine);

            if (NOT_END(f->special) && Is_Value_Managed(f->special))
                Queue_Mark_Opt_Value_Deep(f->special);
        }

        // Need to keep the label symbol alive for error messages/stacktraces
        //
        Mark_Rebser_Only(f->label);

        // We need to GC protect the values in the args no matter what,
        // but it might not be managed yet (e.g. could still contain garbage
        // during argument fulfillment).  But if it is managed, then it needs
        // to be handed to normal GC.
        //
        if (f->varlist != NULL && IS_ARRAY_MANAGED(f->varlist))
            Queue_Mark_Context_Deep(CTX(f->varlist));

        // (Although the above will mark the varlist, it may not mark the
        // values...because it may be a single element array that merely
        // points at the stackvars.  Queue_Mark_Context expects stackvars
        // to be marked separately.)

        // The slots may be stack based or dynamic.  Mark in use but only
        // as far as parameter filling has gotten (may be garbage bits
        // past that).  Could also be an END value of an in-progress arg
        // fulfillment, but in that case it is protected by the evaluating
        // frame's f->out.
        //
        // Refinements need special treatment, and also consideration
        // of if this is the "doing pickups" or not.  If doing pickups
        // then skip the cells for pending refinement arguments.
        //
        REBVAL *param = FUNC_FACADE_HEAD(f->phase);
        REBVAL *arg = f->args_head; // may be stack or dynamic
        for (; NOT_END(param); ++param, ++arg) {
            if (param == f->param && !f->doing_pickups)
                break; // protect arg for current param, but no further

            assert(!IS_UNREADABLE_IF_DEBUG(arg) || f->doing_pickups);

            Queue_Mark_Opt_Value_Deep(arg);
        }
        assert(IS_END(param) ? IS_END(arg) : TRUE); // may not enforce

        Propagate_All_GC_Marks();
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
static REBCNT Sweep_Series(void)
{
    REBCNT count = 0;

    // Optimization here depends on SWITCH of a bank of 4 bits.
    //
    static_assert_c(
        NODE_FLAG_MARKED == FLAGIT_LEFT(3) // 0x1 after right shift
        && (NODE_FLAG_MANAGED == FLAGIT_LEFT(2)) // 0x2 after right shift
        && (NODE_FLAG_FREE == FLAGIT_LEFT(1)) // 0x4 after right shift
        && (NODE_FLAG_NODE == FLAGIT_LEFT(0)) // 0x8 after right shift
    );

    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg != NULL; seg = seg->next) {
        REBSER *s = cast(REBSER*, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[SER_POOL].units; n > 0; --n, ++s) {
            switch (LEFT_N_BITS(s->header.bits, 4)) {
            case 0:
            case 1: // 0x1
            case 2: // 0x2
            case 3: // 0x2 + 0x1
            case 4: // 0x4
            case 5: // 0x4 + 0x1
            case 6: // 0x4 + 0x2
            case 7: // 0x4 + 0x2 + 0x1
                //
                // NODE_FLAG_NODE (0x8) is clear.  This signature is
                // reserved for UTF-8 strings (corresponding to valid ASCII
                // values in the first byte).
                //
                panic (s);

            // v-- Everything below here has NODE_FLAG_NODE set (0x8)

            case 8:
                // 0x8: unmanaged and unmarked, e.g. a series that was made
                // with Make_Series() and hasn't been managed.  It doesn't
                // participate in the GC.  Leave it as is.
                //
                break;

            case 9:
                // 0x8 + 0x1: marked but not managed, this can't happen,
                // because the marking itself asserts nodes are managed.
                //
                panic (s);

            case 10:
                // 0x8 + 0x2: managed but didn't get marked, should be GC'd
                //
                // !!! It would be nice if we could have NODE_FLAG_CELL here
                // as part of the switch, but see its definition for why it
                // is at position 8 from left and not an earlier bit.
                //
                if (s->header.bits & NODE_FLAG_CELL)
                    Free_Node(SER_POOL, s); // Free_Pairing is for manuals
                else
                    GC_Kill_Series(s);
                ++count;
                break;

            case 11:
                // 0x8 + 0x2 + 0x1: managed and marked, so it's still live.
                // Don't GC it, just clear the mark.
                //
                s->header.bits &= ~NODE_FLAG_MARKED;
                break;

            // v-- Everything below this line has the two leftmost bits set
            // in the header.  In the *general* case this could be a valid
            // first byte of a multi-byte sequence in UTF-8...so only the
            // special bit pattern of the free case uses this.

            case 12:
                // 0x8 + 0x4: free node, uses special illegal UTF-8 byte
                //
                assert(LEFT_8_BITS(s->header.bits) == FREED_SERIES_BYTE);
                break;

            case 13:
                // 0x8 + 0x4 + 0x1: "free unmanaged marked node" (?!)
                //
                panic (s);

            case 14:
                // 0x8 + 0x4 + 0x2: "free managed unmarked node" (?!)
                //
                panic (s);

            case 15:
                // 0x8 + 0x4 + 0x2 + 0x1: "free managed marked node" (?!)
                //
                panic (s);
            }
        }
    }

    return count;
}


#if !defined(NDEBUG)

//
//  Fill_Sweeplist: C
//
REBCNT Fill_Sweeplist(REBSER *sweeplist)
{
    assert(SER_WIDE(sweeplist) == sizeof(REBNOD*));
    assert(SER_LEN(sweeplist) == 0);

    REBCNT count = 0;

    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg != NULL; seg = seg->next) {
        REBSER *s = cast(REBSER*, seg + 1);
        REBCNT n;
        for (n = Mem_Pools[SER_POOL].units; n > 0; --n, ++s) {
            switch (LEFT_N_BITS(s->header.bits, 4)) {
            case 9: // 0x8 + 0x1
                assert(IS_SERIES_MANAGED(s));
                if (Is_Rebser_Marked(s))
                    Unmark_Rebser(s);
                else {
                    EXPAND_SERIES_TAIL(sweeplist, 1);
                    *SER_AT(REBNOD*, sweeplist, count) = NOD(s);
                    ++count;
                }
                break;

            case 11: // 0x8 + 0x2 + 0x1
                //
                // It's a cell which is managed where the key is not an END.
                // This is a managed pairing, so mark bit should be heeded.
                //
                // !!! It is a REBNOD, but *not* a "series".
                //
                assert(IS_SERIES_MANAGED(s));
                if (Is_Rebser_Marked(s))
                    Unmark_Rebser(s);
                else {
                    EXPAND_SERIES_TAIL(sweeplist, 1);
                    *SER_AT(REBNOD*, sweeplist, count) = NOD(s);
                    ++count;
                }
                break;
            }
        }
    }

    return count;
}

#endif


//
//  Recycle_Core: C
//
// Recycle memory no longer needed.  If sweeplist is not NULL, then it needs
// to be a series whose width is sizeof(REBSER*), and it will be filled with
// the list of series that *would* be recycled.
//
REBCNT Recycle_Core(REBOOL shutdown, REBSER *sweeplist)
{
    // Ordinarily, it should not be possible to spawn a recycle during a
    // recycle.  But when debug code is added into the recycling code, it
    // could cause a recursion.  Be tolerant of such recursions to make that
    // debugging easier...but make a note that it's not ordinarily legal.
    //
#if !defined(NDEBUG)
    if (GC_Recycling) {
        printf("Recycle re-entry; should only happen in debug scenarios.\n");
        SET_SIGNAL(SIG_RECYCLE);
        return 0;
    }
#endif

    // If disabled by RECYCLE/OFF, exit now but set the pending flag.  (If
    // shutdown, ignore so recycling runs and can be checked for balance.)
    //
    if (!shutdown && GC_Disabled) {
        SET_SIGNAL(SIG_RECYCLE);
        return 0;
    }

#if !defined(NDEBUG)
    GC_Recycling = TRUE;
#endif

    ASSERT_NO_GC_MARKS_PENDING();

    Reify_Any_C_Valist_Frames();


#if !defined(NDEBUG)
    PG_Reb_Stats->Recycle_Counter++;
    PG_Reb_Stats->Recycle_Series = Mem_Pools[SER_POOL].free;

    PG_Reb_Stats->Mark_Count = 0;
#endif

    // WARNING: This terminates an existing open block.  This could be a
    // problem if code is building a new value at the tail, but has not yet
    // updated the TAIL marker.
    //
    TERM_ARRAY_LEN(BUF_COLLECT, ARR_LEN(BUF_COLLECT));

    // MARKING PHASE: the "root set" from which we determine the liveness
    // (or deadness) of a series.  If we are shutting down, we do not mark
    // several categories of series...but we do need to run the root marking.
    // (In particular because that is when pairing series whose lifetimes
    // are bound to frames will be freed, if the frame is expired.)
    //
    Mark_Root_Series();

    if (!shutdown) {
        Mark_Natives();
        Mark_Symbol_Series();

        Mark_Data_Stack();

        Mark_Guarded_Nodes();

        Mark_Frame_Stack_Deep();

        Propagate_All_GC_Marks();

        Mark_Devices_Deep();

    }

    // SWEEPING PHASE

    ASSERT_NO_GC_MARKS_PENDING();

    REBCNT count = 0;

    if (sweeplist != NULL) {
    #if defined(NDEBUG)
        panic (sweeplist);
    #else
        count += Fill_Sweeplist(sweeplist);
    #endif
    }
    else
        count += Sweep_Series();

    // !!! The intent is for GOB! to be unified in the REBNOD pattern, the
    // way that the FFI structures were.  So they are not included in the
    // count, in order to help make the numbers returned consistent between
    // when the sweeplist is used and not.
    //
    Sweep_Gobs();

#if !defined(NDEBUG)
    // Compute new stats:
    PG_Reb_Stats->Recycle_Series
        = Mem_Pools[SER_POOL].free - PG_Reb_Stats->Recycle_Series;
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

        if (Reb_Opts->watch_recycle)
            Debug_Fmt(RM_WATCH_RECYCLE, count);
    }

    ASSERT_NO_GC_MARKS_PENDING();

#if !defined(NDEBUG)
    GC_Recycling = FALSE;
#endif

    return count;
}


//
//  Recycle: C
//
// Recycle memory no longer needed.
//
REBCNT Recycle(void)
{
    // Default to not passing the `shutdown` flag.
    //
    REBCNT n = Recycle_Core(FALSE, NULL);

#ifdef DOUBLE_RECYCLE_TEST
    //
    // If there are two recycles in a row, then the second should not free
    // any additional series that were not freed by the first.  (It also
    // shouldn't crash.)  This is an expensive check, but helpful to try if
    // it seems a GC left things in a bad state that crashed a later GC.
    //
    REBCNT n2 = Recycle_Core(FALSE, NULL);
    assert(n2 == 0);
#endif

    return n;
}


//
//  Guard_Node_Core: C
//
void Guard_Node_Core(const REBNOD *node)
{
#if !defined(NDEBUG)
    if (node->header.bits & NODE_FLAG_CELL) {
        //
        // It is a value.  Cheap check: require that it already contain valid
        // data when the guard call is made (even if GC isn't necessarily
        // going to happen immediately, and value could theoretically become
        // valid before then.)
        //
        const REBVAL* value = cast(const REBVAL*, node);
        assert(
            IS_END(value)
            || IS_BLANK_RAW(value)
            || VAL_TYPE(value) <= REB_MAX_VOID
        );

    #ifdef STRESS_CHECK_GUARD_VALUE_POINTER
        //
        // Technically we should never call this routine to guard a value
        // that lives inside of a series.  Not only would we have to guard the
        // containing series, we would also have to lock the series from
        // being able to resize and reallocate the data pointer.  But this is
        // a somewhat expensive check, so only feasible to run occasionally.
        //
        REBSER *containing = Try_Find_Containing_Series_Debug(value);
        if (containing != NULL)
            panic (containing);
    #endif
    }
    else {
        // It's a series.  Does not ensure the series being guarded is
        // managed, since it can be interesting to guard the managed
        // *contents* of an unmanaged array.  The calling wrappers ensure
        // managedness or not.
    }
#endif

    if (SER_FULL(GC_Guarded))
        Extend_Series(GC_Guarded, 8);

    *SER_AT(
        const REBNOD*,
        GC_Guarded,
        SER_LEN(GC_Guarded)
    ) = node;

    SET_SERIES_LEN(GC_Guarded, SER_LEN(GC_Guarded) + 1);
}


//
//  Snapshot_All_Functions: C
//
// This routine can be used to get a list of all the functions in the system
// at a given moment in time.  Be sure to protect this array from GC when
// enumerating if there is any chance the GC might run (e.g. if user code
// is called to process the function list)
//
REBARR *Snapshot_All_Functions(void)
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
                if (GET_SER_FLAG(s, ARRAY_FLAG_PARAMLIST)) {
                    REBVAL *v = KNOWN(ARR_HEAD(ARR(s)));
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
//  Startup_GC: C
//
// Initialize garbage collector.
//
void Startup_GC(void)
{
    assert(NOT(GC_Disabled));
    assert(NOT(GC_Recycling));

    GC_Ballast = MEM_BALLAST;

    // Temporary series and values protected from GC. Holds node pointers.
    //
    GC_Guarded = Make_Series(15, sizeof(REBNOD*));

    // The marking queue used in lieu of recursion to ensure that deeply
    // nested structures don't cause the C stack to overflow.
    //
    GC_Mark_Stack = Make_Series(100, sizeof(REBARR*));
    TERM_SEQUENCE(GC_Mark_Stack);
}


//
//  Shutdown_GC: C
//
void Shutdown_GC(void)
{
    Free_Series(GC_Guarded);
    Free_Series(GC_Mark_Stack);
}


//=////////////////////////////////////////////////////////////////////////=//
//
// DEPRECATED HOOKS INTO THE CORE GARBAGE COLLECTOR
//
//=////////////////////////////////////////////////////////////////////////=//

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
static void Queue_Mark_Gob_Deep(REBGOB *gob)
{
    REBGOB **pane;
    REBCNT i;

    if (IS_GOB_MARK(gob)) return;

    MARK_GOB(gob);

    if (GOB_PANE(gob)) {
        Mark_Rebser_Only(GOB_PANE(gob));
        pane = GOB_HEAD(gob);
        for (i = 0; i < GOB_LEN(gob); i++, pane++)
            Queue_Mark_Gob_Deep(*pane);
    }

    if (GOB_PARENT(gob)) Queue_Mark_Gob_Deep(GOB_PARENT(gob));

    if (GOB_CONTENT(gob)) {
        if (GOB_TYPE(gob) >= GOBT_IMAGE && GOB_TYPE(gob) <= GOBT_STRING)
            Mark_Rebser_Only(GOB_CONTENT(gob));
        else if (GOB_TYPE(gob) >= GOBT_DRAW && GOB_TYPE(gob) <= GOBT_EFFECT)
            Queue_Mark_Array_Deep(ARR(GOB_CONTENT(gob)));
    }

    if (GOB_DATA(gob)) {
        switch (GOB_DTYPE(gob)) {
        case GOBD_INTEGER:
        case GOBD_NONE:
        default:
            break;
        case GOBD_OBJECT:
            Queue_Mark_Context_Deep(CTX(GOB_DATA(gob)));
            break;
        case GOBD_STRING:
        case GOBD_BINARY:
            Mark_Rebser_Only(GOB_DATA(gob));
            break;
        case GOBD_BLOCK:
            Queue_Mark_Array_Deep(ARR(GOB_DATA(gob)));
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
static REBCNT Sweep_Gobs(void)
{
    REBCNT count = 0;

    REBSEG *seg;
    for (seg = Mem_Pools[GOB_POOL].segs; seg; seg = seg->next) {
        REBGOB *gob = cast(REBGOB*, seg + 1);

        REBCNT n;
        for (n = Mem_Pools[GOB_POOL].units; n > 0; --n, ++gob) {
            if (IS_FREE_NODE(gob)) // unused REBNOD
                continue;

            if (IS_GOB_MARK(gob))
                UNMARK_GOB(gob);
            else {
                Free_Node(GOB_POOL, gob);

                // GC_Ballast is of type REBINT, which might be long
                // and REB_I32_ADD_OF takes (int*)
                // it's illegal to convert form (long*) to (int*) in C++
                i32 tmp;
                GC_Ballast = REB_I32_ADD_OF(
                    GC_Ballast, Mem_Pools[GOB_POOL].wide, &tmp
                ) ? MAX_I32 : tmp;

                if (GC_Ballast > 0)
                    CLR_SIGNAL(SIG_RECYCLE);

                count++;
            }
        }
    }

    return count;
}


//
//  Queue_Mark_Event_Deep: C
//
// 'Queue' refers to the fact that after calling this routine,
// one will have to call Propagate_All_GC_Marks() to have the
// deep transitive closure completely marked.
//
static void Queue_Mark_Event_Deep(const RELVAL *value)
{
    REBREQ *req;

    if (
        IS_EVENT_MODEL(value, EVM_PORT)
        || IS_EVENT_MODEL(value, EVM_OBJECT)
    ) {
        Queue_Mark_Context_Deep(CTX(VAL_EVENT_SER(m_cast(RELVAL*, value))));
    }
    else if (IS_EVENT_MODEL(value, EVM_GUI)) {
        Queue_Mark_Gob_Deep(cast(REBGOB*, VAL_EVENT_SER(m_cast(RELVAL*, value))));
    }

    // FIXME: This test is not in parallel to others.
    if (VAL_EVENT_TYPE(value) == EVT_DROP_FILE
        && GET_FLAG(VAL_EVENT_FLAGS(value), EVF_COPIED)
        )
    {
        assert(FALSE);
        Queue_Mark_Array_Deep(ARR(VAL_EVENT_SER(m_cast(RELVAL*, value))));
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
                Queue_Mark_Context_Deep(CTX(req->port));
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
static void Mark_Devices_Deep(void)
{
    REBDEV **devices = Host_Lib->devices;

    int d;
    for (d = 0; d < RDI_MAX; d++) {
        REBREQ *req;
        REBDEV *dev = devices[d];
        if (!dev)
            continue;

        for (req = dev->pending; req; req = req->next)
            if (req->port)
                Queue_Mark_Context_Deep(CTX(req->port));
    }

    Propagate_All_GC_Marks();
}
