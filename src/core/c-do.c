//
//  File: %c-do.c
//  Summary: "DO Evaluator Wrappers"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
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
// These are the "slightly more user-friendly" interfaces to the evaluator
// from %c-eval.c.  These routines will do the setup of the Reb_Frame state
// for you.
//
// Even "friendlier" interfaces are available as macros on top of these.
// See %sys-do.h for DO_VAL_ARRAY_AT_THROWS() and similar macros.
//

#include "sys-core.h"


//
//  Do_Array_At_Core: C
//
// Most common case of evaluator invocation in Rebol: the data lives in an
// array series.  Generic routine takes flags and may act as either a DO
// or a DO/NEXT at the position given.  Option to provide an element that
// may not be resident in the array to kick off the execution.
//
REBIXO Do_Array_At_Core(
    REBVAL *out,
    const REBVAL *opt_first,
    REBARR *array,
    REBCNT index,
    REBFLGS flags
) {
    struct Reb_Frame f;

    if (opt_first) {
        f.value = opt_first;
        f.indexor = index;
    }
    else {
        // Do_Core() requires caller pre-seed first value, always
        //
        f.value = ARR_AT(array, index);
        f.indexor = index + 1;
    }

    if (IS_END(f.value)) {
        SET_VOID(out);
        return END_FLAG;
    }

    f.out = out;
    f.source.array = array;
    f.flags = flags;
    f.mode = CALL_MODE_GUARD_ARRAY_ONLY;

    Do_Core(&f);

    return f.indexor;
}


//
//  Do_Values_At_Core: C
//
// !!! Not yet implemented--concept is to accept a REBVAL[] array, rather
// than a REBARR of values.
//
// !!! Considerations of this core interface are to see the values as being
// potentially in non-contiguous points in memory, and advanced with some
// skip length between them.  Additionally the idea of some kind of special
// Rebol value or "REB_INSTRUCTION" to say how far to skip is a possibility,
// which would be more general in the sense that it would allow the skip
// distances to be generalized, though this would cost a pointer size
// entity at each point.  The advantage of REB_INSTRUCTION is that only the
// clients using the esoteric ability would be paying anything for it or
// the API complexity, but if an important client like Ren-C++ it might
// be worth the savings.
//
// Note: Functionally it would be possible to assume a 0 index and require
// the caller to bump the value pointer as necessary.  But an index-based
// interface is likely useful to avoid the bookkeeping required for the caller.
//
REBIXO Do_Values_At_Core(
    REBVAL *out,
    REBFLGS flags,
    const REBVAL *opt_head,
    const REBVAL values[],
    REBCNT index
) {
    fail (Error(RE_MISC));
}


//
//  Reify_Va_To_Array_In_Frame: C
//
// For performance and memory usage reasons, a variadic C function call that
// wants to invoke the evaluator with just a comma-delimited list of REBVAL*
// does not need to make a series to hold them.  Do_Core is written to use
// the va_list traversal as an alternate to DO-ing an ARRAY.
//
// However, va_lists cannot be backtracked once advanced.  So in a debug mode
// it can be helpful to turn all the va_lists into arrays before running
// them, so stack frames can be inspected more meaningfully--both for upcoming
// evaluations and those already past.
//
// A non-debug reason to reify a va_list into an array is if the garbage
// collector needs to see the upcoming values to protect them from GC.  In
// this case it only needs to protect those values that have not yet been
// consumed.
//
// Because items may well have already been consumed from the va_list() that
// can't be gotten back, we put in a marker to help hint at the truncation
// (unless told that it's not truncated, e.g. a debug mode that calls it
// before any items are consumed).
//
// This does not touch the current prefetched f->value in the frame--it only
// changes the source and the indexor which will be seen by the next fetch.
//
void Reify_Va_To_Array_In_Frame(struct Reb_Frame *f, REBOOL truncated)
{
    REBDSP dsp_orig = DSP;
    const REBVAL *value;

    assert(f->flags & DO_FLAG_VALIST);
    assert(f->indexor == VALIST_FLAG || f->indexor == END_FLAG);

    //assert(f->eval_fetched == NULL); // could reification ever happen here?

    if (truncated) {
        REBVAL temp;
        VAL_INIT_WRITABLE_DEBUG(&temp);
        Val_Init_Word(&temp, REB_WORD, SYM___OPTIMIZED_OUT__);

        DS_PUSH(&temp);
    }

    if (f->indexor != END_FLAG) {
        while (NOT_END(value = va_arg(*f->source.vaptr, const REBVAL*)))
            DS_PUSH(value);

        if (truncated)
            f->indexor = 1; // skip the --optimized-out--
        else
            f->indexor = 0; // position at the start of the extracted values
    }
    else {
        // Leave at the END_FLAG, but give back the array to serve as
        // notice of the truncation (if it was truncated)
    }

    if (DSP != dsp_orig) {
        f->source.array = Pop_Stack_Values(dsp_orig);
        MANAGE_ARRAY(f->source.array); // held alive while frame running
    }
    else
        f->source.array = EMPTY_ARRAY; // doesn't need to be unique

    // We clear the DO_FLAG_VALIST, assuming that the truncation marker is
    // enough information to record the fact that it was a va_list (revisit
    // if there's another reason to know what it was...)

    f->flags &= ~DO_FLAG_VALIST;
}


//
//  Do_Va_Core: C
//
// (va_list by pointer: http://stackoverflow.com/a/3369762/211160)
//
// Central routine for doing an evaluation of an array of values by calling
// a C function with those parameters (e.g. supplied as arguments, separated
// by commas).  Uses same method to do so as functions like printf() do.
//
// In R3-Alpha this style of invocation was specifically used to call single
// Rebol functions.  It would use a list of REBVAL*s--each of which could
// come from disjoint memory locations and be passed directly with no
// evaluation.  Ren-C replaced this entirely by adapting the evaluator to
// use va_arg() lists for the same behavior as a DO of an ARRAY.
//
// The previously accomplished style of execution with a function which may
// not be in the arglist can be accomplished using `opt_first` to put that
// function into the optional first position.  To instruct the evaluator not
// to do any evaluation on the values supplied as arguments after that
// (corresponding to R3-Alpha's APPLY/ONLY) then DO_FLAG_EVAL_ONLY should be
// used--otherwise they will be evaluated normally.
//
// NOTE: Ren-C no longer supports the built-in ability to supply refinements
// positionally, due to the brittleness of this approach (for both system
// and user code).  The `opt_head` value should be made a path with the
// function at the head and the refinements specified there.  Future
// additions could do this more efficiently by allowing the refinement words
// to be pushed directly to the data stack.
//
// !!! C's va_lists are very dangerous, there is no type checking!  The
// C++ build should be able to check this for the callers of this function
// *and* check that you ended properly.  It means this function will need
// two different signatures (and so will each caller of this routine).
//
// Returns THROWN_FLAG, END_FLAG--or if DO_FLAG_NEXT is used it may return
// VALIST_INCOMPLETE_FLAG.
//
REBIXO Do_Va_Core(
    REBVAL *out,
    const REBVAL *opt_first,
    va_list *vaptr,
    REBFLGS flags
) {
    struct Reb_Frame f;

    if (opt_first)
        f.value = opt_first;
    else {
        // Do_Core() requires caller pre-seed first value, always
        //
        f.value = va_arg(*vaptr, const REBVAL*);
    }

    if (IS_END(f.value)) {
        SET_VOID(out);
        return END_FLAG;
    }

    f.out = out;
    f.indexor = VALIST_FLAG;
    f.source.vaptr = vaptr;
    f.mode = CALL_MODE_GUARD_ARRAY_ONLY;

    f.flags = flags | DO_FLAG_VALIST; // see notes in %sys-do.h on why needed

    Do_Core(&f);

    if (flags & DO_FLAG_NEXT) {
        //
        // The lookahead needed to permit lookback functions (e.g. infix)
        // causes a fetch that cannot be undone.  Hence va_list DO/NEXT can't
        // be resumed -- see VALIST_INCOMPLETE_FLAG.  For a resumable interface
        // on va_list, see the lower level API.
        //
        // Note that the va_list may be reified during the call, so the
        // index may not be VALIST_FLAG at this point.
        //
        // !!! Should this auto-reify, so it can keep going in all cases?
        // The transition from va_list to non is a bit strange, and even
        // if it were possible then users might wonder why the numbers
        // don't line up with the parameter order.  Also, doing it without
        // explicit request undermines knowledge of the efficiency lost.
        //
        if (f.indexor != THROWN_FLAG && f.indexor != END_FLAG) {
            //
            // Try one more fetch and see if it's at the end.  If not, we did
            // not consume all the input.
            //
            FETCH_NEXT_ONLY_MAYBE_END(&f);
            if (f.indexor != END_FLAG) {
                assert(f.indexor == VALIST_FLAG); // couldn't throw!!
                return VALIST_INCOMPLETE_FLAG;
            }
        }

        assert(f.indexor == THROWN_FLAG || f.indexor == END_FLAG);
    }

    return f.indexor;
}


//
//  Do_Va_Throws: C
//
// Wrapper around Do_Va_Core which has the actual variadic interface (as
// opposed to taking the `va_list` whicih has been captured out of the
// variadic interface).
//
REBOOL Do_Va_Throws(REBVAL *out, ...)
{
    va_list va;
    va_start(va, out); // must mention last param before the "..."

#ifdef VA_END_IS_MANDATORY
    struct Reb_State state;
    REBCTX *error;

    PUSH_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) {
        va_end(va); // interject cleanup of whatever va_start() set up...
        fail (error); // ...then just retrigger error
    }
#endif

    REBIXO indexor = Do_Va_Core(
        out,
        NULL, // opt_first
        &va,
        DO_FLAG_TO_END | DO_FLAG_ARGS_EVALUATE | DO_FLAG_LOOKAHEAD
    );

    va_end(va);
    //
    // ^-- This va_end() will *not* be called if a fail() happens to longjmp
    // during the apply.  But is that a problem, you ask?  No survey has
    // turned up an existing C compiler where va_end() isn't a NOOP.
    //
    // But there's implementations we know of, then there's the Standard...
    //
    //    http://stackoverflow.com/a/32259710/211160
    //
    // The Standard is explicit: an implementation *could* require calling
    // va_end() if it wished--it's undefined behavior if you skip it.
    //
    // In the interests of efficiency and not needing to set up trapping on
    // each apply, our default is to assume the implementation does not
    // need the va_end() call.  But for thoroughness, VA_END_IS_MANDATORY is
    // outlined here to show the proper bracketing if it were ever needed.

#ifdef VA_END_IS_MANDATORY
    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
#endif

    assert(indexor == THROWN_FLAG || indexor == END_FLAG);
    return LOGICAL(indexor == THROWN_FLAG);
}


//
//  Sys_Func: C
//
// Gets a system function with tolerance of it not being a function.
//
// (Extraction of a feature that formerly was part of a dedicated dual
// function to Apply_Func_Throws (Do_Sys_Func_Throws())
//
REBVAL *Sys_Func(REBCNT inum)
{
    REBVAL *value = CTX_VAR(Sys_Context, inum);

    if (!IS_FUNCTION(value)) fail (Error(RE_BAD_SYS_FUNC, value));

    return value;
}


//
//  Apply_Only_Throws: C
//
// Takes a list of arguments terminated by END_VALUE (or any IS_END) and
// will do something similar to R3-Alpha's "apply/only" with a value.  If
// that value is a function, it will be called...if it is a SET-WORD! it
// will be assigned, etc.
//
// This is equivalent to putting the value at the head of the input and
// then calling EVAL/ONLY on it.  If all the inputs are not consumed, an
// error will be thrown.
//
// The boolean result will be TRUE if an argument eval or the call created
// a THROWN() value, with the thrown value in `out`.
//
REBOOL Apply_Only_Throws(REBVAL *out, const REBVAL *applicand, ...)
{
    REBIXO indexor;
    va_list va;

#ifdef VA_END_IS_MANDATORY
    struct Reb_State state;
    REBCTX *error;
#endif

    va_start(va, applicand); // must mention last param before the "..."

#ifdef VA_END_IS_MANDATORY
    PUSH_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) {
        va_end(va); // interject cleanup of whatever va_start() set up...
        fail (error); // ...then just retrigger error
    }
#endif

    indexor = Do_Va_Core(
        out,
        applicand, // opt_first
        &va,
        DO_FLAG_NEXT | DO_FLAG_NO_ARGS_EVALUATE | DO_FLAG_LOOKAHEAD
    );

    if (indexor == VALIST_INCOMPLETE_FLAG) {
        //
        // Not consuming all the arguments given suggests a problem as far
        // as this interface is concerned.  To tolerate incomplete states,
        // use Do_Va_Core() directly.
        //
        fail (Error(RE_APPLY_TOO_MANY));
    }

    va_end(va); // see notes in Do_Va_Core RE: VA_END_IS_MANDATORY

#ifdef VA_END_IS_MANDATORY
    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
#endif

    assert(indexor == THROWN_FLAG || indexor == END_FLAG);
    return LOGICAL(indexor == THROWN_FLAG);
}
