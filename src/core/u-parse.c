//
//  File: %u-parse.c
//  Summary: "parse dialect interpreter"
//  Section: utility
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

#define P_RULE          (f->value + 0) // rvalue
#define P_RULE_LVALUE   (f->value) // lvalue
#define P_TYPE          VAL_TYPE(&f->arg[0])
#define P_INPUT         VAL_SERIES(&f->arg[0])
#define P_POS           VAL_INDEX(&f->arg[0])

#define P_FIND_FLAGS    VAL_INT64(&f->arg[1])
#define P_HAS_CASE      LOGICAL(P_FIND_FLAGS & AM_FIND_CASE)

#define P_RESULT        VAL_INT64(&f->arg[2])

// The workings of PARSE don't 100% parallel the DO evaluator, because it can
// go backwards.  Figuring out exactly the points at which it needs to go
// backwards and manage it (such as by copying the data) would be needed
// for things like stream parsing.
//
#define FETCH_NEXT_RULE_MAYBE_END(f) \
    do { \
        TRACE_FETCH_DEBUG("FETCH_NEXT_RULE_MAYBE_END", (f), FALSE); \
        ++((f)->value); \
        TRACE_FETCH_DEBUG("FETCH_NEXT_RULE_MAYBE_END", (f), TRUE); \
    } while(0)

#define FETCH_TO_BAR_MAYBE_END(f) \
    while (NOT_END(f->value) && !IS_BAR(f->value)) \
        { FETCH_NEXT_RULE_MAYBE_END(f); }

enum parse_flags {
    PF_SET,
    PF_COPY,
    PF_NOT,
    PF_NOT2,
    PF_THEN,
    PF_AND,
    PF_REMOVE,
    PF_INSERT,
    PF_CHANGE,
    PF_RETURN,
    PF_WHILE,
    PF_MAX
};

#define MAX_PARSE_DEPTH 512

// Returns SYMBOL or 0 if not a command:
#define GET_CMD(n) (((n) >= SYM_SET && (n) <= SYM_END) ? (n) : 0)
#define VAL_CMD(v) GET_CMD(VAL_WORD_CANON(v))

// Parse_Rules_Loop is used before it is defined, need forward declaration
//
static REBCNT Parse_Rules_Loop(struct Reb_Frame *f, REBCNT depth);


static void Print_Parse_Index(
    enum Reb_Kind type,
    const REBVAL rule[], // positioned at the current rule
    REBSER *series,
    REBCNT index
) {
    REBVAL item;
    VAL_INIT_WRITABLE_DEBUG(&item);
    Val_Init_Series(&item, type, series);
    VAL_INDEX(&item) = index;

    // Either the rules or the data could be positioned at the end.  The
    // data might even be past the end.
    //
    // !!! Or does PARSE adjust to ensure it never is past the end, e.g.
    // when seeking a position given in a variable or modifying?
    //
    if (IS_END(rule)) {
        if (index >= SER_LEN(series))
            Debug_Fmt("[]: ** END **");
        else
            Debug_Fmt("[]: %r", &item);
    }
    else {
        if (index >= SER_LEN(series))
            Debug_Fmt("%r: ** END **", rule);
        else
            Debug_Fmt("%r: %r", rule, &item);
    }
}


//
//  Set_Parse_Series: C
// 
// Change the series and return the new index.
//
static REBCNT Set_Parse_Series(struct Reb_Frame *f, const REBVAL *item)
{
    f->data.stackvars[0] = *item;
    VAL_INDEX(&f->data.stackvars[0]) = (VAL_INDEX(item) > VAL_LEN_HEAD(item))
        ? VAL_LEN_HEAD(item)
        : VAL_INDEX(item);

    if (IS_BINARY(item) || (P_FIND_FLAGS & AM_FIND_CASE))
        P_FIND_FLAGS |= AM_FIND_CASE;
    else
        P_FIND_FLAGS &= ~AM_FIND_CASE;

    return P_POS;
}


//
//  Get_Parse_Value: C
// 
// Get the value of a word (when not a command) or path.
// Returns all other values as-is.
// 
// !!! Because path evaluation does not necessarily wind up
// pointing to a variable that exists in memory, a derived
// value may be created during that process.  Previously
// this derived value was kept on the stack, but that
// meant every path evaluation PUSH'd without a known time
// at which a corresponding DROP would be performed.  To
// avoid the stack overflow, this requires you to pass in
// a "safe" storage value location that will be good for
// as long as the returned pointer is needed.  It *may*
// not be used in the case of a word fetch, so pay attention
// to the return value and not the contents of that variable.
// 
// !!! (Review if this can be done a better way.)
//
static const REBVAL *Get_Parse_Value(REBVAL *safe, const REBVAL *item)
{
    if (IS_BAR(item))
        return item;

    if (IS_WORD(item)) {
        const REBVAL *var;

        if (VAL_CMD(item)) return item;

        // If `item` is not bound, there will be a fail() during GET_VAR
        //
        var = GET_OPT_VAR_MAY_FAIL(item);

        // While NONE! is legal and represents a no-op in parse, if a
        // you write `parse "" [to undefined-value]`...and undefined-value
        // is bound...you may get an UNSET! back.  This should be an
        // error, as it is in the evaluator.  (See how this is handled
        // by REB_WORD in %c-do.c)
        //
        if (IS_VOID(var))
            fail (Error(RE_NO_VALUE, item));

        return var;
    }

    if (IS_PATH(item)) {
        //
        // !!! REVIEW: how should GET-PATH! be handled?

        if (Do_Path_Throws(safe, NULL, item, NULL))
            fail (Error_No_Catch_For_Throw(safe));

        // See notes above about UNSET!
        //
        if (IS_VOID(safe))
            fail (Error(RE_NO_VALUE, item));

        return safe;
    }

    return item;
}


//
//  Parse_Next_String: C
// 
// Match the next item in the string ruleset.
// 
// If it matches, return the index just past it.
// Otherwise return NOT_FOUND.
//
static REBCNT Parse_Next_String(
    struct Reb_Frame *f,
    REBCNT index,
    const REBVAL *item,
    REBCNT depth
) {
    REBSER *ser;
    REBCNT flags = P_FIND_FLAGS | AM_FIND_MATCH | AM_FIND_TAIL;

    REBVAL save;
    VAL_INIT_WRITABLE_DEBUG(&save);

    if (Trace_Level) {
        Trace_Value("input", item);

        // !!! This used STR_AT (obsolete) but it's not clear that this is
        // necessarily a byte sized series.  Switched to BIN_AT, which will
        // assert if it's not BYTE_SIZE()

        Trace_String(BIN_AT(P_INPUT, index), BIN_LEN(P_INPUT) - index);
    }

    if (IS_NONE(item)) return index;

    if (index >= SER_LEN(P_INPUT)) return NOT_FOUND;

    switch (VAL_TYPE(item)) {

    // Do we match a single character?
    case REB_CHAR:
        if (P_HAS_CASE) {
            if (VAL_CHAR(item) == GET_ANY_CHAR(P_INPUT, index))
                index = index + 1;
            else
                index = NOT_FOUND;
        }
        else {
            if (
                UP_CASE(VAL_CHAR(item))
                == UP_CASE(GET_ANY_CHAR(P_INPUT, index))
            ) {
                index = index + 1;
            }
            else
                index = NOT_FOUND;
        }
        break;

    case REB_EMAIL:
    case REB_STRING:
    case REB_BINARY:
        index = Find_Str_Str(
            P_INPUT,
            0,
            index,
            SER_LEN(P_INPUT),
            1,
            VAL_SERIES(item),
            VAL_INDEX(item),
            VAL_LEN_AT(item),
            flags
        );
        break;

    case REB_BITSET:
        if (Check_Bit(
            VAL_SERIES(item), GET_ANY_CHAR(P_INPUT, index), NOT(P_HAS_CASE)
        )) {
            // We matched to a char set, advance.
            //
            index++;
        }
        else
            index = NOT_FOUND;
        break;
/*
    case REB_DATATYPE:  // Currently: integer!
        if (VAL_TYPE_KIND(item) == REB_INTEGER) {
            REBCNT begin = index;
            while (IS_LEX_NUMBER(*str)) str++, index++;
            if (begin == index) index = NOT_FOUND;
        }
        break;
*/
    case REB_TAG:
    case REB_FILE:
//  case REB_ISSUE:
        // !! Can be optimized (w/o COPY)
        ser = Copy_Form_Value(item, 0);
        index = Find_Str_Str(
            P_INPUT,
            0,
            index,
            SER_LEN(P_INPUT),
            1,
            ser,
            0,
            SER_LEN(ser),
            flags
        );
        Free_Series(ser);
        break;

    case REB_NONE:
        break;

    // Parse a sub-rule block:
    case REB_BLOCK:
        {
        const REBVAL *rule_before = P_RULE;
        union Reb_Frame_Source source_before = f->source;
        REBIXO indexor_before = f->indexor;
        REBCNT pos_before = P_POS;

        P_RULE_LVALUE = VAL_ARRAY_AT(item);
        f->source.array = VAL_ARRAY(item);
        f->indexor = VAL_INDEX(item);
        P_POS = index;

        // !!! In DO this creates a new Reb_Frame (for GROUP!), but here nesting
        // doesn't.  Is there a reason why one needs it and the other doesn't?
        // If a separate stack level is not needed for debugging on a GROUP!
        // in DO, and it can't catch a throw, couldn't a similarly light
        // save of the index and array and restore do the job?
        //
        index = Parse_Rules_Loop(f, depth); // updates P_POS

        P_POS = pos_before;
        f->source = source_before;
        f->indexor = indexor_before;
        P_RULE_LVALUE = rule_before;
        }
        // index may be THROWN_FLAG
        break;

    // Do an expression:
    case REB_GROUP:
        // might GC
        if (DO_VAL_ARRAY_AT_THROWS(&save, item)) {
            *f->out = save;
            return THROWN_FLAG;
        }

        index = MIN(index, SER_LEN(P_INPUT)); // may affect tail
        break;

    default:
        fail (Error(RE_PARSE_RULE, item));
    }

    return index;
}


//
//  Parse_Next_Array: C
// 
// Used for parsing ANY-ARRAY! to match the next item in the ruleset.
// If it matches, return the index just past it. Otherwise, return zero.
//
static REBCNT Parse_Next_Array(
    struct Reb_Frame *f,
    REBCNT index,
    const REBVAL *item,
    REBCNT depth
) {
    // !!! THIS CODE NEEDS CLEANUP AND REWRITE BASED ON OTHER CHANGES
    REBARR *array = AS_ARRAY(P_INPUT);
    REBVAL *blk = ARR_AT(array, index);

    REBVAL save;
    VAL_INIT_WRITABLE_DEBUG(&save);

    if (Trace_Level) {
        Trace_Value("input", item);
        if (IS_END(blk)) {
            const char *end_str = "** END **";
            Trace_String(cb_cast(end_str), strlen(end_str));
        }
        else
            Trace_Value("match", blk);
    }

    // !!! The previous code did not have a handling for this, but it fell
    // through to `no_result`.  Is that correct?
    //
    if (IS_END(blk)) goto no_result;

    switch (VAL_TYPE(item)) {

    // Look for specific datattype:
    case REB_DATATYPE:
        index++;
        if (VAL_TYPE(blk) == VAL_TYPE_KIND(item)) break;
        goto no_result;

    // Look for a set of datatypes:
    case REB_TYPESET:
        index++;
        if (TYPE_CHECK(item, VAL_TYPE(blk))) break;
        goto no_result;

    // 'word
    case REB_LIT_WORD:
        index++;
        if (IS_WORD(blk) && (VAL_WORD_CANON(blk) == VAL_WORD_CANON(item)))
            break;
        goto no_result;

    case REB_LIT_PATH:
        index++;
        if (IS_PATH(blk) && !Cmp_Block(blk, item, FALSE)) break;
        goto no_result;

    case REB_NONE:
        break;

    // Parse a sub-rule block:
    case REB_BLOCK:
        {
        const REBVAL *rule_before = P_RULE;
        union Reb_Frame_Source source_before = f->source;
        REBIXO indexor_before = f->indexor;
        REBCNT pos_before = P_POS;

        P_POS = index;
        P_RULE_LVALUE = VAL_ARRAY_AT(item);
        f->source.array = VAL_ARRAY(item);
        f->indexor = VAL_INDEX(item);

        index = Parse_Rules_Loop(f, depth);

        P_POS = pos_before;
        f->value = rule_before;
        f->source = source_before;
        f->indexor = indexor_before;
        }
        // index may be THROWN_FLAG
        break;

    // Do an expression:
    case REB_GROUP:
        // might GC
        if (DO_VAL_ARRAY_AT_THROWS(&save, item)) {
            *f->out = save;
            return THROWN_FLAG;
        }
        // old: if (IS_ERROR(item)) Throw_Error(VAL_CONTEXT(item));
        index = MIN(index, ARR_LEN(array)); // may affect tail
        break;

    // Match with some other value:
    default:
        index++;
        if (Cmp_Value(blk, item, P_HAS_CASE)) goto no_result;
    }

    return index;

no_result:
    return NOT_FOUND;
}


//
//  To_Thru: C
//
static REBCNT To_Thru(
    struct Reb_Frame *f,
    REBCNT index,
    const REBVAL *block,
    REBOOL is_thru
) {
    REBVAL *blk;
    const REBVAL *item;
    REBCNT cmd;
    REBCNT i;
    REBCNT len;

    REBVAL save;
    VAL_INIT_WRITABLE_DEBUG(&save);

    for (; index <= SER_LEN(P_INPUT); index++) {

        for (blk = VAL_ARRAY_HEAD(block); NOT_END(blk); blk++) {

            item = blk;

            // Deal with words and commands
            if (IS_BAR(item)) {
                goto bad_target;
            }
            else if (IS_WORD(item)) {
                if ((cmd = VAL_CMD(item))) {
                    if (cmd == SYM_END) {
                        if (index >= SER_LEN(P_INPUT)) {
                            index = SER_LEN(P_INPUT);
                            goto found;
                        }
                        goto next;
                    }
                    else if (cmd == SYM_QUOTE) {
                        item = ++blk; // next item is the quoted value
                        if (IS_END(item)) goto bad_target;
                        if (IS_GROUP(item)) {
                            // might GC
                            if (DO_VAL_ARRAY_AT_THROWS(&save, item)) {
                                *f->out = save;
                                return THROWN_FLAG;
                            }
                            item = &save;
                        }
                    }
                    else goto bad_target;
                }
                else {
                    // !!! Should mutability be enforced?  It might have to
                    // be if set/copy are used...
                    item = GET_MUTABLE_VAR_MAY_FAIL(item);
                }
            }
            else if (IS_PATH(item)) {
                item = Get_Parse_Value(&save, item);
            }

            // Try to match it:
            if (P_TYPE >= REB_BLOCK) {
                if (ANY_ARRAY(item)) goto bad_target;
                i = Parse_Next_Array(f, index, item, 0);
                if (i == THROWN_FLAG)
                    return THROWN_FLAG;

                if (i != NOT_FOUND) {
                    if (!is_thru) i--;
                    index = i;
                    goto found;
                }
            }
            else if (P_TYPE == REB_BINARY) {
                REBYTE ch1 = *BIN_AT(P_INPUT, index);

                // Handle special string types:
                if (IS_CHAR(item)) {
                    if (VAL_CHAR(item) > 0xff) goto bad_target;
                    if (ch1 == VAL_CHAR(item)) goto found1;
                }
                else if (IS_BINARY(item)) {
                    if (ch1 == *VAL_BIN_AT(item)) {
                        len = VAL_LEN_AT(item);
                        if (len == 1) goto found1;
                        if (0 == Compare_Bytes(
                            BIN_AT(P_INPUT, index),
                            VAL_BIN_AT(item),
                            len,
                            FALSE
                        )) {
                            if (is_thru) index += len;
                            goto found;
                        }
                    }
                }
                else if (IS_INTEGER(item)) {
                    if (VAL_INT64(item) > 0xff) goto bad_target;
                    if (ch1 == VAL_INT32(item)) goto found1;
                }
                else goto bad_target;
            }
            else { // String
                REBCNT ch1 = GET_ANY_CHAR(P_INPUT, index);
                REBCNT ch2;

                if (!P_HAS_CASE) ch1 = UP_CASE(ch1);

                // Handle special string types:
                if (IS_CHAR(item)) {
                    ch2 = VAL_CHAR(item);
                    if (!P_HAS_CASE) ch2 = UP_CASE(ch2);
                    if (ch1 == ch2) goto found1;
                }
                // bitset
                else if (IS_BITSET(item)) {
                    if (Check_Bit(VAL_SERIES(item), ch1, NOT(P_HAS_CASE)))
                        goto found1;
                }
                else if (ANY_STRING(item)) {
                    ch2 = VAL_ANY_CHAR(item);
                    if (!P_HAS_CASE) ch2 = UP_CASE(ch2);

                    if (ch1 == ch2) {
                        len = VAL_LEN_AT(item);
                        if (len == 1) goto found1;

                        i = Find_Str_Str(
                            P_INPUT,
                            0,
                            index,
                            SER_LEN(P_INPUT),
                            1,
                            VAL_SERIES(item),
                            VAL_INDEX(item),
                            len,
                            AM_FIND_MATCH | P_FIND_FLAGS
                        );

                        if (i != NOT_FOUND) {
                            if (is_thru) i += len;
                            index = i;
                            goto found;
                        }
                    }
                }
                else if (IS_INTEGER(item)) {
                    ch1 = GET_ANY_CHAR(P_INPUT, index);  // No casing!
                    if (ch1 == (REBCNT)VAL_INT32(item)) goto found1;
                }
                else goto bad_target;
            }

        next:
            // Check for | (required if not end)
            blk++;
            if (IS_END(blk)) break;
            if (IS_GROUP(blk)) blk++;
            if (IS_END(blk)) break;
            if (!IS_BAR(blk)) {
                item = blk;
                goto bad_target;
            }
        }
    }
    return NOT_FOUND;

found:
    if (NOT_END(blk + 1) && IS_GROUP(blk + 1)) {
        REBVAL evaluated;
        VAL_INIT_WRITABLE_DEBUG(&evaluated);

        if (DO_VAL_ARRAY_AT_THROWS(&evaluated, blk + 1)) {
            *f->out = evaluated;
            return THROWN_FLAG;
        }
        // !!! ignore evaluated if it's not THROWN?
    }
    return index;

found1:
    if (NOT_END(blk + 1) && IS_GROUP(blk + 1)) {
        REBVAL evaluated;
        VAL_INIT_WRITABLE_DEBUG(&evaluated);

        if (DO_VAL_ARRAY_AT_THROWS(&evaluated, blk + 1)) {
            *f->out = save;
            return THROWN_FLAG;
        }
        // !!! ignore evaluated if it's not THROWN?
    }
    return index + (is_thru ? 1 : 0);

bad_target:
    fail (Error(RE_PARSE_RULE, item));
}


//
//  Parse_To: C
// 
// Parse TO a specific:
//     1. integer - index position
//     2. END - end of input
//     3. value - according to datatype
//     4. block of values - the first one we hit
//
static REBCNT Parse_To(
    struct Reb_Frame *f,
    REBCNT index,
    const REBVAL *item,
    REBOOL is_thru
) {
    REBCNT i;
    REBSER *ser;

    if (IS_INTEGER(item)) {
        //
        // TO a specific index position.
        //
        // !!! This allows jumping backward to an index before the parse
        // position, while TO generally only goes forward otherwise.  Should
        // this be done by another operation?  (Like SEEK?)
        //
        // !!! Negative numbers get cast to large integers, needs error!
        // But also, should there be an option for relative addressing?
        //
        i = cast(REBCNT, Int32(item)) - (is_thru ? 0 : 1);
        if (i > SER_LEN(P_INPUT))
            i = SER_LEN(P_INPUT);
    }
    else if (IS_WORD(item) && VAL_WORD_CANON(item) == SYM_END) {
        i = SER_LEN(P_INPUT);
    }
    else if (IS_BLOCK(item)) {
        i = To_Thru(f, index, item, is_thru);
    }
    else {
        if (Is_Array_Series(P_INPUT)) {
            REBVAL word; /// !!!Temp, but where can we put it?
            VAL_INIT_WRITABLE_DEBUG(&word);

            if (IS_LIT_WORD(item)) {  // patch to search for word, not lit.
                word = *item;

                // Only set type--don't reset the header, because that could
                // make the word binding inconsistent with the bits.
                //
                VAL_SET_TYPE_BITS(&word, REB_WORD);
                item = &word;
            }

            i = Find_In_Array(
                AS_ARRAY(P_INPUT),
                index,
                SER_LEN(P_INPUT),
                item,
                1,
                P_HAS_CASE ? AM_FIND_CASE : 0,
                1
            );

            if (i != NOT_FOUND && is_thru) i++;
        }
        else {
            // "str"
            if (ANY_BINSTR(item)) {
                if (!IS_STRING(item) && !IS_BINARY(item)) {
                    // !!! Can this be optimized not to use COPY?
                    ser = Copy_Form_Value(item, 0);
                    i = Find_Str_Str(
                        P_INPUT,
                        0,
                        index,
                        SER_LEN(P_INPUT),
                        1,
                        ser,
                        0,
                        SER_LEN(ser),
                        (P_FIND_FLAGS & AM_FIND_CASE)
                            ? AM_FIND_CASE
                            : 0
                    );
                    if (i != NOT_FOUND && is_thru) i += SER_LEN(ser);
                    Free_Series(ser);
                }
                else {
                    i = Find_Str_Str(
                        P_INPUT,
                        0,
                        index,
                        SER_LEN(P_INPUT),
                        1,
                        VAL_SERIES(item),
                        VAL_INDEX(item),
                        VAL_LEN_AT(item),
                        (P_FIND_FLAGS & AM_FIND_CASE)
                            ? AM_FIND_CASE
                            : 0
                    );
                    if (i != NOT_FOUND && is_thru) i += VAL_LEN_AT(item);
                }
            }
            else if (IS_CHAR(item)) {
                i = Find_Str_Char(
                    VAL_CHAR(item),
                    P_INPUT,
                    0,
                    index,
                    SER_LEN(P_INPUT),
                    1,
                    (P_FIND_FLAGS & AM_FIND_CASE)
                        ? AM_FIND_CASE
                        : 0
                );
                if (i != NOT_FOUND && is_thru) i++;
            }
            else if (IS_BITSET(item)) {
                i = Find_Str_Bitset(
                    P_INPUT,
                    0,
                    index,
                    SER_LEN(P_INPUT),
                    1,
                    VAL_BITSET(item),
                    (P_FIND_FLAGS & AM_FIND_CASE)
                        ? AM_FIND_CASE
                        : 0
                );
                if (i != NOT_FOUND && is_thru) i++;
            }
            else
                fail (Error(RE_PARSE_RULE, item));
        }
    }

    return i;
}


//
//  Do_Eval_Rule: C
// 
// Evaluate the input as a code block. Advance input if
// rule succeeds. Return new index or failure.
// 
// Examples:
//     do skip
//     do end
//     do "abc"
//     do 'abc
//     do [...]
//     do variable
//     do datatype!
//     do quote 123
//     do into [...]
// 
// Problem: cannot write:  set var do datatype!
//
static REBCNT Do_Eval_Rule(struct Reb_Frame *f)
{
    const REBVAL *item = P_RULE;
    REBCNT n;
    struct Reb_Frame newparse;

    REBIXO indexor = P_POS;

    REBVAL value;
    REBVAL save; // REVIEW: Could this just reuse value?
    VAL_INIT_WRITABLE_DEBUG(&value);
    VAL_INIT_WRITABLE_DEBUG(&save);

    // First, check for end of input
    //
    if (P_POS >= SER_LEN(P_INPUT)) {
        if (IS_WORD(item) && VAL_CMD(item) == SYM_END)
            return P_POS;

        return NOT_FOUND;
    }

    // Evaluate next expression, stop processing if BREAK/RETURN/QUIT/THROW...
    //
    DO_NEXT_MAY_THROW(indexor, &value, AS_ARRAY(P_INPUT), indexor);
    if (indexor == THROWN_FLAG) {
        *f->out = value;
        return THROWN_FLAG;
    }

    // Get variable or command:
    if (IS_WORD(item)) {

        n = VAL_CMD(item);

        if (n == SYM_SKIP)
            return IS_VOID(&value) ? NOT_FOUND : P_POS;

        if (n == SYM_QUOTE) {
            item = item + 1;
            FETCH_NEXT_RULE_MAYBE_END(f);
            if (IS_END(item)) fail (Error(RE_PARSE_END, item - 2));
            if (IS_GROUP(item)) {
                // might GC
                if (DO_VAL_ARRAY_AT_THROWS(&save, item)) {
                    *f->out = save;
                    return THROWN_FLAG;
                }
                item = &save;
            }
        }
        else if (n == SYM_INTO) {
            struct Reb_Frame sub_parse;
            REBCNT i;

            item = item + 1;
            FETCH_NEXT_RULE_MAYBE_END(f);

            if (IS_END(item))
                fail (Error(RE_PARSE_END, item - 2));

            item = Get_Parse_Value(&save, item); // sub-rules

            if (!IS_BLOCK(item))
                fail (Error(RE_PARSE_RULE, item - 2));

            if (!ANY_BINSTR(&value) && !ANY_ARRAY(&value))
                return NOT_FOUND;

            sub_parse.data.stackvars = Push_Ended_Trash_Chunk(3, NULL);
            sub_parse.data.stackvars[0] = value; // series
            SET_INTEGER(&sub_parse.data.stackvars[1], P_FIND_FLAGS); // fflags
            SET_INTEGER(&sub_parse.data.stackvars[2], 0); // result
            sub_parse.arg = sub_parse.data.stackvars;
            sub_parse.out = f->out;

            sub_parse.source.array = VAL_ARRAY(item);
            sub_parse.indexor = VAL_INDEX(item);
            sub_parse.value = VAL_ARRAY_AT(item);

            i = Parse_Rules_Loop(&sub_parse, 0);

            Drop_Chunk(sub_parse.data.stackvars);

            if (i == THROWN_FLAG) return THROWN_FLAG;

            if (i == VAL_LEN_HEAD(&value)) return P_POS;

            return NOT_FOUND;
        }
        else if (n > 0)
            fail (Error(RE_PARSE_RULE, item));
        else
            item = Get_Parse_Value(&save, item); // variable
    }
    else if (IS_PATH(item)) {
        item = Get_Parse_Value(&save, item); // variable
    }
    else if (
        IS_SET_WORD(item)
        || IS_GET_WORD(item)
        || IS_SET_PATH(item)
        || IS_GET_PATH(item)
    ) {
        fail (Error(RE_PARSE_RULE, item));
    }

    if (IS_NONE(item))
        return (VAL_TYPE(&value) > REB_NONE) ? NOT_FOUND : P_POS;

    // !!! This copies a single value into a block to use as data.  Is there
    // any way this might be avoided?
    //
    newparse.data.stackvars = Push_Ended_Trash_Chunk(3, NULL);
    Val_Init_Block_Index(
        &newparse.data.stackvars[0],
        Make_Array(1), // !!! "copy the value into its own block"
        0 // position 0
    ); // series (now a REB_BLOCK)
    Append_Value(AS_ARRAY(VAL_SERIES(&newparse.data.stackvars[0])), &value);
    SET_INTEGER(&newparse.data.stackvars[1], P_FIND_FLAGS); // find_flags
    SET_INTEGER(&newparse.data.stackvars[2], 0); // result
    newparse.arg = newparse.data.stackvars;
    newparse.out = f->out;

    newparse.source.array = f->source.array;
    newparse.indexor = f->indexor;
    newparse.value = item;

    {
    PUSH_GUARD_SERIES(VAL_SERIES(&newparse.data.stackvars[0]));
    n = Parse_Next_Array(&newparse, P_POS, item, 0);
    DROP_GUARD_SERIES(VAL_SERIES(&newparse.data.stackvars[0]));
    }

    if (n == THROWN_FLAG)
        return THROWN_FLAG;

    if (n == NOT_FOUND)
        return NOT_FOUND;

    return P_POS;
}


//
//  Parse_Rules_Loop: C
//
static REBCNT Parse_Rules_Loop(struct Reb_Frame *f, REBCNT depth) {
#if !defined(NDEBUG)
    //
    // These parse state variables live in chunk-stack REBVARs, which can be
    // annoying to find to inspect in the debugger.  This makes pointers into
    // the value payloads so they can be seen more easily.
    //
    const REBCNT *pos_debug = &P_POS;
    const REBI64 *result_debug = &P_RESULT;

    REBUPT do_count = TG_Do_Count; // helpful to cache for visibility also
#endif

    const REBVAL *item;     // current rule item
    const REBVAL *word;     // active word to be set
    REBCNT start;       // recovery restart point
    REBCNT i;           // temp index point
    REBCNT begin;       // point at beginning of match
    REBINT count;       // iterated pattern counter
    REBINT mincount;    // min pattern count
    REBINT maxcount;    // max pattern count
    const REBVAL *item_hold;
    REBVAL *val;        // spare
    REBCNT rules_consumed;
    REBFLGS flags;
    REBCNT cmd;

    REBVAL save;
    VAL_INIT_WRITABLE_DEBUG(&save);

    if (C_STACK_OVERFLOWING(&flags)) Trap_Stack_Overflow();

    flags = 0;
    word = 0;
    mincount = maxcount = 1;
    start = begin = P_POS;

    // For each rule in the rule block:
    while (NOT_END(P_RULE)) {

        //Print_Parse_Index(P_TYPE, P_RULE, P_INPUT, P_PAUSE);


        if (--Eval_Count <= 0 || Eval_Signals) {
            //
            // !!! See notes on other invocations about the questions raised by
            // calls to Do_Signals_Throws() by places that do not have a clear
            // path up to return results from an interactive breakpoint.
            //
            REBVAL result;
            VAL_INIT_WRITABLE_DEBUG(&result);

            if (Do_Signals_Throws(&result))
                fail (Error_No_Catch_For_Throw(&result));

            if (IS_ANY_VALUE(&result))
                fail (Error(RE_MISC));
        }

        //--------------------------------------------------------------------
        // Pre-Rule Processing Section
        //
        // For non-iterated rules, including setup for iterated rules.
        // The input index is not advanced here, but may be changed by
        // a GET-WORD variable.
        //--------------------------------------------------------------------

        item = P_RULE;
        FETCH_NEXT_RULE_MAYBE_END(f);

        if (IS_BAR(item))
            return P_POS; // reached it successfully

        // If word, set-word, or get-word, process it:
        if ((VAL_TYPE(item) >= REB_WORD && VAL_TYPE(item) <= REB_GET_WORD)) {
            // Is it a command word?
            if ((cmd = VAL_CMD(item))) {

                if (!IS_WORD(item))
                    fail (Error(RE_PARSE_COMMAND, item)); // no FOO: or :FOO

                if (cmd <= SYM_BREAK) { // optimization

                    switch (cmd) {
                    // Note: mincount = maxcount = 1 on entry
                    case SYM_WHILE:
                        SET_FLAG(flags, PF_WHILE);
                    case SYM_ANY:
                        mincount = 0;
                    case SYM_SOME:
                        maxcount = MAX_I32;
                        continue;

                    case SYM_OPT:
                        mincount = 0;
                        continue;

                    case SYM_COPY:
                        SET_FLAG(flags, PF_COPY);
                        goto set_or_copy_pre_rule;
                    case SYM_SET:
                        SET_FLAG(flags, PF_SET);
                    set_or_copy_pre_rule:
                        item = P_RULE;
                        FETCH_NEXT_RULE_MAYBE_END(f);
                        if (!(IS_WORD(item) || IS_SET_WORD(item)))
                            fail (Error(RE_PARSE_VARIABLE, item));

                        if (VAL_CMD(item))
                            fail (Error(RE_PARSE_COMMAND, item));

                        word = item;
                        continue;

                    case SYM_NOT:
                        SET_FLAG(flags, PF_NOT);
                        flags ^= (1<<PF_NOT2);
                        continue;

                    case SYM_AND:
                        SET_FLAG(flags, PF_AND);
                        continue;

                    case SYM_THEN:
                        SET_FLAG(flags, PF_THEN);
                        continue;

                    case SYM_REMOVE:
                        SET_FLAG(flags, PF_REMOVE);
                        continue;

                    case SYM_INSERT:
                        SET_FLAG(flags, PF_INSERT);
                        goto post_match_processing;

                    case SYM_CHANGE:
                        SET_FLAG(flags, PF_CHANGE);
                        continue;

                    // There are two RETURNs: one is a matching form, so with
                    // 'parse data [return "abc"]' you are not asking to
                    // return the literal string "abc" independent of input.
                    // it will only return if "abc" matches.  This works for
                    // a rule reference as well, such as 'return rule'.
                    //
                    // The second option is if you put the value in parens,
                    // in which case it will just return whatever that value
                    // happens to be, e.g. 'parse data [return ("abc")]'

                    case SYM_RETURN:
                        if (IS_GROUP(P_RULE)) {
                            REBVAL evaluated;
                            VAL_INIT_WRITABLE_DEBUG(&evaluated);

                            if (DO_VAL_ARRAY_AT_THROWS(&evaluated, P_RULE)) {
                                //
                                // If the group evaluation result gives a
                                // THROW, BREAK, CONTINUE, etc then we'll
                                // return that
                                *f->out = evaluated;
                                return THROWN_FLAG;
                            }

                            *f->out = *ROOT_PARSE_NATIVE;
                            CONVERT_NAME_TO_THROWN(
                                f->out, &evaluated, FALSE
                            );

                            // Implicitly returns whatever's in f->out
                            return THROWN_FLAG;
                        }
                        SET_FLAG(flags, PF_RETURN);
                        continue;

                    case SYM_ACCEPT:
                    case SYM_BREAK:
                        P_RESULT = 1;
                        return P_POS;

                    case SYM_REJECT:
                        P_RESULT = -1;
                        return P_POS;

                    case SYM_FAIL:
                        P_POS = NOT_FOUND;
                        goto post_match_processing;

                    case SYM_IF:
                        item = P_RULE;
                        FETCH_NEXT_RULE_MAYBE_END(f);
                        if (IS_END(item)) goto bad_end;

                        if (!IS_GROUP(item))
                            fail (Error(RE_PARSE_RULE, item));

                        // might GC
                        if (DO_VAL_ARRAY_AT_THROWS(&save, item)) {
                            *f->out = save;
                            return THROWN_FLAG;
                        }

                        item = &save;

                        if (IS_CONDITIONAL_TRUE(item))
                            continue;
                        else {
                            P_POS = NOT_FOUND;
                            goto post_match_processing;
                        }

                    case SYM_LIMIT:
                        fail (Error(RE_NOT_DONE));
                        //val = Get_Parse_Value(&save, rule++);
                    //  if (IS_INTEGER(val)) limit = index + Int32(val);
                    //  else if (ANY_SERIES(val)) limit = VAL_INDEX(val);
                    //  else goto
                        //goto bad_rule;
                    //  goto post_match_processing;

                    case SYM__Q_Q:
                        Print_Parse_Index(P_TYPE, P_RULE, P_INPUT, P_POS);
                        continue;
                    }
                }
                // Any other cmd must be a match command, so proceed...

            }
            else {
                // It's not a PARSE command, get or set it

                // word: - set a variable to the series at current index
                if (IS_SET_WORD(item)) {
                    REBVAL temp;
                    VAL_INIT_WRITABLE_DEBUG(&temp);

                    Val_Init_Series_Index(&temp, P_TYPE, P_INPUT, P_POS);

                    *GET_MUTABLE_VAR_MAY_FAIL(item) = temp;

                    continue;
                }

                // :word - change the index for the series to a new position
                if (IS_GET_WORD(item)) {
                    // !!! Should mutability be enforced?
                    item = GET_MUTABLE_VAR_MAY_FAIL(item);
                    if (!ANY_SERIES(item)) // #1263
                        fail (Error(RE_PARSE_SERIES, P_RULE - 1));
                    P_POS = Set_Parse_Series(f, item);
                    continue;
                }

                // word - some other variable
                if (IS_WORD(item)) {
                    // !!! Should mutability be enforced?
                    item = GET_MUTABLE_VAR_MAY_FAIL(item);
                }

                // item can still be 'word or /word
            }
        }
        else if (ANY_PATH(item)) {
            if (IS_PATH(item)) {
                if (Do_Path_Throws(&save, NULL, item, NULL))
                    fail (Error_No_Catch_For_Throw(&save));
                item = &save;
            }
            else if (IS_SET_PATH(item)) {
                REBVAL tmp;
                VAL_INIT_WRITABLE_DEBUG(&tmp);

                Val_Init_Series(&tmp, P_TYPE, P_INPUT);
                VAL_INDEX(&tmp) = P_POS;
                if (Do_Path_Throws(&save, NULL, item, &tmp))
                    fail (Error_No_Catch_For_Throw(&save));
                item = &save;
            }
            else if (IS_GET_PATH(item)) {
                if (Do_Path_Throws(&save, NULL, item, NULL))
                    fail (Error_No_Catch_For_Throw(&save));
                // CureCode #1263 change
                /* if (
                 *    P_TYPE != VAL_TYPE(item)
                 *    || VAL_SERIES(item) != P_INPUT
                 * )
                 */
                if (!ANY_SERIES(&save)) fail (Error(RE_PARSE_SERIES, item));
                P_POS = Set_Parse_Series(f, &save);
                item = NULL;
            }

            if (P_POS > SER_LEN(P_INPUT))
                P_POS = SER_LEN(P_INPUT);

            if (!item) continue; // for SET and GET cases
        }

        if (IS_GROUP(item)) {
            REBVAL evaluated;
            VAL_INIT_WRITABLE_DEBUG(&evaluated);

            // might GC
            if (DO_VAL_ARRAY_AT_THROWS(&evaluated, item)) {
                *f->out = evaluated;
                return THROWN_FLAG;
            }
            // ignore evaluated if it's not THROWN?

            if (P_POS > SER_LEN(P_INPUT)) P_POS = SER_LEN(P_INPUT);
            continue;
        }

        // Counter? 123
        if (IS_INTEGER(item)) { // Specify count or range count
            SET_FLAG(flags, PF_WHILE);
            mincount = maxcount = Int32s(item, 0);
            item = Get_Parse_Value(&save, P_RULE);
            FETCH_NEXT_RULE_MAYBE_END(f);
            if (IS_END(item)) fail (Error(RE_PARSE_END, P_RULE - 2));
            if (IS_INTEGER(item)) {
                maxcount = Int32s(item, 0);
                item = Get_Parse_Value(&save, P_RULE);
                FETCH_NEXT_RULE_MAYBE_END(f);
                if (IS_END(item)) fail (Error(RE_PARSE_END, P_RULE - 2));
            }
        }
        // else fall through on other values and words

        //--------------------------------------------------------------------
        // Iterated Rule Matching Section:
        //
        // Repeats the same rule N times or until the rule fails.
        // The index is advanced and stored in a temp variable i until
        // the entire rule has been satisfied.
        //--------------------------------------------------------------------

        item_hold = item;   // a command or literal match value

        if (VAL_TYPE(item) <= REB_UNSET || VAL_TYPE(item) >= REB_FUNCTION)
            goto bad_rule;

        begin = P_POS;       // input at beginning of match section
        rules_consumed = 0;  // do not use `rule++` below!

        //note: rules var already advanced

        for (count = 0; count < maxcount;) {

            item = item_hold;

            if (IS_BAR(item)) {
                goto bad_rule; // !!! Is this possible?
            }
            if (IS_WORD(item)) {

                switch (cmd = VAL_WORD_CANON(item)) {

                case SYM_SKIP:
                    i = (P_POS < SER_LEN(P_INPUT))
                        ? P_POS + 1
                        : NOT_FOUND;
                    break;

                case SYM_END:
                    i = (P_POS < SER_LEN(P_INPUT))
                        ? NOT_FOUND
                        : SER_LEN(P_INPUT);
                    break;

                case SYM_TO:
                case SYM_THRU:
                    if (IS_END(P_RULE)) goto bad_end;
                    item = Get_Parse_Value(&save, P_RULE);
                    rules_consumed = 1;
                    i = Parse_To(f, P_POS, item, LOGICAL(cmd == SYM_THRU));
                    break;

                case SYM_QUOTE:
                    //
                    // !!! Disallow QUOTE on string series, see #2253
                    //
                    if (!Is_Array_Series(P_INPUT)) goto bad_rule;

                    if (IS_END(P_RULE)) goto bad_end;
                    rules_consumed = 1;
                    if (IS_GROUP(P_RULE)) {
                        // might GC
                        if (DO_VAL_ARRAY_AT_THROWS(&save, P_RULE)) {
                            *f->out = save;
                            return THROWN_FLAG;
                        }
                        item = &save;
                    }
                    else item = P_RULE;

                    if (0 == Cmp_Value(
                        ARR_AT(AS_ARRAY(P_INPUT), P_POS),
                        item,
                        P_HAS_CASE
                    )) {
                        i = P_POS + 1;
                    }
                    else {
                        i = NOT_FOUND;
                    }
                    break;

                case SYM_INTO: {
                    struct Reb_Frame sub_parse;

                    if (IS_END(P_RULE)) goto bad_end;

                    rules_consumed = 1;
                    item = Get_Parse_Value(&save, P_RULE); // sub-rules

                    if (!IS_BLOCK(item)) goto bad_rule;

                    val = ARR_AT(AS_ARRAY(P_INPUT), P_POS);

                    if (IS_END(val) || (!ANY_BINSTR(val) && !ANY_ARRAY(val))) {
                        i = NOT_FOUND;
                        break;
                    }

                    sub_parse.data.stackvars = Push_Ended_Trash_Chunk(3, NULL);
                    sub_parse.data.stackvars[0] = *val;
                    SET_INTEGER(&sub_parse.data.stackvars[1], P_FIND_FLAGS);
                    SET_INTEGER(&sub_parse.data.stackvars[2], P_RESULT);
                    sub_parse.arg = sub_parse.data.stackvars;
                    sub_parse.out = f->out;

                    sub_parse.source.array = VAL_ARRAY(item);
                    sub_parse.indexor = VAL_INDEX(item);
                    sub_parse.value = VAL_ARRAY_AT(item);

                    i = Parse_Rules_Loop(&sub_parse, depth + 1);

                    Drop_Chunk(sub_parse.data.stackvars);

                    if (i == THROWN_FLAG) return THROWN_FLAG;

                    if (i != VAL_LEN_HEAD(val)) {
                        i = NOT_FOUND;
                        break;
                    }

                    i = P_POS + 1;
                    break;
                }

                case SYM_DO:
                    if (!Is_Array_Series(P_INPUT)) goto bad_rule;

                    {
                    REBCNT pos_before = P_POS;

                    i = Do_Eval_Rule(f); // known to change P_RULE (should)

                    P_POS = pos_before; // !!! Simulate restore (needed?)
                    }

                    if (i == THROWN_FLAG) return THROWN_FLAG;

                    rules_consumed = 1;
                    break;

                default:
                    goto bad_rule;
                }
            }
            else if (IS_BLOCK(item)) {
                const REBVAL *rule_before = P_RULE;
                REBCNT pos_before = P_POS;
                union Reb_Frame_Source source_before = f->source;
                REBIXO indexor_before = f->indexor;

                f->indexor = 0;
                f->source.array = VAL_ARRAY(item);
                item = VAL_ARRAY_AT(item);
                P_RULE_LVALUE = item;

                i = Parse_Rules_Loop(f, depth + 1);

                f->source = source_before;
                f->indexor = indexor_before;
                P_POS = pos_before; // !!! Simulate resotration (needed?)
                P_RULE_LVALUE = rule_before; // !!! Simulate restoration (needed?)

                if (i == THROWN_FLAG) return THROWN_FLAG;

                if (P_RESULT) {
                    P_POS = (P_RESULT > 0) ? i : NOT_FOUND;
                    P_RESULT = 0;
                    break;
                }
            }
            else {
                // Parse according to datatype
                REBCNT pos_before = P_POS;
                const REBVAL *rule_before = P_RULE;

                if (Is_Array_Series(P_INPUT))
                    i = Parse_Next_Array(f, P_POS, item, depth + 1);
                else
                    i = Parse_Next_String(f, P_POS, item, depth + 1);

                P_POS = pos_before; // !!! Simulate restoration (needed?)
                P_RULE_LVALUE = rule_before; // !!! Simulate restoration (?)

                // i may be THROWN_FLAG
            }

            if (i == THROWN_FLAG) return THROWN_FLAG;

            // Necessary for special cases like: some [to end]
            // i: indicates new index or failure of the match, but
            // that does not mean failure of the rule, because optional
            // matches can still succeed, if if the last match failed.
            //
            if (i != NOT_FOUND) {
                count++; // may overflow to negative

                if (count < 0)
                    count = MAX_I32; // the forever case

                if (i == P_POS && !GET_FLAG(flags, PF_WHILE)) {
                    //
                    // input did not advance

                    if (count < mincount) {
                        P_POS = NOT_FOUND; // was not enough
                    }
                    break;
                }
            }
            else {
                if (count < mincount) {
                    P_POS = NOT_FOUND; // was not enough
                }
                else if (i != NOT_FOUND) {
                    P_POS = i;
                }
                else {
                    // just keep index as is.
                }
                break;
            }
            P_POS = i;

            // A BREAK word stopped us:
            //if (P_RESULT) {P_RESULT = 0; break;}
        }

        // !!! Recursions or otherwise should be able to advance the rule now
        // that it lives in the parse state
        //
        P_RULE_LVALUE += rules_consumed;

        if (P_POS > SER_LEN(P_INPUT)) P_POS = NOT_FOUND;

        //--------------------------------------------------------------------
        // Post Match Processing:
        //--------------------------------------------------------------------
    post_match_processing:
        // Process special flags:
        if (flags) {
            // NOT before all others:
            if (GET_FLAG(flags, PF_NOT)) {
                if (GET_FLAG(flags, PF_NOT2) && P_POS != NOT_FOUND)
                    P_POS = NOT_FOUND;
                else P_POS = begin;
            }
            if (P_POS == NOT_FOUND) { // Failure actions:
                // !!! if word isn't NULL should we set its var to NONE! ...?
                if (GET_FLAG(flags, PF_THEN)) {
                    FETCH_TO_BAR_MAYBE_END(f);
                    if (NOT_END(P_RULE))
                        FETCH_NEXT_RULE_MAYBE_END(f);
                }
            }
            else {
                //
                // Success actions.  Set count to how much input was advanced
                //
                count = (begin > P_POS) ? 0 : P_POS - begin;

                if (GET_FLAG(flags, PF_COPY)) {
                    REBVAL temp;
                    VAL_INIT_WRITABLE_DEBUG(&temp);

                    Val_Init_Series(
                        &temp,
                        P_TYPE,
                        Is_Array_Series(P_INPUT)
                            ? ARR_SERIES(Copy_Array_At_Max_Shallow(
                                AS_ARRAY(P_INPUT), begin, count
                            ))
                            : Copy_String_Slimming(P_INPUT, begin, count)
                    );
                    *GET_MUTABLE_VAR_MAY_FAIL(word) = temp;
                }
                else if (GET_FLAG(flags, PF_SET)) {
                    REBVAL *var = GET_MUTABLE_VAR_MAY_FAIL(word);

                    if (Is_Array_Series(P_INPUT)) {
                        if (count == 0) SET_NONE(var);
                        else *var = *ARR_AT(AS_ARRAY(P_INPUT), begin);
                    }
                    else {
                        if (count == 0) SET_NONE(var);
                        else {
                            i = GET_ANY_CHAR(P_INPUT, begin);
                            if (P_TYPE == REB_BINARY) {
                                SET_INTEGER(var, i);
                            } else {
                                SET_CHAR(var, i);
                            }
                        }
                    }

                    // !!! Used to reuse item, so item was set to the var at
                    // the end, but was that actually needed?
                    item = var;
                }

                if (GET_FLAG(flags, PF_RETURN)) {
                    // See notes on PARSE's return in handling of SYM_RETURN

                    REBVAL captured;
                    VAL_INIT_WRITABLE_DEBUG(&captured);

                    Val_Init_Series(
                        &captured,
                        P_TYPE,
                        Is_Array_Series(P_INPUT)
                            ? ARR_SERIES(Copy_Array_At_Max_Shallow(
                                AS_ARRAY(P_INPUT), begin, count
                            ))
                            : Copy_String_Slimming(P_INPUT, begin, count)
                    );

                    *f->out = *ROOT_PARSE_NATIVE;
                    CONVERT_NAME_TO_THROWN(f->out, &captured, FALSE);

                    // Implicitly returns whatever's in f->out
                    return THROWN_FLAG;
                }

                if (GET_FLAG(flags, PF_REMOVE)) {
                    if (count) Remove_Series(P_INPUT, begin, count);
                    P_POS = begin;
                }

                if (flags & ((1 << PF_INSERT) | (1 << PF_CHANGE))) {
                    count = GET_FLAG(flags, PF_INSERT) ? 0 : count;
                    cmd = GET_FLAG(flags, PF_INSERT) ? 0 : (1<<AN_PART);
                    item = P_RULE;
                    FETCH_NEXT_RULE_MAYBE_END(f);
                    if (IS_END(item)) goto bad_end;
                    // Check for ONLY flag:
                    if (IS_WORD(item) && (cmd = VAL_CMD(item))) {
                        if (cmd != SYM_ONLY) goto bad_rule;
                        cmd |= (1<<AN_ONLY);
                        item = P_RULE;
                        FETCH_NEXT_RULE_MAYBE_END(f);
                    }
                    // CHECK FOR QUOTE!!
                    item = Get_Parse_Value(&save, item); // new value

                    if (IS_VOID(item)) fail (Error(RE_NO_VALUE, P_RULE - 1));

                    if (IS_END(item)) goto bad_end;

                    if (Is_Array_Series(P_INPUT)) {
                        P_POS = Modify_Array(
                            GET_FLAG(flags, PF_CHANGE) ? A_CHANGE : A_INSERT,
                            AS_ARRAY(P_INPUT),
                            begin,
                            item,
                            cmd,
                            count,
                            1
                        );

                        if (IS_LIT_WORD(item))
                            //
                            // Only set the type, not the whole header (in
                            // order to keep binding information)
                            //
                            VAL_SET_TYPE_BITS(
                                ARR_AT(AS_ARRAY(P_INPUT), P_POS - 1),
                                REB_WORD
                            );
                    }
                    else {
                        if (P_TYPE == REB_BINARY)
                            cmd |= (1 << AN_SERIES); // special flag

                        P_POS = Modify_String(
                            GET_FLAG(flags, PF_CHANGE) ? A_CHANGE : A_INSERT,
                            P_INPUT,
                            begin,
                            item,
                            cmd,
                            count,
                            1
                        );
                    }
                }

                if (GET_FLAG(flags, PF_AND)) P_POS = begin;
            }

            flags = 0;
            word = 0;
        }

        // Goto alternate rule and reset input:
        if (P_POS == NOT_FOUND) {
            FETCH_TO_BAR_MAYBE_END(f);
            if (IS_END(P_RULE)) break;
            FETCH_NEXT_RULE_MAYBE_END(f);
            P_POS = begin = start;
        }

        begin = P_POS;
        mincount = maxcount = 1;

    }
    return P_POS;

bad_rule:
    fail (Error(RE_PARSE_RULE, P_RULE - 1));
bad_end:
    fail (Error(RE_PARSE_END, P_RULE - 1));
    return 0;
}


//
// Shared implementation routine for PARSE? and PARSE.  The difference is that
// PARSE? only returns whether or not a set of rules completed to the end.
// PARSE is more general purpose in terms of the result it provides, and
// it defaults to returning the input.
//
static REB_R Parse_Core(struct Reb_Frame *frame_, REBOOL logic)
{
    PARAM(1, input);
    PARAM(2, rules);
    REFINE(3, case);
    REFINE(4, all);

    REBVAL *rules = ARG(rules);
    REBCNT index;

    struct Reb_Frame parse;

    if (IS_NONE(ARG(rules)) || IS_STRING(ARG(rules))) {
        //
        // !!! R3-Alpha supported "simple parse", which was cued by the rules
        // being either NONE! or a STRING!.  Though this functionality does
        // not exist in Ren-C, it's more informative to give an error telling
        // where to look for the functionality than a generic "parse doesn't
        // take that type" error.
        //
        fail (Error(RE_USE_SPLIT_SIMPLE));
    }


    // The native dispatcher should have pre-filled the output slot with a
    // trash value in the debug build.  We double-check the expectation of
    // whether the parse loop overwites this slot with a result or not.
    //
    assert(IS_TRASH_DEBUG(D_OUT));

    assert(IS_BLOCK(rules));
    parse.value = VAL_ARRAY_AT(rules);

    parse.source.array = VAL_ARRAY(rules);
    parse.indexor = VAL_INDEX(rules);
    // Note: `parse.value` set above

    parse.data.stackvars = Push_Ended_Trash_Chunk(3, NULL);

    parse.data.stackvars[0] = *ARG(input);

    // We always want "case-sensitivity" on binary bytes, vs. treating as
    // case-insensitive bytes for ASCII characters.
    //
    SET_INTEGER(
        &parse.data.stackvars[1],
        REF(case) || IS_BINARY(ARG(input)) ? AM_FIND_CASE : 0
    );

    // !!! Is there some type more meaningful to use for result?  NONE vs.
    // TRUE and FALSE perhaps?  It seems to be 0, -1, and 1...
    //
    SET_INTEGER(&parse.data.stackvars[2], 0);

    parse.arg = parse.data.stackvars;

    parse.out = D_OUT;


    index = Parse_Rules_Loop(&parse, 0);

    Drop_Chunk(parse.data.stackvars);

    if (index == THROWN_FLAG) {
        assert(!IS_TRASH_DEBUG(D_OUT));
        assert(THROWN(D_OUT));
        if (
            IS_FUNCTION_AND(D_OUT, FUNC_CLASS_NATIVE)
            && VAL_FUNC_CODE(ROOT_PARSE_NATIVE) == VAL_FUNC_CODE(D_OUT)
        ) {
            // Note the difference:
            //
            //     parse "1020" [(return true) not-seen]
            //     parse "0304" [return [some ["0" skip]]] not-seen]
            //
            // In the first, a parenthesized evaluation ran a `return`, which
            // is aiming to return from a function using a THROWN().  In
            // the second case parse interrupted *itself* with a THROWN_FLAG
            // to evaluate the expression to the result "0304" from the
            // matched pattern.
            //
            // When parse interrupts itself by throwing, it indicates so
            // by using the throw name of its own REB_NATIVE-valued function.
            // This handles that branch and catches the result value.
            //
            CATCH_THROWN(D_OUT, D_OUT);

            // In the logic case, we are only concerned with matching.  If
            // a construct that can return arbitrary values is used, then
            // failure is triggered with a specific error, saying PARSE must
            // be used instead of PARSE?.
            //
            // !!! Review if this is the best semantics for a parsing variant
            // that is committed to only returning logic TRUE or FALSE, in
            // spite of existence of rules that allow the general PARSE to
            // do otherwise.
            //
            if (logic && !IS_LOGIC(D_OUT))
                fail (Error(RE_PARSE_NON_LOGIC, D_OUT));

            return R_OUT;
        }

        // All other throws should just bubble up uncaught.
        //
        return R_OUT_IS_THROWN;
    }

    // If the loop returned to us, it shouldn't have put anything in out.
    //
    assert(IS_TRASH_DEBUG(D_OUT));

    // Parse can fail if the match rule state can't process pending input.
    //
    if (index == NOT_FOUND)
        return logic ? R_FALSE : R_NONE;

    // If the match rules all completed, but the parse position didn't end
    // at (or beyond) the tail of the input series, the parse also failed.
    //
    if (index < VAL_LEN_HEAD(ARG(input)))
        return logic ? R_FALSE : R_NONE;

    // The end was reached...if doing a logic-based PARSE? then return TRUE.
    //
    if (logic) return R_TRUE;

    // Otherwise it's PARSE so return the input (a series, hence conditionally
    // true, yet more informative for chaining.)  See #2165.
    //
    *D_OUT = *ARG(input);
    return R_OUT;
}


//
//  parse?: native [
//
//  ; NOTE: If changing this, also update PARSE
//
//  "Determines if a series matches the given grammar rules or not."
//
//      input [any-series!]
//          "Input series to parse"
//      rules [block! string! none!]
//          "Rules to parse by (STRING! and NONE! are deprecated)"
//      /case
//          "Uses case-sensitive comparison"
//      /all
//          "(ignored refinement left for Rebol2 transitioning)"
//  ]
//
REBNATIVE(parse_q)
{
    return Parse_Core(frame_, TRUE);
}


//
//  parse: native [
//
//  ; NOTE: If changing this, also update PARSE?
//
//  "Parses a series according to grammar rules and returns a result."
//
//      input [any-series!]
//          "Input series to parse (default result for successful match)"
//      rules [block! string! none!]
//          "Rules to parse by (STRING! and NONE! are deprecated)"
//      /case
//          "Uses case-sensitive comparison"
//      /all
//          "(ignored refinement left for Rebol2 transitioning)"
//  ]
//
REBNATIVE(parse)
{
    return Parse_Core(frame_, FALSE);
}
