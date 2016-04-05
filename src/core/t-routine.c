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

#if !defined(HAVE_LIBFFI_AVAILABLE)

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
#endif


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


//
// Writes into `out` a Rebol value representing the "schema", which describes
// either a basic FFI type or the layout of a STRUCT! (not including data).
//
// Ideally this would be an OBJECT! or user-defined type specification of
// some kind (which is why it's being set up as a value).  However, for
// now it is either an INTEGER! representing an FFI_TYPE -or- a HANDLE!
// containing a REBSER* with one Struct_Field definition in it.  (It's in
// a series in order to allow it to be shared and GC'd among many struct
// instances, or extracted like this).
//
static void Schema_From_Block_May_Fail(
    REBVAL *schema_out, // => INTEGER! or HANDLE! for struct
    REBVAL *param_out, // => TYPESET!
    const REBVAL *blk
){
    if (!IS_BLOCK(blk) || VAL_LEN_AT(blk) == 0)
        fail (Error_Invalid_Arg(blk));

    Val_Init_Typeset(param_out, 0, SYM_0);

    RELVAL *item = VAL_ARRAY_AT(blk);

    if (IS_WORD(item) && VAL_WORD_CANON(item) == SYM_STRUCT_TYPE) {
        //
        // [struct! [...struct definition...]]

        ++item;
        if (IS_END(item) || !IS_BLOCK(item))
            fail (Error_Invalid_Arg(blk));

        // Use the block spec to build a temporary structure through the same
        // machinery that implements `make struct! [...]`

        REBVAL temp;
        if (!MT_Struct(&temp, item, VAL_SPECIFIER(blk), REB_STRUCT))
            fail (Error_Invalid_Arg(blk));

        assert(IS_STRUCT(&temp));

        // We want the schema series (not just the raw Struct_Field*).  This
        // is because what's needed is a GC-protecting reference, otherwise
        // it would go bad after temp's REBSTU gets GC'd.
        //
        // !!! It should be made possible to create a schema without going
        // through a struct creation.  There are "raw" structs with no memory,
        // which would avoid the data series (not the REBSTU array, though)
        //
        SET_HANDLE_DATA(
            schema_out,
            ARR_SERIES(VAL_STRUCT(&temp))->misc.schema
        );

        // Saying "struct!" is legal would suggest any structure is legal.
        // However, when the routine is called it gets a chance to look at
        // the specifics.
        //
        // !!! Original code didn't check anything--size checking added.
        //
        TYPE_SET(param_out, REB_STRUCT);
        return;
    }

    if (VAL_LEN_AT(blk) != 1)
        fail (Error_Invalid_Arg(blk));

    if (IS_WORD(item)) {
        switch (VAL_WORD_CANON(item)) {
        case SYM_VOID:
            SET_BLANK(schema_out); // only valid for return types
            break;

        case SYM_UINT8:
            SET_INTEGER(schema_out, FFI_TYPE_UINT8);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_INT8:
            SET_INTEGER(schema_out, FFI_TYPE_SINT8);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_UINT16:
            SET_INTEGER(schema_out, FFI_TYPE_UINT16);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_INT16:
            SET_INTEGER(schema_out, FFI_TYPE_SINT16);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_UINT32:
            SET_INTEGER(schema_out, FFI_TYPE_UINT32);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_INT32:
            SET_INTEGER(schema_out, FFI_TYPE_SINT32);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_UINT64:
            SET_INTEGER(schema_out, FFI_TYPE_UINT64);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_INT64:
            SET_INTEGER(schema_out, FFI_TYPE_SINT64);
            TYPE_SET(param_out, REB_INTEGER);
            break;

        case SYM_FLOAT:
            SET_INTEGER(schema_out, FFI_TYPE_FLOAT);
            TYPE_SET(param_out, REB_DECIMAL);
            break;

        case SYM_DOUBLE:
            SET_INTEGER(schema_out, FFI_TYPE_DOUBLE);
            TYPE_SET(param_out, REB_DECIMAL);
            break;

        case SYM_POINTER:
            SET_INTEGER(schema_out, FFI_TYPE_POINTER);
            TYPE_SET(param_out, REB_INTEGER);
            TYPE_SET(param_out, REB_STRING);
            TYPE_SET(param_out, REB_BINARY);
            TYPE_SET(param_out, REB_VECTOR);
            TYPE_SET(param_out, REB_FUNCTION); // callback
            break;

        default:
            fail (Error(RE_MISC));
        }
        return;
    }

    fail (Error_Invalid_Arg(blk));
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
    REBSER *store,
    const REBVAL *arg,
    const REBVAL *schema,
    const REBVAL *param
){
    REBUPT offset;

#if !defined(NDEBUG)
    if (param)
        assert(arg != NULL && IS_TYPESET(param));
    else
        assert(arg == NULL); // return value, so just make space (no arg data)
#endif

    struct Reb_Frame *frame_ = FS_TOP; // So you can use the D_xxx macros

    if (IS_HANDLE(schema)) {
        struct Struct_Field *top
            = SER_HEAD(
                struct Struct_Field,
                cast(REBSER*, VAL_HANDLE_DATA(schema))
            );

        assert(top->type == FFI_TYPE_STRUCT);

        // !!! In theory a struct has to be aligned to its maximal alignment
        // needed by a fundamental member.  We'll assume that the largest
        // is sizeof(void*) here...this may waste some space in the padding
        // between arguments, but that shouldn't have any semantic effect.
        //
        REBYTE *dest = Expand_And_Align_Core(
            &offset,
            sizeof(void*),
            store,
            top->size
        );

        if (arg == NULL) {
            //
            // Return values don't have an incoming argument to fill into the
            // calling frame.
            //
            return offset;
        }

        // !!! There wasn't any compatibility checking here before (not even
        // that the arg was a struct.  :-/  It used a stored STRUCT! from
        // when the routine was specified to know what the size should be,
        // and didn't pay attention to the size of the passed-in struct.
        //
        // (One reason it didn't use the size of the passed-struct is
        // because it couldn't do so in the return case where arg was null)

        if (!IS_STRUCT(arg))
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        if (STU_SIZE(VAL_STRUCT(arg)) != top->size)
            fail (Error_Arg_Type(D_LABEL_SYM, param, VAL_TYPE(arg)));

        memcpy(
            dest,
            SER_AT(REBYTE, VAL_STRUCT_DATA_BIN(arg), VAL_STRUCT_OFFSET(arg)),
            STU_SIZE(VAL_STRUCT(arg))
        );

        return offset;
    }

    assert(IS_INTEGER(schema));

    switch (VAL_INT32(schema)) {
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
            REBYTE *raw_ptr = VAL_RAW_DATA_AT(arg);
            memcpy(dest, &raw_ptr, sizeof(raw_ptr)); // copies a *pointer*!
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
        //
        // structs should be processed above by the HANDLE! case, not INTEGER!
        //
        assert(FALSE);
    case FFI_TYPE_VOID:
        //
        // can't return a meaningful offset for "void"--it's only valid for
        // return types, so caller should check and not try to pass it in.
        //
        assert(FALSE);
    default:
        fail (Error_Invalid_Arg(arg));
    }

    return offset;
}


/* convert the return value to rebol
 */
static void ffi_to_rebol(
    REBVAL *out,
    const REBVAL *schema,
    void *ffi_rvalue
) {
    if (IS_HANDLE(schema)) {
        struct Struct_Field *top
            = SER_HEAD(
                struct Struct_Field,
                cast(REBSER*, VAL_HANDLE_DATA(schema))
            );

        assert(top->type == FFI_TYPE_STRUCT);

        REBSTU *stu = Make_Singular_Array(VOID_CELL);
        ARR_SERIES(stu)->misc.schema = cast(REBSER*, VAL_HANDLE_DATA(schema));
        MANAGE_ARRAY(stu);

        REBSER *data = Make_Series(top->size, sizeof(REBYTE), MKS_NONE);
        memcpy(SER_HEAD(REBYTE, data), ffi_rvalue, top->size);
        MANAGE_SERIES(data);

        VAL_RESET_HEADER(out, REB_STRUCT);
        out->payload.structure.stu = stu;
        out->payload.structure.data = data;
        out->payload.structure.offset = 0;

        *ARR_HEAD(stu) = *out; // save canon value
        assert(ARR_LEN(stu) == 1); // should be automatic for singulars
        assert(STU_DATA_BIN(stu) == data);
        return;
    }

    assert(IS_INTEGER(schema));

    switch (VAL_INT32(schema)) {
    case FFI_TYPE_UINT8:
        SET_INTEGER(out, *cast(u8*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT8:
        SET_INTEGER(out, *cast(i8*, ffi_rvalue));
        break;

    case FFI_TYPE_UINT16:
        SET_INTEGER(out, *cast(u16*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT16:
        SET_INTEGER(out, *cast(i16*, ffi_rvalue));
        break;

    case FFI_TYPE_UINT32:
        SET_INTEGER(out, *cast(u32*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT32:
        SET_INTEGER(out, *cast(i32*, ffi_rvalue));
        break;

    case FFI_TYPE_UINT64:
        SET_INTEGER(out, *cast(u64*, ffi_rvalue));
        break;

    case FFI_TYPE_SINT64:
        SET_INTEGER(out, *cast(i64*, ffi_rvalue));
        break;

    case FFI_TYPE_POINTER:
        SET_INTEGER(out, cast(REBUPT, *cast(void**, ffi_rvalue)));
        break;

    case FFI_TYPE_FLOAT:
        SET_DECIMAL(out, *cast(float*, ffi_rvalue));
        break;

    case FFI_TYPE_DOUBLE:
        SET_DECIMAL(out, *cast(double*, ffi_rvalue));
        break;

    case FFI_TYPE_VOID:
        assert(FALSE); // not covered by generic routine.
    default:
        assert(FALSE);
        //
        // !!! Was reporting Error_Invalid_Arg on uninitialized `out`
        //
        fail (Error(RE_MISC));
    }
}


//
//  Routine_Dispatcher: C
//
REB_R Routine_Dispatcher(struct Reb_Frame *f)
{
    REBRIN *rin = FUNC_ROUTINE(f->func);

    if (RIN_LIB(rin) == NULL) {
        //
        // lib is NULL when routine is constructed from address directly,
        // so there's nothing to track whether that gets loaded or unloaded
    }
    else {
        if (GET_LIB_FLAG(RIN_LIB(rin), LIB_FLAG_CLOSED))
            fail (Error(RE_BAD_LIBRARY));
    }

    REBCNT num_fixed = RIN_NUM_FIXED_ARGS(rin);

    REBCNT num_variable;
    REBDSP dsp_orig = DSP; // variadic args pushed to stack, so save base ptr

    if (NOT(GET_RIN_FLAG(rin, ROUTINE_FLAG_VARIADIC)))
        num_variable = 0;
    else {
        // The function specification should have one extra parameter for
        // the variadic source ("...")
        //
        assert(FUNC_NUM_PARAMS(FRM_FUNC(f)) == num_fixed + 1);

        REBVAL *varparam = FUNC_PARAM(FRM_FUNC(f), num_fixed + 1); // 1-based
        REBVAL *vararg = FRM_ARG(f, num_fixed + 1); // 1-based
        assert(
            GET_VAL_FLAG(varparam, TYPESET_FLAG_VARIADIC)
            && IS_VARARGS(vararg)
            && !GET_VAL_FLAG(vararg, VARARGS_FLAG_NO_FRAME)
        );

        // Evaluate the VARARGS! feed of values to the data stack.  This way
        // they will be available to be counted, to know how big to make the
        // FFI argument series.
        //
        REBARR *feed = CTX_VARLIST(VAL_VARARGS_FRAME_CTX(vararg));
        do {
            REBIXO indexor = Do_Vararg_Op_Core(
                f->out, feed, varparam, vararg, SYM_0, VARARG_OP_TAKE
            );
            if (indexor == THROWN_FLAG) {
                assert(THROWN(f->out));
                return R_OUT_IS_THROWN;
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

        num_variable = (DSP - dsp_orig) / 2;
    }

    REBCNT num_args = num_fixed + num_variable;

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

    void *ret_offset;
    if (!IS_BLANK(RIN_RET_SCHEMA(rin))) {
        ret_offset = cast(void*, arg_to_ffi(
            store, // ffi-converted arg appended here
            NULL, // arg: none (we're only making space--leave uninitialized)
            RIN_RET_SCHEMA(rin),
            NULL // param: none (it's a return value/output)
        ));
    };

    REBSER *arg_offsets;
    if (num_args == 0)
        arg_offsets = NULL; // don't waste time with the alloc + free
    else
        arg_offsets = Make_Series(num_args, sizeof(void*), MKS_NONE);

    REBCNT i = 0;

    // First gather the fixed parameters from the frame (known to be
    // of correct types--they were checked by Do_Core() before this point.)
    //
    for (; i < num_fixed; ++i) {
        *SER_AT(void*, arg_offsets, i) = cast(void*, arg_to_ffi(
            store, // ffi-converted arg appended here
            FRM_ARG(f, i + 1), // 1-based
            RIN_ARG_SCHEMA(rin, i), // 0-based
            FUNC_PARAM(FRM_FUNC(f), i + 1) // 1-based
        ));
    }

    // If an FFI routine takes a fixed number of arguments, then its Call
    // InterFace (CIF) can be created just once.  This will be in the RIN_CIF.
    // However a variadic routine requires a CIF that matches the number
    // and types of arguments for that specific call.  This CIF variable will
    // be set to the RIN_CIF if it exists already--or to a dynamically
    // allocated CIF for the varargs case (which will need to be freed).
    //
    REBSER *cif; // one ffi_cif element (in a REBSER for GC on fail())
    REBSER *args_fftypes; // list of ffi_type* if num_variable > 0

    if (num_variable == 0) {
        cif = rin->cif;
    }
    else {
        assert(rin->cif == NULL);

        // CIF creation requires a C array of argument descriptions that is
        // contiguous across both the fixed and variadic parts.  Start by
        // filling in the ffi_type*s for all the fixed args.
        //
        args_fftypes = Make_Series(
            num_fixed + num_variable,
            sizeof(ffi_type*),
            MKS_NONE
        );

        for (i = 0; i < num_fixed; ++i)
            *SER_AT(ffi_type*, args_fftypes, i)
                = SCHEMA_FFTYPE(RIN_ARG_SCHEMA(rin, i));

        REBDSP dsp;
        for (dsp = dsp_orig + 1; i < num_args; dsp += 2, ++i) {
            //
            // This param is used with the variadic type spec, and is
            // initialized as it would be for an ordinary FFI argument.  This
            // means its allowed type flags are set, which is not really
            // necessary.  Whatever symbol name is used here will be seen
            // in error reports.
            //
            REBVAL schema;
            REBVAL param;
            Schema_From_Block_May_Fail(
                &schema,
                &param, // sets type bits in param
                DS_AT(dsp + 1) // will error if this is not a block
            );

            *SER_AT(ffi_type*, args_fftypes, i) = SCHEMA_FFTYPE(&schema);

            VAL_TYPESET_SYM_INIT(&param, SYM_ELLIPSIS);

            *SER_AT(void*, arg_offsets, i) = cast(void*, arg_to_ffi(
                store, // data appended to store
                DS_AT(dsp), // arg
                &schema,
                &param // used for typecheck, VAL_TYPESET_SYM for error msgs
            ));
        }

        DS_DROP_TO(dsp_orig); // done w/args (converted to bytes in `store`)

        cif = Make_Series(1, sizeof(ffi_cif), MKS_NONE);

        ffi_status status = ffi_prep_cif_var( // "_var"-iadic prep_cif version
            SER_HEAD(ffi_cif, cif),
            RIN_ABI(rin),
            num_fixed, // just fixed
            num_args, // fixed plus variable
            IS_BLANK(RIN_RET_SCHEMA(rin))
                ? &ffi_type_void
                : SCHEMA_FFTYPE(RIN_RET_SCHEMA(rin)), // return FFI type
            SER_HEAD(ffi_type*, args_fftypes) // arguments FFI types
        );

        if (status != FFI_OK) {
            //RL_Print("Couldn't prep CIF_VAR\n");
            fail (Error(RE_MISC));
        }
    }

    // Now that all the additions to store have been made, we want to change
    // the offsets of each FFI argument into actual pointers (since the
    // data won't be relocated)
    {
        if (IS_BLANK(RIN_RET_SCHEMA(rin)))
            ret_offset = NULL;
        else
            ret_offset = SER_DATA_RAW(store) + cast(REBUPT, ret_offset);

        REBCNT i;
        for (i = 0; i < num_args; ++i) {
            REBUPT off = cast(REBUPT, *SER_AT(void*, arg_offsets, i));
            assert(off == 0 || off < SER_LEN(store));
            *SER_AT(void*, arg_offsets, i) = SER_DATA_RAW(store) + off;
        }
    }

    // ** THE ACTUAL FFI CALL **
    //
    // Note that the "offsets" are now actually pointers.  Also note that
    // there is no mechanism to "throw" a Rebol value across an FFI boundary.
    // If you could (e.g. by tunneling up through a callback somehow) this
    // would set `f->mode = CALL_MODE_THROW_PENDING`.
    {
        SET_VOID(&Callback_Error); // !!! guarantee it's already void?

        ffi_call(
            SER_HEAD(ffi_cif, cif),
            RIN_FUNCPTR(rin),
            ret_offset, // actually a real pointer now (no longer an offset)
            (num_args == 0)
                ? NULL
                : SER_HEAD(void*, arg_offsets) // also real pointers now
        );

        if (!IS_VOID(&Callback_Error))
            fail (VAL_CONTEXT(&Callback_Error)); // asserts if not ERROR!
    }

    if (IS_BLANK(RIN_RET_SCHEMA(rin)))
        SET_VOID(f->out);
    else
        ffi_to_rebol(f->out, RIN_RET_SCHEMA(rin), ret_offset);

    if (num_args != 0)
        Free_Series(arg_offsets);

    Free_Series(store);

    if (num_variable != 0) {
        Free_Series(cif);
        Free_Series(args_fftypes);
    }

    // Note: cannot "throw" a Rebol value across an FFI boundary.

    assert(!THROWN(f->out));
    return R_OUT;
}


//
//  Free_Routine: C
//
void Free_Routine(REBRIN *rin)
{
    CLEAR_RIN_FLAG(rin, ROUTINE_FLAG_MARK);
    if (GET_RIN_FLAG(rin, ROUTINE_FLAG_CALLBACK))
        ffi_closure_free(RIN_CLOSURE(rin));

    // cif and ffargs are GC-managed, will free themselves

    Free_Node(RIN_POOL, (REBNOD*)rin);
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
    assert(!GET_RIN_FLAG(rin, ROUTINE_FLAG_VARIADIC));
    assert(cif->nargs == RIN_NUM_FIXED_ARGS(rin));

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

    // Build an array of code to run which represents the call.  The first
    // item in that array will be the callback function value, and then
    // the arguments will be the remaining values.
    //
    REBARR *code = Make_Array(1 + cif->nargs);

    RELVAL *elem = ARR_HEAD(code);
    *elem = *FUNC_VALUE(RIN_CALLBACK_FUNC(rin));
    ++elem;

    REBCNT i;
    for (i = 0; i < cif->nargs; ++i, ++elem)
        ffi_to_rebol(SINK(elem), RIN_ARG_SCHEMA(rin, i), args[i]);

    SET_ARRAY_LEN(code, 1 + cif->nargs);
    TERM_ARRAY(code);

    // !!! Currently an array must be managed in order to use it with DO,
    // because the series could be put into a block of a backtrace.  It will
    // be guarded implicitly during the Do_At_Throws(), however.
    //
    MANAGE_ARRAY(code);

    REBVAL result;
    if (Do_At_Throws(&result, code, 0, SPECIFIED)) {
        //
        // !!! Does not check for thrown cases...what should this
        // do in case of THROW, BREAK, QUIT?
        //
        fail (Error_No_Catch_For_Throw(&result));
    }

    // !!! Could Free_Array(array) if not managed to use with DO, instead
    // the GC will free it when it gets around to it.

    // !!! The below code is redundant with arg_to_ffi...however that code
    // is set up to work with series memory, not an already made buffer
    // of the correct size.  The arg_to_ffi might be able to have the
    // "store" created to the right size up front.  Merge these when
    // possible.

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
            VAL_STRUCT_SIZE(&result)
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
    if (!IS_BLOCK(data))
        return FALSE;

    RELVAL *blk = NULL;
    REBCNT eval_idx = 0; /* for spec block evaluation */
    CFUNC *func = NULL;
    REBCNT n = 0; // args start at 0 (return value is separate, RET_FFTYPE)
    REBCNT has_return = 0;
    REBCNT has_abi = 0;
    REBVAL *temp;

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

    out->payload.function.func = AS_FUNC(Make_Array(N_ARGS));

    // first slot is reserved for the "self", see `struct Reb_Func`
    //
    temp = Alloc_Tail_Array(FUNC_PARAMLIST(out->payload.function.func));
    *temp = *out;

    // NONE! in the ret schema slot means ffi_type_void (the default if no
    // return: is specified).  It is not generally a legal "schema", but
    // used in the return slot because it's special (allowed to be void)
    //
    INIT_CELL_WRITABLE_IF_DEBUG(RIN_RET_SCHEMA(r));
    SET_BLANK(RIN_RET_SCHEMA(r));

    INIT_RIN_ABI(r, FFI_DEFAULT_ABI);
    RIN_LIB(r) = NULL;

    blk = VAL_ARRAY_AT(data);

    MANAGE_ARRAY(VAL_FUNC_PARAMLIST(out));

    r->args_schemas = Make_Array(N_ARGS);
    MANAGE_ARRAY(r->args_schemas);

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
            REBSYM sym = VAL_WORD_SYM(blk);

            if (SAME_SYM(sym, SYM_ELLIPSIS)) {
                if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
                    // duplicate ellipsis
                    fail (Error_Invalid_Arg(KNOWN(blk)));
                }

                SET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC);

                REBVAL *param = Alloc_Tail_Array(VAL_FUNC_PARAMLIST(out));

                // Currently the rule is that if VARARGS! is itself a valid
                // parameter type, then the varargs will not chain.  We want
                // chaining as opposed to passing the parameter pack to the
                // C code to process (it wouldn't know what to do with it)
                //
                Val_Init_Typeset(
                    param,
                    ALL_64 & ~FLAGIT_KIND(REB_VARARGS),
                    SYM_VARARGS
                );
                SET_VAL_FLAG(param, TYPESET_FLAG_VARIADIC);
                INIT_VAL_PARAM_CLASS(param, PARAM_CLASS_NORMAL);
            }
            else {
                if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
                    //... has to be the last argument
                    fail (Error_Invalid_Arg(KNOWN(blk)));
                }

                REBVAL *param = Alloc_Tail_Array(VAL_FUNC_PARAMLIST(out));

                ++blk;
                Schema_From_Block_May_Fail(
                    Alloc_Tail_Array(r->args_schemas), // schema (out)
                    param, // param (out)
                    KNOWN(blk) // block (in)
                );

                VAL_TYPESET_SYM_INIT(param, sym);
                INIT_VAL_PARAM_CLASS(param, PARAM_CLASS_NORMAL);
                ++n;
            }
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

                REBVAL param;
                Schema_From_Block_May_Fail(
                    RIN_RET_SCHEMA(r),
                    &param, // dummy (a return/output has no arg to typecheck)
                    KNOWN(blk)
                );
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

    REBCNT num_fixed = n;
    SET_ARRAY_LEN(r->args_schemas, num_fixed);
    TERM_ARRAY(r->args_schemas);
    ASSERT_ARRAY(r->args_schemas);

    if (GET_RIN_FLAG(r, ROUTINE_FLAG_VARIADIC)) {
        //
        // Each individual call needs to use `ffi_prep_cif_var` to make the
        // proper variadic CIF for that call.
        //
        r->cif = NULL;
        r->args_fftypes = NULL;
    }
    else {
        // The same CIF can be used for every call of the routine if it is
        // not variadic.  The fftypes array pointer used must stay alive
        // for the entire the lifetime of the CIF, apparently :-/
        //
        r->cif = Make_Series(1, sizeof(ffi_cif), MKS_NONE);

        if (num_fixed == 0)
            r->args_fftypes = NULL; // 0 size series illegal (others wasteful)
        else
            r->args_fftypes = Make_Series(
                num_fixed, sizeof(ffi_type*), MKS_NONE
            );

        REBCNT i;
        for (i = 0; i < num_fixed; ++i)
            *SER_AT(ffi_type*, r->args_fftypes, i)
                = SCHEMA_FFTYPE(RIN_ARG_SCHEMA(r, i));

        if (
            FFI_OK != ffi_prep_cif(
                SER_HEAD(ffi_cif, r->cif),
                RIN_ABI(r),
                num_fixed,
                IS_BLANK(RIN_RET_SCHEMA(r))
                    ? &ffi_type_void
                    : SCHEMA_FFTYPE(RIN_RET_SCHEMA(r)),
                (r->args_fftypes == NULL)
                    ? NULL
                    : SER_HEAD(ffi_type*, r->args_fftypes)
            )
        ) {
            fail (Error(RE_MISC)); // !!! Couldn't prep cif...
        }

        MANAGE_SERIES(r->cif);
        if (r->args_fftypes)
            MANAGE_SERIES(r->args_fftypes); // must have same lifetime as cif
    }

    if (is_callback) {
        ffi_closure* closure = cast(ffi_closure*, ffi_closure_alloc(
            sizeof(ffi_closure), &RIN_DISPATCHER(r)
        ));

        if (closure == NULL)
            fail (Error(RE_MISC)); // couldn't allocate closure

        INIT_RIN_CLOSURE(r, closure);

        ffi_status status;

        if (
            FFI_OK != ffi_prep_closure_loc(
                RIN_CLOSURE(r),
                SER_HEAD(ffi_cif, r->cif),
                callback_dispatcher,
                r,
                RIN_DISPATCHER(r)
            )
        ) {
            fail (Error(RE_MISC)); // couldn't prep closure
        }
    }

    return TRUE;
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
