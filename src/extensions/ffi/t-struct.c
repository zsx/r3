//
//  File: %t-struct.c
//  Summary: "C struct object datatype"
//  Section: datatypes
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2014 Atronix Engineering, Inc.
// Copyright 2014-2017 Rebol Open Source Contributors
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

#include "reb-struct.h"


// The managed HANDLE! for a ffi_type will have a reference in structs that
// use it.  Basic non-struct FFI_TYPE_XXX use the stock ffi_type_xxx pointers
// that do not have to be freed, so they use simple HANDLE! which do not
// register this cleanup hook.
//
static void cleanup_ffi_type(const REBVAL *v) {
    ffi_type *fftype = VAL_HANDLE_POINTER(ffi_type, v);
    if (fftype->type == FFI_TYPE_STRUCT)
        OS_FREE(fftype->elements);
    OS_FREE(fftype);
}


static void fail_if_non_accessible(const REBVAL *val)
{
    if (VAL_STRUCT_INACCESSIBLE(val)) {
        DECLARE_LOCAL (i);
        Init_Integer(i, cast(REBUPT, VAL_STRUCT_DATA_HEAD(val)));
        fail (Error_Bad_Memory_Raw(i, val));
    }
}

static void get_scalar(
    REBVAL *out,
    REBSTU *stu,
    REBFLD *field,
    REBCNT n // element index, starting from 0
){
    assert(n == 0 || FLD_IS_ARRAY(field));

    REBCNT offset =
        STU_OFFSET(stu) + FLD_OFFSET(field) + (n * FLD_WIDE(field));

    if (FLD_IS_STRUCT(field)) {
        //
        // In order for the schema to participate in GC it must be a series.
        // Currently this series is created with a single value of the root
        // schema in the case of a struct expansion.  This wouldn't be
        // necessary if each field that was a structure offered a REBSER
        // already... !!! ?? !!! ... it will be necessary if the schemas
        // are to uniquely carry an ffi_type freed when they are GC'd
        //
        REBSTU *sub_stu = Alloc_Singular_Array();
        SER(sub_stu)->link.schema = field;
        REBVAL *single = SINK(ARR_HEAD(sub_stu));

        // In this case the structure lives at an offset inside another.
        //
        // Note: The original code allowed this for STU_INACCESSIBLE(stu).
        //
        VAL_RESET_HEADER(single, REB_STRUCT);
        MANAGE_ARRAY(sub_stu);
        single->payload.structure.stu = sub_stu;

        // The parent data may be a singular array for a HANDLE! or a BINARY!
        // series, depending on whether the data is owned by Rebol or not.
        // That series pointer is being referenced again here.
        //
        single->payload.structure.data =
            ARR_HEAD(stu)->payload.structure.data;
        single->extra.struct_offset = offset;

        // With all fields initialized, assign canon value as result
        //
        Move_Value(out, single);
        assert(VAL_STRUCT_SIZE(out) == FLD_WIDE(field));
        return;
    }

    if (STU_INACCESSIBLE(stu)) {
        //
        // !!! This just gets void with no error...that seems like a bad idea,
        // if the data is truly inaccessible.
        //
        Init_Void(out);
        return;
    }

    REBYTE *p = offset + STU_DATA_HEAD(stu);

    switch (FLD_TYPE_SYM(field)) {
    case SYM_UINT8:
        Init_Integer(out, *cast(u8*, p));
        break;

    case SYM_INT8:
        Init_Integer(out, *cast(i8*, p));
        break;

    case SYM_UINT16:
        Init_Integer(out, *cast(u16*, p));
        break;

    case SYM_INT16:
        Init_Integer(out, *cast(i8*, p));
        break;

    case SYM_UINT32:
        Init_Integer(out, *cast(u32*, p));
        break;

    case SYM_INT32:
        Init_Integer(out, *cast(i32*, p));
        break;

    case SYM_UINT64:
        Init_Integer(out, *cast(u64*, p));
        break;

    case SYM_INT64:
        Init_Integer(out, *cast(i64*, p));
        break;

    case SYM_FLOAT:
        Init_Decimal(out, *cast(float*, p));
        break;

    case SYM_DOUBLE:
        Init_Decimal(out, *cast(double*, p));
        break;

    case SYM_POINTER:
        Init_Integer(out, cast(REBUPT, *cast(void**, p)));
        break;

    case SYM_REBVAL:
        Move_Value(out, cast(const REBVAL*, p));
        break;

    default:
        assert(FALSE);
        fail ("Unknown FFI type indicator");
    }
}


//
//  Get_Struct_Var: C
//
static REBOOL Get_Struct_Var(REBVAL *out, REBSTU *stu, const REBVAL *word)
{
    REBARR *fieldlist = STU_FIELDLIST(stu);

    RELVAL *item = ARR_HEAD(fieldlist);
    for (; NOT_END(item); ++item) {
        REBFLD *field = VAL_ARRAY(item);
        if (STR_CANON(FLD_NAME(field)) != VAL_WORD_CANON(word))
            continue;

        if (FLD_IS_ARRAY(field)) {
            //
            // Structs contain packed data for the field type in an array.
            // This data cannot expand or contract, and is not in a
            // Rebol-compatible format.  A Rebol Array is made by
            // extracting the information.
            //
            // !!! Perhaps a fixed-size VECTOR! could have its data
            // pointer into these arrays?
            //
            REBCNT dimension = FLD_DIMENSION(field);
            REBARR *array = Make_Array(dimension);
            REBCNT n;
            for (n = 0; n < dimension; ++n) {
                REBVAL *dest = SINK(ARR_AT(array, n));
                get_scalar(dest, stu, field, n);
            }
            TERM_ARRAY_LEN(array, dimension);
            Init_Block(out, array);
        }
        else
            get_scalar(out, stu, field, 0);

        return TRUE;
    }

    return FALSE; // word not found in struct's field symbols
}


//
//  Struct_To_Array: C
//
// Used by MOLD to create a block.
//
// Cannot fail(), because fail() could call MOLD on a struct!, which will end
// up infinitive recursive calls.
//
REBARR *Struct_To_Array(REBSTU *stu)
{
    REBARR *fieldlist = STU_FIELDLIST(stu);
    RELVAL *item = ARR_HEAD(fieldlist);

    REBDSP dsp_orig = DSP;

    // fail_if_non_accessible(STU_TO_VAL(stu));

    for(; NOT_END(item); ++item) {
        REBFLD *field = VAL_ARRAY(item);

        DS_PUSH_TRASH;
        Init_Set_Word(DS_TOP, FLD_NAME(field)); // required name

        REBARR *typespec = Make_Array(2); // required type

        if (FLD_IS_STRUCT(field)) {
            Init_Word(Alloc_Tail_Array(typespec), Canon(SYM_STRUCT_X));

            DECLARE_LOCAL (nested);
            get_scalar(nested, stu, field, 0);

            PUSH_GUARD_VALUE(nested); // is this guard still necessary?
            Init_Block(
                Alloc_Tail_Array(typespec),
                Struct_To_Array(VAL_STRUCT(nested))
            );
            DROP_GUARD_VALUE(nested);
        }
        else {
            // Elemental type (from a fixed list of known C types)
            //
            Init_Word(Alloc_Tail_Array(typespec), Canon(FLD_TYPE_SYM(field)));
        }

        // "optional dimension and initialization."
        //
        // !!! Comment said the initialization was optional, but it seems
        // that the initialization always happens (?)
        //
        if (FLD_IS_ARRAY(field)) {
            //
            // Dimension becomes INTEGER! in a BLOCK! (to look like a C array)
            //
            REBCNT dimension = FLD_DIMENSION(field);
            REBARR *one_int = Alloc_Singular_Array();
            Init_Integer(ARR_HEAD(one_int), dimension);
            Init_Block(Alloc_Tail_Array(typespec), one_int);

            // Initialization seems to be just another block after that (?)
            //
            REBARR *init = Make_Array(dimension);
            REBCNT n;
            for (n = 0; n < dimension; n ++) {
                REBVAL *dest = SINK(ARR_AT(init, n));
                get_scalar(dest, stu, field, n);
            }
            TERM_ARRAY_LEN(init, dimension);
            Init_Block(Alloc_Tail_Array(typespec), init);
        }
        else {
            REBVAL *dest = Alloc_Tail_Array(typespec);
            get_scalar(dest, stu, field, 0);
        }

        DS_PUSH_TRASH;
        Init_Block(DS_TOP, typespec); // required type
    }

    return Pop_Stack_Values(dsp_orig);
}


void MF_Struct(REB_MOLD *mo, const RELVAL *v, REBOOL form)
{
    UNUSED(form);

    Pre_Mold(mo, v);

    REBARR *array = Struct_To_Array(VAL_STRUCT(v));
    Mold_Array_At(mo, array, 0, 0);
    Free_Array(array);

    End_Mold(mo);
}


static REBOOL same_fields(REBARR *tgt_fieldlist, REBARR *src_fieldlist)
{
    if (ARR_LEN(tgt_fieldlist) != ARR_LEN(src_fieldlist))
        return FALSE;

    RELVAL *tgt_item = ARR_HEAD(tgt_fieldlist);
    RELVAL *src_item = ARR_HEAD(src_fieldlist);

    for (; NOT_END(src_item); ++src_item, ++tgt_item) {
        REBFLD *src_field = VAL_ARRAY(src_item);
        REBFLD *tgt_field = VAL_ARRAY(tgt_item);

        if (
            FLD_IS_STRUCT(tgt_field) &&
            !same_fields(FLD_FIELDLIST(tgt_field), FLD_FIELDLIST(src_field))
        ){
            return FALSE;
        }

        if (NOT(
            SAME_SYM_NONZERO(
                FLD_TYPE_SYM(tgt_field), FLD_TYPE_SYM(src_field)
            )
        )){
            return FALSE;
        }

        if (FLD_IS_ARRAY(tgt_field)) {
            if (!FLD_IS_ARRAY(src_field))
                return FALSE;

            if (FLD_DIMENSION(tgt_field) != FLD_DIMENSION(src_field))
                return FALSE;
        }

        if (FLD_OFFSET(tgt_field) != FLD_OFFSET(src_field))
            return FALSE;

        assert(FLD_WIDE(tgt_field) == FLD_WIDE(src_field));
    }

    assert(IS_END(tgt_item));

    return TRUE;
}


static REBOOL assign_scalar_core(
    REBYTE *data_head,
    REBCNT offset,
    REBFLD *field,
    REBCNT n,
    const REBVAL *val
){
    assert(n == 0 || FLD_IS_ARRAY(field));

    void *data = data_head +
        offset + FLD_OFFSET(field) + (n * FLD_WIDE(field));

    if (FLD_IS_STRUCT(field)) {
        if (!IS_STRUCT(val))
            fail (Error_Invalid_Type(VAL_TYPE(val)));

        if (FLD_WIDE(field) != VAL_STRUCT_SIZE(val))
            fail (val);

        if (!same_fields(FLD_FIELDLIST(field), VAL_STRUCT_FIELDLIST(val)))
            fail (val);

        memcpy(data, VAL_STRUCT_DATA_AT(val), FLD_WIDE(field));

        return TRUE;
    }

    // All other types take numbers

    i64 i;
    double d;

    switch (VAL_TYPE(val)) {
    case REB_DECIMAL:
        d = VAL_DECIMAL(val);
        i = cast(i64, d);
        break;

    case REB_INTEGER:
        i = VAL_INT64(val);
        d = cast(double, i);
        break;

    default:
        fail (Error_Invalid_Type(VAL_TYPE(val)));
    }

    switch (FLD_TYPE_SYM(field)) {
    case SYM_INT8:
        if (i > 0x7f || i < -128)
            fail (Error_Overflow_Raw());
        *cast(i8*, data) = cast(i8, i);
        break;

    case SYM_UINT8:
        if (i > 0xff || i < 0)
            fail (Error_Overflow_Raw());
        *cast(u8*, data) = cast(u8, i);
        break;

    case SYM_INT16:
        if (i > 0x7fff || i < -0x8000)
            fail (Error_Overflow_Raw());
        *cast(i16*, data) = cast(i16, i);
        break;

    case SYM_UINT16:
        if (i > 0xffff || i < 0)
            fail (Error_Overflow_Raw());
        *cast(u16*, data) = cast(u16, i);
        break;

    case SYM_INT32:
        if (i > MAX_I32 || i < MIN_I32)
            fail (Error_Overflow_Raw());
        *cast(i32*, data) = cast(i32, i);
        break;

    case SYM_UINT32:
        if (i > MAX_U32 || i < 0)
            fail (Error_Overflow_Raw());
        *cast(u32*, data) = cast(u32, i);
        break;

    case SYM_INT64:
        *cast(i64*, data) = i;
        break;

    case SYM_UINT64:
        if (i < 0)
            fail (Error_Overflow_Raw());
        *cast(u64*, data) = cast(u64, i);
        break;

    case SYM_FLOAT:
        *cast(float*, data) = cast(float, d);
        break;

    case SYM_DOUBLE:
        *cast(double*, data) = d;
        break;

    case SYM_POINTER: {
        size_t sizeof_void_ptr = sizeof(void*); // avoid constant conditional
        if (sizeof_void_ptr == 4 && i > MAX_U32)
            fail (Error_Overflow_Raw());
        *cast(void**, data) = cast(void*, cast(REBUPT, i));
        break; }

    case SYM_REBVAL:
        //
        // !!! This is a dangerous thing to be doing in generic structs, but
        // for the main purpose of REBVAL (tunneling) it should be okay so
        // long as the REBVAL* that is passed in is actually a pointer into
        // a frame's args.
        //
        *cast(const REBVAL**, data) = val;
        break;

    default:
        assert(FALSE);
        return FALSE;
    }

    return TRUE;
}


inline static REBOOL assign_scalar(
    REBSTU *stu,
    REBFLD *field,
    REBCNT n,
    const REBVAL *val
) {
    return assign_scalar_core(
        STU_DATA_HEAD(stu), STU_OFFSET(stu), field, n, val
    );
}


//
//  Set_Struct_Var: C
//
static REBOOL Set_Struct_Var(
    REBSTU *stu,
    const REBVAL *word,
    const REBVAL *elem,
    const REBVAL *val
) {
    REBARR *fieldlist = STU_FIELDLIST(stu);
    RELVAL *item = ARR_HEAD(fieldlist);

    for (; NOT_END(item); ++item) {
        REBFLD *field = VAL_ARRAY(item);

        if (VAL_WORD_CANON(word) != STR_CANON(FLD_NAME(field)))
            continue;

        if (FLD_IS_ARRAY(field)) {
            if (elem == NULL) { // set the whole array
                if (!IS_BLOCK(val))
                    return FALSE;

                REBCNT dimension = FLD_DIMENSION(field);
                if (dimension != VAL_LEN_AT(val))
                    return FALSE;

                REBCNT n = 0;
                for(n = 0; n < dimension; ++n) {
                    if (!assign_scalar(
                        stu, field, n, KNOWN(VAL_ARRAY_AT_HEAD(val, n))
                    )) {
                        return FALSE;
                    }
                }
            }
            else { // set only one element
                if (!IS_INTEGER(elem) || VAL_INT32(elem) != 1)
                    return FALSE;

                return assign_scalar(stu, field, 0, val);
            }
            return TRUE;
        }

        return assign_scalar(stu, field, 0, val);
    }

    return FALSE;
}


/* parse struct attribute */
static void parse_attr (REBVAL *blk, REBINT *raw_size, REBUPT *raw_addr)
{
    REBVAL *attr = KNOWN(VAL_ARRAY_AT(blk));

    *raw_size = -1;
    *raw_addr = 0;

    while (NOT_END(attr)) {
        if (NOT(IS_SET_WORD(attr)))
            fail (attr);

        switch (VAL_WORD_SYM(attr)) {
        case SYM_RAW_SIZE:
            ++ attr;
            if (NOT_END(attr) && IS_INTEGER(attr)) {
                if (*raw_size > 0)
                    fail ("FFI: duplicate raw size");

                *raw_size = VAL_INT64(attr);
                if (*raw_size <= 0)
                    fail ("FFI: raw size cannot be zero");
            }
            else
                fail (attr);
            break;

        case SYM_RAW_MEMORY:
            ++ attr;
            if (NOT_END(attr) && IS_INTEGER(attr)) {
                if (*raw_addr != 0)
                    fail ("FFI: duplicate raw memory");

                *raw_addr = cast(REBU64, VAL_INT64(attr));
                if (*raw_addr == 0)
                    fail ("FFI: void pointer illegal for raw memory");
            }
            else
                fail (attr);
            break;

        case SYM_EXTERN: {
            ++ attr;

            if (*raw_addr != 0)
                fail ("FFI: raw memory is exclusive with extern");

            if (IS_END(attr) || NOT(IS_BLOCK(attr)) || VAL_LEN_AT(attr) != 2)
                fail (attr);

            REBVAL *lib = KNOWN(VAL_ARRAY_AT_HEAD(attr, 0));
            if (NOT(IS_LIBRARY(lib)))
                fail (attr);
            if (IS_LIB_CLOSED(VAL_LIBRARY(lib)))
                fail (Error_Bad_Library_Raw());

            REBVAL *sym = KNOWN(VAL_ARRAY_AT_HEAD(attr, 1));
            if (NOT(ANY_BINSTR(sym)))
                fail (sym);

            CFUNC *addr = OS_FIND_FUNCTION(
                VAL_LIBRARY_FD(lib),
                s_cast(VAL_RAW_DATA_AT(sym))
            );
            if (addr == NULL)
                fail (Error_Symbol_Not_Found_Raw(sym));

            *raw_addr = cast(REBUPT, addr);
            break; }

        // !!! This alignment code was commented out for some reason.
        /*
        case SYM_ALIGNMENT:
            ++ attr;
            if (!IS_INTEGER(attr))
                fail (attr);

            alignment = VAL_INT64(attr);
            break;
        */

        default:
            fail (attr);
        }

        ++ attr;
    }
}


// The managed handle logic always assumes a cleanup function, so it doesn't
// have to test for NULL.
//
static void cleanup_noop(const REBVAL *v) {
#ifdef NDEBUG
    UNUSED(v);
#else
    assert(IS_HANDLE(v));
#endif
}


//
// set storage memory to external addr: raw_addr
//
// "External Storage" is the idea that a STRUCT! which is modeling a C
// struct doesn't use a BINARY! series as the backing store, rather a pointer
// that is external to the system.  When Atronix added the FFI initially,
// this was done by creating a separate type of REBSER that could use an
// external pointer.  This uses a managed HANDLE! for the same purpose, as
// a less invasive way of doing the same thing.
//
static REBSER *make_ext_storage(
    REBCNT len,
    REBINT raw_size,
    REBUPT raw_addr
) {
    if (raw_size >= 0 && raw_size != cast(REBINT, len)) {
        DECLARE_LOCAL (i);
        Init_Integer(i, raw_size);
        fail (Error_Invalid_Data_Raw(i));
    }

    DECLARE_LOCAL (handle);
    Init_Handle_Managed(handle, cast(REBYTE*, raw_addr), len, &cleanup_noop);

    return SER(handle->extra.singular);
}


//
//  Total_Struct_Dimensionality: C
//
// This recursively counts the total number of data elements inside of a
// struct.  This includes for instance every array element inside a
// nested struct's field, along with its fields.
//
// !!! Is this really how char[1000] would be handled in the FFI?  By
// creating 1000 ffi_types?  :-/
//
static REBCNT Total_Struct_Dimensionality(REBARR *fields)
{
    REBCNT n_fields = 0;

    RELVAL *item = ARR_HEAD(fields);
    for (; NOT_END(item); ++item) {
        REBFLD *field = VAL_ARRAY(item);

        if (FLD_IS_STRUCT(field))
            n_fields += Total_Struct_Dimensionality(FLD_FIELDLIST(field));
        else
            n_fields += FLD_IS_ARRAY(field) ? FLD_DIMENSION(field) : 1;
    }
    return n_fields;
}


//
//  Prepare_Field_For_FFI: C
//
// The main reason structs exist is so that they can be used with the FFI,
// and the FFI requires you to set up a "ffi_type" C struct describing
// each datatype. This is a helper function that sets up proper ffi_type.
// There are stock types for the primitives, but each structure needs its
// own.
//
static void Prepare_Field_For_FFI(REBFLD *schema)
{
    assert(IS_UNREADABLE_IF_DEBUG(FLD_AT(schema, IDX_FIELD_FFTYPE)));

    ffi_type *fftype;

    if (!FLD_IS_STRUCT(schema)) {
        fftype = Get_FFType_For_Sym(FLD_TYPE_SYM(schema));
        assert(fftype != NULL);

        // The FFType pointers returned by Get_FFType_For_Sym should not be
        // freed, so a "simple" handle is used that just holds the pointer.
        //
        Init_Handle_Simple(FLD_AT(schema, IDX_FIELD_FFTYPE), fftype, 0);
        return;
    }

    // For struct fields--on the other hand--it's necessary to do a custom
    // allocation for a new type registered with the FFI.
    //
    fftype = OS_ALLOC(ffi_type);
    fftype->type = FFI_TYPE_STRUCT;

    // "This is set by libffi; you should initialize it to zero."
    // http://www.atmark-techno.com/~yashi/libffi.html#Structures
    //
    fftype->size = 0;
    fftype->alignment = 0;

    REBARR *fieldlist = FLD_FIELDLIST(schema);

    REBCNT dimensionality = Total_Struct_Dimensionality(fieldlist);
    fftype->elements = OS_ALLOC_N(ffi_type*, dimensionality + 1); // NULL term

    RELVAL *item = ARR_HEAD(fieldlist);

    REBCNT j = 0;
    for (; NOT_END(item); ++item) {
        REBFLD *field = VAL_ARRAY(item);
        REBCNT dimension = FLD_IS_ARRAY(field) ? FLD_DIMENSION(field) : 1;

        REBCNT n = 0;
        for (n = 0; n < dimension; ++n)
            fftype->elements[j++] = FLD_FFTYPE(field);
    }

    fftype->elements[j] = NULL;

    Init_Handle_Managed(
        FLD_AT(schema, IDX_FIELD_FFTYPE),
        fftype,
        dimensionality + 1,
        &cleanup_ffi_type
    );
}


//
// This takes a spec like `[int32 [2]]` and sets the output field's properties
// by recognizing a finite set of FFI type keywords defined in %words.r.
//
// This also allows for embedded structure types.  If the type is not being
// included by reference, but rather with a sub-definition inline, then it
// will actually be creating a new `inner` STRUCT! value.  Since this value
// is managed and not referred to elsewhere, there can't be evaluations.
//
static void Parse_Field_Type_May_Fail(
    REBFLD *field,
    REBVAL *spec,
    REBVAL *inner // will be set only if STRUCT!
){
    TRASH_CELL_IF_DEBUG(inner);

    RELVAL *val = VAL_ARRAY_AT(spec);

    if (IS_END(val))
        fail ("Empty field type in FFI");

    if (IS_WORD(val)) {
        REBSYM sym = VAL_WORD_SYM(val);

        // Initialize the type symbol with the unbound word by default (will
        // be overwritten in the struct cases).
        //
        Init_Word(FLD_AT(field, IDX_FIELD_TYPE), Canon(sym));

        switch (sym) {
        case SYM_UINT8:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 1);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_INT8:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 1);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_UINT16:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 2);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_INT16:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 2);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_UINT32:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 4);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_INT32:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 4);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_UINT64:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 8);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_INT64:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 8);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_FLOAT:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 4);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_DOUBLE:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), 8);
            Prepare_Field_For_FFI(field);
            break;

        case SYM_POINTER:
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), sizeof(void*));
            Prepare_Field_For_FFI(field);
            break;

        case SYM_STRUCT_X: {
            ++ val;
            if (!IS_BLOCK(val))
                fail (Error_Unexpected_Type(REB_BLOCK, VAL_TYPE(val)));

            DECLARE_LOCAL (specified);
            Derelativize(specified, val, VAL_SPECIFIER(spec));
            MAKE_Struct(inner, REB_STRUCT, specified); // may fail()

            Init_Integer(
                FLD_AT(field, IDX_FIELD_WIDE),
                VAL_STRUCT_DATA_LEN(inner)
            );
            Init_Block(
                FLD_AT(field, IDX_FIELD_TYPE),
                VAL_STRUCT_FIELDLIST(inner)
            );

            // Borrow the same ffi_type* that was built for the inner struct
            // (What about just storing the STRUCT! value itself in the type
            // field, instead of the array of fields?)
            //
            Move_Value(
                FLD_AT(field, IDX_FIELD_FFTYPE),
                FLD_AT(VAL_STRUCT_SCHEMA(inner), IDX_FIELD_FFTYPE)
            );
            break; }

        case SYM_REBVAL: {
            //
            // While most data types have some kind of proxying of when you
            // pass a Rebol value in (such as turning an INTEGER! into bits
            // for a C `int`) if the argument is marked as being a REBVAL
            // then the VAL_TYPE is ignored, and it acts like a pointer to
            // the actual argument in the frame...whatever that may be.
            //
            // !!! The initial FFI implementation from Atronix would actually
            // store sizeof(REBVAL) in the struct, not sizeof(REBVAL*).  The
            // struct's binary data was then hooked into the garbage collector
            // to make sure that cell was marked.  Because the intended use
            // of the feature is "tunneling" a value from a routine's frame
            // to a callback's frame, the lifetime of the REBVAL* should last
            // for the entirety of the routine it was passed to.
            //
            Init_Integer(FLD_AT(field, IDX_FIELD_WIDE), sizeof(REBVAL*));
            Prepare_Field_For_FFI(field);
            break; }

        default:
            fail (Error_Invalid_Type(VAL_TYPE(val)));
        }
    }
    else if (IS_STRUCT(val)) {
        //
        // [b: [struct-a] val-a]
        //
        Init_Integer(
            FLD_AT(field, IDX_FIELD_WIDE),
            VAL_STRUCT_DATA_LEN(val)
        );
        Init_Block(
            FLD_AT(field, IDX_FIELD_TYPE),
            VAL_STRUCT_FIELDLIST(val)
        );

        // Borrow the same ffi_type* that the struct uses, see above note
        // regarding alternative ideas.
        //
        Move_Value(
            FLD_AT(field, IDX_FIELD_FFTYPE),
            FLD_AT(VAL_STRUCT_SCHEMA(val), IDX_FIELD_FFTYPE)
        );
        Derelativize(inner, val, VAL_SPECIFIER(spec));
    }
    else
        fail (Error_Invalid_Type(VAL_TYPE(val)));

    ++ val;

    // Find out the array dimension (if there is one)
    //
    if (IS_END(val)) {
        Init_Blank(FLD_AT(field, IDX_FIELD_DIMENSION)); // scalar
    }
    else if (IS_BLOCK(val)) {
        //
        // make struct! [a: [int32 [2]] [0 0]]
        //
        DECLARE_LOCAL (ret);
        if (Do_At_Throws(
            ret,
            VAL_ARRAY(val),
            VAL_INDEX(val),
            VAL_SPECIFIER(spec)
        )) {
            // !!! Does not check for thrown cases...what should this
            // do in case of THROW, BREAK, QUIT?
            fail (Error_No_Catch_For_Throw(ret));
        }

        if (!IS_INTEGER(ret))
            fail (Error_Unexpected_Type(REB_INTEGER, VAL_TYPE(val)));

        Init_Integer(FLD_AT(field, IDX_FIELD_DIMENSION), VAL_INT64(ret));
        ++ val;
    }
    else
        fail (Error_Invalid_Type(VAL_TYPE(val)));
}


//
//  Init_Struct_Fields: C
//
// a: make struct! [uint 8 i: 1]
// b: make a [i: 10]
//
void Init_Struct_Fields(REBVAL *ret, REBVAL *spec)
{
    REBVAL *spec_item = KNOWN(VAL_ARRAY_AT(spec));

    while (NOT_END(spec_item)) {
        REBVAL *word;
        if (IS_BLOCK(spec_item)) { // options: raw-memory, etc
            REBINT raw_size = -1;
            REBUPT raw_addr = 0;

            // make sure no other field initialization
            if (VAL_LEN_HEAD(spec) != 1)
                fail (spec);

            parse_attr(spec_item, &raw_size, &raw_addr);
            ret->payload.structure.data
                 = make_ext_storage(VAL_STRUCT_SIZE(ret), raw_size, raw_addr);

            break;
        }
        else {
            word = spec_item;
            if (NOT(IS_SET_WORD(word)))
                fail (word);
        }

        REBVAL *fld_val = spec_item + 1;
        if (IS_END(fld_val))
            fail (Error_Need_Value_Raw(fld_val));

        REBARR *fieldlist = VAL_STRUCT_FIELDLIST(ret);
        RELVAL *item = ARR_HEAD(fieldlist);

        for (; NOT_END(item); ++item) {
            REBFLD *field = VAL_ARRAY(item);

            if (STR_CANON(FLD_NAME(field)) != VAL_WORD_CANON(word))
                continue;

            if (FLD_IS_ARRAY(field)) {
                if (IS_BLOCK(fld_val)) {
                    REBCNT dimension = FLD_DIMENSION(field);

                    if (VAL_LEN_AT(fld_val) != dimension)
                        fail (fld_val);

                    REBCNT n = 0;
                    for (n = 0; n < dimension; ++n) {
                        if (NOT(assign_scalar(
                            VAL_STRUCT(ret),
                            field,
                            n,
                            KNOWN(VAL_ARRAY_AT_HEAD(fld_val, n))
                        ))) {
                            fail (fld_val);
                        }
                    }
                }
                else if (IS_INTEGER(fld_val)) { // interpret as a data pointer
                    void *ptr = cast(void *,
                        cast(REBUPT, VAL_INT64(fld_val))
                    );

                    // assuming valid pointer to enough space
                    memcpy(
                        VAL_STRUCT_DATA_HEAD(ret) + FLD_OFFSET(field),
                        ptr,
                        FLD_LEN_BYTES_TOTAL(field)
                    );
                }
                else
                    fail (fld_val);
            }
            else {
                if (NOT(assign_scalar(VAL_STRUCT(ret), field, 0, fld_val)))
                    fail (fld_val);
            }
            goto next_spec_pair;
        }

        fail ("FFI: field not in the parent struct");

    next_spec_pair:
        spec_item += 2;
    }
}


//
//  MAKE_Struct: C
//
// Format:
//     make struct! [
//         field1 [type1]
//         field2: [type2] field2-init-value
//         field3: [struct [field1 [type1]]]
//         field4: [type1[3]]
//         ...
//     ]
//
void MAKE_Struct(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg) {
    assert(kind == REB_STRUCT);
    UNUSED(kind);

    if (NOT(IS_BLOCK(arg)))
        fail (arg);

    REBINT max_fields = 16;

//
// SET UP SCHEMA
//
    // Every struct has a "schema"--this is a description (potentially
    // hierarchical) of its fields, including any nested structs.  The
    // schema should be shared between common instances of the same struct.
    //
    REBFLD *schema = Make_Array(IDX_FIELD_MAX);
    Init_Blank(FLD_AT(schema, IDX_FIELD_NAME)); // no symbol for struct itself
    // we'll be filling in the IDX_FIELD_TYPE slot with an array of fields
    Init_Blank(FLD_AT(schema, IDX_FIELD_DIMENSION)); // not an array

    Init_Unreadable_Blank(FLD_AT(schema, IDX_FIELD_FFTYPE));

    Init_Blank(FLD_AT(schema, IDX_FIELD_OFFSET)); // the offset is not used
    // we'll be filling in the IDX_FIELD_WIDE at the end.


//
// PROCESS FIELDS
//

    u64 offset = 0; // offset in data

    REBINT raw_size = -1;
    REBUPT raw_addr = 0;

    DECLARE_LOCAL (specified);

    RELVAL *item = VAL_ARRAY_AT(arg);
    if (NOT_END(item) && IS_BLOCK(item)) {
        //
        // !!! This would suggest raw-size, raw-addr, or extern can be leading
        // in the struct definition, perhaps as:
        //
        //     make struct! [[raw-size] ...]
        //
        Derelativize(specified, item, VAL_SPECIFIER(arg));
        parse_attr(specified, &raw_size, &raw_addr);
        ++item;
    }

    // !!! This makes binary data for each struct level? ???
    //
    REBSER *data_bin;
    if (raw_addr == 0)
        data_bin = Make_Binary(max_fields << 2);
    else
        data_bin = NULL; // not used, but avoid maybe uninitialized warning

    REBINT field_idx = 0; // for field index
    REBIXO eval_idx = 0; // for spec block evaluation

    REBDSP dsp_orig = DSP; // use data stack to accumulate fields (BLOCK!s)

    DECLARE_LOCAL (spec);
    DECLARE_LOCAL (init); // for result to save in data

    while (NOT_END(item)) {

        // Add another field...

        REBFLD *field = Make_Array(IDX_FIELD_MAX);

        Init_Unreadable_Blank(FLD_AT(field, IDX_FIELD_FFTYPE));
        Init_Integer(FLD_AT(field, IDX_FIELD_OFFSET), offset);

        // Must be a word or a set-word, with set-words initializing

        REBOOL expect_init;
        if (IS_SET_WORD(item)) {
            expect_init = TRUE;
            if (raw_addr) {
                // initialization is not allowed for raw memory struct
                fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(arg)));
            }
        }
        else if (IS_WORD(item))
            expect_init = FALSE;
        else
            fail (Error_Invalid_Type(VAL_TYPE(item)));

        Init_Word(FLD_AT(field, IDX_FIELD_NAME), VAL_WORD_SPELLING(item));

        ++item;
        if (IS_END(item) || !IS_BLOCK(item))
            fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(arg)));

        Derelativize(spec, item, VAL_SPECIFIER(arg));

        // Fills in the width, dimension, type, and ffi_type (if needed)
        //
        Parse_Field_Type_May_Fail(field, spec, init);

        REBCNT dimension = FLD_IS_ARRAY(field) ? FLD_DIMENSION(field) : 1;
        ++item;

        // !!! Why does the fail take out as an argument?  (Copied from below)

        if (FLD_WIDE(field) > MAX_U32)
            fail (Error_Size_Limit_Raw(out));
        if (dimension > MAX_U32)
            fail (Error_Size_Limit_Raw(out));

        u64 step = cast(u64, FLD_WIDE(field)) * cast(u64, dimension);

        if (step > VAL_STRUCT_LIMIT)
            fail (Error_Size_Limit_Raw(out));

        if (raw_addr == 0)
            EXPAND_SERIES_TAIL(data_bin, step);

        if (expect_init) {
            if (IS_END(item))
               fail (arg);

            if (IS_BLOCK(item)) {
                Derelativize(specified, item, VAL_SPECIFIER(arg));

                if (Reduce_Any_Array_Throws(
                    init, specified, REDUCE_FLAG_DROP_BARS
                )){
                    fail (Error_No_Catch_For_Throw(init));
                }

                ++item;
            }
            else {
                eval_idx = DO_NEXT_MAY_THROW(
                    init,
                    VAL_ARRAY(arg),
                    item - VAL_ARRAY_AT(arg),
                    VAL_SPECIFIER(arg)
                );
                if (eval_idx == THROWN_FLAG)
                    fail (Error_No_Catch_For_Throw(init));

                if (eval_idx == END_FLAG)
                    item = VAL_ARRAY_TAIL(arg);
                else
                    item = VAL_ARRAY_AT_HEAD(item, cast(REBCNT, eval_idx));
            }

            if (FLD_IS_ARRAY(field)) {
                if (IS_INTEGER(init)) { // interpreted as a C pointer
                    void *ptr = cast(void *, cast(REBUPT, VAL_INT64(init)));

                    // assume valid pointer to enough space
                    memcpy(
                        SER_AT(REBYTE, data_bin, cast(REBCNT, offset)),
                        ptr,
                        FLD_LEN_BYTES_TOTAL(field)
                    );
                }
                else if (IS_BLOCK(init)) {
                    REBCNT n = 0;

                    if (VAL_LEN_AT(init) != FLD_DIMENSION(field))
                        fail (init);

                    // assign
                    for (n = 0; n < FLD_DIMENSION(field); n ++) {
                        if (!assign_scalar_core(
                            BIN_HEAD(data_bin),
                            offset,
                            field,
                            n,
                            KNOWN(VAL_ARRAY_AT_HEAD(init, n))
                        )) {
                            fail ("FFI: Failed to assign element value");
                        }
                    }
                }
                else
                    fail (Error_Unexpected_Type(REB_BLOCK, VAL_TYPE(item)));
            }
            else {
                // scalar
                if (!assign_scalar_core(
                    BIN_HEAD(data_bin), offset, field, 0, init
                )) {
                    fail ("FFI: Failed to assign scalar value");
                }
            }
        }
        else if (raw_addr == 0) {
            if (FLD_IS_STRUCT(field)) {
                REBCNT n = 0;
                for (n = 0; n < FLD_DIMENSION(field); n ++) {
                    memcpy(
                        SER_AT(
                            REBYTE,
                            data_bin,
                            cast(REBCNT, offset) + (n * FLD_WIDE(field))
                        ),
                        VAL_STRUCT_DATA_HEAD(init),
                        FLD_WIDE(field)
                    );
                }
            }
            else {
                memset(
                    SER_AT(REBYTE, data_bin, cast(REBCNT, offset)),
                    0,
                    FLD_LEN_BYTES_TOTAL(field)
                );
            }
        }

        offset += step;

        //if (alignment != 0) {
        //  offset = ((offset + alignment - 1) / alignment) * alignment;

        if (offset > VAL_STRUCT_LIMIT)
            fail (Error_Size_Limit_Raw(out));

        ++ field_idx;

        TERM_ARRAY_LEN(field, 6);
        ASSERT_ARRAY(field);

        DS_PUSH_TRASH;
        Init_Block(DS_TOP, field); // really should be an OBJECT!
    }

    REBARR *fieldlist = Pop_Stack_Values(dsp_orig);
    ASSERT_ARRAY(fieldlist);

    Init_Block(FLD_AT(schema, IDX_FIELD_TYPE), fieldlist);
    Prepare_Field_For_FFI(schema);

    Init_Integer(FLD_AT(schema, IDX_FIELD_WIDE), offset); // total size known

    TERM_ARRAY_LEN(schema, IDX_FIELD_MAX);
    ASSERT_ARRAY(schema);

//
// FINALIZE VALUE
//

    REBSTU *stu = Alloc_Singular_Array();

    // Set it to blank so the Kill_Series can be called upon in case of error
    // thrown before it is fully constructed.
    //
    Init_Blank(ARR_HEAD(stu));

    MANAGE_ARRAY(schema);
    SER(stu)->link.schema = schema;

    VAL_RESET_HEADER(out, REB_STRUCT);
    out->payload.structure.stu = stu;
    if (raw_addr) {
        out->payload.structure.data
            = make_ext_storage(
                FLD_LEN_BYTES_TOTAL(schema), raw_size, raw_addr
            );
    }
    else {
        MANAGE_SERIES(data_bin);
        out->payload.structure.data = data_bin;
    }
    out->extra.struct_offset = 0;

    *ARR_HEAD(stu) = *out;
    MANAGE_ARRAY(stu);
}


//
//  TO_Struct: C
//
void TO_Struct(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    MAKE_Struct(out, kind, arg);
}


//
//  PD_Struct: C
//
REBINT PD_Struct(REBPVS *pvs)
{
    REBSTU *stu = VAL_STRUCT(pvs->value);
    if (!IS_WORD(pvs->picker))
        fail (Error_Bad_Path_Select(pvs));

    fail_if_non_accessible(KNOWN(pvs->value));

    if (!pvs->opt_setval || NOT_END(pvs->item + 1)) {
        if (NOT(Get_Struct_Var(pvs->store, stu, pvs->picker)))
            fail (Error_Bad_Path_Select(pvs));

        // !!! Comment here said "Setting element to an array in the struct"
        // and gave the example `struct/field/1: 0`.  What is thus happening
        // here is that the ordinary SET-PATH! dispatch which goes one step
        // at a time can't work to update something whose storage is not
        // a REBVAL*.  So (struct/field) produces a temporary BLOCK! out of
        // the C array data, and if the set just sets an element in that
        // block then it will be forgotten and have no effect.
        //
        // So the workaround is to bypass ordinary dispatch and call it to
        // look ahead manually by one step.  Whatever change is made to
        // the block is then turned around and re-set in the underlying
        // memory that produced it.
        //
        // A better general mechanism for this kind of problem is needed,
        // although it only affects "extension types" which use natively
        // packed structures to store their state instead of REBVAL.  (See
        // a similar technique used by PD_Gob)
        //
        if (
            pvs->opt_setval
            && IS_BLOCK(pvs->store)
            && IS_END(pvs->item + 2)
        ) {
            // !!! This is dodgy; it has to copy (as picker is a pointer to
            // a memory cell it may not own), has to guard (as the next path
            // evaluation may not protect the result...)
            //
            DECLARE_LOCAL (sel_orig);
            Move_Value(sel_orig, pvs->picker);
            PUSH_GUARD_VALUE(sel_orig);

            pvs->value = pvs->store;
            pvs->value_specifier = SPECIFIED;

            if (Next_Path_Throws(pvs)) { // updates pvs->store, pvs->picker
                DROP_GUARD_VALUE(sel_orig);
                fail (Error_No_Catch_For_Throw(pvs->store)); // !!! Review
            }

            DECLARE_LOCAL (specific);
            Derelativize(specific, pvs->value, pvs->value_specifier);

            if (!Set_Struct_Var(stu, sel_orig, pvs->picker, specific))
                fail (Error_Bad_Path_Set(pvs));

            DROP_GUARD_VALUE(sel_orig);

            return PE_OK;
        }

        return PE_USE_STORE;
    }
    else {
        // setting (because opt_setval is non-NULL, and at end of path)

        if (!Set_Struct_Var(stu, pvs->picker, NULL, pvs->opt_setval))
            fail (Error_Bad_Path_Set(pvs));

        return PE_OK;
    }
}


//
//  Cmp_Struct: C
//
REBINT Cmp_Struct(const RELVAL *s, const RELVAL *t)
{
    REBINT n = VAL_STRUCT_FIELDLIST(s) - VAL_STRUCT_FIELDLIST(t);
    fail_if_non_accessible(const_KNOWN(s));
    fail_if_non_accessible(const_KNOWN(t));
    if (n != 0) {
        return n;
    }
    n = VAL_STRUCT(s) - VAL_STRUCT(t);
    return n;
}


//
//  CT_Struct: C
//
REBINT CT_Struct(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    switch (mode) {
    case 1: // strict equality
        return 0 == Cmp_Struct(a, b);

    case 0: // coerced equality
        if (Cmp_Struct(a, b) == 0)
            return 1;

        return (
            IS_STRUCT(a) && IS_STRUCT(b)
            && same_fields(VAL_STRUCT_FIELDLIST(a), VAL_STRUCT_FIELDLIST(b))
            && VAL_STRUCT_SIZE(a) == VAL_STRUCT_SIZE(b)
            && !memcmp(
                VAL_STRUCT_DATA_HEAD(a),
                VAL_STRUCT_DATA_HEAD(b),
                VAL_STRUCT_SIZE(a)
            )
        );

    default:
        ; // let fall through to -1 case
    }
    return -1;
}


//
//  Copy_Struct_Managed: C
//
REBSTU *Copy_Struct_Managed(REBSTU *src)
{
    fail_if_non_accessible(STU_VALUE(src));

    assert(ARR_LEN(src) == 1);
    assert(IS_STRUCT(ARR_AT(src, 0)));

    // This doesn't copy the data out of the array, or the schema...just the
    // value.  In fact, the schema is in the misc field and has to just be
    // linked manually.
    //
    REBSTU *copy = Copy_Array_Shallow(src, SPECIFIED);
    SER(copy)->link.schema = SER(src)->link.schema;

    // Update the binary data with a copy of its sequence.
    //
    // !!! Note that this leaves the offset intact, and will wind up making a
    // copy as big as struct the instance is embedded into if nonzero offset.

    REBSER *bin_copy = Make_Binary(STU_DATA_LEN(src));
    memcpy(BIN_HEAD(bin_copy), STU_DATA_HEAD(src), STU_DATA_LEN(src));
    TERM_BIN_LEN(bin_copy, STU_DATA_LEN(src));
    STU_VALUE(copy)->payload.structure.data = bin_copy;
    assert(STU_DATA_HEAD(copy) == BIN_HEAD(bin_copy));

    MANAGE_SERIES(bin_copy);
    MANAGE_ARRAY(copy);
    return copy;
}


//
//  REBTYPE: C
//
REBTYPE(Struct)
{
    REBVAL *val;
    REBVAL *arg;

    val = D_ARG(1);

    Init_Void(D_OUT);
    // unary actions
    switch(action) {

    case SYM_CHANGE: {
        arg = D_ARG(2);
        if (!IS_BINARY(arg))
            fail (Error_Unexpected_Type(REB_BINARY, VAL_TYPE(arg)));

        if (VAL_LEN_AT(arg) != VAL_STRUCT_DATA_LEN(val))
            fail (arg);

        memcpy(
            VAL_STRUCT_DATA_HEAD(val),
            BIN_HEAD(VAL_SERIES(arg)),
            VAL_STRUCT_DATA_LEN(val)
        );
        Move_Value(D_OUT, val);
        break; }

    case SYM_REFLECT: {
        arg = D_ARG(2);
        switch (VAL_WORD_SYM(arg)) {
        case SYM_VALUES: {
            fail_if_non_accessible(val);
            REBSER *bin = Make_Binary(VAL_STRUCT_SIZE(val));
            memcpy(
                BIN_HEAD(bin),
                VAL_STRUCT_DATA_AT(val),
                VAL_STRUCT_SIZE(val)
            );
            TERM_BIN_LEN(bin, VAL_STRUCT_SIZE(val));
            Init_Binary(D_OUT, bin);
            break; }

        case SYM_SPEC:
            Init_Block(D_OUT, Struct_To_Array(VAL_STRUCT(val)));
            break;

        default:
            fail (Error_Cannot_Reflect(REB_STRUCT, arg));
        }
        break; }

    case SYM_LENGTH_OF:
        Init_Integer(D_OUT, VAL_STRUCT_DATA_LEN(val));
        break;

    default:
        fail (Error_Illegal_Action(REB_STRUCT, action));
    }
    return R_OUT;
}
