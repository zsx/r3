//
//  File: %reb-struct.h
//  Summary: "Struct to C function"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2014 Atronix Engineering, Inc.
// Copyright 2014-2016 Rebol Open Source Contributors
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


#ifdef HAVE_LIBFFI_AVAILABLE
    #include <ffi.h>
#else
    // Non-functional stubs, see notes at top of t-routine.c

    typedef struct _ffi_type
    {
        size_t size;
        unsigned short alignment;
        unsigned short type;
        struct _ffi_type **elements;
    } ffi_type;

    #define FFI_TYPE_VOID       0
    #define FFI_TYPE_INT        1
    #define FFI_TYPE_FLOAT      2
    #define FFI_TYPE_DOUBLE     3
    #define FFI_TYPE_LONGDOUBLE 4
    #define FFI_TYPE_UINT8      5
    #define FFI_TYPE_SINT8      6
    #define FFI_TYPE_UINT16     7
    #define FFI_TYPE_SINT16     8
    #define FFI_TYPE_UINT32     9
    #define FFI_TYPE_SINT32     10
    #define FFI_TYPE_UINT64     11
    #define FFI_TYPE_SINT64     12
    #define FFI_TYPE_STRUCT     13
    #define FFI_TYPE_POINTER    14
    #define FFI_TYPE_COMPLEX    15

    // !!! Heads-up to FFI lib authors: these aren't const definitions.  :-/
    // Stray modifications could ruin these "constants".  Being const-correct
    // in the parameter structs for the type arrays would have been nice...

    extern ffi_type ffi_type_void;
    extern ffi_type ffi_type_uint8;
    extern ffi_type ffi_type_sint8;
    extern ffi_type ffi_type_uint16;
    extern ffi_type ffi_type_sint16;
    extern ffi_type ffi_type_uint32;
    extern ffi_type ffi_type_sint32;
    extern ffi_type ffi_type_uint64;
    extern ffi_type ffi_type_sint64;
    extern ffi_type ffi_type_float;
    extern ffi_type ffi_type_double;
    extern ffi_type ffi_type_pointer;

    // Switched from an enum to allow Panic w/o complaint
    typedef int ffi_status;
    #define FFI_OK 0
    #define FFI_BAD_TYPEDEF 1
    #define FFI_BAD_ABI 2

    typedef enum ffi_abi
    {
        // !!! The real ffi_abi constants will be different per-platform,
        // you would not have the full list.  Interestingly, a subsetting
        // script *might* choose to alter libffi to produce a larger list
        // vs being full of #ifdefs (though that's rather invasive change
        // to the libffi code to be maintaining!)

        FFI_FIRST_ABI = 0x0BAD,
        FFI_WIN64,
        FFI_STDCALL,
        FFI_SYSV,
        FFI_THISCALL,
        FFI_FASTCALL,
        FFI_MS_CDECL,
        FFI_UNIX64,
        FFI_VFP,
        FFI_O32,
        FFI_N32,
        FFI_N64,
        FFI_O32_SOFT_FLOAT,
        FFI_N32_SOFT_FLOAT,
        FFI_N64_SOFT_FLOAT,
        FFI_LAST_ABI,
        FFI_DEFAULT_ABI = FFI_FIRST_ABI
    } ffi_abi;

    typedef struct {
        ffi_abi abi;
        unsigned nargs;
        ffi_type **arg_types;
        ffi_type *rtype;
        unsigned bytes;
        unsigned flags;
    } ffi_cif;

    // The closure is a "black box" but client code takes the sizeof() to
    // pass into the alloc routine...

    typedef struct {
        int stub;
    } ffi_closure;

#endif // HAVE_LIBFFI_AVAILABLE


enum {
    STRUCT_TYPE_UINT8 = 0,
    STRUCT_TYPE_INT8,
    STRUCT_TYPE_UINT16,
    STRUCT_TYPE_INT16,
    STRUCT_TYPE_UINT32,
    STRUCT_TYPE_INT32,
    STRUCT_TYPE_UINT64,
    STRUCT_TYPE_INT64,
    STRUCT_TYPE_INTEGER,

    STRUCT_TYPE_FLOAT,
    STRUCT_TYPE_DOUBLE,
    STRUCT_TYPE_DECIMAL,

    STRUCT_TYPE_POINTER,
    STRUCT_TYPE_STRUCT,
    STRUCT_TYPE_REBVAL,
    STRUCT_TYPE_MAX
};

struct Struct_Field {
    REBARR* spec; /* for nested struct */
    REBSER* fields; /* for nested struct */
    REBSYM sym;

    REBINT type; /* rebol type */

    REBSER *fftype_ser;
    REBSER *fields_fftypes_ser;

    /* size is limited by struct->offset, so only 16-bit */
    REBCNT offset;
    REBCNT dimension; /* for arrays */
    REBCNT size; /* size of element, in bytes */

    /* Note: C89 bitfields may be 'int', 'unsigned int', or 'signed int' */
    unsigned int is_array:1;
    /* field is initialized? */
    /* (used by GC to decide if the value needs to be marked) */
    unsigned int done:1;
};


#define VAL_STRUCT_LIMIT MAX_U32
