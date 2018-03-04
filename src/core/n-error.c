//
//  File: %n-error.c
//  Summary: "native functions for raising and trapping errors"
//  Section: natives
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
// Note that the mechanism by which errors are raised is based on longjmp(),
// and thus can interrupt stacks in progress.  Trapping errors is only done
// by those levels of the stack that have done a PUSH_TRAP (as opposed to
// detecting thrown values, that is "cooperative" and "bubbles" up through
// every stack level in its return slot, with no longjmp()).
//

#include "sys-core.h"


// This is the code which is protected by the exception mechanism.  See the
// rebRescue() API for more information.
//
static REBVAL *Trap_Native_Core(REBFRM *frame_) {
    INCLUDE_PARAMS_OF_TRAP;

    UNUSED(REF(with));
    UNUSED(ARG(handler));
    UNUSED(REF(q));

    const REBVAL *condition = END; // only allow 0-arity functions
    const REBOOL only = REF(with); // voids verbatim only if handler given
    if (Run_Branch_Throws(D_OUT, condition, ARG(code), only)) {
        //
        // returned value is tested for THROWN() status by caller
    }

    return NULL;
}


//
//  trap: native [
//
//  {Tries to DO a block, trapping error as return value (if one is raised).}
//
//      return: [<opt> any-value!]
//          {If ERROR!, error was raised (void if non-raised ERROR! result)}
//      code [block! function!]
//          {Block or zero-arity function to execute}
//      /with
//          "Handle error case with more code (overrides voiding behavior)"
//      handler [block! function!]
//          "If FUNCTION!, spec allows [error [error!]]"
//      /?
//          "Instead of result or error, return LOGIC! of if a trap occurred"
//  ]
//
REBNATIVE(trap)
{
    INCLUDE_PARAMS_OF_TRAP; // ? is renamed as "q"

    REBVAL *error = rebRescue(cast(REBDNG*, &Trap_Native_Core), frame_);
    UNUSED(ARG(code)); // gets used by the above call, via the frame_ pointer

    if (error == NULL) {
        //
        // Even if the protected execution in Trap_Core didn't have an error,
        // it might have thrown.
        //
        if (THROWN(D_OUT))
            return R_OUT_IS_THROWN;

        if (REF(q))
            return R_FALSE;

        // If there is no handler for errors, then "voidify" a non-raised
        // error so that ERROR! always means *raised* error.
        //
        if (NOT(REF(with)) && IS_ERROR(D_OUT))
            return R_VOID;

        return R_OUT;
    }

    assert(IS_ERROR(error));

    if (REF(with)) {
        //
        // The handler may fail() which would leak the error.  We could
        // rebManage() it so it would be freed in that case, but probably
        // just as cheap to copy it and release it.
        //
        // !!! The BLOCK! case doesn't even use the `condition` parameter,
        // so it could release it without moving.
        //
        Move_Value(D_CELL, error);
        rebRelease(error);

        const REBOOL only = TRUE; // return voids as-is
        if (Run_Branch_Throws(D_OUT, D_CELL, ARG(handler), only))
            return R_OUT_IS_THROWN;
    }
    else {
        Move_Value(D_OUT, error);
        rebRelease(error);
    }

    if (REF(q))
        return R_TRUE;

    return R_OUT;
}


//
//  set-location-of-error: native [
//
//  {Sets the WHERE, NEAR, FILE, and LINE fields of an error}
//
//      return: [<opt>]
//      error [error!]
//      location [frame! any-word!]
//  ]
//
REBNATIVE(set_location_of_error)
{
    INCLUDE_PARAMS_OF_SET_LOCATION_OF_ERROR;

    REBCTX *context;
    if (IS_WORD(ARG(location)))
        context = VAL_WORD_CONTEXT(ARG(location));
    else
        context = VAL_CONTEXT(ARG(location));

    REBFRM *where = CTX_FRAME_MAY_FAIL(context);

    REBCTX *error = VAL_CONTEXT(ARG(error));
    Set_Location_Of_Error(error, where);

    return R_VOID;
}
