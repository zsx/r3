//
//  File: %sys-action.h
//  Summary: "Definition of action dispatchers"
//  Section: core
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


// enums in C have no guaranteed size, yet Rebol wants to use known size
// types in its interfaces.  Hence REB_R is a REBCNT from reb-c.h (and not
// this enumerated type containing its legal values).
enum {
    R_OUT = 0,

    // See comments on OPT_VALUE_THROWN about the migration of "thrownness"
    // from being a property signaled to the evaluator.
    //
    // R_OUT_IS_THROWN is a test of that signaling mechanism.  It is currently
    // being kept in parallel with the THROWN() bit and ensured as matching.
    // Being in the state of doing a stack unwind will likely be knowable
    // through other mechanisms even once the thrown bit on the value is
    // gone...so it may not be the case that natives are asked to do their
    // own separate indication, so this may wind up replaced with R_OUT.  For
    // the moment it is good as a double-check.
    //
    R_OUT_IS_THROWN,

    // This is a return value in service of the /? functions.  Since all
    // dispatchers receive an END marker in the f->out slot (a.k.a. D_OUT)
    // then it can be used to tell if the output has been written "in band"
    // by a legal value or void.  This returns TRUE if D_OUT is not END,
    // and FALSE if it still is.
    //
    R_OUT_TRUE_IF_WRITTEN,

    // Similar to R_OUT_WRITTEN_Q, this converts an illegal END marker return
    // value in R_OUT to simply a void.
    //
    R_OUT_VOID_IF_UNWRITTEN,

    // !!! These R_ values are somewhat superfluous...and actually inefficient
    // because they have to be checked by the caller in a switch statement
    // to take the equivalent action.  They have a slight advantage in
    // hand-written C code for making it more clear that if you have used
    // the D_OUT return slot for temporary work that you explicitly want
    // to specify another result...this cannot be caught by the REB_TRASH
    // trick for detecting an unwritten D_OUT.
    //
    R_VOID, // => SET_VOID(D_OUT); return R_OUT;
    R_BLANK, // => SET_BLANK(D_OUT); return R_OUT;
    R_TRUE, // => SET_TRUE(D_OUT); return R_OUT;
    R_FALSE, // => SET_FALSE(D_OUT); return R_OUT;

    // If Do_Core gets back an R_REDO from a dispatcher, it will re-execute
    // the f->func in the frame.  This function may be changed by the
    // dispatcher from what was originally called.
    //
    R_REDO
};
typedef REBCNT REB_R;

// Convenience function for getting the "/?" behavior if it is enabled, and
// doing the default thing--assuming END is being left in the D_OUT slot
//
inline static REB_R R_OUT_Q(REBOOL q) {
    if (q) return R_OUT_TRUE_IF_WRITTEN;
    return R_OUT_VOID_IF_UNWRITTEN;
}

// NATIVE! function
typedef REB_R (*REBNAT)(struct Reb_Frame *frame_);
#define REBNATIVE(n) \
    REB_R N_##n(struct Reb_Frame *frame_)

// ACTION! function (one per each DATATYPE!)
typedef REB_R (*REBACT)(struct Reb_Frame *frame_, REBSYM a);
#define REBTYPE(n) \
    REB_R T_##n(struct Reb_Frame *frame_, REBSYM action)

// PORT!-action function
typedef REB_R (*REBPAF)(struct Reb_Frame *frame_, REBCTX *p, REBSYM a);

// COMMAND! function
typedef REB_R (*CMD_FUNC)(REBCNT n, REBSER *args);

typedef struct Reb_Routine_Info REBRIN;
