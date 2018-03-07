//
//  File: %n-reduce.h
//  Summary: {REDUCE and COMPOSE natives and associated service routines}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
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

#include "sys-core.h"

//
//  Reduce_Any_Array_Throws: C
//
// Reduce array from the index position specified in the value.
//
// If `into` then splice into the existing `out`.  Otherwise, overwrite the
// `out` with all values collected from the stack, into an array matching the
// type of the input.  So [1 + 1 2 + 2] => [3 4], and 1/+/1/2/+/2 => 3/4
//
// !!! This is not necessarily the best answer, it's just the mechanically
// most obvious one.
//
REBOOL Reduce_Any_Array_Throws(
    REBVAL *out,
    REBVAL *any_array,
    REBFLGS flags
) {
    assert(
        NOT(flags & REDUCE_FLAG_KEEP_BARS)
        == LOGICAL(flags & REDUCE_FLAG_DROP_BARS)
    ); // only one should be true, but caller should be explicit of which

    REBDSP dsp_orig = DSP;

    DECLARE_FRAME (f);
    Push_Frame(f, any_array);

    DECLARE_LOCAL (reduced);

    while (FRM_HAS_MORE(f)) {
        if (IS_BAR(f->value)) {
            if (flags & REDUCE_FLAG_KEEP_BARS) {
                DS_PUSH_TRASH;
                Quote_Next_In_Frame(DS_TOP, f);
            }
            else
                Fetch_Next_In_Frame(f);

            continue;
        }

        REBOOL line = GET_VAL_FLAG(f->value, VALUE_FLAG_LINE);

        if (Do_Next_In_Frame_Throws(reduced, f)) {
            Move_Value(out, reduced);
            DS_DROP_TO(dsp_orig);
            Drop_Frame(f);
            return TRUE;
        }

        if (IS_VOID(reduced)) {
            //
            // !!! Review if there should be a form of reduce which allows
            // void expressions.  The general feeling is that it shouldn't
            // be allowed by default, since N expressions would not make N
            // results...and reduce is often used for positional purposes.
            // Substituting anything (like a NONE!, or anything else) would
            // perhaps be disingenuous.
            //
            fail (Error_Reduce_Made_Void_Raw());
        }

        DS_PUSH(reduced);
        if (line)
            SET_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE);
    }

    if (flags & REDUCE_FLAG_INTO)
        Pop_Stack_Values_Into(out, dsp_orig);
    else
        Init_Any_Array(
            out,
            VAL_TYPE(any_array),
            Pop_Stack_Values_Core(
                dsp_orig, NODE_FLAG_MANAGED | SERIES_FLAG_FILE_LINE
            )
        );

    Drop_Frame(f);
    return FALSE;
}


//
//  reduce: native [
//
//  {Evaluates expressions and returns multiple results.}
//
//      return: [<opt> any-value!]
//      value [<opt> any-value!]
//          {If BLOCK!, expressions are reduced, otherwise single value.}
//      /into
//          {Output results into a series with no intermediate storage}
//      target [any-array!]
//  ]
//
REBNATIVE(reduce)
{
    INCLUDE_PARAMS_OF_REDUCE;

    REBVAL *value = ARG(value);

    if (IS_VOID(value))
        return R_VOID; // !!! Should this be allowed?  (Red allows it)

    if (IS_BLOCK(value)) {
        if (REF(into))
            Move_Value(D_OUT, ARG(target));

        if (Reduce_Any_Array_Throws(
            D_OUT,
            value,
            REF(into)
                ? REDUCE_FLAG_INTO | REDUCE_FLAG_KEEP_BARS
                : REDUCE_FLAG_KEEP_BARS
        )){
            return R_OUT_IS_THROWN;
        }

        return R_OUT;
    }

    // A single element should do what is effectively an evaluation but with
    // no arguments.  This is a change in behavior from R3-Alpha, which would
    // just return the input as is, e.g. `reduce quote (1 + 2)` => (1 + 2).
    //
    // !!! Should the error be more "reduce-specific" if args were required?
    //
    if (Eval_Value_Throws(D_OUT, value))
        return R_OUT_IS_THROWN;

    if (NOT(REF(into)))
        return R_OUT; // just return the evaluated item if no /INTO target

    REBVAL *into = ARG(target);
    assert(ANY_ARRAY(into));
    FAIL_IF_READ_ONLY_ARRAY(VAL_ARRAY(into));

    // Insert the single item into the target array at its current position,
    // and return the position after the insertion (the /INTO convention)

    VAL_INDEX(into) = Insert_Series(
        SER(VAL_ARRAY(into)),
        VAL_INDEX(into),
        cast(REBYTE*, D_OUT),
        1 // multiplied by width (sizeof(REBVAL)) in Insert_Series
    );

    Move_Value(D_OUT, into);
    return R_OUT;
}


//
//  Compose_Any_Array_Throws: C
//
// Compose a block from a block of un-evaluated values and GROUP! arrays that
// are evaluated.  This calls into Do_Core, so if 'into' is provided, then its
// series must be protected from garbage collection.
//
//     deep - recurse into sub-blocks
//     only - parens that return blocks are kept as blocks
//
// Writes result value at address pointed to by out.
//
REBOOL Compose_Any_Array_Throws(
    REBVAL *out,
    const REBVAL *any_array,
    REBOOL deep,
    REBOOL only,
    REBOOL into
) {
    REBDSP dsp_orig = DSP;

    DECLARE_FRAME (f);
    Push_Frame(f, any_array);

    DECLARE_LOCAL (composed);
    DECLARE_LOCAL (specific);

    while (FRM_HAS_MORE(f)) {
        REBOOL line = GET_VAL_FLAG(f->value, VALUE_FLAG_LINE);
        if (IS_GROUP(f->value)) {
            //
            // Evaluate the GROUP! at current position into `composed` cell.
            //
            REBSPC *derived = Derive_Specifier(f->specifier, f->value);
            if (Do_At_Throws(
                composed,
                VAL_ARRAY(f->value),
                VAL_INDEX(f->value),
                derived
            )){
                Move_Value(out, composed);
                DS_DROP_TO(dsp_orig);
                Drop_Frame(f);
                return TRUE;
            }

            Fetch_Next_In_Frame(f);

            if (IS_BLOCK(composed) && !only) {
                //
                // compose [blocks ([a b c]) merge] => [blocks a b c merge]
                //
                RELVAL *push = VAL_ARRAY_AT(composed);
                while (NOT_END(push)) {
                    //
                    // `evaluated` is known to be specific, but its specifier
                    // may be needed to derelativize its children.
                    //
                    DS_PUSH_RELVAL(push, VAL_SPECIFIER(composed));
                    if (line) {
                        SET_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE);
                        line = FALSE;
                    }
                    push++;
                }
            }
            else if (!IS_VOID(composed)) {
                //
                // compose [(1 + 2) inserts as-is] => [3 inserts as-is]
                // compose/only [([a b c]) unmerged] => [[a b c] unmerged]
                //
                DS_PUSH(composed);
                if (line)
                    SET_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE);
            }
            else {
                //
                // compose [(print "Voids *vanish*!")] => []
                //
            }
        }
        else if (deep) {
            if (IS_BLOCK(f->value)) {
                //
                // compose/deep [does [(1 + 2)] nested] => [does [3] nested]

                Derelativize(specific, f->value, f->specifier);

                if (Compose_Any_Array_Throws(
                    composed,
                    specific,
                    TRUE,
                    only,
                    into
                )) {
                    Move_Value(out, composed);
                    DS_DROP_TO(dsp_orig);
                    Drop_Frame(f);
                    return TRUE;
                }

                DS_PUSH(composed);
                if (line)
                    SET_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE);
            }
            else {
                if (ANY_ARRAY(f->value)) {
                    //
                    // compose [copy/(orig) (copy)] => [copy/(orig) (copy)]
                    // !!! path and second group are copies, first group isn't
                    //
                    REBSPC *derived = Derive_Specifier(f->specifier, f->value);
                    REBARR *copy = Copy_Array_Shallow(
                        VAL_ARRAY(f->value),
                        derived
                    );
                    DS_PUSH_TRASH;
                    Init_Any_Array_At(
                        DS_TOP, VAL_TYPE(f->value), copy, VAL_INDEX(f->value)
                    ); // ...manages
                }
                else
                    DS_PUSH_RELVAL(f->value, f->specifier);

                if (line)
                    SET_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE);
            }
            Fetch_Next_In_Frame(f);
        }
        else {
            //
            // compose [[(1 + 2)] (reverse "wollahs")] => [[(1 + 2)] "shallow"]
            //
            DS_PUSH_RELVAL(f->value, f->specifier);
            assert(line == GET_VAL_FLAG(DS_TOP, VALUE_FLAG_LINE));
            Fetch_Next_In_Frame(f);
        }
    }

    if (into)
        Pop_Stack_Values_Into(out, dsp_orig);
    else
        Init_Any_Array(
            out,
            VAL_TYPE(any_array),
            Pop_Stack_Values_Core(
                dsp_orig, NODE_FLAG_MANAGED | SERIES_FLAG_FILE_LINE
            )
        );

    Drop_Frame(f);
    return FALSE;
}


//
//  compose: native [
//
//  {Evaluates only the GROUP!s in a block of expressions, returning a block.}
//
//      value
//          "Block to compose (or any other type evaluates to itself)"
//                                          ; ^-- is this sensible?
//      /deep
//          "Compose nested blocks"
//      /only
//          {Insert a block as a single value (not the contents of the block)}
//      /into
//          {Output results into a series with no intermediate storage}
//      out [any-array! any-string! binary!]
//  ]
//
REBNATIVE(compose)
{
    INCLUDE_PARAMS_OF_COMPOSE;

    // !!! Should 'compose quote (a (1 + 2) b)' give back '(a 3 b)' ?
    // What about 'compose quote a/(1 + 2)/b' ?
    //
    if (!IS_BLOCK(ARG(value))) {
        Move_Value(D_OUT, ARG(value));
        return R_OUT;
    }

    // Compose_Values_Throws() expects `out` to contain the target if it is
    // passed TRUE as the `into` flag.
    //
    if (REF(into))
        Move_Value(D_OUT, ARG(out));
    else
        assert(IS_END(D_OUT)); // !!! guaranteed, better signal than `into`?

    if (Compose_Any_Array_Throws(
        D_OUT,
        ARG(value),
        REF(deep),
        REF(only),
        REF(into)
    )) {
        return R_OUT_IS_THROWN;
    }

    return R_OUT;
}


enum FLATTEN_LEVEL {
    FLATTEN_NOT,
    FLATTEN_ONCE,
    FLATTEN_DEEP
};


static void Flatten_Core(
    RELVAL head[],
    REBSPC *specifier,
    enum FLATTEN_LEVEL level
) {
    RELVAL *item = head;
    for (; NOT_END(item); ++item) {
        if (IS_BLOCK(item) && level != FLATTEN_NOT) {
            REBSPC *derived = Derive_Specifier(specifier, item);
            Flatten_Core(
                VAL_ARRAY_AT(item),
                derived,
                level == FLATTEN_ONCE ? FLATTEN_NOT : FLATTEN_DEEP
            );
        }
        else
            DS_PUSH_RELVAL(item, specifier);
    }
}


//
//  flatten: native [
//
//  {Flattens a block of blocks.}
//
//      return: [block!]
//          {The flattened result block}
//      block [block!]
//          {The nested source block}
//      /deep
//  ]
//
REBNATIVE(flatten)
{
    INCLUDE_PARAMS_OF_FLATTEN;

    REBDSP dsp_orig = DSP;

    Flatten_Core(
        VAL_ARRAY_AT(ARG(block)),
        VAL_SPECIFIER(ARG(block)),
        REF(deep) ? FLATTEN_DEEP : FLATTEN_ONCE
    );

    Init_Block(D_OUT, Pop_Stack_Values(dsp_orig));
    return R_OUT;
}
