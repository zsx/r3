/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Module:  t-function.c
**  Summary: function related datatypes
**  Section: datatypes
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"

static REBOOL Same_Func(const REBVAL *val, const REBVAL *arg)
{
    if (VAL_TYPE(val) == VAL_TYPE(arg) &&
        VAL_FUNC_SPEC(val) == VAL_FUNC_SPEC(arg) &&
        VAL_FUNC_PARAMLIST(val) == VAL_FUNC_PARAMLIST(arg) &&
        VAL_FUNC_CODE(val) == VAL_FUNC_CODE(arg)) return TRUE;
    return FALSE;
}


//
//  CT_Function: C
//
REBINT CT_Function(const REBVAL *a, const REBVAL *b, REBINT mode)
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
REBOOL MT_Function(REBVAL *out, REBVAL *def, enum Reb_Kind kind)
{
    // Spec-constructed functions do *not* have definitional returns
    // added automatically.  They are part of the generators.  So the
    // behavior comes--as with any other generator--from the projected
    // code (though round-tripping it via text is not possible in
    // general in any case due to loss of bindings.)
    //
    const REBOOL has_return = FALSE;
    const REBOOL returns_unset = FALSE;

    REBVAL *spec;
    REBVAL *body;

    assert(kind == REB_FUNCTION);

    if (!IS_BLOCK(def)) return FALSE;
    if (VAL_LEN_AT(def) != 2) return FALSE;

    spec = VAL_ARRAY_AT_HEAD(def, 0);
    if (!IS_BLOCK(spec)) return FALSE;

    body = VAL_ARRAY_AT_HEAD(def, 1);
    if (!IS_BLOCK(body)) return FALSE;

    Make_Function(out, returns_unset, spec, body, has_return);

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
        if (!MT_Function(D_OUT, arg, VAL_TYPE_KIND(value)))
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

    case A_REFLECT:
        if (
            VAL_FUNC_CLASS(value) == FUNC_CLASS_CALLBACK
            && VAL_WORD_CANON(arg) == SYM_ADDR
        ) {
            // !!! Temp...there was no OF_SPEC when integrating MT_XXXs
            //
            SET_INTEGER(D_OUT, cast(REBUPT, VAL_ROUTINE_DISPATCHER(value)));
            return R_OUT;
        }

        if (
            VAL_FUNC_CLASS(value) == FUNC_CLASS_ROUTINE
            && VAL_WORD_CANON(arg) == SYM_ADDR
        ) {
            // !!! Same as above, temporary while exploring solutions.
            //
            SET_INTEGER(D_OUT, cast(REBUPT, VAL_ROUTINE_FUNCPTR(value)));
            return R_OUT;
        }

        switch (What_Reflector(arg)) {
        case OF_WORDS:
            Val_Init_Block(D_OUT, List_Func_Words(value));
            return R_OUT;

        case OF_BODY:
            switch (VAL_FUNC_CLASS(value)) {
            case FUNC_CLASS_NATIVE: {
                if (VAL_FUNC_CODE(value) == &N_quote) {
                    //
                    // !!! Should be able to give "equivalent" body of code for
                    // some native functions.  /body already being passed to
                    // a few natives in definitions, just need a place to save
                    // the information (improve native table).
                    //
                    Val_Init_Array(
                        D_OUT,
                        REB_GROUP,
                        Copy_Array_Deep_Managed(
                            VAL_ARRAY(Get_System(
                                SYS_STANDARD,
                                STD_QUOTE_BODY_TEST
                            ))
                        ));
                }
                else {
                    SET_HANDLE_CODE(D_OUT, cast(CFUNC*, VAL_FUNC_CODE(value)));
                }

                return R_OUT;
            }

            case FUNC_CLASS_ACTION: {
                SET_INTEGER(D_OUT, VAL_FUNC_ACT(value));
                return R_OUT;
            }

            case FUNC_CLASS_USER: {
                //
                // BODY-OF is an example of user-facing code that needs to be
                // complicit in the "lie" about the effective bodies of the
                // functions made by the optimized generators FUNC and PROC...

                REBOOL is_fake;
                REBARR *body = Get_Maybe_Fake_Func_Body(&is_fake, value);
                Val_Init_Block(D_OUT, Copy_Array_Deep_Managed(body));

                if (IS_FUNC_DURABLE(value)) {
                    // See #2221 for why durable body copies unbind locals
                    Unbind_Values_Core(
                        VAL_ARRAY_HEAD(D_OUT),
                        AS_CONTEXT(VAL_FUNC_PARAMLIST(value)),
                        TRUE
                    );
                }
                if (is_fake) Free_Array(body); // was shallow copy
                return R_OUT;
            }

            case FUNC_CLASS_COMMAND: {
                SET_INTEGER(D_OUT, 0);
                return R_OUT;
            }

            case FUNC_CLASS_ROUTINE: {
                SET_INTEGER(D_OUT, 0);
                return R_OUT;
            }

            case FUNC_CLASS_CALLBACK: {
                Val_Init_Array(D_OUT, REB_BLOCK, VAL_FUNC_BODY(value));
                return R_OUT;
            }

            case FUNC_CLASS_SPECIALIZED:
                //
                // Just a FRAME! (turned into a BLOCK! by mezzanine BODY-OF)
                //
                Val_Init_Context(D_OUT, REB_FRAME, VAL_FUNC_SPECIAL(value));
                return R_OUT;

            default:
                assert(FALSE);
                return R_NONE;
            }
            break;

        case OF_SPEC:
            Val_Init_Block(
                D_OUT, Copy_Array_Deep_Managed(VAL_FUNC_SPEC(value))
            );

            Unbind_Values_Deep(VAL_ARRAY_HEAD(D_OUT));
            return R_OUT;

        case OF_TYPES: {
            REBARR *copy = Make_Array(VAL_FUNC_NUM_PARAMS(value));
            REBVAL *param;
            REBVAL *typeset;

            // The typesets have a symbol in them for the parameters, and
            // ordinary typesets aren't supposed to have it--that's a
            // special feature for object keys and paramlists!  So clear
            // that symbol out before giving it back.
            //
            param = VAL_FUNC_PARAMS_HEAD(value);
            typeset = ARR_HEAD(copy);
            for (; NOT_END(param); param++, typeset++) {
                assert(VAL_TYPESET_SYM(param) != SYM_0);
                *typeset = *param;
                VAL_TYPESET_SYM(typeset) = SYM_0;
            }
            SET_END(typeset);
            SET_ARRAY_LEN(copy, VAL_FUNC_NUM_PARAMS(value));

            Val_Init_Block(D_OUT, copy);
            return R_OUT;
        }

        case OF_TITLE:
            //
            // Get the first STRING! before any parameter definitions, or
            // NONE! if there isn't one.
            //
            // !!! Is the "TITLE" actually something that should be canonized
            // by the reflection API, or is it entirely up to how the spec is
            // interpreted by HELP?  The policy on allowing strings to be
            // skipped and preserved leans toward the latter concept.
            //
            arg = ARR_HEAD(VAL_FUNC_SPEC(value));
            for (; NOT_END(arg); arg++) {
                if (IS_STRING(arg)) {
                    Val_Init_String(D_OUT, Copy_Sequence(VAL_SERIES(arg)));
                    return R_OUT;
                }

                if (ANY_WORD(arg)) {
                    //
                    // Parameter, so no title after this point.  Note that
                    // "ANY-WORD!" includes ISSUE!, which currently doesn't
                    // have meaning in function specs.
                    //
                    break;
                }
            }
            return R_NONE;

        default:
            fail (Error_Cannot_Reflect(VAL_TYPE(value), arg));
        }
        break;
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
{
    PARAM(1, func);

    SET_INTEGER(D_OUT, VAL_FUNC_CLASS(ARG(func)));
    return R_OUT;
}
