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

typedef struct rxi_cmd_context {
    void *envr;     // for holding a reference to your environment
    REBARR *block;  // block being evaluated
    REBCNT index;   // 0-based index of current command in block
} REBCEC;

typedef int (*RXICAL)(int cmd, const REBVAL *frame, REBCEC *ctx);

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
    UNUSED(val);
}

static void cleanup_extension_quit_handler(const REBVAL *val)
{
    UNUSED(val);
}

//
//  load-extension-helper: native [
//
//  "Low level extension module loader (for DLLs)."
//
//      path-or-handle [file! handle!]
//          "Path to the extension file or handle to a builtin extension"
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
// optional RX_Quit() - cleanup anything needed
{
    INCLUDE_PARAMS_OF_LOAD_EXTENSION_HELPER;

    REBCTX *std_ext_ctx = VAL_CONTEXT(Get_System(SYS_STANDARD, STD_EXTENSION));
    REBCTX *context;

    if (IS_FILE(ARG(path_or_handle))) {
        REBVAL *path = ARG(path_or_handle);

        //Check_Security(SYM_EXTENSION, POL_EXEC, val);

        DECLARE_LOCAL (lib);
        MAKE_Library(lib, REB_LIBRARY, path);

        // check if it's reloading an existing extension
        REBVAL *loaded_exts = CTX_VAR(VAL_CONTEXT(ROOT_SYSTEM), SYS_EXTENSIONS);
        if (IS_BLOCK(loaded_exts)) {
            RELVAL *item = VAL_ARRAY_HEAD(loaded_exts);
            for (; NOT_END(item); ++item) {
                //
                // do some sanity checking, just to avoid crashing if
                // system/extensions was messed up

                if (!IS_OBJECT(item))
                    fail(Error_Bad_Extension_Raw(item));

                REBCTX *item_ctx = VAL_CONTEXT(item);
                if ((CTX_LEN(item_ctx) <= STD_EXTENSION_LIB_BASE)
                    || CTX_KEY_SPELLING(item_ctx, STD_EXTENSION_LIB_BASE)
                    != CTX_KEY_SPELLING(std_ext_ctx, STD_EXTENSION_LIB_BASE)
                    ) {
                    fail(Error_Bad_Extension_Raw(item));
                }
                else {
                    if (IS_BLANK(CTX_VAR(item_ctx, STD_EXTENSION_LIB_BASE)))
                        continue; //builtin extension
                }

                assert(IS_LIBRARY(CTX_VAR(item_ctx, STD_EXTENSION_LIB_BASE)));

                if (
                    VAL_LIBRARY_FD(CTX_VAR(item_ctx, STD_EXTENSION_LIB_BASE))
                    == VAL_LIBRARY_FD(lib)
                ){
                    // found the existing extension, decrease the reference
                    // added by MAKE_library

                    OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
                    Move_Value(D_OUT, KNOWN(item));
                    return R_OUT;
                }
            }
        }
        context = Copy_Context_Shallow(std_ext_ctx);
        Move_Value(CTX_VAR(context, STD_EXTENSION_LIB_BASE), lib);
        Move_Value(CTX_VAR(context, STD_EXTENSION_LIB_FILE), path);

        CFUNC *RX_Init = OS_FIND_FUNCTION(VAL_LIBRARY_FD(lib), "RX_Init");
        if (RX_Init == NULL) {
            OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
            fail(Error_Bad_Extension_Raw(path));
        }

        // Call its RX_Init function for header and code body:
        if (cast(INIT_FUNC, RX_Init)(CTX_VAR(context, STD_EXTENSION_SCRIPT),
            CTX_VAR(context, STD_EXTENSION_MODULES)) < 0) {
            OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
            fail(Error_Extension_Init_Raw(path));
        }
    }
    else {
        assert(IS_HANDLE(ARG(path_or_handle)));
        REBVAL *handle = ARG(path_or_handle);
        if (VAL_HANDLE_CLEANER(handle) != cleanup_extension_init_handler)
            fail(Error_Bad_Extension_Raw(handle));

        INIT_FUNC RX_Init = cast(INIT_FUNC, VAL_HANDLE_CFUNC(handle));
        context = Copy_Context_Shallow(std_ext_ctx);
        if (
            RX_Init(
                CTX_VAR(context, STD_EXTENSION_SCRIPT),
                CTX_VAR(context, STD_EXTENSION_MODULES)
            ) < 0
        ){
            fail(Error_Extension_Init_Raw(handle));
        }
    }

    Init_Object(D_OUT, context);
    return R_OUT;
}


//
//  unload-extension-helper: native [
//
//  "Unload an extension"
//
//      return: [<opt>]
//      ext [object!]
//          "The extension to be unloaded"
//      /cleanup
//      cleaner [handle!]
//          "The RX_Quit pointer for the builtin extension"
//  ]
//
REBNATIVE(unload_extension_helper)
{
    INCLUDE_PARAMS_OF_UNLOAD_EXTENSION_HELPER;

    REBCTX *std = VAL_CONTEXT(Get_System(SYS_STANDARD, STD_EXTENSION));
    REBCTX *context = VAL_CONTEXT(ARG(ext));

    if (
        (CTX_LEN(context) <= STD_EXTENSION_LIB_BASE)
        || (
            CTX_KEY_CANON(context, STD_EXTENSION_LIB_BASE)
            != CTX_KEY_CANON(std, STD_EXTENSION_LIB_BASE)
        )
    ){
        fail (ARG(ext));
    }

    int ret;
    if (!REF(cleanup)) {
        REBVAL *lib = CTX_VAR(context, STD_EXTENSION_LIB_BASE);
        if (!IS_LIBRARY(lib))
            fail (ARG(ext));

        if (IS_LIB_CLOSED(VAL_LIBRARY(lib)))
            fail (Error_Bad_Library_Raw());

        QUIT_FUNC quitter = cast(
            QUIT_FUNC, OS_FIND_FUNCTION(VAL_LIBRARY_FD(lib), "RX_Quit")
        );

        if (quitter == NULL)
            ret = 0;
        else
            ret = quitter();

        OS_CLOSE_LIBRARY(VAL_LIBRARY_FD(lib));
    }
    else {
        if (VAL_HANDLE_CLEANER(ARG(cleaner)) != cleanup_extension_quit_handler)
            fail (ARG(cleaner));

        QUIT_FUNC quitter = cast(QUIT_FUNC, VAL_HANDLE_CFUNC(ARG(cleaner)));
        assert(quitter != NULL);

        ret = quitter();
    }

    if (ret < 0) {
        DECLARE_LOCAL (i);
        Init_Integer(i, ret);
        fail (Error_Fail_To_Quit_Extension_Raw(i));
    }

    return R_VOID;
}


//
// Just an ID for the handler
//
static void cleanup_module_handler(const REBVAL *val)
{
    UNUSED(val);
}


//
//  Make_Extension_Module_Array: C
//
// Make an extension module array for being loaded later
//
REBARR *Make_Extension_Module_Array(
    const REBYTE spec[],
    REBCNT len,
    REBNAT impl[],
    REBCNT n,
    REBCNT error_base
) {
    // the array will be like [spec C_func error_base/none]
    REBARR *arr = Make_Array(3);

    Init_Binary(ARR_AT(arr, 0), Copy_Bytes(spec, len));

    Init_Handle_Managed(
        ARR_AT(arr, 1),
        impl, // It's a *pointer to function pointer*, not a function pointer
        n,
        &cleanup_module_handler
    );

    if (error_base == 0)
        Init_Blank(ARR_AT(arr, 2));
    else
        Init_Integer(ARR_AT(arr, 2), error_base);

    TERM_ARRAY_LEN(arr, 3);
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
        Init_Handle_Managed_Cfunc(
            Alloc_Tail_Array(arr),
            funcs[i],
            0, // length, currently unused
            &cleanup_extension_init_handler
        );

        Init_Handle_Managed_Cfunc(
            Alloc_Tail_Array(arr),
            funcs[i + 1],
            0, // length, currently unused
            &cleanup_extension_quit_handler
        );
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
//          "function value, will be created from the native implementation"
//      spec [block!]
//          "spec of the native"
//      impl [handle!]
//          "a handle returned from RX_Init_ of the extension"
//      index [integer!]
//          "Index of the native"
//      /body
//      code [block!]
//          "User-equivalent body"
//      /unloadable
//          "The native can be unloaded later (when extension is unloaded)"
//  ]
//
REBNATIVE(load_native)
{
    INCLUDE_PARAMS_OF_LOAD_NATIVE;

    if (VAL_HANDLE_CLEANER(ARG(impl)) != cleanup_module_handler)
        fail ("HANDLE! passed to LOAD-NATIVE did not come from RX_Init");

    REBI64 index = VAL_INT64(ARG(index));
    if (index < 0 || cast(REBUPT, index) >= VAL_HANDLE_LEN(ARG(impl)))
        fail ("Index of native is outside range specified by RX_Init");

    REBNAT dispatcher = VAL_HANDLE_POINTER(REBNAT, ARG(impl))[index];
    REBFUN *fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(
            ARG(spec),
            MKF_KEYWORDS | MKF_FAKE_RETURN
        ),
        dispatcher, // unique
        NULL, // no facade (use paramlist)
        NULL // no specialization exemplar (or inherited exemplar)
    );

    if (REF(unloadable))
        SET_VAL_FLAG(FUNC_VALUE(fun), FUNC_FLAG_UNLOADABLE_NATIVE);

    if (REF(body)) {
        *FUNC_BODY(fun) = *ARG(code);
    }
    Move_Value(D_OUT, FUNC_VALUE(fun));
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
    UNUSED(f);

    fail (Error_Native_Unloaded_Raw(FUNC_VALUE(f->phase)));
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
        fail (Error_Non_Unloadable_Native_Raw(ARG(nat)));

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
        canons[i] = STR_CANON(Intern_UTF8_Managed(strings[i], LEN_BYTES(strings[i])));
    }
}


//
//  Hook_Datatype: C
//
// Poor-man's user-defined type hack: this really just gives the ability to
// have the only thing the core knows about a "user-defined-type" be its
// value cell structure and datatype enum number...but have the behaviors
// come from functions that are optionally registered in an extension.
//
// (Actual facets of user-defined types will ultimately be dispatched through
// Rebol-frame-interfaced functions, not raw C structures like this.)
//
void Hook_Datatype(
    enum Reb_Kind kind,
    REBACT act,
    REBPEF pef,
    REBCTF ctf,
    MAKE_FUNC make_func,
    TO_FUNC to_func,
    MOLD_FUNC mold_func
) {
    if (Value_Dispatch[kind] != &T_Unhooked)
        fail ("Value_Dispatch already hooked.");
    if (Path_Dispatch[kind] != &PD_Unhooked)
        fail ("Path_Dispatch already hooked.");
    if (Compare_Types[kind] != &CT_Unhooked)
        fail ("Compare_Types already hooked.");
    if (Make_Dispatch[kind] != &MAKE_Unhooked)
        fail ("Make_Dispatch already hooked.");
    if (To_Dispatch[kind] != &TO_Unhooked)
        fail ("To_Dispatch already hooked.");
    if (Mold_Or_Form_Dispatch[kind] != &MF_Unhooked)
        fail ("Mold_Or_Form_Dispatch already hooked.");

    Value_Dispatch[kind] = act;
    Path_Dispatch[kind] = pef;
    Compare_Types[kind] = ctf;
    Make_Dispatch[kind] = make_func;
    To_Dispatch[kind] = to_func;
    Mold_Or_Form_Dispatch[kind] = mold_func;
}


//
//  Unhook_Datatype: C
//
void Unhook_Datatype(enum Reb_Kind kind)
{
    if (Value_Dispatch[kind] == &T_Unhooked)
        fail ("Value_Dispatch is not hooked.");
    if (Path_Dispatch[kind] == &PD_Unhooked)
        fail ("Path_Dispatch is not hooked.");
    if (Compare_Types[kind] == &CT_Unhooked)
        fail ("Compare_Types is not hooked.");
    if (Make_Dispatch[kind] == &MAKE_Unhooked)
        fail ("Make_Dispatch is not hooked.");
    if (To_Dispatch[kind] == &TO_Unhooked)
        fail ("To_Dispatch is not hooked.");
    if (Mold_Or_Form_Dispatch[kind] == &MF_Unhooked)
        fail ("Mold_Or_Form_Dispatch is not hooked.");

    Value_Dispatch[kind] = &T_Unhooked;
    Path_Dispatch[kind] = &PD_Unhooked;
    Compare_Types[kind] = &CT_Unhooked;
    Make_Dispatch[kind] = &MAKE_Unhooked;
    To_Dispatch[kind] = &TO_Unhooked;
    Mold_Or_Form_Dispatch[kind] = &MF_Unhooked;
}
