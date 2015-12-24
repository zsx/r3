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
**  Module:  n-system.c
**  Summary: native functions for system operations
**  Section: natives
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"


//
//  halt: native [
//  
//  "Stops evaluation and returns to the input prompt."
//  
//      ; No arguments
//  ]
//
REBNATIVE(halt)
{
    fail (VAL_FRAME(TASK_HALT_ERROR));
}


//
//  quit: native [
//  
//  {Stop evaluating and return control to command shell or calling script.}
//  
//      /with {Yield a result (mapped to an integer if given to shell)}
//      value [any-value!] "See: http://en.wikipedia.org/wiki/Exit_status"
//      /return "(deprecated synonym for /WITH)"
//      return-value
//  ]
//
REBNATIVE(quit)
//
// QUIT is implemented via a THROWN() value that bubbles up through
// the stack.  It uses the value of its own native function as the
// name of the throw, like `throw/name value :quit`.
{
    *D_OUT = *FUNC_VALUE(D_FUNC);

    if (D_REF(1)) {
        CONVERT_NAME_TO_THROWN(D_OUT, D_ARG(2), FALSE);
    }
    else if (D_REF(3)) {
        CONVERT_NAME_TO_THROWN(D_OUT, D_ARG(4), FALSE);
    }
    else {
        // Chosen to do it this way because returning to a calling script it
        // will be UNSET! by default, for parity with BREAK and EXIT without
        // a /WITH.  Long view would have RETURN work this way too: CC#2241

        // (UNSET! will be translated to 0 if it gets caught for the shell)

        CONVERT_NAME_TO_THROWN(D_OUT, UNSET_VALUE, FALSE);
    }

    return R_OUT_IS_THROWN;
}


//
//  recycle: native [
//  
//  "Recycles unused memory."
//  
//      /off "Disable auto-recycling"
//      /on "Enable auto-recycling"
//      /ballast "Trigger for auto-recycle (memory used)"
//      size [integer!]
//      /torture "Constant recycle (for internal debugging)"
//  ]
//
REBNATIVE(recycle)
{
    REBCNT count;

    if (D_REF(1)) { // /off
        GC_Active = FALSE;
        return R_UNSET;
    }

    if (D_REF(2)) {// /on
        GC_Active = TRUE;
        VAL_INT64(TASK_BALLAST) = VAL_INT32(TASK_MAX_BALLAST);
    }

    if (D_REF(3)) {// /ballast
        *TASK_MAX_BALLAST = *D_ARG(4);
        VAL_INT64(TASK_BALLAST) = VAL_INT32(TASK_MAX_BALLAST);
    }

    if (D_REF(5)) { // torture
        GC_Active = TRUE;
        VAL_INT64(TASK_BALLAST) = 0;
    }

    count = Recycle();

    SET_INTEGER(D_OUT, count);
    return R_OUT;
}


//
//  stats: native [
//  
//  {Provides status and statistics information about the interpreter.}
//  
//      /show "Print formatted results to console"
//      /profile "Returns profiler object"
//      /timer "High resolution time difference from start"
//      /evals "Number of values evaluated by interpreter"
//      /dump-series pool-id [integer!] 
//      "Dump all series in pool pool-id, -1 for all pools"
//  ]
//
REBNATIVE(stats)
{
    REBI64 n;
    REBCNT flags = 0;
    REBVAL *stats;

    if (D_REF(3)) {
        VAL_TIME(D_OUT) = OS_DELTA_TIME(PG_Boot_Time, 0) * 1000;
        VAL_RESET_HEADER(D_OUT, REB_TIME);
        return R_OUT;
    }

    if (D_REF(4)) {
        n = Eval_Cycles + Eval_Dose - Eval_Count;
        SET_INTEGER(D_OUT, n);
        return R_OUT;
    }

    if (D_REF(2)) {
    #ifdef NDEBUG
        fail (Error(RE_DEBUG_ONLY));
    #else
        stats = Get_System(SYS_STANDARD, STD_STATS);
        *D_OUT = *stats;
        if (IS_OBJECT(stats)) {
            stats = Get_Object(stats, 1);

            VAL_TIME(stats) = OS_DELTA_TIME(PG_Boot_Time, 0) * 1000;
            VAL_RESET_HEADER(stats, REB_TIME);
            stats++;
            SET_INTEGER(stats, Eval_Cycles + Eval_Dose - Eval_Count);
            stats++;
            SET_INTEGER(stats, Eval_Natives);
            stats++;
            SET_INTEGER(stats, Eval_Functions);

            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Series_Made);
            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Series_Freed);
            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Series_Expanded);
            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Series_Memory);
            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Recycle_Series_Total);

            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Blocks);
            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Objects);

            stats++;
            SET_INTEGER(stats, PG_Reb_Stats->Recycle_Counter);
        }
    #endif

        return R_OUT;
    }

    if (D_REF(5)) {
        REBVAL *pool_id = D_ARG(6);
        Dump_Series_In_Pool(VAL_INT32(pool_id));
        return R_NONE;
    }

    if (D_REF(1)) flags = 3;
    n = Inspect_Series(flags);

    SET_INTEGER(D_OUT, n);

    return R_OUT;
}

const char *evoke_help = "Evoke values:\n"
    "[stack-size n] crash-dump delect\n"
    "watch-recycle watch-obj-copy crash\n"
    "1: watch expand\n"
    "2: check memory pools\n"
    "3: check bind table\n"
;

//
//  evoke: native [
//  
//  "Special guru meditations. (Not for beginners.)"
//  
//      chant [word! block! integer!] "Single or block of words ('? to list)"
//  ]
//
REBNATIVE(evoke)
{
    REBVAL *arg = D_ARG(1);
    REBCNT len;

    Check_Security(SYM_DEBUG, POL_READ, 0);

    if (IS_BLOCK(arg)) {
        len = VAL_LEN_AT(arg);
        arg = VAL_ARRAY_AT(arg);
    }
    else len = 1;

    for (; len > 0; len--, arg++) {
        if (IS_WORD(arg)) {
            switch (VAL_WORD_CANON(arg)) {
            case SYM_DELECT:
                Trace_Delect(1);
                break;
            case SYM_CRASH_DUMP:
                Reb_Opts->crash_dump = TRUE;
                break;
            case SYM_WATCH_RECYCLE:
                Reb_Opts->watch_recycle = NOT(Reb_Opts->watch_recycle);
                break;
            case SYM_WATCH_OBJ_COPY:
                Reb_Opts->watch_obj_copy = NOT(Reb_Opts->watch_obj_copy);
                break;
            case SYM_STACK_SIZE:
                arg++;
                Expand_Stack(Int32s(arg, 1));
                break;
            case SYM_CRASH:
                panic (Error(RE_MISC));
            default:
                Out_Str(cb_cast(evoke_help), 1);
            }
        }
        if (IS_INTEGER(arg)) {
            switch (Int32(arg)) {
            case 0:
                Check_Memory();
                Check_Bind_Table();
                break;
            case 1:
                Reb_Opts->watch_expand = TRUE;
                break;
            case 2:
                Check_Memory();
                break;
            case 3:
                Check_Bind_Table();
                break;
            default:
                Out_Str(cb_cast(evoke_help), 1);
            }
        }
    }

    return R_UNSET;
}


//
//  limit-usage: native [
//  
//  "Set a usage limit only once (used for SECURE)."
//  
//      field [word!] "eval (count) or memory (bytes)"
//      limit [any-number!]
//  ]
//
REBNATIVE(limit_usage)
{
    REBCNT sym;

    sym = VAL_WORD_CANON(D_ARG(1));

    // Only gets set once:
    if (sym == SYM_EVAL) {
        if (Eval_Limit == 0) Eval_Limit = Int64(D_ARG(2));
    } else if (sym == SYM_MEMORY) {
        if (PG_Mem_Limit == 0) PG_Mem_Limit = Int64(D_ARG(2));
    }
    return R_UNSET;
}


//
//  Where_For_Call: C
//
// Each call frame maintains the array it is executing in, the current index
// in that array, and the index of where the current expression started.
// This can be deduced into a segment of code to display in the debug views
// to indicate roughly "what's running" at that stack level.
//
// Unfortunately, Rebol doesn't formalize this very well.  There is no lock
// on segments of blocks during their evaluation, and it's possible for
// self-modifying code to scramble the blocks being executed.  The DO
// evaluator is robust in terms of not *crashing*, but the semantics may well
// suprise users.
//
// !!! Should blocks on the stack be locked from modification, at least by
// default unless a special setting for self-modifying code unlocks it?
//
// So long as WHERE information is unreliable, this has to check that
// `expr_index` (where the evaluation started) and `index` (where the
// evaluation thinks it currently is) aren't out of bounds here.  We could
// be giving back positions now unrelated to the call...but it won't crash!
//
REBARR *Where_For_Call(struct Reb_Call *call)
{
    // WARNING: MIN is a C macro and repeats its arguments.
    //
    REBCNT start = MIN(ARRAY_LEN(call->array), call->expr_index);
    REBCNT end = MIN(ARRAY_LEN(call->array), call->index);

    REBARR *where;
    REBOOL pending;

    assert(call->mode != CALL_MODE_0);
    pending = NOT(call->mode == CALL_MODE_FUNCTION);

    // Do a shallow copy so that the WHERE information only includes
    // the range of the array being executed up to the point of
    // currently relevant evaluation, not all the way to the tail
    // of the block (where future potential evaluation would be)
    //
    where = Copy_Values_Len_Extra_Shallow(
        ARRAY_AT(DSF_ARRAY(call), start),
        end - start,
        pending ? 1 : 0
    );

    // Making a shallow copy offers another advantage, that it's
    // possible to get rid of the newline marker on the first element,
    // that would visually disrupt the backtrace for no reason.
    //
    if (end - start > 0)
        VAL_CLR_OPT(ARRAY_HEAD(where), OPT_VALUE_LINE);

    // We add an ellipsis to a pending frame to make it a little bit
    // clearer what is going on.  If someone sees a where that looks
    // like just `* [print]` the asterisk alone doesn't quite send
    // home the message that print is not running and it is
    // argument fulfillment that is why it's not "on the stack"
    // yet, so `* [print ...]` is an attempt to say that better.
    //
    // !!! This is in-band, which can be mixed up with literal usage
    // of ellipsis.  Could there be a better "out-of-band" conveyance?
    // Might the system use colorization in a value option bit.
    //
    if (pending)
        Val_Init_Word_Unbound(
            Alloc_Tail_Array(where), REB_WORD, SYM_ELLIPSIS
        );

    return where;
}


//
//  backtrace: native [
//  
//  "Returns backtrace with WHERE blocks, or other queried property."
//
//      /limit
//          "Limit the length of the backtrace"
//      frames [none! integer!]
//          "Max number of frames (pending and active), none for no limit"
//      /at
//          "Return only a single backtrace property"
//      level [integer!]
//          "Stack level to return property for"
//      /function
//          "Query function value"
//      /label
//          "Query word used to invoke function (NONE! if anyonymous)"
//      /args
//          "Query invocation args (may be modified since invocation)"
//      /only
//          "Do not list depths, only the selected properties"
//  ]
//
REBNATIVE(backtrace)
{
    REFINE(1, limit);
    PARAM(2, frames);
    REFINE(3, at);
    PARAM(4, level);
    REFINE(5, function);
    REFINE(6, label);
    REFINE(7, args);
    REFINE(8, only);

    REBCNT number; // stack level number in the loop (no pending frames)
    REBCNT queried_number; // synonym for the "level" from /AT

    REBCNT row; // row we're on (includes pending frames and maybe ellipsis)
    REBCNT max_rows; // The "frames" from /LIMIT, plus one (for ellipsis)

    REBCNT index; // backwards-counting index for slots in backtrace array

    REBOOL first = TRUE; // special check of first frame for "breakpoint 0"

    REBARR *backtrace;
    struct Reb_Call *call;

    Check_Security(SYM_DEBUG, POL_READ, 0);

    if (REF(limit) && REF(at)) {
        //
        // /LIMIT assumes that you are returning a list of backtrace items,
        // while /AT assumes exactly one.  They are mutually exclusive.
        //
        fail (Error(RE_BAD_REFINES));
    }

    if (REF(limit)) {
        if (IS_NONE(ARG(frames)))
            max_rows = MAX_U32; // NONE is no limit--as many frames as possible
        else {
            if (VAL_INT32(ARG(frames)) < 0)
                fail (Error_Invalid_Arg(ARG(frames)));
            max_rows = VAL_INT32(ARG(frames)) + 1; // + 1 for ellipsis
        }
    }
    else
        max_rows = 20; // On an 80x25 terminal leaves room to type afterward

    if (REF(at)) {
        //
        // If we're asking for a specific stack level via /AT, then we aren't
        // building an array result, just returning a single value.
        //
        // See notes on handling of breakpoint below for why 0 is accepted.
        //
        if (VAL_INT32(ARG(level)) < 0)
            fail (Error_Invalid_Arg(ARG(level)));
        queried_number = VAL_INT32(ARG(level));
    }
    else {
        // We're going to build our backtrace in reverse.  This is done so
        // that the most recent stack frames are at the bottom, that way
        // they don't scroll off the top.  But this is a little harder to
        // get right, so get a count of how big it will be first.
        //
        // !!! This could also be done by over-allocating and then setting
        // the series bias, though that reaches beneath the series layer
        // and makes assumptions about the implementation.  And this isn't
        // *that* complicated, considering.
        //
        index = 0;
        row = 0;
        for (call = DSF->prior; call != NULL; call = PRIOR_DSF(call)) {
            if (call->mode == CALL_MODE_0) continue;

            // index and property, unless /ONLY in which case it will just
            // be the property.
            //
            ++index;
            if (!REF(only))
                ++index;

            ++row;

            if (row >= max_rows) {
                //
                // Past our depth, so this entry is an ellipsis.  Notice that
                // the base case of `/FRAMES 0` produces max_rows of 1, which
                // means you will get just an ellipsis row.
                //
                break;
            }
        }

        backtrace = Make_Array(index);
        SET_ARRAY_LEN(backtrace, index);
        TERM_ARRAY(backtrace);
    }

    row = 0;
    number = 0;
    for (call = DSF->prior; call != NULL; call = call->prior) {
        REBCNT len;
        REBVAL *temp;
        REBOOL pending;

        // Only consider invoked or pending functions in the backtrace.
        //
        // !!! The pending functions aren't actually being "called" yet,
        // their frames are in a partial state of construction.  However it
        // gives a fuller picture to see them in the backtrace.  It may
        // be interesting to see GROUP! stack levels that are being
        // executed as well (as they are something like DO).
        //
        if (call->mode == CALL_MODE_0)
            continue;

        if (call->mode == CALL_MODE_FUNCTION) {
            pending = FALSE;

            if (
                first
                && IS_NATIVE(FUNC_VALUE(call->func))
                && FUNC_CODE(call->func) == &N_breakpoint
            ) {
                // Omitting BREAKPOINTs from the list entirely presents a
                // skewed picture of what's going on.  But giving them
                // "index 1" means that inspecting the frame you're actually
                // interested in (the one where you put the breakpoint) bumps
                // to 2, which feels unnatural.
                //
                // Compromise by not incrementing the stack numbering for
                // this case, leaving a leading breakpoint frame at index 0.
            }
            else
                ++number;
        }
        else
            pending = TRUE;

        first = FALSE;

        ++row;

        if (REF(at)) {
            if (number != queried_number)
                continue;
        }
        else {
            if (row >= max_rows) {
                //
                // If there's more stack levels to be shown than we were asked
                // to show, then put an `+ ...` in the list and break.
                //
                temp = ARRAY_AT(backtrace, --index);
                Val_Init_Word_Unbound(temp, REB_WORD, SYM_PLUS);
                if (!REF(only)) {
                    //
                    // In the non-/ONLY backtrace, the pairing of the ellipsis
                    // with a plus is used in order to keep the "record size"
                    // of the list at an even 2.  Asterisk might have been
                    // used but that is taken for "pending frames".
                    //
                    // !!! Review arbitrary symbolic choices.
                    //
                    temp = ARRAY_AT(backtrace, --index);
                    Val_Init_Word_Unbound(temp, REB_WORD, SYM_ASTERISK);
                    VAL_SET_OPT(temp, OPT_VALUE_LINE); // put on own line
                }
                break;
            }
        }

        // The /ONLY case is bare bones and just gives a block of the label
        // symbols (at this point in time).
        //
        // !!! Should /ONLY omit pending frames?  Should it have a less
        // "loaded" name for the refinement?
        //
        if (REF(only)) {
            if (REF(at)) {
                Val_Init_Word_Unbound(temp, REB_WORD, DSF_LABEL_SYM(call));
                return R_OUT;
            }

            temp = ARRAY_AT(backtrace, --index);
            Val_Init_Word_Unbound(temp, REB_WORD, DSF_LABEL_SYM(call));
            continue;
        }

        // We're either going to write the queried property into the list
        // of backtrace elements, or return it as the single result if /AT
        //
        temp = REF(at) ? D_OUT : ARRAY_AT(backtrace, --index);

        // The queried properties currently override each other; there is
        // no way to ask for more than one.
        //
        // !!! Should it return an object or block if more than one is
        // requested?  It should at least give an incompatible refinement
        // error if it can't support multiple queries in one call.
        //
        if (REF(label)) {
            Val_Init_Word_Unbound(temp, REB_WORD, DSF_LABEL_SYM(call));
        }
        else if (REF(function)) {
            *temp = *FUNC_VALUE(DSF_FUNC(call));
        }
        else if (REF(args)) {
            //
            // There may be "pure local" arguments that should be hidden (in
            // definitional return there's at least RETURN:).  So the
            // array could end up being larger than it needs to be.
            //
            // !!! Wouldn't this be perhaps more useful if it came back as an
            // object, so you knew what the values corresponded to?
            //
            REBARR *array = Make_Array(FUNC_NUM_PARAMS(DSF_FUNC(call)));
            REBVAL *param = FUNC_PARAMS_HEAD(DSF_FUNC(call));
            REBVAL *arg = DSF_ARGS_HEAD(call);
            REBVAL *dest = ARRAY_HEAD(array);

            if (pending) {
                //
                // Don't want to give arguments for pending frames, they may
                // be partially constructed or will be revoked
                //
                Val_Init_Word_Unbound(temp, REB_WORD, SYM_ELLIPSIS);
            }
            else {
                for (; NOT_END(param); param++, arg++) {
                    if (VAL_GET_EXT(param, EXT_TYPESET_HIDDEN))
                        continue;
                    *dest++ = *arg;
                }

                SET_ARRAY_LEN(array, dest - ARRAY_HEAD(array));
                TERM_ARRAY(array);

                Val_Init_Block(temp, array);
            }
        }
        else {
            // `WHERE` is the default to query, because it provides the most
            // data -- not just knowing the function being called but also the
            // context of its invocation.

            Val_Init_Block(temp, Where_For_Call(call));
        }

        // Try and keep the numbering in sync with query used by host to get
        // function frames to do binding in the REPL with.
        //
        if (!pending)
            assert(Call_For_Stack_Level(number, TRUE) == call);

        // Try and keep the numbering in sync with query used by host to get
        // function frames to do binding in the REPL with.
        //
        if (!pending)
            assert(Call_For_Stack_Level(number, TRUE) == call);

        if (REF(at)) {
            //
            // If we were fetching a single stack level, then `temp` above
            // is our singular return result.
            //
            return R_OUT;
        }

        // If building a backtrace, we just keep accumulating results as long
        // as there are stack levels left and the limit hasn't been hit.

        // The integer identifying the stack level (used to refer to it
        // in other debugging commands).  Since we're going in reverse, we
        // add it after the props so it will show up before, and give it
        // the newline break marker.
        //
        temp = ARRAY_AT(backtrace, --index);
        if (pending) {
            //
            // You cannot (or should not) switch to inspect a pending frame,
            // as it is partially constructed.  It gets a "*" in the list
            // instead of a number.
            //
            Val_Init_Word_Unbound(temp, REB_WORD, SYM_ASTERISK);
        }
        else
            SET_INTEGER(temp, number);
        VAL_SET_OPT(temp, OPT_VALUE_LINE);
    }

    // If we ran out of stack levels before finding the single one requested
    // via /AT, return a NONE!
    //
    // !!! Would it be better to give an error?
    //
    if (REF(at))
        return R_NONE;

    // Return accumulated backtrace otherwise.  The reverse filling process
    // should have exactly used up all the index slots, leaving index at 0.
    //
    assert(index == 0);
    Val_Init_Block(D_OUT, backtrace);
    return R_OUT;
}


//
//  Call_For_Stack_Level: C
//
// !!! Unfortunate repetition of logic inside of BACKTRACE; find a way to
// unify the logic for omitting things like breakpoint frames, or either
// considering pending frames or not...
//
// Returns NULL if the given level number does not correspond to a running
// function on the stack.
//
struct Reb_Call *Call_For_Stack_Level(REBCNT level, REBOOL skip_current)
{
    struct Reb_Call *call = DSF;
    REBOOL first = TRUE;

    // We may need to skip some number of frames, if there have been stack
    // levels added since the numeric reference point that "level" was
    // supposed to refer to has changed.  For now that's only allowed to
    // be one level, because it's rather fuzzy which stack levels to
    // omit otherwise (pending? parens?)
    //
    if (skip_current)
        call = call->prior;

    for (; call != NULL; call = call->prior) {
        //
        // Exclude pending functions, parens...any Do_Core levels that are
        // not currently running functions.  (BACKTRACE includes pending
        // functions for display purposes, but does not number them.)
        //
        if (call->mode != CALL_MODE_FUNCTION) continue;

        if (
            first
            && IS_NATIVE(FUNC_VALUE(call->func))
            && FUNC_CODE(call->func) == &N_breakpoint
        ) {
            // this is considered the "0".  Return it only if 0 was requested
            // specifically (you don't "count down to it");
            //
            if (level == 0)
                return call;
            else {
                first = FALSE;
                continue; // don't count it, e.g. don't decrement level
            }
        }

        if (level == 0) {
            //
            // There really is no "level 0" in a stack unless you
            // are at a breakpoint.  Ordinary calls to backtrace
            // will get lists back that start at 1.
            //
            return NULL;
        }

        first = FALSE;

        --level;
        if (level == 0)
            return call;
    }

    return NULL;
}


//
// Index values for the properties in a "resume instruction" (see notes on
// REBNATIVE(resume))
//
enum {
    RESUME_INST_MODE = 0,   // FALSE if /WITH, TRUE if /DO, NONE! if default
    RESUME_INST_PAYLOAD,    // code block to /DO or value of /WITH
    RESUME_INST_TARGET,     // unwind target, NONE! to return from breakpoint
    RESUME_INST_MAX
};


//
//  Do_Breakpoint_Throws: C
//
// A call to Do_Breakpoint_Throws does delegation to a hook in the host, which
// (if registered) will generally start an interactive session for probing the
// environment at the break.  The `resume` native cooperates by being able to
// give back a value (or give back code to run to produce a value) that the
// call to breakpoint returns.
//
// RESUME has another feature, which is to be able to actually unwind and
// simulate a return /AT a function *further up the stack*.  (This may be
// switched to a feature of a "STEP OUT" command at some point.)
//
REBOOL Do_Breakpoint_Throws(
    REBVAL *out,
    REBOOL interrupted, // Ctrl-C (as opposed to a BREAKPOINT)
    const REBVAL *default_value,
    REBOOL do_default
) {
    REBVAL temp;
    REBVAL *target = NONE_VALUE;

    if (!PG_Breakpoint_Quitting_Hook) {
        //
        // Host did not register any breakpoint handler, so raise an error
        // about this as early as possible.
        //
        fail (Error(RE_HOST_NO_BREAKPOINT));
    }

    // We call the breakpoint hook in a loop, in order to keep running if any
    // inadvertent FAILs or THROWs occur during the interactive session.
    // Only a conscious call of RESUME speaks the protocol to break the loop.
    //
    while (TRUE) {
        struct Reb_State state;
        REBFRM *error;

    push_trap:
        PUSH_TRAP(&error, &state);

        // The host may return a block of code to execute, but cannot
        // while evaluating do a THROW or a FAIL that causes an effective
        // "resumption".  Halt is the exception, hence we PUSH_TRAP and
        // not PUSH_UNHALTABLE_TRAP.  QUIT is also an exception, but a
        // desire to quit is indicated by the return value of the breakpoint
        // hook (which may or may not decide to request a quit based on the
        // QUIT command being run).
        //
        // The core doesn't want to get involved in presenting UI, so if
        // an error makes it here and wasn't trapped by the host first that
        // is a bug in the host.  It should have done its own PUSH_TRAP.
        //
        if (error) {
        #if !defined(NDEBUG)
            REBVAL error_value;
            Val_Init_Error(&error_value, error);
            PROBE_MSG(&error_value, "Error not trapped during breakpoint:");
            Panic_Array(FRAME_VARLIST(error));
        #endif

            // In release builds, if an error managed to leak out of the
            // host's breakpoint hook somehow...just re-push the trap state
            // and try it again.
            //
            goto push_trap;
        }

        // Call the host's breakpoint hook.
        //
        if (PG_Breakpoint_Quitting_Hook(&temp, interrupted)) {
            //
            // If a breakpoint hook returns TRUE that means it wants to quit.
            // The value should be the /WITH value (as in QUIT/WITH)
            //
            assert(!THROWN(&temp));
            *out = *ROOT_QUIT_NATIVE;
            CONVERT_NAME_TO_THROWN(out, &temp, FALSE);
            return TRUE; // TRUE = threw
        }

        // If a breakpoint handler returns FALSE, then it should have passed
        // back a "resume instruction" triggered by a call like:
        //
        //     resume/do [fail "This is how to fail from a breakpoint"]
        //
        // So now that the handler is done, we will allow any code handed back
        // to do whatever FAIL it likes vs. trapping that here in a loop.
        //
        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

        // Decode and process the "resume instruction"
        {
            struct Reb_Call *call;
            REBVAL *mode;
            REBVAL *payload;

            assert(IS_PAREN(&temp));
            assert(VAL_LEN_HEAD(&temp) == RESUME_INST_MAX);

            mode = VAL_ARRAY_AT_HEAD(&temp, RESUME_INST_MODE);
            payload = VAL_ARRAY_AT_HEAD(&temp, RESUME_INST_PAYLOAD);
            target = VAL_ARRAY_AT_HEAD(&temp, RESUME_INST_TARGET);

            // The first thing we need to do is determine if the target we
            // want to return to has another breakpoint sandbox blocking
            // us.  If so, what we need to do is actually retransmit the
            // resume instruction so it can break that wall, vs. transform
            // it into an EXIT/FROM that would just get intercepted.
            //
            if (!IS_NONE(target)) {
            #if !defined(NDEBUG)
                REBOOL found = FALSE;
            #endif

                for (call = DSF; call != NULL; call = call->prior) {
                    if (call->mode != CALL_MODE_FUNCTION)
                        continue;

                    if (
                        call != DSF
                        && VAL_TYPE(FUNC_VALUE(call->func)) == REB_NATIVE
                        && FUNC_CODE(call->func) == &N_breakpoint
                    ) {
                        // We hit a breakpoint (that wasn't this call to
                        // breakpoint, at the current DSF) before finding
                        // the sought after target.  Retransmit the resume
                        // instruction so that level will get it instead.
                        //
                        *out = *ROOT_RESUME_NATIVE;
                        CONVERT_NAME_TO_THROWN(out, &temp, FALSE);
                        return TRUE; // TRUE = thrown
                    }

                    if (IS_OBJECT(target)) {
                        if (VAL_TYPE(FUNC_VALUE(call->func)) != REB_CLOSURE)
                            continue;
                        if (
                            VAL_FRAME(target) == AS_FRAME(call->arglist.array)
                        ) {
                            // Found a closure matching the target before we
                            // reached a breakpoint, no need to retransmit.
                            //
                        #if !defined(NDEBUG)
                            found = TRUE;
                        #endif
                            break;
                        }
                    }
                    else {
                        assert(ANY_FUNC(target));
                        if (VAL_TYPE(FUNC_VALUE(call->func)) == REB_CLOSURE)
                            continue;
                        if (VAL_FUNC(target) == call->func) {
                            //
                            // Found a function matching the target before we
                            // reached a breakpoint, no need to retransmit.
                            //
                        #if !defined(NDEBUG)
                            found = TRUE;
                        #endif
                            break;
                        }
                    }
                }

                // RESUME should not have been willing to use a target that
                // is not on the stack.
                //
            #if !defined(NDEBUG)
                assert(found);
            #endif
            }

            if (IS_NONE(mode)) {
                //
                // If the resume instruction had no /DO or /WITH of its own,
                // then it doesn't override whatever the breakpoint provided
                // as a default.  (If neither the breakpoint nor the resume
                // provided a /DO or a /WITH, result will be UNSET.)
                //
                goto return_default; // heeds `target`
            }

            assert(IS_LOGIC(mode));

            if (VAL_LOGIC(mode)) {
                if (DO_ARRAY_THROWS(&temp, payload)) {
                    //
                    // Throwing is not compatible with /AT currently.
                    //
                    if (!IS_NONE(target))
                        fail (Error_No_Catch_For_Throw(&temp));

                    // Just act as if the BREAKPOINT call itself threw
                    //
                    *out = temp;
                    return TRUE; // TRUE = thrown
                }

                // Ordinary evaluation result...
            }
            else
                temp = *payload;
        }

        // The resume instruction will be GC'd.
        //
        goto return_temp;
    }

    DEAD_END;

return_default:

    if (do_default) {
        if (DO_ARRAY_THROWS(&temp, default_value)) {
            //
            // If the code throws, we're no longer in the sandbox...so we
            // bubble it up.  Note that breakpoint runs this code at its
            // level... so even if you request a higher target, any throws
            // will be processed as if they originated at the BREAKPOINT
            // frame.  To do otherwise would require the EXIT/FROM protocol
            // to add support for DO-ing at the receiving point.
            //
            *out = temp;
            return TRUE; // TRUE = thrown
        }
    }
    else
        temp = *default_value; // generally UNSET! if no /WITH

return_temp:

    // The easy case is that we just want to return from breakpoint
    // directly, signaled by the target being NONE!.
    //
    if (IS_NONE(target)) {
        *out = temp;
        return FALSE; // FALSE = not thrown
    }

    // If the target is a function, then we're looking to simulate a return
    // from something up the stack.  This uses the same mechanic as
    // definitional returns--a throw named by the function or closure frame.
    //
    // !!! There is a weak spot in definitional returns for FUNCTION! that
    // they can only return to the most recent invocation; which is a weak
    // spot of FUNCTION! in general with stack relative variables.  Also,
    // natives do not currently respond to definitional returns...though
    // they can do so just as well as FUNCTION! can.
    //
    *out = *target;
    CONVERT_NAME_TO_THROWN(out, &temp, TRUE);

    return TRUE; // TRUE = thrown
}


//
//  breakpoint: native [
//
//  "Signal breakpoint to the host, such as a Read-Eval-Print-Loop (REPL)"
//
//      /with
//          "Return the given value if breakpoint does not trigger"
//      value [unset! any-value!]
//          "Default value to use"
//      /do
//          "Evaluate given code if breakpoint does not trigger"
//      code [block!]
//          "Default code to evaluate"
//  ]
//
REBNATIVE(breakpoint)
{
    REFINE(1, with);
    PARAM(2, value);
    REFINE(3, do);
    PARAM(4, code);

    if (REF(with) && REF(do)) {
        //
        // /WITH and /DO both dictate a default return result, (/DO evaluates
        // and /WITH does not)  They are mutually exclusive.
        //
        fail (Error(RE_BAD_REFINES));
    }

    if (Do_Breakpoint_Throws(
        D_OUT,
        FALSE, // not a Ctrl-C, it's an actual BREAKPOINT
        REF(with) ? ARG(value) : (REF(do) ? ARG(code) : UNSET_VALUE),
        REF(do)
    )) {
        return R_OUT_IS_THROWN;
    }

    return R_OUT;
}


//
//  resume: native [
//
//  {Resume after a breakpoint, can evaluate code in the breaking context.}
//
//      /with
//          "Return the given value as return value from BREAKPOINT"
//      value [unset! any-value!]
//          "Value to use"
//      /do
//          "Evaluate given code as return value from BREAKPOINT"
//      code [block!]
//          "Code to evaluate"
//      /at
//          "Return from another call up stack besides the breakpoint"
//      level [integer!]
//          "Stack level number in BACKTRACE to target in unwinding"
//  ]
//
REBNATIVE(resume)
//
// The host breakpoint hook makes a wall to prevent arbitrary THROWs and
// FAILs from ending the interactive inspection.  But RESUME is special, and
// it makes a very specific instruction (with a throw /NAME of the RESUME
// native) to signal a desire to end the interactive session.
//
// When the BREAKPOINT native gets control back from the hook, it interprets
// and executes the instruction.  This offers the additional benefit that
// each host doesn't have to rewrite interpretation in the hook--they only
// need to recognize a RESUME throw and pass the argument back.
{
    REFINE(1, with);
    PARAM(2, value);
    REFINE(3, do);
    PARAM(4, code);
    REFINE(5, at);
    PARAM(6, level);

    struct Reb_Call *target;
    REBARR *instruction;

    if (REF(with) && REF(do)) {
        //
        // /WITH and /DO both dictate a default return result, (/DO evaluates
        // and /WITH does not)  They are mutually exclusive.
        //
        fail (Error(RE_BAD_REFINES));
    }

    // We don't actually want to run the code for a /DO here.  If we tried
    // to run code from this stack level--and it failed or threw without
    // some special protocol--we'd stay stuck in the breakpoint's sandbox.
    //
    // The /DO code we received needs to actually be run by the host's
    // breakpoint hook, once it knows that non-local jumps to above the break
    // level (throws, returns, fails) actually intended to be "resuming".

    instruction = Make_Array(RESUME_INST_MAX);

    if (REF(with)) {
        SET_FALSE(ARRAY_AT(instruction, RESUME_INST_MODE)); // don't DO value
        *ARRAY_AT(instruction, RESUME_INST_PAYLOAD) = *ARG(value);
    }
    else if (REF(do)) {
        SET_TRUE(ARRAY_AT(instruction, RESUME_INST_MODE)); // DO the value
        *ARRAY_AT(instruction, RESUME_INST_PAYLOAD) = *ARG(code);
    }
    else
        SET_NONE(ARRAY_AT(instruction, RESUME_INST_MODE)); // use default

    if (REF(at)) {
        //
        // We want BREAKPOINT to resume /AT a higher stack level (using the
        // same machinery that definitionally-scoped return would to do it).

        if (VAL_INT32(ARG(level)) < 0)
            fail (Error_Invalid_Arg(ARG(level)));

        // !!! `target` should just be a "FRAME!" (once the new type exists)

        if (!(target = Call_For_Stack_Level(VAL_INT32(ARG(level)), TRUE)))
            fail (Error_Invalid_Arg(ARG(level)));

        if (IS_OBJECT(FUNC_VALUE(target->func))) {
            //
            // !!! A CLOSURE! instantiation can be successfully identified
            // by its frame, as it is a unique object.  Which is good, but
            // the associated costs suggest another approach which would be
            // cheaper and applicable to all functions...hence closure is
            // scheduled to be removed from Ren-C
            //
            Val_Init_Object(
                ARRAY_AT(instruction, RESUME_INST_TARGET),
                AS_FRAME(target->arglist.array)
            );
        }
        else {
            // See notes on OPT_VALUE_EXIT_FROM regarding non-CLOSURE!s and
            // their present inability to target arbitrary frames.
            //
            *ARRAY_AT(instruction, RESUME_INST_TARGET)
                = *FUNC_VALUE(target->func);
        }
    }
    else {
        // We just want BREAKPOINT itself to return, indicated by NONE target.
        //
        SET_NONE(ARRAY_AT(instruction, RESUME_INST_TARGET));
    }

    SET_ARRAY_LEN(instruction, RESUME_INST_MAX);
    TERM_ARRAY(instruction);

    // We put the resume instruction into a PAREN! just to make it a little
    // bit more unusual than a BLOCK!.  More hardened approaches might put
    // a special symbol as a "magic number" or somehow version the protocol,
    // but for now we'll assume that the only decoder is BREAKPOINT and it
    // will be kept in sync.
    //
    Val_Init_Array(D_CELL, REB_PAREN, instruction);

    // Throw the instruction with the name of the RESUME function
    //
    *D_OUT = *FUNC_VALUE(D_FUNC);
    CONVERT_NAME_TO_THROWN(D_OUT, D_CELL, FALSE);
    return R_OUT_IS_THROWN;
}


//
//  check: native [
//
//  "Run an integrity check on a value in debug builds of the interpreter"
//
//      value [any-value!]
//          {System will terminate abnormally if this value is corrupt.}
//  ]
//
REBNATIVE(check)
//
// This forces an integrity check to run on a series.  In R3-Alpha there was
// no debug build, so this was a simple validity check and it returned an
// error on not passing.  But Ren-C is designed to have a debug build with
// checks that aren't designed to fail gracefully.  So this just runs that
// assert rather than replicating code here that can "tolerate" a bad series.
// Review the necessity of this native.
{
    PARAM(1, value);

#ifdef NDEBUG
    fail (Error(RE_DEBUG_ONLY));
#else
    REBVAL *value = ARG(value);

    // !!! Should call generic ASSERT_VALUE macro with more cases
    //
    if (ANY_SERIES(value)) {
        ASSERT_SERIES(VAL_SERIES(value));
    }
    else if (ANY_CONTEXT(value)) {
        ASSERT_FRAME(VAL_FRAME(value));
    }
    else if (ANY_FUNC(value)) {
        ASSERT_ARRAY(VAL_FUNC_SPEC(value));
        ASSERT_ARRAY(VAL_FUNC_PARAMLIST(value));
        if (IS_FUNCTION(value) || IS_CLOSURE(value)) {
            ASSERT_ARRAY(VAL_FUNC_BODY(value));
        }
    }

    return R_TRUE;
#endif

}


//
//  ds: native [
//  "Temporary stack debug"
//      ; No arguments
//  ]
//
REBNATIVE(ds)
{
    Dump_Stack(0, 0);
    return R_UNSET;
}


//
//  do-codec: native [
//  
//  {Evaluate a CODEC function to encode or decode media types.}
//  
//      handle [handle!] "Internal link to codec"
//      action [word!] "Decode, encode, identify"
//      data [binary! image! string!]
//  ]
//
REBNATIVE(do_codec)
{
    REBCDI codi;
    REBVAL *val;
    REBINT result;
    REBSER *ser;

    CLEAR(&codi, sizeof(codi));

    codi.action = CODI_ACT_DECODE;

    val = D_ARG(3);

    switch (VAL_WORD_SYM(D_ARG(2))) {

    case SYM_IDENTIFY:
        codi.action = CODI_ACT_IDENTIFY;
    case SYM_DECODE:
        if (!IS_BINARY(val)) fail (Error(RE_INVALID_ARG, val));
        codi.data = VAL_BIN_AT(D_ARG(3));
        codi.len  = VAL_LEN_AT(D_ARG(3));
        break;

    case SYM_ENCODE:
        codi.action = CODI_ACT_ENCODE;
        if (IS_IMAGE(val)) {
            codi.extra.bits = VAL_IMAGE_BITS(val);
            codi.w = VAL_IMAGE_WIDE(val);
            codi.h = VAL_IMAGE_HIGH(val);
            codi.has_alpha = Image_Has_Alpha(val, FALSE) ? 1 : 0;
        }
        else if (IS_STRING(val)) {
            codi.w = VAL_SERIES_WIDTH(val);
            codi.len = VAL_LEN_AT(val);
            codi.extra.other = VAL_BIN_AT(val);
        }
        else
            fail (Error(RE_INVALID_ARG, val));
        break;

    default:
        fail (Error(RE_INVALID_ARG, D_ARG(2)));
    }

    // Nasty alias, but it must be done:
    // !!! add a check to validate the handle as a codec!!!!
    result = cast(codo, VAL_HANDLE_CODE(D_ARG(1)))(&codi);

    if (codi.error != 0) {
        if (result == CODI_CHECK) return R_FALSE;
        fail (Error(RE_BAD_MEDIA)); // need better!!!
    }

    switch (result) {

    case CODI_CHECK:
        return R_TRUE;

    case CODI_TEXT: //used on decode
        switch (codi.w) {
            default: /* some decoders might not set this field */
            case 1:
                ser = Make_Binary(codi.len);
                break;
            case 2:
                ser = Make_Unicode(codi.len);
                break;
        }
        memcpy(BIN_HEAD(ser), codi.data, codi.w? (codi.len * codi.w) : codi.len);
        SET_SERIES_LEN(ser, codi.len);
        Val_Init_String(D_OUT, ser);
        break;

    case CODI_BINARY: //used on encode
        ser = Make_Binary(codi.len);
        SET_SERIES_LEN(ser, codi.len);

        // optimize for pass-thru decoders, which leave codi.data NULL
        memcpy(
            BIN_HEAD(ser),
            codi.data ? codi.data : codi.extra.other,
            codi.len
        );
        Val_Init_Binary(D_OUT, ser);

        //don't free the text binary input buffer during decode (it's the 3rd arg value in fact)
        // See notice in reb-codec.h on reb_codec_image
        if (codi.data) {
            FREE_N(REBYTE, codi.len, codi.data);
        }
        break;

    case CODI_IMAGE: //used on decode
        ser = Make_Image(codi.w, codi.h, TRUE); // Puts it into RETURN stack position
        memcpy(IMG_DATA(ser), codi.extra.bits, codi.w * codi.h * 4);
        Val_Init_Image(D_OUT, ser);

        // See notice in reb-codec.h on reb_codec_image
        FREE_N(u32, codi.w * codi.h, codi.extra.bits);
        break;

    case CODI_BLOCK:
        Val_Init_Block(D_OUT, AS_ARRAY(codi.extra.other));
        break;

    default:
        fail (Error(RE_BAD_MEDIA)); // need better!!!
    }

    return R_OUT;
}
