//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
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
//  Summary: Evaluator "Do State" and Helpers
//  File: %sys-do.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A Reb_Call structure represents the fixed-size portion for a function's
// call frame.  It is stack allocated, and is used by both Do and Apply.
// (If a dynamic allocation is necessary for the call frame, that dynamic
// portion is allocated as an array in `arglist`.)
//
// The contents of the call frame are all the input and output parameters
// for a call to the evaluator--as well as all of the internal state needed
// by the evaluator loop.  The reason that all the information is exposed
// in this way is to make it faster and easier to delegate branches in
// the Do loop--without bearing the overhead of setting up new stack state.
//

// The flags are specified either way for clarity.
enum {
    // Calls may be created for use by the Do evaluator or by an apply
    // originating from inside the C code itself.  If the call is not for
    // the evaluator, it should not set DO_FLAG_DO and no other flags
    // should be set.
    //
    DO_FLAG_DO = 1 << 0,

    // As exposed by the DO native and its /NEXT refinement, a call to the
    // evaluator can either run to the finish from a position in an array
    // or just do one eval.  Rather than achieve execution to the end by
    // iterative function calls to the /NEXT variant (as in R3-Alpha), Ren-C
    // offers a controlling flag to do it from within the core evaluator
    // as a loop.
    //
    // However: since running to the end follows a different code path than
    // performing DO/NEXT several times, it is important to ensure they
    // achieve equivalent results.  Nuances to preserve this invariant are
    // mentioned from within the code.
    //
    DO_FLAG_NEXT = 1 << 2,
    DO_FLAG_TO_END = 1 << 3,

    // When we're in mid-dispatch of an infix function, the precedence is such
    // that we don't want to do further infix lookahead while getting the
    // arguments.  (e.g. with `1 + 2 * 3` we don't want infix `+` to
    // "look ahead" past the 2 to see the infix `*`)
    //
    DO_FLAG_LOOKAHEAD = 1 << 4,
    DO_FLAG_NO_LOOKAHEAD = 1 << 5
};

enum Reb_Call_Mode {
    CALL_MODE_0, // no special mode signal
    CALL_MODE_ARGS, // ordinary arguments before any refinements seen
    CALL_MODE_REFINE_PENDING, // picking up refinement arguments, none yet
    CALL_MODE_REFINE_ARGS, // at least one refinement has been found
    CALL_MODE_SCANNING, // looking for refinements (used out of order)
    CALL_MODE_SKIPPING, // in the process of skipping an unused refinement
    CALL_MODE_REVOKING, // found an unset and aiming to revoke refinement use
    CALL_MODE_FUNCTION // running an ANY-FUNCTION!
};

// NOTE: The ordering of the fields in `Reb_Call` are specifically done so
// as to accomplish correct 64-bit alignment of pointers on 64-bit systems
// (as long as REBCNT and REBINT are 32-bit on such platforms).  If modifying
// the structure, be sensitive to this issue.
//
struct Reb_Call {
    //
    // `cell` [INTERNAL, REUSABLE, GC-SAFE cell]
    //
    // Some evaluative operations need a unit of additional storage beyond
    // the one available in `out`.  This is a one-REBVAL-sized cell for
    // saving that data, and natives may make use of during their call.
    // At front of struct for alignment.
    //
    REBVAL cell;

    // `func` [INTERNAL, READ-ONLY, GC-PROTECTED]
    //
    // A copy of the function value when a call is in effect.  This is needed
    // to make the function value stable, and not get pulled out from under
    // the call frame.  That could happen due to a modification of the series
    // where the evaluating function lived.  At front of struct for alignment.
    //
    REBFUN *func;

    // `dsp_orig` [INTERNAL, READ-ONLY]
    //
    // The data stack pointer captured on entry to the evaluation.  It is used
    // by debug checks to make sure the data stack stays balanced after each
    // sub-operation.  Yet also, refinements are pushed to the data stack and
    // something to compare against to find out how many is needed.  At this
    // position to sync alignment with same-sized `flags`.
    //
    REBINT dsp_orig;

    // `flags` [INPUT, READ-ONLY (unless FRAMELESS signaling error)]
    //
    // These are DO_FLAG_xxx or'd together.  If the call is being set up
    // for an Apply as opposed to Do, this must be 0.
    //
    REBCNT flags;

    // `out` [INPUT pointer of where to write an OUTPUT, GC-SAFE cell]
    //
    // This is where to write the result of the evaluation.  It should not be
    // in "movable" memory, hence not in a series data array.  Often it is
    // used as an intermediate free location to do calculations en route to
    // a final result, due to being GC-safe
    //
    REBVAL *out;

    // `value` [INPUT, REUSABLE, GC-PROTECTS pointed-to REBVAL]
    //
    // This is the value currently being processed.  Callers pass in the
    // first value pointer...which for any successive evalutions will be
    // updated via picking from `array` based on `index`.  But having the
    // caller pass in the initial value gives the *option* of that value
    // not being resident in the series.
    //
    // (Hence if one has the series `[[a b c] [d e]]` it would be possible to
    // have an independent path value `append/only` and NOT insert it in the
    // series, yet get the effect of `append/only [a b c] [d e]`.  This only
    // works for one value, but is a convenient no-cost trick for apply-like
    // situations...as insertions usually have to "slide down" the values in
    // the series and may also need to perform alloc/free/copy to expand.)
    //
    const REBVAL *value;

    // array [INPUT, READ-ONLY, GC-PROTECTED]
    //
    // The array from which new values will be fetched.  Although the
    // series may have come from a ANY-ARRAY! (e.g. a PATH!), the distinction
    // does not exist at this point...so it will "evaluate like a block".
    //
    REBARR *array;

    // `index` [INPUT, OUTPUT]
    //
    // Index into the array from which new values are fetched after the
    // initial `value`.  Successive fetching is always done by index and
    // not by incrementing `value` for several reasons, though one is to
    // avoid crashing if the input array is modified during the evaluation.
    //
    // !!! While it doesn't *crash*, a good user-facing explanation of
    // handling what it does instead seems not to have been articulated!  :-/
    //
    // At the end of the evaluation it's the index of the next expression
    // to be evaluated, THROWN_FLAG, or END_FLAG.
    //
    REBCNT index;

    // `label_sym` [INTERNAL, READ-ONLY]
    //
    // Functions don't have "names", though they can be assigned
    // to words.  If a function is invoked via word lookup (vs. a literal
    // FUNCTION! value), 'label_sym' will be that WORD!, and a placeholder
    // otherwise.  Placed here for 64-bit alignment after same-size `index`.
    //
    REBCNT label_sym;

    // `arglist` [INTERNAL, VALUES MUTABLE and GC-SAFE if FRAMED]
    //
    // The arglist is an array containing the evaluated arguments with which
    // a function is being invoked (if it is not frameless).  It will be a
    // manually-memory managed series which is freed when the call finishes,
    // or cleaned up in error processing).
    //
    // An exception to this is if `func` is a CLOSURE!.  In that case, it
    // will take ownership of the constructed array, give it over to GC
    // management, and set this field to NULL.
    //
    union {
        REBARR *array;
        REBVAL *chunk;
    } arglist;

    // `param` [INTERNAL, REUSABLE, GC-PROTECTS pointed-to REBVALs]
    //
    // We use the convention that "param" refers to the TYPESET! (plus symbol)
    // from the spec of the function--a.k.a. the "formal argument".  This
    // pointer is moved in step with `arg` during argument fulfillment.
    //
    REBVAL *param;

    // `arg` [INTERNAL, also CACHE of `ARRAY_HEAD(arglist)`]
    //
    // "arg" is the "actual argument"...which holds the pointer to the
    // REBVAL slot in the `arglist` for that corresponding `param`.  These
    // are moved in sync during parameter fulfillment.
    //
    // While a function is running, `arg` is a cache to the data pointer for
    // arglist.  It is used by the macros ARG() and PARAM()...which index
    // by integer constants and may be used several times.  Avoiding the
    // extra indirection can be beneficial.
    //
    REBVAL *arg;

    // `refine` [INTERNAL, REUSABLE, GC-PROTECTS pointed-to REBVAL]
    //
    // The `arg` slot of the refinement currently being processed.  We have to
    // remember its address in case we later see all its arguments are UNSET!,
    // and want to go back and "revoke" it by setting the arg to NONE!.
    //
    REBVAL *refine;

    // `prior` [INTERNAL, READ-ONLY]
    //
    // The prior call frame (may be NULL if this is the topmost stack call).
    //
    struct Reb_Call *prior;

    // `mode` [INTERNAL, READ-ONLY]
    //
    // State variable during parameter fulfillment.  So before refinements,
    // in refinement, skipping...etc.
    //
    // One particularly important usage is CALL_MODE_FUNCTION, which needs to
    // be checked by `Get_Var`.  This is a necessarily evil while FUNCTION!
    // does not have the semantics of CLOSURE!, because pathological cases
    // in "stack-relative" addressing can get their hands on "reused" bound
    // words during the formation process, e.g.:
    //
    //      leaker: func [/exec e /gimme g] [
    //          either gimme [return [g]] [reduce e]
    //      ]
    //
    //      leaker/exec reduce leaker/gimme 10
    //
    // Since a leaked word from another instance of a function can give
    // access to a call frame during its formation, we need a way to tell
    // when a call frame is finished forming...CALL_MODE_FUNCTION is it.
    //
    enum Reb_Call_Mode mode;

    // `expr_index` [INTERNAL, READ-ONLY]
    //
    // Although the evaluator has to know what the current `index` is, the
    // error reporting machinery typically wants to know where the index
    // was *before* the last evaluation started...in order to present an
    // idea of the expression that caused the error.  This is the index
    // of where the currently evaluating expression started.
    //
    REBCNT expr_index;
};

// Each iteration of DO bumps a global count, that in deterministic repro
// cases can be very helpful in identifying the "tick" where certain problems
// are occurring.  The SPORADICALLY() macro uses this to allow flipping
// between different behaviors in debug builds--usually to run the release
// behavior some of the time, and the debug behavior some of the time.  This
// exercises the release code path even when doing a debug build.
//
#ifdef NDEBUG
    #define SPORADICALLY(modulus) FALSE
#else
    #define SPORADICALLY(modulus) (TG_Do_Count % modulus == 0)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  DO_NEXT_MAY_THROW and DO_ARRAY_THROWS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This is an optimized wrapper for the basic building block of Rebol
// evaluation.  They are macros designed to minimize overhead and be
// guaranteed to be inlined at the callsite.  For the central functionality,
// see `Do_Core()`.
//
// The optimized version of the macro will never do an evaluation of a
// type it doesn't have to.  It uses ANY_EVAL() to see if it can get
// out of making a function call...sometimes it cannot because there
// may be an infix lookup possible (we don't know that `[3] + [4]`
// isn't ever going to work...)
//
// The debug build exercises both code paths, by optimizing every other
// execution to bypass the evaluator if possible...and then throwing
// the code through Do_Core the other times.  It's a sampling test, but
// not a bad one for helping keep the methods in sync.
//
// DO_NEXT_MAY_THROW takes in an array and a REBCNT offset into that array
// of where to execute.  Although the return value is a REBCNT, it is *NOT*
// always a series index!!!  It may return:
//
// * END_FLAG if end of series prohibited a full evaluation
//
// * THROWN_FLAG if the output is THROWN()--you MUST check!
//
// * ...or the next index position where one might continue evaluation
//
// ===========================((( IMPORTANT )))==============================
//
//      The THROWN_FLAG means your value does not represent a directly
//      usable value, so you MUST check for it.  It signifies getting
//      back a THROWN()--see notes in sys-value.h about what that means.
//      If you don't know how to handle it, then at least do:
//
//              fail (Error_No_Catch_For_Throw(out));
//
//      If you *do* handle it, be aware it's a throw label with
//      OPT_VALUE_THROWN set in its header, and shouldn't leak to the
//      rest of the system.
//
// ===========================================================================
//
// Note that THROWN() is not an indicator of an error, rather something that
// ordinary language constructs might meaningfully want to process as they
// bubble up the stack.  Some examples would be BREAK, RETURN, and QUIT.
//
// Errors are handled with a different mechanism using longjmp().  So if an
// actual error happened during the DO then there wouldn't even *BE* a return
// value...because the function call would never return!  See PUSH_TRAP()
// and fail() for more information.
//
// DO_ARRAY_THROWS is another helper for the frequent case where one has a
// BLOCK! or a GROUP! at an index which already indicates the point where
// execution is to start.
//

#define END_FLAG 0x80000000  // end of block as index
#define THROWN_FLAG (END_FLAG - 1) // throw as an index

#define DO_NEXT_MAY_THROW_CORE(index_out,out_,array_,index_in,flags_) \
    do { \
        struct Reb_Call c_; \
        c_.value = ARRAY_AT((array_),(index_in)); \
        if (SPORADICALLY(2)) { /* optimize every OTHER execution if DEBUG */ \
            if (IS_END(c_.value)) { \
                SET_UNSET(out_); \
                (index_out) = END_FLAG; \
                break; \
            } \
            if ( \
                !ANY_EVAL(c_.value) \
                && (IS_END(c_.value + 1) || !ANY_EVAL(c_.value + 1)) \
            ) { \
                *(out_) = *ARRAY_AT((array_), (index_in)); \
                (index_out) = ((index_in) + 1); \
                break; \
            } \
        } \
        c_.out = (out_); \
        c_.array = (array_); \
        c_.index = (index_in) + 1; \
        c_.flags = DO_FLAG_DO | DO_FLAG_NEXT | (flags_); \
        Do_Core(&c_); \
        (index_out) = c_.index; \
    } while (FALSE)

#define DO_NEXT_MAY_THROW(index_out,out,array,index) \
    DO_NEXT_MAY_THROW_CORE( \
        (index_out), (out), (array), (index), DO_FLAG_LOOKAHEAD \
    )

// Note: It is safe for `out` and `array` to be the same variable.  The
// array and index are extracted, and will be protected from GC by the DO
// state...so it is legal to DO_ARRAY_THROWS(D_OUT, D_OUT) for instance.
//
#define DO_ARRAY_THROWS(out,array) \
    Do_At_Throws((out), VAL_ARRAY(array), VAL_INDEX(array))


//=////////////////////////////////////////////////////////////////////////=//
//
//  PATH EVALUATION "PVS"
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! This structure and the code using it has not been given a very
// thorough review under Ren-C yet.  It pertains to the dispatch of paths,
// where types can chain an evaluation from one to the next.
//

typedef struct Reb_Path_Value {
    REBVAL *value;  // modified
    REBVAL *select; // modified
    REBVAL *path;   // modified
    REBVAL *store;  // modified (holds constructed values)
    REBVAL *setval; // static
    const REBVAL *orig; // static
} REBPVS;

enum Path_Eval_Result {
    PE_OK,
    PE_SET,
    PE_USE,
    PE_NONE,
    PE_BAD_SELECT,
    PE_BAD_SET,
    PE_BAD_RANGE,
    PE_BAD_SET_TYPE
};

typedef REBINT (*REBPEF)(REBPVS *pvs); // Path evaluator function

typedef REBINT (*REBCTF)(REBVAL *a, REBVAL *b, REBINT s);


//=////////////////////////////////////////////////////////////////////////=//
//
//  ARGUMENT AND PARAMETER ACCESS HELPERS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These accessors are designed to make it convenient for natives and actions
// written in C to access their arguments and refinements.  They are able
// to bind to the implicit Reb_Call* passed to every REBNATIVE() and read
// the information out cleanly, like this:
//
//     PARAM(1, foo);
//     REFINE(2, bar);
//
//     if (IS_INTEGER(ARG(foo)) && REF(bar)) { ... }
//
// Under the hood `PARAM(1, foo)` and `REFINE(2, bar)` make const structs.
// In an optimized build, these structures disappear completely, with all
// addressing done directly into the call frame's cached `arg` pointer.
// It is also possible to get the typeset-with-symbol for a particular
// parameter or refinement, e.g. with `PAR(foo)` or `PAR(bar)`.
//
// The PARAM and REFINE macros use token pasting to name the variables they
// are declaring `p_name` instead of just `name`.  This prevents collisions
// with C++ identifiers, so PARAM(case) and REFINE(new) would make `p_case`
// and `p_new` instead of just `case` and `new` as the variable names.  (This
// is only visible in the debugger.)
//
// As a further aid, the debug build version of the structures contain the
// actual pointers to the arguments.  It also keeps a copy of a cache of the
// type for the arguments, because the numeric type encoding in the bits of
// the header requires a debug call (or by-hand-binary decoding) to interpret
// Whether a refinement was used or not at time of call is also cached.
//

struct Native_Param {
#if !defined(NDEBUG)
    enum Reb_Kind kind_cache;
    REBVAL *arg;
#endif
    int num;
};

struct Native_Refine {
#if !defined(NDEBUG)
    REBOOL used_cache;
    REBVAL *arg;
#endif
    int num;
};

#ifdef NDEBUG
    #define PARAM(n,name) \
        const struct Native_Param p_##name = {n}

    #define REFINE(n,name) \
        const struct Native_Refine p_##name = {n}
#else
    // Capture the argument for debug inspection.  Be sensitive to frameless
    // usage so that parameters may be declared and used with PAR() even if
    // they cannot be used with ARG()
    //
    #define PARAM(n,name) \
        const struct Native_Param p_##name = { \
            call_->arg ? VAL_TYPE(call_->arg + (n)) : REB_TRASH, \
            call_->arg ? call_->arg + (n) : NULL, \
            (n) \
        }

    // As above, do a cache and be tolerant of framelessness.  The seeming odd
    // choice to lie and say a refinement is present in the frameless case is
    // actually to make any frameless native that tries to use REF() get
    // confused and hopefully crash...saying FALSE might make the debug build
    // get cozy with the idea that REF() is legal in a frameless native.
    //
    #define REFINE(n,name) \
        const struct Native_Refine p_##name = { \
            call_->arg ? NOT(IS_NONE(call_->arg + (n))) : TRUE, \
            call_->arg ? call_->arg + (n) : NULL, \
            (n) \
        }
#endif

// Though REF can only be used with a REFINE() declaration, ARG can be used
// with either.
//
#define ARG(name) \
    (call_->arg + (p_##name).num)

#define PAR(name) \
    FUNC_PARAM(call_->func, (p_##name).num) // a TYPESET!

#ifdef NDEBUG
    #define REF(name) \
        NOT(IS_NONE(ARG(name)))
#else
    // An added useless ?: helps check in debug build to make sure we do not
    // try to use REF() on something defined as PARAM(), but only REFINE()
    //
    #define REF(name) \
        ((p_##name).used_cache \
            ? NOT(IS_NONE(ARG(name))) \
            : NOT(IS_NONE(ARG(name))))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  CALL FRAME ACCESS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! To be documented and reviewed.  Legacy naming conventions when the
// arguments to functions lived in the data stack gave the name "DSF" for
// "(D)ata (S)tack (F)rame" which is no longer accurate, as well as the
// convention of prefix with a D_.  The new PARAM()/REFINE()/ARG()/REF()
// scheme is coming up on replacing a lot of it, so these will be needing
// a tune up and choices of better names once that is sorted out.  It
// may be as simple as changing these to C_ or CSP for "call stack pointer"
//

#define DSF (CS_Running + 0) // avoid assignment to DSF via + 0

#define DSF_OUT(c)          cast(REBVAL * const, (c)->out) // writable Lvalue
#define PRIOR_DSF(c)        ((c)->prior)
#define DSF_ARRAY(c)        cast(REBARR * const, (c)->array) // Lvalue
#define DSF_EXPR_INDEX(c)   ((c)->expr_index + 0) // Lvalue
#define DSF_LABEL_SYM(c)    ((c)->label_sym + 0) // Lvalue
#define DSF_FUNC(c)         ((c)->func)
#define DSF_DSP_ORIG(c)     ((c)->dsp_orig + 0) // Lvalue

#define DSF_PARAMS_HEAD(c)  FUNC_PARAMS_HEAD((c)->func)

#define DSF_ARGS_HEAD(c) \
    (IS_CLOSURE(FUNC_VALUE((c)->func)) \
        ? ARRAY_AT((c)->arglist.array, 1) \
        : &(c)->arglist.chunk[1])

// ARGS is the parameters and refinements
// 1-based indexing into the arglist (0 slot is for object/function value)
#ifdef NDEBUG
    #define DSF_ARG(c,n)    ((c)->arg + (n))
#else
    #define DSF_ARG(c,n)    DSF_ARG_Debug((c), (n)) // checks arg index bound
#endif

// Note about D_ARGC: A native should generally not detect the arity it
// was invoked with, (and it doesn't make sense as most implementations get
// the full list of arguments and refinements).  However, ACTION! dispatch
// has several different argument counts piping through a switch, and often
// "cheats" by using the arity instead of being conditional on which action
// ID ran.  Consider when reviewing the future of ACTION!.
//
#define DSF_ARGC(c) \
    cast(REBCNT, FUNC_NUM_PARAMS((c)->func))

#define DSF_CELL(c) (&(c)->cell)

// Quick access functions from natives (or compatible functions that name a
// Reb_Call pointer `call_`) to get some of the common public fields.
//
#define D_OUT       DSF_OUT(call_)          // GC-safe slot for output value
#define D_ARGC      DSF_ARGC(call_)         // count of args+refinements/args
#define D_ARG(n)    DSF_ARG(call_, (n))     // pass 1 for first arg
#define D_REF(n)    LOGICAL(!IS_NONE(D_ARG(n)))    // D_REFinement (not D_REFerence)
#define D_FUNC      DSF_FUNC(call_)         // REBVAL* of running function
#define D_LABEL_SYM DSF_LABEL_SYM(call_)    // symbol or placeholder for call
#define D_CELL      DSF_CELL(call_)         // GC-safe extra value
#define D_DSP_ORIG  DSF_DSP_ORIG(call_)     // Original data stack pointer

#define D_FRAMELESS (!call_->arg)           // Native running w/no call frame

// !!! These should perhaps assert that they're only being used when a
// frameless native is in action.
//
#define D_ARRAY         (call_->array)
#define D_INDEX         (call_->index)
#define D_VALUE         (call_->value)
