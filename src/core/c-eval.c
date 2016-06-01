//
//  File: %c-do.c
//  Summary: "Central Interpreter Evaluator"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
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
    static REBUPT Do_Core_Entry_Checks_Debug(struct Reb_Frame *f);
    static REBUPT Do_Core_Expression_Checks_Debug(struct Reb_Frame *f);
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


// We save the index at the start of the expression in case it is needed
// for error reporting.
//
// !!! FRM_INDEX can account for prefetching, but it cannot know what a
// preloaded head value was unless it was saved under a debug> mode.
//
static inline void Start_New_Expression_Core(struct Reb_Frame *f) {
    f->expr_index = f->indexor;
    if (Trace_Flags)
        Trace_Line(f);
}

#ifdef NDEBUG
    #define START_NEW_EXPRESSION(f) \
        Start_New_Expression_Core(f)
#else
    // Macro is used to mutate local do_count variable in Do_Core (for easier
    // browsing in the watchlist) as well as to not be in a deeper stack level
    // than Do_Core when a DO_COUNT_BREAKPOINT is hit.
    //
    #define START_NEW_EXPRESSION(f) \
        do { \
            Start_New_Expression_Core(f); \
            do_count = Do_Core_Expression_Checks_Debug(f); \
            if (do_count == DO_COUNT_BREAKPOINT) \
                debug_break(); /* see %debug_break.h */ \
        } while (FALSE)
#endif


// Simple macro for wrapping (but not obscuring) a `goto` in the code below
//
#define NOTE_THROWING(g) \
    do { \
        assert(f->indexor == THROWN_FLAG); \
        assert(THROWN(f->out)); \
        g; /* goto statement left at callsite for readability */ \
    } while(0)


//
//  Do_Core: C
//
void Do_Core(struct Reb_Frame * const f)
{
#if !defined(NDEBUG)
    REBUPT do_count; // cache of `f->do_count` (improves watchlist visibility)
#endif

    enum Reb_Param_Class pclass; // cached while fulfilling an argument

    // !!! Temporary hack until better finesse is found...an APPLY wants to
    // treat voids in the frame as valid argument fulfillment for optional
    // arguments (as opposed to SPECIALIZE, which wants to treat them as
    // unspecialized and potentially gathered from the callsite).  The
    // right bits aren't in place yet to know which it is in the middle of
    // the function, but should be streamlined so they are.
    //
    REBOOL applying;

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
        TG_Frame_Stack = f;
    #if !defined(NDEBUG)
        SNAP_STATE(&f->state); // to make sure stack balances, etc.
        do_count = Do_Core_Entry_Checks_Debug(f); // run once per Do_Core()
    #endif
        CLEAR_FRAME_SYM(f);
        applying = FALSE;
        break;

    case CALL_MODE_ARGS:
        assert(TG_Frame_Stack == f); // should already be pushed
        assert(f->label_sym != SYM_0);
        assert(f->label_str != NULL);
    #if !defined(NDEBUG)
        do_count = TG_Do_Count; // entry checks for debug not true here
    #endif
        applying = TRUE;
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

    // Make sure `eval` is trash in debug build if not doing a `reevaluate`.
    // It does not have to be GC safe (for reasons explained below).  We
    // also need to reset evaluation to normal vs. a kind of "inline quoting"
    // in case EVAL/ONLY had enabled that.
    //
    // Note that since the cell lives in a union, it cannot have a constructor
    // so the automatic mark of writable that most REBVALs get could not
    // be used.  Since it's a raw RELVAL, we have to explicitly mark writable.
    //
    INIT_CELL_WRITABLE_IF_DEBUG(&(f->cell.eval));
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

        if (!IS_VOID(f->out)) {
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

    START_NEW_EXPRESSION(f);

    //==////////////////////////////////////////////////////////////////==//
    //
    // BEGIN MAIN SWITCH STATEMENT
    //
    //==////////////////////////////////////////////////////////////////==//

    // This switch is done via ET_XXX and not just switching on the VAL_TYPE()
    // (e.g. REB_XXX).  The reason is due to "jump table" optimizing--because
    // the REB_XXX types are sparse, the switch would be less efficient than
    // when switching on values that are packed consecutively (e.g. ET_XXX).
    //
    // Note that infix ("lookback") functions are dispatched *after* the
    // switch...unless DO_FLAG_NO_LOOKAHEAD is set.

    switch (Eval_Table[VAL_TYPE(f->value)]) { // DO_COUNT_BREAKPOINT lands here

//==//////////////////////////////////////////////////////////////////////==//
//
// [no evaluation] (REB_BLOCK, REB_INTEGER, REB_STRING, etc.)
//
// Copy the value's bits to f->out and fetch the next value.
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
// be if hit in this switch), it evaluates to void.  It only errors in argument
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
// "second condition" had no value.  So if you are looking for a BAR! behavior
// and it's not passing through here, check the construct you are using.
//
//==//////////////////////////////////////////////////////////////////////==//

    case ET_BAR:
        SET_VOID(f->out);
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
//==//////////////////////////////////////////////////////////////////////==//

    case ET_WORD: {
        REBOOL lookback;
        *(f->out) = *Get_Var_Core(&lookback, f->value, GETVAR_READ_ONLY);

        if (IS_FUNCTION(f->out)) { // check before checking unset, for speed
            SET_FRAME_SYM(f, VAL_WORD_SYM(f->value));

            if (lookback) {
                //
                // Note: Infix functions cannot "look back" for a valid first
                // argument at this point, because there's no "Left-Hand-Side"
                // computed to use.  We "look ahead" for an infix operation
                // *after* this switch statement, when a computed value in
                // f->out is there for the infix operation to "look back at".
                //
                // Hence, the only infix functions that can run from this
                // point are those that explicitly tolerate an <end> point as
                // their first argument.

                f->cell.eval = *f->out;
                f->value = const_KNOWN(&f->cell.eval);

                SET_END(f->out);
                goto do_infix_function_in_value_first_arg_is_out;
            }

            f->value = f->out;
            goto do_prefix_function_in_value;
        }

    handle_out_as_if_it_was_gotten_by_word:
        assert(!IS_FUNCTION(f->out)); // lookback local isn't valid if jumped in

        if (IS_VOID(f->out))
            fail (Error(RE_NO_VALUE, f->value)); // need `:x` if `x` is unset

    #if !defined(NDEBUG)
        if (LEGACY(OPTIONS_LIT_WORD_DECAY) && IS_LIT_WORD(f->out))
            VAL_SET_TYPE_BITS(f->out, REB_WORD); // don't reset full header!
    #endif

        FETCH_NEXT_ONLY_MAYBE_END(f);
        break;
    }

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

    #if !defined(NDEBUG)
        if (LEGACY(OPTIONS_SET_WORD_VOID_IS_ERROR) && IS_VOID(f->out))
            fail (Error(RE_NEED_VALUE, f->param)); // e.g. `foo: ()`
    #endif

        *GET_MUTABLE_VAR_MAY_FAIL(f->param) = *(f->out);
        break;

//==//////////////////////////////////////////////////////////////////////==//
//
// [GET-WORD!]
//
// A GET-WORD! does no checking for unsets, no dispatch on functions, and
// will return void if the variable is not set.
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

    case ET_PATH: {
        REBSYM sym;

        if (Do_Path_Throws(
            f->out,
            &sym, // requesting symbol says we process refinements
            f->value,
            NULL // `setval`: null means don't treat as SET-PATH!
        )) {
            f->indexor = THROWN_FLAG;
            NOTE_THROWING(goto return_indexor);
        }

        if (IS_VOID(f->out))
            fail (Error(RE_NO_VALUE, f->value)); // need `:x/y` if `y` is unset

        if (IS_FUNCTION(f->out)) {
            SET_FRAME_SYM(f, sym);

            // object/func or func/refinements or object/func/refinement
            //
            // Because we passed in a label symbol, the path evaluator was
            // willing to assume we are going to invoke a function if it
            // is one.  Hence it left any potential refinements on data stack.
            //
            assert(DSP >= f->dsp_orig);

            // The WORD! dispatch case checks whether the dispatch was via an
            // infix binding at this point, and if so allows the infix function
            // to run only if it has an <end>able left argument.  Paths ignore
            // the infix-or-not status of a binding for several reasons, so
            // this does not come into play here.

            f->value = f->out;
            goto do_prefix_function_in_value;
        }
        else {
            // Path should have been fully processed, no refinements on stack
            //
            assert(DSP == f->dsp_orig);
            FETCH_NEXT_ONLY_MAYBE_END(f);
        }
        break;
    }

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

    #if !defined(NDEBUG)
        if (LEGACY(OPTIONS_SET_WORD_VOID_IS_ERROR) && IS_VOID(f->out))
            fail (Error(RE_NEED_VALUE, f->param)); // e.g. `a/b/c: ()`
    #endif

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
        SET_FRAME_SYM(f, SYM___ANONYMOUS__);

    do_prefix_function_in_value:
        assert(IS_FUNCTION(f->value));
        assert(f->label_sym != SYM_0); // must be something (even "anonymous")
    #if !defined(NDEBUG)
        assert(f->label_str != NULL); // SET_FRAME_SYM sets (for C debugging)
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
        // !!! Currently EVAL cannot be specialized or trigger from "infix"
        //
        if (VAL_FUNC(f->value) == NAT_FUNC(eval)) {
            FETCH_NEXT_ONLY_MAYBE_END(f);

            if (f->indexor == END_FLAG) // e.g. `do [eval]`
                fail (Error_No_Arg(
                    FRM_LABEL(f), FUNC_PARAM(NAT_FUNC(eval), 1)
                ));

            // "DO/NEXT" full expression into the `eval` REBVAR slot
            // (updates index...).  (There is an /ONLY switch to suppress
            // normal evaluation but it does not apply to the value being
            // retriggered itself, just any arguments it consumes.)
            //
            // Though the debug build does a writability init earlier in
            // the routine, the eval's cell bits live in a union that can
            // wind up getting used for other purposes.  Hence the writability
            // must be re-indicated here before the slot is used.
            //
            INIT_CELL_WRITABLE_IF_DEBUG(&f->cell.eval);
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
                f->eval_fetched = END_CELL; // NULL means no eval_fetched :-/

            f->value = const_KNOWN(&f->cell.eval);
            CLEAR_FRAME_SYM(f);
            goto reevaluate; // we don't move index!
        }

    //==////////////////////////////////////////////////////////////////==//
    //
    // FUNCTION! NORMAL ARGUMENT FULFILLMENT PROCESS
    //
    //==////////////////////////////////////////////////////////////////==//

        // Depending on the <durable> settings of a function's arguments, they
        // may wind up resident in stack space or in dynamically allocated
        // space.  This sets up the memory as appropriate for the flags.
        //
        // This extracts f->func and gets the frame set up as it should be
        // (either full of voids or pre-filled with some specialized values,
        // and void for those still needed as arguments).
        //
        Push_Or_Alloc_Vars_For_Underlying_Func(f); // sets f->func

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
        f->mode = CALL_MODE_ARGS;
        f->refine = BAR_VALUE; // "not a refinement arg, evaluate normally"

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
        // specialization and ordinary invocation, the void type is used as
        // a signal to have "unspecialized" behavior.  Hence a normal call
        // just pre-fills all the args with void--which will be overwritten
        // during the argument fulfillment process (unless they turn out to
        // be optional in the invocation).
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

                if (IS_VOID(f->arg)) {

    //=//// UNSPECIALIZED REFINEMENT SLOT (no consumption) ////////////////=//

                    if (f->dsp_orig == DSP) { // no refinements left on stack
                        SET_FALSE(f->arg);
                        f->refine = BLANK_VALUE; // "don't consume args, ever"
                        goto continue_arg_loop;
                    }

                    f->refine = DS_TOP;

                    if (
                        VAL_WORD_SYM(f->refine)
                        == SYMBOL_TO_CANON(VAL_TYPESET_SYM(f->param)) // #2258
                    ) {
                        DS_DROP; // we're lucky: this was next refinement used

                        SET_TRUE(f->arg); // marks refinement used
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

                            SET_TRUE(f->arg); // marks refinement used
                            f->refine = VOID_CELL; // "consume args later"
                            goto continue_arg_loop;
                        }
                    }

                    // Wasn't in the path and not specialized, so not present
                    //
                    SET_FALSE(f->arg);
                    f->refine = BLANK_VALUE; // "don't consume args, ever"
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

                if (!IS_LOGIC(f->arg)) {
                    //
                    // Refinements must specialize as TRUE or FALSE
                    //
                    REBVAL word;
                    Val_Init_Word(&word, REB_WORD, VAL_TYPESET_SYM(f->param));
                    fail (Error(RE_NON_LOGIC_REFINE, &word, Type_Of(f->arg)));
                }
                else if (IS_CONDITIONAL_TRUE(f->arg)) { // must be TRUE
                    SET_TRUE(f->arg);
                    f->refine = f->arg; // remember so we can revoke!
                }
                else { // must be FALSE
                    SET_FALSE(f->arg);
                    f->refine = BLANK_VALUE; // (read-only)
                }

                goto continue_arg_loop;
            }

    //=//// IF JUST SKIPPING TO NEXT REFINEMENT, MOVE ON //////////////////=//

            if (IS_VOID(f->refine))
                goto continue_arg_loop;

    //=//// PURE "LOCAL:" ARG (must be unset, no consumption) /////////////=//

            if (pclass == PARAM_CLASS_PURE_LOCAL) {

                if (IS_VOID(f->arg)) // only legal value - can't specialize
                    goto continue_arg_loop;

                fail (Error_Local_Injection(FRM_LABEL(f), f->param));
            }

    //=//// SPECIALIZED ARG (already filled, so does not consume) /////////=//

            if (NOT(IS_VOID(f->arg))) {

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
                        Val_Init_Typeset(
                            &honest_param,
                            FLAGIT_KIND(REB_VARARGS), // *actually* expected...
                            VAL_TYPESET_SYM(f->param)
                        );

                        fail (Error_Arg_Type(
                            FRM_LABEL(f), &honest_param, VAL_TYPE(f->arg))
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

    //=//// IF UNSPECIALIZED ARG IS INACTIVE, SET VOID AND MOVE ON ////////=//

            // Unspecialized arguments that do not consume do not need any
            // further processing or checking.  void will always be fine.
            //
            if (IS_BLANK(f->refine)) { // FALSE if revoked, and still evaluates
                assert(IS_VOID(f->arg));
                goto continue_arg_loop;
            }

    //=//// VARIADIC ARG (doesn't consume anything *yet*) /////////////////=//

            // Evaluation argument "hook" parameters (marked in MAKE FUNCTION!
            // by a `[[]]` in the spec, and in FUNC by `<...>`).  They point
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

    //=//// AFTER THIS, PARAMS CONSUME FROM CALLSITE IF NOT APPLY ////////=//

            assert(IS_VOID(f->arg));

            if (applying) goto check_arg; // try treating void as optional

    //=//// ERROR ON END MARKER, BAR! IF APPLICABLE //////////////////////=//

            if (f->indexor == END_FLAG) {
                if (!GET_VAL_FLAG(f->param, TYPESET_FLAG_ENDABLE))
                    fail (Error_No_Arg(FRM_LABEL(f), f->param));

                goto continue_arg_loop;
            }

            // Literal expression barriers cannot be consumed in normal
            // evaluation, even if the argument takes a BAR!.  It must come
            // through non-literal means(e.g. `quote '|` or `first [|]`)
            //
            if (f->args_evaluate && IS_BAR(f->value)) {
                if (!GET_VAL_FLAG(f->param, TYPESET_FLAG_ENDABLE))
                    fail (Error(RE_EXPRESSION_BARRIER));

                goto continue_arg_loop;
            }

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
            // a void arg signals the revocation of a refinement usage.

        check_arg:
            ASSERT_VALUE_MANAGED(f->arg);
            assert(pclass != PARAM_CLASS_REFINEMENT);
            assert(pclass != PARAM_CLASS_PURE_LOCAL);

            // See notes on `Reb_Frame.refine` in %sys-do.h for more info.
            //
            assert(
                IS_BLANK(f->refine) || // f->arg is arg to never-used refinment
                IS_LOGIC(f->refine) || // F = revoked, T = used refinement slot
                IS_BAR(f->refine) // f->arg is ordinary function argument
            );

            if (IS_VOID(f->arg)) {
                if (IS_BAR(f->refine)) {
                    //
                    // fall through to check ordinary arg for if <opt> is ok
                }
                else if (IS_CONDITIONAL_FALSE(f->refine)) {
                    //
                    // FALSE means the refinement has already been revoked so
                    // the void is okay.  BLANK! means the refinement was
                    // never in use in the first place.  Don't type check.
                    //
                    goto continue_arg_loop;
                }
                else {
                    assert(IS_LOGIC(f->refine));

                    // We can only revoke the refinement if this is the 1st
                    // refinement arg.  If it's a later arg, then the first
                    // didn't trigger revocation, or refine wouldn't be WORD!
                    //
                    if (f->refine + 1 != f->arg)
                        fail (Error_Bad_Refine_Revoke(f));

                    SET_FALSE(f->refine);
                    f->refine = FALSE_VALUE;
                    goto continue_arg_loop; // don't type check for optionality
                }
            }
            else {
                // If the argument is set, then the refinement shouldn't be
                // in a revoked or unused state.
                //
                if (IS_CONDITIONAL_FALSE(f->refine))
                    fail (Error_Bad_Refine_Revoke(f));
            }

            if (!TYPE_CHECK(f->param, VAL_TYPE(f->arg)))
                fail (
                    Error_Arg_Type(FRM_LABEL(f), f->param, VAL_TYPE(f->arg))
                );

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
            assert(IS_LOGIC(f->refine) && VAL_LOGIC(f->refine));
            DS_DROP;
            f->mode = CALL_MODE_REFINEMENT_PICKUP;
            goto continue_arg_loop; // leaves refine, but bumps param+arg
        }

    #if !defined(NDEBUG)
        if (GET_VAL_FLAG(FUNC_VALUE(f->func), FUNC_FLAG_LEGACY)) {
            //
            // OPTIONS_REFINEMENTS_BLANK was set when this particular function
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

            if (f->func == NAT_FUNC(leave)) {
                CONVERT_NAME_TO_EXIT_THROWN(f->out, VOID_CELL);
            }
            else {
                assert(f->func == NAT_FUNC(return));
                assert(FUNC_NUM_PARAMS(f->func) == 1);
                CONVERT_NAME_TO_EXIT_THROWN(f->out, FRM_ARGS_HEAD(f));
            }

            f->indexor = THROWN_FLAG;
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
            assert(IS_VOID(f->refine));

            if (VAL_TYPESET_CANON(f->param) == SYM_RETURN)
                *(f->refine) = *NAT_VALUE(return);
            else {
                assert(VAL_TYPESET_CANON(f->param) == SYM_LEAVE);
                *(f->refine) = *NAT_VALUE(leave);
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
        // are two entry points to Push_Or_Alloc_Vars_For_Underlying_Func
        // at the moment, so this clearing can't be done by the debug routine
        // at top of loop.
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
        // was and return no value
        //
        if (GET_VAL_FLAG(FUNC_VALUE(f->func), FUNC_FLAG_LEAVE_OR_RETURN)) {
            REBVAL *last_param = FUNC_PARAM(f->func, FUNC_NUM_PARAMS(f->func));
            if (VAL_TYPESET_CANON(last_param) == SYM_LEAVE) {
                SET_VOID(f->out);
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
                        SYM_RETURN, last_param, VAL_TYPE(f->out))
                    );
            }
        }

        f->mode = CALL_MODE_GUARD_ARRAY_ONLY;

        if (Trace_Flags)
            Trace_Return(FRM_LABEL(f), f->out);

        CLEAR_FRAME_SYM(f);

    #if !defined(NDEBUG)
        f->func = NULL;
    #endif
        break;

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
    else if (IS_WORD(f->value)) {
        //
        // Since we're not at an END, we know f->value has been prefetched,
        // so we "peek" at it if it is a WORD!.  If it looks up to an infix
        // function, we will use the value in `out` as the "left-hand-side"
        // of that invocation.
        //
        // We can't overwrite f->value in case this is a DO/NEXT and the
        // prefetched value is supposed to be good for a future Do_Core call.
        // So f->param is used to temporarily hold the fetched pointer.
        //
        REBOOL lookback;
        f->param = Get_Var_Core(&lookback, f->value, GETVAR_READ_ONLY);

    //=//// NOT A FUNCTION, BUT MAKE USE OF THE GET (if not a DO/NEXT) ////=//

        if (!IS_FUNCTION(f->param)) {
            if (NOT(f->flags & DO_FLAG_TO_END))
                goto return_indexor;

            START_NEW_EXPRESSION(f); // v-- DO_COUNT_BREAKPOINT lands below

            *f->out = *f->param;
            goto handle_out_as_if_it_was_gotten_by_word;
        }

    //=//// NOT INFIX, BUT MAKE USE OF THE GET (if not a DO/NEXT) ////////=//

        if (!lookback) {
            if (NOT(f->flags & DO_FLAG_TO_END))
                goto return_indexor;

            START_NEW_EXPRESSION(f); // v-- DO_COUNT_BREAKPOINT lands below

            SET_FRAME_SYM(f, VAL_WORD_SYM(f->value));
            f->value = f->param;
            goto do_prefix_function_in_value;
        }

    //==////////////////////////////////////////////////////////////////==//
    //
    // INFIX/POSTFIX/ETC. "LOOKBACK" PROCESSING
    //
    //==////////////////////////////////////////////////////////////////==//

        // We peeked one word ahead and saw it looked up to an infix function.
        // The desired "first" argument is the product of the previous
        // evaluation (in f->out).  If we jump here from the ET_WORD case,
        // then no previous eval is available...so f->out will be an END_CELL.
        //
        // Handling this isn't as easy as pushing argument storage, poking the
        // value into FRM_ARG(1), and calling ordinary function dispatch to
        // take care of the rest.  That's because the infix function might be
        // a specialization--in which case its first unspecialized argument
        // could be at any index in the frame.  (Pure locals are also permitted
        // at any index in unspecialized functions, so we handle that too.)

        START_NEW_EXPRESSION(f); // v-- DO_COUNT_BREAKPOINT lands below

        SET_FRAME_SYM(f, VAL_WORD_SYM(f->value));
        f->value = f->param;

    do_infix_function_in_value_first_arg_is_out:

        // Infix dispatch can only come from word lookup.  The APPLY operation
        // and DO of a FRAME! should not be able to get here.  (Note: this
        // means that if DO_FLAG_EXECUTE_FRAME is set, we are specializing
        // and must interpret any void f->arg as an unspecified parameter.)
        //
        assert(!applying);

        Push_Or_Alloc_Vars_For_Underlying_Func(f); // sets f->func
        f->param = FUNC_PARAMS_HEAD(f->func);
        f->arg = FRM_ARGS_HEAD(f);

        // Look for the first "normal" argument that has not been specialized
        // to fulfill.  Any soft-quoted specializations will have to be
        // handled in the process.
        //
        for (; ; ++f->param, ++f->arg) {
            if (IS_END(f->param)) {
                //
                // A lookback binding that takes two arguments is "infix".
                // A lookback binding that takes one argument is "postfix".
                // A lookback binding that takes > 2 arguments is weird.
                //
                // Here we look at the parameters list and see nothing, e.g.
                // it's a lookback function with 0 arguments.  It can't take
                // the f->out parameter we have, so we error unless f->out
                // is an END_VALUE.  This makes it "punctuation".
                //
                // !!! There is a subtlety here because punctuation does *not*
                // disable infix lookahead.  This makes it distinct from an
                // arity-1 lookback binding that can tolerate <end>.  Whether
                // this is a good idea or not remains to be seen, but it's
                // a possible distinction to make, so it's being explored.
                //
            handle_infix_as_punctuation:
                if (
                    IS_END(f->out)
                    && (f->lookahead_flags & DO_FLAG_NO_LOOKAHEAD)
                ) {
                    REBVAL word;
                    Val_Init_Word(&word, REB_WORD, f->label_sym);
                    fail (Error(RE_PUNCTUATION_HIT, &word));
                }

                // Note: We set f->lookahead_flags to DO_FLAG_NO_LOOKAHEAD
                // when processing infix/etc usually.  But here it wouldn't
                // matter, because there are no arguments to process.
                //
                FETCH_NEXT_ONLY_MAYBE_END(f);
                goto do_function_arglist_in_progress;
            }

            if (VAL_PARAM_CLASS(f->param) == PARAM_CLASS_PURE_LOCAL) {
                if (IS_VOID(f->arg))
                    continue;

                fail (Error_Local_Injection(FRM_LABEL(f), f->param));
            }

            if (VAL_PARAM_CLASS(f->param) != PARAM_CLASS_NORMAL) {
                if (
                    VAL_PARAM_CLASS(f->param) == PARAM_CLASS_REFINEMENT
                    && IS_VOID(f->arg)
                    && NOT(f->flags & DO_FLAG_EXECUTE_FRAME)
                ) {
                    // If we hit an unused refinement, we're out of normal
                    // parameters.  So we've exhausted the basic arity.
                    //
                    goto handle_infix_as_punctuation;
                }

                // !!! This one is tricky.  Should you be allowed to specialize
                // a function e.g. `specialize :append [dup: true]` and affect
                // its arity without actually supplying the arg?  It seems
                // reasonable but it would require more handling.
                //
                fail (Error(RE_MISC)); // esoteric specialization cases TBD
            }

            if (IS_VOID(f->arg))
                break; // it's either unspecialized or needs our arg

            // Non-void normal parameters must be specializations here.
            //
            assert(f->flags & DO_FLAG_EXECUTE_FRAME);

            if (f->args_evaluate && IS_QUOTABLY_SOFT(f->arg)) {
                if (DO_VALUE_THROWS(SINK(&f->cell.eval), f->arg)) {
                    // infix cannot be refined -- don't need DS_DROP_TO
                    f->indexor = THROWN_FLAG;
                    NOTE_THROWING(goto drop_call_and_return_thrown);
                }
                *f->arg = *KNOWN(&f->cell.eval);
            }
        }

        // Now f->arg is the valid argument slot to write into, but we still
        // have to type check to make sure what's in f->out is a fit.
        //
        if (IS_END(f->out)) {
            if (!GET_VAL_FLAG(f->param, TYPESET_FLAG_ENDABLE))
                fail (Error_No_Arg(FRM_LABEL(f), f->param));

            SET_VOID(f->arg);
        }
        else {
            if (!TYPE_CHECK(f->param, VAL_TYPE(f->out)))
                fail (Error_Arg_Type(
                    FRM_LABEL(f), f->param, VAL_TYPE(f->out))
                );
            *f->arg = *f->out;
        }

        // Now we bump the parameter and arg, and go through ordinary
        // function argument fulfillment.  Note that during the argument
        // evaluations for an infix function, we do not look further ahead.
        //
        f->lookahead_flags = DO_FLAG_NO_LOOKAHEAD;
        ++f->param;
        ++f->arg;

        FETCH_NEXT_ONLY_MAYBE_END(f);
        goto do_function_arglist_in_progress;
    }

    // Continue evaluating rest of block if not just a DO/NEXT
    //
    if (f->flags & DO_FLAG_TO_END) goto value_ready_for_do_next;

return_indexor:
#if !defined(NDEBUG)
    Do_Core_Exit_Checks_Debug(f); // will get called unless a fail() longjmps
#endif

    // Restore the top of stack (if there is a fail() and associated longjmp,
    // this restoration will be done by the Drop_Trap helper.)
    //
    TG_Frame_Stack = f->prior;

    // Caller needs to inspect `index`, at minimum to know if it's THROWN_FLAG
}


//==//////////////////////////////////////////////////////////////////////==//
//
// DEBUG-BUILD ONLY CHECKS
//
//==//////////////////////////////////////////////////////////////////////==//
//
// Due to the length of Do_Core() and how many debug checks it already has,
// three debug-only routines are separated out:
//
// * Do_Core_Entry_Checks_Debug() runs once at the beginning of a Do_Core()
//   call.  It verifies that the fields of the frame the caller has to
//   provide have been pre-filled correctly, and snapshots bits of the
//   interpreter state that are supposed to "balance back to zero" by the
//   end of a run (assuming it completes, and doesn't longjmp from fail()ing)
//
// * Do_Core_Expression_Checks_Debug() runs before each full "expression"
//   is evaluated, e.g. before each DO/NEXT step.  It makes sure the state
//   balanced completely--so no DS_PUSH that wasn't balanced by a DS_POP
//   or DS_DROP (for example).  It also trashes variables in the frame which
//   might accidentally carry over from one step to another, so that there
//   will be a crash instead of a casual reuse.
//
// * Do_Core_Exit_Checks_Debug() runs if the Do_Core() call makes it to the
//   end without a fail() longjmping out from under it.  It also checks to
//   make sure the state has balanced, and that the return result is
//   consistent with the state being returned.
//
// Because none of these routines are in the release build, they cannot have
// any side-effects that affect the interpreter's ordinary operation.
//

#if !defined(NDEBUG)

static REBUPT Do_Core_Entry_Checks_Debug(struct Reb_Frame *f)
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
static REBUPT Do_Core_Expression_Checks_Debug(struct Reb_Frame *f) {
    //
    // There shouldn't have been any "accumulated state", in the sense that
    // we should be back where we started in terms of the data stack, the
    // mold buffer position, the outstanding manual series allocations, etc.
    //
    ASSERT_STATE_BALANCED(&f->state);

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
    f->func = NULL;

    assert(f->label_sym == SYM_0);
    assert(f->label_str == NULL);

    // We specifically don't trash f->param, because infix evaluation needs
    // to start a new expression, where the debug and tracing sees the
    // current f->value but the f->param is holding the next value.
    //
    /* f->param = cast(REBVAL*, 0xDECAFBAD); */
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

    // We bound the count at the max unsigned 32-bit, since otherwise it would
    // roll over to zero and print a message that wasn't asked for, which
    // is annoying even in a debug build.  (It's actually a REBUPT, so this
    // wastes possible bits in the 64-bit build, but there's no MAX_REBUPT.)
    //
    if (TG_Do_Count < MAX_U32) {
        f->do_count = ++TG_Do_Count;
        if (f->do_count == DO_COUNT_BREAKPOINT) {
            REBVAL dump = *f->value;

            PROBE_MSG(&dump, "DO_COUNT_BREAKPOINT hit at...");

            if (f->indexor == VALIST_FLAG) {
                //
                // NOTE: This reifies the va_list in the frame, and hence has
                // side effects.  It may need to be commented out if the
                // problem you are trapping with DO_COUNT_BREAKPOINT was
                // specifically with va_list frame processing.
                //
                const REBOOL truncated = TRUE;
                Reify_Va_To_Array_In_Frame(f, truncated);
            }

            if (f->eval_fetched && NOT_END(f->eval_fetched)) {
                dump = *f->eval_fetched;

                PROBE_MSG(&dump, "EVAL in progress, so next will be...");
            }

            if (f->indexor == END_FLAG) {
                Debug_Fmt("...then at end of array");
            }
            else {
                REBVAL dump;

                Val_Init_Block_Index(
                    &dump, f->source.array, cast(REBCNT, f->indexor)
                );
                PROBE_MSG(&dump, "...then this array for the next input");
            }
        }
    }

    return f->do_count;
}


static void Do_Core_Exit_Checks_Debug(struct Reb_Frame *f) {
    //
    // Make sure the data stack, mold stack, and other structures didn't
    // accumulate any state over the course of the run.
    //
    ASSERT_STATE_BALANCED(&f->state);

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

    // Function execution should have written *some* actual output value
    // over the trash that we put in the return slot before the call.
    //
    assert(!IS_TRASH_DEBUG(f->out));
    assert(VAL_TYPE(f->out) < REB_MAX); // cheap check

    if (f->indexor == THROWN_FLAG)
        assert(THROWN(f->out));
    else
        ASSERT_VALUE_MANAGED(f->out);
}

#endif
