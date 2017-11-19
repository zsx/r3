//
//  File: %n-system.c
//  Summary: "native functions for system operations"
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
    UNUSED(frame_);
    fail (VAL_CONTEXT(TASK_HALT_ERROR));
}


//
//  quit: native [
//
//  {Stop evaluating and return control to command shell or calling script.}
//
//      /with
//          {Yield a result (mapped to an integer if given to shell)}
//      value [any-value!]
//          "See: http://en.wikipedia.org/wiki/Exit_status"
//  ]
//
REBNATIVE(quit)
//
// QUIT is implemented via a THROWN() value that bubbles up through
// the stack.  It uses the value of its own native function as the
// name of the throw, like `throw/name value :quit`.
{
    INCLUDE_PARAMS_OF_QUIT;

    Move_Value(D_OUT, NAT_VALUE(quit));

    if (REF(with))
        CONVERT_NAME_TO_THROWN(D_OUT, ARG(value));
    else {
        // Chosen to do it this way because returning to a calling script it
        // will be no value by default, for parity with BREAK and EXIT without
        // a /WITH.  Long view would have RETURN work this way too: CC#2241

        // void translated to 0 if it gets caught for the shell, see #2241

        CONVERT_NAME_TO_THROWN(D_OUT, VOID_CELL);
    }

    return R_OUT_IS_THROWN;
}


//
//  exit-rebol: native [
//
//  {Stop the current Rebol interpreter, cannot be caught by CATCH/QUIT.}
//
//      /with
//          {Yield a result (mapped to an integer if given to shell)}
//      value [any-value!]
//          "See: http://en.wikipedia.org/wiki/Exit_status"
//  ]
//
REBNATIVE(exit_rebol)
{
    INCLUDE_PARAMS_OF_EXIT_REBOL;

    int code;
    if (REF(with))
        code = VAL_INT32(ARG(value));
    else
        code = EXIT_SUCCESS;

    exit(code);
}


//
//  recycle: native [
//
//  "Recycles unused memory."
//
//      return: [<opt> integer!]
//          {Number of series nodes recycled (if applicable)}
//      /off
//          "Disable auto-recycling"
//      /on
//          "Enable auto-recycling"
//      /ballast
//          "Trigger for auto-recycle (memory used)"
//      size [integer!]
//      /torture
//          "Constant recycle (for internal debugging)"
//      /watch
//          "Monitor recycling (debug only)"
//      /verbose
//          "Dump out information about series being recycled (debug only)"
//  ]
//
REBNATIVE(recycle)
{
    INCLUDE_PARAMS_OF_RECYCLE;

    if (REF(off)) {
        GC_Disabled = TRUE;
        return R_VOID;
    }

    if (REF(on)) {
        GC_Disabled = FALSE;
        VAL_INT64(TASK_BALLAST) = VAL_INT32(TASK_MAX_BALLAST);
    }

    if (REF(ballast)) {
        Move_Value(TASK_MAX_BALLAST, ARG(size));
        VAL_INT64(TASK_BALLAST) = VAL_INT32(TASK_MAX_BALLAST);
    }

    if (REF(torture)) {
        GC_Disabled = TRUE;
        VAL_INT64(TASK_BALLAST) = 0;
    }

    if (GC_Disabled)
        return R_VOID; // don't give back misleading "0", since no recycle ran

    REBCNT count;

    if (REF(verbose)) {
    #if defined(NDEBUG)
        fail (Error_Debug_Only_Raw());
    #else
        REBSER *sweeplist = Make_Series(100, sizeof(REBNOD*));
        count = Recycle_Core(FALSE, sweeplist);
        assert(count == SER_LEN(sweeplist));

        REBCNT index = 0;
        for (index = 0; index < count; ++index) {
            REBNOD *node = *SER_AT(REBNOD*, sweeplist, index);
            PROBE(node);
        }

        Free_Series(sweeplist);

        REBCNT recount = Recycle_Core(FALSE, NULL);
        assert(recount == count);
    #endif
    }
    else {
        count = Recycle();
    }

    if (REF(watch)) {
    #if defined(NDEBUG)
        fail (Error_Debug_Only_Raw());
    #else
        // There might should be some kind of generic way to set these kinds
        // of flags individually, perhaps having them live in SYSTEM/...
        //
        Reb_Opts->watch_recycle = NOT(Reb_Opts->watch_recycle);
        Reb_Opts->watch_expand = NOT(Reb_Opts->watch_expand);
    #endif
    }

    Init_Integer(D_OUT, count);
    return R_OUT;
}


//
//  panic: native [
//
//  "Cause abnormal termination of Rebol (dumps debug info in debug builds)"
//
//      value [string! error!]
//          "Error or message to report (evaluation not counted in ticks)"
//  ]
//
REBNATIVE(panic)
{
    INCLUDE_PARAMS_OF_PANIC;

    REBVAL *v = ARG(value);

    // panic() on the string value itself would report information about the
    // string cell...but panic() on UTF-8 character data assumes you mean to
    // report the contained message.  Use PANIC* if the latter is the intent.
    //
    const void *p;
    if (IS_STRING(v)) {
        REBCNT len = VAL_LEN_AT(v);
        REBCNT index = VAL_INDEX(v);
        REBSER *utf8 = Temp_Bin_Str_Managed(v, &index, &len);
        p = BIN_HEAD(utf8);
    }
    else {
        assert(IS_ERROR(v));
        p = v;
    }

    // Note that by using the frame's tick instead of TG_Tick, we don't count
    // the evaluation of the value argument.  Hence the tick count shown in
    // the dump would be the one that would queue up right to the exact moment
    // *before* the PANIC FUNCTION! was invoked.
    //
#ifdef NDEBUG
    panic_at (p, FRM_FILE(frame_), FRM_LINE(frame_));
#else
    Panic_Core (p, frame_->tick, FRM_FILE(frame_), FRM_LINE(frame_));
#endif
}


//
//  panic*: native [
//
//  "Cause abnormal termination of Rebol, with diagnostics on a value cell"
//
//      value [any-value!]
//          "Suspicious value to panic on (debug build shows diagnostics)"
//  ]
//
REBNATIVE(panic_p)
{
    INCLUDE_PARAMS_OF_PANIC_P;

    // Unlike PANIC, the PANIC* will panic directly on the value.  So instead
    // of displaying a message, PANIC* on a STRING! will show diagnostics of
    // where that string series was allocated (or freed, but that would only
    // happen if it were corrupt...since users shouldn't have freed nodes)
    //
    REBVAL *v = ARG(value);

    // Note that by using the frame's tick instead of TG_Tick, we don't count
    // the evaluation of the value argument.  Hence the tick count shown in
    // the dump would be the one that would queue up right to the exact moment
    // *before* the PANIC* FUNCTION! was invoked.
    //
#ifdef NDEBUG
    panic_at (v, FRM_FILE(frame_), FRM_LINE(frame_));
#else
    Panic_Core (v, frame_->tick, FRM_FILE(frame_), FRM_LINE(frame_));
#endif
}


//
//  limit-usage: native [
//
//  "Set a usage limit only once (used for SECURE)."
//
//      field [word!]
//          "eval (count) or memory (bytes)"
//      limit [any-number!]
//  ]
//
REBNATIVE(limit_usage)
{
    INCLUDE_PARAMS_OF_LIMIT_USAGE;

    REBSYM sym = VAL_WORD_SYM(ARG(field));

    // !!! comment said "Only gets set once"...why?
    //
    if (sym == SYM_EVAL) {
        if (Eval_Limit == 0)
            Eval_Limit = Int64(ARG(limit));
    }
    else if (sym == SYM_MEMORY) {
        if (PG_Mem_Limit == 0)
            PG_Mem_Limit = Int64(ARG(limit));
    }
    else
        fail (ARG(field));

    return R_VOID;
}


//
//  check: native [
//
//  "Run an integrity check on a value in debug builds of the interpreter"
//
//      value [<opt> any-value!]
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
    INCLUDE_PARAMS_OF_CHECK;

#ifdef NDEBUG
    UNUSED(ARG(value));

    fail (Error_Debug_Only_Raw());
#else
    REBVAL *value = ARG(value);

    // For starters, check the memory (if it's bad, all other bets are off)
    //
    Check_Memory_Debug();

    // !!! Should call generic ASSERT_VALUE macro with more cases
    //
    if (ANY_SERIES(value)) {
        ASSERT_SERIES(VAL_SERIES(value));
    }
    else if (ANY_CONTEXT(value)) {
        ASSERT_CONTEXT(VAL_CONTEXT(value));
    }
    else if (IS_FUNCTION(value)) {
        ASSERT_ARRAY(VAL_FUNC_PARAMLIST(value));
        ASSERT_ARRAY(VAL_ARRAY(VAL_FUNC_BODY(value)));
    }

    return R_TRUE;
#endif
}


// Fast count of number of binary digits in a number:
//
// https://stackoverflow.com/a/15327567/211160
//
int ceil_log2(unsigned long long x) {
    static const unsigned long long t[6] = {
        0xFFFFFFFF00000000ull,
        0x00000000FFFF0000ull,
        0x000000000000FF00ull,
        0x00000000000000F0ull,
        0x000000000000000Cull,
        0x0000000000000002ull
    };

    int y = (((x & (x - 1)) == 0) ? 0 : 1);
    int j = 32;
    int i;

    for (i = 0; i < 6; i++) {
    int k = (((x & t[i]) == 0) ? 0 : j);
        y += k;
        x >>= k;
        j >>= 1;
    }

    return y;
}


//
//  c-debug-break-at: native [
//
//  {Break at known evaluation point (only use when running under C debugger}
//
//      return: [<opt>]
//      tick [integer! blank!]
//          {Get from PANIC, REBFRM.tick, REBSER.tick, REBVAL.extra.tick}
//      /relative
//          {TICK parameter represents a count relative to the current tick}
//      /compensate
//          {Round tick up, as in https://math.stackexchange.com/q/2521219/}
// ]
//
REBNATIVE(c_debug_break_at)
{
    INCLUDE_PARAMS_OF_C_DEBUG_BREAK_AT;

#ifndef NDEBUG
    if (REF(compensate)) {
        //
        // Imagine two runs of Rebol console initialization.  In the first,
        // the tick count is 304 when C-DEBUG-BREAK/COMPENSATE is called,
        // right after command line parsing.  Later on a panic() is hit and
        // reports tick count 1020 in the crash log.
        //
        // Wishing to pick apart the bug before it happens, the Rebol Core
        // Developer then re-runs the program with `--breakpoint=1020`, hoping
        // to break at that tick, to catch the downstream appearance of the
        // tick in the panic().  But since command-line processing is in
        // usermode, the addition of the parameter throws off the ticks!
        //
        // https://en.wikipedia.org/wiki/Observer_effect_(physics)
        //
        // Let's say that after the command line processing, it still runs
        // C-DEBUG-BREAK/COMPENSATE, this time at tick 403.  Imagine our goal
        // is to make the parameter to /COMPENSATE something that can be used
        // to conservatively guess the same value to set the tick to, and
        // that /COMPENSATE ARG(bound) that gives a maximum of how far off we
        // could possibly be from the "real" tick. (e.g. "argument processing
        // took no more than 200 additional ticks", which this is consistent
        // with...since 403-304 = 99).
        //
        // The reasoning for why the formula below works for this rounding is
        // given in this StackExchange question and answer:
        //
        // https://math.stackexchange.com/q/2521219/
        //
        TG_Tick = (1 << (ceil_log2(TG_Tick) + 1)) + VAL_INT64(ARG(tick)) - 1;
        return R_VOID;
    }

    if (REF(relative))
        TG_Break_At_Tick = frame_->tick + 1 + VAL_INT64(ARG(tick));
    else
        TG_Break_At_Tick = VAL_INT64(ARG(tick));
    return R_VOID;
#else
    UNUSED(ARG(tick));
    UNUSED(ARG(relative));
    UNUSED(REF(compensate));

    fail (Error_Debug_Only_Raw());
#endif
}


//
//  c-debug-break: native [
//
//  "Break at next evaluation point (only use when running under C debugger)"
//
//      return: [<opt> any-value!]
//          {Invisibly returns what the expression to the right would have}
//      :value [<opt> <end> any-value!]
//          {The head cell of the code to evaluate after the break happens}
//  ]
//
REBNATIVE(c_debug_break)
{
    INCLUDE_PARAMS_OF_C_DEBUG_BREAK;

#ifndef NDEBUG
    TG_Break_At_Tick = frame_->tick + 1;

    // C-DEBUG-BREAK wants to appear invisible to the evaluator, so you can
    // use it at any position (like PROBE).  But unlike PROBE, it doesn't want
    // an evaluated argument...because that would defeat the purpose:
    //
    //    print c-debug-break mold value
    //
    // You would like the break to happen *before* the MOLD, not after it's
    // happened and been passed as an argument!)
    //
    // So we take a hard quoted parameter and then reuse the same mechanic
    // that EVAL does.  However, the evaluator is picky about voids...and will
    // assert if it ever is asked to "evaluate" one.  So squash the request
    // to evaluate if it's a void.
    //
    Move_Value(D_CELL, ARG(value));
    if (IS_VOID(D_CELL))
        SET_VAL_FLAG(D_CELL, VALUE_FLAG_EVAL_FLIP);

    return R_REEVALUATE_CELL;

#else
    UNUSED(ARG(value));

    fail (Error_Debug_Only_Raw());
#endif
}
