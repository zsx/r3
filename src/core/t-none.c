//
//  File: %t-none.c
//  Summary: "none datatype"
//  Section: datatypes
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

#include "sys-core.h"

//
//  CT_None: C
//
REBINT CT_None(const REBVAL *a, const REBVAL *b, REBINT mode)
{
    if (mode >= 0) return (VAL_TYPE(a) == VAL_TYPE(b));
    return -1;
}


//
//  MT_None: C
//
REBOOL MT_None(REBVAL *out, REBVAL *data, enum Reb_Kind type)
{
    VAL_RESET_HEADER(out, type);
    return TRUE;
}


//
//  REBTYPE: C
// 
// ALSO used for unset!
//
REBTYPE(None)
{
    REBVAL *val = D_ARG(1);

    switch (action) {

    case A_MAKE:
    case A_TO:
        if (IS_DATATYPE(val))
            return VAL_TYPE_KIND(val) == REB_NONE ? R_NONE : R_UNSET;
        else
            return IS_NONE(val) ? R_NONE : R_UNSET;

    case A_TAIL_Q:
        if (IS_NONE(val)) return R_TRUE;
        goto trap_it;

    case A_INDEX_OF:
    case A_LENGTH:
    case A_SELECT:
    case A_FIND:
    case A_REMOVE:
    case A_CLEAR:
    case A_TAKE:
        if (IS_NONE(val)) return R_NONE;
    default:
    trap_it:
        fail (Error_Illegal_Action(VAL_TYPE(val), action));
    }

    return R_OUT;
}
