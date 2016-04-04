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
