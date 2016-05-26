//
//  File: %c-signal.c
//  Summary: "Evaluator Interrupt Signal Handling"
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
// "Signal" refers to special events to process periodically during
// evaluation. Search for SET_SIGNAL to find them.
//
// (Note: Not to be confused with SIGINT and unix "signals", although on
// unix an evaluator signal can be triggered by a unix signal.)
//

#include "sys-core.h"


//
//  Do_Signals_Throws: C
//
// Currently the ability of a signal to THROW comes from the processing of
// breakpoints.  The RESUME instruction is able to execute code with /DO,
// and that code may escape from a debug interrupt signal (like Ctrl-C).
//
REBOOL Do_Signals_Throws(REBVAL *out)
{
    struct Reb_State state;
    REBCTX *error;

    REBCNT sigs;
    REBCNT mask;

    assert(Saved_State || PG_Boot_Phase < BOOT_MEZZ);

    // Accumulate evaluation counter and reset countdown:
    if (Eval_Count <= 0) {
        //Debug_Num("Poll:", (REBINT) Eval_Cycles);
        Eval_Cycles += Eval_Dose - Eval_Count;
        Eval_Count = Eval_Dose;
        if (Eval_Limit != 0 && Eval_Cycles > Eval_Limit)
            Check_Security(SYM_EVAL, POL_EXEC, 0);
    }

    if (!(Eval_Signals & Eval_Sigmask)) {
        SET_UNSET(out);
        return FALSE;
    }

    // Be careful of signal loops! EG: do not PRINT from here.
    sigs = Eval_Signals & (mask = Eval_Sigmask);
    Eval_Sigmask = 0;   // avoid infinite loop
    //Debug_Num("Signals:", Eval_Signals);

    // Check for recycle signal:
    if (GET_FLAG(sigs, SIG_RECYCLE)) {
        CLR_SIGNAL(SIG_RECYCLE);
        Recycle();
    }

#ifdef NOT_USED_INVESTIGATE
    if (GET_FLAG(sigs, SIG_EVENT_PORT)) {  // !!! Why not used?
        CLR_SIGNAL(SIG_EVENT_PORT);
        Awake_Event_Port();
    }
#endif

    // Breaking only allowed after MEZZ boot
    //
    if (GET_FLAG(sigs, SIG_INTERRUPT) && PG_Boot_Phase >= BOOT_MEZZ) {
        CLR_SIGNAL(SIG_INTERRUPT);
        Eval_Sigmask = mask;

        if (Do_Breakpoint_Throws(out, TRUE, VOID_CELL, FALSE))
            return TRUE;

        return FALSE;
    }

    // Halting only allowed after MEZZ boot
    //
    if (GET_FLAG(sigs, SIG_HALT) && PG_Boot_Phase >= BOOT_MEZZ) {
        CLR_SIGNAL(SIG_HALT);
        Eval_Sigmask = mask;

        fail (VAL_CONTEXT(TASK_HALT_ERROR));
    }

    Eval_Sigmask = mask;

    SET_UNSET(out);
    return FALSE;
}
