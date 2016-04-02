//
//  File: %t-bitset.c
//  Summary: "bitset datatype"
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

#define MAX_BITSET 0x7fffffff

static inline REBOOL BITS_NOT(REBSER *s) {
    assert(s->misc.negated == TRUE || s->misc.negated == FALSE);
    return s->misc.negated;
}

static inline void INIT_BITS_NOT(REBSER *s, REBOOL negated) {
    s->misc.negated = negated;
}


//
//  CT_Bitset: C
//
REBINT CT_Bitset(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    if (mode >= 0) return (
        BITS_NOT(VAL_SERIES(a)) == BITS_NOT(VAL_SERIES(b))
        &&
        Compare_Binary_Vals(a, b) == 0
    );
    return -1;
}


//
//  Make_Bitset: C
// 
// Return a bitset series (binary.
// 
// len: the # of bits in the bitset.
//
REBSER *Make_Bitset(REBCNT len)
{
    REBSER *ser;

    len = (len + 7) / 8;
    ser = Make_Binary(len);
    Clear_Series(ser);
    SET_SERIES_LEN(ser, len);
    INIT_BITS_NOT(ser, FALSE);

    return ser;
}


//
//  Mold_Bitset: C
//
void Mold_Bitset(const REBVAL *value, REB_MOLD *mold)
{
    REBSER *ser = VAL_SERIES(value);

    if (BITS_NOT(ser)) Append_Unencoded(mold->series, "[not bits ");
    Mold_Binary(value, mold);
    if (BITS_NOT(ser)) Append_Codepoint_Raw(mold->series, ']');
}


//
//  MT_Bitset: C
//
REBOOL MT_Bitset(
    REBVAL *out, RELVAL *data, REBCTX *specifier, enum Reb_Kind type
) {
    REBOOL is_not = FALSE;

    if (IS_BLOCK(data)) {
        REBINT len = Find_Max_Bit(data);
        REBSER *ser;

        if (len < 0 || len > 0xFFFFFF)
            fail (Error_Invalid_Arg_Core(data, specifier));

        ser = Make_Bitset(len);

        {
            REBVAL specific;
            COPY_VALUE(&specific, data, specifier);
            Set_Bits(ser, &specific, TRUE);
        }

        Val_Init_Bitset(out, ser);
        return TRUE;
    }

    if (!IS_BINARY(data)) return FALSE;

    Val_Init_Bitset(out, Copy_Sequence_At_Position(KNOWN(data)));
    INIT_BITS_NOT(VAL_SERIES(out), FALSE);
    return TRUE;
}


//
//  Find_Max_Bit: C
// 
// Return integer number for the maximum bit number defined by
// the value. Used to determine how much space to allocate.
//
REBINT Find_Max_Bit(RELVAL *val)
{
    REBINT maxi = 0;
    REBINT n;

    switch (VAL_TYPE(val)) {

    case REB_CHAR:
        maxi = VAL_CHAR(val) + 1;
        break;

    case REB_INTEGER:
        maxi = Int32s(KNOWN(val), 0);
        break;

    case REB_STRING:
    case REB_FILE:
    case REB_EMAIL:
    case REB_URL:
    case REB_TAG:
//  case REB_ISSUE:
        n = VAL_INDEX(val);
        if (VAL_BYTE_SIZE(val)) {
            REBYTE *bp = VAL_BIN(val);
            for (; n < cast(REBINT, VAL_LEN_HEAD(val)); n++)
                if (bp[n] > maxi) maxi = bp[n];
        }
        else {
            REBUNI *up = VAL_UNI(val);
            for (; n < cast(REBINT, VAL_LEN_HEAD(val)); n++)
                if (up[n] > maxi) maxi = up[n];
        }
        maxi++;
        break;

    case REB_BINARY:
        maxi = VAL_LEN_AT(val) * 8 - 1;
        if (maxi < 0) maxi = 0;
        break;

    case REB_BLOCK:
        for (val = VAL_ARRAY_AT(val); NOT_END(val); val++) {
            n = Find_Max_Bit(val);
            if (n > maxi) maxi = n;
        }
        //maxi++;
        break;

    case REB_BLANK:
        maxi = 0;
        break;

    default:
        return -1;
    }

    return maxi;
}


//
//  Check_Bit: C
// 
// Check bit indicated. Returns TRUE if set.
// If uncased is TRUE, try to match either upper or lower case.
//
REBOOL Check_Bit(REBSER *bset, REBCNT c, REBOOL uncased)
{
    REBCNT i, n = c;
    REBCNT tail = SER_LEN(bset);
    REBOOL flag = FALSE;

    if (uncased) {
        if (n >= UNICODE_CASES) uncased = FALSE; // no need to check
        else n = LO_CASE(c);
    }

    // Check lowercase char:
retry:
    i = n >> 3;
    if (i < tail)
        flag = LOGICAL(BIN_HEAD(bset)[i] & (1 << (7 - ((n) & 7))));

    // Check uppercase if needed:
    if (uncased && !flag) {
        n = UP_CASE(c);
        uncased = FALSE;
        goto retry;
    }

    return BITS_NOT(bset) ? NOT(flag) : flag;
}


//
//  Check_Bit_Str: C
// 
// If uncased is TRUE, try to match either upper or lower case.
//
REBOOL Check_Bit_Str(REBSER *bset, const REBVAL *val, REBOOL uncased)
{
    REBCNT n = VAL_INDEX(val);

    if (VAL_BYTE_SIZE(val)) {
        REBYTE *bp = VAL_BIN(val);
        for (; n < VAL_LEN_HEAD(val); n++)
            if (Check_Bit(bset, bp[n], uncased)) return TRUE;
    }
    else {
        REBUNI *up = VAL_UNI(val);
        for (; n < VAL_LEN_HEAD(val); n++)
            if (Check_Bit(bset, up[n], uncased)) return TRUE;
    }
    return FALSE;
}


//
//  Set_Bit: C
// 
// Set/clear a single bit. Expand if needed.
//
void Set_Bit(REBSER *bset, REBCNT n, REBOOL set)
{
    REBCNT i = n >> 3;
    REBCNT tail = SER_LEN(bset);
    REBYTE bit;

    // Expand if not enough room:
    if (i >= tail) {
        if (!set) return; // no need to expand
        Expand_Series(bset, tail, (i - tail) + 1);
        CLEAR(BIN_AT(bset, tail), (i - tail) + 1);
    }

    bit = 1 << (7 - ((n) & 7));
    if (set)
        BIN_HEAD(bset)[i] |= bit;
    else
        BIN_HEAD(bset)[i] &= ~bit;
}


//
//  Set_Bit_Str: C
//
void Set_Bit_Str(REBSER *bset, const REBVAL *val, REBOOL set)
{
    REBCNT n = VAL_INDEX(val);

    if (VAL_BYTE_SIZE(val)) {
        REBYTE *bp = VAL_BIN(val);
        for (; n < VAL_LEN_HEAD(val); n++)
            Set_Bit(bset, bp[n], set);
    }
    else {
        REBUNI *up = VAL_UNI(val);
        for (; n < VAL_LEN_HEAD(val); n++)
            Set_Bit(bset, up[n], set);
    }
}


//
//  Set_Bits: C
// 
// Set/clear bits indicated by strings and chars and ranges.
//
REBOOL Set_Bits(REBSER *bset, const REBVAL *val, REBOOL set)
{
    REBCNT n;
    REBCNT c;
    RELVAL *item;

    if (IS_CHAR(val)) {
        Set_Bit(bset, VAL_CHAR(val), set);
        return TRUE;
    }

    if (IS_INTEGER(val)) {
        n = Int32s(val, 0);
        if (n > MAX_BITSET) return FALSE;
        Set_Bit(bset, n, set);
        return TRUE;
    }

    if (ANY_BINSTR(val)) {
        Set_Bit_Str(bset, val, set);
        return TRUE;
    }

    if (!ANY_ARRAY(val))
        fail (Error_Invalid_Type(VAL_TYPE(val)));

    item = VAL_ARRAY_AT(val);
    if (NOT_END(item) && IS_WORD(item) && VAL_WORD_CANON(item) == SYM_NOT) {
        INIT_BITS_NOT(bset, TRUE);
        item++;
    }

    // Loop through block of bit specs:
    for (; NOT_END(item); item++) {

        switch (VAL_TYPE(item)) {

        case REB_CHAR:
            c = VAL_CHAR(item);

            // !!! Modified to check for END, is the `else` actually correct?
            //
            if (
                NOT_END(item + 1)
                && IS_WORD(item + 1)
                && VAL_WORD_CANON(item + 1) == SYM_HYPHEN
            ) {
                item += 2;
                if (IS_CHAR(item)) {
                    n = VAL_CHAR(item);
span_bits:
                    if (n < c) fail (Error(RE_PAST_END, item));
                    for (; c <= n; c++) Set_Bit(bset, c, set);
                }
                else
                    fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(val)));
            }
            else Set_Bit(bset, c, set);
            break;

        case REB_INTEGER:
            n = Int32s(KNOWN(item), 0);
            if (n > MAX_BITSET) return FALSE;
            if (IS_WORD(item + 1) && VAL_WORD_CANON(item + 1) == SYM_HYPHEN) {
                c = n;
                item += 2;
                if (IS_INTEGER(item)) {
                    n = Int32s(KNOWN(item), 0);
                    goto span_bits;
                }
                else
                    fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(val)));
            }
            else Set_Bit(bset, n, set);
            break;

        case REB_BINARY:
        case REB_STRING:
        case REB_FILE:
        case REB_EMAIL:
        case REB_URL:
        case REB_TAG:
//      case REB_ISSUE:
            Set_Bit_Str(bset, KNOWN(item), set);
            break;

        case REB_WORD:
            // Special: BITS #{000...}
            if (!IS_WORD(item) || VAL_WORD_CANON(item) != SYM_BITS)
                return FALSE;
            item++;
            if (!IS_BINARY(item)) return FALSE;
            n = VAL_LEN_AT(item);
            c = SER_LEN(bset);
            if (n >= c) {
                Expand_Series(bset, c, (n - c));
                CLEAR(BIN_AT(bset, c), (n - c));
            }
            memcpy(BIN_HEAD(bset), VAL_BIN_AT(item), n);
            break;

        default:
            return FALSE;
        }
    }

    return TRUE;
}


//
//  Check_Bits: C
// 
// Check bits indicated by strings and chars and ranges.
// If uncased is TRUE, try to match either upper or lower case.
//
REBOOL Check_Bits(REBSER *bset, const REBVAL *val, REBOOL uncased)
{
    REBCNT n;
    REBUNI c;
    RELVAL *item;

    if (IS_CHAR(val))
        return Check_Bit(bset, VAL_CHAR(val), uncased);

    if (IS_INTEGER(val))
        return Check_Bit(bset, Int32s(val, 0), uncased);

    if (ANY_BINSTR(val))
        return Check_Bit_Str(bset, val, uncased);

    if (!ANY_ARRAY(val))
        fail (Error_Invalid_Type(VAL_TYPE(val)));

    // Loop through block of bit specs:
    for (item = VAL_ARRAY_AT(val); NOT_END(item); item++) {

        switch (VAL_TYPE(item)) {

        case REB_CHAR:
            c = VAL_CHAR(item);
            if (IS_WORD(item + 1) && VAL_WORD_CANON(item + 1) == SYM_HYPHEN) {
                item += 2;
                if (IS_CHAR(item)) {
                    n = VAL_CHAR(item);
scan_bits:
                    if (n < c) fail (Error(RE_PAST_END, item));
                    for (; c <= n; c++)
                        if (Check_Bit(bset, c, uncased)) goto found;
                }
                else
                    fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(val)));
            }
            else
                if (Check_Bit(bset, c, uncased)) goto found;
            break;

        case REB_INTEGER:
            n = Int32s(KNOWN(item), 0);
            if (n > 0xffff) return FALSE;
            if (IS_WORD(item + 1) && VAL_WORD_CANON(item + 1) == SYM_HYPHEN) {
                c = n;
                item += 2;
                if (IS_INTEGER(item)) {
                    n = Int32s(KNOWN(item), 0);
                    goto scan_bits;
                }
                else
                    fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(val)));
            }
            else
                if (Check_Bit(bset, n, uncased)) goto found;
            break;

        case REB_BINARY:
        case REB_STRING:
        case REB_FILE:
        case REB_EMAIL:
        case REB_URL:
        case REB_TAG:
//      case REB_ISSUE:
            if (Check_Bit_Str(bset, KNOWN(item), uncased)) goto found;
            break;

        default:
            fail (Error_Invalid_Type(VAL_TYPE(item)));
        }
    }
    return FALSE;

found:
    return TRUE;
}


//
//  PD_Bitset: C
//
REBINT PD_Bitset(REBPVS *pvs)
{
    REBSER *ser = VAL_SERIES(pvs->value);

    if (!pvs->opt_setval) {
        if (Check_Bits(ser, pvs->selector, FALSE)) {
            SET_TRUE(pvs->store);
            return PE_USE_STORE;
        }
        return PE_NONE;
    }

    if (Set_Bits(
        ser,
        pvs->selector,
        BITS_NOT(ser)
            ? IS_CONDITIONAL_FALSE(pvs->opt_setval)
            : IS_CONDITIONAL_TRUE(pvs->opt_setval)
    )) {
        return PE_OK;
    }

    fail (Error_Bad_Path_Set(pvs));
}


//
//  Trim_Tail_Zeros: C
// 
// Remove extra zero bytes from end of byte string.
//
void Trim_Tail_Zeros(REBSER *ser)
{
    REBCNT len = SER_LEN(ser);
    REBYTE *bp = BIN_HEAD(ser);

    while (len > 0 && bp[len] == 0)
        len--;

    if (bp[len] != 0)
        len++;

    SET_SERIES_LEN(ser, len);
}


//
//  REBTYPE: C
//
REBTYPE(Bitset)
{
    REBVAL *value = D_ARG(1);
    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;
    REBSER *ser;
    REBINT len;
    REBOOL diff;

    // Check must be in this order (to avoid checking a non-series value);
    if (action >= A_TAKE && action <= A_SORT)
        FAIL_IF_LOCKED_SERIES(VAL_SERIES(value));

    switch (action) {

    // Define PICK for BITSETS?  PICK's set bits and returns #?
    // Add AND, OR, XOR

    case A_PICK:
    case A_FIND:
        if (!Check_Bits(VAL_SERIES(value), arg, D_REF(ARG_FIND_CASE))) return R_BLANK;
        return R_TRUE;

    case A_COMPLEMENT:
    case A_NEGATE:
        ser = Copy_Sequence(VAL_SERIES(value));
        INIT_BITS_NOT(ser, NOT(BITS_NOT(VAL_SERIES(value))));
        Val_Init_Bitset(value, ser);
        break;

    case A_MAKE:
    case A_TO:
        // Determine size of bitset. Returns -1 for errors.
        len = Find_Max_Bit(arg);
        if (len < 0 || len > 0x0FFFFFFF) fail (Error_Invalid_Arg(arg));

        ser = Make_Bitset(len);
        Val_Init_Bitset(value, ser);

        // Nothing more to do.
        if (IS_INTEGER(arg)) break;

        if (IS_BINARY(arg)) {
            memcpy(BIN_HEAD(ser), VAL_BIN_AT(arg), len/8 + 1);
            break;
        }
        // FALL THRU...

    case A_APPEND:  // Accepts: #"a" "abc" [1 - 10] [#"a" - #"z"] etc.
    case A_INSERT:
        diff = TRUE;
        goto set_bits;

    case A_POKE:
        if (!IS_LOGIC(D_ARG(3)))
            fail (Error_Invalid_Arg(D_ARG(3)));
        diff = VAL_LOGIC(D_ARG(3));
set_bits:
        if (BITS_NOT(VAL_SERIES(value))) diff = NOT(diff);
        if (Set_Bits(VAL_SERIES(value), arg, diff)) break;
        fail (Error_Invalid_Arg(arg));

    case A_REMOVE:  // #"a" "abc"  remove/part bs "abcd"  yuk: /part ?
        if (!D_REF(2)) fail (Error(RE_MISSING_ARG)); // /part required
        if (Set_Bits(VAL_SERIES(value), D_ARG(3), FALSE)) break;
        fail (Error_Invalid_Arg(D_ARG(3)));

    case A_COPY:
        Val_Init_Series_Index(
            D_OUT,
            REB_BITSET,
            Copy_Sequence_At_Position(value),
            VAL_INDEX(value)
        );
        INIT_BITS_NOT(VAL_SERIES(D_OUT), BITS_NOT(VAL_SERIES(value)));
        return R_OUT;

    case A_LENGTH:
        len = VAL_LEN_HEAD(value) * 8;
        SET_INTEGER(value, len);
        break;

    case A_TAIL_Q:
        // Necessary to make EMPTY? work:
        return (VAL_LEN_HEAD(value) == 0) ? R_TRUE : R_FALSE;

    case A_CLEAR:
        Clear_Series(VAL_SERIES(value));
        break;

    case A_AND_T:
    case A_OR_T:
    case A_XOR_T:
        if (!IS_BITSET(arg) && !IS_BINARY(arg))
            fail (Error_Math_Args(VAL_TYPE(arg), action));
        ser = Xandor_Binary(action, value, arg);
        Trim_Tail_Zeros(ser);
        Val_Init_Series(D_OUT, VAL_TYPE(value), ser);
        return R_OUT;

    default:
        fail (Error_Illegal_Action(REB_BITSET, action));
    }

    *D_OUT = *value;
    return R_OUT;
}

