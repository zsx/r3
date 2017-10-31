//
//  File: %n-io.c
//  Summary: "native functions for input and output"
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


/** Helper Functions **************************************************/


//
//  form: native [
//
//  "Converts a value to a human-readable string."
//
//      value [<opt> any-value!]
//          "The value to form"
//  ]
//
REBNATIVE(form)
{
    INCLUDE_PARAMS_OF_FORM;

    REBVAL *value = ARG(value);

    Init_String(D_OUT, Copy_Form_Value(value, 0));

    return R_OUT;
}


//
//  mold: native [
//
//  "Converts a value to a REBOL-readable string."
//
//      value [any-value!]
//          "The value to mold"
//      /only
//          {For a block value, mold only its contents, no outer []}
//      /all
//          "Use construction syntax"
//      /flat
//          "No indentation"
//  ]
//
REBNATIVE(mold)
{
    INCLUDE_PARAMS_OF_MOLD;

    DECLARE_MOLD (mo);
    if (REF(all))
        SET_MOLD_FLAG(mo, MOLD_FLAG_ALL);
    if (REF(flat))
        SET_MOLD_FLAG(mo, MOLD_FLAG_INDENT);

    Push_Mold(mo);

    if (REF(only) && IS_BLOCK(ARG(value)))
        SET_MOLD_FLAG(mo, MOLD_FLAG_ONLY);

    Mold_Value(mo, ARG(value));

    Init_String(D_OUT, Pop_Molded_String(mo));

    return R_OUT;
}


//
//  write-stdout: native [
//
//  "Write text to standard output, or raw BINARY! (for control codes / CGI)"
//
//      return: [<opt>]
//      value [string! char! binary!]
//          "Text to write, if a STRING! or CHAR! is converted to OS format"
//  ]
//
REBNATIVE(write_stdout)
{
    INCLUDE_PARAMS_OF_WRITE_STDOUT;

    REBVAL *v = ARG(value);

    if (IS_BINARY(v)) { // raw output
        Prin_OS_String(VAL_BIN_AT(v), VAL_LEN_AT(v), OPT_ENC_RAW);
    }
    else if (IS_CHAR(v)) { // useful for `write-stdout newline`, etc.
        Prin_OS_String(&VAL_CHAR(v), 1, OPT_ENC_UNISRC | OPT_ENC_CRLF_MAYBE);
    }
    else { // string output translated to OS format
        assert(IS_STRING(v));
        if (VAL_BYTE_SIZE(v))
            Prin_OS_String(VAL_BIN_AT(v), VAL_LEN_AT(v), OPT_ENC_CRLF_MAYBE);
        else
            Prin_OS_String(
                VAL_UNI_AT(v),
                VAL_LEN_AT(v),
                OPT_ENC_UNISRC | OPT_ENC_CRLF_MAYBE
            );
    }

    return R_VOID;
}


//
//  new-line: native [
//
//  {Sets or clears the new-line marker within a block or group.}
//
//      position [block! group!]
//          "Position to change marker (modified)"
//      mark
//          "Set TRUE for newline"
//      /all
//          "Set/clear marker to end of series"
//      /skip
//          {Set/clear marker periodically to the end of the series}
//      size [integer!]
//  ]
//
REBNATIVE(new_line)
{
    INCLUDE_PARAMS_OF_NEW_LINE;

    RELVAL *value = VAL_ARRAY_AT(ARG(position));
    REBOOL mark = IS_TRUTHY(ARG(mark));
    REBINT skip = 0;
    REBCNT n;

    if (REF(all)) skip = 1;

    if (REF(skip)) {
        skip = Int32s(ARG(size), 1);
        if (skip < 1) skip = 1;
    }

    for (n = 0; NOT_END(value); n++, value++) {
        if ((skip != 0) && (n % skip != 0)) continue;

        if (mark)
            SET_VAL_FLAG(value, VALUE_FLAG_LINE);
        else
            CLEAR_VAL_FLAG(value, VALUE_FLAG_LINE);

        if (skip == 0) break;
    }

    Move_Value(D_OUT, ARG(position));
    return R_OUT;
}


//
//  new-line?: native [
//
//  {Returns the state of the new-line marker within a block or group.}
//
//      position [block! group!] "Position to check marker"
//  ]
//
REBNATIVE(new_line_q)
{
    INCLUDE_PARAMS_OF_NEW_LINE_Q;

    if (GET_VAL_FLAG(VAL_ARRAY_AT(ARG(position)), VALUE_FLAG_LINE))
        return R_TRUE;

    return R_FALSE;
}


//
//  now: native [
//
//  "Returns current date and time with timezone adjustment."
//
//      /year
//          "Returns year only"
//      /month
//          "Returns month only"
//      /day
//          "Returns day of the month only"
//      /time
//          "Returns time only"
//      /zone
//          "Returns time zone offset from UCT (GMT) only"
//      /date
//          "Returns date only"
//      /weekday
//          {Returns day of the week as integer (Monday is day 1)}
//      /yearday
//          "Returns day of the year (Julian)"
//      /precise
//          "High precision time"
//      /utc
//          "Universal time (zone +0:00)"
//      /local
//          "Give time in current zone without including the time zone"
//  ]
//
REBNATIVE(now)
{
    INCLUDE_PARAMS_OF_NOW;

    REBVAL *ret = D_OUT;
    OS_GET_TIME(D_OUT);

    // However OS-level date and time is plugged into the system, it needs to
    // have enough granularity to give back date, time, and time zone.
    //
    assert(IS_DATE(D_OUT));
    assert(GET_VAL_FLAG(D_OUT, DATE_FLAG_HAS_TIME));
    assert(GET_VAL_FLAG(D_OUT, DATE_FLAG_HAS_ZONE));

    if (NOT(REF(precise))) {
        //
        // The "time" field is measured in nanoseconds, and the historical
        // meaning of not using precise measurement was to use only the
        // seconds portion (with the nanoseconds set to 0).  This achieves
        // that by extracting the seconds and then multiplying by nanoseconds.
        //
        VAL_NANO(ret) = SECS_TO_NANO(VAL_SECS(ret));
    }

    if (REF(utc)) {
        //
        // Say it has a time zone component, but it's 0:00 (as opposed
        // to saying it has no time zone component at all?)
        //
        INIT_VAL_ZONE(ret, 0);
    }
    else if (REF(local)) {
        // Clear out the time zone flag
        //
        CLEAR_VAL_FLAG(ret, DATE_FLAG_HAS_ZONE);
    }
    else {
        if (
            REF(year)
            || REF(month)
            || REF(day)
            || REF(time)
            || REF(date)
            || REF(weekday)
            || REF(yearday)
        ){
            Adjust_Date_Zone(ret, FALSE); // Add time zone, adjust date/time
        }
    }

    REBINT n = -1;

    if (REF(date)) {
        CLEAR_VAL_FLAGS(ret, DATE_FLAG_HAS_TIME | DATE_FLAG_HAS_ZONE);
    }
    else if (REF(time)) {
        VAL_RESET_HEADER(ret, REB_TIME); // reset clears date flags
    }
    else if (REF(zone)) {
        VAL_NANO(ret) = VAL_ZONE(ret) * ZONE_MINS * MIN_SEC;
        VAL_RESET_HEADER(ret, REB_TIME); // reset clears date flags
    }
    else if (REF(weekday))
        n = Week_Day(VAL_DATE(ret));
    else if (REF(yearday))
        n = Julian_Date(VAL_DATE(ret));
    else if (REF(year))
        n = VAL_YEAR(ret);
    else if (REF(month))
        n = VAL_MONTH(ret);
    else if (REF(day))
        n = VAL_DAY(ret);

    if (n > 0)
        Init_Integer(ret, n);

    return R_OUT;
}


//
//  Milliseconds_From_Value: C
//
// Note that this routine is used by the SLEEP extension, as well as by WAIT.
//
REBCNT Milliseconds_From_Value(const RELVAL *v) {
    REBINT msec;

    switch (VAL_TYPE(v)) {
    case REB_INTEGER:
        msec = 1000 * Int32(v);
        break;

    case REB_DECIMAL:
        msec = cast(REBINT, 1000 * VAL_DECIMAL(v));
        break;

    case REB_TIME:
        msec = cast(REBINT, VAL_NANO(v) / (SEC_SEC / 1000));
        break;

    default:
        panic (NULL); // avoid uninitialized msec warning
    }

    if (msec < 0)
        fail (Error_Out_Of_Range(const_KNOWN(v)));

    return cast(REBCNT, msec);
}


//
//  wait: native [
//
//  "Waits for a duration, port, or both."
//
//      value [any-number! time! port! block! blank!]
//      /all
//          "Returns all in a block"
//      /only
//          {only check for ports given in the block to this function}
//  ]
//
REBNATIVE(wait)
{
    INCLUDE_PARAMS_OF_WAIT;

    REBCNT timeout = 0; // in milliseconds
    REBARR *ports = NULL;
    REBINT n = 0;

    Init_Blank(D_OUT);

    RELVAL *val;
    if (IS_BLOCK(ARG(value))) {
        DECLARE_LOCAL (unsafe); // temporary not safe from GC

        if (Reduce_Any_Array_Throws(
            unsafe, ARG(value), REDUCE_FLAG_DROP_BARS
        )){
            Move_Value(D_OUT, unsafe);
            return R_OUT_IS_THROWN;
        }

        ports = VAL_ARRAY(unsafe);
        for (val = ARR_HEAD(ports); NOT_END(val); val++) { // find timeout
            if (Pending_Port(KNOWN(val))) n++;
            if (IS_INTEGER(val)
                || IS_DECIMAL(val)
                || IS_TIME(val)
                )
                break;
        }
        if (IS_END(val)) {
            if (n == 0) return R_BLANK; // has no pending ports!
            else timeout = ALL_BITS; // no timeout provided
            // Init_Blank(val); // no timeout -- BUG: unterminated block in GC
        }
    }
    else
        val = ARG(value);

    if (NOT_END(val)) {
        switch (VAL_TYPE(val)) {
        case REB_INTEGER:
        case REB_DECIMAL:
        case REB_TIME:
            timeout = Milliseconds_From_Value(val);
            break;

        case REB_PORT:
            if (!Pending_Port(KNOWN(val))) return R_BLANK;
            ports = Make_Array(1);
            Append_Value(ports, KNOWN(val));
            // fall thru...
        case REB_BLANK:
            timeout = ALL_BITS; // wait for all windows
            break;

        default:
            fail (Error_Invalid_Arg_Core(val, SPECIFIED));
        }
    }

    // Prevent GC on temp port block:
    // Note: Port block is always a copy of the block.
    if (ports)
        Init_Block(D_OUT, ports);

    // Process port events [stack-move]:
    if (!Wait_Ports(ports, timeout, REF(only))) {
        Sieve_Ports(NULL); // just reset the waked list
        return R_BLANK;
    }
    if (!ports) return R_BLANK;

    // Determine what port(s) waked us:
    Sieve_Ports(ports);

    if (NOT(REF(all))) {
        val = ARR_HEAD(ports);
        if (IS_PORT(val))
            Move_Value(D_OUT, KNOWN(val));
        else
            Init_Blank(D_OUT);
    }

    return R_OUT;
}


//
//  wake-up: native [
//
//  "Awake and update a port with event."
//
//      port [port!]
//      event [event!]
//  ]
//
REBNATIVE(wake_up)
//
// Calls port update for native actors.
// Calls port awake function.
{
    INCLUDE_PARAMS_OF_WAKE_UP;

    REBCTX *port = VAL_CONTEXT(ARG(port));
    FAIL_IF_BAD_PORT(port);

    REBVAL *actor = CTX_VAR(port, STD_PORT_ACTOR);
    if (Is_Native_Port_Actor(actor)) {
        //
        // We don't pass `actor` or `event` in, because we just pass the
        // current call info.  The port action can re-read the arguments.
        //
        Do_Port_Action(frame_, port, SYM_ON_WAKE_UP);
    }

    REBOOL woke_up = TRUE; // start by assuming success

    REBVAL *awake = CTX_VAR(port, STD_PORT_AWAKE);
    if (IS_FUNCTION(awake)) {
        const REBOOL fully = TRUE; // error if not all arguments consumed

        if (Apply_Only_Throws(D_OUT, fully, awake, ARG(event), END))
            fail (Error_No_Catch_For_Throw(D_OUT));

        if (NOT(IS_LOGIC(D_OUT) && VAL_LOGIC(D_OUT)))
            woke_up = FALSE;
    }

    return R_FROM_BOOL(woke_up);
}


//
//  to-rebol-file: native [
//
//  {Converts a local system file path to a REBOL file path.}
//
//      path [file! string!]
//  ]
//
REBNATIVE(to_rebol_file)
{
    INCLUDE_PARAMS_OF_TO_REBOL_FILE;

    REBVAL *arg = ARG(path);

    REBSER *ser = Value_To_REBOL_Path(arg, FALSE);
    if (ser == NULL)
        fail (arg);

    Init_File(D_OUT, ser);
    return R_OUT;
}


//
//  to-local-file: native [
//
//  {Converts a REBOL file path to the local system file path.}
//
//      path [file! string!]
//      /full
//          {Prepends current dir for full path (for relative paths only)}
//  ]
//
REBNATIVE(to_local_file)
{
    INCLUDE_PARAMS_OF_TO_LOCAL_FILE;

    REBVAL *arg = ARG(path);

    REBSER *ser = Value_To_Local_Path(arg, REF(full));
    if (ser == NULL)
        fail (arg);

    Init_String(D_OUT, ser);
    return R_OUT;
}


//
//  what-dir: native [
//  "Returns the current directory path."
//      ; No arguments
//  ]
//
REBNATIVE(what_dir)
{
    REBVAL *current_path = Get_System(SYS_OPTIONS, OPTIONS_CURRENT_PATH);

    if (IS_FILE(current_path) || IS_BLANK(current_path)) {
        //
        // !!! Because of the need to track a notion of "current path" which
        // could be a URL! as well as a FILE!, the state is stored in the
        // system options.  For now--however--it is "duplicate" in the case
        // of a FILE!, because the OS has its own tracked state.  We let the
        // OS state win for files if they have diverged somehow--because the
        // code was already here and it would be more compatible.  But
        // reconsider the duplication.

        REBCHR *lpath;
        REBINT len = OS_GET_CURRENT_DIR(&lpath);

        // allocates extra for end `/`
        REBSER *ser = To_REBOL_Path(
            lpath, len, PATH_OPT_SRC_IS_DIR | (OS_WIDE ? PATH_OPT_UNI_SRC : 0)
        );

        OS_FREE(lpath);

        Init_File(current_path, ser); // refresh in case they diverged
    }
    else if (NOT(IS_URL(current_path))) {
        //
        // Lousy error, but ATM the user can directly edit system/options.
        // They shouldn't be able to (or if they can, it should be validated)
        //
        fail (current_path);
    }

    // Note the expectation is that WHAT-DIR will return a value that can be
    // mutated by the caller without affecting future calls to WHAT-DIR, so
    // the variable holding the current path must be copied.
    //
    Init_Any_Series_At(
        D_OUT,
        VAL_TYPE(current_path),
        Copy_Sequence(VAL_SERIES(current_path)),
        VAL_INDEX(current_path)
    );

    return R_OUT;
}


//
//  change-dir: native [
//
//  {Changes the current path (where scripts with relative paths will be run).}
//
//      path [file! url!]
//  ]
//
REBNATIVE(change_dir)
{
    INCLUDE_PARAMS_OF_CHANGE_DIR;

    REBVAL *arg = ARG(path);
    REBVAL *current_path = Get_System(SYS_OPTIONS, OPTIONS_CURRENT_PATH);

    if (IS_URL(arg)) {
        // There is no directory listing protocol for HTTP (although this
        // needs to be methodized to work for SFTP etc.)  So this takes
        // your word for it for the moment that it's a valid "directory".
        //
        // !!! Should it at least check for a trailing `/`?
    }
    else {
        assert(IS_FILE(arg));

        REBSER *ser = Value_To_OS_Path(arg, TRUE);
        if (ser == NULL)
            fail (arg);

        DECLARE_LOCAL (val);
        Init_String(val, ser); // may be unicode or utf-8
        Check_Security(Canon(SYM_FILE), POL_EXEC, val);

        if (!OS_SET_CURRENT_DIR(SER_HEAD(REBCHR, ser)))
            fail (arg);
    }

    Move_Value(current_path, arg);

    Move_Value(D_OUT, ARG(path));
    return R_OUT;
}


//
//  String_List_To_Array: C
//
// Convert a series of null terminated strings to an array of strings
// separated with '='.
//
static REBARR *String_List_To_Array(REBCHR *str)
{
    REBCNT n;
    REBCNT len = 0;
    REBCHR *start = str;
    REBCHR *eq;
    REBARR *array;

    while ((n = OS_STRLEN(str))) {
        len++;
        str += n + 1; // next
    }

    array = Make_Array(len * 2);

    str = start;
    while ((eq = OS_STRCHR(str+1, '=')) && (n = OS_STRLEN(str))) {
        Init_String(Alloc_Tail_Array(array), Copy_OS_Str(str, eq - str));
        Init_String(
            Alloc_Tail_Array(array), Copy_OS_Str(eq + 1, n - (eq - str) - 1)
        );
        str += n + 1; // next
    }

    return array;
}


//
//  Block_To_String_List: C
//
// Convert block of values to a string that holds
// a series of null terminated strings, followed
// by a final terminating string.
//
REBSER *Block_To_String_List(REBVAL *blk)
{
    DECLARE_MOLD (mo);

    Push_Mold(mo);

    RELVAL *item;
    for (item = VAL_ARRAY_AT(blk); NOT_END(item); ++item) {
        Form_Value(mo, item);
        Append_Codepoint_Raw(mo->series, '\0');
    }
    Append_Codepoint_Raw(mo->series, '\0');

    return Pop_Molded_String(mo);
}


//
//  File_List_To_Array: C
//
// Convert file directory and file name list to block.
//
static REBARR *File_List_To_Array(const REBCHR *str)
{
    REBCNT n;
    REBCNT len = 0;
    const REBCHR *start = str;
    REBARR *blk;
    REBSER *dir;

    while ((n = OS_STRLEN(str))) {
        len++;
        str += n + 1; // next
    }

    blk = Make_Array(len);

    // First is a dir path or full file path:
    str = start;
    n = OS_STRLEN(str);

    if (len == 1) {  // First is full file path
        dir = To_REBOL_Path(str, n, (OS_WIDE ? PATH_OPT_UNI_SRC : 0));
        Init_File(Alloc_Tail_Array(blk), dir);
    }
    else {  // First is dir path for the rest of the files
#ifdef TO_WINDOWS /* directory followed by files */
        assert(sizeof(wchar_t) == sizeof(REBCHR));
        dir = To_REBOL_Path(
            str,
            n,
            PATH_OPT_UNI_SRC | PATH_OPT_FORCE_UNI_DEST | PATH_OPT_SRC_IS_DIR
        );
        str += n + 1; // next
        len = SER_LEN(dir);
        while ((n = OS_STRLEN(str))) {
            SET_SERIES_LEN(dir, len);
            Append_Uni_Uni(dir, cast(const REBUNI*, str), n);
            Init_File(Alloc_Tail_Array(blk), Copy_String_Slimming(dir, 0, -1));
            str += n + 1; // next
        }
#else /* absolute pathes already */
        str += n + 1;
        while ((n = OS_STRLEN(str))) {
            dir = To_REBOL_Path(str, n, (OS_WIDE ? PATH_OPT_UNI_SRC : 0));
            Init_File(Alloc_Tail_Array(blk), Copy_String_Slimming(dir, 0, -1));
            str += n + 1; // next
        }
#endif
    }

    return blk;
}


//
//  request-file: native [
//
//  {Asks user to select a file and returns full file path (or block of paths).}
//
//      /save
//          "File save mode"
//      /multi
//          {Allows multiple file selection, returned as a block}
//      /file
//      name [file!]
//          "Default file name or directory"
//      /title
//      text [string!]
//          "Window title"
//      /filter
//      list [block!]
//          "Block of filters (filter-name filter)"
//  ]
//
REBNATIVE(request_file)
{
    INCLUDE_PARAMS_OF_REQUEST_FILE;

    // !!! This routine used to have an ENABLE_GC and DISABLE_GC
    // reference.  It is not clear what that was protecting, but
    // this code should be reviewed with GC "torture mode", and
    // if any values are being created which cannot be GC'd then
    // they should be created without handing them over to GC with
    // MANAGE_SERIES() instead.

    REBRFR fr;
    CLEARS(&fr);
    fr.files = OS_ALLOC_N(REBCHR, MAX_FILE_REQ_BUF);
    fr.len = MAX_FILE_REQ_BUF/sizeof(REBCHR) - 2;
    fr.files[0] = OS_MAKE_CH('\0');

    if (REF(save))
        fr.flags |= FRF_SAVE;
    if (REF(multi))
        fr.flags |= FRF_MULTI;

    if (REF(file)) {
        REBSER *ser = Value_To_OS_Path(ARG(name), TRUE);
        REBINT n = SER_LEN(ser);

        fr.dir = SER_HEAD(REBCHR, ser);

        if (OS_CH_VALUE(fr.dir[n - 1]) != OS_DIR_SEP) {
            if (n + 2 > fr.len)
                n = fr.len - 2;
            OS_STRNCPY(
                cast(REBCHR*, fr.files),
                SER_HEAD(REBCHR, ser),
                n
            );
            fr.files[n] = OS_MAKE_CH('\0');
        }
    }

    if (REF(filter)) {
        REBSER *ser = Block_To_String_List(ARG(list));
        fr.filter = SER_HEAD(REBCHR, ser);
    }

    if (REF(title)) {
        // !!! By passing NULL we don't get backing series to protect!
        fr.title = Val_Str_To_OS_Managed(NULL, ARG(text));
    }

    if (OS_REQUEST_FILE(&fr)) {
        if (fr.flags & FRF_MULTI) {
            REBARR *array = File_List_To_Array(fr.files);
            Init_Block(D_OUT, array);
        }
        else {
            REBSER *ser = To_REBOL_Path(
                fr.files, OS_STRLEN(fr.files), (OS_WIDE ? PATH_OPT_UNI_SRC : 0)
            );
            Init_File(D_OUT, ser);
        }
    } else
        Init_Blank(D_OUT);

    OS_FREE(fr.files);

    return R_OUT;
}


//
//  get-env: native [
//
//  {Returns the value of an OS environment variable (for current process).}
//
//      return: [string! blank!]
//          {The string of the environment variable, or blank if not set}
//      var [any-string! any-word!]
//  ]
//
REBNATIVE(get_env)
{
    INCLUDE_PARAMS_OF_GET_ENV;

    REBVAL *var = ARG(var);

    Check_Security(Canon(SYM_ENVR), POL_READ, var);

    if (ANY_WORD(var)) {
        REBSER *copy = Copy_Form_Value(var, 0);
        Init_String(var, copy);
    }

    // !!! By passing NULL we don't get backing series to protect!
    REBCHR *os_var = Val_Str_To_OS_Managed(NULL, var);

    REBINT lenplus = OS_GET_ENV(NULL, os_var, 0);
    if (lenplus < 0)
        return R_BLANK;
    if (lenplus == 0) {
        Init_String(D_OUT, Copy_Sequence(VAL_SERIES(EMPTY_STRING)));
        return R_OUT;
    }

    // Two copies...is there a better way?
    REBCHR *buf = ALLOC_N(REBCHR, lenplus);
    OS_GET_ENV(buf, os_var, lenplus);
    Init_String(D_OUT, Copy_OS_Str(buf, lenplus - 1));
    FREE_N(REBCHR, lenplus, buf);

    return R_OUT;
}


//
//  set-env: native [
//
//  {Sets value of operating system environment variable for current process.}
//
//      var [any-string! any-word!]
//          "Variable to set"
//      value [string! blank!]
//          "Value to set, or a BLANK! to unset it"
//  ]
//
REBNATIVE(set_env)
{
    INCLUDE_PARAMS_OF_SET_ENV;

    REBVAL *var = ARG(var);
    REBVAL *value = ARG(value);

    Check_Security(Canon(SYM_ENVR), POL_WRITE, var);

    if (ANY_WORD(var)) {
        REBSER *copy = Copy_Form_Value(var, 0);
        Init_String(var, copy);
    }

    // !!! By passing NULL we don't get backing series to protect!
    REBCHR *os_var = Val_Str_To_OS_Managed(NULL, var);

    if (ANY_STRING(value)) {
        // !!! By passing NULL we don't get backing series to protect!
        REBCHR *os_value = Val_Str_To_OS_Managed(NULL, value);
        if (OS_SET_ENV(os_var, os_value)) {
            // What function could reuse arg2 as-is?
            Init_String(D_OUT, Copy_OS_Str(os_value, OS_STRLEN(os_value)));
            return R_OUT;
        }
        return R_VOID;
    }

    assert(IS_BLANK(value));

    if (OS_SET_ENV(os_var, NULL))
        return R_BLANK;
    return R_VOID;
}


//
//  list-env: native [
//
//  {Returns a map of OS environment variables (for current process).}
//
//      ; No arguments
//  ]
//
REBNATIVE(list_env)
{
    REBARR *array = String_List_To_Array(OS_LIST_ENV());
    REBMAP *map = Mutate_Array_Into_Map(array);
    Init_Map(D_OUT, map);

    return R_OUT;
}

