//
//  File: %f-blocks.c
//  Summary: "primary block series support functions"
//  Section: functional
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
//  Make_Array: C
// 
// Make a series that is the right size to store REBVALs (and
// marked for the garbage collector to look into recursively).
// Terminator included implicitly. Sets TAIL to zero.
//
REBARR *Make_Array(REBCNT capacity)
{
    REBSER *series = Make_Series(capacity + 1, sizeof(REBVAL), MKS_ARRAY);
    REBARR *array = AS_ARRAY(series);
    SET_END(ARR_HEAD(array));

    return array;
}


//
//  Copy_Array_At_Extra_Shallow: C
// 
// Shallow copy an array from the given index thru the tail.
// Additional capacity beyond what is required can be added
// by giving an `extra` count of how many value cells one needs.
//
REBARR *Copy_Array_At_Extra_Shallow(
    REBARR *original,
    REBCNT index,
    REBCTX *specifier,
    REBCNT extra
) {
    REBCNT len = ARR_LEN(original);
    REBARR *copy;

    if (index > len) return Make_Array(extra);

    len -= index;
    copy = Make_Array(len + extra + 1);

    if (specifier == SPECIFIED) {
        //
        // We can just bit-copy a fully specified array.  By its definition
        // it may not contain any RELVALs.  But in the debug build, double
        // check that...
        //
    #if !defined(NDEBUG)
        RELVAL *check = ARR_AT(original, index);
        REBCNT count = 0;
        for (; count < len; ++count) {
            //
            // Temporarily disable check.  The system uses dynamic binding
            // for the moment to resolve relative values in the absence of
            // a specifier, and the general desire is to get the specifiers
            // chained in everywhere they need to be.
            //
            /* assert(IS_SPECIFIC(check)); */
        }
    #endif

        memcpy(ARR_HEAD(copy), ARR_AT(original, index), len * sizeof(REBVAL));
    }
    else {
        // Any RELVALs will have to be handled.  Review if a memcpy with
        // a touch-up phase is faster, or if there is any less naive way.
        //
        RELVAL *src = ARR_AT(original, index);
        REBVAL *dest = KNOWN(ARR_HEAD(copy));
        REBCNT count = 0;
        for (; count < len; ++count, ++dest, ++src)
            COPY_VALUE(dest, src, specifier);
    }

    SET_ARRAY_LEN(copy, len);
    TERM_ARRAY(copy);

    return copy;
}


//
//  Copy_Array_At_Max_Shallow: C
// 
// Shallow copy an array from the given index for given maximum
// length (clipping if it exceeds the array length)
//
REBARR *Copy_Array_At_Max_Shallow(
    REBARR *original,
    REBCNT index,
    REBCTX *specifier,
    REBCNT max
) {
    REBARR *copy;

    if (index > ARR_LEN(original))
        return Make_Array(0);

    if (index + max > ARR_LEN(original))
        max = ARR_LEN(original) - index;

    copy = Make_Array(max + 1);

    memcpy(ARR_HEAD(copy), ARR_AT(original, index), max * sizeof(REBVAL));

    SET_ARRAY_LEN(copy, max);
    TERM_ARRAY(copy);

    return copy;
}


//
//  Copy_Values_Len_Extra_Shallow: C
// 
// Shallow copy the first 'len' values of `head` into a new
// series created to hold exactly that many entries.
//
REBARR *Copy_Values_Len_Extra_Shallow(
    const RELVAL *head,
    REBCTX *specifier,
    REBCNT len,
    REBCNT extra
) {
    REBARR *array;

    array = Make_Array(len + extra + 1);

    if (specifier == SPECIFIED) {
    #if !defined(NDEBUG)
        REBCNT count = 0;
        const RELVAL *check = head;
        for (; count < len; ++count, ++check) {
            /*assert(IS_SPECIFIC(check));*/ // temporarily disable
        }
    #endif
        memcpy(ARR_HEAD(array), head, len * sizeof(REBVAL));
    }
    else {
        REBCNT count = 0;
        const RELVAL *src = head;
        REBVAL *dest = KNOWN(ARR_HEAD(array));
        for (; count < len; ++count, ++src, ++dest)
            COPY_VALUE(dest, src, specifier);
    }

    SET_ARRAY_LEN(array, len);
    TERM_ARRAY(array);

    return array;
}


//
//  Clonify_Values_Len_Managed: C
// 
// Update the first `len` elements of `head[]` to clone the series
// embedded in them *if* they are in the given set of types (and
// if "cloning" makes sense for them, e.g. they are not simple
// scalars).  If the `deep` flag is set, recurse into subseries
// and objects when that type is matched for clonifying.
// 
// Note: The resulting clones will be managed.  The model for
// lists only allows the topmost level to contain unmanaged
// values...and we *assume* the values we are operating on here
// live inside of an array.  (We also assume the source values
// are in an array, and assert that they are managed.)
//
void Clonify_Values_Len_Managed(
    RELVAL *head,
    REBCTX *specifier,
    REBCNT len,
    REBOOL deep,
    REBU64 types
) {
    RELVAL *value = head;
    REBCNT index;

    if (C_STACK_OVERFLOWING(&len)) Trap_Stack_Overflow();

    for (index = 0; index < len; index++, value++) {
        //
        // By the rules, if we need to do a deep copy on the source
        // series then the values inside it must have already been
        // marked managed (because they *might* delve another level deep)
        //
        ASSERT_VALUE_MANAGED(value);

        if (types & FLAGIT_KIND(VAL_TYPE(value)) & TS_SERIES_OBJ) {
        #if !defined(NDEBUG)
            REBOOL legacy = FALSE;
        #endif

            // Objects and series get shallow copied at minimum
            //
            REBSER *series;
            if (ANY_CONTEXT(value)) {
            #if !defined(NDEBUG)
                legacy = GET_ARR_FLAG(
                    CTX_VARLIST(VAL_CONTEXT(value)),
                    SERIES_FLAG_LEGACY
                );
            #endif

                assert(!IS_FRAME(value)); // !!! Don't exist yet...
                INIT_VAL_CONTEXT(
                    value,
                    Copy_Context_Shallow(VAL_CONTEXT(value))
                );
                series = ARR_SERIES(CTX_VARLIST(VAL_CONTEXT(value)));
            }
            else {
                if (Is_Array_Series(VAL_SERIES(value))) {
                #if !defined(NDEBUG)
                    legacy = GET_ARR_FLAG(VAL_ARRAY(value), SERIES_FLAG_LEGACY);
                #endif

                    if (IS_RELATIVE(value)) {
                        assert(
                            VAL_RELATIVE(value)
                            == VAL_FUNC(CTX_FRAME_FUNC_VALUE(specifier))
                        );
                        series = ARR_SERIES(
                            Copy_Array_Shallow(VAL_ARRAY(value), specifier)
                        );
                        CLEAR_VAL_FLAG(value, VALUE_FLAG_RELATIVE);
                        INIT_ARRAY_SPECIFIC(value, specifier);
                    }
                    else {
                        series = ARR_SERIES(
                            Copy_Array_Shallow(
                                VAL_ARRAY(value),
                                VAL_SPECIFIER(KNOWN(value)) // not relative...
                            )
                        );
                    }

                    INIT_VAL_ARRAY(value, AS_ARRAY(series)); // copies args

                    // If it was relative, then copying with a specifier
                    // means it isn't relative any more.
                    //
                    INIT_ARRAY_SPECIFIC(value, SPECIFIED);
                }
                else {
                    series = Copy_Sequence(VAL_SERIES(value));
                    INIT_VAL_SERIES(value, series);
                }
            }

        #if !defined(NDEBUG)
            if (legacy) // propagate legacy
                SET_SER_FLAG(series, SERIES_FLAG_LEGACY);
        #endif

            MANAGE_SERIES(series);

            if (!deep) continue;

            // If we're going to copy deeply, we go back over the shallow
            // copied series and "clonify" the values in it.
            //
            // Since we had to get rid of the relative bindings in the
            // shallow copy, we can pass in SPECIFIED here...but the recursion
            // in Clonify_Values will be threading through any updated specificity
            // through to the new values.
            //
            if (types & FLAGIT_KIND(VAL_TYPE(value)) & TS_ARRAYS_OBJ) {
                REBOOL array = ANY_ARRAY(value);
                REBOOL context = ANY_CONTEXT(value);
                Clonify_Values_Len_Managed(
                     ARR_HEAD(AS_ARRAY(series)),
                     SPECIFIED,
                     VAL_LEN_HEAD(value),
                     deep,
                     types
                );
            }
        }
        else if (
            types & FLAGIT_KIND(VAL_TYPE(value)) & FLAGIT_KIND(REB_FUNCTION)
        ) {
            Clonify_Function(value);
        }
        else {
            // The value is not on our radar as needing to be processed,
            // so leave it as-is.
        }
    }
}


//
//  Copy_Array_Core_Managed: C
// 
// Copy a block, copy specified values, deeply if indicated.
// 
// The resulting series will already be under GC management,
// and hence cannot be freed with Free_Series().
//
REBARR *Copy_Array_Core_Managed(
    REBARR *original,
    REBCNT index,
    REBCTX *specifier,
    REBCNT tail,
    REBCNT extra,
    REBOOL deep,
    REBU64 types
) {
    REBARR *copy;

    if (index > tail) index = tail;

    if (index > ARR_LEN(original)) {
        copy = Make_Array(extra);
        MANAGE_ARRAY(copy);
    }
    else {
        copy = Copy_Values_Len_Extra_Shallow(
            ARR_AT(original, index), specifier, tail - index, extra
        );
        MANAGE_ARRAY(copy);

        if (types != 0) // the copy above should have specified top level
            Clonify_Values_Len_Managed(
                ARR_HEAD(copy), SPECIFIED, ARR_LEN(copy), deep, types
            );
    }

#if !defined(NDEBUG)
    //
    // Propagate legacy flag, hence if a legacy array was loaded with
    // `[switch 1 [2]]` in it (for instance) then when that code is used to
    // make a function body, the `[switch 1 [2]]` in that body will also
    // be marked legacy.  Then if it runs, the SWITCH can dispatch to return
    // blank instead of the Ren-C behavior of returning `2`.
    //
    if (GET_ARR_FLAG(original, SERIES_FLAG_LEGACY))
        SET_ARR_FLAG(copy, SERIES_FLAG_LEGACY);
#endif

    return copy;
}


//
//  Copy_Array_At_Extra_Deep_Managed: C
// 
// Deep copy an array, including all series (strings, blocks,
// parens, objects...) excluding images, bitsets, maps, etc.
// The set of exclusions is the typeset TS_NOT_COPIED.
// 
// The resulting array will already be under GC management,
// and hence cannot be freed with Free_Series().
// 
// Note: If this were declared as a macro it would use the
// `array` parameter more than once, and have to be in all-caps
// to warn against usage with arguments that have side-effects.
//
REBARR *Copy_Array_At_Extra_Deep_Managed(
    REBARR *original,
    REBCNT index,
    REBCTX *specifier,
    REBCNT extra
) {
    return Copy_Array_Core_Managed(
        original,
        index, // at
        specifier,
        ARR_LEN(original), // tail
        extra, // extra
        TRUE, // deep
        TS_SERIES & ~TS_NOT_COPIED // types
    );
}


//
//  Alloc_Tail_Array: C
// 
// Append a REBVAL-size slot to Rebol Array series at its tail.
// Will use existing memory capacity already in the series if it
// is available, but will expand the series if necessary.
// Returns the new value for you to initialize.
// 
// Note: Updates the termination and tail.
//
REBVAL *Alloc_Tail_Array(REBARR *array)
{
    REBVAL *tail;

    EXPAND_SERIES_TAIL(ARR_SERIES(array), 1);
    tail = ARR_TAIL(array);
    SET_END(tail);

    SET_TRASH_IF_DEBUG(tail - 1); // No-op in release builds
    return tail - 1;
}


//
//  Find_Same_Array: C
// 
// Scan a block for any values that reference blocks related
// to the value provided.
// 
// !!! This was used for detection of cycles during MOLD.  The idea is that
// while it is outputting a series, it doesn't want to see that series
// again.  For the moment the only places to worry about with that are
// context varlists and block series or maps.  (Though a function contains
// series for the spec, body, and paramlist...the spec and body are blocks,
// and so recursion would be found when the blocks were output.)
//
REBCNT Find_Same_Array(REBARR *search_values, const REBVAL *value)
{
    REBCNT index = 0;
    REBARR *array;
    REBVAL *other;

    if (ANY_ARRAY(value))
        array = VAL_ARRAY(value);
    else if (IS_MAP(value))
        array = MAP_PAIRLIST(VAL_MAP(value));
    else if (ANY_CONTEXT(value))
        array = CTX_VARLIST(VAL_CONTEXT(value));
    else {
        // Value being worked with is not a candidate for containing an
        // array that could form a loop with one of the search_list values
        //
        return NOT_FOUND;
    }

    other = ARR_HEAD(search_values);
    for (; NOT_END(other); other++, index++) {
        if (ANY_ARRAY(other)) {
            if (array == VAL_ARRAY(other))
                return index;
        }
        else if (IS_MAP(other)) {
            if (array == MAP_PAIRLIST(VAL_MAP(other)))
                return index;
        }
        else if (ANY_CONTEXT(other)) {
            if (array == CTX_VARLIST(VAL_CONTEXT(other)))
                return index;
        }
    }

    return NOT_FOUND;
}


//
//  Unmark: C
// 
// Clear the recusion markers for series and object trees.
// 
// Note: these markers are also used for GC. Functions that
// call this must not be able to trigger GC!
//
void Unmark(RELVAL *val)
{
    REBARR *array;

    if (ANY_ARRAY(val))
        array = VAL_ARRAY(val);
    else if (ANY_CONTEXT(val))
        array = CTX_VARLIST(VAL_CONTEXT(val));
    else {
        // Shouldn't have marked recursively any non-array series (no need)
        //
        assert(
            !ANY_SERIES(val)
            || !GET_SER_FLAG(VAL_SERIES(val), SERIES_FLAG_MARK)
        );
        return;
    }

    if (!GET_ARR_FLAG(array, SERIES_FLAG_MARK)) return; // avoid loop

    CLEAR_ARR_FLAG(array, SERIES_FLAG_MARK);

    for (val = ARR_HEAD(array); NOT_END(val); val++)
        Unmark(val);
}
