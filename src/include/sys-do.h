//
//  File: %sys-do.h
//  Summary: {Evaluator "Do State" and Helpers}
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

    // Not all function invocations require there to be a persistent frame
    // that identifies it.  One will be needed if there are going to be
    // words bound into the frame (in a way that cannot be finessed through
    // relative binding)
    //
    // Note: This flag is not paired, but if it were the alternative would
    // be DO_FLAG_FRAME_CHUNK...which is the default assumption.
    //
    DO_FLAG_HAS_VARLIST = 1 << 7,

    // A pre-built frame can be executed "in-place" without a new allocation.
    // It will be type checked, and also any BAR! parameters will indicate
    // a desire to acquire that argument (permitting partial specialization).
    //
    DO_FLAG_EXECUTE_FRAME = 1 << 8,

    // Usually VALIST_FLAG is enough to tell when there is a source array to
    // examine or not.  However, when the end is reached it is written over
    // with END_FLAG and it's no longer possible to tell if there's an array
    // available to inspect or not.  The few cases that "need to know" are
    // things like error delivery, which want to process the array after
    // expression evaluation is complete.  Review to see if they actually
    // would rather know something else, but this is a cheap flag for now.
    //
    DO_FLAG_VALIST = 1 << 9,

    // Punctuators are a special behavior which is triggered by an arity-0
    // lookahead function.  The idea of a function with no arguments that is
    // "infix-like" did not have another meaning, so since there was a
    // case being paid for in the code recognizing this situation it was
    // given some usefulness...namely to prohibit passing as an argument.
    //
    // !!! This may make BAR! seem obsolete, as it could be implemented as
    // a function.  But BAR! is special as it cannot be quoted, and it has
    // several other purposes...plus it is more efficient to evaluate.
    //
    DO_FLAG_PUNCTUATOR = 1 << 10,

    // While R3-Alpha permitted modifications of an array while it was being
    // executed, Ren-C does not.  It takes a lock if the source is not already
    // read only, and sets it back when Do_Core is finished (or on errors)
    //
    DO_FLAG_TOOK_FRAME_LOCK = 1 << 11,

    // DO_FLAG_CANT_BE_INFIX_LEFT_ARG is ignored if passed into a frame's
    // flags.  It only has effect when applied to the temporary flags
    // applicable to one evaluation.  It is set on the "lookahead flags"
    // when a lookback function of arity 0 is seen.  The meaning given to
    // these functions is that they refuse to serve as the left argument
    // to another lookback function.
    //
    DO_FLAG_CANT_BE_INFIX_LEFT_ARG = 1 << 12,

    // DO_FLAG_APPLYING is used to indicate that the Do_Core code is entering
    // a situation where the frame was already set up and a void means that
    // the argument is "opted out of"...not specialized out.
    //
    DO_FLAG_APPLYING = 1 << 13
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

#define END_FLAG 0x80000000  // end of block as index
#define THROWN_FLAG (END_FLAG - 0x75) // throw as an index

// The VALIST_FLAG is the index used when a C va_list pointer is the input.
// Because access to a `va_list` is strictly increasing through va_arg(),
// there is no way to track an index; fetches are indexed automatically
// and sequentially without possibility for mutation of the list.  Should
// this index be used it will always be the index of a DO_NEXT until either
// an END_FLAG or a THROWN_FLAG is reached.
//
#define VALIST_FLAG (END_FLAG - 0xBD)

// This is not an actual DO state flag that you would see in a Reb_Frame's
// index, but it is a value that is returned in case a non-continuable
// DO_NEXT call is made on va_lists.  One can make the observation that it
// is incomplete only--not resume.
//
#define VALIST_INCOMPLETE_FLAG (END_FLAG - 0xAE)

// The C build simply defines a REBIXO as a synonym for a pointer-sized int.
// In the C++ build, the indexor is a more restrictive class...which redefines
// a subset of operations for integers but does *not* implicitly cast to one
// Hence if a THROWN_FLAG, END_FLAG, VALIST_FLAG etc. is used with integer
// math or put into an `int` variable accidentally, this will be caught.
//
// Because indexors are not stored in REBVALs or places where memory usage
// outweighs the concern of the native performance, they use `REBUPT`
// instead of REBCNT.  The C++ build maintains that size for its class too.
//
// !!! The feature is now selectively enabled, temporarily in order to make
// the binding in Ren-Cpp binary compatible regardless of whether the build
// was done with C or C++
//
#if defined(NDEBUG) || !defined(__cplusplus) || (__cplusplus < 201103L)
    typedef REBUPT REBIXO;
#else
    #include "sys-do-cpp.h"

    #if 0
        typedef Reb_Indexor REBIXO;
    #else
        typedef REBUPT REBIXO;
    #endif
#endif


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


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBOL DO STATE (a.k.a. Reb_Frame)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A Reb_Frame structure represents the fixed-size portion for a function's
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
// See Fail_Core for the handling of freeing of frame state on errors.
//

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
        struct Reb_Value eval; // Reb_Specific_Value doesn't default construct
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
    const REBVAL *value;

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

    // `eval_fetched` [INTERNAL, READ-ONLY, GC-PROTECTS pointed-to REBVAL]
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
    const REBVAL *eval_fetched;

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

    // `indexor` [INPUT, OUTPUT]
    //
    // This can hold an "index OR a flag" related to the current state of
    // the enumeration of the values being evaluated.  For the flags, see
    // notes on REBIXO and END_FLAG, THROWN_FLAG.  Note also the case of
    // a C va_list where the actual index of the REBVAL* is intrinsic to
    // the enumeration...so the indexor will be VA_LIST flag vs. a count.
    //
    // Successive fetching is always done by index and not with `++c-value`.
    // This is for several reasons, but one of them is to avoid crashing if
    // the input array is modified during the evaluation.
    //
    // !!! While it doesn't *crash*, a good user-facing explanation of
    // handling what it does instead seems not to have been articulated!  :-/
    //
    REBIXO indexor;

    // `label_sym` [INTERNAL, READ-ONLY]
    //
    // Functions don't have "names", though they can be assigned to words.
    // Typically the label symbol is passed into the evaluator as SYM_0 and
    // then only changed if a function dispatches by WORD!, however it
    // is possible for Do_Core to be called with a preloaded symbol for
    // better debugging descriptivity.
    //
    REBSYM label_sym;

    // `data` [INTERNAL, VALUES MUTABLE and GC-SAFE]
    //
    // The dynamic portion of the call frame has args with which a function is
    // being invoked.  The data is resident in the "chunk stack".
    //
    // If a client of this array is a NATIVE!, then it will access the data
    // directly by offset index (e.g. `PARAM(3,...)`).  But if it is a
    // FUNCTION! implemented by the user, it has words for arguments and
    // locals to access by, and hence a FRAME!.  The frame is like an OBJECT!
    // but since its data also lives in the chunk stack, words bound into it
    // won't be able to fetch the data after the call has completed.
    //
    // !!! For debugging purposes, it will be necessary to request natives
    // to not "run framelessly" (though that means not-even-a-chunk frame).
    // This might also request not to run as "just a chunk" so that the
    // content of the native frame would be inspectable by words bound into
    // the frame by the debugger.
    //
    union {
        REBARR *varlist;
        REBVAL *stackvars;
    } data;

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
    const REBVAL *param;

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
    // One particularly important usage is ET_FUNCTION, which needs to
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
    // when a call frame is finished forming: Is_Function_Frame_Fulfilling()
    //
    REBET eval_type; // speedier to use REBUPT, but not as nice in debugger

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

#define IS_QUOTABLY_SOFT(v) \
    (IS_GROUP(v) || IS_GET_WORD(v) || IS_GET_PATH(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  DO's LOWEST-LEVEL EVALUATOR HOOKING
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This API is used internally in the implementation of Do_Core.  It does
// not speak in terms of arrays or indices, it works entirely by setting
// up a call frame (f), and threading that frame's state through successive
// operations, vs. setting it up and disposing it on each DO/NEXT step.
//
// Like higher level APIs that move through the input series, this low-level
// API can move at full DO/NEXT intervals.  Unlike the higher APIs, the
// possibility exists to move by single elements at a time--regardless of
// if the default evaluation rules would consume larger expressions.  Also
// making it different is the ability to resume after a DO/NEXT on value
// sources that aren't random access (such as C's va_arg list).
//
// One invariant of access is that the input may only advance.  Before any
// operations are called, any low-level client must have already seeded
// c->value with a valid "fetched" REBVAL*.  END is not valid, so callers
// beginning a Do_To_End must pre-check that condition themselves before
// calling Do_Core.  And if an operation sets the c->index to END_FLAG then
// that must be checked--it's not legal to call more operations on a call
// frame after a fetch reports the end.
//
// Operations are:
//
//  FETCH_NEXT_ONLY_MAYBE_END
//
//      Retrieve next pointer for examination to c->value.  The previous
//      c->value pointer is overwritten.  (No REBVAL bits are moved by
//      this operation, only the 'currently processing' pointer reassigned.)
//      c->index may be set to END_FLAG if the end of input is reached.
//
// DO_NEXT_REFETCH_MAY_THROW
//
//      Executes the already-fetched pointer, consuming as much of the input
//      as necessary to complete a /NEXT (or failing with an error).  This
//      writes the computed REBVAL into a destination location.  After the
//      operation, the next c->value pointer will already be fetched and
//      waiting for examination or use.  c->index may be set to either
//      THROWN_FLAG or END_FLAG by this operation.
//
// QUOTE_NEXT_REFETCH
//
//      This operation is fairly trivial in the sense that it just assigns
//      the REBVAL bits pointed to by the current value to the destination
//      cell.  Then it does a simple fetch.  The main reason for making an
//      operation vs just having callers do the two steps is to monitor
//      when some of the input has been "consumed" vs. merely fetched.
//
//      !!! This is unenforceable in the C build, but in the C++ build it
//      could be ensured c->value was only dereferenced via * and stored
//      to targets through this routine.
//
// This is not intending to be a "published" API of Rebol/Ren-C.  But the
// privileged level of access can be used by natives that feel they can
// optimize performance by working with the evaluator directly.
//
// !!! For better or worse, Do_Core does not lock the series it is iterating.
// This means any arbitrary user code (or system code) could theoretically
// disrupt a series out from under it, and then crash the system on the
// next fetch.  Hence an array and an index are used, and in terms of "crash
// avoidance" (albeit not necessarily "semantic sensibility) it's necessary
// to clip the index to be within the range of the series.  In theory it
// would be possible to track the cases where clipping the index was needed
// or not, though it might be better to just lock the series being evaluated
// currently...this is an open question.
//

#define PUSH_CALL(f) \
    do { \
        (f)->prior = TG_Frame_Stack; \
        TG_Frame_Stack = (f); \
        if (NOT(f->flags & DO_FLAG_VALIST)) \
            if (!GET_ARR_FLAG((f)->source.array, SERIES_FLAG_LOCKED)) { \
                SET_ARR_FLAG(f->source.array, SERIES_FLAG_LOCKED); \
                f->flags |= DO_FLAG_TOOK_FRAME_LOCK; \
            } \
    } while (0)

#define PUSH_ARTIFICIAL_CALL_UNLESS_END(c,v) \
    do { \
        (f)->value = VAL_ARRAY_AT(v); \
        if (IS_END((f)->value)) { \
            (f)->indexor = END_FLAG; \
            break; \
        } \
        (f)->eval_type = ET_INERT; \
        (f)->flags = 0; /* !!! review */ \
        (f)->indexor = VAL_INDEX(v) + 1; \
        (f)->source.array = VAL_ARRAY(v); \
        (f)->eval_fetched = NULL; \
        (f)->label_sym = SYM_0; \
        PUSH_CALL(f); \
    } while (0)

#define UPDATE_EXPRESSION_START(f) \
    (assert((f)->indexor != VALIST_FLAG), (f)->expr_index = (f)->indexor)

#define DROP_CALL(f) \
    do { \
        if ((f)->flags & DO_FLAG_TOOK_FRAME_LOCK) { \
            assert(GET_ARR_FLAG((f)->source.array, SERIES_FLAG_LOCKED)); \
            CLEAR_ARR_FLAG((f)->source.array, SERIES_FLAG_LOCKED); \
        } \
        assert(TG_Frame_Stack == (f)); \
        TG_Frame_Stack = (f)->prior; \
    } while (0)

#if 0
    // For detailed debugging of the fetching; coarse tool used only in very
    // deep debugging of the evaluator.
    //
    #define TRACE_FETCH_DEBUG(m,f,a) \
        Trace_Fetch_Debug((m), (f), (a))
#else
    #define TRACE_FETCH_DEBUG(m,f,a) NOOP
#endif

//
// FETCH_NEXT_ONLY_MAYBE_END (see notes above)
//

#define FETCH_NEXT_ONLY_MAYBE_END_RAW(f) \
    do { \
        if ((f)->eval_fetched) { \
            if (IS_END((f)->eval_fetched)) \
                (f)->value = END_CELL; \
            else \
                (f)->value = (f)->eval_fetched; \
            (f)->eval_fetched = NULL; \
            break; \
        } \
        if ((f)->indexor != VALIST_FLAG) { \
            (f)->value = ARR_AT((f)->source.array, (f)->indexor); \
            ++(f)->indexor; \
            if (IS_END((f)->value)) \
                (f)->indexor = END_FLAG; \
        } \
        else { \
            (f)->value = va_arg(*(f)->source.vaptr, const REBVAL*); \
            if (IS_END((f)->value)) \
                (f)->indexor = END_FLAG; \
            else \
                assert(!IS_VOID((f)->value)); \
        } \
    } while (0)

#ifdef NDEBUG
    #define FETCH_NEXT_ONLY_MAYBE_END(f) \
        FETCH_NEXT_ONLY_MAYBE_END_RAW(f)
#else
    #define FETCH_NEXT_ONLY_MAYBE_END(f) \
        do { \
            TRACE_FETCH_DEBUG("FETCH_NEXT_ONLY_MAYBE_END", (f), FALSE); \
            FETCH_NEXT_ONLY_MAYBE_END_RAW(f); \
            TRACE_FETCH_DEBUG("FETCH_NEXT_ONLY_MAYBE_END", (f), TRUE); \
        } while (0)
#endif

// This macro is the workhorse behind DO_NEXT_REFETCH_MAY_THROW.  It is also
// reused by the higher level DO_NEXT_MAY_THROW operation, because it does
// a useful trick.  It is able to do a quick test to see if a value has no
// evaluator behavior, and if so avoid a recursive call to Do_Core().
//
// However, "inert" values can have evaluator behavior--so this requires a
// lookahead check.  Using va_list has already taken one step further than
// it can by using a "prefetch", and it cannot lookahead again without
// saving the value in another location.  Hence the trick is not used with
// vararg input, and INTEGER!/BLOCK!/etc. go through Do_Core() in that case.
//
// IMPORTANT:
//
//  * `index_out` and `index_in` can be the same variable (and usually are)
//  * `value_out` and `value_in` can be the same variable (and usually are)
//
#define DO_CORE_REFETCH_MAY_THROW(dest,f,flags_) \
    do { \
        struct Reb_Frame f_; \
        f_.eval_type = Eval_Table[VAL_TYPE((f)->value)]; \
        if (!(f)->eval_fetched && (f)->indexor != VALIST_FLAG) { \
            if (SPORADICALLY(2)) { /* every OTHER execution fast if DEBUG */ \
                if ( \
                    (f_.eval_type == ET_INERT) \
                    && (IS_END((f)->value + 1) || !ANY_EVAL((f)->value + 1)) \
                ) { \
                    *(dest) = *(f)->value; \
                    (f)->value = ARR_AT((f)->source.array, (f)->indexor); \
                    if (IS_END((f)->value)) \
                        (f)->indexor = END_FLAG; \
                    else \
                        ++(f)->indexor; \
                    break; \
                } \
            } \
        } \
        f_.out = (dest); \
        f_.source = (f)->source; \
        f_.value = (f)->value; \
        f_.indexor = (f)->indexor; \
        f_.gotten = NULL; \
        f_.lookback = FALSE; \
        f_.flags = DO_FLAG_ARGS_EVALUATE | DO_FLAG_NEXT | (flags_); \
        Do_Core(&f_); \
        assert(f_.indexor == VALIST_FLAG || (f)->indexor != f_.indexor); \
        (f)->indexor = f_.indexor; \
        (f)->value = f_.value; \
        (f)->gotten = NULL; \
    } while (0)

//
// DO_NEXT_REFETCH_MAY_THROW (see notes above)
//

#ifdef NDEBUG
    #define DO_NEXT_REFETCH_MAY_THROW(dest,f,flags) \
        DO_CORE_REFETCH_MAY_THROW((dest), (f), (flags))

#else
    #define DO_NEXT_REFETCH_MAY_THROW(dest,f,flags) \
        do { \
            TRACE_FETCH_DEBUG("DO_NEXT_REFETCH_MAY_THROW", (f), FALSE); \
            DO_CORE_REFETCH_MAY_THROW((dest), (f), (flags)); \
            TRACE_FETCH_DEBUG("DO_NEXT_REFETCH_MAY_THROW", (f), TRUE); \
        } while (0)
#endif

//
// QUOTE_NEXT_REFETCH (see notes above)
//

#ifdef NDEBUG
    #define QUOTE_NEXT_REFETCH(dest,f) \
        do { \
            *dest = *(f)->value; \
            FETCH_NEXT_ONLY_MAYBE_END(f); \
            CLEAR_VAL_FLAG(dest, VALUE_FLAG_EVALUATED); \
        } while (0)

#else
    #define QUOTE_NEXT_REFETCH(dest,f) \
        do { \
            TRACE_FETCH_DEBUG("QUOTE_NEXT_REFETCH", (f), FALSE); \
            *dest = *(f)->value; \
            FETCH_NEXT_ONLY_MAYBE_END(f); \
            CLEAR_VAL_FLAG(dest, VALUE_FLAG_EVALUATED); \
            TRACE_FETCH_DEBUG("QUOTE_NEXT_REFETCH", (f), TRUE); \
        } while (0)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  BASIC API: DO_NEXT_MAY_THROW and DO_ARRAY_THROWS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This is an optimized wrapper for the basic building block of Rebol
// evaluation.  They are macros designed to minimize overhead and be
// guaranteed to be inlined at the callsite.  For the central functionality,
// see `Do_Core()`.
//
// The optimized version of the macro will never do an evaluation of a type
// it doesn't have to.  It uses ANY_EVAL() to see if it can get out of making
// a function call...sometimes it cannot because there may be an infix lookup
// possible (we don't know that `[3] + [4]` isn't ever going to work...)  For
// this reason the optimization cannot work with a va_list pointer, as the
// va_list in C cannot be "peeked ahead at" and then put back (while the
// Rebol array data is random access).
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
// DO_VAL_ARRAY_AT_THROWS is another helper for the frequent case where one
// has a BLOCK! or a GROUP! REBVAL at an index which already indicates the
// point where execution is to start.
//
// (The "Throws" name is because it's expected to usually be used in an
// 'if' statement.  It cues you into realizing that it returns TRUE if a
// THROW interrupts this current DO_BLOCK execution--not asking about a
// "THROWN" that happened as part of a prior statement.)
//
// If it returns FALSE, then the DO completed successfully to end of input
// without a throw...and the output contains the last value evaluated in the
// block (empty blocks give void).  If it returns TRUE then it will be the
// THROWN() value.
//

#define DO_NEXT_MAY_THROW(indexor_out,out,array_in,index) \
    do { \
        struct Reb_Frame dummy; /* not a "real frame", Do_Core not called */ \
        dummy.value = ARR_AT((array_in), (index)); \
        if (IS_END(dummy.value)) { \
            SET_VOID(out); \
            (indexor_out) = END_FLAG; \
            break; \
        } \
        dummy.source.array = (array_in); \
        dummy.indexor = (index) + 1; \
        dummy.eval_fetched = NULL; \
        dummy.gotten = NULL; \
        DO_CORE_REFETCH_MAY_THROW((out), &(dummy), DO_FLAG_LOOKAHEAD); \
        if (THROWN(out)) \
            (indexor_out) = THROWN_FLAG; \
        else if (dummy.indexor == END_FLAG) \
            (indexor_out) = END_FLAG; \
        else { \
            assert(dummy.indexor > 1); \
            (indexor_out) = dummy.indexor - 1; \
        } \
    } while (0)

// Note: It is safe for `out` and `array` to be the same variable.  The
// array and index are extracted, and will be protected from GC by the DO
// state...so it is legal to e.g DO_VAL_ARRAY_AT_THROWS(D_OUT, D_OUT).
//
#define DO_VAL_ARRAY_AT_THROWS(out,array) \
    Do_At_Throws((out), VAL_ARRAY(m_cast(REBVAL*, array)), VAL_INDEX(array))

// Lowercase, because doesn't repeat array parameter.  If macro picked head
// off itself, it would need to be uppercase!
//

#define Do_At_Throws(out,array,index) \
    LOGICAL(THROWN_FLAG == Do_Array_At_Core( \
        (out), NULL, (array), (index), \
        DO_FLAG_TO_END | DO_FLAG_ARGS_EVALUATE | DO_FLAG_LOOKAHEAD))

// Because Do_Core can seed with a single value, we seed with our value and
// an EMPTY_ARRAY.  Revisit if there's a "best" dispatcher...
//
#define DO_VALUE_THROWS(out,value) \
    LOGICAL(THROWN_FLAG == Do_Array_At_Core((out), (value), EMPTY_ARRAY, \
        0, DO_FLAG_TO_END | DO_FLAG_ARGS_EVALUATE | DO_FLAG_LOOKAHEAD))


//=////////////////////////////////////////////////////////////////////////=//
//
//  PATH VALUE STATE "PVS"
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The path value state structure is used by `Do_Path_Throws()` and passed
// to the dispatch routines.  See additional comments in %c-path.c.
//

typedef struct Reb_Path_Value_State {
    //
    // `item` is the current element within the path that is being processed.
    // It is advanced as the path is consumed.
    //
    const REBVAL *item;

    // `selector` is the result of evaluating the current path item if
    // necessary.  So if the path is `a/(1 + 2)` and processing the second
    // `item`, then the selector would be the computed value `3`.
    //
    // (This is what the individual path dispatchers should use.)
    //
    const REBVAL *selector;

    // `value` holds the path value that should be chained from.  (It is the
    // type of `value` that dictates which dispatcher is given the `selector`
    // to get the next step.)
    //
    REBVAL *value;

    // `store` is the storage for constructed values, and also where any
    // thrown value will be written.
    //
    REBVAL *store;

    // `setval` is non-NULL if this is a SET-PATH!, and it is the value to
    // ultimately set the path to.  The set should only occur at the end
    // of the path, so most setters should check `IS_END(pvs->item + 1)`
    // before setting.
    //
    // !!! See notes in %c-path.c about why the path dispatch is more
    // complicated than simply being able to only pass the setval to the last
    // item being dispatched (which would be cleaner, but some cases must
    // look ahead with alternate handling).
    //
    const REBVAL *opt_setval;

    // `orig` original path input, saved for error messages
    //
    const REBVAL *orig;
} REBPVS;

enum Path_Eval_Result {
    PE_OK, // pvs->value points to the element to take the next selector
    PE_SET_IF_END, // only sets if end of path
    PE_USE_STORE, // set pvs->value to be pvs->store
    PE_NONE // set pvs->store to NONE and then pvs->value to pvs->store
};

typedef REBINT (*REBPEF)(REBPVS *pvs); // Path evaluator function

typedef REBINT (*REBCTF)(const REBVAL *a, const REBVAL *b, REBINT s);


//=////////////////////////////////////////////////////////////////////////=//
//
//  ARGUMENT AND PARAMETER ACCESS HELPERS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These accessors are designed to make it convenient for natives and actions
// written in C to access their arguments and refinements.  They are able
// to bind to the implicit Reb_Frame* passed to every REBNATIVE() and read
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
    // Capture the argument (and its type) for debug inspection.
    //
    #define PARAM(n,name) \
        const struct Native_Param p_##name = { \
            VAL_TYPE(frame_->arg + (n) - 1), \
            frame_->arg + (n) - 1, \
            (n) \
        }

    // As above, do a cache and be tolerant of framelessness.
    //
    #define REFINE(n,name) \
        const struct Native_Refine p_##name = { \
            IS_CONDITIONAL_TRUE(frame_->arg + (n) - 1), \
            frame_->arg + (n) - 1, \
            (n) \
        }
#endif

// Though REF can only be used with a REFINE() declaration, ARG can be used
// with either.
//
#define ARG(name) \
    (frame_->arg + (p_##name).num - 1)

#define PAR(name) \
    FUNC_PARAM(frame_->func, (p_##name).num) // a TYPESET!

#ifdef NDEBUG
    #define REF(name) \
        IS_CONDITIONAL_TRUE(ARG(name))
#else
    // An added useless ?: helps check in debug build to make sure we do not
    // try to use REF() on something defined as PARAM(), but only REFINE()
    //
    #define REF(name) \
        ((p_##name).used_cache \
            ? IS_CONDITIONAL_TRUE(ARG(name)) \
            : IS_CONDITIONAL_TRUE(ARG(name)))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  CALL FRAME ACCESS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! To be documented and reviewed.  Legacy naming conventions when the
// arguments to functions lived in the data stack gave the name "FS_TOP" for
// "(D)ata (S)tack (F)rame" which is no longer accurate, as well as the
// convention of prefix with a D_.  The new PARAM()/REFINE()/ARG()/REF()
// scheme is coming up on replacing a lot of it, so these will be needing
// a tune up and choices of better names once that is sorted out.  It
// may be as simple as changing these to FS/FSP for "frame stack pointer"
//

#define FS_TOP (TG_Frame_Stack + 0) // avoid assignment to FS_TOP via + 0

#define FRM_IS_VALIST(f) \
    LOGICAL((f)->flags & DO_FLAG_VALIST)

#define FRM_ARRAY(f) \
    (assert(!FRM_IS_VALIST(f)), (f)->source.array)

#define FRM_INDEX(f) \
    (assert(!FRM_IS_VALIST(f)), (f)->indexor == END_FLAG \
        ? ARR_LEN((f)->source.array) : (f)->indexor - 1)

#define FRM_OUT(f)          cast(REBVAL * const, (f)->out) // writable Lvalue
#define FRM_PRIOR(f)        ((f)->prior)
#define FRM_LABEL(f)        ((f)->label_sym)

#define FRM_FUNC(f)         ((f)->func)
#define FRM_DSP_ORIG(f)     ((f)->dsp_orig + 0) // Lvalue

#define FRM_PARAMS_HEAD(f)  FUNC_PARAMS_HEAD((f)->func)

// `arg` is in use to point at the arguments during evaluation, and `param`
// may hold a SET-WORD! or SET-PATH! available for a lookback to quote.
// But during evaluations, `refine` is free.
//
// Since the GC is aware of the pointers, it can protect whatever refine is
// pointing at.  This can be useful for routines that have a local
// memory cell.  This does not require a push or a pop of anything--it only
// protects as long as the native is running.  (This trick is available to
// the dispatchers as well.)
//
#define PROTECT_FRM_X(f,v) \
    ((f)->refine = (v))

// It's not clear exactly in which situations one might be using this; while
// it seems that when filling function args you could just assume it hasn't
// been reified, there may be "pre-reification" in the future, and also a
// tail call optimization or some other "reuser" of a frame may jump in and
// reuse a frame that's been reified after its initial "chunk only" state.
// For now check the flag and don't just assume it's a raw frame.
//
// Uses ARR_AT instead of CTX_VAR because the varlist may not be finished.
//
#define FRM_ARGS_HEAD(f) \
    (((f)->flags & DO_FLAG_HAS_VARLIST) \
        ? (GET_ARR_FLAG((f)->data.varlist, CONTEXT_FLAG_STACK) \
            ? CTX_STACKVARS(AS_CONTEXT((f)->data.varlist)) \
            : ARR_AT((f)->data.varlist, 1)) \
        : &(f)->data.stackvars[0])

// ARGS is the parameters and refinements
// 1-based indexing into the arglist (0 slot is for object/function value)
#ifdef NDEBUG
    #define FRM_ARG(f,n)    ((f)->arg + (n) - 1)
#else
    #define FRM_ARG(f,n)    FRM_ARG_Debug((f), (n)) // checks arg index bound
#endif

// Note about D_NUM_ARGS: A native should generally not detect the arity it
// was invoked with, (and it doesn't make sense as most implementations get
// the full list of arguments and refinements).  However, ACTION! dispatch
// has several different argument counts piping through a switch, and often
// "cheats" by using the arity instead of being conditional on which action
// ID ran.  Consider when reviewing the future of ACTION!.
//
#define FRM_NUM_ARGS(f) \
    cast(REBCNT, FUNC_NUM_PARAMS((f)->func))

// Quick access functions from natives (or compatible functions that name a
// Reb_Frame pointer `frame_`) to get some of the common public fields.
//
#define D_OUT       FRM_OUT(frame_)         // GC-safe slot for output value
#define D_ARGC      FRM_NUM_ARGS(frame_)        // count of args+refinements/args
#define D_ARG(n)    FRM_ARG(frame_, (n))    // pass 1 for first arg
#define D_REF(n)    IS_CONDITIONAL_TRUE(D_ARG(n))  // REFinement (!REFerence)
#define D_FUNC      FRM_FUNC(frame_)        // REBVAL* of running function
#define D_LABEL_SYM FRM_LABEL(frame_)       // symbol or placeholder for call
#define D_DSP_ORIG  FRM_DSP_ORIG(frame_)    // Original data stack pointer

#define D_PROTECT_X(v)      PROTECT_FRM_X(frame_, (v))
#define D_PROTECT_Y(v)      PROTECT_FRM_Y(frame_, (v))

// Frameless native access
//
// !!! Should `frame_` just be renamed to `c_` to make this briefer and be used
// directly?  It is helpful to have macros to find the usages, however.
//
#define D_FRAME      frame_
#define D_ARRAY     (frame_->source.array)
#define D_INDEXOR   (frame_->indexor)
#define D_VALUE     (frame_->value)
#define D_MODE      (frame_->mode)
