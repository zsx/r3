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
//   in R3-Alpha and Rebol2...but this is forced to blank if it was void,
//   so that THEN and ELSE can distinguish whether a condition ran.
//
// * It is possible to ask the return result to not be "blankified", but
//   return the true value, with the /ONLY refinement.  This is specialized
//   as functions ending in *.  (IF*, EITHER*, CASE*, SWITCH*...)
//
// * Other specializations exist returning a logic of whether the body ever
//   ran by using the /? refinement.  So CASE? does not return the branch
//   values, just true or false based on whether a branch ran.  This is
//   based on testing the result for void.
//
// * Zero-arity function values used as branches will be executed.  Future
//   plans may allow for single-arity functions to be passed the condition,
//   see Run_Branch_Throws() for notes.
//
// * If the /ONLY option is not used, then there is added checking on the
//   condition and branches.  The condition is not allowed to be a literal
//   block, e.g. `[x = 10]`, but may be an expression evaluating to a block.
//   The branches are not allowed to be evaluative *unless* they evaluate to
//   a block...literals such as strings or integers may be used, but not
//   variables or expressions that evaluate to strings or integers.
//

#include "sys-core.h"


//
//  if: native [
//
//  {If TRUTHY? condition, take branch (blocks and functions evaluated)}
//
//      return: [<opt> any-value!]
//          {void on FALSEY? condition, else branch result (BLANK! if void)}
//      condition [any-value!]
//      branch [<opt> any-value!]
//          {Evaluated if block or function, else literal value}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(if)
{
    INCLUDE_PARAMS_OF_IF;

    if (IS_CONDITIONAL_TRUE(ARG(condition), REF(only))) {
        if (Run_Branch_Throws(D_OUT, ARG(branch), REF(only)))
            return R_OUT_IS_THROWN;

        if (REF(only))
            return R_OUT;
        return R_OUT_BLANK_IF_VOID;
    }

    return R_VOID;
}


//
//  unless: native [
//
//  {If FALSEY? condition, take branch (blocks and functions evaluated)}
//
//      return: [<opt> any-value!]
//          {void on TRUTHY? condition, else branch result (BLANK! if void)}
//      condition [any-value!]
//      branch [<opt> any-value!]
//          {Evaluated if block or function, else literal value}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(unless)
{
    INCLUDE_PARAMS_OF_UNLESS;

    if (IS_CONDITIONAL_FALSE(ARG(condition), REF(only))) {
        if (Run_Branch_Throws(D_OUT, ARG(branch), REF(only)))
            return R_OUT_IS_THROWN;

        if (REF(only))
            return R_OUT;
        return R_OUT_BLANK_IF_VOID;
    }

    return R_VOID;
}


//
//  either: native [
//
//  {If TRUTHY? condition, take first branch, else take second branch.}
//
//      return: [<opt> any-value!]
//      condition [any-value!]
//      true-branch [<opt> any-value!]
//      false-branch [<opt> any-value!]
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(either)
{
    INCLUDE_PARAMS_OF_EITHER;

    if (IS_CONDITIONAL_TRUE(ARG(condition), REF(only))) {
        if (Run_Branch_Throws(D_OUT, ARG(true_branch), REF(only)))
            return R_OUT_IS_THROWN;
    }
    else {
        if (Run_Branch_Throws(D_OUT, ARG(false_branch), REF(only)))
            return R_OUT_IS_THROWN;
    }

    if (REF(only))
        return R_OUT;
    return R_OUT_BLANK_IF_VOID;
}


//
//  either-test: native [
//
//  {If value passes test, return that value, otherwise take the branch.}
//
//      return: [<opt> any-value!]
//          {Input value if it matched, or branch result (BLANK! if void)}
//      test [function! datatype! typeset! block! logic!]
//          {Typeset membership, LOGIC! to test TRUTHY?, filter function}
//      value [<opt> any-value!]
//      branch [<opt> any-value!]
//          {If test fails, evaluated if block/function, else literal value}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//      /error-hack
//          "If branch returns an ERROR!, FAIL on it and indicate callsite"
//  ]
//
REBNATIVE(either_test)
{
    INCLUDE_PARAMS_OF_EITHER_TEST;

    REBVAL *test = ARG(test);
    REBVAL *value = ARG(value);

    if (IS_LOGIC(test)) {
        if (IS_VOID(value) || VAL_LOGIC(test) != IS_TRUTHY(value))
            goto test_failed;
        return R_FROM_BOOL(VAL_LOGIC(test));
    }

    // Force single items into array style access so only one version of the
    // code needs to be written.
    //
    RELVAL *item;
    REBSPC *specifier;
    if (IS_BLOCK(test)) {
        item = VAL_ARRAY_AT(test);
        specifier = VAL_SPECIFIER(test);
    }
    else {
        Move_Value(D_CELL, test);
        item = D_CELL; // implicitly terminated
        specifier = SPECIFIED;
    }

    REB_R r; // goto crosses initialization
    r = R_UNHANDLED;

    for (; NOT_END(item); ++item) {
        //
        // If we're dealing with a single item for the test, provided e.g.
        // as :even?, then it's already fetched.  But if it was a block like
        // [:even? integer!] we enumerate it in word form and have to get it.
        //
        const RELVAL *var = IS_WORD(item)
            ? Get_Opt_Var_May_Fail(item, specifier)
            : item;

        if (IS_DATATYPE(var)) {
            if (VAL_TYPE_KIND(var) == VAL_TYPE(value))
                r = R_TRUE; // any type matching counts
            else if (r == R_UNHANDLED)
                r = R_FALSE; // at least one type has to speak up now
        }
        else if (IS_TYPESET(var)) {
            if (TYPE_CHECK(var, VAL_TYPE(value)))
                r = R_TRUE; // any typeset matching counts
            else if (r == R_UNHANDLED)
                r = R_FALSE; // at least one type has to speak up now
        }
        else if (IS_FUNCTION(var)) {
            const REBOOL fully = TRUE;
            if (Apply_Only_Throws(D_OUT, fully, const_KNOWN(var), value, END))
                return R_OUT_IS_THROWN;

            if (IS_VOID(D_OUT))
                fail (Error_No_Return_Raw());

            if (IS_FALSEY(D_OUT))
                goto test_failed; // any function failing breaks it

            // At least one function matching tips the balance, but
            // can't alone outmatch no types matching, if any types
            // were matched at all.
            //
            if (r == R_UNHANDLED)
                r = R_TRUE;
            continue;
        }
        else
            fail (Error_Invalid_Type(VAL_TYPE(var)));
    }

    if (r == R_UNHANDLED) {
        //
        // !!! When the test is just [], what's that?  People aren't likely to
        // write it literally, but it could happen from a COMPOSE or similar.
        //
        fail ("No tests found in EITHER-TEST.");
    }

    if (r == R_FALSE) {
        //
        // This means that some types didn't match and were not later
        // redeemed by a type that did match.  Consider it failure.
        //
        goto test_failed;
    }

    // Someone spoke up for test success and was not overridden.
    //
    assert(r == R_TRUE);
    Move_Value(D_OUT, ARG(value));
    return R_OUT;

test_failed:
    if (Run_Branch_Throws(D_OUT, ARG(branch), REF(only)))
        return R_OUT_IS_THROWN;

    if (REF(error_hack) && IS_ERROR(D_OUT)) {
        //
        // !!! If you try to SPECIALIZE a conditional and specify a branch
        // for it to run, it currently has no good way to get at the
        // parameters of the instance of the specialized function when it
        // is running.  But ENSURE is a commonly used routine that specializes
        // EITHER-TEST, and it wants to indicate the originating value that
        // caused the failure.  For now, let it use /ERROR-HACK so that it
        // generates the error and asks us to trigger it.
        //
        REBCTX *error = VAL_CONTEXT(D_OUT);
        Set_Location_Of_Error(error, frame_);
        fail (error);
    }

    if (REF(only))
        return R_OUT;
    return R_OUT_BLANK_IF_VOID;
}


//
//  either-test-void: native [
//
//  {If value is void, return void, otherwise take the branch.}
//
//      return: [<opt> any-value!]
//          {Void if input is void, or branch result (BLANK! if void)}
//      value [<opt> any-value!]
//      branch [<opt> any-value!]
//          {If valued input, evaluated if block/function, else literal value}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(either_test_void)
//
// Native optimization of `specialize 'either-test-value [test: :void?]`
// Worth it to write because this is the functionality enfixed as THEN.
{
    INCLUDE_PARAMS_OF_EITHER_TEST_VOID;

    if (IS_VOID(ARG(value))) {
        Move_Value(D_OUT, ARG(value));
        return R_OUT;
    }

    if (Run_Branch_Throws(D_OUT, ARG(branch), REF(only)))
        return R_OUT_IS_THROWN;

    if (REF(only))
        return R_OUT;
    return R_OUT_BLANK_IF_VOID;
}


//
//  either-test-value: native [
//
//  {If value is not void, return the value, otherwise take the branch.}
//
//      return: [<opt> any-value!]
//          {Input value if not void, or branch result (BLANK! if void)}
//      value [<opt> any-value!]
//      branch [<opt> any-value!]
//          {If void input, evaluated if block/function, else literal value}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(either_test_value)
//
// Native optimization of `specialize 'either-test-value [test: :any-value?]`
// Worth it to write because this is the functionality enfixed as ELSE.
{
    INCLUDE_PARAMS_OF_EITHER_TEST_VALUE;

    if (!IS_VOID(ARG(value))) {
        Move_Value(D_OUT, ARG(value));
        return R_OUT;
    }

    if (Run_Branch_Throws(D_OUT, ARG(branch), REF(only)))
        return R_OUT_IS_THROWN;

    if (REF(only))
        return R_OUT;
    return R_OUT_BLANK_IF_VOID;
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

    DECLARE_FRAME (f);
    Push_Frame(f, ARG(block));

    while (NOT_END(f->value)) {
        if (Do_Next_In_Frame_Throws(D_CELL, f)) {
            Drop_Frame(f);
            Move_Value(D_OUT, D_CELL);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_CELL)) // voids do not "vote" true or false
            continue;

        if (IS_FALSEY(D_CELL)) { // a failed ALL returns BLANK!
            Drop_Frame(f);
            return R_BLANK;
        }

        Move_Value(D_OUT, D_CELL); // preserve (not overwritten by later voids)
    }

    Drop_Frame(f);

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

    DECLARE_FRAME (f);
    Push_Frame(f, ARG(block));

    REBOOL voted = FALSE;

    while (NOT_END(f->value)) {
        if (Do_Next_In_Frame_Throws(D_OUT, f)) {
            Drop_Frame(f);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_TRUTHY(D_OUT)) { // successful ANY returns the value
            Drop_Frame(f);
            return R_OUT;
        }

        voted = TRUE; // signal at least one non-void result was seen
    }

    Drop_Frame(f);

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

    DECLARE_FRAME (f);
    Push_Frame(f, ARG(block));

    REBOOL voted = FALSE;

    while (NOT_END(f->value)) {
        if (Do_Next_In_Frame_Throws(D_OUT, f)) {
            Drop_Frame(f);
            return R_OUT_IS_THROWN;
        }

        if (IS_VOID(D_OUT)) // voids do not "vote" true or false
            continue;

        if (IS_TRUTHY(D_OUT)) { // any true results mean failure
            Drop_Frame(f);
            return R_BLANK;
        }

        voted = TRUE; // signal that at least one non-void result was seen
    }

    Drop_Frame(f);

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
//          {Last matched case evaluation, or void if no cases matched}
//      cases [block!]
//          "Block of cases (conditions followed by branches)"
//      /all
//          {Evaluate all cases (do not stop at first TRUTHY? case)}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(case)
{
    INCLUDE_PARAMS_OF_CASE; // ? is renamed as "q"

    DECLARE_FRAME (f);
    Push_Frame(f, ARG(cases));

    // With the block argument pushed in the enumerator, that frame slot is
    // available for scratch space in the rest of the routine.

    while (NOT_END(f->value)) {
        if (IS_BAR(f->value)) { // interstitial BAR! legal, `case [1 2 | 3 4]`
            Fetch_Next_In_Frame(f);
            continue;
        }

        // Perform a DO/NEXT's worth of evaluation on a "condition" to test

        if (Do_Next_In_Frame_Throws(D_CELL, f)) {
            Move_Value(D_OUT, D_CELL);
            goto return_thrown;
        }

        if (IS_VOID(D_CELL)) // no void conditions allowed (as with IF)
            fail (Error_No_Return_Raw());

        if (IS_END(f->value)) // require conditions and branches in pairs
            fail (Error_Past_End_Raw());

        if (IS_BAR(f->value)) // BAR! out of sync between condition and branch
            fail (Error_Bar_Hit_Mid_Case_Raw());

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
        if (IS_CONDITIONAL_FALSE(D_CELL, REF(only))) {
            if (Do_Next_In_Frame_Throws(D_CELL, f)) {
                Move_Value(D_OUT, D_CELL);
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

        if (Do_Next_In_Frame_Throws(D_CELL, f)) {
            Move_Value(D_OUT, D_CELL);
            goto return_thrown;
        }

        // !!! Optimization note: if the previous evaluation had gone into
        // D_OUT directly it could just stay there in some cases; and even
        // block evaluation doesn't need the copy.  Review how this shared
        // code might get more efficient if the data were already in D_OUT.
        //
        if (Run_Branch_Throws(D_OUT, D_CELL, REF(only)))
            goto return_thrown;

        if (NOT(REF(all)))
            goto return_matched;

        // keep matching if /ALL
    }

    goto return_maybe_matched;

return_maybe_matched: // CASE/ALL can get here even if D_OUT not written
    Drop_Frame(f);
    if (REF(only))
        return R_OUT_VOID_IF_UNWRITTEN; // user wants voids as-is
    return R_OUT_VOID_IF_UNWRITTEN_BLANK_IF_VOID;

return_matched:
    Drop_Frame(f);
    if (REF(only))
        return R_OUT; // user wants voids as-is
    return R_OUT_BLANK_IF_VOID;

return_thrown:
    Drop_Frame(f);
    return R_OUT_IS_THROWN;
}


//
//  switch: native [
//
//  {Selects a choice and evaluates the block that follows it.}
//
//      return: [<opt> any-value!]
//          {Last case evaluation, or void if no cases matched}
//      value [any-value!]
//          "Target value"
//      cases [block!]
//          "Block of cases (comparison lists followed by block branches)"
//      /default
//          "Default case if no others found"
//      default-case [any-value!]
//          "Block to execute (or value to return)"
//      /all
//          "Evaluate all matches (not just first one)"
//      /strict
//          {Use STRICT-EQUAL? when comparing cases instead of EQUAL?}
//      /only
//          "If branch runs and returns void, do not convert it to BLANK!"
//  ]
//
REBNATIVE(switch)
{
    INCLUDE_PARAMS_OF_SWITCH; // ? is renamed as "q"

    DECLARE_FRAME (f);
    Push_Frame(f, ARG(cases));

    // The evaluator always initializes the out slot to an END marker.  That
    // makes sure it gets overwritten with a value (or void) before returning.
    // But here SWITCH also lets END indicate no matching cases ran yet.

    assert(IS_END(D_OUT));

    REBVAL *value = ARG(value);

    // For safety, notice if someone wrote `switch [x] [...]` with a literal
    // block in source, as that is likely a mistake.
    //
    if (IS_BLOCK(value) && GET_VAL_FLAG(value, VALUE_FLAG_UNEVALUATED))
        fail (Error_Block_Switch_Raw(value));

    // Frame's extra D_CELL is free since the function has > 1 arg.  Reuse it
    // as a temporary GC-safe location for holding evaluations.  This
    // holds the last test so that `switch 9 [1 ["a"] 2 ["b"] "c"]` is "c".

    Init_Void(D_CELL); // used for "fallout"

    while (NOT_END(f->value)) {

        // If a block is seen at this point, it doesn't correspond to any
        // condition to match.  If no more tests are run, let it suppress the
        // feature of the last value "falling out" the bottom of the switch

        if (IS_BLOCK(f->value)) {
            Init_Void(D_CELL);
            goto continue_loop;
        }

        // GROUP!, GET-WORD! and GET-PATH! are evaluated in Ren-C's SWITCH
        // All other types are seen as-is (hence words act "quoted")

        if (
            IS_GROUP(f->value)
            || IS_GET_WORD(f->value)
            || IS_GET_PATH(f->value)
        ){
            if (Eval_Value_Core_Throws(D_CELL, f->value, f->specifier)) {
                Move_Value(D_OUT, D_CELL);
                goto return_thrown;
            }
        }
        else
            Derelativize(D_CELL, f->value, f->specifier);

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
            Fetch_Next_In_Frame(f);
            if (IS_END(f->value))
                goto return_defaulted;
        } while (!IS_BLOCK(f->value));

        // Run the code if it was found.  Because it writes D_OUT with a value
        // (or void), it won't be END--so we'll know at least one case has run.

        REBSPC *derived; // goto would cross initialization
        derived = Derive_Specifier(VAL_SPECIFIER(ARG(cases)), f->value);
        if (Do_At_Throws(
            D_OUT,
            VAL_ARRAY(f->value),
            VAL_INDEX(f->value),
            derived
        )) {
            goto return_thrown;
        }

        // Only keep processing if the /ALL refinement was specified

        if (NOT(REF(all)))
            goto return_matched;

    continue_loop:
        Fetch_Next_In_Frame(f);
    }

    if (NOT_END(D_OUT)) // at least one case body's DO ran and overwrote D_OUT
        goto return_matched;

return_defaulted:
    Drop_Frame(f);

    if (REF(default)) {
        if (Run_Branch_Throws(D_OUT, ARG(default_case), REF(only)))
            return R_OUT_IS_THROWN;

        if (REF(only))
            return R_OUT;
        return R_OUT_BLANK_IF_VOID;
    }

    Move_Value(D_OUT, D_CELL); // last test "falls out", might be void
    return R_OUT;

return_matched:
    Drop_Frame(f);

    if (REF(only))
        return R_OUT;
    return R_OUT_BLANK_IF_VOID;

return_thrown:
    Drop_Frame(f);
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
        fail (Error_Bad_Refines_Raw());

    if (Do_Any_Array_At_Throws(D_OUT, ARG(block))) {
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
                        fail (ARG(names));

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
            if (Do_Any_Array_At_Throws(D_OUT, ARG(handler)))
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
                END
            )){
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
        fail (Error_Use_Fail_For_Error_Raw(value));

        // Note: Caller can put the ERROR! in a block or use some other
        // such trick if it wants to actually throw an error.
        // (Better than complicating via THROW/ERROR-IS-INTENTIONAL!)
    }

    if (REF(name))
        Move_Value(D_OUT, ARG(name_value));
    else {
        // Blank values serve as representative of THROWN() means "no name"
        //
        Init_Blank(D_OUT);
    }

    CONVERT_NAME_TO_THROWN(D_OUT, value);
    return R_OUT_IS_THROWN;
}
