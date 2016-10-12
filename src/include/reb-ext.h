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

// !!! Going forward in terms of the "RL_" API (for those who do not have
// access to the sensitive internal details of the interpreter), it will only
// speak in terms of REBVALs.  So instead of proxying the volatile stack-based
// `struct Reb_Frame *`, the same protections as of a FRAME! will be given
// in the evaluator.  (e.g. Through the REBVAL it will be possible to tell if
// the frame is off the stack and `fail()` instead of crashing.)
//
// (Right now this concept is in its early stages, so there are still direct
// exports of subclasses of REBSER and other things that are too bit fiddly
// to be putting in the non-internal API.  This will be adapted over time as
// the API develops.)
//
// Note that REBVAL is "opaque" as far as the RL_API clients know, and they
// can only extract and adjust properties through the API.  However, these
// pointers are to legitimate REBVALs.
//
typedef REBVAL RXIARG;
typedef REBVAL RXIFRM;


typedef struct rxi_cmd_context {
    void *envr;     // for holding a reference to your environment
    REBARR *block;  // block being evaluated
    REBCNT index;   // 0-based index of current command in block
} REBCEC;

typedef unsigned char REBRXT;
typedef int (*RXICAL)(int cmd, const REBVAL *frame, REBCEC *ctx);


#define RXA_COUNT(f) \
    RL_FRM_NUM_ARGS(f)

#define RXA_ARG(f,n) \
    RL_FRM_ARG((f), (n))

#define RXA_REF(f,n) \
    RL_VAL_LOGIC(RL_FRM_ARG((f), (n)))
    
#define RXA_TYPE(f,n) \
    RL_VAL_TYPE(RL_FRM_ARG((f), (n)))

#define RXA_INT64(f,n) \
    RL_VAL_INT64(RL_FRM_ARG((f), (n)))

#define RXA_INT32(f,n) \
    RL_VAL_INT32(RL_FRM_ARG((f), (n)))

#define RXA_DEC64(f,n) \
    RL_VAL_DECIMAL(RL_FRM_ARG((f), (n)))

#define RXA_LOGIC(f,n) \
    RL_VAL_LOGIC(RL_FRM_ARG((f), (n)))

#define RXA_CHAR(f,n) \
    RL_VAL_CHAR(RL_FRM_ARG((f), (n)))

#define RXA_TIME(f,n) \
    RL_VAL_TIME(RL_FRM_ARG((f), (n)))

#define RXA_DATE(f,n) \
    RL_VAL_DATE(RL_FRM_ARG((f), (n)))

// !!! See notes on RL_Val_Word_Canon_Or_Logic
#define RXA_WORD(f,n) \
    RL_VAL_WORD_CANON_OR_LOGIC(RL_FRM_ARG((f), (n)))

#define RXA_PAIR(f,n) \
    RL_FRM_ARG((f), (n)) // no "pair" object, just return the value

#define RXA_TUPLE(f,n) \
    RL_VAL_TUPLE_DATA(RL_FRM_ARG((f), (n)))

#define RXA_SERIES(f,n) \
    RL_VAL_SERIES(RL_FRM_ARG((f), (n)))

#define RXA_INDEX(f,n) \
    RL_VAL_INDEX(RL_FRM_ARG((f), (n)))

#define RXA_HANDLE(f,n) \
    RL_VAL_HANDLE_DATA(RL_FRM_ARG((f), (n)))

#define RXA_OBJECT(f,n) \
    RL_VAL_CONTEXT(RL_FRM_ARG((f), (n)))

#define RXA_IMAGE(f,n) \
    RL_VAL_SERIES(RL_FRM_ARG((f), (n)))

#define RXA_IMAGE_BITS(f,n) \
    cast(REBYTE*, RL_SERIES(RXA_IMAGE(f,n), RXI_SER_DATA))

#define RXA_IMAGE_WIDTH(f,n) \
    RL_VAL_IMAGE_WIDE(RL_FRM_ARG((f), (n)))

#define RXA_IMAGE_HEIGHT(f,n) \
    RL_VAL_IMAGE_HIGH(RL_FRM_ARG((f), (n)))
    
#define RXA_COLOR_TUPLE(f,n) \
    (TO_RGBA_COLOR(RXA_TUPLE(f,n)[1], RXA_TUPLE(f,n)[2], RXA_TUPLE(f,n)[3], \
        RXA_TUPLE(f,n)[0] > 3 ? RXA_TUPLE(f,n)[4] : 0xff)) //always RGBA order

#define RXI_LOG_PAIR(v) \
    {LOG_COORD_X(RL_VAL_PAIR_X_FLOAT(v)), LOG_COORD_Y(RL_VAL_PAIR_Y_FLOAT(v))}

#define RXA_LOG_PAIR(f,n) \
    RXI_LOG_PAIR(RL_FRM_ARG((f), (n)))

// Command function return values:
enum rxi_return {
    RXR_VOID,
    RXR_BLANK,
    RXR_TRUE,
    RXR_FALSE,

    RXR_VALUE,
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
