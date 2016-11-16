//
//  File: %n-protect.c
//  Summary: "native functions for series and object field protection"
//  Section: natives
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
//  Protect_Key: C
//
static void Protect_Key(RELVAL *key, REBFLGS flags)
{
    if (GET_FLAG(flags, PROT_WORD)) {
        if (GET_FLAG(flags, PROT_SET)) SET_VAL_FLAG(key, TYPESET_FLAG_LOCKED);
        else CLEAR_VAL_FLAG(key, TYPESET_FLAG_LOCKED);
    }

    if (GET_FLAG(flags, PROT_HIDE)) {
        if (GET_FLAG(flags, PROT_SET))
            SET_VAL_FLAGS(key, TYPESET_FLAG_HIDDEN | TYPESET_FLAG_UNBINDABLE);
        else
            CLEAR_VAL_FLAGS(
                key, TYPESET_FLAG_HIDDEN | TYPESET_FLAG_UNBINDABLE
            );
    }
}


//
//  Protect_Value: C
// 
// Anything that calls this must call Unmark() when done.
//
void Protect_Value(RELVAL *value, REBFLGS flags)
{
    if (ANY_SERIES(value) || IS_MAP(value))
        Protect_Series(VAL_SERIES(value), VAL_INDEX(value), flags);
    else if (IS_OBJECT(value) || IS_MODULE(value))
        Protect_Object(value, flags);
}


//
//  Protect_Series: C
// 
// Anything that calls this must call Unmark() when done.
//
void Protect_Series(REBSER *series, REBCNT index, REBFLGS flags)
{
    if (IS_REBSER_MARKED(series)) return; // avoid loop

    if (GET_FLAG(flags, PROT_SET))
        SET_SER_FLAG(series, SERIES_FLAG_LOCKED);
    else
        CLEAR_SER_FLAG(series, SERIES_FLAG_LOCKED);

    if (!Is_Array_Series(series) || !GET_FLAG(flags, PROT_DEEP)) return;

    MARK_REBSER(series); // recursion protection

    RELVAL *val = ARR_AT(AS_ARRAY(series), index);
    for (; NOT_END(val); val++) {
        Protect_Value(val, flags);
    }
}


//
//  Protect_Object: C
// 
// Anything that calls this must call Unmark() when done.
//
void Protect_Object(RELVAL *value, REBFLGS flags)
{
    REBCTX *context = VAL_CONTEXT(value);

    if (IS_REBSER_MARKED(ARR_SERIES(CTX_VARLIST(context))))
        return; // avoid loop

    if (GET_FLAG(flags, PROT_SET))
        SET_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_LOCKED);
    else
        CLEAR_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_LOCKED);

    for (value = CTX_KEY(context, 1); NOT_END(value); value++) {
        Protect_Key(KNOWN(value), flags);
    }

    if (!GET_FLAG(flags, PROT_DEEP)) return;

    MARK_REBSER(ARR_SERIES(CTX_VARLIST(context))); // recursion protection

    value = CTX_VARS_HEAD(context);
    for (; NOT_END(value); value++) {
        Protect_Value(value, flags);
    }
}


//
//  Protect_Word_Value: C
//
static void Protect_Word_Value(REBVAL *word, REBFLGS flags)
{
    REBVAL *key;
    REBVAL *val;

    if (ANY_WORD(word) && IS_WORD_BOUND(word)) {
        key = CTX_KEY(VAL_WORD_CONTEXT(word), VAL_WORD_INDEX(word));
        Protect_Key(key, flags);
        if (GET_FLAG(flags, PROT_DEEP)) {
            //
            // Ignore existing mutability state so that it may be modified.
            // Most routines should NOT do this!
            //
            enum Reb_Kind eval_type; // unused
            val = Get_Var_Core(
                &eval_type,
                word,
                SPECIFIED,
                GETVAR_READ_ONLY
            );
            Protect_Value(val, flags);
            Unmark(val);
        }
    }
    else if (ANY_PATH(word)) {
        REBCNT index;
        REBCTX *context;
        if ((context = Resolve_Path(word, &index))) {
            key = CTX_KEY(context, index);
            Protect_Key(key, flags);
            if (GET_FLAG(flags, PROT_DEEP)) {
                val = CTX_VAR(context, index);
                Protect_Value(val, flags);
                Unmark(val);
            }
        }
    }
}


//
//  Protect: C
// 
// Common arguments between protect and unprotect:
// 
//     1: value
//     2: /deep  - recursive
//     3: /words  - list of words
//     4: /values - list of values
// 
// Protect takes a HIDE parameter as #5.
//
static REB_R Protect(REBFRM *frame_, REBFLGS flags)
{
    PARAM(1, value);
    REFINE(2, deep);
    REFINE(3, words);
    REFINE(4, values);

    REBVAL *value = ARG(value);

    // flags has PROT_SET bit (set or not)

    Check_Security(Canon(SYM_PROTECT), POL_WRITE, value);

    if (REF(deep)) SET_FLAG(flags, PROT_DEEP);
    //if (REF(words)) SET_FLAG(flags, PROT_WORD);

    if (IS_WORD(value) || IS_PATH(value)) {
        Protect_Word_Value(value, flags); // will unmark if deep
        goto return_value_arg;
    }

    if (IS_BLOCK(value)) {
        if (REF(words)) {
            RELVAL *val;
            for (val = VAL_ARRAY_AT(value); NOT_END(val); val++) {
                REBVAL word; // need binding intact, can't just pass RELVAL
                COPY_VALUE(&word, val, VAL_SPECIFIER(value));
                Protect_Word_Value(&word, flags);  // will unmark if deep
            }
            goto return_value_arg;
        }
        if (REF(values)) {
            REBVAL *var;
            RELVAL *item;

            REBVAL safe;

            for (item = VAL_ARRAY_AT(value); NOT_END(item); ++item) {
                if (IS_WORD(item)) {
                    //
                    // Since we *are* PROTECT we allow ourselves to get mutable
                    // references to even protected values to protect them.
                    //
                    enum Reb_Kind eval_type; // unused
                    var = Get_Var_Core(
                        &eval_type,
                        item,
                        VAL_SPECIFIER(value),
                        GETVAR_READ_ONLY
                    );
                }
                else if (IS_PATH(value)) {
                    if (Do_Path_Throws_Core(
                        &safe, NULL, value, SPECIFIED, NULL
                    ))
                        fail (Error_No_Catch_For_Throw(&safe));

                    var = &safe;
                }
                else {
                    safe = *value;
                    var = &safe;
                }

                Protect_Value(var, flags);
                if (GET_FLAG(flags, PROT_DEEP)) Unmark(var);
            }
            goto return_value_arg;
        }
    }

    if (GET_FLAG(flags, PROT_HIDE)) fail (Error(RE_BAD_REFINES));

    Protect_Value(value, flags);

    if (GET_FLAG(flags, PROT_DEEP)) Unmark(value);

return_value_arg:
    *D_OUT = *ARG(value);
    return R_OUT;
}


//
//  protect: native [
//  
//  {Protect a series or a variable from being modified.}
//  
//      value [word! any-series! bitset! map! object! module!]
//      /deep "Protect all sub-series/objects as well"
//      /words "Process list as words (and path words)"
//      /values "Process list of values (implied GET)"
//      /hide "Hide variables (avoid binding and lookup)"
//  ]
//
REBNATIVE(protect)
{
    PARAM(1, value);
    REFINE(2, deep);
    REFINE(3, words);
    REFINE(4, values);
    REFINE(5, hide);

    REBFLGS flags = FLAGIT(PROT_SET);

    if (REF(hide)) SET_FLAG(flags, PROT_HIDE);
    else SET_FLAG(flags, PROT_WORD); // there is no unhide

    // accesses arguments 1 - 4
    return Protect(frame_, flags);
}


//
//  unprotect: native [
//  
//  {Unprotect a series or a variable (it can again be modified).}
//  
//      value [word! any-series! bitset! map! object! module!]
//      /deep "Protect all sub-series as well"
//      /words "Block is a list of words"
//      /values "Process list of values (implied GET)"
//  ]
//
REBNATIVE(unprotect)
{
    // accesses arguments 1 - 4
    return Protect(frame_, FLAGIT(PROT_WORD));
}
