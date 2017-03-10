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
// Copyright 2012-2017 Rebol Open Source Contributors
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
// Control constructs in Ren-C differ from R3-Alpha in some ways:
//
// * If they do not run their body, they evaluate to void ("unset!") and not
//   blank ("none!").  Otherwise the last result of the body evaluation, as
//   in R3-Alpha and Rebol2.
//
// * It is possible to ask the return result to be a LOGIC! of whether the
//   body ever ran using the /? refinement.  Specialized versions are in
//   the bootstrap (e.g. CASE/? is specialized as CASE?)
//
// * Zero-arity function values used as branches will be executed.  Single
//   arity function values used as branches will be executed and passed a
//   LOGIC! parameter of whether the branch is taken (TRUE) or if it should
//   be interpreted as untaken (FALSE).  Functions of other arities will be
//   errors if used as branches.
//
// * The /ONLY option suppresses execution of either FUNCTION! branches or
//   BLOCK! branches, instead evaluating to the raw function or block value.
//

#include "sys-core.h"


// Shared logic for IF and UNLESS (they have the same frame params layout)
//
inline static REB_R If_Unless_Core(REBFRM *frame_, REBOOL trigger)
{
    INCLUDE_PARAMS_OF_IF;  // ? is renamed as "q"

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

        if (REF(q))
            return R_FALSE; // !!! Support this?  It is like having EITHER?

        return R_OUT_VOID_IF_UNWRITTEN; // defaults void if nothing run
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
    INCLUDE_PARAMS_OF_EITHER;

    return Either_Core(
        D_OUT,
        ARG(condition),
        ARG(true_branch),
        ARG(false_branch),
        REF(only)
    );
}


//
//  all: native [
//
//  {Short-circuiting variant of AND, using a block of expressions as input.}
//
//      return: [<opt> any-value!]
//          {Product of last evaluation if all TRUE?, else a BLANK! value.}
//      block [block!]
//          "Block of expressions.  Void evaluations are ignored."
//  ]
//
REBNATIVE(all)
{
    INCLUDE_PARAMS_OF_ALL;

    assert(IS_END(D_OUT)); // guaranteed by the evaluator

    REBFRM f;
    Push_Frame(&f, ARG(block));

    while (NOT_END(f.value)) {
        Do_Next_In_Frame_May_Throw(D_CELL, &f, DO_FLAG_NORMAL);

        if (THROWN(D_CELL)) {
            Drop_Frame(&f);
            Move_Value(D_OUT, D_CELL);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_CELL)) // voids do not "vote" true or false
            continue;

        if (IS_CONDITIONAL_FALSE(D_CELL)) { // a failed ALL returns BLANK!
            Drop_Frame(&f);
            return R_BLANK;
        }

        Move_Value(D_OUT, D_CELL); // preserve (not overwritten by later voids)
    }

    Drop_Frame(&f);

    // If IS_END(out), no successes or failures found (all opt-outs)
    //
    return R_OUT_VOID_IF_UNWRITTEN;
}


//
//  any: native [
//
//  {Short-circuiting version of OR, using a block of expressions as input.}
//
//      return: [<opt> any-value!]
//          {The first TRUE? evaluative result, or BLANK! value if all FALSE?}
//      block [block!]
//          "Block of expressions.  Void evaluations are ignored."
//  ]
//
REBNATIVE(any)
{
    INCLUDE_PARAMS_OF_ANY;

    REBFRM f;
    Push_Frame(&f, ARG(block));

    REBOOL voted = FALSE;

    while (NOT_END(f.value)) {
        Do_Next_In_Frame_May_Throw(D_OUT, &f, DO_FLAG_NORMAL);
        if (THROWN(D_OUT)) {
            Drop_Frame(&f);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_CONDITIONAL_TRUE(D_OUT)) { // successful ANY returns the value
            Drop_Frame(&f);
            return R_OUT;
        }

        voted = TRUE; // signal at least one non-void result was seen
    }

    Drop_Frame(&f);

    if (voted)
        return R_BLANK;

    return R_VOID; // all opt-outs
}


//
//  none: native [
//
//  {Short circuiting version of NOR, using a block of expressions as input.}
//
//      return: [<opt> bar! blank!]
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
    INCLUDE_PARAMS_OF_NONE;

    REBFRM f;
    Push_Frame(&f, ARG(block));

    REBOOL voted = FALSE;

    while (NOT_END(f.value)) {
        Do_Next_In_Frame_May_Throw(D_OUT, &f, DO_FLAG_NORMAL);
        if (THROWN(D_OUT)) {
            Drop_Frame(&f);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_CONDITIONAL_TRUE(D_OUT)) { // any true results mean failure
            Drop_Frame(&f);
            return R_BLANK;
        }

        voted = TRUE; // signal that at least one non-void result was seen
    }

    Drop_Frame(&f);

    if (voted)
        return R_BAR;

    return R_VOID; // all opt-outs
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
    INCLUDE_PARAMS_OF_CASE; // ? is renamed as "q"

    REBFRM f;
    Push_Frame(&f, ARG(block));

    // With the block argument pushed in the enumerator, that frame slot is
    // available for scratch space in the rest of the routine.

    DECLARE_LOCAL (dummy);

    while (NOT_END(f.value)) {
        UPDATE_EXPRESSION_START(&f); // informs the error delivery better

        if (IS_BAR(f.value)) { // interstitial BAR! legal, `case [1 2 | 3 4]`
            Fetch_Next_In_Frame(&f);
            continue;
        }

        // Perform a DO/NEXT's worth of evaluation on a "condition" to test

        Do_Next_In_Frame_May_Throw(D_CELL, &f, DO_FLAG_NORMAL);
        if (THROWN(D_CELL)) {
            Move_Value(D_OUT, D_CELL);
            goto return_thrown;
        }

        if (IS_VOID(D_CELL)) // no void conditions allowed (as with IF)
            fail (Error(RE_NO_RETURN));

        if (IS_END(f.value)) // require conditions and branches in pairs
            fail (Error(RE_PAST_END));

        if (IS_BAR(f.value)) // BAR! out of sync, between condition and branch
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
            Do_Next_In_Frame_May_Throw(D_CELL, &f, DO_FLAG_NORMAL);
            if (THROWN(D_CELL)) {
                Move_Value(D_OUT, D_CELL);
                goto return_thrown;
            }

            // Should the slot contain a single arity function taking a logic
            // (and this not be an /ONLY), then it's treated as a brancher.
            // It is told it failed the test, and may choose to perform some
            // action in a response...but the result is discarded.
            //
            //
            if (Maybe_Run_Failed_Branch_Throws(dummy, D_CELL, REF(only))) {
                Move_Value(D_OUT, dummy);
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

        Do_Next_In_Frame_May_Throw(D_CELL, &f, DO_FLAG_NORMAL);
        if (THROWN(D_CELL)) {
            Move_Value(D_OUT, D_CELL);
            goto return_thrown;
        }

        // !!! Optimization note: if the previous evaluation had gone into
        // D_OUT directly it could just stay there in some cases; and even
        // block evaluation doesn't need the copy.  Review how this shared
        // code might get more efficient if the data were already in D_OUT.
        //
        if (Run_Success_Branch_Throws(D_OUT, D_CELL, REF(only)))
            goto return_thrown;

        if (NOT(REF(all)))
            goto return_matched;

        // keep matching if /ALL
    }

//return_maybe_matched:
    Drop_Frame(&f);
    return R_OUT_Q(REF(q)); // if /?, detect if D_OUT was written to

return_matched:
    Drop_Frame(&f);
    if (REF(q)) return R_TRUE; // /? gets TRUE if at least one case ran
    return R_OUT;

return_thrown:
    Drop_Frame(&f);
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
//      default-case
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
    INCLUDE_PARAMS_OF_SWITCH; // ? is renamed as "q"

    REBFRM f;
    Push_Frame(&f, ARG(cases));

    // The evaluator always initializes the out slot to an END marker.  That
    // makes sure it gets overwritten with a value (or void) before returning.
    // But here SWITCH also lets END indicate no matching cases ran yet.

    assert(IS_END(D_OUT));

    REBVAL *value = ARG(value);

    // For safety, notice if someone wrote `switch [x] [...]` with a literal
    // block in source, as that is likely a mistake.
    //
    if (IS_BLOCK(value) && GET_VAL_FLAG(value, VALUE_FLAG_UNEVALUATED))
        fail (Error(RE_BLOCK_SWITCH, value));

    // Frame's extra D_CELL is free since the function has > 1 arg.  Reuse it
    // as a temporary GC-safe location for holding evaluations.  This
    // holds the last test so that `switch 9 [1 ["a"] 2 ["b"] "c"]` is "c".

    SET_VOID(D_CELL); // used for "fallout"

    while (NOT_END(f.value)) {

        // If a block is seen at this point, it doesn't correspond to any
        // condition to match.  If no more tests are run, let it suppress the
        // feature of the last value "falling out" the bottom of the switch

        if (IS_BLOCK(f.value)) {
            SET_VOID(D_CELL);
            goto continue_loop;
        }

        // GROUP!, GET-WORD! and GET-PATH! are evaluated in Ren-C's SWITCH
        // All other types are seen as-is (hence words act "quoted")

        if (
            IS_GROUP(f.value)
            || IS_GET_WORD(f.value)
            || IS_GET_PATH(f.value)
        ){
            if (EVAL_VALUE_CORE_THROWS(D_CELL, f.value, f.specifier)) {
                Move_Value(D_OUT, D_CELL);
                goto return_thrown;
            }
        }
        else
            Derelativize(D_CELL, f.value, f.specifier);

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
            Fetch_Next_In_Frame(&f);
            if (IS_END(f.value))
                goto return_defaulted;
        } while (!IS_BLOCK(f.value));

        // Run the code if it was found.  Because it writes D_OUT with a value
        // (or void), it won't be END--so we'll know at least one case has run.

        REBSPC *derived; // goto would cross initialization
        derived = Derive_Specifier(VAL_SPECIFIER(ARG(cases)), f.value);
        if (Do_At_Throws(
            D_OUT,
            VAL_ARRAY(f.value),
            VAL_INDEX(f.value),
            derived
        )) {
            goto return_thrown;
        }

        // Only keep processing if the /ALL refinement was specified

        if (NOT(REF(all)))
            goto return_matched;

    continue_loop:
        Fetch_Next_In_Frame(&f);
    }

    if (NOT_END(D_OUT)) // at least one case body's DO ran and overwrote D_OUT
        goto return_matched;

return_defaulted:
    if (REF(default)) {
        const REBOOL only = FALSE;

        if (Run_Success_Branch_Throws(D_OUT, ARG(default_case), only))
            goto return_thrown;
    }
    else
        Move_Value(D_OUT, D_CELL); // last test "falls out", might be void

    Drop_Frame(&f);
    if (REF(q))
        return R_FALSE; // running default code doesn't count for /?
    return R_OUT;

return_matched:
    Drop_Frame(&f);
    if (REF(q))
        return R_TRUE;
    return R_OUT;

return_thrown:
    Drop_Frame(&f);
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
    INCLUDE_PARAMS_OF_CATCH; // ? is renamed as "q"

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

                    Derelativize(temp1, candidate, VAL_SPECIFIER(ARG(names)));
                    Move_Value(temp2, D_OUT);

                    // Return the THROW/NAME's arg if the names match
                    // !!! 0 means equal?, but strict-equal? might be better
                    //
                    if (Compare_Modify_Values(temp1, temp2, 0))
                        goto was_caught;
                }
            }
            else {
                Move_Value(temp1, ARG(names));
                Move_Value(temp2, D_OUT);

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
        Move_Value(thrown_name, D_OUT); // THROWN bit cleared by CATCH_THROWN

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
    INCLUDE_PARAMS_OF_THROW;

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
        Move_Value(D_OUT, ARG(name_value));
    else {
        // Blank values serve as representative of THROWN() means "no name"
        //
        SET_BLANK(D_OUT);
    }

    CONVERT_NAME_TO_THROWN(D_OUT, value);
    return R_OUT_IS_THROWN;
}
