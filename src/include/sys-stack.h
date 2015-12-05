/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Summary: REBOL Stack Definitions
**  Module:  sys-stack.h
**  Notes:
**
**  This contains the definitions for the DATA STACK (DS_*)
**
**  The data stack is mostly for REDUCE and COMPOSE, which use it
**  as a common buffer for values that are being gathered to be
**  inserted into another series.  It's better to go through this
**  buffer step because it means the precise size of the new
**  insertions are known ahead of time.  If a series is created,
**  it will not waste space or time on expansion, and if a series
**  is to be inserted into as a target, the proper size gap for
**  the insertion can be opened up exactly once (without any
**  need for repeatedly shuffling on individual insertions).
**
**  Beyond that purpose, the data stack can also be used as a
**  place to store a value to protect it from the garbage
**  collector.  The stack must be balanced in the case of success
**  when a native or action runs.  But if `fail` is used to trigger
**  an error, then the stack will be automatically balanced in
**  the trap handling.
**
**  The data stack specifically needs contiguous memory for its
**  applications.  That is more important than having stability
**  of pointers to any data on the stack.  Hence if any push or
**  pops can happen, there is no guarantee that the pointers will
**  remain consistent...as the memory buffer may need to be
**  reallocated (and hence relocated).  The index positions will
**  remain consistent, however: and using DSP and DS_AT it is
**  possible to work with stack items by index.
**
**  Note: The requirements for the call stack differ from the data
**  stack, due to a need for pointer stability.  Being an ordinary
**  series, the data stack will relocate its memory on expansion.
**  This creates problems for natives and actions where pointers to
**  parameters are saved to variables from D_ARG(N) macros.  These
**  would need a refresh after every potential expanding operation.
**
***********************************************************************/


/***********************************************************************
**
**  At the moment, the data stack is *mostly* implemented as a typical
**  series.  Pushing unfilled slots on the stack (via PUSH_TRASH_UNSAFE)
**  partially inlines Alloc_Tail_List, so it only pays for the function
**  call in cases where expansion is necessary.
**
**  When Rebol was first open-sourced, there were other deviations from
**  being a normal series.  It was not terminated with an END, so
**  you would be required to call a special DS_TERMINATE() routine to
**  put the terminator in place before using the data stack with a
**  routine that expected termination.  It also had to be expanded
**  manually, so a DS_PUSH was not guaranteed to trigger a potential
**  growth of the stack--if expansion hadn't been anticipated with a
**  large enough space for that push, it would corrupt memory.
**
**  Overall, optimizing the stack structure should be easier now that
**  it has a more dedicated purpose.  So those tricks are not being
**  used for the moment.  Future profiling can try those and other
**  approaches when a stable and complete system has been achieved.
**
***********************************************************************/

// (D)ata (S)tack "(P)ointer" is an integer index into Rebol's data stack
#define DSP \
    cast(REBINT, SERIES_TAIL(DS_Series) - 1)

// Access value at given stack location
#define DS_AT(d) \
    BLK_SKIP(DS_Series, (d))

// Most recently pushed item
#define DS_TOP \
    BLK_LAST(DS_Series)

#if !defined(NDEBUG)
    #define IN_DATA_STACK(p) \
        (SERIES_TAIL(DS_Series) != 0 && (p) >= DS_AT(0) && (p) <= DS_TOP)
#endif

// PUSHING: Note the DS_PUSH macros inherit the property of SET_XXX that
// they use their parameters multiple times.  Don't use with the result of
// a function call because that function could be called multiple times.
//
// If you push "unsafe" trash to the stack, it has the benefit of costing
// nothing extra in a release build for setting the value (as it is just
// left uninitialized).  But you must make sure that a GC can't run before
// you have put a valid value into the slot you pushed.

#define DS_PUSH_TRASH \
    ( \
        SERIES_FITS(DS_Series, 1) \
            ? cast(void, ++DS_Series->tail) \
            : ( \
                SERIES_REST(DS_Series) >= STACK_LIMIT \
                    ? Trap_Stack_Overflow() \
                    : cast(void, cast(REBUPT, Alloc_Tail_Array(DS_Series))) \
            ), \
        SET_TRASH_IF_DEBUG(DS_TOP) \
    )

#define DS_PUSH_TRASH_SAFE \
    (DS_PUSH_TRASH, SET_TRASH_SAFE(DS_TOP), NOOP)

#define DS_PUSH(v) \
    (ASSERT_VALUE_MANAGED(v), DS_PUSH_TRASH, *DS_TOP = *(v), NOOP)

#define DS_PUSH_UNSET \
    (DS_PUSH_TRASH, SET_UNSET(DS_TOP), NOOP)

#define DS_PUSH_NONE \
    (DS_PUSH_TRASH, SET_NONE(DS_TOP), NOOP)

#define DS_PUSH_TRUE \
    (DS_PUSH_TRASH, SET_TRUE(DS_TOP), NOOP)

#define DS_PUSH_INTEGER(n) \
    (DS_PUSH_TRASH, SET_INTEGER(DS_TOP, (n)), NOOP)

#define DS_PUSH_DECIMAL(n) \
    (DS_PUSH_TRASH, SET_DECIMAL(DS_TOP, (n)), NOOP)

// POPPING AND "DROPPING"

#define DS_DROP \
    (--DS_Series->tail, SET_END(BLK_TAIL(DS_Series)), NOOP)

#define DS_POP_INTO(v) \
    do { \
        assert(!IS_TRASH_DEBUG(DS_TOP) || VAL_TRASH_SAFE(DS_TOP)); \
        *(v) = *DS_TOP; \
        DS_DROP; \
    } while (0)

#ifdef NDEBUG
    #define DS_DROP_TO(dsp) \
        (DS_Series->tail = (dsp) + 1, SET_END(BLK_TAIL(DS_Series)), NOOP)
#else
    #define DS_DROP_TO(dsp) \
        do { \
            assert(DSP >= (dsp)); \
            while (DSP != (dsp)) {DS_DROP;} \
        } while (0)
#endif


//
// CHUNK STACK
//
// Like the data stack, the values living in the chunk stack are protected
// from garbage collection.
//
// Unlike the data stack, the chunk stack allows for the pushing and popping
// of arbitrary-sized arrays of values which will not be relocated during
// their lifetime.
//
// This is accomplished using a custom "chunked" allocator.  The two structs
// involved are a list of "Chunkers", which internally have a list of
// "Chunks" threaded between them.  The method keeps one spare chunker
// allocated, and only frees a chunker when a full chunker prior has the last
// element popped out of it.  In memory it looks like this:
//
//      [chunker->next
//          (->payload_left size [value1][value2][value3]...)   // chunk 1
//          (->payload_left size [value1]...)                   // chunk 2
//          (->payload_left size [value1][value2]...)           // chunk 3
//          ...remaining payload space in chunker...
//      ]
//
// Since the chunker size is a known constant, it's possible to quickly deduce
// the chunker a chunk lives in from its pointer and the remaining payload
// amount in the chunker.
//

struct Reb_Chunker;

#define CS_CHUNKER_PAYLOAD (2048 - sizeof(struct Reb_Chunker*))

struct Reb_Chunker {
    struct Reb_Chunker *next;
    REBYTE payload[CS_CHUNKER_PAYLOAD];
};

struct Reb_Chunk;

struct Reb_Chunk {
    //
    // Pointer to the previous chunk.  We rely upon the fact that the low
    // bit of this pointer is always 0 in order for it to be an implicit END
    // for the value array of the previous chunk.
    //
    struct Reb_Chunk *prev;

    //
    // How many bytes are left in the memory chunker this chunk lives in
    // (its own size has already been subtracted from the amount)
    //
    REBCNT payload_left;

    REBCNT size;  // Needed after `payload_left` for 64-bit alignment

    // The `values` is an array whose real size exceeds the struct.  (It is
    // set to a size of one because it cannot be [0] in C++.)  When the
    // value pointer is given back to the user, this is how they speak about
    // the chunk itself.
    //
    // See note above about how the next chunk's `prev` pointer serves as
    // an END marker for this array (which may or may not be necessary for
    // the client's purposes, but function arg lists do make use of it)
    //
    REBVAL values[1];
};

// If we do a sizeof(struct Reb_Chunk) then it includes a value in it that we
// generally don't want for our math, due to C++ "no zero element array" rule
//
#define BASE_CHUNK_SIZE (sizeof(struct Reb_Chunk) - sizeof(REBVAL))


// !!! DSF is to be renamed (C)all (S)tack (P)ointer, but being left as DSF
// in the initial commit to try and cut back on the disruption seen in
// one commit, as there are already a lot of changes.

#define DSF (CS_Running + 0) // avoid assignment to DSF via + 0

#define DSF_OUT(c)          cast(REBVAL * const, (c)->out) // writable Lvalue
#define PRIOR_DSF(c)        ((c)->prior)
#define DSF_ARRAY(c)        cast(REBSER * const, (c)->array) // Lvalue
#define DSF_EXPR_INDEX(c)   ((c)->expr_index + 0) // Lvalue
#define DSF_LABEL_SYM(c)    ((c)->label_sym + 0) // Lvalue
#define DSF_FUNC(c)         c_cast(const REBVAL * const, &(c)->func)
#define DSF_DSP_ORIG(c)     ((c)->dsp_orig + 0) // Lvalue

#define DSF_PARAM_HEAD(c) \
    VAL_FUNC_PARAM(&(c)->func, 1)

#define DSF_ARG_HEAD(c) \
    (IS_CLOSURE(&(c)->func) \
        ? BLK_SKIP((c)->arglist.array, 1) \
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
    cast(REBCNT, VAL_FUNC_NUM_PARAMS(&(c)->func))

#define DSF_CELL(c) (&(c)->cell)


// Quick access functions from natives (or compatible functions that name a
// Reb_Call pointer `call_`) to get some of the common public fields.
//
#define D_OUT       DSF_OUT(call_)          // GC-safe slot for output value
#define D_ARGC      DSF_ARGC(call_)         // count of args+refinements/args
#define D_ARG(n)    DSF_ARG(call_, (n))     // pass 1 for first arg
#define D_REF(n)    (!IS_NONE(D_ARG(n)))    // D_REFinement (not D_REFerence)
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


//
// The compiler will *not* optimize out const pointers as captures of the
// arguments.  Hence if you wrote the following inside of a native, you
// would pay for both a pointer variable *and* an assignment each call:
//
//     REBVAL * const foo = ARG(1);
//
//     if (IS_INTEGER(foo))) { ... }
//
// The real way to get peak performance with no extra storage or cost of
// assignment is to instead declare an integer constant and use the macro
// each time:
//
//     const int foo = 1;
//
//     if (IS_INTEGER(ARG(foo))) { ... }
//
// To help make this less error-prone and systemic, this provides some macros:
//
//     PARAM(1, foo);
//     REFINE(2, bar);
//
//     if (IS_INTEGER(ARG(foo)) && REF(bar)) { ... }
//
// Under the hood `PARAM(1, foo)` and `REFINE(2, bar)` declare structs.
// In an optimized build, these structures disappear completely, with all
// addressing done directly into the call frame's cached `vars` pointer.
//
// To aid in debugging, the debug build version of the structures contain
// the actual pointers to the arguments.  It also keeps a copy of a cache
// of the type for the arguments, because the bitfields inside of a REBVAL
// must be integer and hence don't show the name of the type.  Whether
// a refinemnent was in use or not at the time of the call is also cached.
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

// The PARAM and REFINE macros use token pasting to name the variables they
// are declaring `p_name` instead of just `name`.  This prevents collisions
// with C++ identifiers, so `p_case` and `p_new` instead of just `case` and
// `new` as the variable names.  (This is only visible in the debugger.)
//
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
            call_->arg ? !IS_NONE(call_->arg + (n)) : TRUE, \
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
    VAL_FUNC_PARAM(&call_->func, (p_##name).num) // a TYPESET!

#ifdef NDEBUG
    #define REF(name) \
        (!IS_NONE(ARG(name)))
#else
    // An added useless ?: helps check in debug build to make sure we do not
    // try to use REF() on something defined as PARAM(), but only REFINE()
    //
    #define REF(name) \
        ((p_##name).used_cache ? !IS_NONE(ARG(name)) : !IS_NONE(ARG(name)))
#endif

// OUT is the write location in the call frame for the output.  Historically
// it was referred to as D_OUT, but it is used so frequently as to warrant
// taking the word with no qualifier.  Transition is a work in progress.
//
#define OUT D_OUT
