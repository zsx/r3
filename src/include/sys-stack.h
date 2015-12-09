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
    cast(REBINT, ARRAY_LEN(DS_Array) - 1)

// Access value at given stack location
#define DS_AT(d) \
    ARRAY_AT(DS_Array, (d))

// Most recently pushed item
#define DS_TOP \
    ARRAY_LAST(DS_Array)

#if !defined(NDEBUG)
    #define IN_DATA_STACK(p) \
        (ARRAY_LEN(DS_Array) != 0 && (p) >= DS_AT(0) && (p) <= DS_TOP)
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
        SERIES_FITS(ARRAY_SERIES(DS_Array), 1) \
            ? cast(void, ++DS_Array->series.content.dynamic.len) \
            : ( \
                SERIES_REST(ARRAY_SERIES(DS_Array)) >= STACK_LIMIT \
                    ? Trap_Stack_Overflow() \
                    : cast(void, cast(REBUPT, Alloc_Tail_Array(DS_Array))) \
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
    (--ARRAY_SERIES(DS_Array)->content.dynamic.len, \
        SET_END(ARRAY_TAIL(DS_Array)), NOOP)

#define DS_POP_INTO(v) \
    do { \
        assert(!IS_TRASH_DEBUG(DS_TOP) || VAL_TRASH_SAFE(DS_TOP)); \
        *(v) = *DS_TOP; \
        DS_DROP; \
    } while (0)

#ifdef NDEBUG
    #define DS_DROP_TO(dsp) \
        (SET_ARRAY_LEN(DS_Array, (dsp) + 1), \
            SET_END(ARRAY_TAIL(DS_Array)), NOOP)
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
