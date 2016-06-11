//
//  File: %sys-frame.h
//  Summary: {Evaluator "Do State"}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
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
// The primary routine that performs DO and DO/NEXT is called Do_Core().  It
// takes a single parameter which holds the running state of the evaluator.
// This state may be allocated on the C variable stack:  Do_Core() is
// written such that a longjmp up to a failure handler above it can run
// safely and clean up even though intermediate stacks have vanished.
//
// Ren-C can not only run the evaluator across a REBSER-style series of
// input based on index, it can also fetch those values from a standard C
// array of REBVAL[].  Alternately, it can enumerate through C's `va_list`,
// providing the ability to pass pointers as REBVAL* to comma-separated input
// at the source level.
//
// To provide even greater flexibility, it allows the very first element's
// pointer in an evaluation to come from an arbitrary source.  It doesn't
// have to be resident in the same sequence from which ensuing values are
// pulled, allowing a free head value (such as a FUNCTION! REBVAL in a local
// C variable) to be evaluated in combination from another source (like a
// va_list or series representing the arguments.)  This avoids the cost and
// complexity of allocating a series to combine the values together.
//
// These features alone would not cover the case when REBVAL pointers that
// are originating with C source were intended to be supplied to a function
// with no evaluation.  In R3-Alpha, the only way in an evaluative context
// to suppress such evaluations would be by adding elements (such as QUOTE).
// Besides the cost and labor of inserting these, the risk is that the
// intended functions to be called without evaluation, if they quoted
// arguments would then receive the QUOTE instead of the arguments.
//
// The problem was solved by adding a feature to the evaluator which was
// also opened up as a new privileged native called EVAL.  EVAL's refinements
// completely encompass evaluation possibilities in R3-Alpha, but it was also
// necessary to consider cases where a value was intended to be provided
// *without* evaluation.  This introduced EVAL/ONLY.
//


//
// DO_FLAGS
//
// Used by low level routines, these flags specify behaviors which are
// exposed at a higher level through EVAL, EVAL/ONLY, and EVAL/NOFIX
//
// The flags are specified either way for clarity.
//
enum {
    DO_FLAG_0 = 0, // unused

    // As exposed by the DO native and its /NEXT refinement, a call to the
    // evaluator can either run to the finish from a position in an array
    // or just do one eval.  Rather than achieve execution to the end by
    // iterative function calls to the /NEXT variant (as in R3-Alpha), Ren-C
    // offers a controlling flag to do it from within the core evaluator
    // as a loop.
    //
    // However: since running to the end follows a different code path than
    // performing DO/NEXT several times, it is important to ensure they
    // achieve equivalent results.  There are nuances to preserve this
    // invariant and especially in light of potential interaction with
    // DO_FLAG_LOOKAHEAD.
    //
    // NOTE: DO_FLAG_NEXT is *non-continuable* with va_list.  This is due to
    // contention with DO_FLAG_LOOKAHEAD which would not be able to "un-fetch"
    // in the case of a lookahead for infix that failed (and NO_LOOKAHEAD is
    // very rare with API clients).  But also, the va_list could need a
    // conversion to an array during evaluation...and any continuation would
    // need to be sensitive to this change, which is extra trouble for very
    // little likely benefit.
    //
    DO_FLAG_NEXT = 1 << 1,
    DO_FLAG_TO_END = 1 << 2,

    // When we're in mid-dispatch of an infix function, the precedence is such
    // that we don't want to do further infix lookahead while getting the
    // arguments.  (e.g. with `1 + 2 * 3` we don't want infix `+` to
    // "look ahead" past the 2 to see the infix `*`)
    //
    // Actions taken during lookahead may have no side effects.  If it's used
    // to evaluate a form of source input that cannot be backtracked (e.g.
    // a C variable argument list) then it will not be possible to resume.
    //
    DO_FLAG_LOOKAHEAD = 1 << 3,
    DO_FLAG_NO_LOOKAHEAD = 1 << 4,

    // Write more comments here when feeling in more of a commenting mood
    //
    DO_FLAG_ARGS_EVALUATE = 1 << 5,
    DO_FLAG_NO_ARGS_EVALUATE = 1 << 6,

    // A pre-built frame can be executed "in-place" without a new allocation.
    // It will be type checked, and also any BAR! parameters will indicate
    // a desire to acquire that argument (permitting partial specialization).
    //
    DO_FLAG_EXECUTE_FRAME = 1 << 7,

    // Usually VA_LIST_FLAG is enough to tell when there is a source array to
    // examine or not.  However, when the end is reached it is written over
    // with END_FLAG and it's no longer possible to tell if there's an array
    // available to inspect or not.  The few cases that "need to know" are
    // things like error delivery, which want to process the array after
    // expression evaluation is complete.  Review to see if they actually
    // would rather know something else, but this is a cheap flag for now.
    //
    DO_FLAG_VA_LIST = 1 << 8,

    // While R3-Alpha permitted modifications of an array while it was being
    // executed, Ren-C does not.  It takes a lock if the source is not already
    // read only, and sets it back when Do_Core is finished (or on errors)
    //
    DO_FLAG_TOOK_FRAME_LOCK = 1 << 9,

    // DO_FLAG_APPLYING is used to indicate that the Do_Core code is entering
    // a situation where the frame was already set up.
    //
    DO_FLAG_APPLYING = 1 << 10
};


//=////////////////////////////////////////////////////////////////////////=//
//
//  DO INDEX OR FLAG (a.k.a. "INDEXOR")
//
//=////////////////////////////////////////////////////////////////////////=//
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
//      VALUE_FLAG_THROWN set in its header, and shouldn't leak to the
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


//=////////////////////////////////////////////////////////////////////////=//
//
//  EVALUATION TYPES ("ET_XXX")
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The REB_XXX types are not sequential, but skip by 4 in order to have the
// low 2 bits clear on all values in the enumeration.  This means faster
// extraction and comparison without needing to bit-shift, but it also
// means that a `switch` statement can't be optimized into a jump table--
// which generally requires contiguous values:
//
// http://stackoverflow.com/questions/17061967/c-switch-and-jump-tables
//
// By having a table that can quickly convert an `enum Reb_Kind` into a
// small integer suitable for a switch statement in the evaluator, the
// optimization can be leveraged.  The special value of "0" is picked for
// no evaluation behavior, so the table can have a second use as the quick
// implementation behind the ANY_EVAL macro.  All non-zero values then can
// mean "has some behavior in the evaluator".
//
enum Reb_Eval_Type {
    ET_INERT = 0, // does double duty as logic FALSE in "ANY_EVAL()"
    ET_BAR,
    ET_LIT_BAR,
    ET_WORD,
    ET_SET_WORD,
    ET_GET_WORD,
    ET_LIT_WORD,
    ET_GROUP,
    ET_PATH,
    ET_SET_PATH,
    ET_GET_PATH,
    ET_LIT_PATH,
    ET_FUNCTION,

    // !!! Review more efficient way of expressing safe enumerators

    ET_SAFE_ENUMERATOR,

#if !defined(NDEBUG)
    ET_TRASH,
#endif

    ET_MAX
};

#ifdef NDEBUG
    typedef REBUPT REBET; // native-sized integer is faster in release builds
#else
    typedef enum Reb_Eval_Type REBET; // typed enum is better info in debugger
#endif

// If the type has evaluator behavior (vs. just passing through).  So like
// WORD!, GROUP!, FUNCTION! (as opposed to BLOCK!, INTEGER!, OBJECT!).
// The types are not arranged in an order that makes a super fast test easy
// (though perhaps someday it could be tweaked so that all the evaluated types
// had a certain bit set?) hence use a small fixed table.
//
// Note that this table has 256 entries, of which only those corresponding
// to having the two lowest bits zero are set.  This is to avoid needing
// shifting to check if a value is evaluable.  The other storage could be
// used for properties of the type +1, +2, +3 ... at the cost of a bit of
// math but reusing the values.  Any integer property could be stored for
// the evaluables so long as non-evaluables are 0 in this list.
//
extern const REBET Eval_Table[REB_MAX];

#define ANY_EVAL(v) LOGICAL(Eval_Table[VAL_TYPE(v)])


union Reb_Frame_Source {
    REBARR *array;
    va_list *vaptr;
};

// NOTE: The ordering of the fields in `Reb_Frame` are specifically done so
// as to accomplish correct 64-bit alignment of pointers on 64-bit systems
// (as long as REBCNT and REBINT are 32-bit on such platforms).  If modifying
// the structure, be sensitive to this issue.
//
// Because performance in the core evaluator loop is system-critical, this
// uses full platform `int`s instead of REBCNTs.
//
struct Reb_Frame {
    //
    // `eval` [INTERNAL, NON-READABLE, not GC-PROTECTED?]
    //
    // Placed at the head of the structure for alignment reasons, but the most
    // difficult field of Reb_Frame to explain.  It serves the purpose of a
    // holding cell that is needed while an EVAL is running, because the
    // calculated value that had lived in `c->out` which is being evaluated
    // can't stay in that spot while the next evaluation is writing into it.
    // Frameless natives and other code with call frame access should not
    // tamper w/it or read it--from their point of view it is "random".
    //
    // Once a function evaluation has started and the fields of the FUNC
    // extracted, however, then specifically the eval slot is free up until
    // the function evaluation is over.  As a result, it is used by VARARGS!
    // to hold a piece of state that is visible to all bit-pattern-instances
    // of that same VARARGS! in other locations.  See notes in %sys-value.h
    //
    union {
        RELVAL eval;
        REBARR *subfeed; // during VARARGS! (see also REBSER.misc.subfeed)
    } cell;

    // `func` [INTERNAL, READ-ONLY, GC-PROTECTED]
    //
    // If a function call is currently in effect, `func` holds a pointer to
    // the function being run.  Because functions are identified and passed
    // by a platform pointer as their paramlist REBSER*, you must use
    // `FUNC_VALUE(c->func)` to get a pointer to a canon REBVAL representing
    // that function (to examine its function flags, for instance).
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
    REBUPT dsp_orig; // type is REBDSP, but enforce alignment here

    // `flags` [INPUT, READ-ONLY]
    //
    // These are DO_FLAG_xxx or'd together.  If the call is being set up
    // for an Apply as opposed to Do, this must be 0.
    //
    REBUPT flags; // type is REBFLGS, but enforce alignment here

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
    // first value pointer...which for any successive evaluations will be
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
    // !!! The ramifications of using a disconnected value on debugging
    // which is *not* part of the series is that the "where" will come up
    // with missing information.  This is not the only case where the
    // optimization of not making a series or not making a frame creates
    // a problem--and in general, optimization will interfere with almost any
    // debug feedback.  The proposed solution is to have a "debug mode" which
    // causes more conservatism--for instance, if it is noticed in an
    // evaluation that the value pointer does *not* line up at the head of
    // the series for the evaluation given, it would be cached somewhere...
    // then if any problem needing a where came up, a series would be made
    // to put it in.  (The where series is paying for a copy anyway.)
    //
    const RELVAL *value;

    // `gotten`
    //
    // Work in Get_Var that might need to be reused makes use of this pointer.
    //
    const REBVAL *gotten;

    // This flag is used to tell the function code whether it needs to
    // "lookback" (into f->out) to find its next argument instead of going
    // through normal evaluation.
    //
    // A lookback binding that takes two arguments is "infix".
    // A lookback binding that takes one argument is "postfix".
    // A lookback binding that takes > 2 arguments can be cool (`->` lambdas)
    // A lookback binding that takes zero arguments blocks subsequent lookback
    //
    REBOOL lookback;

    // `pending` [INTERNAL, READ-ONLY, GC-PROTECTS pointed-to REBVAL]
    //
    // Mechanically speaking, running an EVAL has to overwrite `value` from
    // the natural pre-fetching course, so that the evaluated value can be
    // simulated as living in the line of execution.  Because fetching moves
    // forward only, we'd lose the next value if we didn't save it somewhere.
    //
    // This pointer saves the prefetched value that eval overwrites, and
    // by virtue of not being NULL signals to just use the value on the
    // next fetch instead of fetching again.
    //
    const RELVAL *pending;

    // source.array, source.vaptr [INPUT, READ-ONLY, GC-PROTECTED]
    //
    // This is the source from which new values will be fetched.  The most
    // common dispatch of the evaluator is on values that live inside of a
    // Rebol BLOCK! or GROUP!...but something to know is that the `array`
    // could have come from any ANY-ARRAY! (e.g. a PATH!).  The fact that it
    // came from a value marked REB_PATH is not known here: value-bearing
    // series will all "evaluate like a block" when passed to Do_Core.
    //
    // In addition to working with a source of values in a traditional series,
    // in Ren-C it is also possible to feed the evaluator arbitrary REBVAL*s
    // through a variable argument list.  Though this means no array needs to
    // be dynamically allocated, some conditions require the va_list to be
    // converted to an array.  See notes on Reify_Va_To_Array_In_Frame().
    //
    union Reb_Frame_Source source;

    // `index` [INPUT, OUTPUT]
    //
    // This holds the index of the *next* item in the array to fetch as
    // f->value for processing.  It's invalid if the frame is for a C va_list.
    //
    REBCNT index;

    // `specifier` [INPUT, READ-ONLY, GC-PROTECTED]
    //
    // The specifier context is where any relatively bound words are looked
    // up to become "specific".  (If the function inside the specifier does
    // not match the function of any relatively bound words that happen to
    // be fetched into Reb_Frame.value, then that is a problem that will
    // assert in the debug build.)
    //
    // Typically the specifier used is extracted from the payload of the
    // Reb_Any_Series that provided the source.array for the call to DO.
    // It may also be NULL if it is known that there are no relatively bound
    // words that will be encountered from the source.
    //
    REBCTX *specifier;

    // `label_sym` [INTERNAL, READ-ONLY]
    //
    // Functions don't have "names", though they can be assigned to words.
    // Typically the label symbol is passed into the evaluator as SYM_0 and
    // then only changed if a function dispatches by WORD!, however it
    // is possible for Do_Core to be called with a preloaded symbol for
    // better debugging descriptivity.
    //
    REBSYM label_sym;

    // `stackvars` [INTERNAL, VALUES MUTABLE and GC-SAFE]
    //
    // For functions without "indefinite extent", the invocation arguments are
    // stored in the "chunk stack", where allocations are fast, address stable,
    // and implicitly terminated.  If a function has indefinite extent, this
    // will be set to NULL.
    //
    // This can contain END markers at any position during arg fulfillment.
    //
    REBVAL *stackvars;

    // `varlist`
    //
    // For functions with "indefinite extent", the varlist is the CTX_VARLIST
    // of a FRAME! context in which the function's arguments live.  It is
    // also possible for this varlist to come into existence even for functions
    // like natives, if the frame's context is "reified" (e.g. by the debugger)
    // If neither of these conditions are true, it will be NULL
    //
    // This can contain END markers at any position during arg fulfillment,
    // and this means it cannot have a MANAGE_ARRAY call until that is over.
    //
    REBARR *varlist;

    // `param` [INTERNAL, REUSABLE, GC-PROTECTS pointed-to REBVALs]
    //
    // We use the convention that "param" refers to the TYPESET! (plus symbol)
    // from the spec of the function--a.k.a. the "formal argument".  This
    // pointer is moved in step with `arg` during argument fulfillment.
    //
    // (Note: It is const because we don't want to be changing the params,
    // but also because it is used as a temporary to store value if it is
    // advanced but we'd like to hold the old one...this makes it important
    // to protect it from GC if we have advanced beyond as well!)
    //
    // Made relative just to have another RELVAL on hand.
    //
    const RELVAL *param;

    // `arg` [INTERNAL, also CACHE of `ARR_HEAD(arglist)`]
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
    // During parameter fulfillment, this might point to the `arg` slot
    // of a refinement which is having its arguments processed.  Or it may
    // point to another *read-only* value whose content signals information
    // about how arguments should be handled.  The states are chosen to line
    // up naturally with tests in the evaluator, so there's a reasoning:.
    //
    // * If IS_VOID(), then refinements are being skipped and the arguments
    //   that follow should not be written to.
    //
    // * If BLANK!, this is an arg to a refinement that was not used in the
    //   invocation.  No consumption should be performed, arguments should
    //   be written as unset, and any non-unset specializations of arguments
    //   should trigger an error.
    //
    // * If FALSE, this is an arg to a refinement that was used in the
    //   invocation but has been *revoked*.  It still consumes expressions
    //   from the callsite for each remaining argument, but those expressions
    //   must not evaluate to any value.
    //
    // * If TRUE the refinement is active but revokable.  So if evaluation
    //   produces no value, `refine` must be mutated to be FALSE.
    //
    // * If BAR!, it's an ordinary arg...and not a refinement.  It will be
    //   evaluated normally but is not involved with revocation.
    //
    // Because of how this lays out, IS_CONDITIONAL_TRUE() can be used to
    // determine if an argument should be type checked normally...while
    // IS_CONDITIONAL_FALSE() means that the arg's bits must be set to void.
    //
    REBVAL *refine;

    // `prior` [INTERNAL, READ-ONLY]
    //
    // The prior call frame (may be NULL if this is the topmost stack call).
    //
    struct Reb_Frame *prior;

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
    REBET eval_type; // enum in debug build, faster REBUPT in release

    // `expr_index` [INTERNAL, READ-ONLY]
    //
    // Although the evaluator has to know what the current `index` is, the
    // error reporting machinery typically wants to know where the index
    // was *before* the last evaluation started...in order to present an
    // idea of the expression that caused the error.  This is the index
    // of where the currently evaluating expression started.
    //
    REBIXO expr_index;

    // Definitional Return gives back a "corrupted" REBVAL of a return native,
    // whose body is actually an indicator of the return target.  The
    // Reb_Frame only stores the FUNC so we must extract this body from the
    // value if it represents a exit_from
    //
    REBARR *exit_from;

#if !defined(NDEBUG)
    //
    // `label_str` [INTERNAL, DEBUG, READ-ONLY]
    //
    // Knowing the label symbol is not as handy as knowing the actual string
    // of the function this call represents (if any).  It is in UTF8 format,
    // and cast to `char*` to help debuggers that have trouble with REBYTE.
    //
    const char *label_str;

    // `do_count` [INTERNAL, DEBUG, READ-ONLY]
    //
    // The `do_count` represents the expression evaluation "tick" where the
    // Reb_Frame is starting its processing.  This is helpful for setting
    // breakpoints on certain ticks in reproducible situations.
    //
    REBUPT do_count; // !!! Move to dynamic data, available in a debug mode?

    // Debug reuses PUSH_TRAP's snapshotting to check for leaks on each step
    //
    struct Reb_State state;
#endif
};


// It's helpful when looking in the debugger to be able to look at a frame
// and see a cached string for the function it's running (if there is one).
// The release build only considers the frame symbol valid if ET_FUNCTION
//
#ifdef NDEBUG
    #define SET_FRAME_SYM(f,s) \
        ((f)->label_sym = (s))

    #define CLEAR_FRAME_SYM(f) \
        NOOP
#else
    #define SET_FRAME_SYM(f,s) \
        (assert((f)->eval_type == ET_FUNCTION), \
            (f)->label_sym = (s), \
            (f)->label_str = cast(const char*, Get_Sym_Name((f)->label_sym)))

    #define CLEAR_FRAME_SYM(f) \
        ((f)->label_sym = SYM_0, (f)->label_str = NULL)
#endif
