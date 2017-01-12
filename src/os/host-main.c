//
//  File: %host-main.c
//  Summary: "Host environment main entry point"
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
// %host-main.c is the original entry point for the open-sourced R3-Alpha.
// Depending on whether it was POSIX or Windows, it would define either a
// `main()` or `WinMain()`, and implemented a very rudimentary console.
//
// On POSIX systems it uses <termios.h> to implement line editing:
//
// http://pubs.opengroup.org/onlinepubs/7908799/xbd/termios.html
//
// On Windows it uses the Console API:
//
// https://msdn.microsoft.com/en-us/library/ms682087.aspx
//
// !!! Originally %host-main.c was a client of the %reb-host.h (RL_Api).  It
// did not have access to things like the definition of a REBVAL or a REBSER.
// The sparse and convoluted nature of the RL_Api presented an awkward
// barrier, and the "sample console" stagnated as a result.
//
// In lieu of a suitable "abstracted" variant of the core services--be that
// an evolution of RL_Api or otherwise--the console now links directly
// against the Ren-C core.  This provides full access to the routines and
// hooks necessary to evolve the console if one were interested.  (The GUI
// inteface Ren Garden is the flagship console for Ren-C, so that is where
// most investment will be made.)
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
    //
    // On Windows it is required to include <windows.h>, and defining the
    // _WIN32_WINNT constant to 0x0501 specifies the minimum targeted version
    // is Windows XP.  This is the earliest platform API still supported by
    // Visual Studio 2015:
    //
    //     https://msdn.microsoft.com/en-us/library/6sehtctf.aspx
    //
    // R3-Alpha used 0x0500, indicating a minimum target of Windows 2000.  No
    // Windows-XP-specific dependencies were added in Ren-C, but the version
    // was bumped to avoid compilation errors in the common case.
    //
    // !!! Note that %sys-core.h includes <windows.h> as well if building
    // for windows.  The redundant inclusion should not create a problem.
    // (So better to do the inclusion just to test that it doesn't.)
    //
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501
    #include <windows.h>

    // Put any dependencies that include <windows.h> here
    //
    /* #include "..." */
    /* #include "..." */

    // Undefine the Windows version of IS_ERROR to avoid compiler warning
    // when Rebol redefines it.  (Rebol defines IS_XXX for all datatypes.)
    //
    #undef IS_ERROR
    #undef max
    #undef min
#endif


#include "sys-core.h"
#ifdef __cplusplus
extern "C" {
#endif
    extern void RL_Version(REBYTE vers[]);
    extern void RL_Init(void *lib);
    extern void RL_Shutdown(REBOOL clean);

    extern REBOL_HOST_LIB Host_Lib_Init;
#ifdef __cplusplus
}
#endif


// The initialization done by RL_Init() is intended to be as basic as possible
// in order to get the Rebol series/values/array functions ready to be run.
// Once that's ready, the rest of the initialization can take advantage of
// a working evaluator.  This includes PARSE to process the command line
// parameters, or PRINT to output boot banners.
//
// The %make-host-init.r file takes the %host-start.r script and turns it
// into a compressed binary C literal.  That literal can be LOADed and
// executed to return the HOST-START function, which takes the command line
// arguments as an array of STRING! and handles it from there.
//
#include "tmp-host-start.inc"


const REBYTE halt_str[] = "[escape]";
const REBYTE prompt_str[] = ">> ";
const REBYTE result_str[] = "== ";
const REBYTE why_str[] = "** Note: use WHY? for more error information\n";
const REBYTE breakpoint_str[] =
    "** Breakpoint Hit (see BACKTRACE, DEBUG, and RESUME)\n";
const REBYTE interrupted_str[] =
    "** Execution Interrupted (see BACKTRACE, DEBUG, and RESUME)\n";

#ifndef REB_CORE
extern void Init_Windows(void);
extern void OS_Init_Graphics(void);
extern void OS_Destroy_Graphics(void);
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TO_WINDOWS
    HINSTANCE App_Instance = 0;
#endif

#ifdef __cplusplus
}
#endif


//#define TEST_EXTENSIONS
#ifdef TEST_EXTENSIONS
extern void Init_Ext_Test(void);    // see: host-ext-test.c
#endif

// Host bare-bones stdio functs:
extern void Open_StdIO(void);
extern void Close_StdIO(void);
extern void Put_Str(const REBYTE *buf);
extern REBYTE *Get_Str();


/* coverity[+kill] */
void Host_Crash(const char *reason) {
    OS_CRASH(cb_cast("REBOL Host Failure"), cb_cast(reason));
}


// Current stack level displayed in the REPL, where bindings are assumed to
// be made for evaluations.  So if the prompt reads `[3]>>`, and a string
// of text is typed in to be loaded as code, that code will be bound to
// the user context, then the lib context, then to the variables of whatever
// function is located at stack level 3.
//
extern REBCNT HG_Stack_Level;
REBCNT HG_Stack_Level = 1;


// The DEBUG command is a host-specific "native", which modifies state that
// is specific to controlling variables and behaviors in the REPL.  Since
// the core itself seeks to avoid having any UI and only provide evaluation
// services, C code for DEBUG must either be within the host, or the DEBUG
// native would need to implement an abstract protocol that could make
// callbacks into the host.
//
// A standard or library might evolve so that every host does not reimplement
// the debug logic.  However, much of the debugging behavior depends on the
// nature of the host (textual vs. GUI), as well as being able to modify
// state known to the host and not the core.  So for the moment, DEBUG is
// implemented entirely in the host...while commands like BREAKPOINT have
// their implementation in the core with a callback to the host to implement
// the host-specific portion.
//
// !!! Can the REBNATIVE with source-in-comment declaration style be
// something that non-core code can use, vs. this handmade variant?
//
const REBYTE N_debug_spec[] =
    " {Dialect for interactive debugging, see documentation for details}"
    " 'value [_ integer! frame! function! block!]"
        " {Stack level to inspect or dialect block, or enter debug mode}"
    "";
REB_R N_debug(REBFRM *frame_) {
    PARAM(1, value); // no automatic INCLUDE_PARAMS_OF_XXX for manual native

    REBVAL *value = ARG(value);

    if (IS_VOID(value)) {
        //
        // e.g. just `>> debug` and [enter] in the console.  Ideally this
        // would shift the REPL into a mode where all commands issued were
        // assumed to be in the debug dialect, similar to Ren Garden's
        // modalities like `debug>>`.
        //
        Debug_Fmt("Sorry, there is no debug>> 'mode' yet in the console.");
        goto modify_with_confidence;
    }

    if (IS_INTEGER(value) || IS_FRAME(value) || IS_FUNCTION(value)) {
        REBFRM *frame;

        // We pass TRUE here to account for an extra stack level... the one
        // added by DEBUG itself, which presumably should not count.
        //
        if (!(frame = Frame_For_Stack_Level(&HG_Stack_Level, value, TRUE)))
            fail (Error_Invalid_Arg(value));

        Init_Block(D_OUT, Make_Where_For_Frame(frame));
        return R_OUT;
    }

    assert(IS_BLOCK(value));

    Debug_Fmt(
        "Sorry, but the `debug [...]` dialect is not defined yet.\n"
        "Change the stack level (integer!, frame!, function!)\n"
        "Or try out these commands:\n"
        "\n"
        "    BREAKPOINT, RESUME, BACKTRACE\n"
    );

modify_with_confidence:
    Debug_Fmt(
        "(Note: Ren-C is 'modify-with-confidence'...so just because a debug\n"
        "feature you want isn't implemented doesn't mean you can't add it!)\n"
    );

    return R_BLANK;
}


//
//  Do_String()
//
// This is a version of a routine that was offered by the RL_Api, which has
// been expanded here in order to permit the necessary customizations for
// interesting REPL behavior w.r.t. binding, error handling, and response
// to throws.
//
// !!! Now that this code has been moved into the host, the convoluted
// integer-return-scheme can be eliminated and the code integrated more
// clearly into the surrounding calls.
//
int Do_String(
    int *exit_status,
    REBVAL *out,
    const REBYTE *text,
    REBOOL at_breakpoint
) {
    struct Reb_State state;
    REBCTX *error;

    // Breakpoint REPLs are nested, and we may wish to jump out of them to
    // the topmost level via a HALT.  However, all other errors need to be
    // confined, so that if one is doing evaluations during the pause of
    // a breakpoint an error doesn't "accidentally resume" by virtue of
    // jumping the stack out of the REPL.
    //
    // The topmost layer REPL, however, needs to catch halts in order to
    // keep control and not crash out.
    //
    if (at_breakpoint)
        PUSH_TRAP(&error, &state);
    else
        PUSH_UNHALTABLE_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) {
        // Save error for WHY?
        REBVAL *last = Get_System(SYS_STATE, STATE_LAST_ERROR);

        if (ERR_NUM(error) == RE_HALT) {
            assert(!at_breakpoint);
            return -1; // !!! Revisit hardcoded #
        }

        Init_Error(out, error);
        *last = *out;
        return -cast(REBINT, ERR_NUM(error));
    }

    REBARR *code = Scan_UTF8_Managed(text, LEN_BYTES(text));

    // Where code ends up being bound when loaded at the REPL prompt should
    // be more generally configurable.  (It may be, for instance, that one
    // wants to run something with it not bound at all.)  Such choices
    // must come from this REPL host...not from the interpreter itself.
    {
        // First the scanned code is bound into the user context with a
        // fallback to the lib context.
        //
        // !!! This code is very old, and is how the REPL has bound since
        // R3-Alpha.  It comes from RL_Do_String, but should receive a modern
        // review of why it's written exactly this way.
        //
        REBCTX *user_ctx = VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_USER));

        REBVAL vali;
        SET_INTEGER(&vali, CTX_LEN(user_ctx) + 1);

        Bind_Values_All_Deep(ARR_HEAD(code), user_ctx);
        Resolve_Context(user_ctx, Lib_Context, &vali, FALSE, FALSE);

        // If we're stopped at a breakpoint, the REPL should have a concept
        // of what stack level it is inspecting (conveyed by the |#|>> in the
        // prompt).  This does a binding pass using the function for that
        // stack level, just the way a body is bound during Make_Function()
        //
        if (at_breakpoint) {
            REBVAL level;
            SET_INTEGER(&level, HG_Stack_Level);

            REBFRM *frame = Frame_For_Stack_Level(NULL, &level, FALSE);
            assert(frame);

            // Need to manage because it may be no words get bound into it,
            // and we're not putting it into a FRAME! value, so it might leak
            // otherwise if it's reified.
            //
            REBCTX *frame_ctx = Context_For_Frame_May_Reify_Managed(frame);

            Bind_Values_Deep(ARR_HEAD(code), frame_ctx);
        }

        // !!! This was unused code that used to be in Do_String from
        // RL_Api.  It was an alternative path under `flags` which said
        // "Bind into lib or user spaces?" and then "Top words will be
        // added to lib".  Is it relevant in any way?
        //
        /* Bind_Values_Set_Midstream_Shallow(ARR_HEAD(code), Lib_Context);
        Bind_Values_Deep(ARR_HEAD(code), Lib_Context); */
    }

    // The new policy for source code in Ren-C is that it loads read only.
    // This didn't go through the LOAD Rebol function (should it?  it never
    // did before.  :-/)  For now, stick with simple binding but lock it.
    //
#if defined(NDEBUG)
    Deep_Freeze_Array(code);
#else
    if (!LEGACY(OPTIONS_UNLOCKED_SOURCE))
        Deep_Freeze_Array(code);
#endif

    if (Do_At_Throws(out, code, 0, SPECIFIED)) { // array gets GC protected
        if (at_breakpoint) {
            if (
                IS_FUNCTION(out)
                && VAL_FUNC_DISPATCHER(out) == &N_resume
            ) {
                //
                // This means we're done with the embedded REPL.  We want to
                // resume and may be returning a piece of code that will be
                // run by the finishing BREAKPOINT command in the target
                // environment.
                //
                // We'll never return a halt, so we reuse -1 (in this very
                // temporary scheme built on the very clunky historical REPL,
                // which will not last much longer...fingers crossed.)
                //
                DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
                CATCH_THROWN(out, out);
                *exit_status = -1;
                return -1;
            }

            if (
                IS_FUNCTION(out)
                && VAL_FUNC_DISPATCHER(out) == &N_quit
            ) {
                //
                // It would be frustrating if the system did not respond to
                // a QUIT and forced you to do `resume/with [quit]`.  So
                // this is *not* caught, rather passed back up with the
                // special -2 status code.
                //
                DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
                CATCH_THROWN(out, out);
                *exit_status = -2;
                return -2;
            }
        }
        else {
            // We are at the top level REPL, where we catch QUIT
            //
            if (
                IS_FUNCTION(out)
                && VAL_FUNC_DISPATCHER(out) == &N_quit
            ) {
                DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
                CATCH_THROWN(out, out);
                *exit_status = Exit_Status_From_Value(out);
                return -2; // Revisit hardcoded #
            }
        }

        fail (Error_No_Catch_For_Throw(out));
    }

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    return 0;
}


void Host_Repl(int *exit_status, REBVAL *out, REBOOL at_breakpoint) {
    REBOOL why_alert = TRUE;

    #define MAX_CONT_LEVEL 80
    REBYTE cont_str[] = "    ";
    int cont_level = 0;
    REBYTE cont_stack[MAX_CONT_LEVEL] = {0};

    int input_max = 32768;
    int input_len = 0;
    REBYTE *input = OS_ALLOC_N(REBYTE, input_max);

    REBYTE *line;
    int line_len;

    REBYTE *utf8byte;
    REBOOL inside_short_str = FALSE;
    int long_str_level = 0;

    while (TRUE) {
        int do_result;

        if (cont_level > 0) {
            int level;

            cont_str[0] = cont_stack[cont_level - 1];
            Put_Str(cont_str);

            cont_str[0] = ' ';
            for (level = 1; level < cont_level; level++) {
                Put_Str(cont_str);
            }
        }
        else {
            Put_Str(cb_cast("\n"));
            if (at_breakpoint) {
                //
                // If we're stopped at a breakpoint, then the REPL has a
                // modality to it of "which stack level you are examining".
                // This is conveyed through an integer of the stack depth,
                // which is put into the prompt:
                //
                //     |3|>> ...
                //
                REBYTE buf_int[MAX_INT_LEN];
                Put_Str(cb_cast("|"));
                Form_Int(&buf_int[0], HG_Stack_Level);
                Put_Str(buf_int);
                Put_Str(cb_cast("|"));
            }
            Put_Str(prompt_str);
        }

        line = Get_Str();

        if (!line) {
            // !!! "end of stream"...is this a normal exit result or
            // should we be returning some error here?  0 status for now
            *exit_status = 0;
            goto cleanup_and_return;
        }

        line_len = 0;
        for (utf8byte = line; *utf8byte; utf8byte++) {
            line_len++;
            switch (*utf8byte) {
                case '"':
                    if (long_str_level == 0) {
                        inside_short_str = NOT(inside_short_str);
                    }
                    break;
                case '[':
                case '(':
                    if (!inside_short_str && long_str_level == 0) {
                        cont_stack[cont_level++] = *utf8byte;
                        if (cont_level >= MAX_CONT_LEVEL) {
                            OS_FREE(input);
                            Host_Crash("Maximum console continuation level exceeded!");
                        }
                    }
                    break;
                case ']':
                case ')':
                    if (!inside_short_str && long_str_level == 0) {
                        if (cont_level > 0) {
                            cont_stack[--cont_level] = 0;
                        }
                    }
                    break;
                case '{':
                    if (!inside_short_str) {
                        cont_stack[cont_level++] = *utf8byte;
                        if (cont_level >= MAX_CONT_LEVEL) {
                            OS_FREE(input);
                            Host_Crash("Maximum console continuation level exceeded!");
                        }
                        long_str_level++;
                    }
                    break;
                case '}':
                    if (!inside_short_str) {
                        if (cont_level > 0) {
                            cont_stack[--cont_level] = 0;
                        }
                        if (long_str_level > 0) {
                            long_str_level--;
                        }
                    }
                    break;
            }
        }
        inside_short_str = FALSE;

        if (input_len + line_len > input_max) {
            REBYTE *tmp = OS_ALLOC_N(REBYTE, 2 * input_max);
            if (!tmp) {
                OS_FREE(input);
                Host_Crash("Growing console input buffer failed!");
            }
            memcpy(tmp, input, input_len);
            OS_FREE(input);
            input = tmp;
            input_max *= 2;
        }

        memcpy(&input[input_len], line, line_len);
        input_len += line_len;
        input[input_len] = 0;

        OS_FREE(line);

        if (cont_level > 0)
            continue;

        input_len = 0;
        cont_level = 0;

        do_result = Do_String(exit_status, out, input, at_breakpoint);

        // NOTE: Although the operation has finished at this point, it may
        // be that a Ctrl-C set up a pending FAIL, which will be triggered
        // during output below.  See the PUSH_UNHALTABLE_TRAP in the caller.

        if (do_result == -1) {
            //
            // If we're inside a breakpoint, this actually means "resume",
            // because Do_String doesn't do any error trapping if we pass
            // in `at_breakpoint = TRUE`.  Hence any HALT longjmp would
            // have bypassed this, so the -1 signal is reused (for now).
            //
            if (at_breakpoint)
                goto cleanup_and_return;

            // !!! The "Halt" status is communicated via -1, but
            // is not an actual valid "error value".  It cannot be
            // created by user code, and the fact that it is done
            // via the error mechanism is an "implementation detail".
            //
            Put_Str(halt_str);
        }
        else if (do_result == -2) {
            // Command issued a purposeful QUIT or EXIT, exit_status
            // contains status.  Assume nothing was pushed on stack
            goto cleanup_and_return;
        }
        else if (do_result < -2) {
            // Error occurred, print it without molding (formed)
            //
            Out_Value(out, 500, FALSE, 1);

            // Tell them about why on the first error only
            if (why_alert) {
                Put_Str(why_str);
                why_alert = FALSE;
            }
        }
        else {
            assert(do_result >= 0);

            // There was no error.  If the value on top of stack is an unset
            // then nothing will be printed, otherwise print it out.
            //
            if (!IS_VOID(out)) {
                Out_Str(result_str, 0); // "=="
                Out_Value(out, 500, TRUE, 1);
            }
        }
    }

cleanup_and_return:
    OS_FREE(input);
    return;
}


//
//  Host_Breakpoint_Quitting_Hook()
//
// This hook is registered with the core as the function that gets called
// when a breakpoint triggers.
//
// There are only two options for leaving the hook.  One is to return TRUE
// and thus signal a QUIT, where `instruction` is the value to quit /WITH.
// The other choice is to return FALSE, where `instruction` is a purposefully
// constructed "resume instruction".
//
// (Note: See remarks in the implementation of `REBNATIVE(resume)` for the
// format of resume instructions.  But generally speaking, the host does not
// need to know the details, as this represents a protocol that is supposed
// to only be between BREAKPOINT and RESUME.  So the host just needs to
// bubble up the argument to a throw that had the RESUME native's name on it,
// when that type of throw is caught.)
//
// The ways in which a breakpoint hook can be exited are constrained in
// order to "sandbox" it somewhat.  Though a nested REPL may be invoked in
// response to a breakpoint--as is done here--continuation should be done
// purposefully vs. "accidentally resuming" just because a FAIL or a THROW
// happened.  One does not want to hit a breakpoint, then mistype a variable
// name and trigger an error that does a longjmp that effectively cancels
// the interactive breakpoint session!
//
// Hence RESUME and QUIT should be the only ways to get out of the breakpoint.
// Note that RESUME/DO provides a loophole, where it's possible to run code
// that performs a THROW or FAIL which is not trapped by the sandbox.
//
REBOOL Host_Breakpoint_Quitting_Hook(
    REBVAL *instruction_out,
    REBOOL interrupted
) {
    // Notify the user that the breakpoint or interruption was hit.
    //
    if (interrupted)
        Put_Str(interrupted_str);
    else
        Put_Str(breakpoint_str);

    // We save the stack level from before, so that we can put it back when
    // we resume.  Each new breakpoint nesting hit will default to debugging
    // stack level 1...e.g. the level that called breakpoint.
    //
    REBCNT old_stack_level = HG_Stack_Level;

    REBVAL level;
    SET_INTEGER(&level, 1);

    if (Frame_For_Stack_Level(NULL, &level, FALSE) != NULL)
        HG_Stack_Level = 1;
    else
        HG_Stack_Level = 0; // Happens if you just type "breakpoint"

    // Spawn nested REPL.
    //
    int exit_status;
    Host_Repl(&exit_status, instruction_out, TRUE);

    // Restore stack level, which is presumably still valid (there shouldn't
    // have been any way to "delete levels from the stack above" while we
    // were nested).
    //
    // !!! It might be nice if the prompt had a way of conveying that you were
    // in nested breaks, and give the numberings of them adjusted:
    //
    //     |14|6|1|>> ...
    //
    // Or maybe that's TMI?
    //
    HG_Stack_Level = old_stack_level;

    // We get -1 for RESUME and -2 for QUIT, under the current convoluted
    // scheme of return codes.
    //
    // !!! Eliminate return codes now that RL_Api dependence is gone and
    // speak in terms of the REBVALs themselves.
    //
    assert(exit_status == -1 || exit_status == -2);
    return LOGICAL(exit_status == -2);
}


// Register host-specific DEBUG native in user and lib contexts.  (See
// notes on N_debug regarding why the C code implementing DEBUG is in
// the host and not part of Rebol Core.)
//
void Init_Debug_Extension(void) {
    const REBYTE debug_utf8[] = "debug";
    REBSTR *debug_name = Intern_UTF8_Managed(debug_utf8, LEN_BYTES(debug_utf8));

    REBCTX *user_context = VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_USER));
    if (
        0 == Find_Canon_In_Context(Lib_Context, STR_CANON(debug_name), TRUE) &&
        0 == Find_Canon_In_Context(user_context, STR_CANON(debug_name), TRUE)
    ) {
        REBARR *spec_array = Scan_UTF8_Managed(
            N_debug_spec, LEN_BYTES(N_debug_spec)
        );
        REBVAL spec;
        Init_Block(&spec, spec_array);
        Bind_Values_Deep(ARR_HEAD(spec_array), Lib_Context);

        REBFUN *debug_native = Make_Function(
            Make_Paramlist_Managed_May_Fail(&spec, MKF_KEYWORDS),
            &N_debug,
            NULL // no underlying function, this is fundamental
        );

        *Append_Context(Lib_Context, 0, debug_name) = *FUNC_VALUE(debug_native);
        *Append_Context(user_context, 0, debug_name) = *FUNC_VALUE(debug_native);
    }
    else {
        // It's already there--e.g. someone added REBNATIVE(debug).  Assert
        // about it in the debug build, otherwise don't add the host version.
        //
        assert(FALSE);
    }
}


/***********************************************************************
**
**  MAIN ENTRY POINT
**
**  Win32 args:
**      inst:  current instance of the application (app handle)
**      prior: always NULL (use a mutex for single inst of app)
**      cmd:   command line string (or use GetCommandLine)
**      show:  how app window is to be shown (e.g. maximize, minimize, etc.)
**
**  Win32 return:
**      If the function succeeds, terminating when it receives a WM_QUIT
**      message, it should return the exit value contained in that
**      message's wParam parameter. If the function terminates before
**      entering the message loop, it should return zero.
**
**  Posix args: as you would expect in C.
**  Posix return: ditto.
**
*/
/***********************************************************************/

// Using a main entry point for a console program (as opposed to WinMain)
// so that we can connect to the console.  See the StackOverflow question
// "Can one executable be both a console and a GUI application":
//
//     http://stackoverflow.com/questions/493536/
//
// int WINAPI WinMain(HINSTANCE inst, HINSTANCE prior, LPSTR cmd, int show)

int main(int argc, char **argv_ansi)
{
    // Must be done before an console I/O can occur. Does not use reb-lib,
    // so this device should open even if there are other problems.
    //
    Open_StdIO();  // also sets up interrupt handler

    Host_Lib = &Host_Lib_Init;
    RL_Init(Host_Lib);

    // With basic initialization done, we want to turn the platform-dependent
    // argument strings into a block of Rebol strings as soon as possible.
    // That way the command line argument processing can be taken care of by
    // PARSE instead of C code!
    //
    REBARR *argv = Make_Array(argc);

#ifdef TO_WINDOWS
    //
    // Were we using WinMain we'd be getting our arguments in Unicode, but
    // since we're using an ordinary main() we do not.  However, this call
    // lets us slip out and pick up the arguments in Unicode form.
    //
    wchar_t **argv_utf16 = cast(
        wchar_t**, CommandLineToArgvW(GetCommandLineW(), &argc)
    );
    int i = 0;
    for (; i < argc; ++i) {
        if (argv_utf16[i] == NULL)
            continue; // shell bug

        static_assert_c(sizeof(REBUNI) == sizeof(wchar_t));

        Init_String(
            Alloc_Tail_Array(argv),
            Make_UTF16_May_Fail(cast(REBUNI*, argv_utf16[i]))
        );
    }
#else
    // Assume no wide character support, and just take the ANSI C args, which
    // should ideally be in UTF8
    //
    int i = 0;
    for (; i < argc; ++i) {
        if (argv_ansi[i] == NULL)
            continue; // shell bug

        Init_String(
            Alloc_Tail_Array(argv), Make_UTF8_May_Fail(argv_ansi[i])
        );
    }
#endif

    // !!! Register EXPERIMENTAL breakpoint hook.  Note that %host-main.c is
    // not really expected to stick around as the main REPL...
    //
    PG_Breakpoint_Quitting_Hook = &Host_Breakpoint_Quitting_Hook;

    REBVAL argv_value;
    Init_Block(&argv_value, argv);
    PUSH_GUARD_VALUE(&argv_value);

#ifdef TEST_EXTENSIONS
    Init_Ext_Test();
#endif

#ifdef TO_WINDOWS
    // no console, we must be the child process
    if (GetStdHandle(STD_OUTPUT_HANDLE) == 0)
    {
        App_Instance = GetModuleHandle(NULL);
    }
#ifdef REB_CORE
    else //use always the console for R3/core
    {
        // GetWindowsLongPtr support 32 & 64 bit windows
        App_Instance = (HINSTANCE)GetWindowLongPtr(GetConsoleWindow(), GWLP_HINSTANCE);
    }
#else
    //followinng R3/view code behaviors when compiled as:
    //-"console app" mode: stdio redirection works but blinking console window during start
    //-"GUI app" mode stdio redirection doesn't work properly, no blinking console window during start
    else if (argc > 1) // we have command line args
    {
        // GetWindowsLongPtr support 32 & 64 bit windows
        App_Instance = (HINSTANCE)GetWindowLongPtr(GetConsoleWindow(), GWLP_HINSTANCE);
    }
    else // no command line args but a console - launch child process so GUI is initialized and exit
    {
        DWORD dwCreationFlags = CREATE_DEFAULT_ERROR_MODE | DETACHED_PROCESS;
        STARTUPINFO startinfo;
        PROCESS_INFORMATION procinfo;
        ZeroMemory(&startinfo, sizeof(startinfo));
        startinfo.cb = sizeof(startinfo);
        if (!CreateProcess(NULL, argv[0], NULL, NULL, FALSE, dwCreationFlags, NULL, NULL, &startinfo, &procinfo))
            MessageBox(0, L"CreateProcess() failed :(", L"", 0);
        exit(0);
    }
#endif //REB_CORE
#endif //TO_WINDOWS

    // Common code for console & GUI version
#ifndef REB_CORE
    Init_Windows();
    OS_Init_Graphics();
#endif // REB_CORE

    Init_Debug_Extension();

    REBVAL result;

    REBSER *startup = Decompress(
        &Reb_Init_Code[0],
        REB_INIT_SIZE,
        -1,
        FALSE,
        FALSE
    );
    if (startup == NULL)
        panic ("Can't decompress %host-start.r code linked into executable");

    REBSER *embedded = NULL;
    REBI64 embedded_size = 0;
    REBYTE *embedded_utf8 = OS_READ_EMBEDDED(&embedded_size);
    if (embedded_utf8 != NULL) {
        if (embedded_size <= 4)
            panic ("No 4-byte long payload at beginning of embedded script");

        i32 ptype = 0;
        REBYTE *data = embedded_utf8 + sizeof(ptype);
        embedded_size -= sizeof(ptype);

        memcpy(&ptype, embedded_utf8, sizeof(ptype));

        if (ptype == 1) { // COMPRESSed data
            embedded = Decompress(data, embedded_size, -1, FALSE, FALSE);
        }
        else {
            embedded = Make_Binary(embedded_size);
            memcpy(BIN_HEAD(embedded), data, embedded_size);
        }

        OS_FREE(embedded_utf8);
    }

    REBVAL embedded_value;
    if (embedded == NULL)
        SET_BLANK(&embedded_value);
    else
        Init_Block(&embedded_value, embedded);
    PUSH_GUARD_VALUE(&embedded_value);

    struct Reb_State state;
    REBCTX *error;

    PUSH_UNHALTABLE_TRAP(&error, &state);

    REBOOL finished;

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    int exit_status;

    if (error == NULL) {
        REBVAL host_start;
        if (
            Do_String(&exit_status, &host_start, BIN_HEAD(startup), FALSE)
            != 0
        ){
            panic (startup); // just loads functions, shouldn't QUIT or error
        }

        Free_Series(startup);

        if (!IS_FUNCTION(&host_start))
            panic (&host_start); // should not be able to error

        PUSH_GUARD_VALUE(&host_start);

        if (Apply_Only_Throws(
            &result, TRUE, &host_start,
            &argv_value, // argv parameter
            &embedded_value, // embedded-script parameter
            END_CELL
        )) {
            #if !defined(NDEBUG)
                if (LEGACY(OPTIONS_EXIT_FUNCTIONS_ONLY))
                    fail (Error_No_Catch_For_Throw(&result));
            #endif

            if (
                IS_FUNCTION(&result)
                && VAL_FUNC_DISPATCHER(&result) == &N_quit
            ) {
                CATCH_THROWN(&result, &result);
                exit_status = Exit_Status_From_Value(&result);

                DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

                Shutdown_Core();
                OS_EXIT(exit_status);
                DEAD_END;
            }

            fail (Error_No_Catch_For_Throw(&result));
        }

        DROP_GUARD_VALUE(&host_start);
        DROP_GUARD_VALUE(&embedded_value);
        DROP_GUARD_VALUE(&argv_value);
        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

        // HOST-START returns either an integer exit code or a blank if the
        // behavior should be to fall back to the REPL.
        //
        if (IS_BLANK(&result))
            finished = FALSE;
        else if (IS_INTEGER(&result)) {
            finished = TRUE;
            exit_status = VAL_INT32(&result);
        }
        else
            panic (&result); // no other legal return values for now
    }
    else {
        //
        // !!! We are not allowed to ask for a print operation that can take
        // arbitrarily long without allowing for cancellation via Ctrl-C,
        // but here we are wanting to print an error.  If you're printing
        // out an error and get a halt, it won't print the halt.
        //
        REBCTX *halt_error;

        // Save error for WHY?
        //
        REBVAL *last = Get_System(SYS_STATE, STATE_LAST_ERROR);
        Init_Error(last, error);

        PUSH_UNHALTABLE_TRAP(&halt_error, &state);

// The first time through the following code 'halt_error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

        if (halt_error)
            panic ("Halt or error while an error was being printed.");

        Print_Value(last, 1024, FALSE);

        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

        // !!! When running in a script, whether or not the Rebol interpreter
        // just exits in an error case with a bad error code or breaks you
        // into the console to debug the environment should be controlled by
        // a command line option.  Defaulting to returning an error code
        // seems better, because kicking into an interactive session can
        // cause logging systems to hang...

        finished = TRUE;
    }

    // Although the REPL routine does a PUSH_UNHALTABLE_TRAP in order to
    // catch any errors or halts, it then has to report those errors when
    // that trap is engaged.  So imagine it's in the process of trapping an
    // error and prints out a very long one, and the user wants to interrupt
    // the error report with a Ctrl-C...but there's not one in effect.
    //
    // This loop institutes a top-level trap whose only job is to catch the
    // interrupts that occur during overlong error reports inside the REPL.
    //
    while (NOT(finished)) {
        REBVAL value;
        SET_END(&value);
        PUSH_GUARD_VALUE(&value); // !!! Out_Value expects value to be GC safe

        struct Reb_State state;
        REBCTX *error;

        PUSH_UNHALTABLE_TRAP(&error, &state);

    // The first time through the following code 'error' will be NULL, but...
    // `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

        if (error) {
            //
            // If a HALT happens and manages to get here, just go set up the
            // trap again and call into the REPL again.  (It wasn't an
            // evaluation error because those have their own traps, it was a
            // halt that happened during output.)
            //
            assert(ERR_NUM(error) == RE_HALT);
            continue;
        }

        Host_Repl(&exit_status, &value, FALSE);

        DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
        DROP_GUARD_VALUE(&value);

        finished = TRUE;
    }

    OS_QUIT_DEVICES(0);

#ifndef REB_CORE
    OS_Destroy_Graphics();
#endif

    Close_StdIO();

    // No need to do a "clean" shutdown, as we are about to exit the process
    // (Note: The debug build runs through the clean shutdown anyway!)
    //
    RL_Shutdown(FALSE);

    return exit_status;
}
