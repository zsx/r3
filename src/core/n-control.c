//
//  File: %n-control.c
//  Summary: "native functions for control flow"
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
static void Protect_Key(REBVAL *key, REBFLGS flags)
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
void Protect_Value(REBVAL *value, REBFLGS flags)
{
    if (ANY_SERIES(value) || IS_MAP(value))
        Protect_Series(value, flags);
    else if (IS_OBJECT(value) || IS_MODULE(value))
        Protect_Object(value, flags);
}


//
//  Protect_Series: C
// 
// Anything that calls this must call Unmark() when done.
//
void Protect_Series(REBVAL *val, REBFLGS flags)
{
    REBSER *series = VAL_SERIES(val);

    if (GET_SER_FLAG(series, SERIES_FLAG_MARK)) return; // avoid loop

    if (GET_FLAG(flags, PROT_SET))
        SET_SER_FLAG(series, SERIES_FLAG_LOCKED);
    else
        CLEAR_SER_FLAG(series, SERIES_FLAG_LOCKED);

    if (!ANY_ARRAY(val) || !GET_FLAG(flags, PROT_DEEP)) return;

    SET_SER_FLAG(series, SERIES_FLAG_MARK); // recursion protection

    for (val = VAL_ARRAY_AT(val); NOT_END(val); val++) {
        Protect_Value(val, flags);
    }
}


//
//  Protect_Object: C
// 
// Anything that calls this must call Unmark() when done.
//
void Protect_Object(REBVAL *value, REBFLGS flags)
{
    REBCTX *context = VAL_CONTEXT(value);

    if (GET_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_MARK))
        return; // avoid loop

    if (GET_FLAG(flags, PROT_SET))
        SET_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_LOCKED);
    else
        CLEAR_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_LOCKED);

    for (value = CTX_KEY(context, 1); NOT_END(value); value++) {
        Protect_Key(value, flags);
    }

    if (!GET_FLAG(flags, PROT_DEEP)) return;

    SET_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_MARK); // recursion protection

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
            REBOOL lookback; // ignored
            val = Get_Var_Core(&lookback, word, GETVAR_READ_ONLY);
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
static int Protect(struct Reb_Frame *frame_, REBFLGS flags)
{
    PARAM(1, value);
    REFINE(2, deep);
    REFINE(3, words);
    REFINE(4, values);

    REBVAL *val = ARG(value);

    // flags has PROT_SET bit (set or not)

    Check_Security(SYM_PROTECT, POL_WRITE, val);

    if (REF(deep)) SET_FLAG(flags, PROT_DEEP);
    //if (REF(words)) SET_FLAG(flags, PROT_WORD);

    if (IS_WORD(val) || IS_PATH(val)) {
        Protect_Word_Value(val, flags); // will unmark if deep
        goto return_value_arg;
    }

    if (IS_BLOCK(val)) {
        if (REF(words)) {
            for (val = VAL_ARRAY_AT(val); NOT_END(val); val++)
                Protect_Word_Value(val, flags);  // will unmark if deep
            goto return_value_arg;
        }
        if (REF(values)) {
            REBVAL *val2;

            REBVAL safe;

            for (val = VAL_ARRAY_AT(val); NOT_END(val); val++) {
                if (IS_WORD(val)) {
                    //
                    // Since we *are* PROTECT we allow ourselves to get mutable
                    // references to even protected values to protect them.
                    //
                    REBOOL lookback; // ignored
                    Get_Var_Core(&lookback, val, GETVAR_READ_ONLY);
                }
                else if (IS_PATH(val)) {
                    if (Do_Path_Throws(&safe, NULL, val, NULL))
                        fail (Error_No_Catch_For_Throw(&safe));

                    val2 = &safe;
                }
                else
                    val2 = val;

                Protect_Value(val2, flags);
                if (GET_FLAG(flags, PROT_DEEP)) Unmark(val2);
            }
            goto return_value_arg;
        }
    }

    if (GET_FLAG(flags, PROT_HIDE)) fail (Error(RE_BAD_REFINES));

    Protect_Value(val, flags);

    if (GET_FLAG(flags, PROT_DEEP)) Unmark(val);

return_value_arg:
    *D_OUT = *ARG(value);
    return R_OUT;
}


//
//  also: native [
//  
//  {Returns the first value, but also evaluates the second.}
//  
//      value1 [<opt> any-value!]
//      value2 [<opt> any-value!]
//  ]
//
REBNATIVE(also)
{
    PARAM(1, value1);
    PARAM(2, value2);

    *D_OUT = *ARG(value1);
    return R_OUT;
}


//
//  all: native [
//  
//  {Shortcut AND. Returns NONE vs. TRUE (or last evaluation if it was TRUE?)}
//  
//      block [block!] "Block of expressions"
//  ]
//
REBNATIVE(all)
//
// ALL is effectively Rebol's "short-circuit AND".  Unsets do not vote either
// true or false...they are ignored.
// 
// To offer a more generically useful result than just TRUE or FALSE, it will
// use as a "truthy" value whatever the last evaluation in the chain was.  If
// there was no last value, but no conditionally false instance hit to break
// the chain, as in `all []` or `all [1 2 ()]`...it will return TRUE.
// 
// (Note: It would become a more costly operation to retain the last truthy
// value to return 2 in the case of `all [1 2 ()`]`, just to say it could.
// The overhead would undermine the raw efficiency of the operation.)
// 
// For the "falsy" value, ALL uses a NONE! rather than logic FALSE.  It's a
// historical design decision which has some benefits, but perhaps some
// drawbacks to those wishing to use it on logic values and stay in the
// logic domain.  (`all [true true]` => true, `all [false true]` is NONE!).
{
    REBARR *block = VAL_ARRAY(D_ARG(1));
    REBIXO indexor = VAL_INDEX(D_ARG(1));

    SET_TRUE(D_OUT);

    while (indexor != END_FLAG) {
        DO_NEXT_MAY_THROW(indexor, D_OUT, block, indexor);
        if (indexor == THROWN_FLAG) return R_OUT_IS_THROWN;

        if (IS_VOID(D_OUT)) continue;

        if (IS_CONDITIONAL_FALSE(D_OUT)) return R_BLANK;
    }

    if (IS_VOID(D_OUT)) return R_TRUE;

    return R_OUT;
}


//
//  any: native [
//  
//  {Shortcut OR, ignores unsets. Returns the first TRUE? result, or NONE.}
//  
//      block [block!] "Block of expressions"
//  ]
//
REBNATIVE(any)
//
// ANY is effectively Rebol's "short-circuit OR".  Unsets do not vote either
// true or false...they are ignored.
// 
// See ALL's notes about returning the last truthy value or NONE! (vs. FALSE)
// 
// The base case of `any []` is NONE! and not TRUE.  This might seem strange
// given that `all []` is TRUE.  But this ties more into what the questions
// they are used to ask about in practice: "Were all of these things not
// false?" as opposed to "Were any of these things true?"  It is also the
// case that `FALSE OR X OR Y` matches with `TRUE AND X AND Y` as the
// "seed" for not affecting the chain.
{
    REBARR *block = VAL_ARRAY(D_ARG(1));
    REBIXO indexor = VAL_INDEX(D_ARG(1));

    while (indexor != END_FLAG) {
        DO_NEXT_MAY_THROW(indexor, D_OUT, block, indexor);
        if (indexor == THROWN_FLAG) return R_OUT_IS_THROWN;

        if (IS_VOID(D_OUT)) continue;

        if (IS_CONDITIONAL_TRUE(D_OUT)) return R_OUT;
    }

    return R_BLANK;
}


//
//  none: native [
//
//  {Shortcut NOR, ignores unsets. Returns TRUE if all FALSE?, or BLANK.}
//
//      block [block!] "Block of expressions"
//  ]
//
REBNATIVE(none)
//
// !!! In order to reduce confusion and accidents in the near term, the
// %mezz-legacy.r renames this to NONE-OF and makes NONE report an error.
{
    REBARR *block = VAL_ARRAY(D_ARG(1));
    REBIXO indexor = VAL_INDEX(D_ARG(1));

    while (indexor != END_FLAG) {
        DO_NEXT_MAY_THROW(indexor, D_OUT, block, indexor);
        if (indexor == THROWN_FLAG) return R_OUT_IS_THROWN;

        if (IS_VOID(D_OUT)) continue;

        if (IS_CONDITIONAL_TRUE(D_OUT)) return R_BLANK;
    }

    return R_TRUE;
}


//
//  attempt: native [
//  
//  {Tries to evaluate a block and returns result or NONE on error.}
//  
//      block [block!]
//  ]
//
REBNATIVE(attempt)
{
    REBVAL * const block = D_ARG(1);

    struct Reb_State state;
    REBCTX *error;

    PUSH_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) return R_BLANK;

    if (DO_VAL_ARRAY_AT_THROWS(D_OUT, block)) {
        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

        // Throw name is in D_OUT, thrown value is held task local
        return R_OUT_IS_THROWN;
    }

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    return R_OUT;
}


//
//  break: native [
//  
//  {Exit the current iteration of a loop and stop iterating further.}
//  
//      /with
//          {Act as if loop body finished current evaluation with a value}
//      value [<opt> any-value!]
//  ]
//
REBNATIVE(break)
//
// BREAK is implemented via a THROWN() value that bubbles up through
// the stack.  It uses the value of its own native function as the
// name of the throw, like `throw/name value :break`.
{
    REFINE(1, with);
    PARAM(2, value);

    REBVAL *value = REF(with) ? ARG(value) : VOID_CELL;

    *D_OUT = *FUNC_VALUE(D_FUNC);

    CONVERT_NAME_TO_THROWN(D_OUT, value);

    return R_OUT_IS_THROWN;
}


//
//  case: native [
//  
//  {Evaluates each condition, and when true, evaluates what follows it.}
//  
//      block [block!]
//          "Block of cases (conditions followed by values)"
//      /all
//          {Evaluate all cases (do not stop at first TRUE? case)}
//      /?
//          "Instead of last case result, return LOGIC! of if any case matched"
//  ]
//
REBNATIVE(case)
{
    PARAM(1, block);
    REFINE(2, all);
    REFINE(3, q);

    // Save refinement to boolean to free up call frame slot, and reuse its
    // cell as a temporary GC-safe location for holding evaluations.
    //
    REBOOL all = REF(all);
    REBVAL *safe_temp = ARG(all);

    // condition result must survive across potential GC evaluations of
    // the body evaluation re-using `safe-temp`, but can be collapsed to a
    // flag as the full value of the condition is never returned.
    //
    REBOOL matched;

    // To evaluate in a linear sequence most efficiently, CASE uses a single
    // frame, instead of using macros that build and tear one down on each
    // individual evaluation.
    //
    struct Reb_Frame frame;
    struct Reb_Frame *f = &frame;

    PUSH_ARTIFICIAL_CALL_UNLESS_END(f, ARG(block));
    if (f->indexor == END_FLAG) { // quickly terminate on empty array
        if (REF(q))
            return R_FALSE;

        SET_VOID_UNLESS_LEGACY_NONE(D_OUT);
        return R_OUT;
    }

    REBOOL none_ran = TRUE; // !!! TBD: END marker default, so = IS_END(D_OUT)

    while (f->indexor != END_FLAG) {
        UPDATE_EXPRESSION_START(f);
        if (IS_BAR(f->value)) {
            //
            // interstitial (e.g. `case [1 2 | 3 4]`) - BAR! legal here, skip
            //
            FETCH_NEXT_ONLY_MAYBE_END(f);
            continue;
        }

        DO_NEXT_REFETCH_MAY_THROW(safe_temp, f, DO_FLAG_LOOKAHEAD);

        if (f->indexor == THROWN_FLAG) {
            *D_OUT = *safe_temp; // is a RETURN, BREAK, THROW...
            DROP_CALL(f);
            return R_OUT_IS_THROWN;
        }

        // CASE statements are rather freeform as-is, and it seems most useful
        // to return an error on things like:
        //
        //     case [
        //         false [print "skipped"]
        //         false ; no matching body for condition
        //     ]
        //
        if (f->indexor == END_FLAG) {
        #if !defined(NDEBUG)
            if (LEGACY(OPTIONS_BROKEN_CASE_SEMANTICS)) {
                // case [first [a b c]] => true ;-- in Rebol2
                DROP_CALL(f);
                return R_TRUE;
            }
        #endif

            fail (Error(RE_PAST_END));
        }

        // While unset is often a chance to "opt-out" of things, the condition
        // of an IF/UNLESS/EITHER is a spot where opting out is not allowed,
        // so it seems equally applicable to CASE.
        //
        if (IS_VOID(safe_temp)) fail (Error(RE_NO_RETURN));

        // Expression barriers in CASE statements are only allowed at the
        // in-between-pairs spots.  This maximizes their usefulness, because
        // they can actually catch something interesting (being out of sync
        // on conditions and branches).
        //
        if (IS_BAR(f->value))
            fail (Error(RE_BAR_HIT_MID_CASE));

        matched = IS_CONDITIONAL_TRUE(safe_temp);

        // We DO the next expression, rather than just assume it is a
        // literal block.  That allows you to write things like:
        //
        //     condition: true
        //     case [condition 10 + 20] ;-- returns 30
        //
        // But we need to DO regardless of the condition being true or
        // false.  Rebol2 would just skip over one item (the 10 in this
        // case) and get an error.  Code not in blocks must be evaluated
        // even if false, as it is with 'if false (print "eval'd")'
        //
        // If the source was a literal block then the Do_Next_May_Throw
        // will *probably* be a no-op, but consider infix operators:
        //
        //     case [true [stuff] + [more stuff]]
        //
        // Until such time as DO guarantees such things aren't legal,
        // CASE must evaluate block literals too.

    #if !defined(NDEBUG)
        if (LEGACY(OPTIONS_BROKEN_CASE_SEMANTICS) && !matched) {
            //
            // case [true add 1 2] => 3
            // case [false add 1 2] => 2 ;-- in Rebol2
            //
            FETCH_NEXT_ONLY_MAYBE_END(f);

            // forgets the last evaluative result for a TRUE condition
            // when /ALL is set (instead of keeping it to return)
            //
            SET_VOID_UNLESS_LEGACY_NONE(D_OUT);
            continue;
        }
    #endif

        DO_NEXT_REFETCH_MAY_THROW(safe_temp, f, DO_FLAG_LOOKAHEAD);

        if (f->indexor == THROWN_FLAG) {
            *D_OUT = *safe_temp; // is a RETURN, BREAK, THROW...
            DROP_CALL(f);
            return R_OUT_IS_THROWN;
        }

        if (matched) {
            if (IS_BLOCK(safe_temp)) {
                //
                // The classical implementation of CASE is defined to give two
                // evals for things like:
                //
                //     stuff: [print "This will be printed"]
                //     case [true stuff]
                //
                // This puts it more closely in the spirit of being a kind of
                // "optimized IF-ELSE" as `if true stuff` would also behave
                // in the manner of running that block.
                //
                if (DO_VAL_ARRAY_AT_THROWS(D_OUT, safe_temp)) {
                    DROP_CALL(f);
                    return R_OUT_IS_THROWN;
                }
            }
            else
                *D_OUT = *safe_temp;

        #if !defined(NDEBUG)
            if (LEGACY(OPTIONS_BROKEN_CASE_SEMANTICS)) {
                if (IS_VOID(D_OUT)) {
                    // case [true [] false [1 + 2]] => true ;-- in Rebol2
                    SET_TRUE(D_OUT);
                }
            }
        #endif

            if (all) {
                none_ran = FALSE;
                continue; // keep matching if /ALL
            }

            DROP_CALL(f);

            if (REF(q)) return R_TRUE; // one case ran (this one) for /?

            return R_OUT; // evaluative result of case otherwise (may be void)
        }
    }

    DROP_CALL(f);

    // D_OUT will still be END if no cases ran

    if (REF(q))
        return none_ran ? R_FALSE : R_TRUE; // /? asks if any cases ran

    if (none_ran)
        SET_VOID_UNLESS_LEGACY_NONE(D_OUT); // otherwise void if no cases ran

    return R_OUT; // last case evaluative result if a case ran (may be void)
}


//
//  catch: native [
//  
//  {Catches a throw from a block and returns its value.}
//  
//      block [block!] "Block to evaluate"
//      /name
//          "Catches a named throw" ;-- should it be called /named ?
//      names [block! word! function! object!]
//          "Names to catch (single name if not block)"
//      /quit
//          "Special catch for QUIT native"
//      /any
//          {Catch all throws except QUIT (can be used with /QUIT)}
//      /with
//          "Handle thrown case with code"
//      handler [block! function!]
//          "If FUNCTION!, spec matches [value name]"
//      /?
//         "Instead of result or catch, return LOGIC! of if a catch occurred"
//  ]
//
REBNATIVE(catch)
//
// There's a refinement for catching quits, and CATCH/ANY will not alone catch
// it (you have to CATCH/ANY/QUIT).  Currently the label for quitting is the
// NATIVE! function value for QUIT.
{
    PARAM(1, block);
    REFINE(2, name);
    PARAM(3, names);
    REFINE(4, quit);
    REFINE(5, any);
    REFINE(6, with);
    PARAM(7, handler);
    REFINE(8, q);

    // /ANY would override /NAME, so point out the potential confusion
    //
    if (REF(any) && REF(name))
        fail (Error(RE_BAD_REFINES));

    if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(block))) {
        if (
            (
                REF(any)
                && (
                    !IS_FUNCTION_AND(D_OUT, FUNC_CLASS_NATIVE)
                    || VAL_FUNC_CODE(D_OUT) != &N_quit
                )
            )
            || (
                REF(quit)
                && (
                    IS_FUNCTION_AND(D_OUT, FUNC_CLASS_NATIVE)
                    && VAL_FUNC_CODE(D_OUT) == &N_quit
                )
            )
        ) {
            goto was_caught;
        }

        if (REF(name)) {
            //
            // We use equal? by way of Compare_Modify_Values, and re-use the
            // refinement slots for the mutable space

            REBVAL * const temp1 = ARG(quit);
            REBVAL * const temp2 = ARG(any);

            // !!! The reason we're copying isn't so the VALUE_FLAG_THROWN bit
            // won't confuse the equality comparison...but would it have?

            if (IS_BLOCK(ARG(names))) {
                //
                // Test all the words in the block for a match to catch

                REBVAL *candidate = VAL_ARRAY_AT(ARG(names));
                for (; NOT_END(candidate); candidate++) {
                    //
                    // !!! Should we test a typeset for illegal name types?
                    //
                    if (IS_BLOCK(candidate))
                        fail (Error(RE_INVALID_ARG, ARG(names)));

                    *temp1 = *candidate;
                    *temp2 = *D_OUT;

                    // Return the THROW/NAME's arg if the names match
                    // !!! 0 means equal?, but strict-equal? might be better
                    //
                    if (Compare_Modify_Values(temp1, temp2, 0))
                        goto was_caught;
                }
            }
            else {
                *temp1 = *ARG(names);
                *temp2 = *D_OUT;

                // Return the THROW/NAME's arg if the names match
                // !!! 0 means equal?, but strict-equal? might be better
                //
                if (Compare_Modify_Values(temp1, temp2, 0))
                    goto was_caught;
            }
        }
        else {
            // Return THROW's arg only if it did not have a /NAME supplied
            //
            if (IS_BLANK(D_OUT))
                goto was_caught;
        }

        // Throw name is in D_OUT, thrown value is held task local
        //
        return R_OUT_IS_THROWN;
    }

    if (REF(q)) return R_FALSE;

    return R_OUT;

was_caught:
    if (REF(with)) {
        REBVAL *handler = ARG(handler);

        // We again re-use the refinement slots, but this time as mutable
        // space protected from GC for the handler's arguments
        //
        REBVAL *thrown_arg = ARG(any);
        REBVAL *thrown_name = ARG(quit);

        CATCH_THROWN(thrown_arg, D_OUT);
        *thrown_name = *D_OUT; // THROWN bit cleared by TAKE_THROWN_ARG

        if (IS_BLOCK(handler)) {
            //
            // There's no way to pass args to a block (so just DO it)
            //
            if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(handler)))
                return R_OUT_IS_THROWN;

            if (REF(q)) return R_TRUE;

            return R_OUT;
        }
        else if (IS_FUNCTION(handler)) {
            //
            // !!! THIS CAN BE REWRITTEN AS A DO/NEXT via Do_Va_Core()!
            // There's no reason to have each of these cases when it
            // could just be one call that either consumes all the
            // subsequent args or does not.
            //
            if (
                VAL_FUNC_NUM_PARAMS(handler) == 0
                || IS_REFINEMENT(VAL_FUNC_PARAM(handler, 1))
            ) {
                // If the handler is zero arity or takes a first parameter
                // that is a refinement, call it with no arguments
                //
                if (Apply_Only_Throws(D_OUT, handler, END_CELL))
                    return R_OUT_IS_THROWN;
            }
            else if (
                VAL_FUNC_NUM_PARAMS(handler) == 1
                || IS_REFINEMENT(VAL_FUNC_PARAM(handler, 2))
            ) {
                // If the handler is arity one (with a non-refinement
                // parameter), or a greater arity with a second parameter that
                // is a refinement...call it with *just* the thrown value.
                //
                if (Apply_Only_Throws(D_OUT, handler, thrown_arg, END_CELL))
                    return R_OUT_IS_THROWN;
            }
            else {
                // For all other handler signatures, try passing both the
                // thrown arg and the thrown name.  Let Apply take care of
                // checking that the arguments are legal for the call.
                //
                if (Apply_Only_Throws(
                    D_OUT, handler, thrown_arg, thrown_name, END_CELL
                )) {
                    return R_OUT_IS_THROWN;
                }
            }

            if (REF(q)) return R_TRUE;

            return R_OUT;
        }
    }

    // If no handler, just return the caught thing
    //
    CATCH_THROWN(D_OUT, D_OUT);

    if (REF(q)) return R_TRUE;

    return R_OUT;
}


//
//  throw: native [
//  
//  "Throws control back to a previous catch."
//  
//      value [<opt> any-value!] "Value returned from catch"
//      /name "Throws to a named catch"
//      name-value [word! function! object!]
//  ]
//
REBNATIVE(throw)
{
    REBVAL * const value = D_ARG(1);
    REBOOL named = D_REF(2);
    REBVAL * const name_value = D_ARG(3);

    if (IS_ERROR(value)) {
        //
        // We raise an alert from within the implementation of throw for
        // trying to use it to trigger errors, because if THROW just didn't
        // take errors in the spec it wouldn't guide what *to* use.
        //
        fail (Error(RE_USE_FAIL_FOR_ERROR, value));

        // Note: Caller can put the ERROR! in a block or use some other
        // such trick if it wants to actually throw an error.
        // (Better than complicating via THROW/ERROR-IS-INTENTIONAL!)
    }

    if (named) {
        // blocks as names would conflict with name_list feature in catch
        assert(!IS_BLOCK(name_value));
        *D_OUT = *name_value;
    }
    else {
        // None values serving as representative of THROWN() means "no name"

        // !!! This convention might be a bit "hidden" while debugging if
        // one misses the THROWN() bit.  But that's true of THROWN() values
        // in general.  Debug output should make noise about THROWNs
        // whenever it sees them.

        SET_BLANK(D_OUT);
    }

    CONVERT_NAME_TO_THROWN(D_OUT, value);

    // Throw name is in D_OUT, thrown value is held task local
    return R_OUT_IS_THROWN;
}


//
//  comment: native [
//
//  {Ignores the argument value and returns nothing (with no evaluations).}
//
//      :value [block! any-string! binary! any-scalar!]
//          "Literal value to be ignored."
//  ]
//
REBNATIVE(comment)
{
    PARAM(1, value);

    // All the work was already done (at the cost of setting up
    // state that would just have to be torn down).

    return R_VOID;
}


//
//  continue: native [
//  
//  "Throws control back to top of loop for next iteration."
//  
//      /with
//          {Act as if loop body finished current evaluation with a value}
//      value [<opt> any-value!]
//  ]
//
REBNATIVE(continue)
//
// CONTINUE is implemented via a THROWN() value that bubbles up through
// the stack.  It uses the value of its own native function as the
// name of the throw, like `throw/name value :continue`.
{
    REFINE(1, with);
    PARAM(2, value);

    REBVAL *value = REF(with) ? ARG(value) : VOID_CELL;

    *D_OUT = *FUNC_VALUE(D_FUNC);

    CONVERT_NAME_TO_THROWN(D_OUT, value);

    return R_OUT_IS_THROWN;
}


//
//  do: native [
//  
//  {Evaluates a block of source code (directly or fetched according to type)}
//  
//      source [
//          <opt> ;-- should DO accept an optional argument (chaining?)
//          blank! ;-- same question... necessary, or not?
//          block! ;-- source code in block form
//          string! ;-- source code in text form
//          binary! ;-- treated as UTF-8
//          url! ;-- load code from URL via protocol
//          file! ;-- load code from file on local disk
//          group! ;-- !!! likely not needed with the AS aliasing
//          tag! ;-- proposed as module library tag name, hacked as demo
//          error! ;-- should use FAIL instead
//          function! ;-- will only run arity 0 functions (avoids DO variadic)
//          frame! ;-- acts like APPLY (voids are optionals, not unspecialized)
//      ]
//      /args
//          {If value is a script, this will set its system/script/args}
//      arg
//          "Args passed to a script (normally a string)"
//      /next
//          {Do next expression only, return it, update block variable}
//      var [word! blank!]
//          "Variable updated with new block position"
//  ]
//
REBNATIVE(do)
{
    PARAM(1, value);
    REFINE(2, args);
    PARAM(3, arg);
    REFINE(4, next);
    PARAM(5, var); // if BLANK!, DO/NEXT only but no var update

    REBVAL *value = ARG(value);

    switch (VAL_TYPE(value)) {
    case REB_0:
        // useful for `do if ...` types of scenarios
        return R_VOID;

    case REB_BLANK:
        // useful for `do all ...` types of scenarios
        return R_BLANK;

    case REB_BLOCK:
    case REB_GROUP:
        if (REF(next)) {
            REBIXO indexor = VAL_INDEX(value);

            DO_NEXT_MAY_THROW(
                indexor, // updated
                D_OUT,
                VAL_ARRAY(value),
                indexor
            );

            if (indexor == THROWN_FLAG) {
                // the throw should make the value irrelevant, but if caught
                // then have it indicate the start of the thrown expression

                // !!! What if the block was mutated, and D_ARG(1) is no
                // longer actually the expression that started the throw?

                if (!IS_BLANK(ARG(var)))
                    *GET_MUTABLE_VAR_MAY_FAIL(ARG(var)) = *value;
                return R_OUT_IS_THROWN;
            }

            if (!IS_BLANK(ARG(var))) {
                //
                // "continuation" of block...turn END_FLAG into the end so it
                // can test TAIL? as true to know the evaluation finished.
                //
                // !!! Is there merit to setting to NONE! instead?  Easier to
                // test and similar to FIND.  On the downside, "lossy" in
                // that after the DOs are finished the var can't be used to
                // recover the series again...you'd have to save it.
                //
                if (indexor == END_FLAG)
                    VAL_INDEX(value) = VAL_LEN_HEAD(value);
                else
                    VAL_INDEX(value) = cast(REBCNT, indexor);

                *GET_MUTABLE_VAR_MAY_FAIL(ARG(var)) = *ARG(value);
            }

            return R_OUT;
        }

        if (DO_VAL_ARRAY_AT_THROWS(D_OUT, value))
            return R_OUT_IS_THROWN;

        return R_OUT;

    case REB_BINARY:
    case REB_STRING:
    case REB_URL:
    case REB_FILE:
    case REB_TAG:
        //
        // See code called in system/intrinsic/do*
        //
        if (Apply_Only_Throws(
            D_OUT,
            Sys_Func(SYS_CTX_DO_P),
            value,
            REF(args) ? TRUE_VALUE : FALSE_VALUE,
            REF(args) ? ARG(arg) : BLANK_VALUE, // can't put void in block
            REF(next) ? TRUE_VALUE : FALSE_VALUE,
            REF(next) ? ARG(var) : BLANK_VALUE, // can't put void in block
            END_CELL
        )) {
            return R_OUT_IS_THROWN;
        }
        return R_OUT;

    case REB_ERROR:
        //
        // FAIL is the preferred operation for triggering errors, as it has
        // a natural behavior for blocks passed to construct readable messages
        // and "FAIL X" more clearly communicates a failure than "DO X"
        // does.  However DO of an ERROR! would have to raise an error
        // anyway, so it might as well raise the one it is given.
        //
        fail (VAL_CONTEXT(value));

    case REB_FUNCTION: {
        //
        // Ren-C will only run arity 0 functions from DO, otherwise EVAL
        // must be used.  Look for the first non-local parameter to tell.
        //
        REBVAL *param = FUNC_PARAMS_HEAD(VAL_FUNC(value));
        while (
            NOT_END(param)
            && (VAL_PARAM_CLASS(param) == PARAM_CLASS_PURE_LOCAL)
        ) {
            ++param;
        }
        if (NOT_END(param))
            fail (Error(RE_USE_EVAL_FOR_EVAL));

        if (DO_VALUE_THROWS(D_OUT, value))
            return R_OUT_IS_THROWN;
        return R_OUT;
    }

    case REB_FRAME: {
        //
        // To allow efficient applications, this does not make a copy of the
        // FRAME!.  However it goes through the argument traversal in order
        // to do type checking.
        //
        // !!! Check needed to not run an already running frame!  User should
        // be told to copy the frame if they try.
        //
        // Right now all stack based contexts are either running (in which
        // case you shouldn't run them again) or expired (in which case their
        // values are unavailable).  It may come to pass that an interesting
        // trick lets you reuse a stack context and unwind it as a kind of
        // GOTO to reuse it, but that would be deep voodoo.  Just handle the
        // kind of frames that come in as "objects plus function the object
        // is for" flavor.
        //
        assert(!GET_ARR_FLAG(
            CTX_VARLIST(VAL_CONTEXT(value)), CONTEXT_FLAG_STACK)
        );

        struct Reb_Frame frame;
        struct Reb_Frame *f = &frame;

        // Apply_Frame_Core sets up most of the Reb_Frame, but expects these
        // arguments to be filled in.
        //
        f->out = D_OUT;
        f->func = CTX_FRAME_FUNC(VAL_CONTEXT(value));
        f->exit_from = NULL;

        f->data.varlist = CTX_VARLIST(VAL_CONTEXT(value)); // need w/NULL def

        return Apply_Frame_Core(f, SYM___ANONYMOUS__, NULL);
    }

    case REB_TASK:
        Do_Task(value);
        *D_OUT = *value;
        return R_OUT;
    }

    // Note: it is not possible to write a wrapper function in Rebol
    // which can do what EVAL can do for types that consume arguments
    // (like SET-WORD!, SET-PATH! and FUNCTION!).  DO used to do this for
    // functions only, EVAL generalizes it.
    //
    fail (Error(RE_USE_EVAL_FOR_EVAL));
}


//
//  eval: native [
//  
//  {(Special) Process received value *inline* as the evaluator loop would.}
//  
//      value [<opt> any-value!]
//          {BLOCK! passes-thru, FUNCTION! runs, SET-WORD! assigns...}
//      args [[<opt> any-value!]]
//          {Variable number of args required as evaluation's parameters}
//      /only
//          {Suppress evaluation on any ensuing arguments value consumes}
//      :quoted [[any-value!]]
//          {Variadic feed used to acquire quoted arguments (if needed)}
//  ]
//
REBNATIVE(eval)
{
    // There should not be any way to call this actual function, because it
    // will be intercepted by recognizing its identity in the evaluator loop
    // itself (required to do the "magic")
    //
    fail (Error(RE_MISC));
}


//
//  variadic?: native [
//
//  {Returns TRUE if a function may take a variable number of arguments.}
//
//      func [function!]
//  ]
//
REBNATIVE(variadic_q)
{
    PARAM(1, func);

    REBVAL *param = VAL_FUNC_PARAMS_HEAD(ARG(func));
    for (; NOT_END(param); ++param) {
        if (GET_VAL_FLAG(param, TYPESET_FLAG_VARIADIC))
            return R_TRUE;
    }

    return R_FALSE;
}


//
//  exit: native [
//  
//  {Leave enclosing function, or jump /FROM.}
//  
//      /with
//          "Result for enclosing state (default is no value)"
//      value [<opt> any-value!]
//      /from
//          "Jump the stack to return from a specific frame or call"
//      level [frame! function! integer!]
//          "Frame, function, or stack index to exit from"
//  ]
//
REBNATIVE(exit)
//
// EXIT is implemented via a THROWN() value that bubbles up through
// the stack.
//
// !!! Allowing to pass an INTEGER! to exit from a function based on its
// BACKTRACE number is a bit low-level, and perhaps should be restricted to
// a debugging mode (though it is a useful tool in "code golf").
{
    REFINE(1, with);
    PARAM(2, value);
    REFINE(3, from);
    PARAM(4, level);

    REBVAL *level = ARG(level);

#if !defined(NDEBUG)
    //
    // Though the Ren-C default is to allow exiting from natives (and not
    // to provide the poor invariant of different behavior based on whether
    // the containing function is native or not), the legacy switch lets
    // EXIT skip consideration of non-FUNCTIONs.
    //
    if (LEGACY(OPTIONS_DONT_EXIT_NATIVES)) {
        struct Reb_Frame *frame = frame_->prior;

        while (
            frame != NULL
            && FUNC_CLASS(frame->func) != FUNC_CLASS_USER
        ) {
            frame = frame->prior;
        }

        if (frame == NULL)
            fail (Error(RE_INVALID_EXIT));

        *D_OUT = *FUNC_VALUE(frame->func);

        CONVERT_NAME_TO_EXIT_THROWN(
            D_OUT, REF(with) ? ARG(value) : VOID_CELL
        );

        return R_OUT_IS_THROWN;
    }
#endif

    if (IS_VOID(level)) {
        //
        // The thrown exit protocol understands integers to be a count down
        // of how many frames to skip.  If no /FROM argument is provided
        // that means they want to exit from the function that called exit,
        // so the protocol here is to use a count of 2 (that way exit does
        // not exit itself...)
        //
        SET_INTEGER(D_OUT, 2);
    }
    else if (IS_INTEGER(level)) {
        //
        // Pursuant to the above, if we get an integer we want to bump it
        // up by 1 from what the user asked for, in order to account for
        // EXIT's exit from itself.
        //
        // (Note that if a refinement like /WITH is used it is possible to
        // wind up in a debug stack during argument fulfillment to an exit,
        // so you may actually have reason to "EXIT from an EXIT"...so better
        // to use the count than to have a rule like "EXIT rejects EXITs")
        //
        if (VAL_INT32(level) < 0)
            fail (Error(RE_INVALID_EXIT));

        SET_INTEGER(D_OUT, VAL_INT32(level) + 1);
    }
    else {
        assert(IS_FRAME(level) || IS_FUNCTION(level));

        *D_OUT = *level;
    }

    CONVERT_NAME_TO_EXIT_THROWN(D_OUT, REF(with) ? ARG(value) : VOID_CELL);

    return R_OUT_IS_THROWN;
}


//
//  fail: native [
//  
//  {Interrupts execution by reporting an error (a TRAP can intercept it).}
//  
//      reason [error! string! block!] 
//      "ERROR! value, message string, or failure spec"
//  ]
//
REBNATIVE(fail)
{
    REBVAL * const reason = D_ARG(1);

    if (IS_ERROR(reason)) {
        fail (VAL_CONTEXT(reason));
    }
    else if (IS_STRING(reason) || IS_BLOCK(reason)) {
        // Ultimately we'd like FAIL to use some clever error-creating
        // dialect when passed a block, maybe something like:
        //
        //     fail [<invalid-key> {The key} key-name: key {is invalid}]
        //
        // That could provide an error ID, the format message, and the
        // values to plug into the slots to make the message...which could
        // be extracted from the error if captured (e.g. error/id and
        // `error/key-name`.  Another option would be something like:
        //
        //     fail/with [{The key} :key-name {is invalid}] [key-name: key]
        //
        if (IS_BLOCK(reason)) {
            REBVAL *item = VAL_ARRAY_AT(reason);

            REBVAL pending_delimiter;

            REB_MOLD mo;
            CLEARS(&mo);

            SET_END(&pending_delimiter);

            // Check to make sure we're only drawing from the limited types
            // we accept (reserving room for future dialect expansion)
            //
            for (; NOT_END(item); item++) {
                if (IS_STRING(item) || IS_SCALAR(item))
                    continue;

                // Leave the group in and let the reduce take care of it
                //
                if (IS_GROUP(item))
                    continue;

                // Literal blocks in the spec given to Format used by PRINT
                // has special meaning for BLOCK! (and BAR! when not used
                // in the middle of an expression)
                //
                if (IS_BLOCK(item) || IS_BAR(item))
                    continue;

                // Leave words in to be handled by the reduce step as long
                // as they don't look up to functions.
                //
                // !!! This keeps the option open of being able to know that
                // strings that appear in the block appear in the error
                // message so it can be templated.
                //
                if (IS_WORD(item) || IS_GET_WORD(item)) {
                    const REBVAL *var = TRY_GET_OPT_VAR(item);
                    if (!var || !IS_FUNCTION(var))
                        continue;
                }

                // The only way to tell if a path resolves to a function
                // or not is to actually evaluate it, and we are delegating
                // to Reduce_Block ATM.  For now we force you to use a GROUP!
                //
                //     fail [{Erroring on} (the/safe/side) {for now.}]
                //
                fail (Error(RE_LIMITED_FAIL_INPUT));
            }

            // Use the same logic that PRINT does, which will create newline
            // at expression barriers and form literal blocks with no spaces

            Push_Mold(&mo);
            if (Format_GC_Safe_Value_Throws(
                D_OUT,
                &mo,
                &pending_delimiter, // variable shared by recursions
                reason,
                TRUE, // reduce
                ROOT_DEFAULT_PRINT_DELIMITER, // same as PRINT (customizable?)
                0 // depth
            )) {
                return R_OUT_IS_THROWN;
            }

            Val_Init_String(reason, Pop_Molded_String(&mo));
        }

        if (Make_Error_Object_Throws(D_OUT, reason)) {
            // Throw name is in D_OUT, thrown value is held task local
            return R_OUT_IS_THROWN;
        }

        fail (VAL_CONTEXT(D_OUT));
    }

    DEAD_END;
}


static REB_R If_Unless_Core(struct Reb_Frame *frame_, REBOOL trigger) {
    PARAM(1, condition);
    PARAM(2, branch);
    REFINE(3, only);
    REFINE(4, q); // actually "?" - return TRUE if branch taken, else FALSE

    assert((trigger == TRUE) || (trigger == FALSE));

    if (IS_CONDITIONAL_TRUE(ARG(condition)) == trigger) {
        if (REF(only) || !IS_BLOCK(ARG(branch))) {
            if (!REF(q)) {
                *D_OUT = *ARG(branch);
                return R_OUT;
            }
            return R_TRUE;
        }

        if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(branch)))
            return R_OUT_IS_THROWN;

        if (!REF(q))
            return R_OUT;

        return R_TRUE;
    }

    if (!REF(q)) {
        SET_VOID_UNLESS_LEGACY_NONE(D_OUT);
        return R_OUT;
    }

    return R_FALSE;
}


//
//  if: native [
//  
//  {If TRUE? condition, return branch value; evaluate blocks by default.}
//  
//      condition
//      branch ; [<opt> any-value!]
//      /only
//          "Return block branches literally instead of evaluating them."
//      /?
//          "Instead of branch result, return LOGIC! of if branch was taken"
//  ]
//
REBNATIVE(if)
{
    return If_Unless_Core(frame_, TRUE);
}


//
//  unless: native [
//
//  {If FALSE? condition, return branch value; evaluate blocks by default.}
//
//      condition
//      branch ; [<opt> any-value!]
//      /only
//          "Return block branches literally instead of evaluating them."
//      /?
//          "Instead of branch result, return TRUE? if branch was taken"
//  ]
//
REBNATIVE(unless)
{
    return If_Unless_Core(frame_, FALSE);
}


//
//  either: native [
//
//  {If TRUE condition? first branch, else second; evaluate blocks by default.}
//
//      condition
//      true-branch [<opt> any-value!]
//      false-branch [<opt> any-value!]
//      /only "Return block arg instead of evaluating it."
//  ]
//
REBNATIVE(either)
{
    PARAM(1, condition);
    PARAM(2, true_branch);
    PARAM(3, false_branch);
    REFINE(4, only);

    if (IS_CONDITIONAL_TRUE(ARG(condition))) {
        if (REF(only) || !IS_BLOCK(ARG(true_branch))) {
            *D_OUT = *ARG(true_branch);
        }
        else if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(true_branch)))
            return R_OUT_IS_THROWN;
    }
    else {
        if (REF(only) || !IS_BLOCK(ARG(false_branch))) {
            *D_OUT = *ARG(false_branch);
        }
        else if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(false_branch)))
            return R_OUT_IS_THROWN;
    }

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


//
//  return: native [
//  
//  "Returns a value from a function."
//  
//      value [<opt> any-value!]
//  ]
//
REBNATIVE(return)
//
// There is a RETURN native defined, and its native function spec is
// utilized to create the appropriate help and calling protocol
// information for values that have overridden its VAL_FUNC_CODE
// slot with a VAL_FUNC_EXIT_FROM spec.
// 
// However: this native is unset and its actual code body should
// never be able to be called.  The non-definitional return construct
// that people should use if they need it would be EXIT and EXIT/WITH
{
    panic (Error(RE_MISC));
}


//
//  leave: native [
//
//  "Leaves a procedure, giving no result to the caller."
//
//  ]
//
REBNATIVE(leave)
//
// See notes on REBNATIVE(return)
{
    panic (Error(RE_MISC));
}


//
//  switch: native [
//  
//  {Selects a choice and evaluates the block that follows it.}
//  
//      value
//          "Target value"
//      cases [block!]
//          "Block of cases to check"
//      /default
//          "Default case if no others found"
//      case
//          "Block to execute (or value to return)"
//      /all
//          "Evaluate all matches (not just first one)"
//      /strict
//          {Use STRICT-EQUAL? when comparing cases instead of EQUAL?}
//      /?
//          "Instead of last case result, return LOGIC! of if any case matched"
//  ]
//
REBNATIVE(switch)
{
    PARAM(1, value);
    PARAM(2, cases);
    REFINE(3, default);
    PARAM(4, case);
    REFINE(5, all);
    REFINE(6, strict);
    REFINE(7, q);

    REBVAL *value = ARG(value);
    REBVAL *cases = ARG(cases);
    // has_default implied by default_case not being blank
    REBVAL *default_case = ARG(case);
    REBOOL all = REF(all);
    REBOOL strict = REF(strict);

    REBOOL found = FALSE;

    REBVAL *item = VAL_ARRAY_AT(cases);

    SET_VOID_UNLESS_LEGACY_NONE(D_OUT); // default return if no cases run

    for (; NOT_END(item); item++) {

        // The way SWITCH works with blocks is that blocks are considered
        // bodies to match for other value types, so you can't use them
        // as case keys themselves.  They'll be skipped until we find
        // a non-block case we want to match.

        if (IS_BLOCK(item)) {
            // Each time we see a block that we don't take, we reset
            // the output to void...because we only leak evaluations
            // out the bottom of the switch if no block would catch it

            SET_VOID_UNLESS_LEGACY_NONE(D_OUT);
            continue;
        }

        // GET-WORD!, GET-PATH!, and GROUP! are evaluated (an escaping
        // mechanism as in lit-quotes of function specs to avoid quoting)
        // You can still evaluate to one of these, e.g. `(quote :foo)` to
        // use parens to produce a GET-WORD! to test against.

        if (IS_GROUP(item) || IS_GET_WORD(item) || IS_GET_PATH(item)) {

        #if !defined(NDEBUG)
            //
            // Note: Mezzanine can no longer support a non-eval'ing switch.
            // Guide usage of the flag by currently running function *only*.
            //
            if (LEGACY_RUNNING(OPTIONS_NO_SWITCH_EVALS)) {
                *D_OUT = *item;
                goto compare_values;
            }
        #endif

            if (DO_VALUE_THROWS(D_OUT, item))
                return R_OUT_IS_THROWN;
        }
        else {
            // Even if we're just using the item literally, we need to copy
            // it from the block the user loaned us...because the type
            // coercion in Compare_Modify_Values could mutate it.

            *D_OUT = *item;
        }

    #if !defined(NDEBUG)
    compare_values: // only used by LEGACY(OPTIONS_NO_SWITCH_EVALS)
    #endif

        // It's okay that we are letting the comparison change `value`
        // here, because equality is supposed to be transitive.  So if it
        // changes 0.01 to 1% in order to compare it, anything 0.01 would
        // have compared equal to so will 1%.  (That's the idea, anyway,
        // required for `a = b` and `b = c` to properly imply `a = c`.)

        if (!Compare_Modify_Values(value, D_OUT, strict ? 1 : 0))
            continue;

        // Skip ahead to try and find a block, to treat as code

        while (!IS_BLOCK(item)) {
            if (IS_END(item)) break;
            item++;
        }

        found = TRUE;

        if (DO_VAL_ARRAY_AT_THROWS(D_OUT, item))
            return R_OUT_IS_THROWN;

        // Only keep processing if the /ALL refinement was specified

        if (!all) {
            if (REF(q)) return R_TRUE; // accepted a case for /?

            return R_OUT;
        }
    }

    if (!found && IS_BLOCK(default_case)) {
        if (DO_VAL_ARRAY_AT_THROWS(D_OUT, default_case))
            return R_OUT_IS_THROWN;

        if (REF(q)) return R_FALSE; // running a default doesn't count for /?

        return R_OUT;
    }

    #if !defined(NDEBUG)
        //
        // R3-Alpha made `switch 1 [1]` NONE!, in Ren-C this is 1 (and useful
        // especially with evaluated items like GROUP! for a fallthrough
        // syntax alternative to `switch/default`.)  Mezzanine relies on this
        // now, so only use the legacy behavior if the currently running
        // function is "legacy" marked.  It's not perfect, see notes.
        //
        if (LEGACY_RUNNING(OPTIONS_NO_SWITCH_FALLTHROUGH))
            return R_BLANK;
    #endif

    if (REF(q))
        return found ? R_TRUE : R_FALSE;

    return R_OUT;
}


//
//  trap: native [
//  
//  {Tries to DO a block, trapping error as return value (if one is raised).}
//  
//      block [block!]
//      /with
//          "Handle error case with code"
//      handler [block! function!]
//          "If FUNCTION!, spec allows [error [error!]]"
//      /?
//         "Instead of result or error, return LOGIC! of if a trap occurred"
//  ]
//
REBNATIVE(trap)
{
    PARAM(1, block);
    REFINE(2, with);
    PARAM(3, handler);
    REFINE(4, q);

    struct Reb_State state;
    REBCTX *error;

    PUSH_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) {
        if (REF(with)) {
            REBVAL *handler = ARG(handler);

            if (IS_BLOCK(handler)) {
                // There's no way to pass 'error' to a block (so just DO it)
                if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(handler)))
                    return R_OUT_IS_THROWN;

                if (REF(q)) return R_TRUE;

                return R_OUT;
            }
            else if (IS_FUNCTION(handler)) {

                if (
                    (VAL_FUNC_NUM_PARAMS(handler) == 0)
                    || IS_REFINEMENT(VAL_FUNC_PARAM(handler, 1))
                ) {
                    // Arity zero handlers (or handlers whose first
                    // parameter is a refinement) we call without the ERROR!
                    //
                    if (Apply_Only_Throws(D_OUT, handler, END_CELL))
                        return R_OUT_IS_THROWN;
                }
                else {
                    REBVAL arg;
                    Val_Init_Error(&arg, error);

                    // If the handler takes at least one parameter that
                    // isn't a refinement, try passing it the ERROR! we
                    // trapped.  Apply will do argument checking.
                    //
                    if (Apply_Only_Throws(D_OUT, handler, &arg, END_CELL))
                        return R_OUT_IS_THROWN;
                }

                if (REF(q)) return R_TRUE;

                return R_OUT;
            }

            panic (Error(RE_MISC)); // should not be possible (type-checking)
        }

        if (REF(q)) return R_TRUE;

        Val_Init_Error(D_OUT, error);
        return R_OUT;
    }

    if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(block))) {
        // Note that we are interested in when errors are raised, which
        // causes a tricky C longjmp() to the code above.  Yet a THROW
        // is different from that, and offers an opportunity to each
        // DO'ing stack level along the way to CATCH the thrown value
        // (with no need for something like the PUSH_TRAP above).
        //
        // We're being given that opportunity here, but doing nothing
        // and just returning the THROWN thing for other stack levels
        // to look at.  For the construct which does let you catch a
        // throw, see REBNATIVE(catch), which has code for this case.

        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
        return R_OUT_IS_THROWN;
    }

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    if (REF(q)) return R_FALSE;

    return R_OUT;
}
