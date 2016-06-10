//
//  File: %n-reduce.h
//  Summary: {REDUCE and COMPOSE natives and associated service routines}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
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
// !!! The R3-Alpha REDUCE routine contained several auxiliariy refinements
// used by fringe dialects.  These need review for whether they are still in
// working order--or if they need to just be replaced or removed.
//

#include "sys-core.h"

//
//  Reduce_Array_Throws: C
//
// Reduce array from the index position specified in the value.
// Collect all values from stack and make them into a BLOCK! REBVAL.
//
// !!! Review generalization of this to produce an array and not a REBVAL
// of a particular kind.
//
REBOOL Reduce_Array_Throws(
    REBVAL *out,
    REBARR *array,
    REBCNT index,
    REBCTX *specifier,
    REBOOL into
) {
    REBDSP dsp_orig = DSP;
    REBIXO indexor = index;

    // Through the DO_NEXT_MAY_THROW interface, we can't tell the difference
    // between DOing an array that evaluates to void and an empty
    // array, because both give back an unset value and an end position.  But
    // we want:
    //
    //     reduce [] => []
    //     reduce [()] => error
    //
    // So must do a special check to handle the former.  This could be changed
    // to use the lower level DO API, however.
    //
    if (IS_END(ARR_AT(array, index))) {
        if (into)
            return FALSE;

        Val_Init_Block(out, Make_Array(0));
        return FALSE;
    }

    while (indexor != END_FLAG) {
        REBVAL reduced;
        DO_NEXT_MAY_THROW(indexor, &reduced, array, indexor, specifier);

        if (indexor == THROWN_FLAG) {
            *out = reduced;
            DS_DROP_TO(dsp_orig);
            return TRUE;
        }

        if (IS_VOID(&reduced)) {
            //
            // !!! Review if there should be a form of reduce which allows
            // void expressions.  The general feeling is that it shouldn't
            // be allowed by default, since N expressions would not make N
            // results...and reduce is often used for positional purposes.
            // Substituting anything (like a NONE!, or anything else) would
            // perhaps be disingenuous.
            //
            fail (Error(RE_REDUCE_MADE_VOID));
        }

        DS_PUSH(&reduced);
    }

    if (into)
        Pop_Stack_Values_Into(out, dsp_orig);
    else
        Val_Init_Block(out, Pop_Stack_Values(dsp_orig));

    return FALSE;
}


//
//  Reduce_Only: C
//
// Reduce only words and paths not found in word list.
//
void Reduce_Only(
    REBVAL *out,
    REBARR *block,
    REBCNT index,
    REBCTX *specifier,
    REBVAL *words,
    REBOOL into
) {
    REBDSP dsp_orig = DSP;
    RELVAL *item;
    REBARR *arr = 0;
    REBCNT idx = 0;

    if (IS_BLOCK(words)) {
        arr = VAL_ARRAY(words);
        idx = VAL_INDEX(words);
    }

    for (item = ARR_AT(block, index); NOT_END(item); item++) {
        if (IS_WORD(item)) {
            // Check for keyword:
            if (
                arr &&
                NOT_FOUND != Find_Word_In_Array(arr, idx, VAL_WORD_CANON(item))
            ) {
                DS_PUSH_RELVAL(item, specifier);
                continue;
            }

            DS_PUSH(GET_OPT_VAR_MAY_FAIL(item, specifier));
        }
        else if (IS_PATH(item)) {
            REBVAL temp;

            if (arr) {
                // Check for keyword/path:
                const RELVAL *v = VAL_ARRAY_AT(item);
                if (IS_WORD(v)) {
                    if (
                        NOT_FOUND
                        != Find_Word_In_Array(arr, idx, VAL_WORD_CANON(v))
                    ) {
                        DS_PUSH_RELVAL(item, specifier);
                        continue;
                    }
                }
            }

            if (Do_Path_Throws_Core(&temp, NULL, item, specifier, NULL))
                fail (Error_No_Catch_For_Throw(&temp));

            DS_PUSH(&temp);
        }
        else
            DS_PUSH_RELVAL(item, specifier);
    }

    if (into)
        Pop_Stack_Values_Into(out, dsp_orig);
    else
        Val_Init_Block(out, Pop_Stack_Values(dsp_orig));

    assert(DSP == dsp_orig);
}


//
//  Reduce_Array_No_Set_Throws: C
//
REBOOL Reduce_Array_No_Set_Throws(
    REBVAL *out,
    REBARR *block,
    REBCNT index,
    REBCTX *specifier,
    REBOOL into
) {
    REBDSP dsp_orig = DSP;
    REBIXO indexor = index;

    while (index < ARR_LEN(block)) {
        RELVAL *value = ARR_AT(block, index);
        if (IS_SET_WORD(value)) {
            DS_PUSH_RELVAL(value, specifier);
            index++;
        }
        else {
            REBVAL reduced;
            DO_NEXT_MAY_THROW(indexor, &reduced, block, indexor, specifier);
            if (indexor == THROWN_FLAG) {
                *out = reduced;
                DS_DROP_TO(dsp_orig);
                return TRUE;
            }
            DS_PUSH(&reduced);
        }
    }

    if (into)
        Pop_Stack_Values_Into(out, dsp_orig);
    else
        Val_Init_Block(out, Pop_Stack_Values(dsp_orig));

    return FALSE;
}


//
//  reduce: native [
//
//  {Evaluates expressions and returns multiple results.}
//
//      value
//      /no-set
//          "Keep set-words as-is. Do not set them."
//      /only
//          "Only evaluate words and paths, not functions"
//      words [block! blank!]
//          "Optional words that are not evaluated (keywords)"
//      /into
//          {Output results into a series with no intermediate storage}
//      target [any-array!]
//  ]
//
REBNATIVE(reduce)
{
    PARAM(1, value);
    REFINE(2, no_set);
    REFINE(3, only);
    PARAM(4, words);
    REFINE(5, into);
    PARAM(6, target);

    REBVAL *value = ARG(value);

    if (IS_BLOCK(value)) {
        if (REF(into))
            *D_OUT = *ARG(target);

        if (REF(no_set)) {
            if (Reduce_Array_No_Set_Throws(
                D_OUT,
                VAL_ARRAY(value),
                VAL_INDEX(value),
                VAL_SPECIFIER(value),
                REF(into)
            )) {
                return R_OUT_IS_THROWN;
            }
        }
        else if (REF(only)) {
            Reduce_Only(
                D_OUT,
                VAL_ARRAY(value),
                VAL_INDEX(value),
                VAL_SPECIFIER(value),
                ARG(words),
                REF(into)
            );
        }
        else {
            if (Reduce_Array_Throws(
                D_OUT,
                VAL_ARRAY(value),
                VAL_INDEX(value),
                VAL_SPECIFIER(value),
                REF(into)
            )) {
                return R_OUT_IS_THROWN;
            }
        }

        return R_OUT;
    }

    if (REF(only) || REF(no_set) || REF(into)) {
        //
        // !!! These features on single elements have not been defined or
        // implemented, and should be reviewed.
        //
        fail (Error(RE_MISC));
    }

    // A single element should do what is effectively an evaluation but with
    // no arguments.  This is a change in behavior from R3-Alpha, which would
    // just return the input as is, e.g. `reduce quote (1 + 2)` => (1 + 2).
    //
    // !!! Should the error be more "reduce-specific" if args were required?
    //
    if (EVAL_VALUE_THROWS(D_OUT, value))
        return R_OUT_IS_THROWN;

    return R_OUT;
}


//
//  Compose_Values_Throws: C
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
REBOOL Compose_Values_Throws(
    REBVAL *out,
    const RELVAL *head,
    REBCTX *specifier,
    REBOOL deep,
    REBOOL only,
    REBOOL into
) {
    const RELVAL *value = head;
    REBDSP dsp_orig = DSP;

    for (; NOT_END(value); value++) {
        if (IS_GROUP(value)) {
            REBVAL evaluated;
            if (Do_At_Throws(
                &evaluated,
                VAL_ARRAY(value),
                VAL_INDEX(value),
                specifier
            )) {
                *out = evaluated;
                DS_DROP_TO(dsp_orig);
                return TRUE;
            }

            if (IS_BLOCK(&evaluated) && !only) {
                //
                // compose [blocks ([a b c]) merge] => [blocks a b c merge]
                //
                RELVAL *push = VAL_ARRAY_AT(&evaluated);
                while (!IS_END(push)) {
                    DS_PUSH_RELVAL(push, VAL_SPECIFIER(&evaluated));
                    push++;
                }
            }
            else if (!IS_VOID(&evaluated)) {
                //
                // compose [(1 + 2) inserts as-is] => [3 inserts as-is]
                // compose/only [([a b c]) unmerged] => [[a b c] unmerged]
                //
                DS_PUSH(&evaluated);
            }
            else {
                //
                // compose [(print "Unsets *vanish*!")] => []
                //
            }
        }
        else if (deep) {
            if (IS_BLOCK(value)) {
                //
                // compose/deep [does [(1 + 2)] nested] => [does [3] nested]

                REBVAL composed;
                if (Compose_Values_Throws(
                    &composed,
                    VAL_ARRAY_AT(value),
                    specifier,
                    TRUE,
                    only,
                    into
                )) {
                    *out = composed;
                    DS_DROP_TO(dsp_orig);
                    return TRUE;
                }

                DS_PUSH(&composed);
            }
            else {
                DS_PUSH_RELVAL(value, specifier);
                if (ANY_ARRAY(value)) {
                    //
                    // compose [copy/(orig) (copy)] => [copy/(orig) (copy)]
                    // !!! path and second group are copies, first group isn't
                    //
                    INIT_VAL_ARRAY(
                        DS_TOP,
                        Copy_Array_At_Shallow(
                            VAL_ARRAY(value),
                            VAL_INDEX(value),
                            specifier
                        )
                    );
                    MANAGE_ARRAY(VAL_ARRAY(DS_TOP));
                }
            }
        }
        else {
            //
            // compose [[(1 + 2)] (reverse "wollahs")] => [[(1 + 2)] "shallow"]
            //
            DS_PUSH_RELVAL(value, specifier);
        }
    }

    if (into)
        Pop_Stack_Values_Into(out, dsp_orig);
    else
        Val_Init_Block(out, Pop_Stack_Values(dsp_orig));

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
    PARAM(1, value);
    REFINE(2, deep);
    REFINE(3, only);
    REFINE(4, into);
    PARAM(5, out);

    // !!! Should 'compose quote (a (1 + 2) b)' give back '(a 3 b)' ?
    // What about 'compose quote a/(1 + 2)/b' ?
    //
    if (!IS_BLOCK(ARG(value))) {
        *D_OUT = *ARG(value);
        return R_OUT;
    }

    // Compose_Values_Throws() expects `out` to contain the target if it is
    // passed TRUE as the `into` flag.
    //
    if (REF(into)) *D_OUT = *ARG(out);

    if (Compose_Values_Throws(
        D_OUT,
        VAL_ARRAY_HEAD(ARG(value)),
        VAL_SPECIFIER(ARG(value)),
        REF(deep),
        REF(only),
        REF(into)
    )) {
        return R_OUT_IS_THROWN;
    }

    return R_OUT;
}
