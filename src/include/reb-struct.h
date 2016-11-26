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


//=////////////////////////////////////////////////////////////////////////=//
//
//  LIBRARY! (`struct Reb_Library`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A library represents a loaded .DLL or .so file.  This contains native
// code, which can be executed either through the "COMMAND" method (Rebol
// extensions) or the FFI interface.
//
// !!! The COMMAND method of extension is being deprecated by Ren-C, instead
// leaning on the idea of writing new natives using the same API that
// the system uses internally.
//

inline static void *LIB_FD(REBLIB *l) {
    return ARR_SERIES(l)->misc.fd; // file descriptor
}

inline static REBOOL IS_LIB_CLOSED(const REBLIB *l) {
    return LOGICAL(ARR_SERIES(l)->misc.fd == NULL);
}

inline static REBCTX *VAL_LIBRARY_META(const RELVAL *v) {
    return ARR_SERIES(v->payload.library.singular)->link.meta;
}

inline static REBLIB *VAL_LIBRARY(const RELVAL *v) {
    return v->payload.library.singular;
}

inline static void *VAL_LIBRARY_FD(const RELVAL *v) {
    return LIB_FD(VAL_LIBRARY(v));
}



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


//=////////////////////////////////////////////////////////////////////////=//
//
// What used to be a specialized C structure for representing a FFI field
// has been changed to an ordinary Rebol array.  This makes it so it does not
// require modifications to the garbage collector.
//
// [0] - A WORD! name for the field (or BLANK! if anonymous ?)
//
// [1] - INTEGER! type, e.g. FFI_TYPE_XXX constants, or a BLOCK! of fields
//       if this is a struct.
//
// [2] - An INTEGER! of the array dimensionality, or BLANK! if not an array
//
// [3] - HANDLE! to the ffi_type* representing this entire field.  This will
//       appear other places, so it uses the shared form of HANDLE! which
//       will GC the memory pointed to when the last reference goes away.
//
//       !!! This is a cache, in a sense that it is fully produced from
//       the rest of the information.  It was sometimes set to NULL in the
//       previous implementation and then calculated and cached, so the
//       BLANK! state is used for that.  Is this state really necessary, or
//       can it be guaranteed there's always a handle here?
//
// [4] - An INTEGER! of the offset this field is relative to the beginning
//       of its entire containing structure.  Will be BLANK! if the structure
//       is actually the root structure itself.
//
//       !!! Comment said "size is limited by struct->offset, so only 16-bit"
//
// [5] - An INTEGER! size of an individual field element ("wide"), in bytes.
//
// !!! Previously, there was a bit tracking whether the field represented a
// REBVAL or not.  The actual FFI type was an array of FFI_TYPE_POINTER of
// length 4, hence carrying the literal bit pattern of a value, in a place
// that would be needed to be protected from GC.  Ren-C will not do this,
// instead if a value is to be passed around it should be done by pointer
// and protected from GC by something else.  (RL_API routines will be using
// REBVAL* pointers into series nodes that have GC protection for the length
// of their function call...and internal api clients do explicit handling.)
// 

typedef REBARR REBFLD;

#define FLD_AT(a, n) ARR_AT((a), (n)) // help locate indexed accesses

inline static REBSTR *FLD_NAME(REBFLD *f) {
    if (IS_BLANK(FLD_AT(f, 0)))
        return NULL;
    return VAL_WORD_SPELLING(FLD_AT(f, 0));
}

inline static unsigned short FLD_CTYPE(REBFLD *f) {
    if (IS_BLOCK(FLD_AT(f, 1)))
        return FFI_TYPE_STRUCT;
    return VAL_INT32(FLD_AT(f, 1));
}

inline static REBOOL FLD_IS_STRUCT(REBFLD *f)
    { return IS_BLOCK(FLD_AT(f, 1)); }

inline static REBARR *FLD_FIELDLIST(REBFLD *f) {
    assert(FLD_IS_STRUCT(f));
    return VAL_ARRAY(FLD_AT(f, 1));
}

inline static REBOOL FLD_IS_ARRAY(REBFLD *f) {
    if (IS_BLANK(FLD_AT(f, 2)))
        return FALSE;
    assert(IS_INTEGER(FLD_AT(f, 2)));
    return TRUE;
}

inline static REBCNT FLD_DIMENSION(REBFLD *f) {
    assert(FLD_IS_ARRAY(f));
    return VAL_INT32(FLD_AT(f, 2));
}

inline static ffi_type *FLD_FFTYPE(REBFLD *f)
    { return cast(ffi_type*, VAL_HANDLE_DATA(FLD_AT(f, 3))); }

inline static REBCNT FLD_OFFSET(REBFLD *f)
    { return VAL_INT32(FLD_AT(f, 4)); }

inline static REBCNT FLD_WIDE(REBFLD *f)
    { return VAL_INT32(FLD_AT(f, 5)); }

inline static REBCNT FLD_LEN_BYTES_TOTAL(REBFLD *f) {
    if (FLD_IS_ARRAY(f))
        return FLD_WIDE(f) * FLD_DIMENSION(f);
    return FLD_WIDE(f);
}


#define VAL_STRUCT_LIMIT MAX_U32


//=////////////////////////////////////////////////////////////////////////=//
//
//  STRUCT! (`struct Reb_Struct`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Struct is used by the FFI code to describe the layout of a C `struct`,
// so that Rebol data can be proxied to a C function call.
//
// !!! Atronix added the struct type to get coverage in the FFI, and it is
// possible to make and extract structure data even if one is not calling
// a routine at all.  This might not be necessary, in that the struct
// description could be an ordinary OBJECT! which is used in FFI specs
// and tells how to map data into a Rebol object used as a target.
//

inline static REBVAL *STU_VALUE(REBSTU *stu) {
    assert(ARR_LEN(stu) == 1);
    return KNOWN(ARR_HEAD(stu));
}

#define STU_INACCESSIBLE(stu) \
    VAL_STRUCT_INACCESSIBLE(STU_VALUE(stu))

inline static REBFLD *STU_SCHEMA(REBSTU *stu) {
    REBFLD *schema = ARR_SERIES(stu)->link.schema;
    assert(FLD_IS_STRUCT(schema));
    return schema;
}

inline static REBARR *STU_FIELDLIST(REBSTU *stu) {
    return FLD_FIELDLIST(STU_SCHEMA(stu));
}

inline static REBCNT STU_SIZE(REBSTU *stu) {
    return FLD_WIDE(STU_SCHEMA(stu));
}

inline static REBSER *STU_DATA_BIN(REBSTU *stu) {
    return STU_VALUE(stu)->payload.structure.data;
}

inline static REBCNT STU_OFFSET(REBSTU *stu) {
    return STU_VALUE(stu)->extra.struct_offset;
}

#define STU_FFTYPE(stu) \
    FLD_FFTYPE(STU_SCHEMA(stu))

#define VAL_STRUCT(v) \
    ((v)->payload.structure.stu)

#define VAL_STRUCT_SCHEMA(v) \
    STU_SCHEMA(VAL_STRUCT(v))

#define VAL_STRUCT_SIZE(v) \
    STU_SIZE(VAL_STRUCT(v))

#define VAL_STRUCT_DATA_BIN(v) \
    ((v)->payload.structure.data)

inline static REBOOL VAL_STRUCT_INACCESSIBLE(const RELVAL *v) {
    REBSER *bin = VAL_STRUCT_DATA_BIN(v);
    if (GET_SER_FLAG(bin, SERIES_FLAG_INACCESSIBLE)) {
        assert(GET_SER_FLAG(bin, SERIES_FLAG_EXTERNAL));
        return TRUE;
    }
    return FALSE;
}

#define VAL_STRUCT_OFFSET(v) \
    ((v)->extra.struct_offset)

#define VAL_STRUCT_FIELDLIST(v) \
    STU_FIELDLIST(VAL_STRUCT(v))

#define VAL_STRUCT_FFTYPE(v) \
    STU_FFTYPE(VAL_STRUCT(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  ROUTINE SUPPORT
//
//=////////////////////////////////////////////////////////////////////////=//
//
// "Routine info" used to be a specialized C structure, which referenced
// Rebol functions/values/series.  This meant there had to be specialized
// code in the garbage collector.  It actually went as far as to have a memory
// pool for objects that was sizeof(Reb_Routine_Info), which complicates the
// concerns further.
//
// That "invasive" approach is being gradually generalized to speak in the
// natural vocabulary of Rebol values.  What enables the transition is that
// arbitrary C allocations (such as an ffi_closure*) can use the new freeing
// handler feature of a GC'd HANDLE! value.  So now "routine info" is just
// a BLOCK! REBVAL*, which lives in the FUNC_BODY of a routine, and has some
// HANDLE!s in it that array.
//
// !!! An additional benefit is that if the structures used internally
// are actual Rebol-manipulatable values, then that means more parts of the
// FFI extension itself could be written as Rebol.  e.g. the FFI spec analysis
// could be done with PARSE, as opposed to harder-to-edit-and-maintain
// internal API C code.
//
// The layout of the array of the REBRIN* BLOCK! is as follows:
//
// [0] - The HANDLE! of a CFUNC*, obeying the interface of the C-format call.
//       If it's a routine, then it's the pointer to a pre-existing function
//       in the DLL that the routine intends to wrap.  If a callback, then
//       it's a fabricated function pointer returned by ffi_closure_alloc,
//       which presents the "thunk"...a C function that other C functions can
//       call which will then delegate to Rebol to call the wrapped FUNCTION!.
//
//       Additionally, callbacks poke a data pointer into the HANDLE! with
//       ffi_closure*.  (The closure allocation routine gives back a void* and
//       not an ffi_closure* for some reason.  Perhaps because it takes a
//       size that might be bigger than the size of a closure?)
//
// [1] - An INTEGER! indicating which ABI is used by the CFUNC (enum ffi_abi)

// [2] - The LIBRARY! the CFUNC* lives in if a routine, or the FUNCTION! to
//       be called if this is a callback.
//
// [3] - The "schema" of the return type.  This is either an INTEGER! (which
//       is the FFI_TYPE constant of the return) or a BINARY! containing
//       the bit pattern of a `Struct_Field` (it's held in a series to allow
//       it to be referenced multiple places and participate in GC, though
//       the ultimate goal is to just use OBJECT! here.)
//
// [4] - An ARRAY! of the argument schemas; each also INTEGER! or BINARY!,
//       following the same pattern as the return value.
//
// [5] - A HANDLE! containing one ffi_cif*, or BLANK! if variadic.  The Call
//       InterFace (CIF) for a C function with fixed arguments can be created
//       once and then used many times.  For a variadic routine, it must be
//       created on each call to match the number and types of arguments.
//
// [6] - A HANDLE! which is actually an array of ffi_type*, so a C array of
//       pointers.  They refer into the CIF so the CIF must live as long
//       as these references are to be used.  BLANK! if variadic.
//
// [7] - A LOGIC! of whether this routine is variadic.  Since variadic-ness is
//       something that gets exposed in the FUNCTION! interface itself, this
//       may become redundant as an internal property of the implementation.
//

#define RIN_AT(a, n) ARR_AT((a), (n)) // help locate indexed accesses

inline static CFUNC *RIN_CFUNC(REBRIN *r)
    { return VAL_HANDLE_CODE(RIN_AT(r, 0)); }

inline static ffi_abi RIN_ABI(REBRIN *r)
    { return cast(ffi_abi, VAL_INT32(RIN_AT(r, 1))); }

inline static REBOOL RIN_IS_CALLBACK(REBRIN *r) {
    if (IS_FUNCTION(RIN_AT(r, 2)))
        return TRUE;
    assert(IS_LIBRARY(RIN_AT(r, 2)) || IS_BLANK(RIN_AT(r, 2)));
    return FALSE;
}

inline static ffi_closure* RIN_CLOSURE(REBRIN *r) {
    assert(RIN_IS_CALLBACK(r)); // only callbacks have ffi_closure
    return cast(ffi_closure*, VAL_HANDLE_DATA(RIN_AT(r, 0)));
}

inline static REBLIB *RIN_LIB(REBRIN *r) {
    assert(NOT(RIN_IS_CALLBACK(r)));
    return VAL_LIBRARY(RIN_AT(r, 2));
}

inline static REBFUN *RIN_CALLBACK_FUNC(REBRIN *r) {
    assert(RIN_IS_CALLBACK(r));
    return VAL_FUNC(RIN_AT(r, 2));
}

inline static REBVAL *RIN_RET_SCHEMA(REBRIN *r)
    { return KNOWN(RIN_AT(r, 3)); }

inline static REBCNT RIN_NUM_FIXED_ARGS(REBRIN *r)
    { return VAL_LEN_HEAD(RIN_AT(r, 4)); }

inline static REBVAL *RIN_ARG_SCHEMA(REBRIN *r, REBCNT n) // 0-based arg index
    { return KNOWN(VAL_ARRAY_AT_HEAD(RIN_AT(r, 4), (n))); }

inline static ffi_cif *RIN_CIF(REBRIN *r)
    { return cast(ffi_cif*, VAL_HANDLE_DATA(RIN_AT(r, 5))); }

inline static ffi_type** RIN_ARG_TYPES(REBRIN *r)
    { return cast(ffi_type**, VAL_HANDLE_DATA(RIN_AT(r, 6))); }

inline static REBOOL RIN_IS_VARIADIC(REBRIN *r)
    { return VAL_LOGIC(RIN_AT(r, 7)); }


#define Get_FFType_Enum_Info(sym_out,kind_out,type) \
    cast(ffi_type*, Get_FFType_Enum_Info_Core((sym_out), (kind_out), (type)))

inline static void* SCHEMA_FFTYPE_CORE(const RELVAL *schema) {
    if (IS_BLOCK(schema)) {
        REBFLD *field = VAL_ARRAY(schema);
        Prepare_Field_For_FFI(field);
        return FLD_FFTYPE(field);
    }

    // Avoid creating a "VOID" type in order to not give the illusion of
    // void parameters being legal.  The NONE! return type is handled
    // exclusively by the return value, to prevent potential mixups.
    //
    assert(IS_INTEGER(schema));

    enum Reb_Kind kind; // dummy
    REBSTR *name; // dummy
    return Get_FFType_Enum_Info(&name, &kind, VAL_INT32(schema));
}

#define SCHEMA_FFTYPE(schema) \
    cast(ffi_type*, SCHEMA_FFTYPE_CORE(schema))
