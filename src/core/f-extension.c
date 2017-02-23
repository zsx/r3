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
// NOTE: The R3-Alpha extension mechanism and API are deprecated in Ren-C.
//
// See %reb-ext.h for a general overview of R3-Alpha extensions.  Also:
//
// http://www.rebol.com/r3/docs/concepts/extensions-embedded.html
//

#include "sys-core.h"

#include "reb-ext.h"
#include "reb-evtypes.h"

#include "reb-lib.h"
#include "sys-ext.h"

//(*call)(int cmd, RXIFRM *args);

typedef struct reb_ext {
    RXICAL call;                // Call(function) entry point
    void *dll;                  // DLL library "handle"
    int  index;                 // Index in extension table
    int  object;                // extension object reference
} REBEXT;

// !!!! The list below should not be hardcoded, but until someone
// needs a lot of extensions, it will do fine.
REBEXT Ext_List[64];
REBCNT Ext_Next = 0;


typedef REBYTE *(INFO_FUNC)(REBINT opts, void *lib);

//
// Just an ID for the handler
//
static void cleanup_extension_init_handler(const REBVAL *val)
{
}

static void cleanup_extension_quit_handler(const REBVAL *val)
{
}

//
//  load-extension-helper: native [
//
//  "Low level extension module loader (for DLLs)."
//
//      path-or-handle [file! handle!] "Path to the extension file or handle to a builtin extension"
//  ]
//
REBNATIVE(load_extension_helper)
//
// Low level extension loader:
//
// 1. Opens the DLL for the extension
// 2. Calls RX_Init() to initialize and get its definition header (REBOL)
// 3. Creates a extension object and returns it
// 4. REBOL code then uses that object to define the extension module
//    including natives, data, exports, etc.
//
// Each extension is defined as DLL with:
//
// RX_Init() - init anything needed
// optinoal RX_Quit() - cleanup anything needed
{
    INCLUDE_PARAMS_OF_LOAD_EXTENSION_HELPER;

    REBCTX *std_ext_ctx = VAL_CONTEXT(Get_System(SYS_STANDARD, STD_EXTENSION));
    REBCTX *context;

    if (IS_FILE(ARG(path_or_handle))) {
        REBVAL *path = ARG(path_or_handle);

        //Check_Security(SYM_EXTENSION, POL_EXEC, val);

        REBVAL lib;
        MAKE_Library(&lib, REB_LIBRARY, path);

        // check if it's reloading an existing extension
        REBVAL *loaded_exts = CTX_VAR(VAL_CONTEXT(ROOT_SYSTEM), SYS_EXTENSIONS);
        if (IS_BLOCK(loaded_exts)) {
            RELVAL *item = VAL_ARRAY_HEAD(loaded_exts);
            for (; NOT_END(item); ++item) {
                // do some sanity checking, just to avoid crashing if system/extensions was messed up
                if (!IS_OBJECT(item))
                    fail(Error(RE_BAD_EXTENSION, item));

                REBCTX *item_ctx = VAL_CONTEXT(item);
                if ((CTX_LEN(item_ctx) <= STD_EXTENSION_LIB_BASE)
                    || CTX_KEY_SPELLING(item_ctx, STD_EXTENSION_LIB_BASE)
                    != CTX_KEY_SPELLING(std_ext_ctx, STD_EXTENSION_LIB_BASE)
                    ) {
                    fail(Error(RE_BAD_EXTENSION, item));
                }
                else {
                    if (IS_BLANK(CTX_VAR(item_ctx, STD_EXTENSION_LIB_BASE))) {//builtin extension
                        continue;
                    }
                }

                assert(IS_LIBRARY(CTX_VAR(item_ctx, STD_EXTENSION_LIB_BASE)));

                if (VAL_LIBRARY_FD(&lib)
                    == VAL_LIBRARY_FD(CTX_VAR(item_ctx, STD_EXTENSION_LIB_BASE))) {
                    // found the existing extension
                    OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(&lib)); //decrease the reference added by MAKE_library
                    *D_OUT = *KNOWN(item);
                    return R_OUT;
                }
            }
        }
        context = Copy_Context_Shallow(std_ext_ctx);
        *CTX_VAR(context, STD_EXTENSION_LIB_BASE) = lib;
        *CTX_VAR(context, STD_EXTENSION_LIB_FILE) = *path;

        CFUNC *RX_Init = OS_FIND_FUNCTION(VAL_LIBRARY_FD(&lib), "RX_Init");
        if (!RX_Init) {
            OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(&lib));
            fail(Error(RE_BAD_EXTENSION, path));
        }

        // Call its RX_Init function for header and code body:
        if (cast(INIT_FUNC, RX_Init)(CTX_VAR(context, STD_EXTENSION_SCRIPT),
            CTX_VAR(context, STD_EXTENSION_MODULES)) < 0) {
            OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(&lib));
            fail(Error(RE_EXTENSION_INIT, path));
        }
    }
    else {
        assert(IS_HANDLE(ARG(path_or_handle)));
        REBVAL *handle = ARG(path_or_handle);
        if (VAL_HANDLE_CLEANER(handle) != cleanup_extension_init_handler) {
            fail(Error(RE_BAD_EXTENSION, handle));
        }
        INIT_FUNC RX_Init = cast(INIT_FUNC, VAL_HANDLE_POINTER(handle));
        context = Copy_Context_Shallow(std_ext_ctx);
        if (RX_Init(CTX_VAR(context, STD_EXTENSION_SCRIPT),
            CTX_VAR(context, STD_EXTENSION_MODULES)) < 0) {
            fail(Error(RE_EXTENSION_INIT, handle));
        }
    }

    Init_Object(D_OUT, context);
    return R_OUT;
}


//
//  unload-extension-helper: native [
//
//  "Unload an extension"
//      return: [<opt>]
//      ext [object!] "The extension to be unloaded"
//      /cleanup cleaner [handle!] "The RX_Quit pointer for the builtin extension"
//  ]
//
REBNATIVE(unload_extension_helper)
{
    INCLUDE_PARAMS_OF_UNLOAD_EXTENSION_HELPER;

    REBCTX *std = VAL_CONTEXT(Get_System(SYS_STANDARD, STD_EXTENSION));
    REBCTX *context = VAL_CONTEXT(ARG(ext));
    if ((CTX_LEN(context) <= STD_EXTENSION_LIB_BASE)
        || (CTX_KEY_CANON(context, STD_EXTENSION_LIB_BASE)
            != CTX_KEY_CANON(std, STD_EXTENSION_LIB_BASE))) {
        fail(Error(RE_INVALID_ARG, ARG(ext)));
    }
    if (!REF(cleanup)) {
        REBVAL *lib = CTX_VAR(context, STD_EXTENSION_LIB_BASE);
        if (!IS_LIBRARY(lib))
            fail(Error(RE_INVALID_ARG, ARG(ext)));

        if (IS_LIB_CLOSED(VAL_LIBRARY(lib)))
            fail(Error(RE_BAD_LIBRARY));

        CFUNC *RX_Quit = OS_FIND_FUNCTION(VAL_LIBRARY_FD(lib), "RX_Quit");
        if (!RX_Quit) {
            OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
            return R_VOID;
        }
        int ret = cast(QUIT_FUNC, RX_Quit)();
        if (ret < 0) {
            OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
            REBVAL i;
            SET_INTEGER(&i, ret);
            fail(Error(RE_FAIL_TO_QUIT_EXTENSION, i));
        }

        OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
    }
    else {
        if (VAL_HANDLE_CLEANER(ARG(cleaner)) != cleanup_extension_quit_handler)
            fail(Error(RE_INVALID_ARG, ARG(cleaner)));
        void *RX_Quit = VAL_HANDLE_POINTER(ARG(cleaner));
        int ret = cast(QUIT_FUNC, RX_Quit)();
        if (ret < 0) {
            REBVAL i;
            SET_INTEGER(&i, ret);
            fail(Error(RE_FAIL_TO_QUIT_EXTENSION, i));
        }
    }

    return R_VOID;
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
        rebext = &Ext_List[cast(REBUPT, VAL_HANDLE_POINTER(handle))];
        if (!rebext || !rebext->call) goto bad_func_def;
    }

    if (!IS_INTEGER(command_num) || VAL_INT64(command_num) > 0xffff)
        goto bad_func_def;

    if (!IS_BLOCK(spec)) goto bad_func_def;

    REBFUN *fun; // goto would cross initialization
    fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(spec, MKF_KEYWORDS),
        &Command_Dispatcher,
        NULL, // no underlying function, fundamental
        NULL // not providing a specialization
    );

    // There is no "code" for a body, but there is information that tells the
    // Command_Dispatcher what to do: `extension` and `command_num`.  This
    // two element array is placed in the FUNC_BODY value.
    //
    REBARR *body_array; // goto would cross initialization
    body_array = Make_Array(2);
    Append_Value(body_array, extension);
    Append_Value(body_array, command_num);

    Init_Block(FUNC_BODY(fun), body_array); // manages

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
        Init_Block(&def, array);

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
    INCLUDE_PARAMS_OF_MAKE_COMMAND;

    REBVAL *def = ARG(def);
    if (VAL_LEN_AT(def) != 3)
        fail (Error_Invalid_Arg(def));

    REBVAL spec;
    Derelativize(&spec, VAL_ARRAY_AT(def), VAL_SPECIFIER(def));

    REBVAL extension;
    Derelativize(&extension, VAL_ARRAY_AT(def) + 1, VAL_SPECIFIER(def));

    REBVAL command_num;
    Derelativize(&command_num, VAL_ARRAY_AT(def) + 2, VAL_SPECIFIER(def));

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
        cast(REBUPT, VAL_HANDLE_POINTER(VAL_CONTEXT_VAR(data, SELFISH(1))))
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
    Init_Any_Context(&frame, REB_FRAME, frame_ctx);

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



//
// Just an ID for the handler
//
static void cleanup_module_handler(const REBVAL *val)
{
}


//
//  Make_Extension_Module_Array: C
//
// Make an extension module array for being loaded later
//
REBARR *Make_Extension_Module_Array(
    const REBYTE spec[], REBCNT len,
    REBNAT impl[], REBCNT n,
    REBCNT error_base)
{
    // the array will be like [spec C_func error_base/none]
    REBARR *arr = Make_Array(3);
    TERM_ARRAY_LEN(arr, 3);
    Init_Binary(ARR_AT(arr, 0), Copy_Bytes(spec, len));
    Init_Handle_Managed(ARR_AT(arr,1), cast(void *, impl), n, &cleanup_module_handler);
    if (error_base < 0) {
        SET_BLANK(ARR_AT(arr, 2));
    } else {
        SET_INTEGER(ARR_AT(arr, 2), error_base);
    }
    return arr;
}


//
//  Prepare_Boot_Extensions: C
//
// Convert an extension [Init Quit] array to [handle! handle!] array
//
void Prepare_Boot_Extensions(REBVAL *exts, CFUNC **funcs, REBCNT n)
{
    REBARR *arr = Make_Array(n);
    REBCNT i;
    for (i = 0; i < n; i += 2) {
        RELVAL *val = Alloc_Tail_Array(arr);
        Init_Handle_Managed(val, cast(void *, funcs[i]),
            0, &cleanup_extension_init_handler);
        val = Alloc_Tail_Array(arr);
        Init_Handle_Managed(val, cast(void *, funcs[i + 1]),
            0, &cleanup_extension_quit_handler);
    }
    Init_Block(exts, arr);
}

//
//  Shutdown_Boot_Extensions: C
//
// Call QUIT functions of boot extensions in the reversed order
//
// Note that this function does not call unload-extension, that is why it is
// called SHUTDOWN instead of UNLOAD, because it's only supposed to be called
// when the interpreter is shutting down, at which point, unloading an extension
// is not necessary. Plus, there is not an elegant way to call unload-extension
// on each of boot extensions: boot extensions are passed to host-start as a
// block, and there is no host-shutdown function which would be an ideal place
// to such things.
//
void Shutdown_Boot_Extensions(CFUNC **funcs, REBCNT n)
{
    for (; n > 1; n -= 2) {
        cast(QUIT_FUNC, funcs[n - 1])();
    }
}


//
//  load-native: native [
//
//  "Load a native from a built-in extension"
//
//      return: [function!]
//      "function value, will be created from the native implementation"
//      spec [block!]
//      "spec of the native"
//      impl [handle!]
//      "a handle returned from RX_Init_ of the extension"
//      index [integer!]
//      "Index of the native"
//      /body
//      code [block!]
//      "User-equivalent body"
//      /unloadable
//      "The native can be unloaded later (when the extension is unloaded)"
//  ]
//
REBNATIVE(load_native)
{
    INCLUDE_PARAMS_OF_LOAD_NATIVE;

    if (VAL_HANDLE_CLEANER(ARG(impl)) != cleanup_module_handler
        || VAL_INT64(ARG(index)) < 0
        || cast(REBUPT, VAL_INT64(ARG(index))) >= VAL_HANDLE_LEN(ARG(impl)))
        fail (Error(RE_MISC));

    REBFUN *fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(ARG(spec), MKF_KEYWORDS | MKF_FAKE_RETURN),
        cast(REBNAT*, VAL_HANDLE_POINTER(ARG(impl)))[VAL_INT64(ARG(index))], // unique
        NULL, // no underlying function, this is fundamental
        NULL // not providing a specialization
    );

    if (REF(unloadable))
        SET_VAL_FLAG(FUNC_VALUE(fun), FUNC_FLAG_UNLOADABLE_NATIVE);

    if (REF(body)) {
        *FUNC_BODY(fun) = *ARG(code);
    }
    *D_OUT = *FUNC_VALUE(fun);
    return R_OUT;
}


//
//  Unloaded_Dispatcher: C
//
// This will be the dispatcher for the natives in an extension after the
// extension is unloaded.
//
static REB_R Unloaded_Dispatcher(REBFRM *f)
{
    assert(f != NULL); // unused argument warning otherwise
    fail (Error(RE_NATIVE_UNLOADED, FUNC_VALUE(f->func)));
}


//
//  unload-native: native [
//
//  "Unload a native when the containing extension is unloaded"
//
//      return: [<opt>]
//      nat [function!] "The native function to be unloaded"
//  ]
//
REBNATIVE(unload_native)
{
    INCLUDE_PARAMS_OF_UNLOAD_NATIVE;

    REBFUN *fun = VAL_FUNC(ARG(nat));
    if (NOT_VAL_FLAG(FUNC_VALUE(fun), FUNC_FLAG_UNLOADABLE_NATIVE))
        fail (Error(RE_NON_UNLOADABLE_NATIVE, ARG(nat)));

    FUNC_DISPATCHER(VAL_FUNC(ARG(nat))) = Unloaded_Dispatcher;

    return R_VOID;
}


//
//  Init_Extension_Words: C
//
// Intern strings and save their canonical forms
//
void Init_Extension_Words(const REBYTE* strings[], REBSTR *canons[], REBCNT n)
{
    REBCNT i;
    for (i = 0; i < n; ++i) {
        canons[i] = Intern_UTF8_Managed(strings[i], LEN_BYTES(strings[i]));
    }
}
