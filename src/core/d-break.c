//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
//=////////////////////////////////////////////////////////////////////////=//
//
//  Summary: Debug Breaking and Resumption
//  File: %d-break.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2016 Rebol Open Source Contributors
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
// This file contains interactive debugging support for breaking and
// resuming.  The instructions BREAKPOINT and PAUSE are natives which will
// call a host environment hook which can then begin an interactive debugging
// session.  During that time Rebol functions may continue to be called,
// though there is a sandbox which prevents the code from throwing or causing
// errors which will propagate past the breakpoint.  The only way to
// resume normal operation is with a "resume instruction".
//
// !!! Interactive debugging is a work in progress, and comments are in the
// functions below.
//

#include "sys-core.h"


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
// environment at the break.  The RESUME native cooperates by being able to
// give back a value (or give back code to run to produce a value) that the
// call to breakpoint returns.
//
// RESUME has another feature, which is to be able to actually unwind and
// simulate a return /AT a function *further up the stack*.  (This may be
// switched to a feature of a "step out" command at some point.)
//
REBOOL Do_Breakpoint_Throws(
    REBVAL *out,
    REBOOL interrupted, // Ctrl-C (as opposed to a BREAKPOINT)
    const REBVAL *default_value,
    REBOOL do_default
) {
    REBVAL *target = BLANK_VALUE;

    REBVAL temp;

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
        REBCTX *error;

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
            Panic_Array(CTX_VARLIST(error));
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
            // The value should be the /WITH value (as in QUIT/WITH), so
            // not actually a "resume instruction" in this case.
            //
            assert(!THROWN(&temp));
            *out = *NAT_VALUE(quit);
            CONVERT_NAME_TO_THROWN(out, &temp);
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
            REBFRM *frame;
            REBVAL *mode;
            REBVAL *payload;

            #if !defined(NDEBUG)
                REBOOL found = FALSE;
            #endif

            assert(IS_GROUP(&temp));
            assert(VAL_LEN_HEAD(&temp) == RESUME_INST_MAX);

            // The instruction was built from raw material, non-relative
            //
            mode = KNOWN(VAL_ARRAY_AT_HEAD(&temp, RESUME_INST_MODE));
            payload = KNOWN(VAL_ARRAY_AT_HEAD(&temp, RESUME_INST_PAYLOAD));
            target = KNOWN(VAL_ARRAY_AT_HEAD(&temp, RESUME_INST_TARGET));

            assert(IS_FRAME(target));

            //
            // The first thing we need to do is determine if the target we
            // want to return to has another breakpoint sandbox blocking
            // us.  If so, what we need to do is actually retransmit the
            // resume instruction so it can break that wall, vs. transform
            // it into an EXIT/FROM that would just get intercepted.
            //

            for (frame = FS_TOP; frame != NULL; frame = frame->prior) {
                if (NOT(Is_Any_Function_Frame(frame)))
                    continue;
                if (Is_Function_Frame_Fulfilling(frame))
                    continue;

                if (
                    frame != FS_TOP
                    && (
                        FUNC_DISPATCHER(frame->func) == &N_pause
                        || FUNC_DISPATCHER(frame->func) == &N_breakpoint
                    )
                ) {
                    // We hit a breakpoint (that wasn't this call to
                    // breakpoint, at the current FS_TOP) before finding
                    // the sought after target.  Retransmit the resume
                    // instruction so that level will get it instead.
                    //
                    *out = *NAT_VALUE(resume);
                    CONVERT_NAME_TO_THROWN(out, &temp);
                    return TRUE; // TRUE = thrown
                }

                // If the frame were the one we were looking for, it would be
                // reified (so it would have a context to match)
                //
                if (frame->varlist == NULL)
                    continue;

                if (VAL_CONTEXT(target) == AS_CONTEXT(frame->varlist)) {
                    // Found a match before hitting any breakpoints, so no
                    // need to retransmit.
                    //
                #if !defined(NDEBUG)
                    found = TRUE;
                #endif
                    break;
                }
            }

            // RESUME should not have been willing to use a target that
            // is not on the stack.
            //
        #if !defined(NDEBUG)
            assert(found);
        #endif

            if (IS_BLANK(mode)) {
                //
                // If the resume instruction had no /DO or /WITH of its own,
                // then it doesn't override whatever the breakpoint provided
                // as a default.  (If neither the breakpoint nor the resume
                // provided a /DO or a /WITH, result will be void.)
                //
                goto return_default; // heeds `target`
            }

            assert(IS_LOGIC(mode));

            if (VAL_LOGIC(mode)) {
                if (DO_VAL_ARRAY_AT_THROWS(&temp, payload)) {
                    //
                    // Throwing is not compatible with /AT currently.
                    //
                    if (!IS_BLANK(target))
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
        if (DO_VAL_ARRAY_AT_THROWS(&temp, default_value)) {
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
        temp = *default_value; // generally void if no /WITH

return_temp:
    //
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
    Make_Thrown_Exit_Value(out, target, &temp, NULL);

    return TRUE; // TRUE = thrown
}


//
//  breakpoint: native [
//
//  "Signal breakpoint to the host (simple variant of PAUSE dialect)"
//
//  ]
//
REBNATIVE(breakpoint)
//
// The reason BREAKPOINT needs to exist as a native is to be recognized by
// BACKTRACE as being a "0" stack level (e.g. probably not interesting to be
// where you are probing variables).  Backtrace should not *always* skip the
// most recent stack level however, because of a "Ctrl-C"-like debugging
// break, where the most recent stack level *is* the one to inspect.
{
    if (Do_Breakpoint_Throws(
        D_OUT,
        FALSE, // not a Ctrl-C, it's an actual BREAKPOINT
        VOID_CELL, // default result if RESUME does not override
        FALSE // !execute (don't try to evaluate the VOID_CELL)
    )) {
        return R_OUT_IS_THROWN;
    }

    return R_OUT;
}


//
//  pause: native [
//
//  "Pause in the debugger before running the provided code"
//
//      :code [group!] ;-- or LIT-WORD! name or BLOCK! for dialect
//          "Run the given code if breakpoint does not override"
//  ]
//
REBNATIVE(pause)
{
    PARAM(1, code);

    if (Do_Breakpoint_Throws(
        D_OUT,
        FALSE, // not a Ctrl-C, it's an actual BREAKPOINT
        ARG(code), // default result if RESUME does not override
        TRUE // execute (run the GROUP! as code, don't return as-is)
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
//      value [<opt> any-value!]
//          "Value to use"
//      /do
//          "Evaluate given code as return value from BREAKPOINT"
//      code [block!]
//          "Code to evaluate"
//      /at
//          "Return from another call up stack besides the breakpoint"
//      level [frame! function! integer!]
//          "Stack level to target in unwinding (can be BACKTRACE #)"
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

    REBARR *instruction;

    // We want BREAKPOINT to resume /AT a higher stack level (using the
    // same machinery that definitionally-scoped return would to do it).
    // Frames will be reified as necessary.
    //
    REBFRM *frame;

    REBVAL cell;

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
        SET_FALSE(ARR_AT(instruction, RESUME_INST_MODE)); // don't DO value
        *SINK(ARR_AT(instruction, RESUME_INST_PAYLOAD)) = *ARG(value);
    }
    else if (REF(do)) {
        SET_TRUE(ARR_AT(instruction, RESUME_INST_MODE)); // DO the value
        *SINK(ARR_AT(instruction, RESUME_INST_PAYLOAD)) = *ARG(code);
    }
    else {
        SET_BLANK(ARR_AT(instruction, RESUME_INST_MODE)); // use default

        // Even though this slot should be ignored, use BAR! to try and make
        // any attempts to use it more conspicuous (an unset wouldn't be)
        //
        SET_BAR(ARR_AT(instruction, RESUME_INST_PAYLOAD));
    }

    if (REF(at)) {
        //
        // `level` is currently allowed to be anything that backtrace can
        // handle (integers, functions for most recent call, literal FRAME!)

        if (!(frame = Frame_For_Stack_Level(NULL, ARG(level), TRUE)))
            fail (Error_Invalid_Arg(ARG(level)));

        // !!! It's possible to specify a context to return at which is
        // "underneath" a breakpoint.  So being at a breakpoint and doing
        // `if true [resume/at :if]` would try and specify the IF running
        // in the interactive breakpoint session.  The instruction will
        // error with no breakpoint to catch the resume...but a better error
        // could be given here if the case were detected early.
    }
    else {
        // We just want a BREAKPOINT or PAUSE themselves to return, so find
        // the most recent one (if any, error if none found).

        frame = FS_TOP;
        for (; frame != NULL; frame = frame->prior) {
            if (NOT(Is_Any_Function_Frame(frame))) continue;
            if (Is_Function_Frame_Fulfilling(frame)) continue;

            if (
                FUNC_DISPATCHER(frame->func) == &N_pause
                || FUNC_DISPATCHER(frame->func) == &N_breakpoint
            ) {
                break;
            }
        }

        if (frame == NULL)
            fail (Error(RE_NO_CURRENT_PAUSE));
    }

    Val_Init_Context(
        ARR_AT(instruction, RESUME_INST_TARGET),
        REB_FRAME,
        Context_For_Frame_May_Reify_Managed(frame)
    );

    TERM_ARRAY_LEN(instruction, RESUME_INST_MAX);

    // We put the resume instruction into a GROUP! just to make it a little
    // bit more unusual than a BLOCK!.  More hardened approaches might put
    // a special symbol as a "magic number" or somehow version the protocol,
    // but for now we'll assume that the only decoder is BREAKPOINT and it
    // will be kept in sync.
    //
    Val_Init_Array(&cell, REB_GROUP, instruction);

    // Throw the instruction with the name of the RESUME function
    //
    *D_OUT = *NAT_VALUE(resume);
    CONVERT_NAME_TO_THROWN(D_OUT, &cell);
    return R_OUT_IS_THROWN;
}
