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
**  Module:  f-extension.c
**  Summary: support for extensions
**  Section: functional
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"

#include "reb-ext.h" // includes copy of ext-types.h
#include "reb-evtypes.h"

#include "reb-lib.h"


//(*call)(int cmd, RXIFRM *args);

typedef struct reb_ext {
    RXICAL call;                // Call(function) entry point
    void *dll;                  // DLL library "handle"
    int  index;                 // Index in extension table
    int  object;                // extension object reference
} REBEXT;

#include "tmp-exttypes.h"


// !!!! The list below should not be hardcoded, but until someone
// needs a lot of extensions, it will do fine.
REBEXT Ext_List[64];
REBCNT Ext_Next = 0;


/***********************************************************************
**
**  Local functions
**
***********************************************************************/

/***********************************************************************
**
x*/ void Value_To_RXI(RXIARG *arg, const REBVAL *val)
/*
***********************************************************************/
{
    switch (VAL_TYPE(val)) {
    case REB_LOGIC:
        //
        // LOGIC! changed to just be a header bit, and there is no VAL_I32
        // in the "payload" any longer.  It must be proxied.
        //
        arg->i2.int32a = (VAL_LOGIC(val) == TRUE) ? 1 : 0;
        break;

    case REB_INTEGER:
        arg->int64 = VAL_INT64(val);
        break;

    case REB_DECIMAL:
    case REB_PERCENT:
        arg->dec64 = VAL_DECIMAL(val);
        break;

    case REB_PAIR:
        arg->pair.x = VAL_PAIR_X(val);
        arg->pair.y = VAL_PAIR_Y(val);
        break;

    case REB_CHAR:
        arg->i2.int32a = VAL_CHAR(val);
        break;

    case REB_TUPLE:
        arg->addr = cast(void *, VAL_TUPLE_DATA(val));
        break;

    case REB_TIME:
        arg->int64 = VAL_TIME(val);
        break;

    case REB_DATE: /* FIXME: avoid type punning */
        arg->i2.int32a = VAL_ALL_BITS(val)[2];
        arg->i2.int32b = 0;
        break;

    case REB_WORD:
    case REB_SET_WORD:
    case REB_GET_WORD:
    case REB_LIT_WORD:
    case REB_REFINEMENT:
    case REB_ISSUE:
        arg->i2.int32a = VAL_WORD_CANON(val);
        arg->i2.int32b = 0;
        break;

    case REB_STRING:
    case REB_FILE:
    case REB_EMAIL:
    case REB_URL:
    case REB_TAG:
    case REB_BLOCK:
    case REB_GROUP:
    case REB_PATH:
    case REB_SET_PATH:
    case REB_GET_PATH:
    case REB_LIT_PATH:
    case REB_BINARY:
    case REB_BITSET:
    case REB_VECTOR:
        arg->sri.series = VAL_SERIES(val);
        arg->sri.index = VAL_INDEX(val);
        break;

    case REB_GOB:
        arg->addr = VAL_GOB(val);
        break;

    case REB_HANDLE:
        arg->addr = VAL_HANDLE_DATA(val);
        break;

    case REB_IMAGE:
        arg->iwh.image = VAL_SERIES(val);
        arg->iwh.width = VAL_IMAGE_WIDE(val);
        arg->iwh.height = VAL_IMAGE_HIGH(val);
        break;

    case REB_OBJECT:
    case REB_MODULE:
        arg->addr = VAL_CONTEXT(val);
        break;

    default:
        arg->int64 = 0;
        break;
    }
    return;
}

/***********************************************************************
**
x*/ void RXI_To_Value(REBVAL *val, const RXIARG *arg, REBRXT type)
/*
***********************************************************************/
{
    switch (type) {
    case RXT_TRASH:
        SET_TRASH_SAFE(val);
        break;

    case RXT_UNSET:
        SET_UNSET(val);
        break;

    case RXT_NONE:
        SET_NONE(val);
        break;

    case RXT_LOGIC:
        assert((arg->i2.int32a == 0) || (arg->i2.int32a == 1));
        SET_LOGIC(val, arg->i2.int32a);
        break;

    case RXT_INTEGER:
        SET_INTEGER(val, arg->int64);
        break;

    case RXT_DECIMAL:
        SET_DECIMAL(val, arg->dec64);
        break;

    case RXT_PERCENT:
        SET_PERCENT(val, arg->dec64);
        break;

    case RXT_CHAR:
        SET_CHAR(val, arg->i2.int32a);
        break;

    case RXT_PAIR:
        SET_PAIR(val, arg->pair.x, arg->pair.y);
        break;

    case RXT_TUPLE:
        SET_TUPLE(val, arg->addr);
        break;

    case RXT_TIME:
        SET_TIME(val, arg->int64);
        break;

    case RXT_DATE: // FIXME: see Value_To_RXI
        VAL_RESET_HEADER(val, REB_DATE);
        VAL_ALL_BITS(val)[2] = arg->i2.int32a;
        break;

    case RXT_WORD:
        Val_Init_Word(val, REB_WORD, arg->i2.int32a);
        break;

    case RXT_SET_WORD:
        Val_Init_Word(val, REB_SET_WORD, arg->i2.int32a);
        break;

    case RXT_GET_WORD:
        Val_Init_Word(val, REB_GET_WORD, arg->i2.int32a);
        break;

    case RXT_LIT_WORD:
        Val_Init_Word(val, REB_LIT_WORD, arg->i2.int32a);
        break;

    case RXT_REFINEMENT:
        Val_Init_Word(val, REB_REFINEMENT, arg->i2.int32a);
        break;

    case RXT_ISSUE:
        Val_Init_Word(val, REB_ISSUE, arg->i2.int32a);
        break;

    case RXT_BINARY:
        VAL_RESET_HEADER(val, REB_BINARY);
        goto ser;

    case RXT_STRING:
        VAL_RESET_HEADER(val, REB_STRING);
        goto ser;

    case RXT_FILE:
        VAL_RESET_HEADER(val, REB_FILE);
        goto ser;

    case RXT_EMAIL:
        VAL_RESET_HEADER(val, REB_EMAIL);
        goto ser;

    case RXT_URL:
        VAL_RESET_HEADER(val, REB_URL);
        goto ser;

    case RXT_TAG:
        VAL_RESET_HEADER(val, REB_TAG);
        goto ser;

    case RXT_BLOCK:
        VAL_RESET_HEADER(val, REB_BLOCK);
        goto ser;

    case RXT_GROUP:
        VAL_RESET_HEADER(val, REB_GROUP);
        goto ser;

    case RXT_PATH:
        VAL_RESET_HEADER(val, REB_PATH);
        goto ser;

    case RXT_SET_PATH:
        VAL_RESET_HEADER(val, REB_SET_PATH);
        goto ser;

    case RXT_GET_PATH:
        VAL_RESET_HEADER(val, REB_GET_PATH);
        goto ser;

    case RXT_LIT_PATH:
        VAL_RESET_HEADER(val, REB_LIT_PATH);
        goto ser;

    case RXT_BITSET:
        VAL_RESET_HEADER(val, REB_BITSET);
        goto ser;

    case RXT_VECTOR:
        VAL_RESET_HEADER(val, REB_VECTOR);
        goto ser;

    case RXT_GOB:
        SET_GOB(val, arg->addr);
        break;

    case RXT_HANDLE:
        SET_HANDLE_DATA(val, arg->addr);
        break;

    case RXT_IMAGE:
        INIT_VAL_SERIES(val, arg->iwh.image);
        VAL_IMAGE_WIDE(val) = arg->iwh.width;
        VAL_IMAGE_HIGH(val) = arg->iwh.height;
        break;

    case RXT_OBJECT:
        Val_Init_Object(val, arg->addr);
        break;

    case RXT_MODULE:
        Val_Init_Module(val, arg->addr);
        break;

    default:
        fail(Error(RE_BAD_CMD_ARGS));
    }

    return;

ser:
    INIT_VAL_SERIES(val, arg->sri.series);
    VAL_INDEX(val) = arg->sri.index;
}

/***********************************************************************
**
x*/ void RXI_To_Block(RXIFRM *frm, REBVAL *out) {
/*
***********************************************************************/
    REBCNT n;
    REBARR *array;
    REBVAL *val;
    REBCNT len;

    array = Make_Array(len = RXA_COUNT(frm));
    for (n = 1; n <= len; n++) {
        val = Alloc_Tail_Array(array);
        RXI_To_Value(val, &frm->args[n], RXA_TYPE(frm, n));
    }
    Val_Init_Block(out, array);
}


typedef REBYTE *(INFO_FUNC)(REBINT opts, void *lib);


//
//  load-extension: native [
//  
//  "Low level extension module loader (for DLLs)."
//  
//      name [file! binary!] "DLL file or UTF-8 source"
//      /dispatch {Specify native command dispatch (from hosted extensions)}
//      function [handle!] "Command dispatcher (native)"
//  ]
//
REBNATIVE(load_extension)
//
// Low level extension loader:
// 
// 1. Opens the DLL for the extension
// 2. Calls its Info() command to get its definition header (REBOL)
// 3. Inits an extension structure (dll, Call() function)
// 4. Creates a extension object and returns it
// 5. REBOL code then uses that object to define the extension module
//    including commands, functions, data, exports, etc.
// 
// Each extension is defined as DLL with:
// 
// init() - init anything needed
// quit() - cleanup anything needed
// call() - dispatch a native
{
    PARAM(1, name);
    REFINE(2, dispatch);
    PARAM(3, function);

    REBCHR *name;
    void *dll;
    REBCNT error;
    REBYTE *code;
    CFUNC *info; // INFO_FUNC
    REBCTX *context;
    REBVAL *val = D_ARG(1);
    REBEXT *ext;
    CFUNC *call; // RXICAL
    REBSER *src;
    int Remove_after_first_run;
    //Check_Security(SYM_EXTENSION, POL_EXEC, val);

    if (!REF(dispatch)) { // No /dispatch, use the DLL file:

        if (!IS_FILE(val)) fail (Error_Invalid_Arg(val));

        // !!! By passing NULL we don't get backing series to protect!
        name = Val_Str_To_OS_Managed(NULL, val);

        // Try to load the DLL file:
        if (!(dll = OS_OPEN_LIBRARY(name, &error))) {
            fail (Error(RE_NO_EXTENSION, val));
        }

        // Call its info() function for header and code body:
        if (!(info = OS_FIND_FUNCTION(dll, cs_cast(BOOT_STR(RS_EXTENSION, 0))))){
            OS_CLOSE_LIBRARY(dll);
            fail (Error(RE_BAD_EXTENSION, val));
        }

        // Obtain info string as UTF8:
        if (!(code = cast(INFO_FUNC*, info)(0, Extension_Lib()))) {
            OS_CLOSE_LIBRARY(dll);
            fail (Error(RE_EXTENSION_INIT, val));
        }

        // Import the string into REBOL-land:
        src = Copy_Bytes(code, -1); // Nursery protected
        call = OS_FIND_FUNCTION(dll, cs_cast(BOOT_STR(RS_EXTENSION, 2))); // zero is allowed
    }
    else {
        // Hosted extension:
        src = VAL_SERIES(val);
        call = VAL_HANDLE_CODE(D_ARG(3));
        dll = 0;
    }

    ext = &Ext_List[Ext_Next];
    CLEARS(ext);
    ext->call = cast(RXICAL, call);
    ext->dll = dll;
    ext->index = Ext_Next++;

    // Extension return: dll, info, filename
    context = Copy_Context_Shallow(
        VAL_CONTEXT(Get_System(SYS_STANDARD, STD_EXTENSION))
    );
    Val_Init_Object(D_OUT, context);

    // Set extension fields needed:
    val = CTX_VAR(context, STD_EXTENSION_LIB_BASE);
    VAL_RESET_HEADER(val, REB_HANDLE);
    VAL_I32(val) = ext->index;

    if (!D_REF(2))
        *CTX_VAR(context, STD_EXTENSION_LIB_FILE) = *D_ARG(1);

    Val_Init_Binary(CTX_VAR(context, STD_EXTENSION_LIB_BOOT), src);

    return R_OUT;
}


//
//  Make_Command: C
// 
// A REB_COMMAND is used to connect a Rebol function spec to implementation
// inside of a C DLL.  That implementation uses a set of APIs (RXIARG, etc.)
// which were developed prior to Rebol becoming open source.  It was
// intended that C developers could use an API that was a parallel subset
// of Rebol's internal code, that would be binary stable to survive any
// reorganizations.
// 
// `extension` is an object or module that represents the properties of the
// DLL or shared library (including its DLL handle, load or unload status,
// etc.)  `command-num` is a numbered function inside of that DLL, which
// (one hopes) has a binary interface able to serve the spec which was
// provided.  Though the same spec format is used as for ordinary functions
// in Rebol, the allowed datatypes are more limited...as not all Rebol types
// had a parallel interface under this conception.
// 
// Subsequent to the open-sourcing, the Ren/C initiative is not focusing on
// the REB_COMMAND model--preferring to connect the Rebol core directly as
// a library to bindings.  However, as it was the only extension model
// available under closed-source Rebol3, several pieces of code were built
// to depend upon it for functionality.  This included the cryptography
// extensions needed for secure sockets and a large part of the GUI.
// 
// Being able to quarantine the REB_COMMAND machinery to only builds that
// need it is a working objective.
//
void Make_Command(
    REBVAL *out,
    const REBVAL *spec,
    const REBVAL *extension,
    const REBVAL *command_num
) {
    if (!IS_MODULE(extension) && !IS_OBJECT(extension)) goto bad_func_def;

    // Check that handle and extension are somewhat valid (not used)
    {
        REBEXT *rebext;
        REBVAL *handle = VAL_CONTEXT_VAR(extension, SELFISH(1));
        if (!IS_HANDLE(handle)) goto bad_func_def;
        rebext = &Ext_List[VAL_I32(handle)];
        if (!rebext || !rebext->call) goto bad_func_def;
    }

    if (!IS_INTEGER(command_num) || VAL_INT64(command_num) > 0xffff)
        goto bad_func_def;

    if (!IS_BLOCK(spec)) goto bad_func_def;

    VAL_RESET_HEADER(out, REB_FUNCTION); // clears exts and opts in header...
    INIT_VAL_FUNC_CLASS(out, FUNC_CLASS_COMMAND);

    // See notes in `Make_Function()` about why a copy is *required*.
    VAL_FUNC_SPEC(out) =
        Copy_Array_At_Deep_Managed(VAL_ARRAY(spec), VAL_INDEX(spec));

    out->payload.function.func
        = AS_FUNC(Make_Paramlist_Managed(VAL_FUNC_SPEC(out), SYM_0));

    // There is no "body", but we want to save `extension` and `command_num`
    // and the only place there is to put it is in the place where a function
    // body series would go.  So make a 2 element series to store them and
    // copy the values into it.
    //
    VAL_FUNC_BODY(out) = Make_Array(2);
    Append_Value(VAL_FUNC_BODY(out), extension);
    Append_Value(VAL_FUNC_BODY(out), command_num);
    MANAGE_ARRAY(VAL_FUNC_BODY(out));

    // Put the command REBVAL in slot 0 so that REB_COMMAND, like other
    // function types, can find the function value from the paramlist.

    *FUNC_VALUE(out->payload.function.func) = *out;

    // Make sure the command doesn't use any types for which an "RXT" parallel
    // datatype (to a REB_XXX type) has not been published:
    {
        REBVAL *args = VAL_FUNC_PARAMS_HEAD(out);
        for (; NOT_END(args); args++) {
            if (
                (3 != ~VAL_TYPESET_BITS(args)) // not END and UNSET (no args)
                && (VAL_TYPESET_BITS(args) & ~RXT_ALLOWED_TYPES)
            ) {
                fail (Error(RE_BAD_FUNC_ARG, args));
            }
        }
    }

    return;

bad_func_def:
    {
        // emulate error before refactoring (improve if it's relevant...)
        REBVAL def;
        REBARR *array = Make_Array(3);
        Append_Value(array, spec);
        Append_Value(array, extension);
        Append_Value(array, command_num);
        Val_Init_Block(&def, array);

        fail (Error(RE_BAD_FUNC_DEF, &def));
    }
}


//
//  make-command: native [
//
//  {Native for creating the FUNCTION! subclass for what was once COMMAND!}
//
//      def [block!]
//  ]
//
REBNATIVE(make_command)
{
    PARAM(1, def);

    REBVAL *def = ARG(def);

    if (VAL_LEN_AT(def) != 3)
        fail (Error_Invalid_Arg(def));

    // Validity checking on the 3 elements done inside Make_Command, will
    // fail() if the input is not good.
    //
    Make_Command(
        D_OUT,
        VAL_ARRAY_AT(def), // spec
        VAL_ARRAY_AT(def) + 1, // extension
        VAL_ARRAY_AT(def) + 2 // command_num
    );

    return R_OUT;
}


//
//  Do_Command_Core: C
// 
// Evaluates the arguments for a command function and creates
// a resulting stack frame (struct or object) for command processing.
// 
// A command value consists of:
//     args - same as other funcs
//     spec - same as other funcs
//     body - [ext-obj func-index]
//
void Do_Command_Core(struct Reb_Frame *frame_)
{
    // All of these were checked above on definition:
    REBVAL *val = ARR_HEAD(FUNC_BODY(D_FUNC));
    // Handler
    REBEXT *ext = &Ext_List[VAL_I32(VAL_CONTEXT_VAR(val, SELFISH(1)))];
    REBCNT cmd = cast(REBCNT, Int32(val + 1));

    REBCNT n;
    RXIFRM frm; // args stored here

    // Copy args to command frame (array of args):
    RXA_COUNT(&frm) = D_ARGC;
    if (D_ARGC > 7) fail (Error(RE_BAD_COMMAND));
    val = D_ARG(1);
    for (n = 1; n <= D_ARGC; n++, val++) {
        RXA_TYPE(&frm, n) = Reb_To_RXT[VAL_TYPE_0(val)];
        Value_To_RXI(&frm.args[n], val);
    }

    // Call the command:
    n = ext->call(cmd, &frm, cast(REBCEC*, TG_Command_Execution_Context));

    assert(!THROWN(D_OUT));

    switch (n) {
    case RXR_VALUE:
        RXI_To_Value(D_OUT, &frm.args[1], RXA_TYPE(&frm, 1));
        break;
    case RXR_BLOCK:
        RXI_To_Block(&frm, D_OUT);
        break;
    case RXR_UNSET:
        SET_UNSET(D_OUT);
        break;
    case RXR_NONE:
        SET_NONE(D_OUT);
        break;
    case RXR_TRUE:
        SET_TRUE(D_OUT);
        break;
    case RXR_FALSE:
        SET_FALSE(D_OUT);
        break;

    case RXR_BAD_ARGS:
        fail (Error(RE_BAD_CMD_ARGS));

    case RXR_NO_COMMAND:
        fail (Error(RE_NO_CMD));

    case RXR_ERROR:
        fail (Error(RE_COMMAND_FAIL));

    default:
        SET_UNSET(D_OUT);
    }

    // Note: no current interface for Rebol "commands" to throw (to the extent
    // that REB_COMMAND has a future in Ren-C).  If it could throw, then
    // this would set `f->mode = CALL_MODE_THROW_PENDING` in that case.
}
