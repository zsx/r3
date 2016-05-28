//
//  File: %t-logic.c
//  Summary: "logic datatype"
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
/*
**  Symbolic bit logic was experimental - but proved not to add much
**  value because the overhead of access offset the savings of storage.
**  It would be better to add a general purpose bit parsing dialect,
**  somewhat similar to R2's struct datatype.
*/

#include "sys-core.h"
#include "sys-deci-funcs.h"


//
//  CT_Logic: C
//
REBINT CT_Logic(const REBVAL *a, const REBVAL *b, REBINT mode)
{
    if (mode >= 0)  return (VAL_LOGIC(a) == VAL_LOGIC(b));
    return -1;
}


//
//  MT_Logic: C
//
REBOOL MT_Logic(REBVAL *out, REBVAL *data, enum Reb_Kind type)
{
    if (!IS_INTEGER(data)) return FALSE;
    SET_LOGIC(out, VAL_INT64(data) != 0);
    return TRUE;
}


//
//  REBTYPE: C
//
REBTYPE(Logic)
{
    REBOOL val1 = VAL_LOGIC(D_ARG(1));
    REBOOL val2;
    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;

    if (IS_BINARY_ACT(action)) {
        if (IS_LOGIC(arg))
            val2 = VAL_LOGIC(arg);
        else if (IS_BLANK(arg))
            val2 = FALSE;
        else
            fail (Error_Unexpected_Type(REB_LOGIC, VAL_TYPE(arg)));
    }

    switch (action) {

    case A_AND_T:
        val1 = LOGICAL(val1 && val2);
        break;

    case A_OR_T:
        val1 = LOGICAL(val1 || val2);
        break;

    case A_XOR_T:
        val1 = LOGICAL(!val1 != !val2);
        break;

    case A_COMPLEMENT:
        val1 = NOT(val1);
        break;

    case A_RANDOM:
        if (D_REF(2)) { // /seed
            // random/seed false restarts; true randomizes
            Set_Random(val1 ? (REBINT)OS_DELTA_TIME(0, 0) : 1);
            return R_VOID;
        }
        if (Random_Int(D_REF(3)) & 1) // /secure
            return R_TRUE;
        return R_FALSE;

    case A_TO:
        // As a "Rebol conversion", TO falls in line with the rest of the
        // interpreter canon that all non-blank non-logic values are
        // considered effectively "truth".
        //
        if (IS_CONDITIONAL_TRUE(arg))
            return R_TRUE;
        return R_FALSE;

    case A_MAKE:
        // As a construction routine, MAKE takes more liberties in the
        // meaning of its parameters, so it lets zero values be false.
        //
        // !!! Is there a better idea for MAKE that does not hinge on the
        // "zero is false" concept?  Is there a reason it should?
        //
        if (
            IS_CONDITIONAL_FALSE(arg)
            || (IS_INTEGER(arg) && VAL_INT64(arg) == 0)
            || (
                (IS_DECIMAL(arg) || IS_PERCENT(arg))
                && (VAL_DECIMAL(arg) == 0.0)
            )
            || (IS_MONEY(arg) && deci_is_zero(VAL_MONEY_AMOUNT(arg)))
        ) {
            return R_FALSE;
        }
        return R_TRUE;

    default:
        fail (Error_Illegal_Action(REB_LOGIC, action));
    }

    return val1 ? R_TRUE : R_FALSE;
}
