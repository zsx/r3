//
//  File: %t-function.c
//  Summary: "function related datatypes"
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

static REBOOL Same_Func(const RELVAL *val, const RELVAL *arg)
{
    assert(IS_FUNCTION(val) && IS_FUNCTION(arg));

    // !!! The "EXIT_FROM" tweak that facilitated definitionally scoped RETURN
    // is under study for generalization, to let individual function values
    // distinguish themselves "cheaply" from archetypal ones.  It's not yet
    // generalized cleanly, so we can't guarantee this test...temporarily
    // consider all RETURNs and all LEAVEs equal until ready for this.
    //
    /* if (VAL_FUNC_EXIT_FROM(val) == VAL_FUNC_EXIT_FROM(arg)) */

    if (VAL_FUNC_PARAMLIST(val) == VAL_FUNC_PARAMLIST(arg)) {
        assert(VAL_FUNC_DISPATCHER(val) == VAL_FUNC_DISPATCHER(arg));
        assert(VAL_FUNC_BODY(val) == VAL_FUNC_BODY(arg));
        return TRUE;
    }

    return FALSE;
}


//
//  CT_Function: C
//
REBINT CT_Function(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    if (mode >= 0) return Same_Func(a, b) ? 1 : 0;
    return -1;
}


//
//  MT_Function: C
// 
// For REB_FUNCTION and "make spec", there is a function spec block and then
// a block of Rebol code implementing that function.  In that case we expect
// that `def` should be:
// 
//     [[spec] [body]]
// 
// With REB_COMMAND, the code is implemented via a C DLL, under a system of
// APIs that pre-date Rebol's open sourcing and hence Ren/C:
// 
//     [[spec] extension command-num]
// 
// See notes in Make_Command() regarding that mechanism and meaning.
//
REBOOL MT_Function(
    REBVAL *out, RELVAL *def, REBCTX *specifier, enum Reb_Kind kind
) {
    assert(kind == REB_FUNCTION);

    if (!IS_BLOCK(def)) return FALSE;
    if (VAL_LEN_AT(def) != 2) return FALSE;

    REBVAL spec;
    COPY_VALUE(
        &spec,
        VAL_ARRAY_AT_HEAD(def, 0),
        IS_SPECIFIC(def) ? VAL_SPECIFIER(KNOWN(def)) : specifier
    );
    if (!IS_BLOCK(&spec)) return FALSE;

    REBVAL body;
    COPY_VALUE(
        &body,
        VAL_ARRAY_AT_HEAD(def, 1),
        IS_SPECIFIC(def) ? VAL_SPECIFIER(KNOWN(def)) : specifier
    );
    if (!IS_BLOCK(&body)) return FALSE;

    // Spec-constructed functions do *not* have definitional returns
    // added automatically.  They are part of the generators.  So the
    // behavior comes--as with any other generator--from the projected
    // code (though round-tripping it via text is not possible in
    // general in any case due to loss of bindings.)
    //
    REBFUN *fun = Make_Plain_Function_May_Fail(&spec, &body, MKF_NONE);

    *out = *FUNC_VALUE(fun);

    // We only get here if Make() doesn't raise an error...
    return TRUE;
}


//
//  REBTYPE: C
//
REBTYPE(Function)
{
    REBVAL *value = D_ARG(1);
    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;

    switch (action) {
    case A_TO:
        // `to function! foo` is meaningless (and should not be given meaning,
        // because `to function! [print "DOES exists for this, for instance"]`
        break;

    case A_MAKE:
        if (!IS_DATATYPE(value)) fail (Error_Invalid_Arg(value));

        // MT_Function checks for `[[spec] [body]]` arg if function/closure
        // and for `[[spec] extension command-num]` if command
        if (!IS_BLOCK(arg))
            fail (Error_Bad_Make(VAL_TYPE_KIND(value), arg));
        if (!MT_Function(D_OUT, arg, SPECIFIED, VAL_TYPE_KIND(value)))
            fail (Error_Bad_Make(VAL_TYPE_KIND(value), arg));
        return R_OUT;

    case A_COPY:
        // !!! The R3-Alpha theory was that functions could modify "their
        // bodies" while running, effectively accruing state that one might
        // want to snapshot.  See notes on Clonify_Function about why this
        // idea may be incorrect.
        *D_OUT = *value;
        Clonify_Function(D_OUT);
        return R_OUT;

    case A_REFLECT: {
        REBSYM canon = VAL_WORD_CANON(arg);

        switch (canon) {
        case SYM_ADDR:
            if (
                IS_FUNCTION_RIN(value)
                && GET_RIN_FLAG(VAL_FUNC_ROUTINE(value), ROUTINE_FLAG_CALLBACK)
            ) {
                SET_INTEGER(
                    D_OUT, cast(REBUPT, RIN_DISPATCHER(VAL_FUNC_ROUTINE(value)))
                );
                return R_OUT;
            }
            if (
                IS_FUNCTION_RIN(value)
                && !GET_RIN_FLAG(VAL_FUNC_ROUTINE(value), ROUTINE_FLAG_CALLBACK)
            ) {
                SET_INTEGER(
                    D_OUT, cast(REBUPT, RIN_FUNCPTR(VAL_FUNC_ROUTINE(value)))
                );
                return R_OUT;
            }
            break;

        case SYM_WORDS:
            Val_Init_Block(D_OUT, List_Func_Words(value, FALSE)); // no locals
            return R_OUT;

        case SYM_BODY:
            if (IS_FUNCTION_HIJACKER(value))
                fail (Error(RE_MISC)); // body corrupt, need to recurse

            if (IS_FUNCTION_PLAIN(value)) {
                //
                // BODY-OF is an example of user-facing code that needs to be
                // complicit in the "lie" about the effective bodies of the
                // functions made by the optimized generators FUNC and PROC.
                //
                // Note that since the function body contains relative arrays
                // and words, there needs to be some frame to specify them
                // before a specific REBVAL can be made.  Usually that's the
                // frame of the running instance of the function...but because
                // we're reflecting data out of it, we have to either unbind
                // them or make up a frame.  Making up a frame that acts like
                // it's off the stack and the variables are dead is easiest
                // for now...but long term perhaps unbinding them is better,
                // though this is "more informative".  See #2221.

                REBOOL is_fake;
                REBARR *body = Get_Maybe_Fake_Func_Body(&is_fake, value);
                Val_Init_Block(
                    D_OUT,
                    Copy_Array_Deep_Managed(
                        body,
                        Make_Expired_Frame_Ctx_Managed(VAL_FUNC(value))
                    )
                );

                if (is_fake) Free_Array(body); // was shallow copy
                return R_OUT;
            }

            // For other function types, leak internal guts and hope for
            // the best, temporarily.
            //
            Val_Init_Array(
                D_OUT,
                REB_BLOCK,
                Copy_Array_Deep_Managed(
                    VAL_ARRAY(VAL_FUNC_BODY(value)), SPECIFIED
                )
            );
            return R_OUT;

        case SYM_TYPES: {
            REBARR *copy = Make_Array(VAL_FUNC_NUM_PARAMS(value));
            REBVAL *param;
            REBVAL *typeset;

            // The typesets have a symbol in them for the parameters, and
            // ordinary typesets aren't supposed to have it--that's a
            // special feature for object keys and paramlists!  So clear
            // that symbol out before giving it back.
            //
            param = VAL_FUNC_PARAMS_HEAD(value);
            typeset = SINK(ARR_HEAD(copy));
            for (; NOT_END(param); param++, typeset++) {
                assert(VAL_TYPESET_SYM(param) != SYM_0);
                *typeset = *param;
                VAL_TYPESET_SYM_INIT(typeset, SYM_0);
            }
            SET_END(typeset);
            SET_ARRAY_LEN(copy, VAL_FUNC_NUM_PARAMS(value));

            Val_Init_Block(D_OUT, copy);
            return R_OUT;
        }

        default:
            fail (Error_Cannot_Reflect(VAL_TYPE(value), arg));
        }
        break; }
    }

    fail (Error_Illegal_Action(VAL_TYPE(value), action));
}


//
//  func-class-of: native [
//
//  {Internal-use-only for implementing NATIVE?, ACTION?, CALLBACK?, etc.}
//
//      func [function!]
//  ]
//
REBNATIVE(func_class_of)
//
// !!! The concept of the VAL_FUNC_CLASS was killed, because functions get
// their classification by way of their dispatch pointers.  Generally
// speaking, functions should be a "black box" to user code, and it's only
// at the "meta" level that a function would choose to expose whether it
// is something like a specialization or an adaptation...but that would be
// purely documentary, and could lie.
{
    PARAM(1, func);

    REBVAL *value = ARG(func);
    REBCNT n;

    if (IS_FUNCTION_PLAIN(value))
        n = 2;
    else if (IS_FUNCTION_ACTION(value))
        n = 3;
    else if (IS_FUNCTION_COMMAND(value))
        n = 4;
    else if (IS_FUNCTION_RIN(value)) {
        if (!GET_RIN_FLAG(VAL_FUNC_ROUTINE(value), ROUTINE_FLAG_CALLBACK))
            n = 5;
        else
            n = 6;
    }
    else if (IS_FUNCTION_SPECIALIZER(value))
        n = 7;
    else {
        // !!! A shaky guess, but assume native if none of the above.
        n = 1;
    }

    SET_INTEGER(D_OUT, n);
    return R_OUT;
}
