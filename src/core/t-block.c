//
//  File: %t-block.c
//  Summary: "block related datatypes"
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
//  CT_Array: C
// 
// "Compare Type" dispatcher for the following types:
// 
//     CT_Block(REBVAL *a, REBVAL *b, REBINT mode)
//     CT_Group(REBVAL *a, REBVAL *b, REBINT mode)
//     CT_Path(REBVAL *a, REBVAL *b, REBINT mode)
//     CT_Set_Path(REBVAL *a, REBVAL *b, REBINT mode)
//     CT_Get_Path(REBVAL *a, REBVAL *b, REBINT mode)
//     CT_Lit_Path(REBVAL *a, REBVAL *b, REBINT mode)
//
REBINT CT_Array(const REBVAL *a, const REBVAL *b, REBINT mode)
{
    REBINT num;

    num = Cmp_Block(a, b, LOGICAL(mode == 1));
    if (mode >= 0) return (num == 0);
    if (mode == -1) return (num >= 0);
    return (num > 0);
}

static void No_Nones(REBVAL *arg) {
    arg = VAL_ARRAY_AT(arg);
    for (; NOT_END(arg); arg++) {
        if (IS_BLANK(arg)) fail (Error_Invalid_Arg(arg));
    }
}


//
//  MT_Array: C
// 
// "Make Type" dispatcher for the following subtypes:
// 
//     MT_Block
//     MT_Group
//     MT_Path
//     MT_Set_Path
//     MT_Get_Path
//     MT_Lit_Path
//
REBOOL MT_Array(REBVAL *out, REBVAL *data, enum Reb_Kind type)
{
    REBCNT i;

    if (!ANY_ARRAY(data)) return FALSE;
    if (type >= REB_PATH && type <= REB_LIT_PATH) {
        REBVAL *head = VAL_ARRAY_HEAD(data);
        if (IS_END(head) || !ANY_WORD(head))
            return FALSE;
    }

    *out = *data++;
    VAL_RESET_HEADER(out, type);

    // !!! This did not have special END handling previously, but it would have
    // taken the 0 branch.  Review if this is sensible.
    //
    i = NOT_END(data) && IS_INTEGER(data) ? Int32(data) - 1 : 0;

    if (i > VAL_LEN_HEAD(out)) i = VAL_LEN_HEAD(out); // clip it
    VAL_INDEX(out) = i;
    return TRUE;
}


//
//  Find_In_Array: C
// 
// Flags are set according to: ALL_FIND_REFS
// 
// Main Parameters:
// start - index to start search
// end   - ending position
// len   - length of target
// skip  - skip factor
// dir   - direction
// 
// Comparison Parameters:
// case  - case sensitivity
// wild  - wild cards/keys
// 
// Final Parmameters:
// tail  - tail position
// match - sequence
// SELECT - (value that follows)
//
REBCNT Find_In_Array(
    REBARR *array,
    REBCNT index,
    REBCNT end,
    const REBVAL *target,
    REBCNT len,
    REBCNT flags,
    REBINT skip
) {
    REBVAL *value;
    REBVAL *val;
    REBCNT cnt;
    REBCNT start = index;

    if (flags & (AM_FIND_REVERSE | AM_FIND_LAST)) {
        skip = -1;
        start = 0;
        if (flags & AM_FIND_LAST) index = end - len;
        else index--;
    }

    // Optimized find word in block:
    if (ANY_WORD(target)) {
        for (; index >= start && index < end; index += skip) {
            value = ARR_AT(array, index);
            if (ANY_WORD(value)) {
                cnt = (VAL_WORD_SYM(value) == VAL_WORD_SYM(target));
                if (flags & AM_FIND_CASE) {
                    // Must be same type and spelling:
                    if (cnt && VAL_TYPE(value) == VAL_TYPE(target)) return index;
                }
                else {
                    // Can be different type or alias:
                    if (cnt || VAL_WORD_CANON(value) == VAL_WORD_CANON(target)) return index;
                }
            }
            if (flags & AM_FIND_MATCH) break;
        }
        return NOT_FOUND;
    }
    // Match a block against a block:
    else if (ANY_ARRAY(target) && !(flags & AM_FIND_ONLY)) {
        for (; index >= start && index < end; index += skip) {
            cnt = 0;
            value = ARR_AT(array, index);
            for (val = VAL_ARRAY_AT(target); NOT_END(val); val++, value++) {
                if (0 != Cmp_Value(value, val, LOGICAL(flags & AM_FIND_CASE)))
                    break;
                if (++cnt >= len) {
                    return index;
                }
            }
            if (flags & AM_FIND_MATCH) break;
        }
        return NOT_FOUND;
    }
    // Find a datatype in block:
    else if (IS_DATATYPE(target) || IS_TYPESET(target)) {
        for (; index >= start && index < end; index += skip) {
            value = ARR_AT(array, index);
            // Used if's so we can trace it...
            if (IS_DATATYPE(target)) {
                if (VAL_TYPE(value) == VAL_TYPE_KIND(target)) return index;
                if (IS_DATATYPE(value) && VAL_TYPE_KIND(value) == VAL_TYPE_KIND(target)) return index;
            }
            if (IS_TYPESET(target)) {
                if (TYPE_CHECK(target, VAL_TYPE(value))) return index;
                if (IS_DATATYPE(value) && TYPE_CHECK(target, VAL_TYPE_KIND(value))) return index;
                if (IS_TYPESET(value) && EQUAL_TYPESET(value, target)) return index;
            }
            if (flags & AM_FIND_MATCH) break;
        }
        return NOT_FOUND;
    }
    // All other cases:
    else {
        for (; index >= start && index < end; index += skip) {
            value = ARR_AT(array, index);
            if (0 == Cmp_Value(value, target, LOGICAL(flags & AM_FIND_CASE)))
                return index;
            if (flags & AM_FIND_MATCH) break;
        }
        return NOT_FOUND;
    }
}


//
//  Make_Block_Type_Throws: C
// 
// Arg can be:
//     1. integer (length of block)
//     2. block (copy it)
//     3. value (convert to a block)
//
REBOOL Make_Block_Type_Throws(
    REBVAL *out,
    enum Reb_Kind type,
    REBOOL make,
    REBVAL *arg
) {
    REBCNT len;
    REBARR *array;

    // make block! [1 2 3]
    if (ANY_ARRAY(arg)) {
        len = VAL_ARRAY_LEN_AT(arg);
        if (len > 0 && type >= REB_PATH && type <= REB_LIT_PATH)
            No_Nones(arg);
        array = Copy_Values_Len_Shallow(
            VAL_ARRAY_AT(arg), VAL_SPECIFIER(arg), len
        );
        goto done;
    }

    if (IS_STRING(arg)) {
        REBCNT index, len = 0;
        REBSER *temp = Temp_Bin_Str_Managed(arg, &index, &len);
        INIT_VAL_SERIES(arg, temp); // caution: macro copies args!
        array = Scan_Source(VAL_BIN(arg), VAL_LEN_AT(arg));
        goto done;
    }

    if (IS_BINARY(arg)) {
        array = Scan_Source(VAL_BIN_AT(arg), VAL_LEN_AT(arg));
        goto done;
    }

    if (IS_MAP(arg)) {
        array = Map_To_Array(VAL_MAP(arg), 0);
        goto done;
    }

    if (ANY_CONTEXT(arg)) {
        array = Context_To_Array(VAL_CONTEXT(arg), 3);
        goto done;
    }

    if (IS_VECTOR(arg)) {
        array = Vector_To_Array(arg);
        goto done;
    }

//  if (make && IS_BLANK(arg)) {
//      array = Make_Array(0);
//      goto done;
//  }

    if (IS_VARARGS(arg)) {
        //
        // Converting a VARARGS! to an ANY-ARRAY! involves spooling those
        // varargs to the end and making an array out of that.  It's not known
        // how many elements that will be, so they're gathered to the data
        // stack to find the size, then an array made.  Note that | will stop
        // a normal or soft-quoted varargs, but a hard-quoted varargs will
        // grab all the values to the end of the source.

        REBDSP dsp_orig = DSP;

        REBARR *feed;
        REBARR **subfeed_addr;

        const REBVAL *param;
        REBVAL fake_param;

        // !!! This MAKE will be destructive to its input (the varargs will
        // be fetched and exhausted).  That's not necessarily obvious, but
        // with a TO conversion it would be even less obvious...
        //
        if (!make)
            fail (Error(RE_VARARGS_MAKE_ONLY));

        if (GET_VAL_FLAG(arg, VARARGS_FLAG_NO_FRAME)) {
            feed = VAL_VARARGS_ARRAY1(arg);

            // Just a vararg created from a block, so no typeset or quoting
            // settings available.  Make a fake parameter that hard quotes
            // and takes any type (it will be type checked if in a chain).
            //
            Val_Init_Typeset(&fake_param, ALL_64, SYM_ELLIPSIS);
            INIT_VAL_PARAM_CLASS(&fake_param, PARAM_CLASS_HARD_QUOTE);
            param = &fake_param;
        }
        else {
            feed = CTX_VARLIST(VAL_VARARGS_FRAME_CTX(arg));
            param = VAL_VARARGS_PARAM(arg);
        }

        do {
            REBIXO indexor = Do_Vararg_Op_Core(
                out, feed, param, arg, SYM_0, VARARG_OP_TAKE
            );

            if (indexor == THROWN_FLAG) {
                DS_DROP_TO(dsp_orig);
                return TRUE;
            }
            if (indexor == END_FLAG)
                break;
            assert(indexor == VALIST_FLAG);

            DS_PUSH(out);
        } while (TRUE);

        Val_Init_Array(out, type, Pop_Stack_Values(dsp_orig));

        if (FALSE) {
            //
            // !!! If desired, input could be fed back into the varargs
            // after exhaustion by setting it up with array data as a new
            // subfeed.  Probably doesn't make sense to re-feed with data
            // that has been evaluated, and subfeeds can't be fixed up
            // like this either...disabled for now.
            //
            subfeed_addr = SUBFEED_ADDR_OF_FEED(feed);
            assert(*subfeed_addr == NULL); // all values should be exhausted
            *subfeed_addr = Make_Singular_Array(out);
            MANAGE_ARRAY(*subfeed_addr);
            *SUBFEED_ADDR_OF_FEED(*subfeed_addr) = NULL;
        }

        return FALSE;
    }

    // to block! typset
    if (!make && IS_TYPESET(arg) && type == REB_BLOCK) {
        Val_Init_Array(out, type, Typeset_To_Array(arg));
        return FALSE;
    }

    if (make) {
        // make block! 10
        if (IS_INTEGER(arg) || IS_DECIMAL(arg)) {
            len = Int32s(arg, 0);
            Val_Init_Array(out, type, Make_Array(len));
            return FALSE;
        }
        fail (Error_Invalid_Arg(arg));
    }

    array = Copy_Values_Len_Shallow(arg, SPECIFIED, 1); // REBVAL, known

done:
    Val_Init_Array(out, type, array);
    return FALSE;
}


// WARNING! Not re-entrant. !!!  Must find a way to push it on stack?
// Fields initialized to zero due to global scope
static struct {
    REBOOL cased;
    REBOOL reverse;
    REBCNT offset;
    REBVAL *compare;
} sort_flags;


//
//  Compare_Val: C
//
static int Compare_Val(void *thunk, const void *v1, const void *v2)
{
    // !!!! BE SURE that 64 bit large difference comparisons work

    if (sort_flags.reverse)
        return Cmp_Value(
            cast(const REBVAL*, v2) + sort_flags.offset,
            cast(const REBVAL*, v1) + sort_flags.offset,
            sort_flags.cased
        );
    else
        return Cmp_Value(
            cast(const REBVAL*, v1) + sort_flags.offset,
            cast(const REBVAL*, v2) + sort_flags.offset,
            sort_flags.cased
        );

/*
    REBI64 n = VAL_INT64((REBVAL*)v1) - VAL_INT64((REBVAL*)v2);
    if (n > 0) return 1;
    if (n < 0) return -1;
    return 0;
*/
}


//
//  Compare_Call: C
//
static int Compare_Call(void *thunk, const void *v1, const void *v2)
{
    REBVAL *args = NULL;
    REBINT tristate = -1;
    const void *tmp = NULL;

    REBVAL result;

    if (!sort_flags.reverse) { /*swap v1 and v2 */
        tmp = v1;
        v1 = v2;
        v2 = tmp;
    }

    args = ARR_AT(VAL_FUNC_PARAMLIST(sort_flags.compare), 1);
    if (NOT_END(args) && !TYPE_CHECK(args, VAL_TYPE(cast(const REBVAL*, v1)))) {
        fail (Error(
            RE_EXPECT_ARG,
            Type_Of(sort_flags.compare),
            args,
            Type_Of(cast(const REBVAL*, v1))
        ));
    }
    ++ args;
    if (NOT_END(args) && !TYPE_CHECK(args, VAL_TYPE(cast(const REBVAL*, v2)))) {
        fail (Error(
            RE_EXPECT_ARG,
            Type_Of(sort_flags.compare),
            args,
            Type_Of(cast(const REBVAL*, v2))
        ));
    }

    if (Apply_Only_Throws(
        &result,
        sort_flags.compare,
        v1,
        v2,
        END_CELL
    )) {
        fail (Error_No_Catch_For_Throw(&result));
    }

    if (IS_LOGIC(&result)) {
        if (VAL_LOGIC(&result)) tristate = 1;
    }
    else if (IS_INTEGER(&result)) {
        if (VAL_INT64(&result) > 0) tristate = 1;
        if (VAL_INT64(&result) == 0) tristate = 0;
    }
    else if (IS_DECIMAL(&result)) {
        if (VAL_DECIMAL(&result) > 0) tristate = 1;
        if (VAL_DECIMAL(&result) == 0) tristate = 0;
    }
    else if (IS_CONDITIONAL_TRUE(&result)) tristate = 1;

    return tristate;
}


//
//  Sort_Block: C
// 
// series [any-series!]
// /case {Case sensitive sort}
// /skip {Treat the series as records of fixed size}
// size [integer!] {Size of each record}
// /compare  {Comparator offset, block or function}
// comparator [integer! block! function!]
// /part {Sort only part of a series}
// limit [any-number! any-series!] {Length of series to sort}
// /all {Compare all fields}
// /reverse {Reverse sort order}
//
static void Sort_Block(
    REBVAL *block,
    REBOOL ccase,
    REBVAL *skipv,
    REBVAL *compv,
    REBVAL *part,
    REBOOL all,
    REBOOL rev
) {
    REBCNT len;
    REBCNT skip = 1;
    REBCNT size = sizeof(REBVAL);
//  int (*sfunc)(const void *v1, const void *v2);

    sort_flags.cased = ccase;
    sort_flags.reverse = rev;
    sort_flags.compare = 0;
    sort_flags.offset = 0;

    if (IS_INTEGER(compv)) sort_flags.offset = Int32(compv)-1;
    if (IS_FUNCTION(compv)) sort_flags.compare = compv;

    // Determine length of sort:
    len = Partial1(block, part);
    if (len <= 1) return;

    // Skip factor:
    if (!IS_VOID(skipv)) {
        skip = Get_Num_From_Arg(skipv);
        if (skip <= 0 || len % skip != 0 || skip > len)
            fail (Error_Out_Of_Range(skipv));
    }

    // Use fast quicksort library function:
    if (skip > 1) len /= skip, size *= skip;

    if (sort_flags.compare)
        reb_qsort_r(VAL_ARRAY_AT(block), len, size, NULL, Compare_Call);
    else
        reb_qsort_r(VAL_ARRAY_AT(block), len, size, NULL, Compare_Val);

}


//
//  Trim_Array: C
// 
// See Trim_String().
//
static void Trim_Array(REBARR *array, REBCNT index, REBCNT flags)
{
    REBVAL *head = ARR_HEAD(array);
    REBCNT out = index;
    REBCNT end = ARR_LEN(array);

    if (flags & AM_TRIM_TAIL) {
        for (; end >= (index + 1); end--) {
            if (VAL_TYPE(head + end - 1) > REB_BLANK) break;
        }
        Remove_Series(ARR_SERIES(array), end, ARR_LEN(array) - end);
        if (!(flags & AM_TRIM_HEAD) || index >= end) return;
    }

    if (flags & AM_TRIM_HEAD) {
        for (; index < end; index++) {
            if (VAL_TYPE(head + index) > REB_BLANK) break;
        }
        Remove_Series(ARR_SERIES(array), out, index - out);
    }

    if (flags == 0) {
        for (; index < end; index++) {
            if (VAL_TYPE(head + index) > REB_BLANK) {
                *ARR_AT(array, out) = head[index];
                out++;
            }
        }
        Remove_Series(ARR_SERIES(array), out, end - out);
    }
}


//
//  Shuffle_Block: C
//
void Shuffle_Block(REBVAL *value, REBOOL secure)
{
    REBCNT n;
    REBCNT k;
    REBCNT idx = VAL_INDEX(value);
    REBVAL *data = VAL_ARRAY_HEAD(value);
    REBVAL swap;

    for (n = VAL_LEN_AT(value); n > 1;) {
        k = idx + (REBCNT)Random_Int(secure) % n;
        n--;
        swap = data[k];
        data[k] = data[n + idx];
        data[n + idx] = swap;
    }
}


//
//  PD_Array: C
// 
// Path dispatch for the following types:
// 
//     PD_Block
//     PD_Group
//     PD_Path
//     PD_Get_Path
//     PD_Set_Path
//     PD_Lit_Path
//
REBINT PD_Array(REBPVS *pvs)
{
    REBINT n = 0;

    /* Issues!!!
        a/1.3
        a/not-found: 10 error or append?
        a/not-followed: 10 error or append?
    */

    if (IS_INTEGER(pvs->selector)) {
        n = Int32(pvs->selector) + VAL_INDEX(pvs->value) - 1;
    }
    else if (IS_WORD(pvs->selector)) {
        n = Find_Word_In_Array(
            VAL_ARRAY(pvs->value),
            VAL_INDEX(pvs->value),
            VAL_WORD_CANON(pvs->selector)
        );
        if (cast(REBCNT, n) != NOT_FOUND) n++;
    }
    else {
        // other values:
        n = 1 + Find_In_Array_Simple(
            VAL_ARRAY(pvs->value),
            VAL_INDEX(pvs->value),
            pvs->selector
        );
    }

    if (n < 0 || cast(REBCNT, n) >= VAL_LEN_HEAD(pvs->value)) {
        if (pvs->opt_setval)
            fail(Error_Bad_Path_Select(pvs));

        return PE_NONE;
    }

    if (pvs->opt_setval)
        FAIL_IF_LOCKED_SERIES(VAL_SERIES(pvs->value));

    pvs->value = VAL_ARRAY_AT_HEAD(pvs->value, n);

    return PE_SET_IF_END;
}


//
//  Pick_Block: C
//
REBVAL *Pick_Block(const REBVAL *block, const REBVAL *selector)
{
    REBINT n = 0;

    n = Get_Num_From_Arg(selector);
    n += VAL_INDEX(block) - 1;
    if (n < 0 || cast(REBCNT, n) >= VAL_LEN_HEAD(block)) return 0;
    return VAL_ARRAY_AT_HEAD(block, n);
}


//
//  REBTYPE: C
// 
// Implementation of type dispatch of the following:
// 
//     REBTYPE(Block)
//     REBTYPE(Group)
//     REBTYPE(Path)
//     REBTYPE(Get_Path)
//     REBTYPE(Set_Path)
//     REBTYPE(Lit_Path)
//
REBTYPE(Array)
{
    REBVAL *value = D_ARG(1);
    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;

    REBARR *array;
    REBINT index;

    // Support for port: OPEN [scheme: ...], READ [ ], etc.
    if (action >= PORT_ACTIONS && IS_BLOCK(value))
        return T_Port(frame_, action);

    // Common operations for any series type (length, head, etc.)
    {
        REB_R r;
        if (Series_Common_Action_Returns(&r, frame_, action))
            return r;
    }

    // Special case (to avoid fetch of index and tail below):
    if (action == A_MAKE || action == A_TO) {
        //
        // make block! ...
        // to block! ...
        //
        assert(IS_DATATYPE(value));

        if (
            Make_Block_Type_Throws(
                value, // out
                VAL_TYPE_KIND(value), // type
                LOGICAL(action == A_MAKE), // make? (as opposed to to?)
                arg // size, block to copy, or value to convert
            )
        ) {
            *D_OUT = *value;
            return R_OUT_IS_THROWN;
        }

        if (ANY_PATH(value)) {
            // Get rid of any line break options on the path's elements
            REBVAL *clear = VAL_ARRAY_HEAD(value);
            for (; NOT_END(clear); clear++) {
                CLEAR_VAL_FLAG(clear, VALUE_FLAG_LINE);
            }
        }
        *D_OUT = *value;
        return R_OUT;
    }

    // Extract the array and the index from value.
    //
    // NOTE: Partial1() used below can mutate VAL_INDEX(value), be aware :-/
    //
    array = VAL_ARRAY(value);
    index = cast(REBINT, VAL_INDEX(value));

    switch (action) {
    case A_POKE:
    case A_PICK: {
pick_using_arg:
        value = Pick_Block(value, arg);
        if (action == A_PICK) {
            if (!value)
                return R_VOID;

            *D_OUT = *value;
        } else {
            FAIL_IF_LOCKED_ARRAY(array);
            if (!value) fail (Error_Out_Of_Range(arg));
            arg = D_ARG(3);
            *value = *arg;
            *D_OUT = *arg;
        }
        return R_OUT;
    }

    case A_TAKE: {
        REFINE(2, part);
        PARAM(3, limit);
        REFINE(4, deep);
        REFINE(5, last);

        REBINT len;

        FAIL_IF_LOCKED_ARRAY(array);

        if (REF(part)) {
            len = Partial1(value, ARG(limit));
            if (len == 0)
                goto return_empty_block;
        } else
            len = 1;

        index = VAL_INDEX(value); // /part can change index

        if (REF(last))
            index = VAL_LEN_HEAD(value) - len;

        if (index < 0 || index >= cast(REBINT, VAL_LEN_HEAD(value))) {
            if (!REF(part))
                return R_VOID;

            goto return_empty_block;
        }

        // if no /part, just return value, else return block:
        if (!REF(part))
            *D_OUT = ARR_HEAD(array)[index];
        else
            Val_Init_Block(
                D_OUT, Copy_Array_At_Max_Shallow(array, index, GUESSED, len)
            );
        Remove_Series(ARR_SERIES(array), index, len);
        return R_OUT;
    }

    //-- Search:

    case A_FIND:
    case A_SELECT: {
        REBCNT args = Find_Refines(frame_, ALL_FIND_REFS);
        REBINT len = ANY_ARRAY(arg) ? VAL_ARRAY_LEN_AT(arg) : 1;

        REBCNT ret;
        REBINT limit;

        if (args & AM_FIND_PART)
            limit = Partial1(value, D_ARG(ARG_FIND_LIMIT));
        else
            limit = cast(REBINT, VAL_LEN_HEAD(value));

        ret = 1;
        if (args & AM_FIND_SKIP) ret = Int32s(D_ARG(ARG_FIND_SIZE), 1);
        ret = Find_In_Array(array, index, limit, arg, len, args, ret);

        if (ret >= cast(REBCNT, limit)) {
            if (action == A_FIND) return R_BLANK;
            return R_VOID;
        }
        if (args & AM_FIND_ONLY) len = 1;
        if (action == A_FIND) {
            if (args & (AM_FIND_TAIL | AM_FIND_MATCH)) ret += len;
            VAL_INDEX(value) = ret;
        }
        else {
            ret += len;
            if (ret >= cast(REBCNT, limit)) {
                if (action == A_FIND) return R_BLANK;
                return R_VOID;
            }
            value = ARR_AT(array, ret);
        }
        *D_OUT = *value;
        return R_OUT;
    }

    //-- Modification:
    case A_APPEND:
    case A_INSERT:
    case A_CHANGE: {
        REBCNT args = 0;

        // Length of target (may modify index): (arg can be anything)
        //
        REBINT len = Partial1(
            (action == A_CHANGE)
                ? value
                : arg,
            D_ARG(AN_LIMIT)
        );

        FAIL_IF_LOCKED_ARRAY(array);
        index = VAL_INDEX(value);

        if (D_REF(AN_ONLY)) SET_FLAG(args, AN_ONLY);
        if (D_REF(AN_PART)) SET_FLAG(args, AN_PART);
        index = Modify_Array(
            action,
            array,
            index,
            arg,
            args,
            len,
            D_REF(AN_DUP) ? Int32(D_ARG(AN_COUNT)) : 1
        );
        VAL_INDEX(value) = index;
        *D_OUT = *value;
        return R_OUT;
    }

    case A_CLEAR: {
        FAIL_IF_LOCKED_ARRAY(array);
        if (index < cast(REBINT, VAL_LEN_HEAD(value))) {
            if (index == 0) Reset_Array(array);
            else {
                SET_END(ARR_AT(array, index));
                SET_SERIES_LEN(VAL_SERIES(value), cast(REBCNT, index));
            }
        }
        *D_OUT = *value;
        return R_OUT;
    }

    //-- Creation:

    case A_COPY: {
        REFINE(2, part);
        PARAM(3, limit);
        REFINE(4, deep);
        REFINE(5, types);
        PARAM(6, kinds);

        REBU64 types = 0;
        REBARR *copy;

        if (REF(deep)) {
            types |= REF(types) ? 0 : TS_STD_SERIES;
        }
        if (REF(types)) {
            if (IS_DATATYPE(ARG(kinds)))
                types |= FLAGIT_KIND(VAL_TYPE(ARG(kinds)));
            else
                types |= VAL_TYPESET_BITS(ARG(kinds));
        }
        copy = Copy_Array_Core_Managed(
            array,
            VAL_INDEX(value), // at
            GUESSED,
            VAL_INDEX(value) + Partial1(value, ARG(limit)), // tail
            0, // extra
            REF(deep), // deep
            types // types
        );
        Val_Init_Array(D_OUT, VAL_TYPE(value), copy);
        return R_OUT;
    }

    //-- Special actions:

    case A_TRIM: {
        REBCNT args = Find_Refines(frame_, ALL_TRIM_REFS);
        FAIL_IF_LOCKED_ARRAY(array);

        if (args & ~(AM_TRIM_HEAD|AM_TRIM_TAIL)) fail (Error(RE_BAD_REFINES));
        Trim_Array(array, index, args);
        *D_OUT = *value;
        return R_OUT;
    }

    case A_SWAP: {
        if (!ANY_ARRAY(arg))
            fail (Error_Invalid_Arg(arg));

        FAIL_IF_LOCKED_ARRAY(array);
        FAIL_IF_LOCKED_ARRAY(VAL_ARRAY(arg));

        if (
            index < cast(REBINT, VAL_LEN_HEAD(value))
            && VAL_INDEX(arg) < VAL_LEN_HEAD(arg)
        ) {
            REBVAL temp = *VAL_ARRAY_AT(value);
            *VAL_ARRAY_AT(value) = *VAL_ARRAY_AT(arg);
            *VAL_ARRAY_AT(arg) = temp;
        }
        *D_OUT = *D_ARG(1);
        return R_OUT;
    }

    case A_REVERSE: {
        REBINT len = Partial1(value, D_ARG(3));

        FAIL_IF_LOCKED_ARRAY(array);

        if (len != 0) {
            value = VAL_ARRAY_AT(value);
            arg = value + len - 1;
            for (len /= 2; len > 0; len--) {
                REBVAL temp = *value;
                *value++ = *arg;
                *arg-- = temp;
            }
        }
        *D_OUT = *D_ARG(1);
        return R_OUT;
    }

    case A_SORT: {
        FAIL_IF_LOCKED_ARRAY(array);
        Sort_Block(
            value,
            D_REF(2),   // case sensitive
            D_ARG(4),   // skip size
            D_ARG(6),   // comparator
            D_ARG(8),   // part-length
            D_REF(9),   // all fields
            D_REF(10)   // reverse
        );
        *D_OUT = *value;
        return R_OUT;
    }

    case A_RANDOM: {
        REFINE(2, seed);
        REFINE(3, secure);
        REFINE(4, only);

        if (REF(seed)) fail (Error(RE_BAD_REFINES));

        if (REF(only)) { // pick an element out of the array
            if (index >= cast(REBINT, VAL_LEN_HEAD(value)))
                return R_BLANK;

            SET_INTEGER(
                ARG(seed),
                1 + (Random_Int(REF(secure)) % (VAL_LEN_HEAD(value) - index))
            );
            arg = ARG(seed); // argument to pick
            action = A_PICK;
            goto pick_using_arg;
        }

        Shuffle_Block(value, REF(secure));
        *D_OUT = *value;
        return R_OUT;
    }

    default:
        break; // fallthrough to error
    }

    fail (Error_Illegal_Action(VAL_TYPE(value), action));

return_empty_block:
    Val_Init_Block(D_OUT, Make_Array(0));
    return R_OUT;
}


#if !defined(NDEBUG)

//
//  Assert_Array_Core: C
//
void Assert_Array_Core(REBARR *array)
{
    REBCNT len;

    // Basic integrity checks (series is not marked free, etc.)  Note that
    // we don't use ASSERT_SERIES the macro here, because that checks to
    // see if the series is an array...and if so, would call this routine
    //
    Assert_Series_Core(ARR_SERIES(array));

    if (!Is_Array_Series(ARR_SERIES(array)))
        Panic_Array(array);

    for (len = 0; len < ARR_LEN(array); len++) {
        REBVAL *value = ARR_AT(array, len);

        if (IS_END(value)) {
            // Premature end
            Panic_Array(array);
        }
    }

    if (NOT_END(ARR_AT(array, ARR_LEN(array)))) {
        // Not legal to not have an END! at all
        Panic_Array(array);
    }
}
#endif
