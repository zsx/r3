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
        CONVERT_NAME_TO_THROWN(D_OUT, D_ARG(2));
    }
    else if (D_REF(3)) {
        CONVERT_NAME_TO_THROWN(D_OUT, D_ARG(4));
    }
    else {
        // Chosen to do it this way because returning to a calling script it
        // will be UNSET! by default, for parity with BREAK and EXIT without
        // a /WITH.  Long view would have RETURN work this way too: CC#2241

        // (UNSET! will be translated to 0 if it gets caught for the shell)

        CONVERT_NAME_TO_THROWN(D_OUT, UNSET_VALUE);
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
//  backtrace: native [
//  
//  "Returns backtrace with label symbols, include properties if requested"
//
//      /limit
//          "Limit the length of the backtrace"
//      depth [none! integer!]
//          "Max stack levels to return, none for no limit"
//      /at
//          "Return only a single backtrace record"
//      level [integer!]
//          "Stack level to return properties for"
//      /function
//          "List function value in properties"
//      /where
//          "List block evaluation position in properties"
//      /args
//          "List args block in properties (may be modified since invocation)"
//      /only
//          "Do not list depths, only the selected properties"
//  ]
//
REBNATIVE(backtrace)
{
    REFINE(1, limit);
    PARAM(2, depth);
    REFINE(3, at);
    PARAM(4, level);
    REFINE(5, function);
    REFINE(6, where);
    REFINE(7, args);
    REFINE(8, only);

    REBCNT n = 1;
    REBCNT depth;
    REBCNT level;

    REBCNT index;

    REBARR *backtrace;
    struct Reb_Call *call = DSF->prior; // skip REBNATIVE(backtrace)

    Check_Security(SYM_DEBUG, POL_READ, 0);

    if (REF(limit) && REF(at)) {
        //
        // /LIMIT assumes that you are returning a list of backtrace items,
        // while /AT assumes exactly one.  They are mutually exclusive.
        //
        fail (Error(RE_BAD_REFINES));
    }

    if (REF(limit)) {
        if (IS_NONE(ARG(depth)))
            depth = MAX_U32; // NONE is no limit--as many frames as possible
        else {
            if (VAL_INT32(ARG(depth)) < 0)
                fail (Error_Invalid_Arg(ARG(depth)));
            depth = VAL_INT32(ARG(depth));
        }
    }
    else
        depth = 20; // On an 80x25 terminal leaves room to type...

    if (REF(at)) {
        if (VAL_INT32(ARG(level)) < 1)
            fail (Error_Invalid_Arg(ARG(level)));
        level = VAL_INT32(ARG(level));
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
        // *that* complicated.
        //
        index = 1;
        for (; call != NULL; call = PRIOR_DSF(call)) {
            if (call->mode != CALL_MODE_FUNCTION) continue;

            if (index > depth) {
                ++index; break; // just one slot for ellipsis
            }

            // index and props for all other slots, unless /ONLY
            //
            index += REF(only) ? 1 : 2;
        }

        // Because our depth count started at 1 index started at 1 (so the
        // check against the depth limit was right), but for the actual
        // element count we need to decrement it.
        //
        --index;

        // Reset the call for the second walk
        //
        call = DSF->prior;

        // Worst case scenario in terms of size, the backtrace will be all
        // pairs of integers with property blocks (Objects?).  The user
        // should be able to pick from the result by integer with 1 being
        // the topmost stack level.
        //
        // We never show backtrace itself (this currently running native)
        // in the list, hence a (-1) * 2.  But we may add a SYM_ELLIPSIS to
        // the start if output has been truncated, so that adds 1 element.
        //
        backtrace = Make_Array(index);
        SET_ARRAY_LEN(backtrace, index);
        TERM_ARRAY(backtrace);
    }

    for (; call != NULL; call = PRIOR_DSF(call)) {
        REBARR *props;
        REBCNT len;
        REBVAL *temp;

        //
        // Only consider invoked functions (not pending ones or parens)
        //
        if (call->mode != CALL_MODE_FUNCTION)
            continue;

        if (REF(at)) {
            if (n != level) {
                ++n;
                continue;
            }
        }
        else {
            if (n > depth) {
                //
                // If there's more stack levels to be shown than we were asked
                // to show, then put an ellipsis in the list and break.
                //
                temp = ARRAY_AT(backtrace, --index);
                Val_Init_Word_Unbound(temp, REB_WORD, SYM_ELLIPSIS);
                if (!REF(only))
                    VAL_SET_OPT(temp, OPT_VALUE_LINE); // put on own line
                break;
            }
        }

        // The /ONLY case is bare bones and just gives a block of the label
        // symbols (at this point in time).
        //
        if (REF(only)) {
            if (REF(at)) {
                Val_Init_Word_Unbound(temp, REB_WORD, DSF_LABEL_SYM(call));
                return R_OUT;
            }

            temp = ARRAY_AT(backtrace, --index);
            Val_Init_Word_Unbound(temp, REB_WORD, DSF_LABEL_SYM(call));
            ++n;
            continue;
        }

        // Make the properties list array the right size for the refinements
        // that were requested.
        //
        len = (
            1 // label always in the list
            + REF(function) ? 1 : 0
            + REF(where) ? 1 : 0
            + REF(args) ? 1 : 0
        );
        props = Make_Array(len);

        Val_Init_Word_Unbound(
            Alloc_Tail_Array(props), REB_WORD, DSF_LABEL_SYM(call)
        );

        if (REF(function))
            *Alloc_Tail_Array(props) = *FUNC_VALUE(DSF_FUNC(call));

        if (REF(where))
            Val_Init_Block_Index(
                Alloc_Tail_Array(props), DSF_ARRAY(call), DSF_EXPR_INDEX(call)
            );

        if (REF(args)) {
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

            for (; NOT_END(param); param++, arg++, dest++) {
                if (VAL_GET_EXT(param, EXT_TYPESET_HIDDEN))
                    continue;
                *dest = *arg;
            }

            SET_ARRAY_LEN(array, dest - ARRAY_HEAD(array));
            TERM_ARRAY(array);

            Val_Init_Block(Alloc_Tail_Array(props), array);
        }

        if (REF(at)) {
            //
            // If we were fetching a single stack level, then the props
            // is our return result (if we found that level and got here)
            //
            Val_Init_Block(OUT, props);
            return R_OUT;
        }

        // If building a backtrace, we just keep accumulating results as long
        // as there are stack levels left and the limit hasn't been hit.
        //
        temp = ARRAY_AT(backtrace, --index);
        Val_Init_Block(temp, props);

        // The integer identifying the stack level (used to refer to it
        // in other debugging commands).  Since we're going in reverse, we
        // add it after the props so it will show up before, and give it
        // the newline break marker.
        //
        temp = ARRAY_AT(backtrace, --index);
        SET_INTEGER(temp, n);
        VAL_SET_OPT(temp, OPT_VALUE_LINE);
        ++n;
    }

    // If we ran out of stack levels before finding the single one requested
    // via /AT, return a NONE!
    //
    if (REF(at))
        return R_NONE;

    // Return accumulated backtrace otherwise.  The reverse filling process
    // should have exactly used up all the index slots, leaving index at 0.
    //
    assert(index == 0);
    Val_Init_Block(OUT, backtrace);
    return R_OUT;
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
