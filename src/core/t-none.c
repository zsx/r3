//
//  File: %t-none.c
//  Summary: "none (blank) datatype"
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
//  CT_Unit: C
//
REBINT CT_Unit(const REBVAL *a, const REBVAL *b, REBINT mode)
{
    if (mode >= 0) return (VAL_TYPE(a) == VAL_TYPE(b));
    return -1;
}


//
//  MT_Unit: C
//
REBOOL MT_Unit(REBVAL *out, REBVAL *data, enum Reb_Kind type)
{
    VAL_RESET_HEADER(out, type);
    return TRUE;
}


//
//  REBTYPE: C
//
REBTYPE(Unit)
{
    REBVAL *val = D_ARG(1);

    if (action == A_MAKE || action == A_TO) {
        assert(IS_DATATYPE(val) && VAL_TYPE_KIND(val) != REB_0);
        if (!MT_Unit(D_OUT, NULL, VAL_TYPE_KIND(val)))
            assert(FALSE);
        return R_OUT;
    }

    assert(!IS_VOID(val));

    switch (action) {
    case A_TAIL_Q:
        return R_TRUE;

    case A_INDEX_OF:
    case A_LENGTH:
    case A_SELECT:
    case A_FIND:
    case A_REMOVE:
    case A_CLEAR:
    case A_TAKE:
        return R_BLANK;

    default:
        break;
    }

    fail (Error_Illegal_Action(VAL_TYPE(val), action));
}
