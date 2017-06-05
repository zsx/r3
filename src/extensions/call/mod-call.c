//
//  File: %mod-call.c
//  Summary: "Native Functions for spawning and controlling processes"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 Atronix Engineering
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

#if !defined( __cplusplus) && defined(TO_LINUX)
    //
    // See feature_test_macros(7) -- this definition is redundant under C++
    //
    #define _GNU_SOURCE  // Needed for pipe2 on Linux when #include <unistd.h>
    //
    // !!! Investigate why moving this lower in the file, e.g. in the unix
    // include section, is having trouble.  %sys-core.h should not be
    // including it by default...
#endif


#ifdef IS_ERROR
#undef IS_ERROR //winerror.h defines this, so undef it to avoid the warning
#endif
#include "sys-core.h"
#include "sys-ext.h"

#include "tmp-mod-call-first.h"

// !!! In the original code for CALL, the division of labor was such that
// all the "Rebolisms" had to have the properties extracted before calling
// the "pure C" interface for the host's abstract communication with the
// process spawning API.  Ren-C's plan is to generally abandon the abstract
// OS and let extensions interact via ports and natives, so these flags
// won't be needed.

#define INHERIT_TYPE 0
#define NONE_TYPE 1
#define STRING_TYPE 2
#define FILE_TYPE 3
#define BINARY_TYPE 4

#define FLAG_WAIT 1
#define FLAG_CONSOLE 2
#define FLAG_SHELL 4
#define FLAG_INFO 8


#ifdef TO_WINDOWS
    #include <windows.h>
    #include <process.h>
    #include <shlobj.h>

    #include "call-windows.inc"
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <unistd.h>

    #include "call-posix.inc"
#endif


//
//  call: native/export [
//
//  "Run another program; return immediately (unless /WAIT)."
//
//      command [string! block! file!]
//          {An OS-local command line (quoted as necessary), a block with
//          arguments, or an executable file}
//      /wait
//          "Wait for command to terminate before returning"
//      /console
//          "Runs command with I/O redirected to console"
//      /shell
//          "Forces command to be run from shell"
//      /info
//          "Returns process information object"
//      /input
//          "Redirects stdin to in"
//      in [string! binary! file! blank!]
//      /output
//          "Redirects stdout to out"
//      out [string! binary! file! blank!]
//      /error
//          "Redirects stderr to err"
//      err [string! binary! file! blank!]
//  ]
//
REBNATIVE(call)
//
// !!! Parameter usage may require WAIT mode even if not explicitly requested.
// /WAIT should be default, with /ASYNC (or otherwise) as exception!
{
    INCLUDE_PARAMS_OF_CALL;

    REBINT r;
    REBVAL *arg = ARG(command);
    REBU64 pid = MAX_U64; // Was REBI64 of -1, but OS_CREATE_PROCESS wants u64
    u32 flags = 0;

    // We synthesize the argc and argv from our REBVAL arg, and in the
    // process we may need to do dynamic allocations of argc strings.  In
    // Rebol this is always done by making a series, and if those series
    // are managed then we need to keep them SAVEd from the GC for the
    // duration they will be used.  Due to an artifact of the current
    // implementation, FILE! and STRING! turned into OS-compatible character
    // representations must be managed...so we need to save them over
    // the duration of the call.  We hold the pointers to remember to unsave.
    //
    int argc;
    const REBCHR **argv;
    REBCHR *cmd;
    REBSER *argv_ser = NULL;
    REBSER *argv_saved_sers = NULL;
    REBSER *cmd_ser = NULL;

    REBVAL *input = NULL;
    REBVAL *output = NULL;
    REBVAL *err = NULL;

    // Sometimes OS_CREATE_PROCESS passes back a input/output/err pointers,
    // and sometimes it expects one as input.  If it expects one as input
    // then we may have to transform the REBVAL into pointer data the OS
    // expects.  If we do so then we have to clean up after that transform.
    // (That cleanup could be just a Free_Series(), but an artifact of
    // implementation forces us to use managed series hence SAVE/UNSAVE)
    //
    REBSER *input_ser = NULL;
    REBSER *output_ser = NULL;
    REBSER *err_ser = NULL;

    // Pointers to the string data buffers corresponding to input/output/err,
    // which may be the data of the expanded path series, the data inside
    // of a STRING!, or NULL if NONE! or default of INHERIT_TYPE
    //
    char *os_input = NULL;
    char *os_output = NULL;
    char *os_err = NULL;

    int input_type = INHERIT_TYPE;
    int output_type = INHERIT_TYPE;
    int err_type = INHERIT_TYPE;

    REBCNT input_len = 0;
    REBCNT output_len = 0;
    REBCNT err_len = 0;

    int exit_code = 0;

    Check_Security(Canon(SYM_CALL), POL_EXEC, arg);

    // If input_ser is set, it will be both managed and saved
    //
    if (REF(input)) {
        REBVAL *param = ARG(in);
        input = param;
        if (IS_STRING(param)) {
            input_type = STRING_TYPE;
            os_input = cast(char*, Val_Str_To_OS_Managed(&input_ser, param));
            PUSH_GUARD_SERIES(input_ser);
            input_len = VAL_LEN_AT(param);
        }
        else if (IS_BINARY(param)) {
            input_type = BINARY_TYPE;
            os_input = s_cast(VAL_BIN_AT(param));
            input_len = VAL_LEN_AT(param);
        }
        else if (IS_FILE(param)) {
            input_type = FILE_TYPE;
            input_ser = Value_To_OS_Path(param, FALSE);
            MANAGE_SERIES(input_ser);
            PUSH_GUARD_SERIES(input_ser);
            os_input = SER_HEAD(char, input_ser);
            input_len = SER_LEN(input_ser);
        }
        else if (IS_BLANK(param)) {
            input_type = NONE_TYPE;
        }
        else
            fail (param);
    }

    // Note that os_output is actually treated as an *input* parameter in the
    // case of a FILE! by OS_CREATE_PROCESS.  (In the other cases it is a
    // pointer of the returned data, which will need to be freed with
    // OS_FREE().)  Hence the case for FILE! is handled specially, where the
    // output_ser must be unsaved instead of OS_FREE()d.
    //
    if (REF(output)) {
        REBVAL *param = ARG(out);
        output = param;
        if (IS_STRING(param)) {
            output_type = STRING_TYPE;
        }
        else if (IS_BINARY(param)) {
            output_type = BINARY_TYPE;
        }
        else if (IS_FILE(param)) {
            output_type = FILE_TYPE;
            output_ser = Value_To_OS_Path(param, FALSE);
            MANAGE_SERIES(output_ser);
            PUSH_GUARD_SERIES(output_ser);
            os_output = SER_HEAD(char, output_ser);
            output_len = SER_LEN(output_ser);
        }
        else if (IS_BLANK(param)) {
            output_type = NONE_TYPE;
        }
        else
            fail (param);
    }

    (void)input; // suppress unused warning but keep variable

    // Error case...same note about FILE! case as with Output case above
    //
    if (REF(error)) {
        REBVAL *param = ARG(err);
        err = param;
        if (IS_STRING(param)) {
            err_type = STRING_TYPE;
        }
        else if (IS_BINARY(param)) {
            err_type = BINARY_TYPE;
        }
        else if (IS_FILE(param)) {
            err_type = FILE_TYPE;
            err_ser = Value_To_OS_Path(param, FALSE);
            MANAGE_SERIES(err_ser);
            PUSH_GUARD_SERIES(err_ser);
            os_err = SER_HEAD(char, err_ser);
            err_len = SER_LEN(err_ser);
        }
        else if (IS_BLANK(param)) {
            err_type = NONE_TYPE;
        }
        else
            fail (param);
    }

    if (
        REF(wait) ||
        (
            input_type == STRING_TYPE
            || input_type == BINARY_TYPE
            || output_type == STRING_TYPE
            || output_type == BINARY_TYPE
            || err_type == STRING_TYPE
            || err_type == BINARY_TYPE
        ) // I/O redirection implies /WAIT

    ){
        flags |= FLAG_WAIT;
    }

    if (REF(console))
        flags |= FLAG_CONSOLE;

    if (REF(shell))
        flags |= FLAG_SHELL;

    if (REF(info))
        flags |= FLAG_INFO;

    // Translate the first parameter into an `argc` and a pointer array for
    // `argv[]`.  The pointer array is backed by `argv_series` which must
    // be freed after we are done using it.
    //
    if (IS_STRING(arg)) {
        // `call {foo bar}` => execute %"foo bar"

        // !!! Interpreting string case as an invocation of %foo with argument
        // "bar" has been requested and seems more suitable.  Question is
        // whether it should go through the shell parsing to do so.

        cmd = Val_Str_To_OS_Managed(&cmd_ser, arg);
        PUSH_GUARD_SERIES(cmd_ser);

        argc = 1;
        argv_ser = Make_Series(argc + 1, sizeof(REBCHR*));
        argv = SER_HEAD(const REBCHR*, argv_ser);

        argv[0] = cmd;
        // Already implicitly SAVEd by cmd_ser, no need for argv_saved_sers

        argv[argc] = NULL;
    }
    else if (IS_BLOCK(arg)) {
        // `call ["foo" "bar"]` => execute %foo with arg "bar"

        int i;

        cmd = NULL;
        argc = VAL_LEN_AT(arg);

        if (argc <= 0) fail (Error_Too_Short_Raw());

        argv_ser = Make_Series(argc + 1, sizeof(REBCHR*));
        argv_saved_sers = Make_Series(argc, sizeof(REBSER*));
        argv = SER_HEAD(const REBCHR*, argv_ser);
        for (i = 0; i < argc; i ++) {
            RELVAL *param = VAL_ARRAY_AT_HEAD(arg, i);
            if (IS_STRING(param)) {
                REBSER *ser;
                argv[i] = Val_Str_To_OS_Managed(&ser, KNOWN(param));
                PUSH_GUARD_SERIES(ser);
                SER_HEAD(REBSER*, argv_saved_sers)[i] = ser;
            }
            else if (IS_FILE(param)) {
                REBSER *path = Value_To_OS_Path(KNOWN(param), FALSE);
                argv[i] = SER_HEAD(REBCHR, path);

                MANAGE_SERIES(path);
                PUSH_GUARD_SERIES(path);
                SER_HEAD(REBSER*, argv_saved_sers)[i] = path;
            }
            else
                fail (Error_Invalid_Arg_Core(param, VAL_SPECIFIER(arg)));
        }
        argv[argc] = NULL;
    }
    else if (IS_FILE(arg)) {
        // `call %"foo bar"` => execute %"foo bar"

        REBSER *path = Value_To_OS_Path(arg, FALSE);

        cmd = NULL;
        argc = 1;
        argv_ser = Make_Series(argc + 1, sizeof(REBCHR*));
        argv_saved_sers = Make_Series(argc, sizeof(REBSER*));

        argv = SER_HEAD(const REBCHR*, argv_ser);

        argv[0] = SER_HEAD(REBCHR, path);
        MANAGE_SERIES(path);
        PUSH_GUARD_SERIES(path);
        SER_HEAD(REBSER*, argv_saved_sers)[0] = path;

        argv[argc] = NULL;
    }
    else
        fail (arg);

    r = OS_Create_Process(
#ifdef TO_WINDOWS
        cast(const wchar_t*, cmd),
        argc,
        cast(const wchar_t**, argv),
#else
        cast(const char*, cmd),
        argc,
        cast(const char**, argv),
#endif
        flags, &pid, &exit_code,
        input_type, os_input, input_len,
        output_type, &os_output, &output_len,
        err_type, &os_err, &err_len
    );

    // Call may not succeed if r != 0, but we still have to run cleanup
    // before reporting any error...
    //
    if (argv_saved_sers) {
        int i = argc;
        assert(argc > 0);
        do {
            // Count down: must unsave the most recently saved series first!
            DROP_GUARD_SERIES(*SER_AT(REBSER*, argv_saved_sers, i - 1));
            --i;
        } while (i != 0);
        Free_Series(argv_saved_sers);
    }
    if (cmd_ser) DROP_GUARD_SERIES(cmd_ser);
    Free_Series(argv_ser); // Unmanaged, so we can free it

    if (output_type == STRING_TYPE) {
        if (output != NULL
            && output_len > 0) {
            // !!! Somewhat inefficient: should there be Append_OS_Str?
            REBSER *ser = Copy_OS_Str(os_output, output_len);
            Append_String(VAL_SERIES(output), ser, 0, SER_LEN(ser));
            OS_FREE(os_output);
            Free_Series(ser);
        }
    } else if (output_type == BINARY_TYPE) {
        if (output != NULL
            && output_len > 0) {
            Append_Unencoded_Len(VAL_SERIES(output), os_output, output_len);
            OS_FREE(os_output);
        }
    }

    if (err_type == STRING_TYPE) {
        if (err != NULL
            && err_len > 0) {
            // !!! Somewhat inefficient: should there be Append_OS_Str?
            REBSER *ser = Copy_OS_Str(os_err, err_len);
            Append_String(VAL_SERIES(err), ser, 0, SER_LEN(ser));
            OS_FREE(os_err);
            Free_Series(ser);
        }
    } else if (err_type == BINARY_TYPE) {
        if (err != NULL
            && err_len > 0) {
            Append_Unencoded_Len(VAL_SERIES(err), os_err, err_len);
            OS_FREE(os_err);
        }
    }

    // If we used (and possibly created) a series for input/output/err, then
    // that series was managed and saved from GC.  Unsave them now.  Note
    // backwardsness: must unsave the most recently saved series first!!
    //
    if (err_ser) DROP_GUARD_SERIES(err_ser);
    if (output_ser) DROP_GUARD_SERIES(output_ser);
    if (input_ser) DROP_GUARD_SERIES(input_ser);

    if (REF(info)) {
        REBCTX *info = Alloc_Context(REB_OBJECT, 2);

        Init_Integer(Append_Context(info, NULL, Canon(SYM_ID)), pid);
        if (REF(wait))
            Init_Integer(
                Append_Context(info, NULL, Canon(SYM_EXIT_CODE)),
                exit_code
            );

        Init_Object(D_OUT, info);
        return R_OUT;
    }

    if (r != 0) {
        Make_OS_Error(D_OUT, r);
        fail (Error_Call_Fail_Raw(D_OUT));
    }

    // We may have waited even if they didn't ask us to explicitly, but
    // we only return a process ID if /WAIT was not explicitly used
    //
    if (REF(wait))
        Init_Integer(D_OUT, exit_code);
    else
        Init_Integer(D_OUT, pid);

    return R_OUT;
}


//
//  get-os-browsers: native/export [
//
//  "Ask the OS or registry what command(s) to use for starting a browser."
//
//      return: [block!]
//          {Block of strings, where %1 should be substituted with the string}
//  ]
//
REBNATIVE(get_os_browsers)
//
// !!! Using the %1 convention is not necessarily ideal vs. having some kind
// of more "structural" result, it was just easy because it's how the string
// comes back from the Windows registry.  Review.
{
    INCLUDE_PARAMS_OF_GET_OS_BROWSERS;

    REBDSP dsp_orig = DSP;

#if defined(TO_WINDOWS)

    HKEY key;
    if (
        RegOpenKeyEx(
            HKEY_CLASSES_ROOT,
            L"http\\shell\\open\\command",
            0,
            KEY_READ,
            &key
        ) != ERROR_SUCCESS
    ){
        fail ("Could not open registry key for http\\shell\\open\\command");
    }

    static_assert_c(sizeof(REBUNI) == sizeof(wchar_t));

    DWORD num_bytes = 0; // pass NULL and use 0 for initial length, to query

    DWORD type;
    DWORD flag = RegQueryValueExW(key, L"", 0, &type, NULL, &num_bytes);
    
    if (
        (flag != ERROR_MORE_DATA && flag != ERROR_SUCCESS)
        || num_bytes == 0
        || type != REG_SZ // RegQueryValueExW returns unicode
        || num_bytes % 2 != 0 // byte count should be even for unicode
    ) {
        RegCloseKey(key);
        fail ("Could not read registry key for http\\shell\\open\\command");
    }

    REBCNT len = num_bytes / 2;

    REBSER *ser = Make_Unicode(len);
    flag = RegQueryValueEx(
        key, L"", 0, &type, cast(LPBYTE, UNI_HEAD(ser)), &num_bytes
    );
    RegCloseKey(key);

    if (flag != ERROR_SUCCESS)
        fail ("Could not read registry key for http\\shell\\open\\command");

    while (*UNI_AT(ser, len - 1) == 0) {
        //
        // Don't count terminators; seems the guarantees are a bit fuzzy
        // about whether the string in the registry has one included in the
        // byte count or not.
        //
        --len;
    }
    TERM_UNI_LEN(ser, len);

    DS_PUSH_TRASH;
    Init_String(DS_TOP, ser);

#elif defined(TO_LINUX)

    // Caller should try xdg-open first, then try x-www-browser otherwise
    //
    DS_PUSH_TRASH;
    Init_String(DS_TOP, Make_UTF8_May_Fail("xdg-open %1"));
    DS_PUSH_TRASH;
    Init_String(DS_TOP, Make_UTF8_May_Fail("x-www-browser %1"));

#else // Just try /usr/bin/open on POSIX, OS X, Haiku, etc.

    // Just use /usr/bin/open
    //
    DS_PUSH_TRASH;
    Init_String(DS_TOP, Make_UTF8_May_Fail("/usr/bin/open %1"));

#endif

    Init_Block(D_OUT, Pop_Stack_Values(dsp_orig));
    return R_OUT;
}


//
//  sleep: native [
//
//  "Use system sleep to wait a certain amount of time (doesn't use PORT!s)."
//
//      return: [<opt>]
//      duration [integer! decimal! time!]
//          {Length to sleep (integer and decimal are measuring seconds)}
//
//  ]
//
REBNATIVE(sleep)
//
// !!! This is a temporary workaround for the fact that it is not currently
// possible to do a WAIT on a time from within an AWAKE handler.  A proper
// solution would presumably solve that problem, so two different functions
// would not be needed.
//
// This function was needed by @GrahamChiu, and puting it in the CALL module
// isn't necessarily ideal, but it's better than making the core dependent
// on Sleep() vs. usleep()...and all the relevant includes have been
// established here.
{
    INCLUDE_PARAMS_OF_SLEEP;

    REBCNT msec = Milliseconds_From_Value(ARG(duration));

#ifdef TO_WINDOWS
    Sleep(msec);
#else
    usleep(msec * 1000);
#endif

    return R_VOID;
}


#include "tmp-mod-call-last.h"
