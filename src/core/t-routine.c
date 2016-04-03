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


//
// According to the libffi documentation, the arguments "must be suitably
// aligned; it is the caller's responsibility to ensure this".
//
// We assume the store's data pointer will have suitable alignment for any
// type (currently Make_Series() is expected to match malloc() in this way).
// This will round the offset positions to an alignment appropriate for the
// type size given.
//
// This means sequential arguments in the store may have padding between them.
//
inline static REBYTE *Expand_And_Align_Core(
    REBUPT *offset_out,
    REBCNT align,
    REBSER *store,
    REBCNT size
){
    REBCNT padding = SER_LEN(store) % align;
    if (padding != 0)
        padding = align - padding;

    *offset_out = SER_LEN(store) + padding;
    EXPAND_SERIES_TAIL(store, padding + size);
    return SER_DATA_RAW(store) + *offset_out;
}

inline static REBYTE *Expand_And_Align(
    REBUPT *offset_out,
    REBSER *store,
    REBCNT size // assumes align == size
){
    return Expand_And_Align_Core(offset_out, size, store, size);
}


/* make a copy of the argument
 * arg referes to return value when idx = 0
 * function args start from idx = 1
 *
 * @ptrs is an array with a length of number of arguments of @rot
 *
 * */
static REBUPT arg_to_ffi(
    REBRIN *r,
    REBVAL *param,
    REBVAL *arg, // will be null if return value (just make space--no data)
    ffi_type *arg_ffi_type,
    REBSER *store
){
    REBUPT offset;

#if !defined(NDEBUG)
    if (param)
        assert(arg != NULL && IS_TYPESET(param));
    else
        assert(arg == NULL);
#endif

    struct Reb_Frame *frame_ = FS_TOP; // So you can use the D_xxx macros

    switch (arg_ffi_type->type) {
    case FFI_TYPE_UINT8:
        {
        u8 u;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(u));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        u = cast(u8, VAL_INT64(arg));
        memcpy(dest, &u, sizeof(u));
        }
        break;

    case FFI_TYPE_SINT8:
        {
        i8 i;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(i));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        i = cast(i8, VAL_INT64(arg));
        memcpy(dest, &i, sizeof(i));
        }
        break;

    case FFI_TYPE_UINT16:
        {
        u16 u;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(u));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        u = cast(u16, VAL_INT64(arg));
        memcpy(dest, &u, sizeof(u));
        }
        break;

    case FFI_TYPE_SINT16:
        {
        i16 i;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(i));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        i = cast(i16, VAL_INT64(arg));
        memcpy(dest, &i, sizeof(i));
        }
        break;

    case FFI_TYPE_UINT32:
        {
        u32 u;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(u));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        u = cast(u32, VAL_INT64(arg));
        memcpy(dest, &u, sizeof(u));
        }
        break;

    case FFI_TYPE_SINT32:
        {
        i32 i;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(i));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        i = cast(i32, VAL_INT64(arg));
        memcpy(dest, &i, sizeof(i));
        }
        break;

    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
        {
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(REBI64));
        if (!arg) break;

        if (!IS_INTEGER(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        memcpy(dest, &VAL_INT64(arg), sizeof(REBI64));
        }
        break;

    case FFI_TYPE_POINTER: {
        //
        // Note: Function pointers and data pointers may not be same size.
        //
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(void*));
        if (!arg) break;

        switch (VAL_TYPE(arg)) {
        case REB_INTEGER:
            {
            REBIPT ipt = VAL_INT64(arg); // REBIPT is like C99's intptr_t
            memcpy(dest, &VAL_INT64(arg), sizeof(void*));
            }
            break;

        case REB_STRING:
        case REB_BINARY:
        case REB_VECTOR:
            {
            // !!! This is a questionable idea, giving out pointers directly
            // into Rebol series data.  One issue is that the recipient of
            // the data doesn't know whether to interpret it as REBYTE[] or as
            // REBUNI[]...because it's passing the raw data of strings which
            // can be wide or not based on things that have happened in the
            // lifetime of that string.  Another is that the data may be
            // relocated in memory if any modifications happen during a
            // callback...so the memory is not "stable".
            //
            REBYTE *raw = VAL_RAW_DATA_AT(arg);
            memcpy(dest, &raw, sizeof(raw));
            }
            break;

        case REB_FUNCTION: {
            if (!GET_RIN_FLAG(VAL_FUNC_ROUTINE(arg), ROUTINE_FLAG_CALLBACK))
                fail (Error(RE_ONLY_CALLBACK_PTR));

            void* dispatcher = RIN_DISPATCHER(VAL_FUNC_ROUTINE(arg));
            memcpy(dest, &dispatcher, sizeof(dispatcher));
            break; }

        default:
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));
        }
        break; } // end case FFI_TYPE_POINTER

    case FFI_TYPE_FLOAT:
        {
        float f;
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(f));
        if (!arg) break;

        if (!IS_DECIMAL(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        f = cast(float, VAL_DECIMAL(arg));
        memcpy(dest, &f, sizeof(f));
        }
        break;

    case FFI_TYPE_DOUBLE:
        {
        REBYTE *dest = Expand_And_Align(&offset, store, sizeof(double));
        if (!arg) break;

        if (!IS_DECIMAL(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        memcpy(dest, &VAL_DECIMAL(arg), sizeof(double));
        }
        break;

    case FFI_TYPE_STRUCT:
        {
        // !!! In theory a struct has to be aligned to its maximal alignment
        // needed by a fundamental member.  We'll assume that the largest
        // is sizeof(void*) here...this may waste some space in the padding
        // between arguments, but that shouldn't have any semantic effect.
        //
        REBYTE *dest = Expand_And_Align_Core(
            &offset,
            sizeof(void*),
            store,
            (arg == NULL)
                ? STRUCT_LEN(&RIN_RVALUE(r))
                : STRUCT_LEN(&VAL_STRUCT(arg))
        );
        if (!arg) break;

        memcpy(
            dest,
            SER_AT(REBYTE, VAL_STRUCT_DATA_BIN(arg), VAL_STRUCT_OFFSET(arg)),
            STRUCT_LEN(&VAL_STRUCT(arg))
        );
        }
        break;

    case FFI_TYPE_VOID:
        {
        if (arg)
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        offset = 0; // used to return NULL, is a signal needed for that?
        break;
        }

    default:
        fail (Error_Invalid_Arg(arg));
    }

    return offset;
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
        SET_VOID(rebol_ret);
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

    if (RIN_LIB(r) == NULL) {
        //
        // lib is NULL when routine is constructed from address directly,
        // so there's nothing to track whether that gets loaded or unloaded
    }
    else {
        if (GET_LIB_FLAG(RIN_LIB(r), LIB_FLAG_CLOSED))
            fail (Error(RE_BAD_LIBRARY));
    }

    ffi_type *rtype = *SER_HEAD(ffi_type*, RIN_FFI_ARG_TYPES(r));

    // The FFI arguments are passed by void*.  Those void pointers point to
    // transformations of the Rebol arguments into ranges of memory of
    // various sizes.  This is the backing store for those arguments, which
    // is appended to for each one.  The memory is freed after the call.
    //
    // The offsets array has one element for each argument.  These point at
    // indexes of where each FFI variable resides.  Offsets are used instead
    // of pointers in case the store has to be resized, which may move the
    // base of the series.  Hence the offsets must be mutated into pointers
    // at the last minute before the FFI call.
    //
    REBSER *store = Make_Series(1, sizeof(REBYTE), MKS_NONE);
    REBSER *offsets;
    REBCNT num_args;

    // If an FFI routine takes a fixed number of arguments, then its Call
    // InterFace (CIF) can be created just once.  This will be in the RIN_CIF.
    // However a variadic routine requires a CIF that matches the number
    // and types of arguments for that specific call.  This CIF variable will
    // be set to the RIN_CIF if it exists already--or to a dynamically
    // allocated CIF for the varargs case (which will need to be freed).
    //
    ffi_cif *cif;
    REBARR *vararray;

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
        //
        // Last typeset should have been made variadic when routine was made
        //
        REBVAL *vararg = FRM_ARG(f, FRM_NUM_ARGS(f));
        assert(IS_VARARGS(vararg));

        REBCNT num_fixed = FRM_NUM_ARGS(f) - 1; // don't include the varargs

        // Evaluate the VARARGS! feed of values to the data stack.  This way
        // they will be available to be counted, to know how big to make the
        // FFI argument series.
        //
        // !!! Duplicated code exists in MAKE BLOCK! from a VARARGS!

        REBDSP dsp_orig = DSP;

        REBVAL fake_param;

        REBARR *feed;
        const REBVAL *vararg_param;
        REBVAL *vararg_arg;
        if (GET_VAL_FLAG(vararg, VARARGS_FLAG_NO_FRAME)) {
            feed = VAL_VARARGS_ARRAY1(vararg);

            // Just a vararg created from a block, so no typeset or quoting
            // settings available.  Make a fake parameter that hard quotes
            // and takes any type (it will be type checked if in a chain).
            //
            Val_Init_Typeset(&fake_param, ALL_64, SYM_ELLIPSIS);
            INIT_VAL_PARAM_CLASS(&fake_param, PARAM_CLASS_HARD_QUOTE);
            vararg_param = &fake_param;
            vararg_arg = &fake_param; // doesn't matter
        }
        else {
            feed = CTX_VARLIST(VAL_VARARGS_FRAME_CTX(vararg));
            vararg_param = VAL_VARARGS_PARAM(vararg);
            vararg_arg = VAL_VARARGS_ARG(vararg);
        }

        do {
            REBIXO indexor = Do_Vararg_Op_Core(
                f->out, feed, vararg_param, vararg_arg, SYM_0, VARARG_OP_TAKE
            );
            if (indexor == THROWN_FLAG) {
                return TRUE;
            }
            if (indexor == END_FLAG)
                break;

            DS_PUSH(f->out);
        } while (TRUE);

        // !!! The Atronix va_list interface required a type to be specified
        // for each argument--achieving what you would get if you used a
        // C cast on each variadic argument.  Such as:
        //
        //     printf reduce ["%d, %f" 10 + 20 [int32] 12.34 [float]]
        //
        // While this provides generality, it may be useful to use defaulting
        // like C's where integer types default to `int` and floating point
        // types default to `double`.  In the VARARGS!-based syntax it could
        // offer several possibilities:
        //
        //     (printf "%d, %f" (10 + 20) 12.34)
        //     (printf "%d, %f" [int32 10 + 20] 12.34)
        //     (printf "%d, %f" [int32] 10 + 20 [float] 12.34)
        //
        // For the moment, this is following the idea that there must be
        // pairings of values and then blocks (though the values are evaluated
        // expressions).
        //
        if ((DSP - dsp_orig) % 2 != 0)
            fail (Error(RE_MISC));

        REBCNT num_variable = (DSP - dsp_orig) / 2;
        num_args = num_fixed + num_variable;

        offsets = Make_Series(num_args + 1, sizeof(void*), MKS_NONE);

        // reset length
        SET_SERIES_LEN(RIN_FFI_ARG_TYPES(r), num_fixed + 1);

        REBCNT j = 1;

        // First gather the fixed parameters from the frame (known to be
        // of correct types)

        for (; j <= num_fixed; ++j) {
            *SER_AT(void*, offsets, j) = cast(void*, arg_to_ffi(
                r,
                FUNC_PARAM(FRM_FUNC(f), j),
                FRM_ARG(f, j),
                *SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), j),
                store
            ));
        }

        REBCNT i;
        for (i = 1; j <= num_args; i += 2, ++j) {
            REBVAL param;

            // Start with no type bits initially (process_type_block will
            // add them).
            //
            // !!! Clearer name for individual variadic args than "..."?
            //
            Val_Init_Typeset(&param, 0, SYM_ELLIPSIS);

            EXPAND_SERIES_TAIL(RIN_FFI_ARG_TYPES(r), 1);
            process_type_block(
                r,
                &param, // will set the type bits to match for some reason
                DS_AT(dsp_orig + i + 1), // error if this is not a block
                j, // used to write into RIN_FFI_ARG_TYPES
                FALSE // not a return value
            );

            *SER_AT(void*, offsets, j) = cast(void*, arg_to_ffi(
                r,
                &param,
                DS_AT(dsp_orig + i),
                *SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), j),
                store
            ));
        }

        DS_DROP_TO(dsp_orig); // done w/args (converted to bytes in `store`)

        assert(j == SER_LEN(RIN_FFI_ARG_TYPES(r)));

        // Variadics must use ffi_prep_cif_var for each variadic call.  (it
        // would be possible to cache them if the same variadic signature
        // was used repeatedly)
        //
        assert(RIN_CIF(r) == NULL);
        cif = OS_ALLOC(ffi_cif);
        if (
            FFI_OK != ffi_prep_cif_var( // "_var"-iadic version of prep_cif
                cif,
                RIN_ABI(r),
                num_fixed, // just fixed
                num_args, // fixed plus variable
                rtype,
                (num_args > 1)
                    ? SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), 1)
                    : NULL
            )
        ){
            OS_FREE(cif);
            //RL_Print("Couldn't prep CIF_VAR\n");
            fail (Error(RE_MISC));
        }
    }
    else if (SER_LEN(RIN_FFI_ARG_TYPES(r)) == 1) {
        //
        // 1 means it's just a return value (no arguments)
        //
        num_args = 0;
        cif = RIN_CIF(r);
        offsets = Make_Series(1, sizeof(void*), MKS_NONE); // needs return
    }
    else {
        num_args = SER_LEN(RIN_FFI_ARG_TYPES(r)) - 1;
        assert(num_args > 0);

        cif = RIN_CIF(r);

        offsets = Make_Series(num_args + 1, sizeof(void*), MKS_NONE);

        REBCNT i;
        for (i = 1; i <= num_args; ++i) {
            *SER_AT(void*, offsets, i) = cast(void*, arg_to_ffi(
                r,
                FUNC_PARAM(FRM_FUNC(f), i),
                FRM_ARG(f, i), // 1-based access
                *SER_AT(ffi_type*, RIN_FFI_ARG_TYPES(r), i),
                store
            ));
        }
    }

    *SER_AT(void*, offsets, 0) = cast(void*, arg_to_ffi(
        r,
        NULL, // no argument to copy (will be written by the call)
        NULL,
        rtype,
        store
    ));

    // Now that all the additions to store have been made, we want to change
    // the offsets of each FFI argument into actual pointers.
    {
        REBCNT i;
        for (i = 0; i <= num_args; ++i) {
            REBUPT off = cast(REBUPT, *SER_AT(void*, offsets, i));
            assert(off == 0 || off < SER_LEN(store));
            *SER_AT(void*, offsets, i) = SER_DATA_RAW(store) + off;
        }
    }

    SET_VOID(&Callback_Error); // !!! Should we guarantee it's already void?

    ffi_call(
        cif,
        RIN_FUNCPTR(r),
        rtype->type == FFI_TYPE_VOID
            ? NULL
            : *SER_AT(void*, offsets, 0), // actually pointer now, not offset
        num_args > 0
            ? SER_AT(void*, offsets, 1) // actually pointers now, not offsets
            : NULL
    );

    if (!IS_VOID(&Callback_Error))
        fail (VAL_CONTEXT(&Callback_Error)); // asserts if not ERROR!

    ffi_to_rebol(
        r,
        rtype,
        *SER_AT(void*, offsets, 0), // still actually a pointer
        f->out
    );

    Free_Series(offsets);
    Free_Series(store);

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC))
        OS_FREE(cif);

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

                REBVAL *typeset = Alloc_Tail_Array(VAL_FUNC_PARAMLIST(out));

                // Currently the rule is that if VARARGS! is itself a valid
                // parameter type, then the varargs will not chain.  We want
                // chaining as opposed to passing the parameter pack to the
                // C code to process (it wouldn't know what to do with it)
                //
                Val_Init_Typeset(
                    typeset,
                    ALL_64 & ~FLAGIT_KIND(REB_VARARGS),
                    SYM_VARARGS
                );
                SET_VAL_FLAG(typeset, TYPESET_FLAG_VARIADIC);
                INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_NORMAL);
            }
            else {
                if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
                    //... has to be the last argument
                    fail (Error_Invalid_Arg(KNOWN(blk)));
                }

                REBVAL *typeset = Alloc_Tail_Array(VAL_FUNC_PARAMLIST(out));
                Val_Init_Typeset(typeset, 0, VAL_WORD_SYM(blk));
                EXPAND_SERIES_TAIL(RIN_FFI_ARG_TYPES(r), 1);
                INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_NORMAL);

                ++blk;
                process_type_block(
                    r, VAL_FUNC_PARAM(out, n), KNOWN(blk), n, TRUE
                );
            }

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
