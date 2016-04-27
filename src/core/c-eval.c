//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
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
//  Summary: Central Interpreter Evaluator
//  File: %c-do.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This file contains `Do_Core()`, which is the central evaluator which
// is behind DO.  It can execute single evaluation steps (e.g. a DO/NEXT)
// or it can run the array to the end of its content.  A flag controls that
// behavior, and there are other flags for controlling its other behaviors.
//
// For comprehensive notes on the input parameters, output parameters, and
// internal state variables...see %sys-do.h and `struct Reb_Frame`.
//
// NOTES:
//
// * This is a very long routine.  That is largely on purpose, because it
//   doesn't contain repeated portions.  If it were broken into functions that
//   would add overhead for little benefit, and prevent interesting tricks
//   and optimizations.  Note that it is broken down into sections, and
//   the invariants in each section are made clear with comments and asserts.
//
// * The evaluator only moves forward, and it consumes exactly one element
//   from the input at a time.  This input may be a source where the index
//   needs to be tracked and care taken to contain the index within its
//   boundaries in the face of change (e.g. a mutable ARRAY).  Or it may be
//   an entity which tracks its own position on each fetch, where "indexor"
//   is serving as a flag and should be left static.
//
// !!! There is currently no "locking" or other protection on the arrays that
// are in the call stack and executing.  Each iteration must be prepared for
// the case that the array has been modified out from under it.  The code
// evaluator will not crash, but re-fetches...ending the evaluation if the
// array has been shortened to before the index, and using possibly new
// values.  The benefits of this self-modifying lenience should be reviewed
// to inform a decision regarding the locking of arrays during evaluation.
//

#include "sys-core.h"

#include "tmp-evaltypes.inc"


#if !defined(NDEBUG)
    //
    // Forward declarations for debug-build-only code--routines at end of
    // file.  (Separated into functions to reduce clutter in the main logic.)
    //
    static REBCNT Do_Core_Entry_Checks_Debug(struct Reb_Frame *f);
    static REBCNT Do_Core_Expression_Checks_Debug(struct Reb_Frame *f);
    static void Do_Core_Exit_Checks_Debug(struct Reb_Frame *f);

    // The `do_count` should be visible in the C debugger watchlist as a
    // local variable in Do_Core() for each stack level.  So if a fail()
    // happens at a deterministic moment in a run, capture the number from
    // the level of interest and recompile with it here to get a breakpoint
    // at that tick.
    //
    //      *** DON'T COMMIT THIS v-- KEEP IT AT ZERO! ***
    #define DO_COUNT_BREAKPOINT    0
    //      *** DON'T COMMIT THIS --^ KEEP IT AT ZERO! ***
    //
    // !!! Taking this number on the command line could be convenient.
#endif


// Simple macro for wrapping (but not obscuring) a `goto` in the code below
//
#define NOTE_THROWING(g) \
    do { \
        assert(f->indexor == THROWN_FLAG); \
        assert(THROWN(f->out)); \
        g; /* goto statement left at callsite for readability */ \
    } while(0)


// In Ren-C, marking an argument used is done by setting it to a WORD! which
// has the same symbol as the refinement itself.  This makes certain chaining
// scenarios easier (though APPLY is being improved to the point where it
// may be less necessary).  This macro makes it clear that's what's happening.
//
#define MARK_REFINEMENT_USED(arg,param) \
    Val_Init_Word((arg), REB_WORD, VAL_TYPESET_SYM(param));


//
//  Do_Core: C
//
void Do_Core(struct Reb_Frame * const f)
{
#if !defined(NDEBUG)
    REBCNT do_count; // cache of `f->do_count` (improves watchlist visibility)
#endif

    enum Reb_Param_Class pclass; // cached while fulfilling an argument

    // APPLY may wish to do a fast jump to doing type checking on the args.
    // Other callers may have similar interests (and this may be explored
    // further with "continuations" of the evaluator).  If requested, skip
    // the dispatch and go straight to a label.
    //
    switch (f->mode) {
    case CALL_MODE_GUARD_ARRAY_ONLY:
        //
        // Chain the call state into the stack, and mark it as generally not
        // having valid fields to GC protect (more in use during functions).
        //
        f->prior = TG_Frame_Stack;
        if (TG_Frame_Stack)
            TG_Frame_Stack->next = f;
        TG_Frame_Stack = f;
    #if !defined(NDEBUG)
        SNAP_STATE(&f->state); // to make sure stack balances, etc.
        do_count = Do_Core_Entry_Checks_Debug(f); // run once per Do_Core()
    #endif
        f->opt_label_sym = SYM_0;
        break;

    case CALL_MODE_ARGS:
        assert(TG_Frame_Stack == f); // should already be pushed
        assert(f->opt_label_sym != SYM_0);
    #if !defined(NDEBUG)
        do_count = TG_Do_Count; // entry checks for debug not true here
    #endif
        goto do_function_arglist_in_progress;

    default:
        assert(FALSE);
    }

    // Check just once (stack level would be constant if checked in a loop)
    //
    if (C_STACK_OVERFLOWING(&f)) Trap_Stack_Overflow();

    // Capture the data stack pointer on entry (used by debug checks, but
    // also refinements are pushed to stack and need to be checked if there
    // are any that are not processed)
    //
    f->dsp_orig = DSP;

    // Indicate that we do not have a value already fetched by eval which is
    // pending to be the next fetch (after the eval's "slipstreamed" f->value
    // is done processing).
    //
    f->eval_fetched = NULL;

    // The f->out slot is GC protected while the natives or user code runs.
    // To keep it from crashing the GC, we put in "safe trash" that will be
    // acceptable to the GC but raise alerts if any other code reads it.
    //
    SET_TRASH_SAFE(f->out);

value_ready_for_do_next:
    //
    // f->value is expected to be set here, as is f->index
    //
    // !!! are there more rules for the locations value can't point to?
    // Note that a fetched value pointer may be within a va_arg list.  Also
    // consider the GC implications of running ANY non-EVAL/ONLY scenario;
    // how do you know the values are safe?  (See ideas in %sys-do.h)
    //
    assert(f->value && !IS_END(f->value) && f->value != f->out);
    assert(f->indexor != END_FLAG && f->indexor != THROWN_FLAG);

    if (Trace_Flags) Trace_Line(f);

    // Save the index at the start of the expression in case it is needed
    // for error reporting.  FRM_INDEX can account for prefetching, but it
    // cannot know what a preloaded head value was unless it was saved
    // under a debug> mode.
    //
    f->expr_index = f->indexor;

    // Make sure `eval` is trash in debug build if not doing a `reevaluate`.
    // It does not have to be GC safe (for reasons explained below).  We
    // also need to reset evaluation to normal vs. a kind of "inline quoting"
    // in case EVAL/ONLY had enabled that.
    //
    VAL_INIT_WRITABLE_DEBUG(&(f->cell.eval)); // in union, always reinit
    SET_TRASH_IF_DEBUG(&(f->cell.eval));

    f->args_evaluate = NOT(f->flags & DO_FLAG_NO_ARGS_EVALUATE);

    assert(Eval_Count != 0);
    if (--Eval_Count == 0 || Eval_Signals) {
        //
        // Note that Do_Signals_Throws() may do a recycle step of the GC, or
        // it may spawn an entire interactive debugging session via
        // breakpoint before it returns.  It may also FAIL and longjmp out.
        //
        if (Do_Signals_Throws(f->out)) {
            f->indexor = THROWN_FLAG;
            NOTE_THROWING(goto return_indexor);
        }

        if (!IS_UNSET(f->out)) {
            //
            // !!! What to do with something like a Ctrl-C-based breakpoint
            // session that does something like `resume/with 10`?  We are
            // "in-between" evaluations, so that 10 really has no meaning
            // and is just going to get discarded.  FAIL for now to alert
            // the user that something is off, but perhaps the failure
            // should be contained in a sandbox and restart the break?
            //
            fail (Error(RE_MISC));
        }
    }

reevaluate:
    // ^--
    // `reevaluate` is jumped to by EVAL, and must skip the possible Recycle()
    // from the above.  Whenever `eval` holds a REBVAL it is unseen by the GC
    // *by design*.  This avoids having to initialize it or GC-safe null it
    // each time through the evaluator loop.  It will only be protected by
    // the GC indirectly when its properties are extracted during the switch,
    // such as a function that gets stored into `f->func`.
    //
    // (We also want the debugger to consider the triggering EVAL as the
    // start of the expression, and don't want to advance `expr_index`).

    f->lookahead_flags = (f->flags & DO_FLAG_LOOKAHEAD)
        ? DO_FLAG_LOOKAHEAD
        : DO_FLAG_NO_LOOKAHEAD;

    // On entry we initialized `f->out` to a GC-safe value, and no evaluations
    // should write END markers or unsafe trash in the slot.  As evaluations
    // proceed the value they wrote in `f->out` should be fine to leave there
    // as it won't crash the GC--and is cheaper than overwriting.  But in the
    // debug build, throw safe trash in the slot half the time to catch stray
    // reuses of irrelevant data...and test the release path the other half.
    //
    if (SPORADICALLY(2)) SET_TRASH_SAFE(f->out);

#if !defined(NDEBUG)
    do_count = Do_Core_Expression_Checks_Debug(f); // per-DO/NEXT debug checks
    cast(void, do_count); // suppress unused warning
#endif

    //==////////////////////////////////////////////////////////////////==//
    //
    // BEGIN MAIN SWITCH STATEMENT
    //
    //==////////////////////////////////////////////////////////////////==//

    switch (Eval_Table[VAL_TYPE(f->value)]) { // see ET_XXX, RE: jump table

//==//////////////////////////////////////////////////////////////////////==//
//
// [no evaluation] (REB_BLOCK, REB_INTEGER, REB_STRING, etc.)
//
// Copy the value's bits to f->out and fetch the next value.  (Infix behavior
// may kick in for this same "DO/NEXT" step--see processing after switch.)
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_NONE:
        QUOTE_NEXT_REFETCH(f->out, f);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [BAR! and LIT-BAR!]
//
// If an expression barrier is seen in-between expressions (as it will always
// be if hit in this switch), it becomes UNSET!.  It only errors in argument
// fulfillment during the switch case for ANY-FUNCTION!.
//
// LIT-BAR! decays into an ordinary BAR! if seen here by the evaluator.
//
// Note that natives and dialects frequently do their own interpretation of
// BAR!--rather than just evaluate it and let it mean something equivalent
// to an unset.  For instance:
//
//     case [false [print "F"] | true [print ["T"]]
//
// If CASE did not specially recognize BAR!, it would complain that the
// "second condition" was UNSET!.  So if you are looking for a BAR! behavior
// and it's not passing through here, check the construct you are using.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_BAR:
        SET_UNSET(f->out);
        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;

    case ET_LIT_BAR:
        SET_BAR(f->out);
        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [WORD!]
//
// A plain word tries to fetch its value through its binding.  It will fail
// and longjmp out of this stack if the word is unbound (or if the binding is
// to a variable which is not set).  Should the word look up to a function,
// then that function will be called by jumping to the ANY-FUNCTION! case.
//
// Note: Infix functions cannot be dispatched from this point, as there is no
// "Left-Hand-Side" computed to use.  Infix dispatch happens on words during
// a lookahead *after* this switch statement, when a omputed value in f->out
// is available.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_WORD:
        *(f->out) = *GET_OPT_VAR_MAY_FAIL(f->value);

    dispatch_the_word_in_out:
        if (IS_FUNCTION(f->out)) { // check before checking unset, for speed
            if (GET_VAL_FLAG(f->out, FUNC_FLAG_INFIX))
                fail (Error(RE_NO_OP_ARG, f->value)); // see Note above

            f->opt_label_sym = VAL_WORD_SYM(f->value);

        #if !defined(NDEBUG)
            f->label_str = cast(const char*, Get_Sym_Name(f->opt_label_sym));
        #endif

            f->value = f->out;
            goto do_function_in_value;
        }

        if (IS_UNSET(f->out))
            fail (Error(RE_NO_VALUE, f->value)); // need `:x` if `x` is unset

    #if !defined(NDEBUG)
        if (LEGACY(OPTIONS_LIT_WORD_DECAY) && IS_LIT_WORD(f->out))
            VAL_SET_TYPE_BITS(f->out, REB_WORD); // don't reset full header!
    #endif

        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [SET-WORD!]
//
// Does the evaluation into `out`, then gets the variable indicated by the
// word and writes the result there as well.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_SET_WORD:
        f->param = f->value; // fetch writes f->value, so save SET-WORD! ptr

        FETCH_NEXT_ONLY_MAYBE_END(f);
        if (f->indexor == END_FLAG)
            fail (Error(RE_NEED_VALUE, f->param)); // e.g. `do [foo:]`

        if (f->args_evaluate) {
            //
            // A SET-WORD! handles lookahead like a prefix function would;
            // so it uses lookahead on its arguments regardless of f->flags
            //
            DO_NEXT_REFETCH_MAY_THROW(f->out, f, DO_FLAG_LOOKAHEAD);
            if (f->indexor == THROWN_FLAG)
                NOTE_THROWING(goto return_indexor);
        }
        else
            QUOTE_NEXT_REFETCH(f->out, f);

        if (IS_UNSET(f->out))
            fail (Error(RE_NEED_VALUE, f->param)); // e.g. `foo: ()`

        *GET_MUTABLE_VAR_MAY_FAIL(f->param) = *(f->out);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [GET-WORD!]
//
// A GET-WORD! does no checking for unsets, no dispatch on functions, and
// will return an UNSET! if that is what the variable is.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_GET_WORD:
        *(f->out) = *GET_OPT_VAR_MAY_FAIL(f->value);
        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;

//==/////////////////////////////////////////////////////////////////////==//
//
// [LIT-WORD!]
//
// Note we only want to reset the type bits in the header, not the whole
// header--because header bits contain information like WORD_FLAG_BOUND.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_LIT_WORD:
        QUOTE_NEXT_REFETCH(f->out, f);
        VAL_SET_TYPE_BITS(f->out, REB_WORD);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [GROUP!]
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_GROUP:
        if (DO_VAL_ARRAY_AT_THROWS(f->out, f->value)) {
            f->indexor = THROWN_FLAG;
            NOTE_THROWING(goto return_indexor);
        }
        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [PATH!]
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_PATH:
        if (Do_Path_Throws(f->out, &f->opt_label_sym, f->value, NULL)) {
            f->indexor = THROWN_FLAG;
            NOTE_THROWING(goto return_indexor);
        }

        if (IS_FUNCTION(f->out)) {
            //
            // object/func or func/refinements or object/func/refinement
            //
            // Because we passed in a label symbol, the path evaluator was
            // willing to assume we are going to invoke a function if it
            // is one.  Hence it left any potential refinements on data stack.
            //
            assert(DSP >= f->dsp_orig);

            // Cannot handle infix because prior value is wiped out above
            // (Theoretically we could save it if we are DO-ing a chain of
            // values, and make it work.  But then, a loop of DO/NEXT
            // may not behave the same as DO-ing the whole block.  Bad.)
            //
            if (GET_VAL_FLAG(f->out, FUNC_FLAG_INFIX))
                fail (Error_Has_Bad_Type(f->out));

            f->value = f->out;
            goto do_function_in_value;
        }
        else {
            // Path should have been fully processed, no refinements on stack
            //
            assert(DSP == f->dsp_orig);
            FETCH_NEXT_ONLY_MAYBE_END(f);
        }
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [SET-PATH!]
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_SET_PATH:
        f->param = f->value; // fetch writes f->value, save SET-WORD! pointer

        FETCH_NEXT_ONLY_MAYBE_END(f);

        // `do [a/b/c:]` is not legal
        //
        if (f->indexor == END_FLAG)
            fail (Error(RE_NEED_VALUE, f->param));

        // We want the result of the set path to wind up in `out`, so go
        // ahead and put the result of the evaluation there.  Do_Path_Throws
        // will *not* put this value in the output when it is making the
        // variable assignment!
        //
        if (f->args_evaluate) {
            //
            // A SET-PATH! handles lookahead like a prefix function would;
            // so it uses lookahead on its arguments regardless of f->flags
            //
            DO_NEXT_REFETCH_MAY_THROW(f->out, f, DO_FLAG_LOOKAHEAD);

            if (f->indexor == THROWN_FLAG)
                NOTE_THROWING(goto return_indexor);
        }
        else {
            *(f->out) = *(f->value);
            FETCH_NEXT_ONLY_MAYBE_END(f);
        }

        // `a/b/c: ()` is not legal (cannot assign path from unset)
        //
        if (IS_UNSET(f->out))
            fail (Error(RE_NEED_VALUE, f->param));

        // !!! The evaluation ordering of SET-PATH! evaluation seems to break
        // the "left-to-right" nature of the language:
        //
        //     >> foo: make object! [bar: 10]
        //
        //     >> foo/(print "left" 'bar): (print "right" 20)
        //     right
        //     left
        //     == 20
        //
        // In addition to seeming "wrong" it also necessitates an extra cell
        // of storage.  This should be reviewed along with Do_Path generally.
        {
            REBVAL temp;
            VAL_INIT_WRITABLE_DEBUG(&temp);
            if (Do_Path_Throws(&temp, NULL, f->param, f->out)) {
                f->indexor = THROWN_FLAG;
                *(f->out) = temp;
                NOTE_THROWING(goto return_indexor);
            }
        }

        // We did not pass in a symbol, so not a call... hence we cannot
        // process refinements.  Should not get any back.
        //
        assert(DSP == f->dsp_orig);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [GET-PATH!]
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_GET_PATH:
        //
        // returns in word the path item, DS_TOP has value
        //
        if (Do_Path_Throws(f->out, NULL, f->value, NULL)) {
            f->indexor = THROWN_FLAG;
            NOTE_THROWING(goto return_indexor);
        }

        // We did not pass in a symbol ID
        //
        assert(DSP == f->dsp_orig);
        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [LIT-PATH!]
//
// We only set the type, in order to preserve the header bits... (there
// currently aren't any for ANY-PATH!, but there might be someday.)
//
// !!! Aliases a REBSER under two value types, likely bad, see #2233
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_LIT_PATH:
        QUOTE_NEXT_REFETCH(f->out, f);
        VAL_SET_TYPE_BITS(f->out, REB_PATH);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [FUNCTION!]
//
// If a function makes it to the SWITCH statement, that means it is either
// literally a function value in the array (`do compose [(:+) 1 2]`) or is
// being retriggered via EVAL.  Note that infix functions that are
// encountered in this way will behave as prefix--their infix behavior
// is only triggered when they are looked up from a word.  See #1934.
//
// Most function evaluations are triggered from a SWITCH on a WORD! or PATH!,
// which jumps in at the `do_function_in_value` label.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_FUNCTION:
        //
        // Note: Because this is a function value being hit literally in
        // a block, no word was used to get it, so its name is unknown.
        //
        if (f->opt_label_sym == SYM_0)
            f->opt_label_sym = SYM___ANONYMOUS__;

    do_function_in_value:
        //
        // `do_function_in_value` expects the function to be in f->value,
        // and if it's a definitional return we need to extract its target.
        // (the REBVAL you get from FUNC_VALUE() does not have the exit_from
        // poked into it.)
        //
        // Note that you *can* have a 'literal' definitional return value,
        // because the user can compose it into a block like any function.
        //
        assert(IS_FUNCTION(f->value));
        f->func = VAL_FUNC(f->value);

        // A label symbol should always be put in place for a function
        // dispatch by this point, even if it's just "anonymous".  Cache a
        // string for it to be friendlier in the C debugging watchlist.
        //
    #if !defined(NDEBUG)
        assert(f->opt_label_sym != SYM_0);
        f->label_str = cast(const char*, Get_Sym_Name(f->opt_label_sym));
    #endif

        // There may be refinements pushed to the data stack to process, if
        // the call originated from a path dispatch.
        //
        assert(DSP >= f->dsp_orig);

        // We reset the lookahead_flags here to do a lookahead regardless
        // of what was passed in by the caller.  The reason is that each
        // level of function dispatch resets it.  Consider:
        //
        //     >> "1" = mold 2 - 1
        //
        // mold is not infix.  Hence while it is acquiring its arguments
        // that needs to have lookahead.
        //
        // This means that the caller can only control lookahead at the
        // granularity of the DO/NEXT points; it will be dictated by the
        // function itself at each level after that.  Note that when an
        // infix function is found after the loop, it jumps in lower than
        // this point to do the execution, so its change to lookahead is
        // not overwritten by this.
        //
        f->lookahead_flags = DO_FLAG_LOOKAHEAD;

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! EVAL HANDLING
    //
    //==////////////////////////////////////////////////////////////////==//

        // The EVAL "native" is unique because it cannot be a function that
        // runs "under the evaluator"...because it *is the evaluator itself*.
        // Hence it is handled in a special way.
        //
        if (f->func == PG_Eval_Func) {
            FETCH_NEXT_ONLY_MAYBE_END(f);

            if (f->indexor == END_FLAG) // e.g. `do [eval]`
                fail (Error_No_Arg(FRM_LABEL(f), FUNC_PARAM(PG_Eval_Func, 1)));

            // "DO/NEXT" full expression into the `eval` REBVAR slot
            // (updates index...).  (There is an /ONLY switch to suppress
            // normal evaluation but it does not apply to the value being
            // retriggered itself, just any arguments it consumes.)
            //
            VAL_INIT_WRITABLE_DEBUG(&f->cell.eval);
            DO_NEXT_REFETCH_MAY_THROW(&f->cell.eval, f, f->lookahead_flags);

            if (f->indexor == THROWN_FLAG)
                NOTE_THROWING(goto return_indexor);

            // There's only one refinement to EVAL and that is /ONLY.  It can
            // push one refinement to the stack or none.  The state will
            // twist up the evaluator for the next evaluation only.
            //
            if (DSP > f->dsp_orig) {
                assert(DSP == f->dsp_orig + 1);
                assert(VAL_WORD_SYM(DS_TOP) == SYM_ONLY); // canonized on push
                DS_DROP;
                f->args_evaluate = FALSE;
            }
            else
                f->args_evaluate = TRUE;

            // Jumping to the `reevaluate:` label will skip the fetch from the
            // array to get the next `value`.  So seed it with the address of
            // eval result, and step the index back by one so the next
            // increment will get our position sync'd in the block.
            //
            // If there's any reason to be concerned about the temporary
            // item being GC'd, it should be taken care of by the implicit
            // protection from the Do Stack.  (e.g. if it contains a function
            // that gets evaluated it will wind up in f->func, if it's a
            // GROUP! or PATH!-containing-GROUP! it winds up in f->array...)
            //
            // Note that we may be at the end (which would usually be a NULL
            // case for f->value) but we are splicing in eval over that,
            // which keeps the switch from crashing.
            //
            if (f->value)
                f->eval_fetched = f->value;
            else
                f->eval_fetched = END_VALUE; // NULL means no eval_fetched :-/

            f->value = &f->cell.eval;
            goto reevaluate; // we don't move index!
        }

    //==////////////////////////////////////////////////////////////////==//
    //
    // DEFINITIONAL RETURN EXTRACTION
    //
    //==////////////////////////////////////////////////////////////////==//

        // At this point `f->value` is still good because we have not
        // advanced the input.  We extract the special exit_from property
        // contained in optimized definitional returns.
        //
        if (f->func == PG_Leave_Func) {
            f->exit_from = VAL_FUNC_EXIT_FROM(f->value);
            goto do_definitional_exit_from;
        }

        if (f->func == PG_Return_Func)
            f->exit_from = VAL_FUNC_EXIT_FROM(f->value);
        else
            f->exit_from = NULL;

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! NORMAL ARGUMENT FULFILLMENT PROCESS
    //
    //==////////////////////////////////////////////////////////////////==//

        // Depending on the <durable> settings of a function's arguments, they
        // may wind up resident in stack space or in dynamically allocated
        // space.  This sets up the memory as appropriate for the flags.
        //
        Push_Or_Alloc_Vars_For_Call(f);

        // If it's a specialization, we've already taken care of what we
        // needed to know from that specialization--all further references
        // will need to talk about the function which is being called.
        //
        // !!! For debugging, it would probably be desirable to indicate
        // that this call of the function originated from a specialization.
        // So that would mean saving the specialization's f->func somewhere.
        //
        if (FUNC_CLASS(f->func) == FUNC_CLASS_SPECIALIZED) {
            f->func = CTX_FRAME_FUNC(
                FUNC_VALUE(f->func)->payload.function.impl.special
            );
            f->flags |= DO_FLAG_EXECUTE_FRAME;
        }

        // Advance the input, which loses our ability to inspect the function
        // value further.  Note we are allowed to be at a END_FLAG (such
        // as if the function has no arguments, or perhaps its first argument
        // is hard quoted as HELP's is and it can accept that.)
        //
        FETCH_NEXT_ONLY_MAYBE_END(f);

        // We assume you can enumerate both the formal parameters (in the
        // spec) and the actual arguments (in the call frame) using pointer
        // incrementation, that they are both terminated by END, and
        // that there are an equal number of values in both.
        //
        f->param = FUNC_PARAMS_HEAD(f->func);

        // Since we know we're not going to just overwrite it, go ahead and
        // grab the arg head.  While fulfilling arguments the GC might be
        // invoked, so we have to initialize `refine` to something too...
        //
        f->arg = FRM_ARGS_HEAD(f);

    do_function_arglist_in_progress:
        //
        // f->out may have either contained the infix argument (if jumped in)
        // or if this was a fresh loop iteration, the debug build had
        // set f->out to a safe trash.  Using the statistical technique
        // again, we mimic the release build behavior of trust *half* the
        // time, and put in a trapping trash the other half...
        //
    #if !defined(NDEBUG)
        if (SPORADICALLY(2))
            SET_TRASH_SAFE(f->out);
    #endif

        // While fulfilling arguments the GC might be invoked, and it may
        // examine subfeed (which could be set during argument acquisition)
        //
        f->cell.subfeed = NULL;

        // This loop goes through the parameter and argument slots.  Ordinary
        // calls have all the arguments initialized to BAR!, indicating they
        // are unspecialized--so they are acquired from the callsite.  Partial
        // specializations can use BAR! as well, but with other values
        // pre-existing in arg slots being soft-quoted as the value to use.

        f->mode = CALL_MODE_ARGS;

        f->refine = TRUE_VALUE; // "not a refinement arg, evaluate normally"

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! NORMAL ARGUMENT FULFILLMENT LOOP
    //
    //==////////////////////////////////////////////////////////////////==//

        // This loop goes through the parameter and argument slots.  Based on
        // the parameter type, it may be necessary to "consume" an expression
        // from values that come after the invokation point.  But not all
        // params will consume arguments for all calls.  See notes below.
        //
        // For this one body of code to be able to handle both function
        // specialization and ordinary invocation, the BAR! type is used as
        // a signal to have "unspecialized" behavior.  Hence a normal call
        // just pre-fills all the args with BAR!--which will be overwritten
        // during the argument fulfillment process.
        //
        // It is mostly straightforward, but notice that refinements are
        // somewhat tricky.  These two calls mean different things:
        //
        //     foo: func [a /b c /d e] [...]
        //
        //     foo/b/d (1 + 2) (3 + 4) (5 + 6)
        //     foo/d/b (1 + 2) (3 + 4) (5 + 6)
        //
        // The order of refinements in the definition (b d) might not match
        // what order the refinements are invoked in the path.  This means
        // the "visitation order" of the parameters while walking across
        // parameters in the array might not match the "consumption order"
        // of the expressions that are being fetched from the callsite.
        //
        // To get around that, there's a trick.  An out-of-order refinement
        // makes a note in the stack about a parameter and arg position that
        // it sees that it will need to come back to.  It pokes those two
        // pointers into extra space in the refinement's word on the stack,
        // since that word isn't using its binding.  See WORD_FLAG_PICKUP for
        // the type of WORD! that is used to implement this.
        //
        for (; NOT_END(f->param); ++f->param, ++f->arg) {
            assert(IS_TYPESET(f->param));
            pclass = VAL_PARAM_CLASS(f->param);

            if (pclass == PARAM_CLASS_REFINEMENT) {

                if (f->mode == CALL_MODE_REFINEMENT_PICKUP)
                    break; // pickups finished when another refinement is hit

                if (IS_BAR(f->arg)) {

    //=//// UNSPECIALIZED REFINEMENT SLOT (no consumption) ////////////////=//

                    if (f->dsp_orig == DSP) { // no refinements left on stack
                        SET_NONE(f->arg);
                        f->refine = NONE_VALUE; // "don't consume args, ever"
                        goto continue_arg_loop;
                    }

                    f->refine = DS_TOP;

                    if (
                        VAL_WORD_SYM(f->refine)
                        == SYMBOL_TO_CANON(VAL_TYPESET_SYM(f->param)) // #2258
                    ) {
                        DS_DROP; // we're lucky: this was next refinement used

                        MARK_REFINEMENT_USED(f->arg, f->param);
                        f->refine = f->arg; // "consume args (can be revoked)"
                        goto continue_arg_loop;
                    }

                    --f->refine; // not lucky: if in use, this is out of order

                    for (; f->refine > DS_AT(f->dsp_orig); --f->refine) {
                        if (
                            VAL_WORD_SYM(f->refine) // canonized when pushed
                            == SYMBOL_TO_CANON(
                                VAL_TYPESET_SYM(f->param) // #2258
                            )
                        ) {
                            // The call uses this refinement but we'll have to
                            // come back to it when the expression index to
                            // consume lines up.  Make a note of the param
                            // and arg and poke them into the stack WORD!.
                            //
                            UNBIND_WORD(f->refine);
                            SET_VAL_FLAG(f->refine, WORD_FLAG_PICKUP);
                            f->refine->payload.any_word.place.pickup.param
                                = f->param;
                            f->refine->payload.any_word.place.pickup.arg
                                = f->arg;

                            MARK_REFINEMENT_USED(f->arg, f->param);
                            f->refine = UNSET_VALUE; // "consume args later"
                            goto continue_arg_loop;
                        }
                    }

                    // Wasn't in the path and not specialized, so not present
                    //
                    SET_NONE(f->arg);
                    f->refine = NONE_VALUE; // "don't consume args, ever"
                    goto continue_arg_loop;
                }

    //=//// SPECIALIZED REFINEMENT SLOT (no consumption) //////////////////=//

                if (f->args_evaluate && IS_QUOTABLY_SOFT(f->arg)) {
                    //
                    // Needed for `(copy [1 2 3])`, active specializations

                    if (DO_VALUE_THROWS(f->out, f->arg)) {
                        DS_DROP_TO(f->dsp_orig);
                        f->indexor = THROWN_FLAG;
                        NOTE_THROWING(goto drop_call_and_return_thrown);
                    }

                    *(f->arg) = *(f->out);
                }

                // If any TRUE? value we consider the refinement used, but
                // UNSET! is neither conditionally true nor false
                //
                if (IS_UNSET(f->arg))
                    fail (Error_Arg_Type(
                        FRM_LABEL(f), f->param, Type_Of(f->arg))
                    );

                if (IS_CONDITIONAL_TRUE(f->arg)) {
                    Val_Init_Word(
                        f->arg, REB_WORD, VAL_TYPESET_SYM(f->param)
                    );
                    f->refine = f->arg; // remember so we can revoke!
                }
                else {
                    SET_NONE(f->arg);
                    f->refine = NONE_VALUE; // (read-only)
                }

                goto continue_arg_loop;
            }

    //=//// IF JUST SKIPPING TO NEXT REFINEMENT, MOVE ON //////////////////=//

            if (IS_UNSET(f->refine))
                goto continue_arg_loop;

    //=//// PURE "LOCAL:" ARG (must be unset, no consumption) /////////////=//

            if (pclass == PARAM_CLASS_PURE_LOCAL) {

                if (IS_BAR(f->arg)) { // no specialization (common case)
                    SET_UNSET(f->arg);
                    goto continue_arg_loop;
                }

                if (IS_UNSET(f->arg)) // the only legal specialized value
                    goto continue_arg_loop;

                fail (Error_Local_Injection(FRM_LABEL(f), f->param));
            }

    //=//// SPECIALIZED ARG (already filled, so does not consume) /////////=//

            if (NOT(IS_BAR(f->arg))) {

                // The arg came preloaded with a value to use.  Handle soft
                // quoting first, in case arg needs evaluation.

                if (f->args_evaluate && IS_QUOTABLY_SOFT(f->arg)) {

                    if (DO_VALUE_THROWS(f->out, f->arg)) {
                        DS_DROP_TO(f->dsp_orig);
                        f->indexor = THROWN_FLAG;
                        NOTE_THROWING(goto drop_call_and_return_thrown);
                    }

                    *(f->arg) = *(f->out);
                }

                // Varargs are special, because the type checking doesn't
                // actually check the type of the parameter--it's always
                // a VARARGS!.  Also since the "types accepted" are a lie
                // (an [integer! <...>] takes VARARGS!, not INTEGER!) then
                // an "honest" parameter has to be made to give the error.
                //
                if (
                    IS_CONDITIONAL_TRUE(f->refine) // not unused or revoking
                    && GET_VAL_FLAG(f->param, TYPESET_FLAG_VARIADIC)
                ) {
                    if (!IS_VARARGS(f->arg)) {
                        REBVAL honest_param;
                        VAL_INIT_WRITABLE_DEBUG(&honest_param);

                        Val_Init_Typeset(
                            &honest_param,
                            FLAGIT_KIND(REB_VARARGS), // *actually* expected...
                            VAL_TYPESET_SYM(f->param)
                        );

                        fail (Error_Arg_Type(
                            FRM_LABEL(f), &honest_param, Type_Of(f->arg))
                        );
                    }

                    // !!! Passing the varargs through directly does not
                    // preserve the type checking or symbol.  This suggests
                    // that even array-based varargs frames should have
                    // an optional frame and parameter.  Consider specializing
                    // variadics to be TBD until the type checking issue
                    // is sorted out.
                    //
                    assert(FALSE);

                    goto continue_arg_loop;
                }

                goto check_arg; // normal checking, handles errors also
            }

    //=//// IF UNSPECIALIZED ARG IS INACTIVE, SET UNSET AND MOVE ON ///////=//

            // Unspecialized arguments that do not consume do not need any
            // further processing or checking.  UNSET! will always be fine.
            //
            if (IS_NONE(f->refine)) { // FALSE if revoked, and still evaluates
                SET_UNSET(f->arg);
                goto continue_arg_loop;
            }

    //=//// VARIADIC ARG (doesn't consume anything *yet*) /////////////////=//

            // Evaluation argument "hook" parameters (signaled in MAKE FUNCION!
            // by a `|` in the typeset, and in FUNC by `<...>`).  They point
            // back to this call through a reified FRAME!, and are able to
            // consume additional arguments during the function run.
            //
            if (GET_VAL_FLAG(f->param, TYPESET_FLAG_VARIADIC)) {
                //
                // !!! Can EVAL/ONLY be supported by variadics?  What would
                // it mean?  It generally means that argument fulfillment will
                // ignore the quoting settings, if that's all it is then
                // the varargs needs to have this flag communicated...but
                // then should it function variadically anyway?
                //
                assert(f->args_evaluate);

                VAL_RESET_HEADER(f->arg, REB_VARARGS);

                VAL_VARARGS_FRAME_CTX(f->arg)
                    = Context_For_Frame_May_Reify(f, NULL, FALSE);
                ENSURE_ARRAY_MANAGED(
                    CTX_VARLIST(VAL_VARARGS_FRAME_CTX(f->arg))
                );

                VAL_VARARGS_PARAM(f->arg) = f->param; // type checks on TAKE

                assert(f->cell.subfeed == NULL); // NULL earlier in switch case
                goto continue_arg_loop;
            }

    //=//// AFTER THIS, PARAMS CONSUME--ERROR ON END MARKER, BAR! ////////=//

            // Note that if a function has a quoted argument whose types
            // permit unset, then hitting the end of expressions to consume
            // is allowed, in order to implement console commands like HELP
            // (which acts as arity 1 or 0, using this trick)
            //
            //     >> foo: func [:a [unset!]] [
            //         if unset? :a ["special allowance"]
            //     ]
            //
            //     >> do [foo]
            //     == "special allowance"

            if (f->indexor == END_FLAG) {
                if (pclass == PARAM_CLASS_NORMAL)
                    fail (Error_No_Arg(FRM_LABEL(f), f->param));

                assert(
                    pclass == PARAM_CLASS_HARD_QUOTE
                    || pclass == PARAM_CLASS_SOFT_QUOTE
                );

                if (!TYPE_CHECK(f->param, REB_UNSET))
                    fail (Error_No_Arg(FRM_LABEL(f), f->param));

                SET_UNSET(f->arg);
                goto continue_arg_loop;
            }

            // Literal expression barriers cannot be consumed in normal
            // evaluation, even if the argument takes a BAR!.  It must come
            // through non-literal means(e.g. `quote '|` or `first [|]`)
            //
            if (f->args_evaluate && IS_BAR(f->value))
                fail (Error(RE_EXPRESSION_BARRIER));

    //=//// REGULAR ARG-OR-REFINEMENT-ARG (consumes a DO/NEXT's worth) ////=//

            if (pclass == PARAM_CLASS_NORMAL) {
                if (f->args_evaluate) {
                    DO_NEXT_REFETCH_MAY_THROW(f->arg, f, f->lookahead_flags);

                    if (f->indexor == THROWN_FLAG) {
                        *(f->out) = *(f->arg);

                        // If we have refinements pending on the data
                        // stack we need to balance those...
                        //
                        DS_DROP_TO(f->dsp_orig);

                        NOTE_THROWING(goto drop_call_and_return_thrown);
                    }
                }
                else
                    QUOTE_NEXT_REFETCH(f->arg, f);

                goto check_arg;
            }

    //=//// QUOTED ARG-OR-REFINEMENT-ARG (HARD OR SOFT QUOTE) /////////////=//

            if (
                pclass == PARAM_CLASS_SOFT_QUOTE
                && f->args_evaluate // it's not an EVAL/ONLY
                && IS_QUOTABLY_SOFT(f->value)
            ) {
                if (DO_VALUE_THROWS(f->out, f->value)) {
                    DS_DROP_TO(f->dsp_orig);
                    f->indexor = THROWN_FLAG;
                    NOTE_THROWING(goto drop_call_and_return_thrown);
                }

                *(f->arg) = *(f->out);
                FETCH_NEXT_ONLY_MAYBE_END(f);
                goto continue_arg_loop;
            }

            // This is either not one of the "soft quoted" cases, or
            // "hard quoting" was explicitly used with GET-WORD!:

            assert(
                pclass == PARAM_CLASS_HARD_QUOTE
                || pclass == PARAM_CLASS_SOFT_QUOTE
            );

            QUOTE_NEXT_REFETCH(f->arg, f);

    //=//// TYPE CHECKING FOR (MOST) ARGS AT END OF ARG LOOP //////////////=//

            // Some arguments can be fulfilled and skip type checking or
            // take care of it themselves.  But normal args pass through
            // this code which checks the typeset and also handles it when
            // an UNSET! arg signals the revocation of a refinement usage.

        check_arg:
            ASSERT_VALUE_MANAGED(f->arg);
            assert(pclass != PARAM_CLASS_REFINEMENT);
            assert(pclass != PARAM_CLASS_PURE_LOCAL);

            // See notes on `Reb_Frame.refine` in %sys-do.h for more info.
            //
            assert(
                IS_NONE(f->refine) || // arg to unused refinement
                IS_LOGIC(f->refine) || // F = revoked, T = not refinement arg
                IS_WORD(f->refine) // refinement arg in use, but revokable
            );

            if (IS_UNSET(f->arg)) {
                if (IS_WORD(f->refine)) {
                    //
                    // We can only revoke the refinement if this is the 1st
                    // refinement arg.  If it's a later arg, then the first
                    // didn't trigger revocation, or refine wouldn't be WORD!
                    //
                    if (f->refine + 1 != f->arg)
                        fail (Error(RE_BAD_REFINE_REVOKE));

                    SET_NONE(f->refine);
                    f->refine = FALSE_VALUE;
                }

                if (IS_CONDITIONAL_FALSE(f->refine))
                    goto continue_arg_loop; // don't type check revoked/unused
            }
            else {
                // If the argument is set, then the refinement shouldn't be
                // in a revoked or unused state.
                //
                if (IS_CONDITIONAL_FALSE(f->refine))
                    fail (Error(RE_BAD_REFINE_REVOKE));
            }

            if (!TYPE_CHECK(f->param, VAL_TYPE(f->arg)))
                fail (Error_Arg_Type(FRM_LABEL(f), f->param, Type_Of(f->arg)));

        continue_arg_loop: // `continue` might bind to the wrong scope
            NOOP;
        }

        // There may have been refinements that were skipped because the
        // order of definition did not match the order of usage.  They were
        // left on the stack with a pointer to the `param` and `arg` after
        // them for later fulfillment.
        //
        if (DSP != f->dsp_orig) {
            if (!GET_VAL_FLAG(DS_TOP, WORD_FLAG_PICKUP)) {
                //
                // The walk through the arguments didn't fill in any
                // information for this word, so it was either a duplicate of
                // one that was fulfilled or not a refinement the function
                // has at all.
                //
                fail (Error(RE_BAD_REFINE, DS_TOP));
            }
            f->param = DS_TOP->payload.any_word.place.pickup.param;
            f->refine = f->arg = DS_TOP->payload.any_word.place.pickup.arg;
            assert(IS_WORD(f->refine));
            DS_DROP;
            f->mode = CALL_MODE_REFINEMENT_PICKUP;
            goto continue_arg_loop; // leaves refine, but bumps param+arg
        }

        //
        // Execute the function with all arguments ready.
        //
    #if !defined(NDEBUG)
        if (GET_VAL_FLAG(FUNC_VALUE(f->func), FUNC_FLAG_LEGACY)) {
            //
            // OPTIONS_REFINEMENTS_TRUE was set when this particular function
            // was created.  Use the debug-build's legacy post-processing
            // so refinements and their args work like in Rebol2/R3-Alpha.
            //
            Legacy_Convert_Function_Args_Debug(f);
        }
    #endif

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! THROWING OF "RETURN" + "LEAVE" DEFINITIONAL EXITs
    //
    //==////////////////////////////////////////////////////////////////==//

        if (f->exit_from) {
        do_definitional_exit_from:
            //
            // If it's a definitional return, then we need to do the throw
            // for the return, named by the value in the exit_from.  This
            // should be the RETURN native with 1 arg as the function, and
            // the native code pointer should have been replaced by a
            // REBFUN (if function) or REBCTX (if durable) to jump to.
            //
            // !!! Long term there will always be frames for user functions
            // where definitional returns are possible, but for now they
            // still only make them by default if <durable> requested)
            //
            // LEAVE jumps directly here, because it doesn't need to go
            // through any parameter evaluation.  (Note that RETURN can't
            // simply evaluate the next item without inserting an opportunity
            // for the debugger, e.g. `return (breakpoint)`...)
            //
            ASSERT_ARRAY(f->exit_from);

            // We only have a REBARR*, but want to actually THROW a full
            // REBVAL (FUNCTION! or FRAME! if it has a context) which matches
            // the paramlist.  In either case, the value comes from slot [0]
            // of the RETURN_FROM array, but in the debug build do an added
            // sanity check.
            //
            if (GET_ARR_FLAG(f->exit_from, ARRAY_FLAG_CONTEXT_VARLIST)) {
                //
                // Request to exit from a specific FRAME!
                //
                *(f->out) = *CTX_VALUE(AS_CONTEXT(f->exit_from));
                assert(IS_FRAME(f->out));
                assert(CTX_VARLIST(VAL_CONTEXT(f->out)) == f->exit_from);
            }
            else {
                // Request to dynamically exit from first ANY-FUNCTION! found
                // that has a given parameter list
                //
                *(f->out) = *FUNC_VALUE(AS_FUNC(f->exit_from));
                assert(IS_FUNCTION(f->out));
                assert(VAL_FUNC_PARAMLIST(f->out) == f->exit_from);
            }

            f->indexor = THROWN_FLAG;

            if (f->func == PG_Leave_Func) {
                //
                // LEAVE never created an arglist, so it doesn't have to
                // free one.  Also, it wants to just return UNSET!
                //
                CONVERT_NAME_TO_THROWN(f->out, UNSET_VALUE, TRUE);
                NOTE_THROWING(goto return_indexor);
            }

            // On the other hand, RETURN did make an arglist that has to be
            // dropped from the chunk stack.
            //
            assert(FUNC_NUM_PARAMS(f->func) == 1);
            CONVERT_NAME_TO_THROWN(f->out, FRM_ARGS_HEAD(f), TRUE);
            NOTE_THROWING(goto drop_call_and_return_thrown);
        }

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! ARGUMENTS NOW GATHERED, DISPATCH CALL
    //
    //==////////////////////////////////////////////////////////////////==//

        assert(DSP == f->dsp_orig);

        // Although the Make_Call wrote safe trash into the output slot, we
        // need to do it again for the dispatch, since the spots are used to
        // do argument fulfillment into.
        //
        SET_TRASH_SAFE(f->out);

        // param, refine, and args should be valid and safe for GC here

        // Now we reset arg to the head of the argument list.  This provides
        // fast access for the callees, so they don't have to go through an
        // indirection further than just f->arg to get it.
        //
        // !!! When hybrid frames are introduced, review the question of
        // which pointer "wins".  Might more than one be used?
        //
        if (f->flags & DO_FLAG_FRAME_CONTEXT) {
            //
            // !!! Here this caches a dynamic series data pointer in arg.
            // For arbitrary series this is not legal to do, because a
            // resize could relocate it...but we know the argument list will
            // not expand in the current implementation.  However, a memory
            // compactor would likely prefer that there not be too many
            // locked series in order to more freely rearrange memory, so
            // this is a tradeoff.
            //
            assert(GET_ARR_FLAG(
                AS_ARRAY(f->data.context), SERIES_FLAG_FIXED_SIZE
            ));
            f->arg = CTX_VARS_HEAD(f->data.context);
        }
        else {
            // We cache the stackvars data pointer in the stack allocated
            // case.  Note that even if the frame becomes "reified" as a
            // context, the data pointer will be the same over the stack
            // level lifetime.
            //
            f->arg = &f->data.stackvars[0];
        }

        // If the function has a native-optimized version of definitional
        // return, the local for this return should so far have just been
        // ensured in last slot...and left unset by the arg filling.
        //
        // Now fill in the var for that local with a "hacked up" native
        // Note that FUNCTION! uses its PARAMLIST as the RETURN_FROM
        // usually, but not if it's reusing a frame.
        //
        if (GET_VAL_FLAG(FUNC_VALUE(f->func), FUNC_FLAG_LEAVE_OR_RETURN)) {
            f->param = FUNC_PARAM(
                f->func, VAL_FUNC_NUM_PARAMS(FUNC_VALUE(f->func))
            );
            f->refine = FRM_ARG(f, VAL_FUNC_NUM_PARAMS(FUNC_VALUE(f->func)));

            assert(VAL_PARAM_CLASS(f->param) == PARAM_CLASS_PURE_LOCAL);
            assert(IS_UNSET(f->refine));

            if (VAL_TYPESET_CANON(f->param) == SYM_RETURN)
                *(f->refine) = *ROOT_RETURN_NATIVE;
            else {
                assert(VAL_TYPESET_CANON(f->param) == SYM_LEAVE);
                *(f->refine) = *ROOT_LEAVE_NATIVE;
            }

            // !!! Having to pick a function paramlist or a context for
            // definitional return (and doubly testing this flag) is a likely
            // temporary state of affairs, as all functions able to have a
            // definitional return will have contexts in NewFunction.
            //
            if (f->flags & DO_FLAG_FRAME_CONTEXT)
                VAL_FUNC_EXIT_FROM(f->refine) = CTX_VARLIST(f->data.context);
            else
                VAL_FUNC_EXIT_FROM(f->refine) = FUNC_PARAMLIST(f->func);
        }

        if (Trace_Flags) Trace_Func(FRM_LABEL(f), FUNC_VALUE(f->func));

        assert(f->indexor != THROWN_FLAG);

        f->mode = CALL_MODE_FUNCTION;

        // If the Do_XXX_Core function dispatcher throws, we can't let it
        // write `f->indexor` directly to become THROWN_FLAG because we may
        // "recover" from the throw by realizing it was a RETURN.  If that
        // is the case, the function we called is the one that returned...
        // so there could still be code after it to execute, and that index
        // will be needed.
        //
        // Rather than have a separate `REBOOL threw`, this goes ahead and
        // overwrites `f->mode` with a special state DO_MODE_THROWN.  It was
        // going to need to be updated anyway back to DO_MODE_0, so no harm
        // in reusing it for the indicator.
        //
        if (PG_Func_Profiler != NULL) {
            f->eval_time = OS_DELTA_TIME(0, 0);
            Func_Profile_Start(f);
        }
        switch (VAL_FUNC_CLASS(FUNC_VALUE(f->func))) {
        case FUNC_CLASS_NATIVE:
            Do_Native_Core(f);
            break;

        case FUNC_CLASS_ACTION:
            Do_Action_Core(f);
            break;

        case FUNC_CLASS_COMMAND:
            Do_Command_Core(f);
            break;

        case FUNC_CLASS_CALLBACK:
        case FUNC_CLASS_ROUTINE:
            Do_Routine_Core(f);
            break;

        case FUNC_CLASS_USER:
            Do_Function_Core(f);
            break;

        case FUNC_CLASS_SPECIALIZED:
            //
            // Shouldn't get here--the specific function type should have been
            // extracted from the frame to use.
            //
            assert(FALSE);
            break;

        default:
            fail (Error(RE_MISC));
        }
        if (PG_Func_Profiler != NULL) {
            f->eval_time = OS_DELTA_TIME(f->eval_time, 0);
            Func_Profile_End(f);
        }

    #if !defined(NDEBUG)
        assert(
            f->mode == CALL_MODE_FUNCTION
            || f->mode == CALL_MODE_THROW_PENDING
        );
        assert(THROWN(f->out) == LOGICAL(f->mode == CALL_MODE_THROW_PENDING));
    #endif

    drop_call_and_return_thrown:
        //
        // The same label is currently used for both these outcomes, and
        // which happens depends on whether eval_fetched is NULL or not
        //
        if (f->flags & DO_FLAG_FRAME_CONTEXT) {
            if (CTX_STACKVARS(f->data.context) != NULL)
                Drop_Chunk(CTX_STACKVARS(f->data.context));

            if (GET_ARR_FLAG(
                CTX_VARLIST(f->data.context), SERIES_FLAG_MANAGED
            )) {
                // Context at some point became managed and hence may still
                // have outstanding references.  The accessible flag should
                // have been cleared by the drop chunk above.
                //
                assert(
                    !GET_ARR_FLAG(
                        CTX_VARLIST(f->data.context), SERIES_FLAG_ACCESSIBLE
                    )
                );
            }
            else {
                // If nothing happened that might have caused the context to
                // become managed (e.g. Val_Init_Word() using it or a
                // Val_Init_Object() for the frame) then the varlist can just
                // go away...
                //
                Free_Array(CTX_VARLIST(f->data.context));
                //
                // NOTE: Even though we've freed the pointer, we still compare
                // it for identity below when checking to see if this was the
                // stack level being thrown to!
            }
        }
        else
            Drop_Chunk(f->data.stackvars);

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! CATCHING OF EXITs (includes catching RETURN + LEAVE)
    //
    //==////////////////////////////////////////////////////////////////==//

        // A definitional return should only be intercepted if it was for this
        // particular function invocation.  Definitional return abilities have
        // been extended to natives and actions, in order to permit stack
        // control in debug situations (and perhaps some non-debug capabilities
        // will be discovered as well).
        //
        if (
            f->mode == CALL_MODE_THROW_PENDING
            && GET_VAL_FLAG(f->out, VALUE_FLAG_EXIT_FROM)
        ) {
            if (IS_FRAME(f->out)) {
                //
                // This identifies an exit from a *specific* functiion
                // invocation.  We can only match it if we have a reified
                // frame context.
                //
                if (
                    (f->flags & DO_FLAG_FRAME_CONTEXT) &&
                    VAL_CONTEXT(f->out) == AS_CONTEXT(f->data.context)
                ) {
                    CATCH_THROWN(f->out, f->out);
                    f->mode = CALL_MODE_GUARD_ARRAY_ONLY;
                }
            }
            else if (IS_FUNCTION(f->out)) {
                //
                // This identifies an exit from whichever instance of the
                // function is most recent on the stack.  This can be used
                // to exit without reifying a frame.  If exiting dynamically
                // when all that was named was a function, but definitionally
                // scoped returns should ideally have a trick for having
                // the behavior of a reified frame without needing to do
                // so (for now, they use this path in FUNCTION!)
                //
                if (VAL_FUNC_PARAMLIST(f->out) == FUNC_PARAMLIST(f->func)) {
                    CATCH_THROWN(f->out, f->out);
                    f->mode = CALL_MODE_GUARD_ARRAY_ONLY;
                }
            }
            else if (IS_INTEGER(f->out)) {
                //
                // If it's an integer, we drop the value at each stack level
                // until 1 is reached...
                //
                if (VAL_INT32(f->out) == 1) {
                    CATCH_THROWN(f->out, f->out);
                    f->mode = CALL_MODE_GUARD_ARRAY_ONLY;
                }
                else {
                    // don't reset header (keep thrown flag as is), just bump
                    // the count down by one...
                    //
                    --VAL_INT64(f->out);
                    //
                    // ...and stay in thrown mode...
                }
            }
            else {
                assert(FALSE); // no other low-level EXIT/FROM supported
            }
        }

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! CALL COMPLETION (Type Check Result, Throw If Needed)
    //
    //==////////////////////////////////////////////////////////////////==//

        // If running a frame execution then clear that flag out.
        //
        f->flags &= ~DO_FLAG_EXECUTE_FRAME;

    #if !defined(NDEBUG)
        //
        // No longer need to check f->data.context for thrown status if it
        // was used, so overwrite the dead pointer in the union.  Note there
        // are two entry points to Push_Or_Alloc_Vars_For_Call at the moment,
        // so this clearing can't be done by the debug routine at top of loop.
        //
        f->data.stackvars = NULL;
    #endif

        // If the throw wasn't intercepted as an exit from this function call,
        // accept the throw.  We only care about the mode getting set cleanly
        // back to CALL_MODE_GUARD_ARRAY_ONLY if evaluation continues...
        //
        if (f->mode == CALL_MODE_THROW_PENDING) {
            f->indexor = THROWN_FLAG;
            NOTE_THROWING(goto return_indexor);
        }
        else if (f->indexor == THROWN_FLAG)
            NOTE_THROWING(goto return_indexor);

        // Here we know the function finished and did not throw or exit.  If
        // it has a definitional return we need to type check it--and if it
        // has a leave we have to squash whatever the last evaluative result
        // was and replace it with an UNSET!
        //
        if (GET_VAL_FLAG(FUNC_VALUE(f->func), FUNC_FLAG_LEAVE_OR_RETURN)) {
            REBVAL *last_param = FUNC_PARAM(f->func, FUNC_NUM_PARAMS(f->func));
            if (VAL_TYPESET_CANON(last_param) == SYM_LEAVE) {
                SET_UNSET(f->out);
            }
            else {
                // The type bits of the definitional return are not applicable
                // to the `return` word being associated with a FUNCTION!
                // vs. an INTEGER! (for instance).  It is where the type
                // information for the non-existent return function specific
                // to this call is hidden.
                //
                assert(VAL_TYPESET_CANON(last_param) == SYM_RETURN);
                if (!TYPE_CHECK(last_param, VAL_TYPE(f->out)))
                    fail (Error_Arg_Type(
                        SYM_RETURN, last_param, Type_Of(f->out))
                    );
            }
        }

        f->mode = CALL_MODE_GUARD_ARRAY_ONLY;

        if (Trace_Flags) Trace_Return(FRM_LABEL(f), f->out);
        break;


//==//////////////////////////////////////////////////////////////////////==//
//
// [FRAME!]
//
// If a literal FRAME! is hit in the source, then its associated function
// will be executed with the data.  By default it will act like a
// function specialization in terms of interpretation of the BAR! and
// soft quoted arguments, unless EVAL/ONLY or DO_FLAG_NO_ARGS_EVALUATE
// are used.
//
// To allow efficient applications, this does not make a copy of the FRAME!.
// It considers it to be the prebuilt content.  However, it will rectify
// any refinements to ensure they are the WORD! value or NONE!, as in the
// case of specialization...and it also will type check.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_FRAME:
        //
        // While *technically* possible that a context could be in use by more
        // than one function at a time, this is a dangerous enough idea to
        // prohibit unless some special situation arises and it's explicitly
        // said what is meant to be done.
        //
        /*if (GET_VAL_FLAG(f->value, EXT_CONTEXT_RUNNING))
           fail (Error(RE_FRAME_ALREADY_USED, f->value)); */

        if (f->opt_label_sym == SYM_0)
            f->opt_label_sym = SYM___ANONYMOUS__;

        assert(f->data.stackvars == NULL);
        f->data.context = VAL_CONTEXT(f->value);
        f->func = CTX_FRAME_FUNC(VAL_CONTEXT(f->value));

        if (GET_ARR_FLAG(
            CTX_VARLIST(VAL_CONTEXT(f->value)), CONTEXT_FLAG_STACK)
        ) {
            f->arg = VAL_CONTEXT_STACKVARS(f->value);
        }
        else
            f->arg = CTX_VARS_HEAD(VAL_CONTEXT(f->value));

        f->param = CTX_KEYS_HEAD(VAL_CONTEXT(f->value));

        f->flags |= DO_FLAG_FRAME_CONTEXT | DO_FLAG_EXECUTE_FRAME;

        f->exit_from = NULL;

        FETCH_NEXT_ONLY_MAYBE_END(f);
        goto do_function_arglist_in_progress;

//==//////////////////////////////////////////////////////////////////////==//
//
// [ ??? ] => panic
//
// All types must match a case in the switch.  This shouldn't happen.
//
//==//////////////////////////////////////////////////////////////////////==//

    default:
        panic (Error(RE_MISC));
    }

    //==////////////////////////////////////////////////////////////////==//
    //
    // END MAIN SWITCH STATEMENT
    //
    //==////////////////////////////////////////////////////////////////==//

    // There shouldn't have been any "accumulated state", in the sense that
    // we should be back where we started in terms of the data stack, the
    // mold buffer position, the outstanding manual series allocations, etc.
    //
    ASSERT_STATE_BALANCED(&f->state);

    // It's valid for the operations above to fall through after a fetch or
    // refetch that could have reached the end.
    //
    if (f->indexor == END_FLAG)
        goto return_indexor;

    // Throws should have already returned at the time of throw, by jumping
    // to the `thrown_index` label.
    //
    assert(f->indexor != THROWN_FLAG && !THROWN(f->out));

    if (f->flags & DO_FLAG_NO_LOOKAHEAD) {
        //
        // Don't do infix lookahead if asked *not* to look.  It's not typical
        // to be requested by callers (there is already no infix lookahead
        // by using DO_FLAG_EVAL_ONLY, so those cases don't need to ask.)
        //
        // However, recursive cases of DO disable infix dispatch if they are
        // currently processing an infix operation.  The currently processing
        // operation is thus given "higher precedence" by this disablement.
    }
    else {
        // Since we're not at an END, we know f->value has been prefetched,
        // so we can "peek" at it.
        //
        // If it is a WORD! that looks up to an infix function, we will use
        // the value sitting in `out` as the "left-hand-side" (parameter 1)
        // of that invocation.  (See #1934 for the resolution that literal
        // function values in the source will act as if they were prefix,
        // so word lookup is the only way to get infix behavior.)
        //
        if (IS_WORD(f->value)) {
            f->param = GET_OPT_VAR_MAY_FAIL(f->value);

            if (
                IS_FUNCTION(f->param)
                && GET_VAL_FLAG(f->param, FUNC_FLAG_INFIX)
            ) {
                f->opt_label_sym = VAL_WORD_SYM(f->value);
                f->func = VAL_FUNC(f->param);

                // The warped function values used for definitional return
                // usually need their EXIT_FROMs extracted, but here we should
                // not worry about it as neither RETURN nor LEAVE are infix
                //
                assert(f->func != PG_Leave_Func);
                assert(f->func != PG_Return_Func);
                f->exit_from = NULL;

                // We go ahead and start the vars, and put our evaluated
                // result into it as the "left-hand-side" before calling into
                // the rest of function's behavior.
                //
                Push_Or_Alloc_Vars_For_Call(f);

                // Infix functions must have at least arity 1 (exactly 2?)
                //
                assert(FUNC_NUM_PARAMS(f->func) >= 1);
                f->param = FUNC_PARAMS_HEAD(f->func);
                if (!TYPE_CHECK(f->param, VAL_TYPE(f->out)))
                    fail (Error_Arg_Type(
                        FRM_LABEL(f), f->param, Type_Of(f->out))
                    );

                // Use current `out` as first argument of the infix function
                //
                f->arg = FRM_ARGS_HEAD(f);
                *(f->arg) = *(f->out);

                ++f->param;
                ++f->arg;

                // During the argument evaluations, do not look further ahead
                //
                f->lookahead_flags = DO_FLAG_NO_LOOKAHEAD;

                FETCH_NEXT_ONLY_MAYBE_END(f);
                goto do_function_arglist_in_progress;
            }

            // Perhaps not an infix function, but we just paid for a variable
            // lookup.  If this isn't just a DO/NEXT, use the work!
            //
            if (f->flags & DO_FLAG_TO_END) {
                //
                // We need to update the `expr_index` since we are skipping
                // the whole `do_at_index` preparation for the next cycle,
                // and also need to run the "expression checks" in debug
                // builds to update the tick count and clear out state.
                //
                f->expr_index = f->indexor;
                *(f->out) = *(f->param); // param trashed by Expression_Checks

            #if !defined(NDEBUG)
                do_count = Do_Core_Expression_Checks_Debug(f);
            #endif

                if (Trace_Flags) Trace_Line(f);

                goto dispatch_the_word_in_out; // will handle the FETCH_NEXT
            }
        }

        // Note: PATH! may contain parens, which would need to be evaluated
        // during lookahead.  This could cause side-effects if the lookahead
        // fails.  Consequently, PATH! should not be a candidate for doing
        // an infix dispatch.
    }

    // Continue evaluating rest of block if not just a DO/NEXT
    //
    if (f->flags & DO_FLAG_TO_END) goto value_ready_for_do_next;

return_indexor:
    //
    // Jumping here skips the natural check that would be done after the
    // switch on the value being evaluated, so we assert balance here too.
    //
    ASSERT_STATE_BALANCED(&f->state);

#if !defined(NDEBUG)
    Do_Core_Exit_Checks_Debug(f); // will get called unless a fail() longjmps
#endif

    // Restore the top of stack (if there is a fail() and associated longjmp,
    // this restoration will be done by the Drop_Trap helper.)
    //
    TG_Frame_Stack = f->prior;
    if (TG_Frame_Stack)
        TG_Frame_Stack->next = NULL;

    // Caller needs to inspect `index`, at minimum to know if it's THROWN_FLAG
}


#if !defined(NDEBUG)

//
// The entry checks to DO are for verifying that the setup of the Reb_Frame
// passed in was valid.  They run just once for each Do_Core() call, and
// are only in the debug build.
//
static REBCNT Do_Core_Entry_Checks_Debug(struct Reb_Frame *f)
{
    // Though we can protect the value written into the target pointer 'out'
    // from GC during the course of evaluation, we can't protect the
    // underlying value from relocation.  Technically this would be a problem
    // for any series which might be modified while this call is running, but
    // most notably it applies to the data stack--where output used to always
    // be returned.
    //
    // !!! A non-contiguous data stack which is not a series is a possibility.
    //
#ifdef STRESS_CHECK_DO_OUT_POINTER
    REBSER *containing = Try_Find_Containing_Series_Debug(f->out);

    if (containing) {
        if (GET_SER_FLAG(series, SERIES_FLAG_FIXED_SIZE)) {
            //
            // Currently it's considered OK to be writing into a fixed size
            // series, for instance the durable portion of a function's
            // arg storage.  It's assumed that the memory will not move
            // during the course of the argument evaluation.
            //
        }
        else {
            Debug_Fmt("Request for ->out location in movable series memory");
            assert(FALSE);
        }
    }
#else
    assert(!IN_DATA_STACK(f->out));
#endif

    // The caller must preload ->value with the first value to process.  It
    // may be resident in the array passed that will be used to fetch further
    // values, or it may not.
    //
    assert(f->value);

    // All callers should ensure that the type isn't an END marker before
    // bothering to invoke Do_Core().
    //
    assert(NOT_END(f->value));

    // The DO_FLAGs were decided to come in pairs for clarity, to make sure
    // that each callsite of the core routines was clear on what it was
    // asking for.  This may or may not be overkill long term, but helps now.
    //
    assert(
        LOGICAL(f->flags & DO_FLAG_NEXT)
        != LOGICAL(f->flags & DO_FLAG_TO_END)
    );
    assert(
        LOGICAL(f->flags & DO_FLAG_LOOKAHEAD)
        != LOGICAL(f->flags & DO_FLAG_NO_LOOKAHEAD)
    );
    assert(
        LOGICAL(f->flags & DO_FLAG_ARGS_EVALUATE)
        != LOGICAL(f->flags & DO_FLAG_NO_ARGS_EVALUATE)
    );

    // This flag is managed solely by the frame code; shouldn't come in set
    //
    assert(NOT(f->flags & DO_FLAG_FRAME_CONTEXT));

#if !defined(NDEBUG)
    //
    // This has to be nulled out in the debug build by the code itself inline,
    // because sometimes one stackvars call ends and then another starts
    // before the debug preamble is run.  Give it an initial NULL here though.
    //
    f->data.stackvars = NULL;
#endif

    // Snapshot the "tick count" to assist in showing the value of the tick
    // count at each level in a stack, so breakpoints can be strategically
    // set for that tick based on higher levels than the value you might
    // see during a crash.
    //
    f->do_count = TG_Do_Count;
    return f->do_count;
}


//
// The iteration preamble takes care of clearing out variables and preparing
// the state for a new "/NEXT" evaluation.  It's a way of ensuring in the
// debug build that one evaluation does not leak data into the next, and
// making the code shareable allows code paths that jump to later spots
// in the switch (vs. starting at the top) to reuse the work.
//
static REBCNT Do_Core_Expression_Checks_Debug(struct Reb_Frame *f) {
    //
    // The ->mode is examined by parts of the system as a sign of whether
    // the stack represents a function invocation or not.  If it is changed
    // from CALL_MODE_GUARD_ARRAY_ONLY during an evaluation step, it must
    // be changed back before a next step is to run.
    //
    assert(f->mode == CALL_MODE_GUARD_ARRAY_ONLY);

    // If running the evaluator, then this frame should be the topmost on the
    // frame stack.
    //
    assert(f == FS_TOP);

    // We checked for END when we entered Do_Core() and short circuited
    // that, but if we're running DO_FLAG_TO_END then the catch for that is
    // an index check.  We shouldn't go back and `do_at_index` on an end!
    //
    assert(f->value && NOT_END(f->value));
    assert(f->indexor != THROWN_FLAG);

    // Note that `f->indexor` *might* be END_FLAG in the case of an eval;
    // if you write `do [eval help]` then it will load help in as f->value
    // and retrigger, and `help` (for instance) is capable of handling a
    // prefetched input that is at end.  This is different from most cases
    // where END_FLAG directly implies prefetch input was exhausted and
    // f->value must be NULL.
    //
    assert(f->indexor != END_FLAG || IS_END(f->eval_fetched));

    // The value we are processing should not be THROWN() and any series in
    // it should be under management by the garbage collector.
    //
    // !!! THROWN() bit on individual values is in the process of being
    // deprecated, in favor of the evaluator being in a "throwing state".
    //
    assert(!THROWN(f->value));
    ASSERT_VALUE_MANAGED(f->value);

    // Trash call variables in debug build to make sure they're not reused.
    // Note that this call frame will *not* be seen by the GC unless it gets
    // chained in via a function execution, so it's okay to put "non-GC safe"
    // trash in at this point...though by the time of that call, they must
    // hold valid values.
    //
    f->func = cast(REBFUN*, 0xDECAFBAD);

    if (f->opt_label_sym == SYM_0)
        f->label_str = "(no current label)";
    else
        f->label_str = cast(const char*, Get_Sym_Name(f->opt_label_sym));

    f->param = cast(REBVAL*, 0xDECAFBAD);
    f->arg = cast(REBVAL*, 0xDECAFBAD);
    f->refine = cast(REBVAL*, 0xDECAFBAD);

    f->exit_from = cast(REBARR*, 0xDECAFBAD);

    // Mutate va_list sources into arrays at fairly random moments in the
    // debug build.  It should be able to handle it at any time.
    //
    if (f->indexor == VALIST_FLAG && SPORADICALLY(50)) {
        const REBOOL truncated = TRUE;
        Reify_Va_To_Array_In_Frame(f, truncated);
    }

    // This counter is helpful for tracking a specific invocation.
    // If you notice a crash, look on the stack for the topmost call
    // and read the count...then put that here and recompile with
    // a breakpoint set.  (The 'TG_Do_Count' value is captured into a
    // local 'count' so you still get the right count after recursion.)
    //
    // We bound it at the max unsigned 32-bit because otherwise it would
    // roll over to zero and print a message that wasn't asked for, which
    // is annoying even in a debug build.
    //
    if (TG_Do_Count < MAX_U32) {
        f->do_count = ++TG_Do_Count;
        if (f->do_count == DO_COUNT_BREAKPOINT) {
            if (f->indexor == VALIST_FLAG) {
                //
                // !!! Can't fetch the next value here without destroying the
                // forward iteration.  Destructive debugging techniques could
                // be added here on demand, or non-destructive ones that
                // logged the va_list into a dynamically allocated array
                // could be put in the debug build, etc.  Add when necessary.
                //
                Debug_Fmt(
                    "Do_Core() count trap (va_list, no nondestructive fetch)"
                );
            }
            else if (f->indexor == END_FLAG) {
                assert(f->value != NULL);
                Debug_Fmt("Performing EVAL at end of array (no args)");
                PROBE_MSG(f->value, "Do_Core() count trap");
            }
            else {
                REBVAL dump;
                VAL_INIT_WRITABLE_DEBUG(&dump);

                PROBE_MSG(f->value, "Do_Core() count trap");
                Val_Init_Block_Index(
                    &dump, f->source.array, cast(REBCNT, f->indexor)
                );
                PROBE_MSG(&dump, "Do_Core() next up...");
            }
        }
    }

    return f->do_count;
}


//
// Run at the exit of Do_Core()
//
static void Do_Core_Exit_Checks_Debug(struct Reb_Frame *f) {
    if (
        f->indexor != END_FLAG && f->indexor != THROWN_FLAG
        && f->indexor != VALIST_FLAG
    ) {
        // If we're at the array's end position, then we've prefetched the
        // last value for processing (and not signaled end) but on the
        // next fetch we *will* signal an end.
        //
        assert(f->indexor <= ARR_LEN(f->source.array));
    }

    if (f->flags & DO_FLAG_TO_END)
        assert(f->indexor == THROWN_FLAG || f->indexor == END_FLAG);

    if (f->indexor == END_FLAG) {
        assert(f->value == NULL); // NULLing out value may become debug-only
        assert(NOT_END(f->out)); // series END marker shouldn't leak out
    }

    if (f->indexor == THROWN_FLAG)
        assert(THROWN(f->out));

    // Function execution should have written *some* actual output value
    // over the trash that we put in the return slot before the call.
    //
    assert(!IS_TRASH_DEBUG(f->out));
    assert(VAL_TYPE(f->out) < REB_MAX); // cheap check

    ASSERT_VALUE_MANAGED(f->out);
}

#endif
