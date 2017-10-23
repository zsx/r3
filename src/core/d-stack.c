//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
//=////////////////////////////////////////////////////////////////////////=//
//
//  Summary: Debug Stack Reflection and Querying
//  File: %d-stack.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2017 Rebol Open Source Contributors
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
// This file contains interactive debugging support for examining and
// interacting with the stack.
//
// !!! Interactive debugging is a work in progress, and comments are in the
// functions below.
//

#include "sys-core.h"


//
//  Collapsify_Array: C
//
// This will replace "long" nested blocks with collapsed versions with
// ellipses to show they have been cut off.  It does not change the arrays
// in question, but replaces them with copies.
//
void Collapsify_Array(REBARR *array, REBSPC *specifier, REBCNT limit)
{
    RELVAL *item = ARR_HEAD(array);
    for (; NOT_END(item); ++item) {
        if (ANY_ARRAY(item) && VAL_LEN_AT(item) > limit) {
            REBSPC *derived = Derive_Specifier(specifier, item);
            REBARR *copy = Copy_Array_At_Max_Shallow(
                VAL_ARRAY(item),
                VAL_INDEX(item),
                derived,
                limit + 1
            );

            Init_Word(ARR_AT(copy, limit), Canon(SYM_ELLIPSIS));

            Collapsify_Array(
                copy,
                SPECIFIED,
                limit
            );

            enum Reb_Kind kind = VAL_TYPE(item);
            Init_Any_Array_At(item, kind, copy, 0); // at 0 now
            assert(IS_SPECIFIC(item));
            assert(NOT_VAL_FLAG(item, VALUE_FLAG_LINE)); // should be cleared
        }
    }
}


//
//  Make_Where_For_Frame: C
//
// Each call frame maintains the array it is executing in, the current index
// in that array, and the index of where the current expression started.
// This can be deduced into a segment of code to display in the debug views
// to indicate roughly "what's running" at that stack level.  The code is
// a shallow copy of the array content.
//
// The resulting WHERE information only includes the range of the array being
// executed up to the point of currently relevant evaluation.  It does not
// go all the way to the tail of the block (where future potential evaluation
// should be.
//
// !!! Unfortunately, Rebol doesn't formalize this very well.  There is no
// lock on segments of blocks during their evaluation (should there be?).
// It's possible for self-modifying code to scramble the blocks being executed.
// The DO evaluator is robust in terms of not *crashing*, but the semantics
// may well suprise users.
//
// !!! DO also offers a feature whereby values can be supplied at the start
// of an evaluation which are not resident in the array.  It also can run
// on an irreversible C va_list of REBVAL*, where these disappear as the
// evaluation proceeds.  A special debug setting would be needed to hang
// onto these values for the purposes of better error messages (at the cost
// of performance).
//
REBARR *Make_Where_For_Frame(REBFRM *f)
{
    if (FRM_IS_VALIST(f)) {
        //
        // Traversing a C va_arg, so reify into a (truncated) array.
        //
        const REBOOL truncated = TRUE;
        Reify_Va_To_Array_In_Frame(f, truncated);
    }


    // WARNING: MIN is a C macro and repeats its arguments.
    //
    REBCNT start = MIN(ARR_LEN(FRM_ARRAY(f)), FRM_EXPR_INDEX(f));
    REBCNT end = MIN(ARR_LEN(FRM_ARRAY(f)), FRM_INDEX(f));

    assert(end >= start);

    assert(Is_Function_Frame(f));
    REBOOL pending = Is_Function_Frame_Fulfilling(f);

    REBCNT dsp_start = DSP;

    // !!! We may be running a function where the value for the function was a
    // "head" value not in the array.  These cases could substitute the symbol
    // for the currently executing function.  Reconsider when such cases
    // appear and can be studied.
    /*
        DS_PUSH_TRASH;
        Init_Word(DS_TOP, ...?)
    */

    REBCNT n;
    for (n = start; n < end; ++n) {
        DS_PUSH_TRASH;
        if (IS_VOID(ARR_AT(FRM_ARRAY(f), n))) {
            //
            // If a va_list is used to do a non-evaluative call (something
            // like R3-Alpha's APPLY/ONLY) then void cells are currently
            // allowed.  Reify_Va_To_Array_In_Frame() may come along and
            // make a special block containing voids, which we don't want
            // to expose in a user-visible block.  Since this array is just
            // for display purposes and is "lossy" (as evidenced by the ...)
            // substitute a placeholder to avoid crashing the GC.
            //
            assert(GET_SER_FLAG(FRM_ARRAY(f), ARRAY_FLAG_VOIDS_LEGAL));
            Init_Word(DS_TOP, Canon(SYM___VOID__));
        }
        else
            Derelativize(DS_TOP, ARR_AT(FRM_ARRAY(f), n), f->specifier);

        if (n == start) {
            //
            // Get rid of any newline marker on the first element,
            // that would visually disrupt the backtrace for no reason.
            //
            CLEAR_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE);
        }
    }

    // We add an ellipsis to a pending frame to make it a little bit
    // clearer what is going on.  If someone sees a where that looks
    // like just `* [print]` the asterisk alone doesn't quite send
    // home the message that print is not running and it is
    // argument fulfillment that is why it's not "on the stack"
    // yet, so `* [print ...]` is an attempt to say that better.
    //
    // !!! This is in-band, which can be mixed up with literal usage
    // of ellipsis.  Could there be a better "out-of-band" conveyance?
    // Might the system use colorization in a value option bit?
    //
    if (pending) {
        DS_PUSH_TRASH;
        Init_Word(DS_TOP, Canon(SYM_ELLIPSIS));
    }

    REBARR *where = Pop_Stack_Values(dsp_start);

    // Simplify overly-deep blocks embedded in the where so they show (...)
    // instead of printing out fully.
    //
    Collapsify_Array(where, SPECIFIED, 3);

    return where;
}


//
//  where-of: native [
//
//  "Get execution point summary for a function call (if still on stack)"
//
//      frame [frame!]
//  ]
//
REBNATIVE(where_of)
//
// !!! This routine should probably be used to get the information for the
// where of an error, which should likely be out-of-band.
{
    INCLUDE_PARAMS_OF_WHERE_OF;

    REBFRM *f = CTX_FRAME_IF_ON_STACK(VAL_CONTEXT(ARG(frame)));
    if (f == NULL)
        fail (Error_Frame_Not_On_Stack_Raw());

    Init_Block(D_OUT, Make_Where_For_Frame(f));
    return R_OUT;
}


//
//  label-of: native [
//
//  "Get word label used to invoke a function call (if still on stack)"
//
//      return: [word! blank!]
//      frame [frame!]
//  ]
//
REBNATIVE(label_of)
{
    INCLUDE_PARAMS_OF_LABEL_OF;

    REBFRM *f = CTX_FRAME_IF_ON_STACK(VAL_CONTEXT(ARG(frame)));
    if (f == NULL)
        fail (Error_Frame_Not_On_Stack_Raw());

    if (f->opt_label == NULL)
        return R_BLANK;

    Init_Word(D_OUT, f->opt_label);
    return R_OUT;
}


//
//  file-of: native [
//
//  "Get filename of origin for any series"
//
//      return: [file! url! blank!]
//      series [any-series!]
//  ]
//
REBNATIVE(file_of)
{
    INCLUDE_PARAMS_OF_FILE_OF;

    REBSER *s = VAL_SERIES(ARG(series));

    if (NOT_SER_FLAG(s, SERIES_FLAG_FILE_LINE))
        return R_BLANK;

    // !!! How to tell whether it's a URL! or a FILE! ?
    //
    Scan_File(D_OUT, STR_HEAD(LINK(s).filename), SER_LEN(LINK(s).filename));
    return R_OUT;
}


//
//  line-of: native [
//
//  "Get line of origin for any series"
//
//      return: [integer! blank!]
//      series [any-series!]
//  ]
//
REBNATIVE(line_of)
{
    INCLUDE_PARAMS_OF_LINE_OF;

    REBSER *s = VAL_SERIES(ARG(series));

    if (NOT_SER_FLAG(s, SERIES_FLAG_FILE_LINE))
        return R_BLANK;

    Init_Integer(D_OUT, MISC(s).line);
    return R_OUT;
}


//
//  function-of: native [
//
//  "Get the FUNCTION! for a frame"
//
//      return: [function!]
//      frame [frame!]
//  ]
//
REBNATIVE(function_of)
{
    INCLUDE_PARAMS_OF_FUNCTION_OF;

    REBVAL *frame = ARG(frame);

    // The phase contains the paramlist of the actual function (the context is
    // the keylist of the *underlying* function).
    //
    // But to get the function REBVAL, the phase has to be combined with the
    // binding of the FRAME! value.  Otherwise you'd know (for instance) that
    // you had a RETURN, but you wouldn't know where to return *from*.
    //
    Move_Value(D_OUT, FUNC_VALUE(frame->payload.any_context.phase));
    D_OUT->extra.binding = frame->extra.binding;

    return R_OUT;
}


//
//  Is_Context_Running_Or_Pending: C
//
REBOOL Is_Context_Running_Or_Pending(REBCTX *frame_ctx)
{
    REBFRM *f = CTX_FRAME_IF_ON_STACK(frame_ctx);
    if (f == NULL)
        return FALSE;

    if (Is_Function_Frame_Fulfilling(f))
        return FALSE;

    return TRUE;
}


//
//  running?: native [
//
//  "Returns TRUE if a FRAME! is on the stack and executing (arguments done)."
//
//      frame [frame!]
//  ]
//
REBNATIVE(running_q)
{
    INCLUDE_PARAMS_OF_RUNNING_Q;

    REBCTX *frame_ctx = VAL_CONTEXT(ARG(frame));

    REBFRM *f = CTX_FRAME_IF_ON_STACK(frame_ctx);
    if (f == NULL)
        return R_FALSE;

    if (Is_Function_Frame_Fulfilling(f))
        return R_FALSE;

    return R_TRUE;
}


//
//  pending?: native [
//
//  "Returns TRUE if a FRAME! is on the stack, but is gathering arguments."
//
//      frame [frame!]
//  ]
//
REBNATIVE(pending_q)
{
    INCLUDE_PARAMS_OF_PENDING_Q;

    REBCTX *frame_ctx = VAL_CONTEXT(ARG(frame));

    REBFRM *f = CTX_FRAME_IF_ON_STACK(frame_ctx);
    if (f == NULL)
        return R_FALSE;

    if (Is_Function_Frame_Fulfilling(f))
        return R_TRUE;

    return R_FALSE;
}
