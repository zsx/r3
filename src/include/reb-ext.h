//
//  File: %reb-ext.h
//  Summary: "R3-Alpha Extension Mechanism API"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
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
// This contains support routines for what was known in the R3-Alpha as a
// "COMMAND!".  This was a way of extending Rebol using C routines that
// could be wrapped to act as a Rebol function:
//
// http://www.rebol.com/r3/docs/concepts/extensions-embedded.html
//
// Like a "Native", a "Command" is implemented as a C function.  Unlike a
// native, a command cannot directly process REBVAL*s.  Instead, it speaks in
// tems of something called an RXIARG--which is a very thin abstraction to
// permit interacting with some kinds of Rebol values.
//
// Operations on RXIARG values are a parallel subset to operations on REBVALs,
// but using entirely different routines and constants.  So getting the
// RXI_TYPE(rxiarg) could come back with RXT_BLOCK, while VAL_TYPE(value)
// for the same item would give REB_BLOCK.  This duplication is intended to
// provide a layer of abstraction so that changes to the internals (that
// Rebol natives would have to deal with) would not necessarily need to
// affect extension code.
//
// As a way of attempting to make it easier to maintain these parallel APIs,
// Rebol scripts that are part of the build process would produce things
// like the enumerated types from tables.  See %make-host-ext.r
//
// For the implementation of dispatch connecting Rebol to extensions, see
// the code in %f-extension.c.
//
// Subsequent to the open-sourcing, the Ren-C initiative is not focusing on
// the REB_COMMAND model--preferring to connect the Rebol core directly as
// a library to bindings.  However, as it was the only extension model
// available under closed-source Rebol, several pieces of code were built
// to depend upon it for functionality.  This included the cryptography
// extensions needed for secure sockets and a large part of R3-View.
//
// Being able to quarantine the REB_COMMAND machinery to only builds that
// need it is a working objective.
//

// Naming conventions:
//
// RL:  REBOL library API function (or function access macro)
// RXI: REBOL eXtensions Interface (general constructs)
// RXA: REBOL eXtensions function Argument (value)
// RXR: REBOL eXtensions function Return types
// RXE: REBOL eXtensions Error codes
//

#include "reb-defs.h"
#include "ext-types.h"


// Value structure (for passing args to and from):
#pragma pack(4)
typedef union rxi_arg_val {
    void *addr;
    i64 int64;
    double dec64;
    REBXYF pair;
    REBYTE bytes[8];
    struct {
        i32 int32a;
        i32 int32b;
    } i2;
    struct {
        REBD32 dec32a;
        REBD32 dec32b;
    } d2;
    struct {
        REBSER *series;
        u32 index;
    } sri;
    REBSER *context; // !!! never assigned, seems to expect `series` overlap
    struct {
        REBSER *image;
        int width:16;
        int height:16;
    } iwh;
} RXIARG;

// For direct access to arg array:
#define RXI_COUNT(a)    (a[0].bytes[0])
#define RXI_TYPE(a,n)   (a[0].bytes[n])
#define RXI_COLOR_TUPLE(a) \
    (TO_RGBA_COLOR(a.bytes[1], a.bytes[2], a.bytes[3], a.bytes[0] > 3 \
        ? a.bytes[4] \
        : 0xff)) //always RGBA order

// Command function call frame:
#define RXIFRM_MAX_ARGS 8
typedef struct rxi_cmd_frame {
    RXIARG rxiargs[RXIFRM_MAX_ARGS]; // arg values (64 bits each)
} RXIFRM;

typedef struct rxi_cmd_context {
    void *envr;     // for holding a reference to your environment
    REBARR *block;  // block being evaluated
    REBCNT index;   // 0-based index of current command in block
} REBCEC;

typedef unsigned char REBRXT;
typedef int (*RXICAL)(int cmd, RXIFRM *args, REBCEC *ctx);

#pragma pack()

// Access macros (indirect access via RXIFRM pointer):
#define RXA_ARG(f,n)    ((f)->rxiargs[n])
#define RXA_COUNT(f)    (RXA_ARG(f,0).bytes[0]) // number of args
#define RXA_TYPE(f,n)   (RXA_ARG(f,0).bytes[n]) // types (of first 7 args)
#define RXA_REF(f,n)    (RXA_ARG(f,n).i2.int32a)

#define RXA_INT64(f,n)  (RXA_ARG(f,n).int64)
#define RXA_INT32(f,n)  (i32)(RXA_ARG(f,n).int64)
#define RXA_DEC64(f,n)  (RXA_ARG(f,n).dec64)
#define RXA_LOGIC(f,n)  (RXA_ARG(f,n).i2.int32a)
#define RXA_CHAR(f,n)   (RXA_ARG(f,n).i2.int32a)
#define RXA_TIME(f,n)   (RXA_ARG(f,n).int64)
#define RXA_DATE(f,n)   (RXA_ARG(f,n).i2.int32a)
#define RXA_WORD(f,n)   (RXA_ARG(f,n).i2.int32a)
#define RXA_LOG_PAIR(f,n) \
    {LOG_COORD_X(RXA_ARG(f,n).pair.x), LOG_COORD_Y(RXA_ARG(f,n).pair.y)}
#define RXA_PAIR(f,n)   (RXA_ARG(f,n).pair)
#define RXA_TUPLE(f,n)  (RXA_ARG(f,n).bytes)
#define RXA_SERIES(f,n) (RXA_ARG(f,n).sri.series)
#define RXA_INDEX(f,n)  (RXA_ARG(f,n).sri.index)
#define RXA_HANDLE(f,n) (RXA_ARG(f,n).addr)
#define RXA_OBJECT(f,n) (RXA_ARG(f,n).context)
#define RXA_MODULE(f,n) (RXA_ARG(f,n).context)
#define RXA_IMAGE(f,n)  (RXA_ARG(f,n).iwh.image)
#define RXA_IMAGE_BITS(f,n) \
	cast(REBYTE *, RL_SERIES((RXA_ARG(f,n).iwh.image), RXI_SER_DATA))
#define RXA_IMAGE_WIDTH(f,n)  (RXA_ARG(f,n).iwh.width)
#define RXA_IMAGE_HEIGHT(f,n) (RXA_ARG(f,n).iwh.height)
#define RXA_COLOR_TUPLE(f,n) \
    (TO_RGBA_COLOR(RXA_TUPLE(f,n)[1], RXA_TUPLE(f,n)[2], RXA_TUPLE(f,n)[3], \
        RXA_TUPLE(f,n)[0] > 3 ? RXA_TUPLE(f,n)[4] : 0xff)) //always RGBA order

#define RXI_LOG_PAIR(v) {LOG_COORD_X(v.pair.x) , LOG_COORD_Y(v.pair.y)}

// Command function return values:
enum rxi_return {
    RXR_UNSET,
    RXR_NONE,
    RXR_TRUE,
    RXR_FALSE,

    RXR_VALUE,
    RXR_BLOCK,
    RXR_ERROR,
    RXR_BAD_ARGS,
    RXR_NO_COMMAND,
    RXR_MAX
};

// Used with RXI_SERIES_INFO:
enum {
    RXI_SER_DATA,   // pointer to data
    RXI_SER_TAIL,   // series tail index (length of data)
    RXI_SER_SIZE,   // size of series (in units)
    RXI_SER_WIDE,   // width of series (in bytes)
    RXI_SER_LEFT,   // units free in series (past tail)
    RXI_MAX
};

// Error Codes (returned in result value from some API functions):
enum {
    RXE_NO_ERROR,
    RXE_NO_WORD,    // the word cannot be found (e.g. in an object)
    RXE_NOT_FUNC,   // the value is not a function (for callback)
    RXE_BAD_ARGS,   // function arguments to not match
    RXE_MAX
};

#define SET_EXT_ERROR(v,n) ((v)->i2.int32a = (n))
#define GET_EXT_ERROR(v)   ((v)->i2.int32a)
