//
//  File: %t-strut.c
//  Summary: "C struct object datatype"
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

#include "sys-core.h"

#define STATIC_assert(e) do {(void)sizeof(char[1 - 2*!(e)]);} while(0)

#define IS_INTEGER_TYPE(t) \
    ((t) < STRUCT_TYPE_INTEGER)

#define IS_DECIMAL_TYPE(t) \
    ((t) > STRUCT_TYPE_INTEGER && (t) < STRUCT_TYPE_DECIMAL)

#define IS_NUMERIC_TYPE(t) \
    (IS_INTEGER_TYPE(t) || IS_DECIMAL_TYPE(t))

static const REBINT type_to_sym [STRUCT_TYPE_MAX] = {
    SYM_UINT8,
    SYM_INT8,
    SYM_UINT16,
    SYM_INT16,
    SYM_UINT32,
    SYM_INT32,
    SYM_UINT64,
    SYM_INT64,
    -1, //SYM_INTEGER,

    SYM_FLOAT,
    SYM_DOUBLE,
    -1, //SYM_DECIMAL,

    SYM_POINTER,
    -1, //SYM_STRUCT
    SYM_REBVAL
    //STRUCT_TYPE_MAX
};


static void fail_if_non_accessible(const REBVAL *val)
{
    if (VAL_STRUCT_INACCESSIBLE(val)) {
        REBSER *data = VAL_STRUCT_DATA_BIN(val);
        REBVAL i;
        SET_INTEGER(&i, cast(REBUPT, SER_DATA_RAW(data)));
        fail (Error(RE_BAD_MEMORY, &i, val));
    }
}

static void get_scalar(
    REBSTU *stu,
    const struct Struct_Field *field,
    REBCNT n, // element index, starting from 0
    REBVAL *val
){
    if (STU_INACCESSIBLE(stu)) {
        //
        // !!! This just sets the field to void with no error...that seems
        // like a bad idea.
        //
        if (field->type != STRUCT_TYPE_STRUCT) {
            SET_VOID(val);
            return;
        }
    }

    assert(n == 0 || field->is_array); // only index in field[N], not field

    REBYTE *data = SER_AT(
        REBYTE,
        STU_DATA_BIN(stu),
        STU_OFFSET(stu) + field->offset + n * field->size
    );

    switch (field->type) {
    case STRUCT_TYPE_UINT8:
        SET_INTEGER(val, *cast(u8*, data));
        break;

    case STRUCT_TYPE_INT8:
        SET_INTEGER(val, *cast(i8*, data));
        break;

    case STRUCT_TYPE_UINT16:
        SET_INTEGER(val, *cast(u16*, data));
        break;

    case STRUCT_TYPE_INT16:
        SET_INTEGER(val, *cast(i8*, data));
        break;

    case STRUCT_TYPE_UINT32:
        SET_INTEGER(val, *cast(u32*, data));
        break;

    case STRUCT_TYPE_INT32:
        SET_INTEGER(val, *cast(i32*, data));
        break;

    case STRUCT_TYPE_UINT64:
        SET_INTEGER(val, *cast(u64*, data));
        break;

    case STRUCT_TYPE_INT64:
        SET_INTEGER(val, *cast(i64*, data));
        break;

    case STRUCT_TYPE_FLOAT:
        SET_DECIMAL(val, *cast(float*, data));
        break;

    case STRUCT_TYPE_DOUBLE:
        SET_DECIMAL(val, *cast(double*, data));
        break;

    case STRUCT_TYPE_POINTER:
        SET_INTEGER(val, cast(REBUPT, *cast(void**, data)));
        break;

    case STRUCT_TYPE_STRUCT:
        {
        // In order for the schema to participate in GC it must be a series.
        // Currently this series is created with a single value of the root
        // schema in the case of a struct expansion.  This wouldn't be
        // necessary if each field that was a structure offered a REBSER
        // already... !!! ?? !!! ... it will be necessary if the schemas
        // are to uniquely carry an ffi_type freed when they are GC'd
        //
        REBSER *field_1 = Make_Series(
            sizeof(struct Struct_Field), 1, MKS_NONE
        );
        *SER_HEAD(struct Struct_Field, field_1) = *field;
        SET_SERIES_LEN(field_1, 1);
        MANAGE_SERIES(field_1);

        REBSTU *sub_stu = Make_Singular_Array(VOID_CELL);
        ARR_SERIES(sub_stu)->misc.schema = field_1;

        // In this case the structure lives at an offset inside another.
        //
        VAL_RESET_HEADER(val, REB_STRUCT);
        val->payload.structure.stu = sub_stu;
        val->payload.structure.data = STU_DATA_BIN(stu); // inside parent data
        val->payload.structure.offset
            = data - BIN_HEAD(VAL_STRUCT_DATA_BIN(val));
        assert(VAL_STRUCT_SIZE(val) == field->size); // implicit from schema

        // With all fields initialized, assign canon value as singular value
        //
        *ARR_HEAD(stu) = *val;
        assert(ARR_LEN(sub_stu) == 1);
        MANAGE_ARRAY(sub_stu);
        }
        break;

    case STRUCT_TYPE_REBVAL:
        memcpy(val, data, sizeof(REBVAL));
        break;

    default:
        assert(FALSE);
        fail (Error(RE_MISC));
    }
}


//
//  Get_Struct_Var: C
//
static REBOOL Get_Struct_Var(REBSTU *stu, const REBVAL *word, REBVAL *val)
{
    REBSER *fieldlist = STU_FIELDLIST(stu);

    struct Struct_Field *field = SER_HEAD(struct Struct_Field, fieldlist);

    REBCNT i;
    for (i = 0; i < SER_LEN(fieldlist); ++i, ++field) {
        if (
            VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, field->sym))
            == VAL_WORD_CANON(word)
        ){
            if (field->is_array) {
                //
                // Structs contain packed data for the field type in an array.
                // This data cannot expand or contract, and is not in a
                // Rebol-compatible format.  A Rebol Array is made by
                // extracting the information.
                //
                // !!! Perhaps a fixed-size VECTOR! could have its data
                // pointer into these arrays?
                //
                REBARR *array = Make_Array(field->dimension);
                REBCNT n;
                for (n = 0; n < field->dimension; n ++) {
                    REBVAL elem;
                    get_scalar(stu, field, n, &elem);
                    Append_Value(array, &elem);
                }
                Val_Init_Block(val, array);
            }
            else
                get_scalar(stu, field, 0, val);

            return TRUE;
        }
    }

    return FALSE; // word not found in struct's field symbols
}


#ifdef NEED_SET_STRUCT_VARS
//
//  Set_Struct_Vars: C
//
static void Set_Struct_Vars(REBSTU *strut, REBVAL *blk)
{
}
#endif


//
//  Struct_To_Array: C
// 
// Used by MOLD to create a block.
//
// Cannot fail(), because fail() could call MOLD on a struct!, which will end
// up infinitive recursive calls
//
REBARR *Struct_To_Array(REBSTU *stu)
{
    REBARR *array = Make_Array(10);
    REBSER *fieldlist = STU_FIELDLIST(stu);
    struct Struct_Field *field = SER_HEAD(struct Struct_Field, fieldlist);

    // We are building a recursive structure.  So if we did not hand each
    // sub-series over to the GC then a single Free_Series() would not know
    // how to free them all.  There would have to be a specialized walk to
    // free the resulting structure.  Hence, don't invoke the GC until the
    // root series being returned is done being used or is safe from GC!
    //
    MANAGE_ARRAY(array);

    // fail_if_non_accessible(STU_TO_VAL(stu));

    REBCNT i;
    for(i = 0; i < SER_LEN(fieldlist); i++, field ++) {
        REBVAL *val = NULL;
        REBVAL *type_blk = NULL;

        /* required field name */
        val = Alloc_Tail_Array(array);
        Val_Init_Word(val, REB_SET_WORD, field->sym);

        /* required type */
        type_blk = Alloc_Tail_Array(array);
        Val_Init_Block(type_blk, Make_Array(1));

        val = Alloc_Tail_Array(VAL_ARRAY(type_blk));
        if (field->type == STRUCT_TYPE_STRUCT) {
            REBVAL *nested;
            DS_PUSH_TRASH_SAFE;
            nested = DS_TOP;

            Val_Init_Word(val, REB_WORD, SYM_STRUCT_TYPE);
            get_scalar(stu, field, 0, nested);
            val = Alloc_Tail_Array(VAL_ARRAY(type_blk));
            Val_Init_Block(val, Struct_To_Array(VAL_STRUCT(nested)));

            DS_DROP;
        } else
            Val_Init_Word(val, REB_WORD, type_to_sym[field->type]);

        /* optional dimension */
        if (field->dimension > 1) {
            REBARR *dim = Make_Array(1);
            REBVAL *dv = NULL;
            val = Alloc_Tail_Array(VAL_ARRAY(type_blk));
            Val_Init_Block(val, dim);

            dv = Alloc_Tail_Array(dim);
            SET_INTEGER(dv, field->dimension);
        }

        /* optional initialization */
        if (field->dimension > 1) {
            REBARR *dim = Make_Array(1);
            REBCNT n = 0;
            val = Alloc_Tail_Array(array);
            Val_Init_Block(val, dim);
            for (n = 0; n < field->dimension; n ++) {
                REBVAL *dv = Alloc_Tail_Array(dim);
                get_scalar(stu, field, n, dv);
            }
        } else {
            val = Alloc_Tail_Array(array);
            get_scalar(stu, field, 0, val);
        }
    }
    return array;
}


static REBOOL same_fields(REBSER *tgt, REBSER *src)
{
    struct Struct_Field *tgt_fields = SER_HEAD(struct Struct_Field, tgt);
    struct Struct_Field *src_fields = SER_HEAD(struct Struct_Field, src);
    REBCNT n;

    if (SER_LEN(tgt) != SER_LEN(src)) {
        return FALSE;
    }

    for(n = 0; n < SER_LEN(src); n ++) {
        if (tgt_fields[n].type != src_fields[n].type) {
            return FALSE;
        }
        if (VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, tgt_fields[n].sym))
            != VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, src_fields[n].sym))
            || tgt_fields[n].offset != src_fields[n].offset
            || tgt_fields[n].dimension != src_fields[n].dimension
            || tgt_fields[n].size != src_fields[n].size) {
            return FALSE;
        }
        if (tgt_fields[n].type == STRUCT_TYPE_STRUCT
            && ! same_fields(tgt_fields[n].fields, src_fields[n].fields)) {
            return FALSE;
        }
    }

    return TRUE;
}


static REBOOL assign_scalar_core(
    REBSER *data_bin,
    REBCNT offset,
    struct Struct_Field *field,
    REBCNT n,
    const REBVAL *val
){
    assert(n == 0 || field->is_array);

    void *data = SER_AT(
        REBYTE,
        data_bin,
        offset + field->offset + n * field->size
    );

    u64 i = 0;
    double d = 0;

    if (field->type == STRUCT_TYPE_REBVAL) {
        memcpy(data, val, sizeof(REBVAL));
        return TRUE;
    }

    switch (VAL_TYPE(val)) {
        case REB_DECIMAL:
            if (!IS_NUMERIC_TYPE(field->type))
                fail (Error_Invalid_Type(VAL_TYPE(val)));

            d = VAL_DECIMAL(val);
            i = (u64) d;
            break;
        case REB_INTEGER:
            if (!IS_NUMERIC_TYPE(field->type))
                if (field->type != STRUCT_TYPE_POINTER)
                    fail (Error_Invalid_Type(VAL_TYPE(val)));

            i = (u64) VAL_INT64(val);
            d = (double)i;
            break;
        case REB_STRUCT:
            if (STRUCT_TYPE_STRUCT != field->type)
                fail (Error_Invalid_Type(VAL_TYPE(val)));
            break;
        default:
            fail (Error_Invalid_Type(VAL_TYPE(val)));
    }

    switch (field->type) {
        case STRUCT_TYPE_INT8:
            *(i8*)data = (i8)i;
            break;
        case STRUCT_TYPE_UINT8:
            *(u8*)data = (u8)i;
            break;
        case STRUCT_TYPE_INT16:
            *(i16*)data = (i16)i;
            break;
        case STRUCT_TYPE_UINT16:
            *(u16*)data = (u16)i;
            break;
        case STRUCT_TYPE_INT32:
            *(i32*)data = (i32)i;
            break;
        case STRUCT_TYPE_UINT32:
            *(u32*)data = (u32)i;
            break;
        case STRUCT_TYPE_INT64:
            *(i64*)data = (i64)i;
            break;
        case STRUCT_TYPE_UINT64:
            *(u64*)data = (u64)i;
            break;
        case STRUCT_TYPE_POINTER:
            *cast(void**, data) = cast(void*, cast(REBUPT, i));
            break;
        case STRUCT_TYPE_FLOAT:
            *(float*)data = (float)d;
            break;
        case STRUCT_TYPE_DOUBLE:
            *(double*)data = (double)d;
            break;
        case STRUCT_TYPE_STRUCT:
            if (field->size != VAL_STRUCT_SIZE(val))
                fail (Error_Invalid_Arg(val));

            if (same_fields(field->fields, VAL_STRUCT_FIELDLIST(val))) {
                memcpy(
                    data,
                    SER_AT(
                        REBYTE,
                        VAL_STRUCT_DATA_BIN(val),
                        VAL_STRUCT_OFFSET(val)
                    ),
                    field->size
                );
            } else
                fail (Error_Invalid_Arg(val));
            break;
        default:
            /* should never be here */
            return FALSE;
    }
    return TRUE;
}


inline static REBOOL assign_scalar(
    REBSTU *stu,
    struct Struct_Field *field,
    REBCNT n,
    const REBVAL *val
) {
    return assign_scalar_core(
        STU_DATA_BIN(stu), STU_OFFSET(stu), field, n, val
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
    REBSER *fieldlist = STU_FIELDLIST(stu);
    struct Struct_Field *field = SER_HEAD(struct Struct_Field, fieldlist);

    REBCNT i;

    for (i = 0; i < SER_LEN(fieldlist); i ++, field ++) {
        if (
            VAL_WORD_CANON(word)
            == VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, field->sym))
        ) {
            if (field->is_array) {
                if (elem == NULL) { //set the whole array
                    REBCNT n = 0;

                    if (!IS_BLOCK(val))
                        return FALSE;

                    if (field->dimension != VAL_LEN_AT(val))
                        return FALSE;

                    for(n = 0; n < field->dimension; n ++) {
                        if (!assign_scalar(
                            stu, field, n, KNOWN(VAL_ARRAY_AT_HEAD(val, n))
                        )) {
                            return FALSE;
                        }
                    }
                }
                else { // set only one element
                    if (!IS_INTEGER(elem)
                        || VAL_INT32(elem) <= 0
                        || VAL_INT32(elem) > cast(REBINT, field->dimension)
                    ) {
                        return FALSE;
                    }
                    return assign_scalar(stu, field, VAL_INT32(elem) - 1, val);
                }
                return TRUE;
            } else {
                return assign_scalar(stu, field, 0, val);
            }
            return TRUE;
        }
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
        if (IS_SET_WORD(attr)) {
            switch (VAL_WORD_CANON(attr)) {
                case SYM_RAW_SIZE:
                    ++ attr;
                    if (NOT_END(attr) && IS_INTEGER(attr)) {
                        if (*raw_size > 0) /* duplicate raw-size */
                            fail (Error_Invalid_Arg(attr));

                        *raw_size = VAL_INT64(attr);
                        if (*raw_size <= 0)
                            fail (Error_Invalid_Arg(attr));
                    }
                    else
                        fail (Error_Invalid_Arg(attr));
                    break;

                case SYM_RAW_MEMORY:
                    ++ attr;
                    if (NOT_END(attr) && IS_INTEGER(attr)) {
                        if (*raw_addr != 0) /* duplicate raw-memory */
                            fail (Error_Invalid_Arg(attr));

                        *raw_addr = cast(REBU64, VAL_INT64(attr));
                        if (*raw_addr == 0)
                            fail (Error_Invalid_Arg(attr));
                    }
                    else
                        fail (Error_Invalid_Arg(attr));
                    break;

                case SYM_EXTERN:
                    ++ attr;

                    if (*raw_addr != 0) // raw-memory is exclusive with extern
                        fail (Error_Invalid_Arg(attr));

                    if (IS_END(attr) || !IS_BLOCK(attr)
                        || VAL_LEN_AT(attr) != 2) {
                        fail (Error_Invalid_Arg(attr));
                    }
                    else {
                        REBVAL *lib;
                        REBVAL *sym;
                        CFUNC *addr;

                        lib = KNOWN(VAL_ARRAY_AT_HEAD(attr, 0));
                        sym = KNOWN(VAL_ARRAY_AT_HEAD(attr, 1));

                        if (!IS_LIBRARY(lib))
                            fail (Error_Invalid_Arg(attr));

                        if (GET_LIB_FLAG(VAL_LIB_HANDLE(lib), LIB_FLAG_CLOSED))
                            fail (Error(RE_BAD_LIBRARY));

                        if (!ANY_BINSTR(sym))
                            fail (Error_Invalid_Arg(sym));

                        addr = OS_FIND_FUNCTION(
                            LIB_FD(VAL_LIB_HANDLE(lib)),
                            s_cast(VAL_RAW_DATA_AT(sym))
                        );
                        if (!addr)
                            fail (Error(RE_SYMBOL_NOT_FOUND, sym));

                        *raw_addr = cast(REBUPT, addr);
                    }
                    break;

                    /*
                       case SYM_ALIGNMENT:
                       ++ attr;
                       if (IS_INTEGER(attr)) {
                       alignment = VAL_INT64(attr);
                       } else {
                       fail (Error_Invalid_Arg(attr));
                       }
                       break;
                       */
                default:
                    fail (Error_Invalid_Arg(attr));
            }
        }
        else
            fail (Error_Invalid_Arg(attr));

        ++ attr;
    }
}

//
// set storage memory to external addr: raw_addr
//
// !!! The STRUCT! type is being converted to be "more like a user defined
// type", in that it will be driven less by specialized C structures and
// done more in usermode mechanics.  One glitch in that is this routine
// which apparently was used specifically on structs to allow their data
// pointer to come from an external memory address.  With STRUCT! relying
// on BINARY! for its storage, this introduces the concept of an
// "external binary".  While a generalized external binary might be
// an interesting feature, it would come at the cost that every series
// access on BINARY! for the data would have to check if it was loaded
// or not.
//
// This suggests perhaps that BINARY! can't be used and some kind of handle
// would instead.
//
// This is something that should be discussed with Atronix to figure out
// exactly why this was added
//
static REBSER *make_ext_storage(
    REBCNT len,
    REBINT raw_size,
    REBUPT raw_addr
) {
    if (raw_size >= 0 && raw_size != cast(REBINT, len))
        fail (Error(RE_INVALID_DATA));

    REBSER *ser = Make_Series(
        len + 1, // include term. !!! not included otherwise (?)
        1, // width
        MKS_EXTERNAL
    );

    SER_SET_EXTERNAL_DATA(ser, cast(REBYTE*, raw_addr));
    SET_SER_FLAG(ser, SERIES_FLAG_ACCESSIBLE); // accessible by default
    SET_SERIES_LEN(ser, len);

    MANAGE_SERIES(ser);
    return ser;
}


//
// This takes a spec like `[int32 [2]]` and sets the output field's properties
// such as `type`, `size`, `is_array`, `dimension`, etc.  It does this by
// recognizing a finite set of FFI type keywords defined in %words.r.
//
// This also allows for embedded structure types.  If the type is not being
// included by reference, but rather with a sub-definition inline, then it
// will actually be creating a new `inner` STRUCT! value.  Since this value
// is managed and not referred to elsewhere, there can't be evaluations.
//
static void Parse_Field_Type_May_Fail(
    struct Struct_Field *field,
    REBVAL *spec,
    REBVAL *inner,
    REBVAL **init
){
    RELVAL *val = VAL_ARRAY_AT(spec);

    if (IS_END(val))
        fail (Error(RE_MISC)); // !!! better error

    if (IS_WORD(val)) {

        switch (VAL_WORD_CANON(val)) {
        case SYM_UINT8:
            field->type = STRUCT_TYPE_UINT8;
            field->size = 1;
            break;

        case SYM_INT8:
            field->type = STRUCT_TYPE_INT8;
            field->size = 1;
            break;

        case SYM_UINT16:
            field->type = STRUCT_TYPE_UINT16;
            field->size = 2;
            break;

        case SYM_INT16:
            field->type = STRUCT_TYPE_INT16;
            field->size = 2;
            break;

        case SYM_UINT32:
            field->type = STRUCT_TYPE_UINT32;
            field->size = 4;
            break;

        case SYM_INT32:
            field->type = STRUCT_TYPE_INT32;
            field->size = 4;
            break;

        case SYM_UINT64:
            field->type = STRUCT_TYPE_UINT64;
            field->size = 8;
            break;

        case SYM_INT64:
            field->type = STRUCT_TYPE_INT64;
            field->size = 8;
            break;

        case SYM_FLOAT:
            field->type = STRUCT_TYPE_FLOAT;
            field->size = 4;
            break;

        case SYM_DOUBLE:
            field->type = STRUCT_TYPE_DOUBLE;
            field->size = 8;
            break;

        case SYM_POINTER:
            field->type = STRUCT_TYPE_POINTER;
            field->size = sizeof(void*);
            break;

        case SYM_STRUCT_TYPE:
            ++ val;
            if (IS_BLOCK(val)) {
                REBOOL res = MT_Struct(
                    inner, val, VAL_SPECIFIER(spec), REB_STRUCT
                );
                assert(res);

                field->size = SER_LEN(VAL_STRUCT_DATA_BIN(inner));
                field->type = STRUCT_TYPE_STRUCT;
                field->fields = VAL_STRUCT_FIELDLIST(inner);
                field->spec = VAL_STRUCT_SPEC(inner);
                *init = inner; // a shortcut for struct intialization
            }
            else
                fail (Error_Unexpected_Type(REB_BLOCK, VAL_TYPE(val)));
            break;

        case SYM_REBVAL:
            field->type = STRUCT_TYPE_REBVAL;
            field->size = sizeof(REBVAL);
            break;

        default:
            fail (Error_Invalid_Type(VAL_TYPE(val)));
        }
    }
    else if (IS_STRUCT(val)) {
        //
        // [b: [struct-a] val-a]
        //
        field->size = SER_LEN(VAL_STRUCT_DATA_BIN(val));
        field->type = STRUCT_TYPE_STRUCT;
        field->fields = VAL_STRUCT_FIELDLIST(val);
        field->spec = VAL_STRUCT_SPEC(val);
        COPY_VALUE(*init, val, VAL_SPECIFIER(spec));
    }
    else
        fail (Error_Invalid_Type(VAL_TYPE(val)));

    ++ val;

    if (IS_END(val)) {
        field->dimension = 1; // scalar
        field->is_array = 0; // FALSE, but bitfield must be integer
    }
    else if (IS_BLOCK(val)) {
        //
        // make struct! [a: [int32 [2]] [0 0]]
        //
        REBVAL ret;
        if (Do_At_Throws(
            &ret,
            VAL_ARRAY(val),
            VAL_INDEX(val),
            VAL_SPECIFIER(spec)
        )) {
            // !!! Does not check for thrown cases...what should this
            // do in case of THROW, BREAK, QUIT?
            fail (Error_No_Catch_For_Throw(&ret));
        }

        if (!IS_INTEGER(&ret))
            fail (Error_Unexpected_Type(REB_INTEGER, VAL_TYPE(val)));

        field->dimension = cast(REBCNT, VAL_INT64(&ret));
        field->is_array = 1; // TRUE, but bitfield must be integer
        ++ val;
    }
    else
        fail (Error_Invalid_Type(VAL_TYPE(val)));
}


//
//  MT_Struct: C
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
REBOOL MT_Struct(
    REBVAL *out, RELVAL *data, REBCTX *specifier, enum Reb_Kind type
) {
    if (!IS_BLOCK(data))
        fail (Error_Invalid_Arg_Core(data, specifier));

    REBINT max_fields = 16;

//
// SET UP SCHEMA
//
    // Every struct has a "schema"--this is a description (potentially
    // hierarchical) of its fields, including any nested structs.  The
    // schema should be shared between common instances of the same struct.
    //
    REBSER *field_1 = Make_Series(1, sizeof(struct Struct_Field), MKS_NONE);
    SET_SERIES_LEN(field_1, 1);

    struct Struct_Field *schema = SER_HEAD(struct Struct_Field, field_1);

    schema->spec = Copy_Array_Shallow(VAL_ARRAY(data), specifier);
    schema->type = STRUCT_TYPE_STRUCT;
    schema->is_array = FALSE;
    schema->sym = SYM_0; // no symbol for the struct itself
    schema->offset = 999999; // shouldn't be used
    schema->fields = Make_Series(
        max_fields, sizeof(struct Struct_Field), MKS_NONE
    );

    // `schema->size =` ...don't know until after the fields are made

//
// PROCESS FIELDS
//

    u64 offset = 0; // offset in data

    REBINT raw_size = -1;
    REBUPT raw_addr = 0;

    RELVAL *blk = VAL_ARRAY_AT(data);
    if (IS_BLOCK(blk)) {
        //
        // !!! This would suggest raw-size, raw-addr, or extern can be leading
        // in the struct definition, perhaps as:
        //
        //     make struct! [[raw-size] ...]
        //
        REBVAL specified;
        COPY_VALUE(&specified, blk, specifier);
        parse_attr(&specified, &raw_size, &raw_addr);
        ++ blk;
    }

    // !!! This makes binary data for each struct level? ???
    //
    REBSER *data_bin;
    if (raw_addr == 0)
        data_bin = Make_Binary(max_fields << 2);

    REBINT field_idx = 0; // for field index
    REBIXO eval_idx = 0; // for spec block evaluation
    REBCNT alignment = 0;

    while (NOT_END(blk)) {

        // Add another field...

        EXPAND_SERIES_TAIL(schema->fields, 1);
        struct Struct_Field *field = SER_AT(
            struct Struct_Field,
            schema->fields,
            field_idx
        );
        field->offset = (REBCNT)offset;

        // Must be a word or a set-word, with set-words initializing

        REBOOL expect_init;
        if (IS_SET_WORD(blk)) {
            expect_init = TRUE;
            if (raw_addr) {
                // initialization is not allowed for raw memory struct
                fail (Error_Invalid_Arg_Core(blk, specifier));
            }
        }
        else if (IS_WORD(blk))
            expect_init = FALSE;
        else
            fail (Error_Invalid_Type(VAL_TYPE(blk)));

        field->sym = VAL_WORD_SYM(blk);

        ++blk;
        if (IS_END(blk) || !IS_BLOCK(blk))
            fail (Error_Invalid_Arg_Core(blk, specifier));

        REBVAL spec;
        COPY_VALUE(&spec, blk, specifier);

        REBVAL inner;
        SET_BLANK(&inner);
        PUSH_GUARD_VALUE(&inner);

        REBVAL *init = NULL; // for result to save in data

        Parse_Field_Type_May_Fail(field, &spec, &inner, &init);

        u64 step = 0;

        ++ blk;

        STATIC_assert(sizeof(field->size) <= 4);
        STATIC_assert(sizeof(field->dimension) <= 4);

        step = (u64)field->size * (u64)field->dimension;
        if (step > VAL_STRUCT_LIMIT)
            fail (Error(RE_SIZE_LIMIT, out));

        if (raw_addr == 0)
            EXPAND_SERIES_TAIL(data_bin, step);

        if (expect_init) {
            REBVAL safe; // result of reduce or do (GC saved during eval)

            init = &safe;

            if (IS_END(blk))
               fail (Error_Invalid_Arg_Core(blk, specifier));

            if (IS_BLOCK(blk)) {
                if (Reduce_Array_Throws(
                    init,
                    VAL_ARRAY(blk),
                    0,
                    IS_SPECIFIC(blk)
                        ? VAL_SPECIFIER(KNOWN(blk))
                        : specifier,
                    FALSE
                )) {
                    fail (Error_No_Catch_For_Throw(init));
                }
                ++ blk;
            }
            else {
                eval_idx = DO_NEXT_MAY_THROW(
                    init,
                    VAL_ARRAY(data),
                    blk - VAL_ARRAY_AT(data),
                    specifier
                );
                if (eval_idx == THROWN_FLAG)
                    fail (Error_No_Catch_For_Throw(init));

                if (eval_idx == END_FLAG)
                    blk = KNOWN(VAL_ARRAY_TAIL(data));
                else
                    blk = KNOWN(VAL_ARRAY_AT_HEAD(data, eval_idx));
            }

            if (field->is_array) {
                if (IS_INTEGER(init)) { // interpreted as a C pointer
                    void *ptr = cast(void *, cast(REBUPT, VAL_INT64(init)));

                    // assume valid pointer to enough space
                    memcpy(
                        SER_AT(REBYTE, data_bin, cast(REBCNT, offset)),
                        ptr,
                        field->size * field->dimension
                    );
                }
                else if (IS_BLOCK(init)) {
                    REBCNT n = 0;

                    if (VAL_LEN_AT(init) != field->dimension)
                        fail (Error_Invalid_Arg(init));

                    // assign
                    for (n = 0; n < field->dimension; n ++) {
                        if (!assign_scalar_core(
                            data_bin,
                            offset,
                            field,
                            n,
                            KNOWN(VAL_ARRAY_AT_HEAD(init, n))
                        )) {
                            //RL_Print("Failed to assign element value\n");
                            fail (Error(RE_MISC));
                        }
                    }
                }
                else
                    fail (Error_Unexpected_Type(REB_BLOCK, VAL_TYPE(blk)));
            }
            else {
                // scalar
                if (!assign_scalar_core(data_bin, offset, field, 0, init)) {
                    //RL_Print("Failed to assign scalar value\n");
                    fail (Error(RE_MISC));
                }
            }
        }
        else if (raw_addr == 0) {
            if (field->type == STRUCT_TYPE_STRUCT) {
                REBCNT n = 0;
                for (n = 0; n < field->dimension; n ++) {
                    memcpy(
                        SER_AT(
                            REBYTE,
                            data_bin,
                            cast(REBCNT, offset) + n * field->size
                        ),
                        BIN_HEAD(VAL_STRUCT_DATA_BIN(init)),
                        field->size
                    );
                }
            }
            else if (field->type == STRUCT_TYPE_REBVAL) {
                REBCNT n = 0;
                for (n = 0; n < field->dimension; n ++) {
                    if (!assign_scalar(
                        VAL_STRUCT(out), field, n, VOID_CELL
                    )) {
                        //RL_Print("Failed to assign element value\n");
                        fail (Error(RE_MISC));
                    }
                }
            }
            else {
                memset(
                    SER_AT(REBYTE, data_bin, cast(REBCNT, offset)),
                    0,
                    field->size * field->dimension
                );
            }
        }

        offset +=  step;

        //if (alignment != 0) {
        //  offset = ((offset + alignment - 1) / alignment) * alignment;

        if (offset > VAL_STRUCT_LIMIT)
            fail (Error(RE_SIZE_LIMIT, out));

        field->done = 1; // TRUE, but bitfields must be integer

        ++ field_idx;

        DROP_GUARD_VALUE(&inner);
    }


//
// FINALIZE VALUE
//

    schema->size = offset; // now we know the total size so save it
    ENSURE_ARRAY_MANAGED(schema->spec);
    MANAGE_SERIES(schema->fields);

    REBSTU *stu = Make_Singular_Array(VOID_CELL);
    MANAGE_SERIES(field_1);
    ARR_SERIES(stu)->misc.schema = field_1;

    VAL_RESET_HEADER(out, REB_STRUCT);
    out->payload.structure.stu = stu;
    if (raw_addr) {
        out->payload.structure.data
            = make_ext_storage(schema->size, raw_size, raw_addr);
    }
    else {
        MANAGE_SERIES(data_bin);
        out->payload.structure.data = data_bin;
    }
    out->payload.structure.offset = 0;

    *ARR_HEAD(stu) = *out;
    assert(ARR_LEN(stu) == 1);
    MANAGE_ARRAY(stu);

    return TRUE; // always either returns TRUE or fail()s
}


//
//  PD_Struct: C
//
REBINT PD_Struct(REBPVS *pvs)
{
    struct Struct_Field *field = NULL;
    REBCNT i = 0;
    REBSTU *stu = VAL_STRUCT(pvs->value);
    if (!IS_WORD(pvs->selector))
        fail (Error_Bad_Path_Select(pvs));

    fail_if_non_accessible(KNOWN(pvs->value));

    if (!pvs->opt_setval || NOT_END(pvs->item + 1)) {
        if (!Get_Struct_Var(stu, pvs->selector, pvs->store))
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
            // !!! This is dodgy; it has to copy (as selector is a pointer to
            // a memory cell it may not own), has to guard (as the next path
            // evaluation may not protect the result...)
            //
            REBVAL sel_orig = *pvs->selector;
            PUSH_GUARD_VALUE(&sel_orig);

            pvs->value = pvs->store;
            pvs->value_specifier = SPECIFIED;

            if (Next_Path_Throws(pvs)) { // updates pvs->store, pvs->selector
                DROP_GUARD_VALUE(&sel_orig);
                fail (Error_No_Catch_For_Throw(pvs->store)); // !!! Review
            }

            {
                REBVAL specific;
                COPY_VALUE(&specific, pvs->value, pvs->value_specifier);

                if (!Set_Struct_Var(stu, &sel_orig, pvs->selector, &specific))
                    fail (Error_Bad_Path_Set(pvs));
            }

            DROP_GUARD_VALUE(&sel_orig);

            return PE_OK;
        }

        return PE_USE_STORE;
    }
    else {
        // setting (because opt_setval is non-NULL, and at end of path)

        if (!Set_Struct_Var(stu, pvs->selector, NULL, pvs->opt_setval))
            fail (Error_Bad_Path_Set(pvs));

        return PE_OK;
    }

    fail (Error_Bad_Path_Select(pvs));
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
    //printf("comparing struct a (%p) with b (%p), mode: %d\n", a, b, mode);
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
                    BIN_HEAD(VAL_STRUCT_DATA_BIN(a)),
                    BIN_HEAD(VAL_STRUCT_DATA_BIN(b)),
                    VAL_STRUCT_SIZE(a)
                )
            );

        default:
            return -1;
    }
    return -1;
}


//
//  Copy_Struct: C
//
REBSTU *Copy_Struct_Managed(REBSTU *src)
{
    // !!! Whatever this is it will need to be done another way now.  Note:
    // review what an "external struct" was supposed to be.
    //
    //fail_if_non_accessible(STU_TO_VAL(src));

    assert(ARR_LEN(src) == 2);
    assert(IS_HANDLE(ARR_AT(src, 0)));
    assert(IS_BINARY(ARR_AT(src, 1)));

    // A shallow copy will get the schema as-is (plain HANDLE!) but will not
    // get new instance data for the binary in slot 2.
    //
    REBSTU *copy = Copy_Array_Shallow(src, SPECIFIED);

    // Update the binary data with a copy of its sequence.
    //
    // !!! Note that this leaves the offset intact, and will wind up making a
    // copy as big as struct the instance is embedded into if nonzero offset.

    REBSER *bin_copy = Copy_Sequence(VAL_SERIES(ARR_AT(copy, 1)));
    INIT_VAL_SERIES(ARR_AT(copy, 1), bin_copy);
    assert(STU_DATA_BIN(copy) == bin_copy);

    MANAGE_SERIES(bin_copy);
    MANAGE_ARRAY(copy);
    return copy;
}


//
// a: make struct! [uint 8 i: 1]
// b: make a [i: 10]
//
static void init_fields(REBVAL *ret, REBVAL *spec)
{
    REBVAL *blk = NULL;

    for (blk = KNOWN(VAL_ARRAY_AT(spec)); NOT_END(blk); blk += 2) {
        unsigned int i = 0;
        REBVAL *word = blk;
        REBVAL *fld_val = blk + 1;

        if (IS_BLOCK(word)) { // options: raw-memory, etc
            REBINT raw_size = -1;
            REBUPT raw_addr = 0;

            // make sure no other field initialization
            if (VAL_LEN_HEAD(spec) != 1)
                fail (Error_Invalid_Arg(spec));

            parse_attr(word, &raw_size, &raw_addr);
            ret->payload.structure.data
                 = make_ext_storage(VAL_STRUCT_SIZE(ret), raw_size, raw_addr);

            break;
        }
        else if (! IS_SET_WORD(word))
            fail (Error_Invalid_Arg(word));

        if (IS_END(fld_val))
            fail (Error(RE_NEED_VALUE, fld_val));

        REBSER *fieldlist = VAL_STRUCT_FIELDLIST(ret);

        for (i = 0; i < SER_LEN(fieldlist); i ++) {
            struct Struct_Field *fld
                = SER_AT(struct Struct_Field, fieldlist, i);

            if (fld->sym == VAL_WORD_CANON(word)) {
                if (fld->dimension > 1) {
                    REBCNT n = 0;
                    if (IS_BLOCK(fld_val)) {
                        if (VAL_LEN_AT(fld_val) != fld->dimension)
                            fail (Error_Invalid_Arg(fld_val));

                        for(n = 0; n < fld->dimension; n ++) {
                            if (!assign_scalar(
                                VAL_STRUCT(ret),
                                fld,
                                n,
                                KNOWN(VAL_ARRAY_AT_HEAD(fld_val, n))
                            )) {
                                fail (Error_Invalid_Arg(fld_val));
                            }
                        }
                    }
                    else if (IS_INTEGER(fld_val)) {
                        void *ptr = cast(void *,
                            cast(REBUPT, VAL_INT64(fld_val))
                        );

                        // assuming valid pointer to enough space
                        memcpy(
                            SER_AT(
                                REBYTE,
                                VAL_STRUCT_DATA_BIN(ret),
                                fld->offset
                            ),
                            ptr,
                            fld->size * fld->dimension
                        );
                    }
                    else
                        fail (Error_Invalid_Arg(fld_val));
                }
                else {
                    if (!assign_scalar(VAL_STRUCT(ret), fld, 0, fld_val))
                        fail (Error_Invalid_Arg(fld_val));
                }
                break;
            }
        }

        if (i == SER_LEN(fieldlist))
            fail (Error_Invalid_Arg(word)); // field not in the parent struct
    }
}


//
//  REBTYPE: C
//
REBTYPE(Struct)
{
    REBVAL *val;
    REBVAL *arg;
    REBVAL *ret;

    val = D_ARG(1);

    ret = D_OUT;

    SET_VOID(ret);
    // unary actions
    switch(action) {
        case A_MAKE:
            //RL_Print("%s, %d, Make struct action\n", __func__, __LINE__);
        case A_TO:
            //RL_Print("%s, %d, To struct action\n", __func__, __LINE__);
            arg = D_ARG(2);

            // Clone an existing STRUCT:
            if (IS_STRUCT(val)) {
                VAL_RESET_HEADER(ret, REB_STRUCT);
                ret->payload.structure.stu
                    = Copy_Struct_Managed(VAL_STRUCT(val));

                // only accept value initialization
                init_fields(ret, arg);
            } else if (!IS_DATATYPE(val)) {
                goto is_arg_error;
            } else {
                // Initialize STRUCT from block:
                // make struct! [float a: 0]
                // make struct! [double a: 0]
                if (IS_BLOCK(arg)) {
                    if (!MT_Struct(ret, arg, VAL_SPECIFIER(arg), REB_STRUCT)) {
                        goto is_arg_error;
                    }
                }
                else
                    fail (Error_Bad_Make(REB_STRUCT, arg));
            }
            VAL_RESET_HEADER(ret, REB_STRUCT);
            break;

        case A_CHANGE:
            {
                arg = D_ARG(2);
                if (!IS_BINARY(arg))
                    fail (Error_Unexpected_Type(REB_BINARY, VAL_TYPE(arg)));

                if (VAL_LEN_AT(arg) != SER_LEN(VAL_STRUCT_DATA_BIN(val)))
                    fail (Error_Invalid_Arg(arg));

                memcpy(
                    BIN_HEAD(VAL_STRUCT_DATA_BIN(val)),
                    BIN_HEAD(VAL_SERIES(arg)),
                    BIN_LEN(VAL_STRUCT_DATA_BIN(val))
                );
            }
            break;

        case A_REFLECT:
            {
                REBINT n;
                arg = D_ARG(2);
                n = VAL_WORD_CANON(arg); // zero on error
                switch (n) {
                    case SYM_VALUES:
                        fail_if_non_accessible(val);
                        Val_Init_Binary(ret, Copy_Sequence_At_Len(VAL_STRUCT_DATA_BIN(val), VAL_STRUCT_OFFSET(val), VAL_STRUCT_SIZE(val)));
                        break;
                    case SYM_SPEC:
                        Val_Init_Block(
                            ret,
                            Copy_Array_Deep_Managed(
                                VAL_STRUCT_SPEC(val), SPECIFIED
                            )
                        );
                        Unbind_Values_Deep(VAL_ARRAY_HEAD(val));
                        break;
                    case SYM_ADDR:
                        SET_INTEGER(
                            ret,
                            cast(REBUPT, SER_AT(
                                REBYTE,
                                VAL_STRUCT_DATA_BIN(val),
                                VAL_STRUCT_OFFSET(val)
                            ))
                        );
                        break;
                    default:
                        fail (Error_Cannot_Reflect(REB_STRUCT, arg));
                }
            }
            break;

        case A_LENGTH:
            SET_INTEGER(ret, SER_LEN(VAL_STRUCT_DATA_BIN(val)));
            break;
        default:
            fail (Error_Illegal_Action(REB_STRUCT, action));
    }
    return R_OUT;

is_arg_error:
    fail (Error_Unexpected_Type(REB_STRUCT, VAL_TYPE(arg)));
}


//
//  destroy-struct-storage: native [
//
//  {Destroy the external memory associated the struct}
//      s   [struct!]
//      /free func [function!] {Specify the function to free the memory}
//  ]
//
REBNATIVE(destroy_struct_storage)
{
    PARAM(1, val);
    REFINE(2, free_q);
    PARAM(3, free_func);

    if (REF(free_q)) {
        if (IS_FUNCTION_RIN(ARG(free_func)))
            fail (Error(RE_FREE_NEEDS_ROUTINE));
    }

    return Destroy_External_Storage(D_OUT,
                                    VAL_STRUCT_DATA_BIN(ARG(val)),
                                    REF(free_q)? ARG(free_func) : NULL);
}
