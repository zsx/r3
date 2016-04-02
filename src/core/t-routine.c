//
//  File: %t-routine.c
//  Summary: "External Routine Support"
//  Section: datatypes
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
// When Rebol3 was open-sourced in 12-Dec-2012, that version had lost
// support for the ROUTINE! type from Rebol2.  It was later
// reimplemented by Atronix in their fork via the cross-platform (and
// popularly used) Foreign Function Interface library "libffi":
//
//     https://en.wikipedia.org/wiki/Libffi
//
// Yet Rebol is very conservative about library dependencies that
// introduce their "own build step", due to the complexity introduced.
// If one is to build libffi for a particular platform, that requires
// having the rather messy GNU autotools installed.  Notice the
// `Makefile.am`, `acinclude.m4`, `autogen.sh`, `configure.ac`,
// `configure.host`, etc:
//
//     https://github.com/atgreen/libffi
//
// Suddenly, you need more than just a C compiler (and a rebol.exe) to
// build Rebol.  You now need to have everything to configure and
// build libffi.  -OR- it would mean a dependency on a built library
// you had to find or get somewhere that was not part of the OS
// naturally, which can be a wild goose chase with version
// incompatibility.  If you `sudo apt-get libffi`, now you need apt-get
// *and* you pull down any dependencies as well!
//
// (Note: Rebol's "just say no" attitude is the heart of the Rebellion:
//
//     http://www.rebol.com/cgi-bin/blog.r?view=0497
//
// ...so keeping the core true to this principle is critical.  If this
// principle is compromised, the whole point of the project is lost.)
//
// Yet Rebol2 had ROUTINE!.  Red also has ROUTINE!, and is hinging its
// story for rapid interoperability on it (you should not have to
// wrap and recompile a DLL of C functions just to call them).  Users
// want the feature and always ask...and Atronix needs it enough to have
// had @ShixinZeng write it!
//
// Regarding the choice of libffi in particular, it's a strong sign to
// notice how many other language projects are using it.  Short list
// taken from 2015 Wikipedia:
//
//     Python, Haskell, Dalvik, F-Script, PyPy, PyObjC, RubyCocoa,
//     JRuby, Rubinius, MacRuby, gcj, GNU Smalltalk, IcedTea, Cycript,
//     Pawn, Squeak, Java Native Access, Common Lisp, Racket,
//     Embeddable Common Lisp and Mozilla.
//
// Rebol could roll its own implementation.  But that takes time and
// maintenance, and it's hard to imagine how much better a job could
// be done for a C-based foreign function interface on these platforms;
// it's light and quite small once built.  So it makes sense to
// "extract" libffi's code out of its repo to form one .h and .c file.
// They'd live in the Rebol sources and build with the existing process,
// with no need for GNU Autotools (which are *particularly* crufty!!!)
//
// Doing such extractions by hand is how Rebol was originally done;
// that made it hard to merge updates.  As a more future-proof method,
// @HostileFork wrote a make-zlib.r extractor that can take a copy of
// the zlib repository and do the work (mostly) automatically.  Going
// forward it seems prudent to do the same with libffi and any other
// libraries that Rebol co-opts into its turnkey build process.
//
// Until that happens for libffi, not definining HAVE_LIBFFI_AVAILABLE,
// will give you a short list of non-functional "stubs".  These can
// allow t-routine.c to compile anyway.  That assists with maintenance
// of the code and keeping it on the radar, even among those doing core
// maintenance who are not building against the FFI.
//
// (Note: Longer term there may be a story by which a feature like
// ROUTINE! could be implemented as a third party extension.  There is
// short-term thinking trying to facilitate this for GOB! in Ren/C, to
// try and open the doors to more type extensions.  That's a hard
// problem in itself...and the needs of ROUTINE! are hooked a bit more
// tightly into the evaluation loop.  So possibly not happening.)
//

#include "sys-core.h"

#include "mem-pools.h" // low-level memory pool access

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

    ffi_type ffi_type_void = { 0, 0, FFI_TYPE_VOID, NULL };
    ffi_type ffi_type_uint8 = { 0, 0, FFI_TYPE_UINT8, NULL };
    ffi_type ffi_type_sint8 = { 0, 0, FFI_TYPE_SINT8, NULL };
    ffi_type ffi_type_uint16 = { 0, 0, FFI_TYPE_UINT16, NULL };
    ffi_type ffi_type_sint16 = { 0, 0, FFI_TYPE_SINT16, NULL };
    ffi_type ffi_type_uint32 = { 0, 0, FFI_TYPE_UINT32, NULL };
    ffi_type ffi_type_sint32 = { 0, 0, FFI_TYPE_SINT32, NULL };
    ffi_type ffi_type_uint64 = { 0, 0, FFI_TYPE_UINT64, NULL };
    ffi_type ffi_type_sint64 = { 0, 0, FFI_TYPE_SINT64, NULL };
    ffi_type ffi_type_float = { 0, 0, FFI_TYPE_FLOAT, NULL };
    ffi_type ffi_type_double = { 0, 0, FFI_TYPE_DOUBLE, NULL };
    ffi_type ffi_type_pointer = { 0, 0, FFI_TYPE_POINTER, NULL };

    // Switched from an enum to allow Panic w/o complaint
    typedef int ffi_status;
    const int FFI_OK = 0;
    const int FFI_BAD_TYPEDEF = 1;
    const int FFI_BAD_ABI = 2;

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

    ffi_status ffi_prep_cif(
        ffi_cif *cif,
        ffi_abi abi,
        unsigned int nargs,
        ffi_type *rtype,
        ffi_type **atypes
    ) {
        fail (Error(RE_NOT_FFI_BUILD));
    }

    ffi_status ffi_prep_cif_var(
        ffi_cif *cif,
        ffi_abi abi,
        unsigned int nfixedargs,
        unsigned int ntotalargs,
        ffi_type *rtype,
        ffi_type **atypes
    ) {
        fail (Error(RE_NOT_FFI_BUILD));
    }

    void ffi_call(
        ffi_cif *cif,
        void (*fn)(void),
        void *rvalue,
        void **avalue
    ) {
        fail (Error(RE_NOT_FFI_BUILD));
    }

    // The closure is a "black box" but client code takes the sizeof() to
    // pass into the alloc routine...

    typedef struct {
        int stub;
    } ffi_closure;

    void *ffi_closure_alloc(size_t size, void **code) {
        fail (Error(RE_NOT_FFI_BUILD));
    }

    ffi_status ffi_prep_closure_loc(
        ffi_closure *closure,
        ffi_cif *cif,
        void (*fun)(ffi_cif *, void *, void **, void *),
        void *user_data,
        void *codeloc
    ) {
        panic (Error(RE_NOT_FFI_BUILD));
    }

    void ffi_closure_free (void *closure) {
        panic (Error(RE_NOT_FFI_BUILD));
    }
#endif // HAVE_LIBFFI_AVAILABLE

inline static void QUEUE_EXTRA_MEM(REBRIN *r, void *p) {
    *SER_AT(void*, r->extra_mem, SER_LEN(r->extra_mem)) = p;
    EXPAND_SERIES_TAIL(r->extra_mem, 1);
}


static ffi_type *struct_type_to_ffi[STRUCT_TYPE_MAX];


static void process_type_block(
    REBRIN *r,
    REBVAL *param,
    REBVAL *blk,
    REBCNT n,
    REBOOL make
);


static void init_type_map()
{
    if (struct_type_to_ffi[0]) return;
    struct_type_to_ffi[STRUCT_TYPE_UINT8] = &ffi_type_uint8;
    struct_type_to_ffi[STRUCT_TYPE_INT8] = &ffi_type_sint8;
    struct_type_to_ffi[STRUCT_TYPE_UINT16] = &ffi_type_uint16;
    struct_type_to_ffi[STRUCT_TYPE_INT16] = &ffi_type_sint16;
    struct_type_to_ffi[STRUCT_TYPE_UINT32] = &ffi_type_uint32;
    struct_type_to_ffi[STRUCT_TYPE_INT32] = &ffi_type_sint32;
    struct_type_to_ffi[STRUCT_TYPE_UINT64] = &ffi_type_uint64;
    struct_type_to_ffi[STRUCT_TYPE_INT64] = &ffi_type_sint64;

    struct_type_to_ffi[STRUCT_TYPE_FLOAT] = &ffi_type_float;
    struct_type_to_ffi[STRUCT_TYPE_DOUBLE] = &ffi_type_double;

    struct_type_to_ffi[STRUCT_TYPE_POINTER] = &ffi_type_pointer;
}


//
//  CT_Routine: C
//
REBINT CT_Routine(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    if (mode >= 0)
        return VAL_FUNC_ROUTINE(a) == VAL_FUNC_ROUTINE(b);

    return -1;
}


//
//  CT_Callback: C
//
REBINT CT_Callback(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    return -1;
}


static REBCNT n_struct_fields (REBSER *fields)
{
    REBCNT n_fields = 0;

    REBCNT i;
    for (i = 0; i < SER_LEN(fields); ++i) {
        struct Struct_Field *field = SER_AT(struct Struct_Field, fields, i);

        if (field->type != STRUCT_TYPE_STRUCT)
            n_fields += field->dimension;
        else
            n_fields += n_struct_fields(field->fields);
    }
    return n_fields;
}


static ffi_type* struct_to_ffi(
    REBRIN *r,
    REBVAL *param,
    REBSER *fields,
    REBOOL make
){
    assert(IS_TYPESET(param));

    ffi_type *stype = NULL;
    if (make) {
        stype = OS_ALLOC(ffi_type);
        QUEUE_EXTRA_MEM(r, stype);
    }
    else {
        REBSER *ser = Make_Series(2, sizeof(ffi_type), MKS_NONE);
        SET_SER_FLAG(ser, SERIES_FLAG_FIXED_SIZE);
        stype = SER_HEAD(ffi_type, ser);
        PUSH_GUARD_SERIES(ser);
    }

    stype->size = 0;
    stype->alignment = 0;
    stype->type = FFI_TYPE_STRUCT;

    /* one extra for NULL */
    if (make) {
        stype->elements = OS_ALLOC_N(ffi_type*, 1 + n_struct_fields(fields));
        QUEUE_EXTRA_MEM(r, stype->elements);
    }
    else {
        REBSER *ser = Make_Series(
            2 + n_struct_fields(fields), sizeof(ffi_type*), MKS_NONE
        );
        SET_SER_FLAG(ser, SERIES_FLAG_FIXED_SIZE);
        stype->elements = SER_HEAD(ffi_type*, ser);
        PUSH_GUARD_SERIES(ser);
    }

    REBCNT j = 0;
    REBCNT i;
    for (i = 0; i < SER_LEN(fields); ++i) {
        struct Struct_Field *field = SER_AT(struct Struct_Field, fields, i);
        if (field->type == STRUCT_TYPE_REBVAL) {
            //
            // "don't see a point to pass a rebol value to external functions"
            //
            // !!! ^-- What if the value is being passed through and will
            // come back via a callback?
            //
            fail (Error(RE_MISC));
        }
        else if (field->type != STRUCT_TYPE_STRUCT) {
            if (!struct_type_to_ffi[field->type])
                return NULL;

            REBCNT n;
            for (n = 0; n < field->dimension; ++n) {
                stype->elements[j++] = struct_type_to_ffi[field->type];
            }
        }
        else {
            ffi_type *subtype = struct_to_ffi(r, param, field->fields, make);
            if (!subtype)
                return NULL;

            REBCNT n = 0;
            for (n = 0; n < field->dimension; ++n) {
                stype->elements[j++] = subtype;
            }
        }
    }
    stype->elements[j] = NULL;

    return stype;
}


/* convert the type of "elem", and store it in "out" with index of "idx"
 */
static REBOOL rebol_type_to_ffi(
    REBRIN *r,
    REBVAL *param,
    const REBVAL *elem,
    REBCNT idx,
    REBOOL make
){
    assert(IS_TYPESET(param));

    ffi_type **args = SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r));

    if (IS_WORD(elem)) {
        REBVAL *temp;

        switch (VAL_WORD_CANON(elem)) {
        case SYM_VOID:
            args[idx] = &ffi_type_void;
            break;

        case SYM_UINT8:
            args[idx] = &ffi_type_uint8;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_INT8:
            args[idx] = &ffi_type_sint8;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_UINT16:
            args[idx] = &ffi_type_uint16;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_INT16:
            args[idx] = &ffi_type_sint16;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_UINT32:
            args[idx] = &ffi_type_uint32;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_INT32:
            args[idx] = &ffi_type_sint32;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_UINT64:
            args[idx] = &ffi_type_uint64;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_INT64:
            args[idx] = &ffi_type_sint64;
            TYPE_SET(param, REB_INTEGER);
            break;

        case SYM_FLOAT:
            args[idx] = &ffi_type_float;
            TYPE_SET(param, REB_DECIMAL);
            break;

        case SYM_DOUBLE:
            args[idx] = &ffi_type_double;
            TYPE_SET(param, REB_DECIMAL);
            break;

        case SYM_POINTER:
            args[idx] = &ffi_type_pointer;
            TYPE_SET(param, REB_INTEGER);
            TYPE_SET(param, REB_STRING);
            TYPE_SET(param, REB_BINARY);
            TYPE_SET(param, REB_VECTOR);
            TYPE_SET(param, REB_FUNCTION); // callback
            break;

        default:
            return FALSE;
        }
        temp = Alloc_Tail_Array(RIN_FFI_ARG_STRUCTS(r));
        SET_BLANK(temp);
        return TRUE;
    }

    if (IS_STRUCT(elem)) {
        ffi_type *ftype = struct_to_ffi(
            r, param, VAL_STRUCT_FIELDS(elem), make
        );
        if (!ftype)
            return FALSE;

        args[idx] = ftype;
        TYPE_SET(param, REB_STRUCT);

        // !!! Comment said "for callback and return value"
        //
        if (idx == 0)
            Copy_Struct_Val(elem, KNOWN(ARR_HEAD(RIN_FFI_ARG_STRUCTS(r))));
        else
            Copy_Struct_Val(elem, Alloc_Tail_Array(RIN_FFI_ARG_STRUCTS(r)));

        return TRUE;
    }

    return FALSE;
}


/* make a copy of the argument
 * arg referes to return value when idx = 0
 * function args start from idx = 1
 *
 * @ptrs is an array with a length of number of arguments of @rot
 *
 * For FFI_TYPE_POINTER, a temperary pointer could be needed
 * (whose address is returned). ptrs[idx] is the temperary pointer.
 * */
static void *arg_to_ffi(
    REBRIN *r,
    REBVAL *param,
    REBVAL *arg,
    ffi_type *arg_ffi_type,
    void **ptr,
    REBOOL returning
){
    assert(IS_TYPESET(param));

    struct Reb_Frame *frame_ = FS_TOP; // So you can use the D_xxx macros

    switch (arg_ffi_type->type) {
    case FFI_TYPE_UINT8:
        if (!IS_INTEGER(arg)) {
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }
        else {
        #ifdef BIG_ENDIAN
            u8 i = (u8) VAL_INT64(arg);
            memcpy(&VAL_INT64(arg), &i, sizeof(u8));
        #endif
            return &VAL_INT64(arg);
        }

    case FFI_TYPE_SINT8:
        if (!IS_INTEGER(arg)) {
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }
        else {
        #ifdef BIG_ENDIAN
            i8 i = (i8) VAL_INT64(arg);
            memcpy(&VAL_INT64(arg), &i, sizeof(i8));
        #endif
            return &VAL_INT64(arg);
        }

    case FFI_TYPE_UINT16:
        if (!IS_INTEGER(arg)) {
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }
        else {
        #ifdef BIG_ENDIAN
            u16 i = (u16) VAL_INT64(arg);
            memcpy(&VAL_INT64(arg), &i, sizeof(u16));
        #endif
            return &VAL_INT64(arg);
        }

    case FFI_TYPE_SINT16:
        if (!IS_INTEGER(arg)) {
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }
        else {
        #ifdef BIG_ENDIAN
            i16 i = (i16) VAL_INT64(arg);
            memcpy(&VAL_INT64(arg), &i, sizeof(i16));
        #endif
            return &VAL_INT64(arg);
        }

    case FFI_TYPE_UINT32:
        if (!IS_INTEGER(arg)) {
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }
        else {
        #ifdef BIG_ENDIAN
            u32 i = (u32) VAL_INT64(arg);
            memcpy(&VAL_INT64(arg), &i, sizeof(u32));
        #endif
            return &VAL_INT64(arg);
        }

    case FFI_TYPE_SINT32:
        if (!IS_INTEGER(arg)) {
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }
        else {
        #ifdef BIG_ENDIAN
            i32 i = (i32) VAL_INT64(arg);
            memcpy(&VAL_INT64(arg), &i, sizeof(i32));
        #endif
            return &VAL_INT64(arg);
        }

    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        return &VAL_INT64(arg);

    case FFI_TYPE_POINTER:
        switch (VAL_TYPE(arg)) {
        case REB_INTEGER:
            return &VAL_INT64(arg);

        case REB_STRING:
        case REB_BINARY:
        case REB_VECTOR:
            *ptr = VAL_RAW_DATA_AT(arg);
            return ptr;

        case REB_FUNCTION:
            if (!GET_RIN_FLAG(VAL_FUNC_ROUTINE(arg), ROUTINE_FLAG_CALLBACK))
                fail (Error(RE_ONLY_CALLBACK_PTR));
            *ptr = RIN_DISPATCHER(VAL_FUNC_ROUTINE(arg));
            return ptr;

        default:
            fail (Error_Arg_Type(
                D_LABEL_SYM, param, VAL_TYPE(arg)
            ));
        }

    case FFI_TYPE_FLOAT:
        {
        if (!IS_DECIMAL(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        // !!! hackish, store the single precision floating point number
        // in a double precision variable
        //
        float a = cast(float, VAL_DECIMAL(arg));
        memcpy(&VAL_DECIMAL(arg), &a, sizeof(a));
        return &VAL_DECIMAL(arg);
        }

    case FFI_TYPE_DOUBLE:
        if (!IS_DECIMAL(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));
        return &VAL_DECIMAL(arg);

    case FFI_TYPE_STRUCT:
        if (returning) {
            Copy_Struct(&RIN_RVALUE(r), &VAL_STRUCT(arg));
        }
        else {
            if (!IS_STRUCT(arg))
                fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));
        }
        return SER_AT(
            REBYTE,
            VAL_STRUCT_DATA_BIN(arg),
            VAL_STRUCT_OFFSET(arg)
        );

    case FFI_TYPE_VOID:
        if (!returning)
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));
        return NULL;

    default:
        fail (Error_Invalid_Arg(arg));
    }

    return NULL;
}


/* convert the return value to rebol
 */
static void ffi_to_rebol(
    REBRIN *rin,
    ffi_type *ffi_rtype,
    void *ffi_rvalue,
    REBVAL *rebol_ret
){
    switch (ffi_rtype->type) {
    case FFI_TYPE_UINT8:
        SET_INTEGER(rebol_ret, *cast(u8*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT8:
        SET_INTEGER(rebol_ret, *cast(i8*, ffi_rvalue));
        break;

    case FFI_TYPE_UINT16:
        SET_INTEGER(rebol_ret, *cast(u16*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT16:
        SET_INTEGER(rebol_ret, *cast(i16*, ffi_rvalue));
        break;

    case FFI_TYPE_UINT32:
        SET_INTEGER(rebol_ret, *cast(u32*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT32:
        SET_INTEGER(rebol_ret, *cast(i32*, ffi_rvalue));
        break;

    case FFI_TYPE_UINT64:
        SET_INTEGER(rebol_ret, *cast(u64*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT64:
        SET_INTEGER(rebol_ret, *cast(i64*, ffi_rvalue));
        break;

    case FFI_TYPE_POINTER:
        SET_INTEGER(rebol_ret, cast(REBUPT, *cast(void**, ffi_rvalue)));
        break;

    case FFI_TYPE_FLOAT:
        SET_DECIMAL(rebol_ret, *cast(float*, ffi_rvalue));
        break;

    case FFI_TYPE_DOUBLE:
        SET_DECIMAL(rebol_ret, *cast(double*, ffi_rvalue));
        break;

    case FFI_TYPE_STRUCT:
        VAL_RESET_HEADER(rebol_ret, REB_STRUCT);
        Copy_Struct(&RIN_RVALUE(rin), &VAL_STRUCT(rebol_ret));
        memcpy(
            SER_AT(
                REBYTE,
                VAL_STRUCT_DATA_BIN(rebol_ret),
                VAL_STRUCT_OFFSET(rebol_ret)
            ),
            ffi_rvalue,
            VAL_STRUCT_LEN(rebol_ret)
        );
        break;

    case FFI_TYPE_VOID:
        break;

    default:
        fail (Error_Invalid_Arg(rebol_ret));
    }
}


//
//  Routine_Dispatcher: C
//
REB_R Routine_Dispatcher(struct Reb_Frame *f)
{
    REBRIN *r = FUNC_ROUTINE(f->func);

    // !!! This code calls GUARD_SERIES, but doesn't remember what series
    // it guarded.  It remembers the initial pointer and restores it...which
    // is a bit hacky for reaching beneath the abstraction layer.  It also
    // means that it could silently cover up cases of stray guards leaking
    // that were not ones it would have released.
    //
    // Comment here said: "Temporary series could be allocated in
    // process_type_block, recursively."
    //
    REBCNT series_guard_tail = SER_LEN(GC_Series_Guard);

    if (RIN_LIB(r) == NULL) {
        //
        // lib is NULL when routine is constructed from address directly,
        // so there's nothing to track whether that gets loaded or unloaded
    }
    else {
        if (GET_LIB_FLAG(RIN_LIB(r), LIB_FLAG_CLOSED))
            fail (Error(RE_BAD_LIBRARY));
    }

    // temporary series to hold pointer parameters...must be big enough
    //
    REBSER *ffi_args_ptrs = Make_Series(
        SER_LEN(RIN_FFI_ARG_TYPES(r)), sizeof(void *), MKS_NONE
    );

    // `ser` is a series of FFI arguments (void*).  It will be NULL if the
    // function has no arguments.
    //
    REBSER *ser_ffi_args;
    ffi_cif *cif;
    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
        //
        // !!! In the Atronix branch FFI, a variadic routine's parameter
        // list is a single element long--a BLOCK!.  This block is unpacked
        // of the fixed and variadic arguments.  This is distinct from the
        // Ren-C varargs model where the called routine consumes as many
        // arguments as it wants once called from the callsite.  However,
        // a BAR! could terminate the variadic, or it could be contained
        // inside a GROUP!...giving the user choices to work with something
        // compatible with Ren-C's parameter packs.
        //
        REBVAL *va_values = FRM_ARGS_HEAD(f);
        if (!IS_BLOCK(va_values))
            fail (Error_Invalid_Arg(va_values));

        // Number of fixed arguments.  Must subtract 1 because the [0]th
        // element slot is the return value.
        //
        REBCNT n_fixed = ARR_LEN(RIN_FIXED_ARGS(r)) - 1;

        // It appears that variadic arguments in each call come as a pair of
        // the datatype and the value.  So there must be an even number.
        //
        if ((VAL_LEN_AT(va_values) - n_fixed) % 2 != 0)
            fail (Error_Invalid_Arg(va_values));

        ser_ffi_args = Make_Series(
            n_fixed + (VAL_LEN_AT(va_values) - n_fixed) / 2,
            sizeof(void *),
            MKS_NONE
        );

        ffi_type **arg_types = NULL;

        // reset length
        SET_SERIES_LEN(RIN_FFI_ARG_TYPES(r), n_fixed + 1);

        REBCNT i = 1;
        REBCNT j = 1;
        for (; i < VAL_LEN_HEAD(va_values) + 1; ++i, ++j) {
            REBVAL *reb_arg = KNOWN(VAL_ARRAY_AT_HEAD(va_values, i - 1));

            REBVAL temp;

            REBVAL *param = NULL; // catch reuse

            if (i <= n_fixed) {
                param = KNOWN(ARR_AT(RIN_FIXED_ARGS(r), i));
                if (!TYPE_CHECK(param, VAL_TYPE(reb_arg))) {
                    fail (Error_Arg_Type(
                        FRM_LABEL(f),
                        KNOWN(ARR_AT(RIN_FIXED_ARGS(r), i)),
                        VAL_TYPE(reb_arg)
                    ));
                }
            } else {
                /* initialize rin->args */

                if (i == VAL_LEN_HEAD(va_values)) /* type is missing */
                    fail (Error_Invalid_Arg(reb_arg));

                REBVAL *reb_type = KNOWN(VAL_ARRAY_AT_HEAD(va_values, i));
                if (!IS_BLOCK(reb_type))
                    fail (Error_Invalid_Arg(reb_type));

                // Start with no type bits initially (process_type_block will
                // add them).
                //
                // !!! Clearer name for individual variadic args than "..."?
                //
                Val_Init_Typeset(&temp, 0, SYM_ELLIPSIS);
                param = &temp;

                EXPAND_SERIES_TAIL(RIN_FFI_ARG_TYPES(r), 1);
                process_type_block(r, param, reb_type, j, FALSE);
                ++i;
            }
            *SER_AT(void*, ser_ffi_args, j - 1) = arg_to_ffi(
                r,
                param,
                reb_arg,
                *SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), j),
                SER_AT(void*, ffi_args_ptrs, j),
                FALSE // not a return value
            );
        }

        /* series data could have moved */
        arg_types = SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r));

        assert(j == SER_LEN(RIN_FFI_ARG_TYPES(r)));

        // Variadics must use ffi_prep_cif_var for each variadic call.  (it
        // would be possible to cache them if the same variadic signature
        // was used repeatedly)
        //
        assert(RIN_CIF(r) == NULL);
        cif = OS_ALLOC(ffi_cif);
        if (
            FFI_OK != ffi_prep_cif_var(
                cif,
                RIN_ABI(r),
                n_fixed, // number of fixed arguments
                j - 1, // number of all arguments
                arg_types[0], // return type
                &arg_types[1]
            )
        ){
            OS_FREE(cif);
            //RL_Print("Couldn't prep CIF_VAR\n");
            fail (Error_Invalid_Arg(va_values));
        }
    }
    else if (SER_LEN(RIN_FFI_ARG_TYPES(r)) == 1) {
        //
        // 1 means it's just a return value (no arguments)
        //
        assert(RIN_CIF(r) != NULL);
        cif = RIN_CIF(r);

        ser_ffi_args = NULL;
    }
    else {
        assert(RIN_CIF(r) != NULL);
        cif = RIN_CIF(r);

        assert(SER_LEN(RIN_FFI_ARG_TYPES(r)) > 1);
        ser_ffi_args = Make_Series(
            SER_LEN(RIN_FFI_ARG_TYPES(r)) - 1,
            sizeof(void *),
            MKS_NONE
        );

        REBCNT i;
        for (i = 1; i < SER_LEN(RIN_FFI_ARG_TYPES(r)); ++i) {
            *SER_AT(void*, ser_ffi_args, i - 1) = arg_to_ffi(
                r,
                FUNC_PARAM(FRM_FUNC(f), i),
                FRM_ARG(f, i), // 1-based access
                *SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), i),
                SER_AT(void*, ffi_args_ptrs, i),
                FALSE // not a return value
            );
        }
    }

    // "prep" the return value
    //
    // !!! why is this necessary before the call to the C routine is made?
    // shouldn't the type and bits be set by the return result conversion
    // after the call is finished?
    //
    ffi_type *rtype = *SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r));

    switch (rtype->type) {
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
    case FFI_TYPE_POINTER:
        SET_INTEGER(f->out, 0);
        break;

    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
        SET_DECIMAL(f->out, 0);
        break;

    case FFI_TYPE_STRUCT:
        VAL_RESET_HEADER(f->out, REB_STRUCT);
        break;

    case FFI_TYPE_VOID:
        SET_VOID(f->out);
        break;

    default:
        // !!! Was passing uninitialized f->out to Error_Invalid_Arg
        fail (Error(RE_MISC));
    }

    REBVAL dummy_param;
    Val_Init_Typeset(&dummy_param, 0, SYM_RETURN);

    void *rvalue = arg_to_ffi(
        r,
        &dummy_param,
        f->out,
        *SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), 0),
        SER_AT(void*, ffi_args_ptrs, 0),
        TRUE // is a return value
    );

    SET_VOID(&Callback_Error);

    ffi_call(
        cif,
        RIN_FUNCPTR(r),
        rvalue,
        ser_ffi_args ? SER_HEAD(void*, ser_ffi_args) : NULL
    );

    if (IS_ERROR(&Callback_Error))
        fail (VAL_CONTEXT(&Callback_Error));

    ffi_to_rebol(
        r,
        SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r))[0],
        rvalue,
        f->out
    );

    Free_Series(ffi_args_ptrs);

    if (ser_ffi_args)
        Free_Series(ser_ffi_args);

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC))
        OS_FREE(cif);

    //restore the saved series stack pointer
    SET_SERIES_LEN(GC_Series_Guard, series_guard_tail);

    // Note: cannot "throw" a Rebol value across an FFI boundary.

    assert(!THROWN(f->out));
    return R_OUT;
}


//
//  Free_Routine: C
//
void Free_Routine(REBRIN *rin)
{
    REBCNT n = 0;
    for (n = 0; n < SER_LEN(rin->extra_mem); ++n) {
        void *addr = *SER_AT(void*, rin->extra_mem, n);
        //printf("freeing %p\n", addr);
        OS_FREE(addr);
    }

    CLEAR_RIN_FLAG(rin, ROUTINE_FLAG_MARK);
    if (GET_RIN_FLAG(rin, ROUTINE_FLAG_CALLBACK))
        ffi_closure_free(RIN_CLOSURE(rin));

    if (GET_RIN_FLAG(rin, ROUTINE_FLAG_VARIADIC))
        assert(RIN_CIF(rin) == NULL);
    else
        OS_FREE(RIN_CIF(rin));

    Free_Node(RIN_POOL, (REBNOD*)rin);
}


static void process_type_block(
    REBRIN *r,
    REBVAL *param,
    REBVAL *blk,
    REBCNT n,
    REBOOL make
){
    if (!IS_BLOCK(blk))
        fail (Error_Invalid_Arg(blk));

    REBVAL *t = KNOWN(VAL_ARRAY_AT(blk));
    if (IS_WORD(t) && VAL_WORD_CANON(t) == SYM_STRUCT_TYPE) {
        /* followed by struct definition */
        REBVAL tmp;
        SET_BLANK(&tmp); // GC should not reach uninitialized values
        PUSH_GUARD_VALUE(&tmp);

        ++t;
        if (!IS_BLOCK(t) || VAL_LEN_AT(blk) != 2)
            fail (Error_Invalid_Arg(blk));

        if (!MT_Struct(&tmp, t, SPECIFIED, REB_STRUCT))
            fail (Error_Invalid_Arg(blk));

        if (!rebol_type_to_ffi(r, param, &tmp, n, make))
            fail (Error_Invalid_Arg(blk));

        DROP_GUARD_VALUE(&tmp);
    }
    else {
        if (VAL_LEN_AT(blk) != 1)
            fail (Error_Invalid_Arg(blk));

        if (!rebol_type_to_ffi(r, param, t, n, make))
            fail (Error_Invalid_Arg(t));
    }
}


static void callback_dispatcher(
    ffi_cif *cif,
    void *ret,
    void **args,
    void *user_data
){
    if (!IS_VOID(&Callback_Error)) // !!!is this possible?
        return;

    REBRIN *rin = cast(REBRIN*, user_data);

    // We do not want to longjmp() out of the callback if there is an error.
    // It needs to allow the FFI processing to unwind the stack normally so
    // that it's in a good state.  Therefore this must trap any fail()s.
    //
    struct Reb_State state;
    REBCTX *error;

    PUSH_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) {
        Val_Init_Error(&Callback_Error, error);
        return;
    }

    REBARR *array = Make_Array(1 + cif->nargs);

    REBVAL *elem = Alloc_Tail_Array(array);
    *elem = *FUNC_VALUE(RIN_CALLBACK_FUNC(rin));

    REBCNT i;
    for (i = 0; i < cif->nargs; ++i) {
        elem = Alloc_Tail_Array(array);

        switch (cif->arg_types[i]->type) {
        case FFI_TYPE_UINT8:
            SET_INTEGER(elem, *(u8*)args[i]);
            break;

        case FFI_TYPE_SINT8:
            SET_INTEGER(elem, *(i8*)args[i]);
            break;

        case FFI_TYPE_UINT16:
            SET_INTEGER(elem, *(u16*)args[i]);
            break;

        case FFI_TYPE_SINT16:
            SET_INTEGER(elem, *(i16*)args[i]);
            break;

        case FFI_TYPE_UINT32:
            SET_INTEGER(elem, *(u32*)args[i]);
            break;

        case FFI_TYPE_SINT32:
            SET_INTEGER(elem, *(i32*)args[i]);
            break;

        case FFI_TYPE_UINT64:
        case FFI_TYPE_POINTER:
            SET_INTEGER(elem, *(u64*)args[i]);
            break;

        case FFI_TYPE_SINT64:
            SET_INTEGER(elem, *(i64*)args[i]);
            break;

        case FFI_TYPE_STRUCT:
            if (!IS_STRUCT(ARR_AT(RIN_FFI_ARG_STRUCTS(rin), i + 1)))
                fail (Error_Invalid_Arg(
                    KNOWN(ARR_AT(RIN_FFI_ARG_STRUCTS(rin), i + 1))
                ));

            Copy_Struct_Val(
                KNOWN(ARR_AT(RIN_FFI_ARG_STRUCTS(rin), i + 1)), elem
            );
            memcpy(
                SER_AT(
                    REBYTE,
                    VAL_STRUCT_DATA_BIN(elem),
                    VAL_STRUCT_OFFSET(elem)
                ),
                args[i],
                VAL_STRUCT_LEN(elem)
            );
            break;

        default:
            // !!! was fail (Error_Invalid_Arg(elem)) w/uninitizalized elem
            fail (Error(RE_MISC));
        }
    }

    // !!! Currently an array must be managed in order to use it with DO,
    // because the series could be put into a block of a backtrace.  It will
    // be guarded implicitly during the Do_At_Throws(), however.
    //
    MANAGE_ARRAY(array);

    REBVAL result;
    if (Do_At_Throws(&result, array, 0, SPECIFIED)) {
        //
        // !!! Does not check for thrown cases...what should this
        // do in case of THROW, BREAK, QUIT?
        //
        fail (Error_No_Catch_For_Throw(&result));
    }

    // !!! Could Free_Array(array) if not managed to use with DO

    switch (cif->rtype->type) {
    case FFI_TYPE_VOID:
        break;

    case FFI_TYPE_UINT8:
        *((u8*)ret) = (u8)VAL_INT64(&result);
        break;

    case FFI_TYPE_SINT8:
        *((i8*)ret) = (i8)VAL_INT64(&result);
        break;

    case FFI_TYPE_UINT16:
        *((u16*)ret) = (u16)VAL_INT64(&result);
        break;

    case FFI_TYPE_SINT16:
        *((i16*)ret) = (i16)VAL_INT64(&result);
        break;

    case FFI_TYPE_UINT32:
        *((u32*)ret) = (u32)VAL_INT64(&result);
        break;

    case FFI_TYPE_SINT32:
        *((i32*)ret) = (i32)VAL_INT64(&result);
        break;

    case FFI_TYPE_UINT64:
    case FFI_TYPE_POINTER:
        *((u64*)ret) = (u64)VAL_INT64(&result);
        break;

    case FFI_TYPE_SINT64:
        *((i64*)ret) = (i64)VAL_INT64(&result);
        break;

    case FFI_TYPE_STRUCT:
        memcpy(
            ret,
            SER_AT(
                REBYTE,
                VAL_STRUCT_DATA_BIN(&result),
                VAL_STRUCT_OFFSET(&result)
            ),
            VAL_STRUCT_LEN(&result)
        );
        break;

    default:
        fail (Error_Invalid_Arg(&result));
    }

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);
}


//
//  MT_Routine: C
// 
// format:
// make routine! [[
//     "document"
//     arg1 [type1 type2] "note"
//     arg2 [type3] "note"
//     ...
//     argn [typen] "note"
//     return: [type] "note"
//     abi: word "note"
// ] lib "name"]
//
REBOOL MT_Routine(
    REBVAL *out,
    RELVAL *data,
    REBCTX *specifier,
    REBOOL is_callback
) {
    ffi_type ** args = NULL;
    RELVAL *blk = NULL;
    REBCNT eval_idx = 0; /* for spec block evaluation */
    REBSER *extra_mem = NULL;
    REBOOL ret = TRUE;
    CFUNC *func = NULL;
    REBCNT n = 1; /* arguments start with the index 1 (return type has a index of 0) */
    REBCNT has_return = 0;
    REBCNT has_abi = 0;
    REBVAL *temp;

    if (!IS_BLOCK(data)) {
        return FALSE;
    }

    // !!! This code has a challenging property with the new invariant, that
    // a function is created in a single step from a paramlist and a
    // dispatcher.  The challenging property is that in order to GC protect
    // a routine as it is being built, its "REBRIN" must be called out to
    // the GC...which is done by being part of a routine.  But since
    // evaluations happen during the building to make the paramlist, this
    // is a Catch-22.
    //
    // Specific binding branch has an implementation that sorts this out, but
    // may alter other behaviors.  In the meantime this is left building the
    // function internals by hand.

    VAL_RESET_HEADER(out, REB_FUNCTION);

    REBARR* body_array = Make_Singular_Array(BLANK_VALUE);
    out->payload.function.body = body_array;
    MANAGE_ARRAY(body_array);
    assert(IS_BLANK(VAL_FUNC_BODY(out)));

    ARR_SERIES(body_array)->misc.dispatcher = &Routine_Dispatcher;
    assert(VAL_FUNC_DISPATCHER(out) == &Routine_Dispatcher);

    REBRIN *r = cast(REBRIN*, Make_Node(RIN_POOL));

    VAL_RESET_HEADER(VAL_FUNC_BODY(out), REB_HANDLE);
    VAL_HANDLE_DATA(VAL_FUNC_BODY(out)) = cast(REBRIN*, r);

    memset(r, 0, sizeof(REBRIN));
    SET_RIN_FLAG(r, ROUTINE_FLAG_USED);

    if (is_callback)
        SET_RIN_FLAG(r, ROUTINE_FLAG_CALLBACK);

#define N_ARGS 8

    // !!! Routines use different spec logic than the other generators.

    out->payload.function.func = AS_FUNC(Make_Array(N_ARGS));

    VAL_FUNC_META(out) = NULL; /* Copy_Array_Shallow(VAL_ARRAY(data)) */

    RIN_FFI_ARG_TYPES(r) = Make_Series(N_ARGS, sizeof(ffi_type*), MKS_NONE);

    out->payload.function.func = AS_FUNC(Make_Array(N_ARGS));

    // first slot is reserved for the "self", see `struct Reb_Func`
    //
    temp = Alloc_Tail_Array(FUNC_PARAMLIST(out->payload.function.func));
    *temp = *out;

    RIN_FFI_ARG_STRUCTS(r) = Make_Array(N_ARGS);
    // reserve for returning struct
    temp = Alloc_Tail_Array(RIN_FFI_ARG_STRUCTS(r));

    // !!! should this be INIT_CELL_WRITABLE_IF_DEBUG(), e.g. write-only location?
    //
    SET_BLANK(temp);

    INIT_RIN_ABI(r, FFI_DEFAULT_ABI);
    RIN_LIB(r) = NULL;

    extra_mem = Make_Series(N_ARGS, sizeof(void*), MKS_NONE);
    RIN_EXTRA_MEM(r) = extra_mem;

    args = SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r));
    EXPAND_SERIES_TAIL(RIN_FFI_ARG_TYPES(r), 1); //reserved for return type
    args[0] = &ffi_type_void; //default return type

    init_type_map();

    blk = VAL_ARRAY_AT(data);

    MANAGE_ARRAY(VAL_FUNC_PARAMLIST(out));

    MANAGE_SERIES(RIN_FFI_ARG_TYPES(r));
    MANAGE_ARRAY(RIN_FFI_ARG_STRUCTS(r));
    MANAGE_SERIES(RIN_EXTRA_MEM(r));

    if (!is_callback) {
        REBIXO indexor = 0;

        if (!IS_BLOCK(&blk[0]))
            fail (Error_Unexpected_Type(REB_BLOCK, VAL_TYPE(&blk[0])));

        REBVAL lib;
        indexor = DO_NEXT_MAY_THROW(&lib, VAL_ARRAY(data), 1, specifier);

        if (indexor == THROWN_FLAG)
            fail (Error_No_Catch_For_Throw(&lib));

        if (IS_INTEGER(&lib)) {
            if (indexor != END_FLAG)
                fail (Error_Invalid_Arg(KNOWN(&blk[cast(REBCNT, indexor)])));

            //treated as a pointer to the function
            if (VAL_INT64(&lib) == 0)
                fail (Error_Invalid_Arg(&lib));

            // Cannot cast directly to a function pointer from a 64-bit value
            // on 32-bit systems; first cast to int that holds Unsigned PoinTer
            //
            RIN_FUNCPTR(r) = cast(CFUNC*, cast(REBUPT, VAL_INT64(&lib)));
        }
        else {
            REBSER *byte_sized;
            REBCNT b_index;
            REBCNT b_len;
            REBCNT fn_idx = cast(REBCNT, indexor);

            if (!IS_LIBRARY(&lib))
                fail (Error_Invalid_Arg(&lib));

            if (!IS_STRING(&blk[fn_idx]))
                fail (Error_Invalid_Arg(KNOWN(&blk[fn_idx])));

            if (NOT_END(&blk[fn_idx + 1]))
                fail (Error_Invalid_Arg(KNOWN(&blk[fn_idx + 1])));

            RIN_LIB(r) = VAL_LIB_HANDLE(&lib);
            if (RIN_LIB(r) == NULL)
                fail (Error_Invalid_Arg(&lib));

            TERM_SEQUENCE(VAL_SERIES(&blk[fn_idx]));

            // OS_FIND_FUNCTION takes a char* on both Windows and Posix.  The
            // string that gets here could be REBUNI wide or BYTE_SIZE(), so
            // make sure it's turned into a char* before passing.
            //
            // !!! Should it error if any bytes need to be UTF8 encoded?
            //
            b_index = VAL_INDEX(&blk[fn_idx]);
            b_len = VAL_LEN_AT(&blk[fn_idx]);
            byte_sized = Temp_Bin_Str_Managed(
                KNOWN(&blk[fn_idx]), &b_index, &b_len
            );

            func = OS_FIND_FUNCTION(
                LIB_FD(RIN_LIB(r)),
                SER_HEAD(char, byte_sized)
            );

            if (!func) {
                //printf("Couldn't find function: %s\n", VAL_DATA_AT(&blk[2]));
                fail (Error_Invalid_Arg(KNOWN(&blk[fn_idx])));
            }

            RIN_FUNCPTR(r) = func;
        }
    }
    else {
        REBIXO indexor = 0;

        if (!IS_BLOCK(&blk[0]))
            fail (Error_Invalid_Arg(KNOWN(&blk[0])));

        REBVAL fun;
        indexor = DO_NEXT_MAY_THROW(&fun, VAL_ARRAY(data), 1, specifier);

        if (indexor == THROWN_FLAG)
            fail (Error_No_Catch_For_Throw(&fun));

        if (!IS_FUNCTION(&fun))
            fail (Error_Invalid_Arg(&fun));

        RIN_CALLBACK_FUNC(r) = VAL_FUNC(&fun);

        if (indexor != END_FLAG)
            fail (Error_Invalid_Arg(KNOWN(&blk[cast(REBCNT, indexor)])));
    }



    blk = VAL_ARRAY_AT(&blk[0]);
    for (; NOT_END(blk); ++blk) {
        if (IS_STRING(blk)) {
            // Notes in the spec, ignore them
            continue;
        }

        switch (VAL_TYPE(blk)) {
        case REB_WORD:
            {
            REBVAL *v = NULL;
            if (VAL_WORD_CANON(blk) == SYM_ELLIPSIS) {
                if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
                    // duplicate ellipsis
                    fail (Error_Invalid_Arg(KNOWN(blk)));
                }

                SET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC);

                // Change the argument list to be a block
                RIN_FIXED_ARGS(r) = Copy_Array_Shallow(
                    VAL_FUNC_PARAMLIST(out), SPECIFIED
                );
                MANAGE_ARRAY(RIN_FIXED_ARGS(r));
                Remove_Series(
                    ARR_SERIES(VAL_FUNC_PARAMLIST(out)),
                    1,
                    ARR_LEN(VAL_FUNC_PARAMLIST(out))
                );
                v = Alloc_Tail_Array(VAL_FUNC_PARAMLIST(out));
                Val_Init_Typeset(
                    v, FLAGIT_KIND(REB_BLOCK), SYM_VARARGS
                );
            }
            else {
                if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
                    //... has to be the last argument
                    fail (Error_Invalid_Arg(KNOWN(blk)));
                }
                v = Alloc_Tail_Array(VAL_FUNC_PARAMLIST(out));
                Val_Init_Typeset(v, 0, VAL_WORD_SYM(blk));
                EXPAND_SERIES_TAIL(RIN_FFI_ARG_TYPES(r), 1);

                ++blk;
                process_type_block(
                    r, VAL_FUNC_PARAM(out, n), KNOWN(blk), n, TRUE
                );
            }

            // Function dispatch needs to know whether parameters are
            // to be hard quoted, soft quoted, refinements, or
            // evaluated.  This is signaled with bits on the typeset.
            //
            INIT_VAL_PARAM_CLASS(v, PARAM_CLASS_NORMAL);

            ++n;
            }
            break;

        case REB_SET_WORD:
            switch (VAL_WORD_CANON(blk)) {
            case SYM_ABI:
                ++blk;
                if (!IS_WORD(blk) || has_abi > 1)
                    fail (Error_Invalid_Arg(KNOWN(blk)));

                switch (VAL_WORD_CANON(blk)) {
                case SYM_DEFAULT:
                    INIT_RIN_ABI(r, FFI_DEFAULT_ABI);
                    break;

    #ifdef X86_WIN64

                case SYM_WIN64:
                    INIT_RIN_ABI(r, FFI_WIN64);
                    break;

    #elif defined(X86_WIN32) || defined(TO_LINUX_X86) || defined(TO_LINUX_X64)

                case SYM_STDCALL:
                    INIT_RIN_ABI(r, FFI_STDCALL);
                    break;

                case SYM_SYSV:
                    INIT_RIN_ABI(r, FFI_SYSV);
                    break;

                case SYM_THISCALL:
                    INIT_RIN_ABI(r, FFI_THISCALL);
                    break;

                case SYM_FASTCALL:
                    INIT_RIN_ABI(r, FFI_FASTCALL);
                    break;

            #ifdef X86_WIN32
                case SYM_MS_CDECL:
                    INIT_RIN_ABI(r, FFI_MS_CDECL);
                    break;
            #else
                case SYM_UNIX64:
                    INIT_RIN_ABI(r, FFI_UNIX64);
                    break;
            #endif //X86_WIN32

    #elif defined (TO_LINUX_ARM)

                case SYM_VFP:
                    INIT_RIN_ABI(r, FFI_VFP);
                    break;

                case SYM_SYSV:
                    INIT_RIN_ABI(r, FFI_SYSV);
                    break;

    #elif defined (TO_LINUX_MIPS)

                case SYM_O32:
                    INIT_RIN_ABI(r, FFI_O32);
                    break;

                case SYM_N32:
                    INIT_RIN_ABI(r, FFI_N32);
                    break;

                case SYM_N64:
                    INIT_RIN_ABI(r, FFI_N64);
                    break;

                case SYM_O32_SOFT_FLOAT:
                    INIT_RIN_ABI(r, FFI_O32_SOFT_FLOAT);
                    break;

                case SYM_N32_SOFT_FLOAT:
                    INIT_RIN_ABI(r, FFI_N32_SOFT_FLOAT);
                    break;

                case SYM_N64_SOFT_FLOAT:
                    INIT_RIN_ABI(r, FFI_N64_SOFT_FLOAT);
                    break;

    #endif //X86_WIN64

                default:
                    fail (Error_Invalid_Arg(KNOWN(blk)));
                }
                ++has_abi;
                break; // case SYM_ABI

            case SYM_RETURN:
                {
                if (has_return > 1)
                    fail (Error_Invalid_Arg(KNOWN(blk)));

                ++has_return;
                ++blk;

                REBVAL dummy;
                Val_Init_Typeset(&dummy, 0, SYM_RETURN);
                process_type_block(r, &dummy, KNOWN(blk), 0, TRUE);
                }
                break;

            default:
                fail (Error_Invalid_Arg(KNOWN(blk)));
            }
            break;

        default:
            fail (Error_Invalid_Arg(KNOWN(blk)));
        }
    }

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
        //
        // Each individual call needs to use `ffi_prep_cif_var` to make the
        // proper variadic CIF for that call.
        //
        INIT_RIN_CIF(r, NULL);
    }
    else {
        // The same CIF can be used for every call of the routine if it is
        // not variadic.
        //
        INIT_RIN_CIF(r, OS_ALLOC(ffi_cif));

        /* series data could have moved */
        args = SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r));
        if (
            FFI_OK != ffi_prep_cif(
                RIN_CIF(r),
                RIN_ABI(r),
                SER_LEN(RIN_FFI_ARG_TYPES(r)) - 1,
                args[0],
                &args[1]
            )
        ) {
            // !!! Couldn't prep cif...how is the freeing of the CIF managed??
            //
            ret = FALSE;
        }
    }

    if (is_callback) {
        INIT_RIN_CLOSURE(r, ffi_closure_alloc(
            sizeof(ffi_closure), &RIN_DISPATCHER(r)
        ));
        if (RIN_CLOSURE(r) == NULL) {
            ret = FALSE;
        }
        else {
            ffi_status status;

            status = ffi_prep_closure_loc(
                RIN_CLOSURE(r),
                RIN_CIF(r),
                callback_dispatcher,
                r,
                RIN_DISPATCHER(r)
            );

            if (status != FFI_OK) {
                ret = FALSE;
            }
        }
    }

    return ret;
}


//
//  make-routine: native [
//
//  {Native for creating the FUNCTION! for what was once ROUTINE!}
//
//      def [block!]
//  ]
//
REBNATIVE(make_routine)
{
    PARAM(1, def);

    const REBOOL is_callback = FALSE;

    MT_Routine(D_OUT, ARG(def), SPECIFIED, is_callback);

    return R_OUT;
}


//
//  make-callback: native [
//
//  {Native for creating the FUNCTION! for what was once CALLBACK!}
//
//      def [block!]
//  ]
//
REBNATIVE(make_callback)
{
    PARAM(1, def);

    const REBOOL is_callback = TRUE;

    MT_Routine(D_OUT, ARG(def), SPECIFIED, is_callback);

    return R_OUT;
}
