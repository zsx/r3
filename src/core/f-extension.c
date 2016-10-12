//
//  File: %f-extension.c
//  Summary: "support for extensions"
//  Section: functional
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
// NOTE: The R3-Alpha extension mechanism and API are deprecated in Ren-C.
//
// See %reb-ext.h for a general overview of R3-Alpha extensions.  Also:
//
// http://www.rebol.com/r3/docs/concepts/extensions-embedded.html
//

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

    REBVAL *val = ARG(name);

    REBSER *src;
    CFUNC *call; // RXICAL
    void *dll;

    //Check_Security(SYM_EXTENSION, POL_EXEC, val);

    if (!REF(dispatch)) { // use the DLL file

        if (!IS_FILE(val)) fail (Error_Invalid_Arg(val));

        // !!! By passing NULL we don't get backing series to protect!
        REBCHR *name = Val_Str_To_OS_Managed(NULL, val);

        // Try to load the DLL file:
        REBCNT err_num;
        if (!(dll = OS_OPEN_LIBRARY(name, &err_num))) {
            fail (Error(RE_NO_EXTENSION, val));
        }

        // Call its INFO_FUNC info() function for header and code body:

        CFUNC *info = OS_FIND_FUNCTION(
            dll,
            cs_cast(BOOT_STR(RS_EXTENSION, 0))
        );
        if (!info){
            OS_CLOSE_LIBRARY(dll);
            fail (Error(RE_BAD_EXTENSION, val));
        }

        // Obtain info string as UTF8:
        REBYTE *code;
        if (!(code = cast(INFO_FUNC*, info)(0, Extension_Lib()))) {
            OS_CLOSE_LIBRARY(dll);
            fail (Error(RE_EXTENSION_INIT, val));
        }

        // Import the string into REBOL-land:
        src = Copy_Bytes(code, -1);
        call = OS_FIND_FUNCTION(
            dll, cs_cast(BOOT_STR(RS_EXTENSION, 2))
        ); // zero is allowed
    }
    else {
        // Hosted extension:
        src = VAL_SERIES(val);
        call = VAL_HANDLE_CODE(ARG(function));
        dll = 0;
    }

    REBEXT *ext = &Ext_List[Ext_Next];
    CLEARS(ext);
    ext->call = cast(RXICAL, call);
    ext->dll = dll;
    ext->index = Ext_Next++;

    // Extension return: dll, info, filename
    REBCTX *context = Copy_Context_Shallow(
        VAL_CONTEXT(Get_System(SYS_STANDARD, STD_EXTENSION))
    );

    // Set extension fields needed:
    Init_Handle_Simple(
        CTX_VAR(context, STD_EXTENSION_LIB_BASE),
        NULL, // code
        cast(void*, cast(REBUPT, ext->index)) // data
    );

    if (!REF(dispatch))
        *CTX_VAR(context, STD_EXTENSION_LIB_FILE) = *ARG(name);

    Val_Init_Binary(CTX_VAR(context, STD_EXTENSION_LIB_BOOT), src);

    Val_Init_Object(D_OUT, context);
    return R_OUT;
}


//
//  Make_Command: C
// 
// `extension` is an object or module that represents the properties of the
// DLL or shared library (including its DLL handle, load or unload status,
// etc.)  `command-num` is a numbered function inside of that DLL, which
// (one hopes) has a binary interface able to serve the spec which was
// provided.  Though the same spec format is used as for ordinary functions
// in Rebol, the allowed datatypes are more limited...as not all Rebol types
// had a parallel interface under this conception.
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
        rebext = &Ext_List[cast(REBUPT, VAL_HANDLE_DATA(handle))];
        if (!rebext || !rebext->call) goto bad_func_def;
    }

    if (!IS_INTEGER(command_num) || VAL_INT64(command_num) > 0xffff)
        goto bad_func_def;

    if (!IS_BLOCK(spec)) goto bad_func_def;

    REBFUN *fun; // goto would cross initialization
    fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(spec, MKF_KEYWORDS),
        &Command_Dispatcher,
        NULL // no underlying function, fundamental
    );

    // There is no "code" for a body, but there is information that tells the
    // Command_Dispatcher what to do: `extension` and `command_num`.  This
    // two element array is placed in the FUNC_BODY value.
    //
    REBARR *body_array; // goto would cross initialization
    body_array = Make_Array(2);
    Append_Value(body_array, extension);
    Append_Value(body_array, command_num);

    Val_Init_Block(FUNC_BODY(fun), body_array); // manages

    *out = *FUNC_VALUE(fun);
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
//      return: [function!]
//      def [block!]
//  ]
//
REBNATIVE(make_command)
{
    PARAM(1, def);

    REBVAL *def = ARG(def);

    REBVAL spec;
    REBVAL extension;
    REBVAL command_num;

    if (VAL_LEN_AT(def) != 3)
        fail (Error_Invalid_Arg(def));

    COPY_VALUE(&spec, VAL_ARRAY_AT(def), VAL_SPECIFIER(def));
    COPY_VALUE(&extension, VAL_ARRAY_AT(def) + 1, VAL_SPECIFIER(def));
    COPY_VALUE(&command_num, VAL_ARRAY_AT(def) + 2, VAL_SPECIFIER(def));

    // Validity checking on the 3 elements done inside Make_Command, will
    // fail() if the input is not good.
    //
    Make_Command(D_OUT, &spec, &extension, &command_num);

    return R_OUT;
}


//
//  Command_Dispatcher: C
//
// !!! The very ad-hoc nature of the R3-Alpha extension API has made it a
// legacy-maintenance-only area.  See notes in %reb-ext.h.  It is being
// slowly upgraded with a more articulated sense of the difference between
// the C "internal API" and the C "external API".
//
REB_R Command_Dispatcher(REBFRM *f)
{
    // For a "body", a command has a data array with [ext-obj func-index]
    // See Make_Command() for an explanation of these two values.
    //
    REBVAL *data = KNOWN(VAL_ARRAY_HEAD(FUNC_BODY(f->func)));
    REBEXT *handler = &Ext_List[
        cast(REBUPT, VAL_HANDLE_DATA(VAL_CONTEXT_VAR(data, SELFISH(1))))
    ];
    REBCNT cmd_num = cast(REBCNT, Int32(data + 1));

    // !!! We don't want to pass a `struct Reb_Frame*` directly to clients of
    // the RL_Api (that's for internal APIs and user natives only).  Since
    // that API is aiming for a day when it only speaks in terms of REBVALs
    // (not RELVALs, REBSERs, etc) go ahead and reify and make a FRAME!.
    //
    REBCTX *frame_ctx = Context_For_Frame_May_Reify_Managed(f);
    
    // !!! Although the frame value *contents* may be copied into other safe
    // locations and GC managed for an indefinite lifetime, this particular
    // REBVAL pointer will only be safe and good for the duration of the call.
    //
    REBVAL frame;
    PUSH_GUARD_CONTEXT(frame_ctx);
    Val_Init_Context(&frame, REB_FRAME, frame_ctx);

    // Clients of the RL_Api should not be aware of the implementation detail
    // of END_CELL.  Pre-write f->out with void so that if a routine exposes
    // FRM_OUT they won't see it.
    //
    SET_VOID(f->out);

    REBCNT n  = handler->call(
        cmd_num, &frame, cast(REBCEC*, TG_Command_Execution_Context)
    );

    DROP_GUARD_CONTEXT(frame_ctx);

    assert(!THROWN(f->out));

    switch (n) {
    case RXR_VALUE:
        //
        // !!! By convention, it appears that returning RXR_VALUE means to
        // consider the first arg to be the return value.
        //
        *f->out = *FRM_ARG(f, 1);
        break;

    case RXR_VOID:
        SET_VOID(f->out);
        break;

    case RXR_BLANK:
        SET_BLANK(f->out);
        break;

    case RXR_TRUE:
        SET_TRUE(f->out);
        break;

    case RXR_FALSE:
        SET_FALSE(f->out);
        break;

    case RXR_BAD_ARGS:
        fail (Error(RE_BAD_CMD_ARGS));

    case RXR_NO_COMMAND:
        fail (Error(RE_NO_CMD));

    case RXR_ERROR:
        fail (Error(RE_COMMAND_FAIL));

    default:
        SET_VOID(f->out);
    }

    // Note: no current interface for Rebol "commands" to throw (to the extent
    // that REB_COMMAND has a future in Ren-C).

    return R_OUT;
}
