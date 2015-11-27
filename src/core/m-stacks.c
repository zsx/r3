/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Module:  m-stack.c
**  Summary: data and function call stack implementation
**  Section: memory
**  Notes:
**
***********************************************************************/

#include "sys-core.h"


//
//  Init_Stacks: C
//
void Init_Stacks(REBCNT size)
{
    CS_Top = NULL;
    CS_Running = NULL;

    DS_Series = Make_Array(size);
    Set_Root_Series(TASK_STACK, DS_Series, "data stack"); // uses special GC
}


//
//  Shutdown_Stacks: C
//
void Shutdown_Stacks(void)
{
    assert(!CS_Running);
    assert(!CS_Top);

    assert(DSP == -1);
}


//
//  Push_Stack_Values: C
// 
// Pushes sequential values from a series onto the stack all
// in one go.  All of this needs review in terms of whether
// things like COMPOSE should be using arbitrary stack pushes
//      in the first place or if it should not pile up the stack
// like this.
// 
// !!! Notably simple implementation, just hammering out the
// client interfaces that made sequential stack memory assumptions.
//
void Push_Stack_Values(const REBVAL *values, REBINT length)
{
    Insert_Series(
        DS_Series, SERIES_TAIL(DS_Series), cast(const REBYTE*, values), length
    );
}


//
//  Pop_Stack_Values: C
// 
// Pop_Stack_Values computed values from the stack into the series
// specified by "into", or if into is NULL then store it as a
// block on top of the stack.  (Also checks to see if into
// is protected, and will trigger a trap if that is the case.)
// 
// Protocol for /INTO is to set the position to the tail.
//
void Pop_Stack_Values(REBVAL *out, REBINT dsp_start, REBOOL into)
{
    REBSER *series;
    REBCNT len = DSP - dsp_start;
    REBVAL *values = BLK_SKIP(DS_Series, dsp_start + 1);

    if (into) {
        assert(ANY_ARRAY(out));
        series = VAL_SERIES(out);
        if (IS_PROTECT_SERIES(series)) fail (Error(RE_PROTECTED));
        VAL_INDEX(out) = Insert_Series(
            series, VAL_INDEX(out), cast(REBYTE*, values), len
        );
    }
    else {
        series = Copy_Values_Len_Shallow(values, len);
        Val_Init_Block(out, series);
    }

    DS_DROP_TO(dsp_start);
}


//
//  Expand_Stack: C
// 
// Expand the datastack. Invalidates any references to stack
// values, so code should generally use stack index integers,
// not pointers into the stack.
//
void Expand_Stack(REBCNT amount)
{
    if (SERIES_REST(DS_Series) >= STACK_LIMIT) Trap_Stack_Overflow();
    Extend_Series(DS_Series, amount);
    Debug_Fmt(cs_cast(BOOT_STR(RS_STACK, 0)), DSP, SERIES_REST(DS_Series));
}


//
//  Push_New_Arglist_For_Call: C
// 
// Allocate the series of REBVALs inspected by a non-frameless function when
// executed (the values behind D_ARG(1), D_REF(2), etc.)  Since the call
// contains the function, it is known how many parameters are needed.
//
// The call frame will be pushed onto the call stack, and hence its fields
// will be seen by the GC and protected.
// 
// However...we do not set the frame as "Running" at the same time we create
// it.  We need to fulfill its arguments in the caller's frame before we
// actually invoke the function, so it's Dispatch_Call that actually moves
// it to the running status.
//
void Push_New_Arglist_For_Call(struct Reb_Call *call_) {
    REBCNT index;
    REBCNT num_vars;

    // Should not already have an arglist
    //
    assert(!call_->arglist);

    // `num_vars` is the total number of elements in the series, including the
    // function's "Self" REBVAL in the 0 slot.
    //
    assert(ANY_FUNC(D_FUNC));
    num_vars = SERIES_LEN(VAL_FUNC_PARAMLIST(D_FUNC));

    // Make an array to hold the arguments.  It will always be at least one
    // variable long, because function frames start with the value of the
    // function in slot 0.
    //
    // CLOSURE! will wind up managing this series and taking it over.
    //
    // !!! Though it may seem expensive to create this array, it may be that
    // 0, 1, or 2-element arrays will be very cheap to make in the future.
    //
    call_->arglist = Make_Array(num_vars); // D_ARGC uses arglist length!

    // Write some garbage (that won't crash the GC) into the `cell` slot in
    // the debug build.  `out` and `func` are known to be GC-safe.
    //
    SET_TRASH_SAFE(D_CELL);

    // Even though we can't push this stack frame to be CS_Running yet, it
    // still needs to be considered for GC.  In a recursive DO we can get
    // many pending frames before we come back to actually putting the
    // topmost one in effect.
    //
    call_->prior = CS_Top;
    CS_Top = call_;

    // This will be a function or closure frame, and we always have the
    // 0th element set to the value of the function itself.  This allows
    // the single REBSER* to be able to lead us back to access the entire
    // REBVAL worth of information.
    //
    // !!! Review to see if there's a cheap way to put the closure frame here
    // instead of the closure function value, as Do_Closure_Throws() is just
    // going to overwrite this slot.
    //
    *BLK_HEAD(call_->arglist) = *D_FUNC;

    // Make_Call does not fill the args in the frame--that is up to Do_Core
    // and Apply_Block to do as they go along.  But the frame has to survive
    // Recycle() during arg fulfillment...slots can't be left uninitialized.
    // It is important to set to UNSET for bookkeeping so that refinement
    // scanning knows when it has filled a refinement slot (and hence its
    // args) or not.
    //
    index = 1;
    while (index < num_vars) {
        SET_UNSET(BLK_SKIP(call_->arglist, index));
        index++;
    }
    SET_END(BLK_SKIP(call_->arglist, index));
    call_->arglist->tail = num_vars;
}


//
//  Drop_Call_Arglist: C
// 
// Free a call frame's arglist series.  These are done in a stack, so the
// call being dropped needs to be the last one pushed.
//
// Note that if a `fail` occurs this function will *not* be called, because
// a longjmp will skip the code that would have called it.  The point
// where it longjmps to will not be able to read the stack-allocated Reb_Call,
// because that stack will be done.
//
// Hence there cannot be anything in the Reb_Call structure that would not
// be able to be freed by the trap handlers implicitly (no malloc'd members,
// no cleanup needing imperative code, etc.)
//
void Drop_Call_Arglist(struct Reb_Call* call)
{
    assert(call == CS_Top);

    if (IS_CLOSURE(&call->func)) {
        //
        // CLOSURE! should have extracted the arglist and managed it by GC
        //
        assert(!call->arglist);
    }
    else {
        assert(
            BLK_LEN(call->arglist) ==
            SERIES_LEN(VAL_FUNC_PARAMLIST(&call->func))
        );

        // For other function types we free the frame.  This is not dangerous
        // for natives/etc. because there is no word binding to "leak" and be
        // dereferenced after the call.  But FUNCTION! words have some issues
        // related to this leak.
        //
        // !!! Review if a performant FUNCTION!/CLOSURE! unification exists,
        // to plug this problem with FUNCTION!.
        //
        Free_Series(call->arglist);
    }

    call->arglist = NULL;

    // Drop to the prior top call stack frame
    //
    CS_Top = call->prior;
}


#if !defined(NDEBUG)

//
//  DSF_ARG_Debug: C
// 
// Debug-only version of getting a variable out of a call
// frame, which asserts if you use an index that is higher
// than the number of arguments in the frame.
//
REBVAL *DSF_ARG_Debug(struct Reb_Call *call, REBCNT n)
{
    assert(n != 0 && n <= BLK_LEN(call->arglist));
    return BLK_SKIP(call->arglist, n);
}

#endif
