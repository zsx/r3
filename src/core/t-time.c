//
//  File: %t-time.c
//  Summary: "time datatype"
//  Section: datatypes
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

#include "sys-core.h"

//
//  Split_Time: C
//
void Split_Time(REBI64 t, REB_TIMEF *tf)
{
    // note: negative sign will be lost.
    REBI64 h, m, s, n, i;

    if (t < 0) t = -t;

    h = t / HR_SEC;
    i = t - (h * HR_SEC);
    m = i / MIN_SEC;
    i = i - (m * MIN_SEC);
    s = i / SEC_SEC;
    n = i - (s * SEC_SEC);

    tf->h = (REBCNT)h;
    tf->m = (REBCNT)m;
    tf->s = (REBCNT)s;
    tf->n = (REBCNT)n;
}

//
//  Join_Time: C
//
// !! A REB_TIMEF has lost the sign bit available on the REBI64
// used for times.  If you want to make it negative, you need
// pass in a flag here.  (Flag added to help document the
// issue, as previous code falsely tried to judge the sign
// of tf->h, which is always positive.)
//
REBI64 Join_Time(REB_TIMEF *tf, REBOOL neg)
{
    REBI64 t;

    t = (tf->h * HR_SEC) + (tf->m * MIN_SEC) + (tf->s * SEC_SEC) + tf->n;
    return neg ? -t : t;
}

//
//  Scan_Time: C
//
// Scan string and convert to time.  Return zero if error.
//
const REBYTE *Scan_Time(REBVAL *out, const REBYTE *cp, REBCNT len)
{
    TRASH_CELL_IF_DEBUG(out);
    cast(void, len); // !!! should len be paid attention to?

    REBOOL neg;
    if (*cp == '-') {
        ++cp;
        neg = TRUE;
    }
    else if (*cp == '+') {
        ++cp;
        neg = FALSE;
    }
    else
        neg = FALSE;

    if (*cp == '-' || *cp == '+')
        return NULL; // small hole: --1:23

    // Can be:
    //    HH:MM       as part1:part2
    //    HH:MM:SS    as part1:part2:part3
    //    HH:MM:SS.DD as part1:part2:part3.part4
    //    MM:SS.DD    as part1:part2.part4

    REBINT part1 = -1;
    cp = Grab_Int(cp, &part1);
    if (part1 > MAX_HOUR)
        return NULL;

    if (*cp++ != ':')
        return NULL;

    const REBYTE *sp;

    REBINT part2 = -1;
    sp = Grab_Int(cp, &part2);
    if (part2 < 0 || sp == cp)
        return NULL;

    cp = sp;

    REBINT part3 = -1;
    if (*cp == ':') {   // optional seconds
        sp = cp + 1;
        cp = Grab_Int(sp, &part3);
        if (part3 < 0 || cp == sp)
            return NULL;
    }

    REBINT part4 = -1;
    if (*cp == '.' || *cp == ',') {
        sp = ++cp;
        cp = Grab_Int_Scale(sp, &part4, 9);
        if (part4 == 0)
            part4 = -1;
    }

    REBYTE merid;
    if (
        (UP_CASE(*cp) == 'A' || UP_CASE(*cp) == 'P')
        && (UP_CASE(cp[1]) == 'M')
    ){
        merid = cast(REBYTE, UP_CASE(*cp));
        cp += 2;
    }
    else
        merid = '\0';

    VAL_RESET_HEADER(out, REB_TIME);

    if (part3 >= 0 || part4 < 0) { // HH:MM mode
        if (merid != '\0') {
            if (part1 > 12)
                return NULL;

            if (part1 == 12)
                part1 = 0;

            if (merid == 'P')
                part1 += 12;
        }

        if (part3 < 0)
            part3 = 0;

        VAL_NANO(out) = HOUR_TIME(part1) + MIN_TIME(part2) + SEC_TIME(part3);
    }
    else {
        // MM:SS mode

        if (merid != '\0')
            return NULL; // no AM/PM for minutes

        VAL_NANO(out) = MIN_TIME(part1) + SEC_TIME(part2);
    }

    if (part4 > 0)
        VAL_NANO(out) += part4;

    if (neg)
        VAL_NANO(out) = -VAL_NANO(out);

    return cp;
}


//
//  MF_Time: C
//
void MF_Time(REB_MOLD *mo, const RELVAL *v, REBOOL form)
{
    UNUSED(form); // no difference between MOLD and FORM at this time

    REB_TIMEF tf;
    Split_Time(VAL_NANO(v), &tf); // loses sign

    const char *fmt;
    if (tf.s == 0 && tf.n == 0)
        fmt = "I:2";
    else
        fmt = "I:2:2";

    if (VAL_NANO(v) < cast(REBI64, 0))
        Append_Codepoint_Raw(mo->series, '-');

    Emit(mo, fmt, tf.h, tf.m, tf.s, 0);

    if (tf.n > 0)
        Emit(mo, ".i", tf.n);
}


//
//  CT_Time: C
//
REBINT CT_Time(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    REBINT num = Cmp_Time(a, b);
    if (mode >= 0)  return (num == 0);
    if (mode == -1) return (num >= 0);
    return (num > 0);
}


//
//  MAKE_Time: C
//
void MAKE_Time(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    assert(kind == REB_TIME);
    UNUSED(kind);

    switch (VAL_TYPE(arg)) {
    case REB_TIME: // just copy it (?)
        Move_Value(out, arg);
        return;

    case REB_STRING: { // scan using same decoding as LOAD would
        REBCNT len;
        REBYTE *bp = Temp_Byte_Chars_May_Fail(arg, MAX_SCAN_TIME, &len, FALSE);

        if (Scan_Time(out, bp, len) == NULL)
            goto no_time;

        return; }

    case REB_INTEGER: // interpret as seconds
        if (VAL_INT64(arg) < -MAX_SECONDS || VAL_INT64(arg) > MAX_SECONDS)
            fail (Error_Out_Of_Range(arg));

        Init_Time_Nanoseconds(out, VAL_INT64(arg) * SEC_SEC);
        return;

    case REB_DECIMAL:
        if (
            VAL_DECIMAL(arg) < cast(REBDEC, -MAX_SECONDS)
            || VAL_DECIMAL(arg) > cast(REBDEC, MAX_SECONDS)
        ){
            fail (Error_Out_Of_Range(arg));
        }
        Init_Time_Nanoseconds(out, DEC_TO_SECS(VAL_DECIMAL(arg)));
        return;

    case REB_BLOCK: { // [hh mm ss]
        if (VAL_ARRAY_LEN_AT(arg) > 3)
            goto no_time;

        RELVAL *item = VAL_ARRAY_AT(arg);
        if (NOT(IS_INTEGER(item)))
            goto no_time;

        REBOOL neg;
        REBI64 i = Int32(item);
        if (i < 0) {
            i = -i;
            neg = TRUE;
        }
        else
            neg = FALSE;

        REBI64 secs = i * 3600;
        if (secs > MAX_SECONDS)
            goto no_time;

        if (NOT_END(++item)) {
            if (NOT(IS_INTEGER(item)))
                goto no_time;

            if ((i = Int32(item)) < 0)
                goto no_time;

            secs += i * 60;
            if (secs > MAX_SECONDS)
                goto no_time;

            if (NOT_END(++item)) {
                if (IS_INTEGER(item)) {
                    if ((i = Int32(item)) < 0)
                        goto no_time;

                    secs += i;
                    if (secs > MAX_SECONDS)
                        goto no_time;
                }
                else if (IS_DECIMAL(item)) {
                    if (
                        secs + cast(REBI64, VAL_DECIMAL(item)) + 1
                        > MAX_SECONDS
                    ){
                        goto no_time;
                    }

                    // added in below
                }
                else
                    goto no_time;
            }
        }

        REBI64 nano = secs * SEC_SEC;
        if (IS_DECIMAL(item))
            nano += DEC_TO_SECS(VAL_DECIMAL(item));

        if (neg)
            nano = -nano;

        Init_Time_Nanoseconds(out, nano);
        return; }

    default:
        goto no_time;
    }

no_time:
    fail (Error_Bad_Make(REB_TIME, arg));
}


//
//  TO_Time: C
//
void TO_Time(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    MAKE_Time(out, kind, arg);
}


//
//  Cmp_Time: C
//
// Given two TIME!s (or DATE!s with a time componet), compare them.
//
REBINT Cmp_Time(const RELVAL *v1, const RELVAL *v2)
{
    REBI64 t1 = VAL_NANO(v1);
    REBI64 t2 = VAL_NANO(v2);

    if (t2 == t1)
        return 0;
    if (t1 > t2)
        return 1;
    return -1;
}


//
//  Pick_Time: C
//
void Pick_Time(REBVAL *out, const REBVAL *value, const REBVAL *picker)
{
    REBINT i;
    if (IS_WORD(picker)) {
        switch (VAL_WORD_SYM(picker)) {
        case SYM_HOUR:   i = 0; break;
        case SYM_MINUTE: i = 1; break;
        case SYM_SECOND: i = 2; break;
        default:
            fail (picker);
        }
    }
    else if (IS_INTEGER(picker))
        i = VAL_INT32(picker) - 1;
    else
        fail (picker);

    REB_TIMEF tf;
    Split_Time(VAL_NANO(value), &tf); // loses sign

    switch(i) {
    case 0: // hours
        Init_Integer(out, tf.h);
        break;
    case 1: // minutes
        Init_Integer(out, tf.m);
        break;
    case 2: // seconds
        if (tf.n == 0)
            Init_Integer(out, tf.s);
        else
            Init_Decimal(out, cast(REBDEC, tf.s) + (tf.n * NANO));
        break;
    default:
        Init_Void(out); // "out of range" behavior for pick
    }
}


//
//  Poke_Time_Immediate: C
//
void Poke_Time_Immediate(
    REBVAL *value,
    const REBVAL *picker,
    const REBVAL *poke
) {
    REBINT i;
    if (IS_WORD(picker)) {
        switch (VAL_WORD_SYM(picker)) {
        case SYM_HOUR:   i = 0; break;
        case SYM_MINUTE: i = 1; break;
        case SYM_SECOND: i = 2; break;
        default:
            fail (picker);
        }
    }
    else if (IS_INTEGER(picker))
        i = VAL_INT32(picker) - 1;
    else
        fail (picker);

    REB_TIMEF tf;
    Split_Time(VAL_NANO(value), &tf); // loses sign

    REBINT n;
    if (IS_INTEGER(poke) || IS_DECIMAL(poke))
        n = Int32s(poke, 0);
    else if (IS_BLANK(poke))
        n = 0;
    else
        fail (poke);

    switch(i) {
    case 0:
        tf.h = n;
        break;
    case 1:
        tf.m = n;
        break;
    case 2:
        if (IS_DECIMAL(poke)) {
            REBDEC f = VAL_DECIMAL(poke);
            if (f < 0.0)
                fail (Error_Out_Of_Range(poke));

            tf.s = cast(REBINT, f);
            tf.n = cast(REBINT, (f - tf.s) * SEC_SEC);
        }
        else {
            tf.s = n;
            tf.n = 0;
        }
        break;
    default:
        fail (picker);
    }

    VAL_NANO(value) = Join_Time(&tf, FALSE);
}


//
//  PD_Time: C
//
REBINT PD_Time(REBPVS *pvs)
{
    if (pvs->opt_setval) {
        //
        // !!! Since TIME! is an immediate value, allowing a SET-PATH! will
        // modify the result of the expression but not the source.
        //
        Poke_Time_Immediate(KNOWN(pvs->value), pvs->picker, pvs->opt_setval);
        return PE_OK;
    }

    Pick_Time(pvs->store, KNOWN(pvs->value), pvs->picker);
    return PE_USE_STORE;
}


//
//  REBTYPE: C
//
REBTYPE(Time)
{
    REBVAL *val = D_ARG(1);

    REBI64 secs = VAL_NANO(val);

    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;

    // !!! This used to use IS_BINARY_ACT(), which is not available under
    // the symbol-based dispatch.  Consider doing another way.
    //
    if (
        action == SYM_ADD
        || action == SYM_SUBTRACT
        || action == SYM_MULTIPLY
        || action == SYM_DIVIDE
        || action == SYM_REMAINDER
    ){
        REBINT  type = VAL_TYPE(arg);

        assert(arg);

        if (type == REB_TIME) {     // handle TIME - TIME cases
            REBI64 secs2 = VAL_NANO(arg);

            switch (action) {

            case SYM_ADD:
                secs = Add_Max(REB_TIME, secs, secs2, MAX_TIME);
                goto fixTime;

            case SYM_SUBTRACT:
                secs = Add_Max(REB_TIME, secs, -secs2, MAX_TIME);
                goto fixTime;

            case SYM_DIVIDE:
                if (secs2 == 0) fail (Error_Zero_Divide_Raw());
                //secs /= secs2;
                VAL_RESET_HEADER(D_OUT, REB_DECIMAL);
                VAL_DECIMAL(D_OUT) = (REBDEC)secs / (REBDEC)secs2;
                return R_OUT;

            case SYM_REMAINDER:
                if (secs2 == 0) fail (Error_Zero_Divide_Raw());
                secs %= secs2;
                goto setTime;

            default:
                fail (Error_Math_Args(REB_TIME, action));
            }
        }
        else if (type == REB_INTEGER) {     // handle TIME - INTEGER cases
            REBI64 num = VAL_INT64(arg);

            switch(action) {
            case SYM_ADD:
                secs = Add_Max(REB_TIME, secs, num * SEC_SEC, MAX_TIME);
                goto fixTime;

            case SYM_SUBTRACT:
                secs = Add_Max(REB_TIME, secs, num * -SEC_SEC, MAX_TIME);
                goto fixTime;

            case SYM_MULTIPLY:
                secs *= num;
                if (secs < -MAX_TIME || secs > MAX_TIME)
                    fail (Error_Type_Limit_Raw(Get_Type(REB_TIME)));
                goto setTime;

            case SYM_DIVIDE:
                if (num == 0) fail (Error_Zero_Divide_Raw());
                secs /= num;
                Init_Integer(D_OUT, secs);
                goto setTime;

            case SYM_REMAINDER:
                if (num == 0) fail (Error_Zero_Divide_Raw());
                secs %= num;
                goto setTime;

            default:
                fail (Error_Math_Args(REB_TIME, action));
            }
        }
        else if (type == REB_DECIMAL) {     // handle TIME - DECIMAL cases
            REBDEC dec = VAL_DECIMAL(arg);

            switch(action) {
            case SYM_ADD:
                secs = Add_Max(REB_TIME, secs, (i64)(dec * SEC_SEC), MAX_TIME);
                goto fixTime;

            case SYM_SUBTRACT:
                secs = Add_Max(REB_TIME, secs, (i64)(dec * -SEC_SEC), MAX_TIME);
                goto fixTime;

            case SYM_MULTIPLY:
                secs = (REBI64)(secs * dec);
                goto setTime;

            case SYM_DIVIDE:
                if (dec == 0.0) fail (Error_Zero_Divide_Raw());
                secs = (REBI64)(secs / dec);
                goto setTime;

//          case SYM_REMAINDER:
//              ld = fmod(ld, VAL_DECIMAL(arg));
//              goto decTime;

            default:
                fail (Error_Math_Args(REB_TIME, action));
            }
        }
        else if (type == REB_DATE && action == SYM_ADD) { // TIME + DATE case
            // Swap args and call DATE datatupe:
            Move_Value(D_ARG(3), val); // (temporary location for swap)
            Move_Value(D_ARG(1), arg);
            Move_Value(D_ARG(2), D_ARG(3));
            return T_Date(frame_, action);
        }
        fail (Error_Math_Args(REB_TIME, action));
    }
    else {
        // unary actions
        switch(action) {

        case SYM_ODD_Q:
            return ((SECS_FROM_NANO(secs) & 1) != 0) ? R_TRUE : R_FALSE;

        case SYM_EVEN_Q:
            return ((SECS_FROM_NANO(secs) & 1) == 0) ? R_TRUE : R_FALSE;

        case SYM_NEGATE:
            secs = -secs;
            goto setTime;

        case SYM_ABSOLUTE:
            if (secs < 0) secs = -secs;
            goto setTime;

        case SYM_ROUND: {
            INCLUDE_PARAMS_OF_ROUND;

            UNUSED(PAR(value));

            REBFLGS flags = (
                (REF(to) ? RF_TO : 0)
                | (REF(even) ? RF_EVEN : 0)
                | (REF(down) ? RF_DOWN : 0)
                | (REF(half_down) ? RF_HALF_DOWN : 0)
                | (REF(floor) ? RF_FLOOR : 0)
                | (REF(ceiling) ? RF_CEILING : 0)
                | (REF(half_ceiling) ? RF_HALF_CEILING : 0)
            );

            if (REF(to)) {
                arg = ARG(scale);
                if (IS_TIME(arg)) {
                    secs = Round_Int(secs, flags, VAL_NANO(arg));
                }
                else if (IS_DECIMAL(arg)) {
                    VAL_DECIMAL(arg) = Round_Dec(
                        cast(REBDEC, secs),
                        flags,
                        Dec64(arg) * SEC_SEC
                    );
                    VAL_DECIMAL(arg) /= SEC_SEC;
                    VAL_RESET_HEADER(arg, REB_DECIMAL);
                    Move_Value(D_OUT, ARG(scale));
                    return R_OUT;
                }
                else if (IS_INTEGER(arg)) {
                    VAL_INT64(arg) = Round_Int(secs, 1, Int32(arg) * SEC_SEC) / SEC_SEC;
                    VAL_RESET_HEADER(arg, REB_INTEGER);
                    Move_Value(D_OUT, ARG(scale));
                    return R_OUT;
                }
                else
                    fail (arg);
            }
            else {
                secs = Round_Int(secs, flags | RF_TO, SEC_SEC);
            }
            goto fixTime; }

        case SYM_RANDOM: {
            INCLUDE_PARAMS_OF_RANDOM;

            UNUSED(PAR(value));

            if (REF(only))
                fail (Error_Bad_Refines_Raw());

            if (REF(seed)) {
                Set_Random(secs);
                return R_VOID;
            }
            secs = Random_Range(secs / SEC_SEC, REF(secure)) * SEC_SEC;
            goto fixTime; }

        default:
            break;
        }
    }
    fail (Error_Illegal_Action(REB_TIME, action));

fixTime:
setTime:
    VAL_RESET_HEADER(D_OUT, REB_TIME);
    VAL_NANO(D_OUT) = secs;
    return R_OUT;
}
