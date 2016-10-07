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
static int Protect(REBFRM *frame_, REBFLGS flags)
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
//  also: native [
//
//  {Returns the first value, but also evaluates the second.}
//
//      return: [<opt> any-value!]
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
//  {Short-circuiting variant of AND, using a block of expressions as input.}
//
//      return: [any-value!]
//          {Product of last evaluation if all TRUE?, else a BLANK! value.}
//      block [block!]
//          "Block of expressions.  Void evaluations are ignored."
//  ]
//
REBNATIVE(all)
{
    PARAM(1, block);

    Reb_Enumerator e;
    PUSH_SAFE_ENUMERATOR(&e, ARG(block)); // DO-ing code could disrupt `block`

    if (IS_END(e.value)) { // `all []` is considered TRUE
        DROP_SAFE_ENUMERATOR(&e);
        return R_TRUE;
    }

    do {
        DO_NEXT_REFETCH_MAY_THROW(D_OUT, &e, DO_FLAG_NORMAL);
        if (THROWN(D_OUT)) {
            DROP_SAFE_ENUMERATOR(&e);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_CONDITIONAL_FALSE(D_OUT)) { // a failed ALL returns BLANK!
            DROP_SAFE_ENUMERATOR(&e);
            return R_BLANK;
        }
    } while (NOT_END(e.value));

    // Note: Though ALL wants to use as a "truthy" value whatever the last
    // evaluation was, with `all [1 2 ()]`...the 2 is already gone.  There
    // would be overhead trying to preserve it.  Considering that `all []`
    // has to pull a TRUE out of thin air anyway, it is accepted.

    if (IS_VOID(D_OUT))
        SET_TRUE(D_OUT);

    DROP_SAFE_ENUMERATOR(&e);
    return R_OUT;
}


//
//  any: native [
//  
//  {Short-circuiting version of OR, using a block of expressions as input.}
//  
//      return: [any-value!]
//          {The first TRUE? evaluative result, or BLANK! value if all FALSE?}
//      block [block!]
//          "Block of expressions.  Void evaluations are ignored."
//  ]
//
REBNATIVE(any)
{
    PARAM(1, block);

    Reb_Enumerator e;
    PUSH_SAFE_ENUMERATOR(&e, ARG(block)); // DO-ing code could disrupt `block`

    if (IS_END(e.value)) { // `any []` is a failure case, returns BLANK!
        DROP_SAFE_ENUMERATOR(&e);
        return R_BLANK;
    }

    // Note: although `all []` is TRUE, `any []` is BLANK!.  This sides with
    // general usage as "Were all of these things not false?" as opposed to
    // "Were any of these things true?".  Also, `FALSE OR X OR Y` shows it
    // as the "unit" for OR, matching `TRUE AND X AND Y` as the seed that
    // doesn't affect the outcome of the chain.

    do {
        DO_NEXT_REFETCH_MAY_THROW(D_OUT, &e, DO_FLAG_NORMAL);
        if (THROWN(D_OUT)) {
            DROP_SAFE_ENUMERATOR(&e);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_CONDITIONAL_TRUE(D_OUT)) { // successful ANY returns the value
            DROP_SAFE_ENUMERATOR(&e);
            return R_OUT;
        }
    } while (NOT_END(e.value));

    DROP_SAFE_ENUMERATOR(&e);
    return R_BLANK;
}


//
//  none: native [
//
//  {Short circuiting version of NOR, using a block of expressions as input.}
//
//      return: [logic! blank!]
//          {TRUE if all expressions are FALSE?, or BLANK if any are TRUE?}
//      block [block!]
//          "Block of expressions.  Void evaluations are ignored."
//  ]
//
REBNATIVE(none)
//
// !!! In order to reduce confusion and accidents in the near term, the
// %mezz-legacy.r renames this to NONE-OF and makes NONE report an error.
{
    PARAM(1, block);

    Reb_Enumerator e;
    PUSH_SAFE_ENUMERATOR(&e, ARG(block)); // DO-ing code could disrupt `block`

    if (IS_END(e.value)) // `none []` is a success case, returns TRUE
        return R_TRUE;

    do {
        DO_NEXT_REFETCH_MAY_THROW(D_OUT, &e, DO_FLAG_NORMAL);
        if (THROWN(D_OUT)) {
            DROP_SAFE_ENUMERATOR(&e);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_CONDITIONAL_TRUE(D_OUT)) { // successful ANY returns the value
            DROP_SAFE_ENUMERATOR(&e);
            return R_BLANK;
        }
    } while (NOT_END(e.value));

    DROP_SAFE_ENUMERATOR(&e);
    return R_TRUE;
}


//
//  attempt: native [
//  
//  {Tries to evaluate a block and returns result or NONE on error.}
//
//      return: [<opt> any-value!]
//      block [block!]
//  ]
//
REBNATIVE(attempt)
{
    REBVAL *block = D_ARG(1);

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

    *D_OUT = *NAT_VALUE(break);

    CONVERT_NAME_TO_THROWN(D_OUT, REF(with) ? ARG(value) : VOID_CELL);

    return R_OUT_IS_THROWN;
}


//
//  case: native [
//  
//  {Evaluates each condition, and when true, evaluates what follows it.}
//
//      return: [<opt> any-value!]
//          {Void if no cases matched, or last case evaluation (may be void)}
//      block [block!]
//          "Block of cases (conditions followed by values)"
//      /all
//          {Evaluate all cases (do not stop at first TRUE? case)}
//      /only
//          {Do not evaluate block or function branches, return as-is}
//      /?
//          "Instead of last case result, return LOGIC! of if any cases ran"
//  ]
//
REBNATIVE(case)
{
    PARAM(1, block); // overwritten as scratch space after enumerator init
    REFINE(2, all);
    REFINE(3, only);
    REFINE(4, q);

    Reb_Enumerator e;
    PUSH_SAFE_ENUMERATOR(&e, ARG(block)); // DO-ing cases could disrupt `block`

    while (NOT_END(e.value)) {
        UPDATE_EXPRESSION_START(&e); // informs the error delivery better

        if (IS_BAR(e.value)) { // interstitial BAR! legal, `case [1 2 | 3 4]`
            FETCH_NEXT_ONLY_MAYBE_END(&e);
            continue;
        }

        // Perform a DO/NEXT's worth of evaluation on a "condition" to test

        DO_NEXT_REFETCH_MAY_THROW(D_CELL, &e, DO_FLAG_NORMAL);
        if (THROWN(D_CELL)) {
            *D_OUT = *D_CELL;
            goto return_thrown;
        }

        if (IS_VOID(D_CELL)) // no void conditions allowed (as with IF)
            fail (Error(RE_NO_RETURN));

        if (IS_END(e.value)) // require conditions and branches in pairs
            fail (Error(RE_PAST_END));

        if (IS_BAR(e.value)) // BAR! out of sync, between condition and branch
            fail (Error(RE_BAR_HIT_MID_CASE));

        // Regardless of whether a "condition" was true or false, it's
        // necessary to evaluate the next "branch" to know how far to skip:
        //
        //     condition: true
        //     case [condition 10 + 20 true {hello}] ;-- returns 30
        //
        //     condition: false
        //     case [condition 10 + 20 true {hello}] ;-- returns {hello}
        //
        // This uses the safe form, so you can't say `case [[x] [y]]` because
        // the [x] condition is a literal block.  However you can say
        // `foo: [x] | case [foo [y]]`, since it is evaluated, or use a
        // GROUP! as in `case [([x]) [y]]`.
        //
        if (NOT(IS_CONDITIONAL_TRUE_SAFE(D_CELL))) {
            DO_NEXT_REFETCH_MAY_THROW(D_CELL, &e, DO_FLAG_NORMAL);
            if (THROWN(D_CELL)) {
                *D_OUT = *D_CELL;
                goto return_thrown;
            }

            // Should the slot contain a single arity function taking a logic
            // (and this not be an /ONLY), then it's treated as a brancher.
            // It is told it failed the test, and may choose to perform some
            // action in a response...but the result is discarded.
            //
            // Its result is put in the block argument cell, whose contents
            // and index have been extracted already and aren't used further.
            // (this might confuse debuggers, but if that's going to be
            // considered a problem then every native has to be reviewed,
            // as this is a common space-saving tactic)
            //
            if (Maybe_Run_Failed_Branch_Throws(ARG(block), D_CELL, REF(only))) {
                *D_OUT = *ARG(block);
                goto return_thrown;
            }

            continue;
        }

        // When the condition is TRUE?, CASE actually does a double evaluation
        // if a block is yielded as the branch:
        //
        //     stuff: [print "This will be printed"]
        //     case [true stuff]
        //
        // Similar to IF TRUE STUFF, so CASE can act like many IFs at once.

        DO_NEXT_REFETCH_MAY_THROW(D_CELL, &e, DO_FLAG_NORMAL);
        if (THROWN(D_CELL)) {
            *D_OUT = *D_CELL;
            goto return_thrown;
        }

        // !!! Optimization note: if the previous evaluation had gone into
        // D_OUT directly it could just stay there in some cases; and even
        // block evaluation doesn't need the copy.  Review how this shared
        // code might get more efficient if the data were already in D_OUT.
        //
        if (Run_Success_Branch_Throws(D_OUT, D_CELL, REF(only)))
            goto return_thrown;

        if (NOT(REF(all))) goto return_matched;

        // keep matching if /ALL
    }

//return_maybe_matched:
    DROP_SAFE_ENUMERATOR(&e);
    return R_OUT_Q(REF(q)); // if /ran?, detect if D_OUT was written to

return_matched:
    DROP_SAFE_ENUMERATOR(&e);
    if (REF(q)) return R_TRUE; // /ran? gets TRUE if at least one case ran
    return R_OUT;

return_thrown:
    DROP_SAFE_ENUMERATOR(&e);
    return R_OUT_IS_THROWN;
}



//
//  catch: native [
//  
//  {Catches a throw from a block and returns its value.}
//
//      return: [<opt> any-value!]
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
                && (!IS_FUNCTION(D_OUT) || VAL_FUNC_DISPATCHER(D_OUT) != &N_quit)
            )
            || (
                REF(quit)
                && (IS_FUNCTION(D_OUT) && VAL_FUNC_DISPATCHER(D_OUT) == &N_quit)
            )
        ) {
            goto was_caught;
        }

        if (REF(name)) {
            //
            // We use equal? by way of Compare_Modify_Values, and re-use the
            // refinement slots for the mutable space

            REBVAL *temp1 = ARG(quit);
            REBVAL *temp2 = ARG(any);

            // !!! The reason we're copying isn't so the VALUE_FLAG_THROWN bit
            // won't confuse the equality comparison...but would it have?

            if (IS_BLOCK(ARG(names))) {
                //
                // Test all the words in the block for a match to catch

                RELVAL *candidate = VAL_ARRAY_AT(ARG(names));
                for (; NOT_END(candidate); candidate++) {
                    //
                    // !!! Should we test a typeset for illegal name types?
                    //
                    if (IS_BLOCK(candidate))
                        fail (Error(RE_INVALID_ARG, ARG(names)));

                    COPY_VALUE(temp1, candidate, VAL_SPECIFIER(ARG(names)));
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
            // This calls the function but only does a DO/NEXT.  Hence the
            // function might be arity 0, arity 1, or arity 2.  If it has
            // greater arity it will process more arguments.
            //
            if (Apply_Only_Throws(
                D_OUT,
                FALSE, // do not alert if handler doesn't consume all args
                handler,
                thrown_arg,
                thrown_name,
                END_CELL)
            ) {
                return R_OUT_IS_THROWN;
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
//      value [<opt> any-value!]
//          "Value returned from catch"
//      /name
//          "Throws to a named catch"
//      name-value [word! function! object!]
//  ]
//
REBNATIVE(throw)
//
// Choices are currently limited for what one can use as a "name" of a THROW.
// Note blocks as names would conflict with the `name_list` feature in CATCH.
//
// !!! Should parameters be /NAMED and NAME ?
{
    PARAM(1, value);
    REFINE(2, name);
    PARAM(3, name_value);

    REBVAL *value = ARG(value);

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

    if (REF(name))
        *D_OUT = *ARG(name_value);
    else {
        // Blank values serve as representative of THROWN() means "no name"
        //
        SET_BLANK(D_OUT);
    }

    CONVERT_NAME_TO_THROWN(D_OUT, value);
    return R_OUT_IS_THROWN;
}


//
//  comment: native [
//
//  {Ignores the argument value.}
//
//      return: [<opt>]
//          {Nothing.}
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

    *D_OUT = *NAT_VALUE(continue);

    CONVERT_NAME_TO_THROWN(D_OUT, REF(with) ? ARG(value) : VOID_CELL);

    return R_OUT_IS_THROWN;
}


//
//  do: native [
//  
//  {Evaluates a block of source code (directly or fetched according to type)}
//
//      return: [<opt> any-value!]
//      source [
//          <opt> ;-- should DO accept an optional argument (chaining?)
//          blank! ;-- same question... necessary, or not?
//          block! ;-- source code in block form
//          string! ;-- source code in text form
//          binary! ;-- treated as UTF-8
//          url! ;-- load code from URL via protocol
//          file! ;-- load code from file on local disk
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
//      var [any-word! blank!]
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
    case REB_MAX_VOID:
        // useful for `do if ...` types of scenarios
        return R_VOID;

    case REB_BLANK:
        // useful for `do all ...` types of scenarios
        return R_BLANK;

    case REB_BLOCK:
    case REB_GROUP:
        if (REF(next)) {
            REBIXO indexor = VAL_INDEX(value);

            indexor = DO_NEXT_MAY_THROW(
                D_OUT,
                VAL_ARRAY(value),
                indexor,
                VAL_SPECIFIER(value)
            );

            if (indexor == THROWN_FLAG) {
                // the throw should make the value irrelevant, but if caught
                // then have it indicate the start of the thrown expression

                // !!! What if the block was mutated, and D_ARG(1) is no
                // longer actually the expression that started the throw?

                if (!IS_BLANK(ARG(var))) {
                    *GET_MUTABLE_VAR_MAY_FAIL(ARG(var), SPECIFIED)
                        = *value;
                }

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

                *GET_MUTABLE_VAR_MAY_FAIL(ARG(var), SPECIFIED)
                    = *ARG(value);
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
            TRUE, // error if not all arguments consumed
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
            && (VAL_PARAM_CLASS(param) == PARAM_CLASS_LOCAL)
        ) {
            ++param;
        }
        if (NOT_END(param))
            fail (Error(RE_USE_EVAL_FOR_EVAL));

        if (EVAL_VALUE_THROWS(D_OUT, value))
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

        REBFRM frame;
        REBFRM *f = &frame;

        // Apply_Frame_Core sets up most of the Reb_Frame, but expects these
        // arguments to be filled in.
        //
        f->out = D_OUT;
        f->gotten = CTX_FRAME_FUNC_VALUE(VAL_CONTEXT(value));
        f->func = VAL_FUNC(f->gotten);
        f->binding = VAL_BINDING(value);

        f->varlist = CTX_VARLIST(VAL_CONTEXT(value)); // need w/NULL def

        return Apply_Frame_Core(f, Canon(SYM___ANONYMOUS__), NULL);
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
//      /only
//          {Suppress evaluation on any ensuing arguments value consumes}
//  ]
//
REBNATIVE(eval)
{
    PARAM(1, value);
    REFINE(2, only);

    REBFRM *f = frame_; // implicit parameter to every dispatcher/native

    f->cell = *ARG(value);

    // Save the prefetched f->value for what would be the usual next
    // item (including if it was an END marker) into f->pending.
    // Then make f->value the address of the eval result.
    //
    // Since the evaluation result is a REBVAL and not a RELVAL, it
    // is specific.  This means the `f->specifier` (which can only
    // specify values from the source array) won't ever be applied
    // to it, since it only comes into play for IS_RELATIVE values.
    //
    f->pending = f->value;
    SET_FRAME_VALUE(f, &f->cell); // SPECIFIED
    f->eval_type = VAL_TYPE(f->value);

    // The f->gotten (if any) was the fetch for the f->value we just
    // put in pending...not the f->value we just set.  Not only is
    // it more expensive to hold onto that cache than to lose it,
    // but an eval can do anything...so the f->gotten might wind
    // up being completely different after the eval.  So forget it.
    //
    f->gotten = NULL;

    return REF(only) ? R_REEVALUATE_ONLY : R_REEVALUATE;
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
//  Make_Thrown_Exit_Value: C
//
// This routine will generate a THROWN() value that can be used to indicate
// a desire to exit from a particular level in the stack with a value (or void)
//
// It is used in the implementation of the EXIT native.
//
void Make_Thrown_Exit_Value(
    REBVAL *out,
    const REBVAL *level, // FRAME!, FUNCTION! (or INTEGER! relative to frame)
    const REBVAL *value,
    REBFRM *frame // only required if level is INTEGER!
) {
    *out = *NAT_VALUE(exit);

    if (IS_INTEGER(level)) {
        REBCNT count = VAL_INT32(level);
        if (count <= 0)
            fail (Error(RE_INVALID_EXIT));

        REBFRM *f = frame->prior;
        for (; TRUE; f = f->prior) {
            if (f == NULL)
                fail (Error(RE_INVALID_EXIT));

            if (NOT(Is_Any_Function_Frame(f))) continue; // only exit functions

            if (Is_Function_Frame_Fulfilling(f)) continue; // not ready to exit

        #if !defined(NDEBUG)
            if (LEGACY(OPTIONS_DONT_EXIT_NATIVES))
                if (NOT(IS_FUNCTION_PLAIN(FUNC_VALUE(f->func))))
                    continue; // R3-Alpha would exit the first user function
        #endif

            --count;

            if (count == 0) {
                //
                // We want the integer-based exits to identify frames uniquely.
                // Without a context varlist, a frame can't be unique.
                //
                Context_For_Frame_May_Reify_Managed(f);
                assert(f->varlist);
                out->extra.binding = f->varlist;
                break;
            }
        }
    }
    else if (IS_FRAME(level)) {
        out->extra.binding = CTX_VARLIST(VAL_CONTEXT(level));
    }
    else {
        assert(IS_FUNCTION(level));
        out->extra.binding = VAL_FUNC_PARAMLIST(level);
    }

    CONVERT_NAME_TO_THROWN(out, value);
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
// EXIT is implemented via a THROWN() value that bubbles up through the stack.
// Using EXIT's function REBVAL with a target `binding` field is the
// protocol understood by Do_Core to catch a throw itself.
//
// !!! Allowing to pass an INTEGER! to exit from a function based on its
// BACKTRACE number is a bit low-level, and perhaps should be restricted to
// a debugging mode (though it is a useful tool in "code golf").
{
    REFINE(1, with);
    PARAM(2, value);
    REFINE(3, from);
    PARAM(4, level);

    if (NOT(REF(from)))
        SET_INTEGER(ARG(level), 1); // default--exit one function stack level

    assert(REF(with) || IS_VOID(ARG(value)));

    Make_Thrown_Exit_Value(D_OUT, ARG(level), ARG(value), frame_);

    return R_OUT_IS_THROWN;
}


//
//  fail: native [
//  
//  {Interrupts execution by reporting an error (a TRAP can intercept it).}
//  
//      reason [error! string! block!] 
//          "ERROR! value, message string, or failure spec"
//      /where
//          "Specify an originating location other than the FAIL itself"
//      location [frame! any-word!]
//          "Frame or parameter at which to indicate the error originated"
//  ]
//
REBNATIVE(fail)
{
    PARAM(1, reason);
    REFINE(2, where);
    PARAM(3, location);

    REBVAL *reason = ARG(reason);

    if (IS_ERROR(reason))
        fail (VAL_CONTEXT(reason)); // if argument is an error, trigger as-is

    if (IS_BLOCK(reason)) {
        //
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

        RELVAL *item = VAL_ARRAY_AT(reason);

        REBVAL pending_delimiter;
        SET_END(&pending_delimiter);

        REB_MOLD mo;
        CLEARS(&mo);

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
                const REBVAL *var
                    = TRY_GET_OPT_VAR(item, VAL_SPECIFIER(reason));

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
        if (Form_Value_Throws(
            D_OUT,
            &mo,
            &pending_delimiter, // variable shared by recursions
            reason,
            FORM_FLAG_REDUCE
                | FORM_FLAG_NEWLINE_SEQUENTIAL_STRINGS, // no newline at end
            SPACE_VALUE, // delimiter same as PRINT (customizable?)
            0 // depth
        )) {
            return R_OUT_IS_THROWN;
        }

        Val_Init_String(reason, Pop_Molded_String(&mo));
    }

    assert(IS_STRING(reason));

    REBFRM *where = NULL;
    if (REF(where)) {
        REBCTX *context;
        if (IS_WORD(ARG(location)))
            context = VAL_WORD_CONTEXT(ARG(location));
        else
            context = VAL_CONTEXT(ARG(location));
        where = CTX_FRAME(context);

        // !!! If where comes back NULL, what to do?  Probably bad if someone
        // is trying to decipher an error to trigger another error.  Maybe
        // the meta info on the error could be annotated with "tried a
        // where that was for an expired stack frame" or similar...
    }

    if (Make_Error_Object_Throws(D_OUT, reason, where)) {
        // Throw name is in D_OUT, thrown value is held task local
        return R_OUT_IS_THROWN;
    }

    fail (VAL_CONTEXT(D_OUT));
}



inline static REB_R If_Unless_Core(REBFRM *frame_, REBOOL trigger)
{
    PARAM(1, condition);
    PARAM(2, branch);
    REFINE(3, only);
    REFINE(4, q); //  return TRUE if branch taken, else FALSE

    // Test is "safe", e.g. literal blocks aren't allowed, `if [x] [...]`
    //
    if (IS_CONDITIONAL_TRUE_SAFE(ARG(condition)) != trigger) {
        //
        // Don't take the branch.
        //
        // The behavior for functions in the FALSE case is slightly tricky.
        // If the function is arity-0, it should not be run--just as a branch
        // for a block should not be run.  *but* if it's arity-1 then it runs
        // either way, and just gets passed FALSE.
        //
        // (This permits certain constructions like `if condition x else y`,
        // where `x else y` generates an infix function that takes a LOGIC!)
        //
        if (Maybe_Run_Failed_Branch_Throws(D_OUT, ARG(branch), REF(only)))
            return R_OUT_IS_THROWN;

        SET_VOID(D_OUT); // default if nothing run (and not /?)

        if (REF(q))
            return R_FALSE; // !!! Support this?  It is like having EITHER?
        return R_OUT;
    }

    if (Run_Success_Branch_Throws(D_OUT, ARG(branch), REF(only)))
        return R_OUT_IS_THROWN;

    if (REF(q))
        return R_TRUE;
    return R_OUT;
}


//
//  if: native [
//  
//  {If TRUE? condition, return branch value; evaluate blocks by default.}
//
//      return: [<opt> any-value!]
//          {Void on FALSE?, branch result if TRUE? condition (may be void)}
//      condition [any-value!]
//      branch [<opt> any-value!]
//          {Evaluated if block, 0-arity function, or arity-1 LOGIC! function}
//      /only
//          "Return block/function branches instead of evaluating them."
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
//      return: [<opt> any-value!]
//          {Void on FALSE?, branch result if TRUE? condition (may be void)}
//      condition [any-value!]
//      branch [<opt> any-value!]
//          {Evaluated if block, 0-arity function, or arity-1 LOGIC! function}
//      /only
//          "Quote block/function branches instead of evaluating them."
//      /?
//          "Instead of branch result, return TRUE? if branch was taken"
//  ]
//
REBNATIVE(unless)
{
    return If_Unless_Core(frame_, FALSE);
}


// Shared logic between EITHER and BRANCHER (enfixed as ELSE)
//
inline static REB_R Either_Core(
    REBVAL *out,
    REBVAL *condition,
    REBVAL *true_branch,
    REBVAL *false_branch,
    REBOOL only
) {
    if (IS_CONDITIONAL_TRUE_SAFE(condition)) { // SAFE means no literal blocks
        if (Run_Success_Branch_Throws(out, true_branch, only))
            return R_OUT_IS_THROWN;
    }
    else {
        if (Run_Success_Branch_Throws(out, false_branch, only))
            return R_OUT_IS_THROWN;
    }

    return R_OUT;
}


//
//  either: native [
//
//  {If TRUE condition? first branch, else second; evaluate blocks by default.}
//
//      return: [<opt> any-value!]
//      condition [any-value!]
//      true-branch [<opt> any-value!]
//      false-branch [<opt> any-value!]
//      /only
//          "Quote block/function branches instead of evaluating them."
//  ]
//
REBNATIVE(either)
{
    PARAM(1, condition);
    PARAM(2, true_branch);
    PARAM(3, false_branch);
    REFINE(4, only);

    return Either_Core(
        D_OUT,
        ARG(condition),
        ARG(true_branch),
        ARG(false_branch),
        REF(only)
    );
}


//
//  Brancher_Dispatcher: C
//
// The BRANCHER native is used by ELSE, and basically reuses the logic of the
// implementation of EITHER.
//
REB_R Brancher_Dispatcher(REBFRM *f)
{
    REBVAL *condition = FRM_ARG(f, 1);

    assert(IS_PAIR(FUNC_BODY(f->func)));

    REBVAL *true_branch = PAIRING_KEY(FUNC_BODY(f->func)->payload.pair);
    REBVAL *false_branch = FUNC_BODY(f->func)->payload.pair;

    // Note: There is no /ONLY switch.  IF cannot pass it through, because
    // running `IF/ONLY condition [foo] ELSE [bar]` would return the
    // logic-taking function that ELSE defines.  Just pass FALSE.
    //
    return Either_Core(FRM_OUT(f), condition, true_branch, false_branch, FALSE);
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
{
    PARAM(1, value);

    REBVAL *value = ARG(value);
    REBFRM *f = frame_; // implicit parameter to REBNATIVE()

    if (f->binding == NULL) // raw native, not a variant FUNCTION made
        fail (Error(RE_RETURN_ARCHETYPE));

    // The frame this RETURN is being called from may well not be the target
    // function of the return (that's why it's a "definitional return").  So
    // examine the binding.  Currently it can be either a FRAME!'s varlist or
    // a FUNCTION! paramlist.

    REBFUN *target =
        IS_FUNCTION(ARR_HEAD(f->binding))
            ? AS_FUNC(f->binding)
            : AS_FUNC(CTX_KEYLIST(AS_CONTEXT(f->binding)));

    REBVAL *typeset = FUNC_PARAM(target, FUNC_NUM_PARAMS(target));
    assert(VAL_PARAM_SYM(typeset) == SYM_RETURN);

    // Check to make sure the types match.  If it were not done here, then
    // the error would not point out the bad call...just the function that
    // wound up catching it.
    //
    if (!TYPE_CHECK(typeset, VAL_TYPE(value)))
        fail (Error_Bad_Return_Type(
            f->label, // !!! Should climb stack to get real label?
            VAL_TYPE(value)
        ));

    *D_OUT = *NAT_VALUE(exit); // see also Make_Thrown_Exit_Value
    D_OUT->extra.binding = f->binding;

    CONVERT_NAME_TO_THROWN(D_OUT, value);
    return R_OUT_IS_THROWN;
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
    if (frame_->binding == NULL) // raw native, not a variant PROCEDURE made
        fail (Error(RE_RETURN_ARCHETYPE));

    *D_OUT = *NAT_VALUE(exit); // see also Make_Thrown_Exit_Value
    D_OUT->extra.binding = frame_->binding;

    CONVERT_NAME_TO_THROWN(D_OUT, VOID_CELL);
    return R_OUT_IS_THROWN;
}


//
//  switch: native [
//  
//  {Selects a choice and evaluates the block that follows it.}
//
//      return: [<opt> any-value!]
//          {Void if no cases matched, or last case evaluation (may be void)}
//      value [any-value!]
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
    PARAM(4, default_case);
    REFINE(5, all);
    REFINE(6, strict);
    REFINE(7, q);

    Reb_Enumerator e;
    PUSH_SAFE_ENUMERATOR(&e, ARG(cases)); // DO-ing matches may disrupt `cases`

    // The evaluator always initializes the out slot to an END marker.  That
    // makes sure it gets overwritten with a value (or void) before returning.
    // But here SWITCH also lets END indicate no matching cases ran yet.

    assert(IS_END(D_OUT));

    REBVAL *value = ARG(value);

    // For safety, notice if someone wrote `switch [x] [...]` with a literal
    // block in source, as that is likely a mistake.
    //
    if (IS_BLOCK(value) && !GET_VAL_FLAG(value, VALUE_FLAG_EVALUATED))
        fail (Error(RE_BLOCK_SWITCH, value));

    // Frame's extra D_CELL is free since the function has > 1 arg.  Reuse it
    // as a temporary GC-safe location for holding evaluations.  This
    // holds the last test so that `switch 9 [1 ["a"] 2 ["b"] "c"]` is "c".

    SET_VOID(D_CELL); // used for "fallout"

    while (NOT_END(e.value)) {

        // If a block is seen at this point, it doesn't correspond to any
        // condition to match.  If no more tests are run, let it suppress the
        // feature of the last value "falling out" the bottom of the switch

        if (IS_BLOCK(e.value)) {
            SET_VOID(D_CELL);
            goto continue_loop;
        }

        // GROUP!, GET-WORD! and GET-PATH! are evaluated in Ren-C's SWITCH
        // All other types are seen as-is (hence words act "quoted")

        if (
            IS_GROUP(e.value)
            || IS_GET_WORD(e.value)
            || IS_GET_PATH(e.value)
        ) {
            if (EVAL_VALUE_CORE_THROWS(D_CELL, e.value, e.specifier)) {
                *D_OUT = *D_CELL;
                goto return_thrown;
            }
            // Note: e.value may have gone stale during DO, must REFETCH
        }
        else
            COPY_VALUE(D_CELL, e.value, e.specifier);

        // It's okay that we are letting the comparison change `value`
        // here, because equality is supposed to be transitive.  So if it
        // changes 0.01 to 1% in order to compare it, anything 0.01 would
        // have compared equal to so will 1%.  (That's the idea, anyway,
        // required for `a = b` and `b = c` to properly imply `a = c`.)
        //
        // !!! This means fallout can be modified from its intent.  Rather
        // than copy here, this is a reminder to review the mechanism by
        // which equality is determined--and why it has to mutate.

        if (!Compare_Modify_Values(ARG(value), D_CELL, REF(strict) ? 1 : 0))
            goto continue_loop;

        // Skip ahead to try and find a block, to treat as code for the match

        do {
            FETCH_NEXT_ONLY_MAYBE_END(&e);
            if (IS_END(e.value)) break;
        } while (!IS_BLOCK(e.value));

        // Run the code if it was found.  Because it writes D_OUT with a value
        // (or void), it won't be END--so we'll know at least one case has run.

        if (Do_At_Throws(
            D_OUT,
            VAL_ARRAY(e.value),
            VAL_INDEX(e.value),
            IS_RELATIVE(e.value)
                ? VAL_SPECIFIER(ARG(cases)) // if relative, use parent's...
                : VAL_SPECIFIER(const_KNOWN(e.value)) // ...else use child's
        )) {
            goto return_thrown;
        }

        // Only keep processing if the /ALL refinement was specified

        if (NOT(REF(all))) goto return_matched;

    continue_loop:
        FETCH_NEXT_ONLY_MAYBE_END(&e);
    }

    if (NOT_END(D_OUT)) // at least one case body's DO ran and overwrote D_OUT
        goto return_matched;

    if (REF(default)) {
        if (DO_VAL_ARRAY_AT_THROWS(D_OUT, ARG(default_case)))
            goto return_thrown;
    }
    else
        *D_OUT = *D_CELL; // let last test value "fall out", might be void

//return_defaulted:
    DROP_SAFE_ENUMERATOR(&e);
    if (REF(q)) return R_FALSE; // running default code doesn't count for /?
    return R_OUT;

return_matched:
    DROP_SAFE_ENUMERATOR(&e);
    if (REF(q)) return R_TRUE;
    return R_OUT;

return_thrown:
    DROP_SAFE_ENUMERATOR(&e);
    return R_OUT_IS_THROWN;
}


//
//  trap: native [
//  
//  {Tries to DO a block, trapping error as return value (if one is raised).}
//
//      return: [<opt> any-value!]
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
                REBVAL arg;
                Val_Init_Error(&arg, error);

                // Try passing the handler the ERROR! we trapped.  Passing
                // FALSE for `fully` means it will not raise an error if
                // the handler happens to be arity 0.
                //
                if (Apply_Only_Throws(D_OUT, FALSE, handler, &arg, END_CELL))
                    return R_OUT_IS_THROWN;

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
