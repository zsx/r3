//
//  File: %n-do.c
//  Summary: "native functions for DO, EVAL, APPLY"
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
// Ren-C's philosophy of DO is that the argument to it represents a place to
// find source code.  Hence `DO 3` does not evaluate to the number 3, any
// more than `DO "print hello"` would evaluate to `"print hello"`.  If a
// generalized evaluator is needed, use the special-purpose function EVAL.
//
// Note that although the code for running blocks and frames is implemented
// here as C, the handler for processing STRING!, FILE!, TAG!, URL!, etc. is
// dispatched out to some Rebol code.  See `system/intrinsic/do*`.
//

#include "sys-core.h"


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

        return Apply_Frame_Core(f, Canon(SYM___ANONYMOUS__), NULL); }

    default:
        break;
    }

    // Note: it is not possible to write a wrapper function in Rebol
    // which can do what EVAL can do for types that consume arguments
    // (like SET-WORD!, SET-PATH! and FUNCTION!).  DO used to do this for
    // functions only, EVAL generalizes it.
    //
    fail (Error(RE_USE_EVAL_FOR_EVAL));
}


//
//  apply: native [
//
//  {Invoke a function with all required arguments specified.}
//
//      return: [<opt> any-value!]
//      value [function! any-word! any-path!]
//          {Function or specifying word (preserves word name for debug info)}
//      def [block!]
//          {Frame definition block (will be bound and evaluated)}
//  ]
//
REBNATIVE(apply)
{
    PARAM(1, value);
    PARAM(2, def);

    REBVAL *def = ARG(def);

    REBFRM frame;
    REBFRM *f = &frame;

#if !defined(NDEBUG)
    RELVAL *first_def = VAL_ARRAY_AT(def);

    // !!! Because APPLY has changed, help warn legacy usages by alerting
    // if the first element of the block is not a SET-WORD!.  A BAR! can
    // subvert the warning: `apply :foo [| comment {This is a new APPLY} ...]`
    //
    if (NOT_END(first_def)) {
        if (!IS_SET_WORD(first_def) && !IS_BAR(first_def)) {
            fail(Error(RE_APPLY_HAS_CHANGED));
        }
    }
#endif

    // We don't limit to taking a FUNCTION! value directly, because that loses
    // the symbol (for debugging, errors, etc.)  If caller passes a WORD!
    // then we lookup the variable to get the function, but save the symbol.
    //
    REBSTR *name;
    Get_If_Word_Or_Path_Arg(D_OUT, &name, ARG(value));
    if (name == NULL)
        name = Canon(SYM___ANONYMOUS__); // Do_Core requires non-NULL symbol

    if (!IS_FUNCTION(D_OUT))
        fail(Error(RE_APPLY_NON_FUNCTION, ARG(value))); // for SPECIALIZE too

    f->gotten = D_OUT;
    f->out = D_OUT;

    return Apply_Frame_Core(f, name, def);
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