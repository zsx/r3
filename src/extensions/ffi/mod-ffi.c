//
//  File: %mod-ffi.c
//  Summary: "Foreign function interface main C file"
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

#include "sys-core.h"
#include "sys-ext.h"

#include "tmp-mod-ffi-first.h"

#include "reb-struct.h"

// There is a platform-dependent list of legal ABIs which the MAKE-ROUTINE
// and MAKE-CALLBACK natives take as an option via refinement
//
static ffi_abi Abi_From_Word(const REBVAL *word) {
    switch (VAL_WORD_SYM(word)) {
    case SYM_DEFAULT:
        return FFI_DEFAULT_ABI;

#ifdef X86_WIN64
    case SYM_WIN64:
        return FFI_WIN64;

#elif defined(X86_WIN32) || defined(TO_LINUX_X86) || defined(TO_LINUX_X64)
    case SYM_STDCALL:
        return FFI_STDCALL;

    case SYM_SYSV:
        return FFI_SYSV;

    case SYM_THISCALL:
        return FFI_THISCALL;

    case SYM_FASTCALL:
        return FFI_FASTCALL;

#ifdef X86_WIN32
    case SYM_MS_CDECL:
        return FFI_MS_CDECL;
#else
    case SYM_UNIX64:
        return FFI_UNIX64;
#endif //X86_WIN32

#elif defined (TO_LINUX_ARM)
    case SYM_VFP:
        return FFI_VFP;

    case SYM_SYSV:
        return FFI_SYSV;

#elif defined (TO_LINUX_MIPS)
    case SYM_O32:
        return FFI_O32;

    case SYM_N32:
        return FFI_N32;

    case SYM_N64:
        return FFI_N64;

    case SYM_O32_SOFT_FLOAT:
        return FFI_O32_SOFT_FLOAT;

    case SYM_N32_SOFT_FLOAT:
        return FFI_N32_SOFT_FLOAT;

    case SYM_N64_SOFT_FLOAT:
        return FFI_N64_SOFT_FLOAT;
#endif //X86_WIN64

    default:
        break;
    }

    fail (word);
}


//
//  make-routine: native/export [
//
//  {Create a bridge for interfacing with arbitrary C code in a DLL}
//
//      return: [function!]
//      lib [library!]
//          {Library DLL that function lives in (get with MAKE LIBRARY!)}
//      name [string!]
//          {Linker name of the function in the DLL}
//      ffi-spec [block!]
//          {Description of what C argument types the function takes}
//      /abi
//          {Specify the Application Binary Interface (vs. using default)}
//      abi-type [word!]
//          {'CDECL, 'FASTCALL, 'STDCALL, etc.}
//  ]
//
REBNATIVE(make_routine)
//
// !!! Would be nice if this could just take a filename and the lib management
// was automatic, e.g. no LIBRARY! type.
{
    INCLUDE_PARAMS_OF_MAKE_ROUTINE;

    ffi_abi abi;
    if (REF(abi))
        abi = Abi_From_Word(ARG(abi_type));
    else
        abi = FFI_DEFAULT_ABI;

    // Make sure library wasn't closed with CLOSE
    //
    REBLIB *lib = VAL_LIBRARY(ARG(lib));
    if (lib == NULL)
        fail (ARG(lib));

    // Try to find the C function pointer in the DLL, if it's there.
    // OS_FIND_FUNCTION takes a char* on both Windows and Posix.  The
    // string that gets here could be REBUNI wide or BYTE_SIZE(), so
    // make sure it's turned into a char* before passing.
    //
    // !!! Should it error if any bytes need to be UTF8 encoded?
    //
    REBVAL *name = ARG(name);
    REBCNT b_index = VAL_INDEX(name);
    REBCNT b_len = VAL_LEN_AT(name);
    REBSER *byte_sized = Temp_Bin_Str_Managed(name, &b_index, &b_len);

    CFUNC *cfunc = OS_FIND_FUNCTION(
        LIB_FD(lib),
        SER_AT(char, byte_sized, b_index) // name may not be at head index
    );
    if (cfunc == NULL)
        fail ("FFI: Couldn't find function in library");

    // Process the parameter types into a function, then fill it in

    REBFUN *fun = Alloc_Ffi_Function_For_Spec(ARG(ffi_spec), abi);
    REBRIN *r = FUNC_ROUTINE(fun);

    Init_Handle_Cfunc(RIN_AT(r, IDX_ROUTINE_CFUNC), cfunc, 0);
    Move_Value(RIN_AT(r, IDX_ROUTINE_ORIGIN), ARG(lib));

    Move_Value(D_OUT, FUNC_VALUE(fun));
    return R_OUT;
}


//
//  make-routine-raw: native/export [
//
//  {Create a bridge for interfacing with a C function, by pointer}
//
//      return: [function!]
//      pointer [integer!]
//          {Raw address of function in memory}
//      ffi-spec [block!]
//          {Description of what C argument types the function takes}
//      /abi
//          {Specify the Application Binary Interface (vs. using default)}
//      abi-type [word!]
//          {'CDECL, 'FASTCALL, 'STDCALL, etc.}
//  ]
//
REBNATIVE(make_routine_raw)
//
// !!! Would be nice if this could just take a filename and the lib management
// was automatic, e.g. no LIBRARY! type.
{
    INCLUDE_PARAMS_OF_MAKE_ROUTINE_RAW;

    ffi_abi abi;
    if (REF(abi))
        abi = Abi_From_Word(ARG(abi_type));
    else
        abi = FFI_DEFAULT_ABI;

    // Cannot cast directly to a function pointer from a 64-bit value
    // on 32-bit systems; first cast to (U)nsigned int that holds (P)oin(T)er
    //
    CFUNC *cfunc = cast(CFUNC*, cast(REBUPT, VAL_INT64(ARG(pointer))));
    if (cfunc == NULL)
        fail ("FFI: NULL pointer not allowed for raw MAKE-ROUTINE");

    REBFUN *fun = Alloc_Ffi_Function_For_Spec(ARG(ffi_spec), abi);
    REBRIN *r = FUNC_ROUTINE(fun);

    Init_Handle_Cfunc(RIN_AT(r, IDX_ROUTINE_CFUNC), cfunc, 0);
    Init_Blank(RIN_AT(r, IDX_ROUTINE_ORIGIN)); // no LIBRARY! in this case.

    Move_Value(D_OUT, FUNC_VALUE(fun));
    return R_OUT;
}


//
//  wrap-callback: native/export [
//
//  {Wrap function so it can be called by raw C code via a memory address.}
//
//      return: [function!]
//      action [function!]
//          {The existing Rebol function whose behavior is being wrapped}
//      ffi-spec [block!]
//          {Description of what C types each Rebol argument should map to}
//      /abi
//          {Specify the Application Binary Interface (vs. using default)}
//      abi-type [word!]
//          {'CDECL, 'FASTCALL, 'STDCALL, etc.}
//  ]
//
REBNATIVE(wrap_callback)
{
    INCLUDE_PARAMS_OF_WRAP_CALLBACK;

    ffi_abi abi;
    if (REF(abi))
        abi = Abi_From_Word(ARG(abi_type));
    else
        abi = FFI_DEFAULT_ABI;

    REBFUN *fun = Alloc_Ffi_Function_For_Spec(ARG(ffi_spec), abi);
    REBRIN *r = FUNC_ROUTINE(fun);

    void *thunk; // actually CFUNC (FFI uses void*, may not be same size!)
    ffi_closure *closure = cast(ffi_closure*, ffi_closure_alloc(
        sizeof(ffi_closure), &thunk
    ));

    if (closure == NULL)
        fail ("FFI: Couldn't allocate closure");

    ffi_status status = ffi_prep_closure_loc(
        closure,
        RIN_CIF(r),
        callback_dispatcher, // when thunk is called it calls this function...
        r, // ...and this piece of data is passed to callback_dispatcher
        thunk
    );

    if (status != FFI_OK)
        fail ("FFI: Couldn't prep closure");

    if (sizeof(void*) != sizeof(CFUNC*))
        fail ("FFI does not work when void* size differs from CFUNC* size");

    // It's the FFI's fault for using the wrong type for the thunk.  Use a
    // memcpy in order to get around strict checks that absolutely refuse to
    // let you do a cast here.
    //
    CFUNC *cfunc_thunk;
    memcpy(&cfunc_thunk, &thunk, sizeof(cfunc_thunk));

    Init_Handle_Cfunc(RIN_AT(r, IDX_ROUTINE_CFUNC), cfunc_thunk, 0);
    Init_Handle_Managed(
        RIN_AT(r, IDX_ROUTINE_CLOSURE),
        closure,
        0,
        &cleanup_ffi_closure
    );
    Move_Value(RIN_AT(r, IDX_ROUTINE_ORIGIN), ARG(action));

    Move_Value(D_OUT, FUNC_VALUE(fun));
    return R_OUT;
}


//
//  addr-of: native/export [
//
//  {Get the memory address of an FFI STRUCT! or routine/callback}
//
//      return: [integer!]
//          {Memory address expressed as an up-to-64-bit integer}
//      value [function! struct!]
//          {Fixed address structure or routine to get the address of}
//  ]
//
REBNATIVE(addr_of) {
    INCLUDE_PARAMS_OF_ADDR_OF;

    REBVAL *v = ARG(value);

    if (IS_FUNCTION(v)) {
        if (NOT(IS_FUNCTION_RIN(v)))
            fail ("Can only take address of FUNCTION!s created though FFI");

        // The CFUNC is fabricated by the FFI if it's a callback, or
        // just the wrapped DLL function if it's an ordinary routine
        //
        Init_Integer(
            D_OUT, cast(REBUPT, RIN_CFUNC(VAL_FUNC_ROUTINE(v)))
        );
        return R_OUT;
    }

    assert(IS_STRUCT(v));

    // !!! If a structure wasn't mapped onto "raw-memory" from the C,
    // then currently the data for that struct is a BINARY!, not a handle to
    // something which was malloc'd.  Much of the system is designed to be
    // able to handle memory relocations of a series data, but if a pointer is
    // given to code it may expect that address to be permanent.  Data
    // pointers currently do not move (e.g. no GC compaction) unless there is
    // a modification to the series, but this may change...in which case a
    // "do not move in memory" bit would be needed for the BINARY! or a
    // HANDLE! to a non-moving malloc would need to be used instead.
    //
    Init_Integer(D_OUT, cast(REBUPT, VAL_STRUCT_DATA_AT(v)));
    return R_OUT;
}


//
//  make-similar-struct: native/export [
//
//  "Create a STRUCT! that reuses the underlying spec of another STRUCT!"
//
//      return: [struct!]
//      spec [struct!]
//          "Struct with interface to copy"
//      body [block! any-context! blank!]
//          "keys and values defining instance contents (bindings modified)"
//  ]
//
REBNATIVE(make_similar_struct)
//
// !!! Compatibility for `MAKE some-struct [...]` from Atronix R3.  There
// isn't any real "inheritance management" for structs, but it allows the
// re-use of the structure's field definitions, so it is a means of saving on
// memory (?)  Code retained for examination.
{
    INCLUDE_PARAMS_OF_MAKE_SIMILAR_STRUCT;

    REBVAL *spec = ARG(spec);
    REBVAL *body = ARG(body);

    REBSTU *stu = Copy_Struct_Managed(VAL_STRUCT(spec));

    Move_Value(D_OUT, STU_VALUE(stu));

    // !!! Comment said "only accept value initialization"
    //
    Init_Struct_Fields(D_OUT, body);
    return R_OUT;
}


//
//  destroy-struct-storage: native [
//
//  {Destroy the external memory associated the struct}
//
//      struct [struct!]
//      /free
//          {Specify the function to free the memory}
//      free-func [function!]
//  ]
//
REBNATIVE(destroy_struct_storage)
{
    INCLUDE_PARAMS_OF_DESTROY_STRUCT_STORAGE;

    REBSER *data = ARG(struct)->payload.structure.data;
    if (NOT_SER_FLAG(data, SERIES_FLAG_ARRAY))
        fail (Error_No_External_Storage_Raw());

    RELVAL *handle = ARR_HEAD(ARR(data));

    DECLARE_LOCAL (pointer);
    Init_Integer(pointer, cast(REBUPT, VAL_HANDLE_POINTER(void, handle)));

    if (VAL_HANDLE_LEN(handle) == 0)
        fail (Error_Already_Destroyed_Raw(pointer));

    // TBD: assert handle length was correct for memory block size

    SET_HANDLE_LEN(handle, 0);

    if (REF(free)) {
        if (NOT(IS_FUNCTION_RIN(ARG(free_func))))
            fail (Error_Free_Needs_Routine_Raw());

        if (Do_Va_Throws(D_OUT, ARG(free_func), pointer, END))
            return R_OUT_IS_THROWN;
    }

    return R_VOID;
}


#include "tmp-mod-ffi-last.h"
