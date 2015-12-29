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
//  Summary: Definitions for Series (REBSER) plus Array, Frame, and Map
//  File: %sys-series.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Note: the word "Series" is overloaded in Rebol to refer to two related but
// distinct concepts:
//
// * The internal system datatype, also known as a REBSER.  It's a low-level
//   implementation of something similar to a vector or an array in other
//   languages.  It is an abstraction which represens a contiguous region
//   of memory containing equally-sized elements.
//
// * The user-level value type ANY-SERIES!.  This might be more accurately
//   called POSITION!, because it includes both a pointer to a REBSER of
//   data and an index offset into that data.  Attempts to reconcile all
//   the naming issues from historical Rebol have not yielded a satisfying
//   alternative, so the ambiguity has stuck.
//
// This file regards the first interpretation of the word series.  For more
// on the ANY-SERIES! value type and its embedded index, see %sys-value.h
// in the definition of `struct Reb_Any_Series`.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A REBSER is a contiguous-memory structure with an optimization of behaving
// like a kind of "double-ended queue".  It is able to reserve capacity at
// both the tail and the head, and when data is taken from the head it will
// retain that capacity...reusing it on later insertions at the head.
//
// The space at the head is called the "bias", and to save on pointer math
// per-access, the stored data pointer is actually adjusted to include the
// bias.  This biasing is backed out upon insertions at the head, and also
// must be subtracted completely to free the pointer using the address
// originally given by the allocator.
//
// The element size in a REBSER is known as the "width".  It is designed
// to support widths of elements up to 255 bytes.  (See note on SERIES_FREED
// about accomodating 256-byte elements.)
//
// REBSERs may be either manually memory managed or delegated to the garbage
// collector.  Free_Series() may only be called on manual series.  See
// MANAGE_SERIES() and PUSH_GUARD_SERIES() for remarks on how to work safely
// with pointers to garbage collected series, to avoid having them be GC'd
// out from under the code while working with them.
//
// This file defines series subclasses which are type-incompatible with
// REBSER for safety.  (In C++ they would be derived classes, so common
// operations would not require casting...but this is C.)  The subclasses
// are explained where they are defined.
//


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBSER (a.k.a. `struct Reb_Series`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This structure is a small fixed-size header for the series, containing
// information about its content.  Every string and block in REBOL uses one
// of these to permit GC and compaction.
//
// The REBSER is fixed-size, and is allocated as a "node" from a memory pool
// that quickly grants and releases memory ranges that are sizeof(REBSER)
// without needing to use malloc() and free() for each individual allocation.
// These nodes can also be enumerated in the pool without needing the series
// to be tracked via a linked list or other structure.  That enumeration is
// done for instance by the garbage collector.
//
// A REBSER node pointer will remain valid as long as outstanding references
// to the series exist in values visible to the GC.  On the other hand, the
// series's data pointer may be freed and reallocated to respond to the needs
// of resizing--and in the future may be reallocated just as an idle task
// by the GC to reclaim or optimize space.  Hence pointers into data in a
// managed series *must not be held onto across evaluations*.
//
// !!! An upcoming feature is the ability to avoid a dynamic allocation for
// the series data values in cases of short series (of lengths 0, 1, or
// perhaps even 2 or more if series nodes can be drawn from different pools).
// This would mean putting the values directly into the series node itself,
// and using the implicit terminating tricks of END to terminate with a
// misc pointer doing double duty for another purpose.  The groundwork is
// laid but there are still some details to work out.
//

// Series Flags
//
enum {
    // `OPT_SER_MARK` is used in the "mark and sweep" method of garbage
    // collection.  It is also used for other purposes which need to go
    // through and set a generic bit, e.g. to protect against loops in
    // the transitive closure ("if you hit a SER_MARK, then you've already
    // processed this series").
    //
    // Because of the dual purpose, it's important to be sure to not run
    // garbage collection while one of these alternate uses is in effect.
    // It's also important to reset the bit when done, as GC assumes when
    // it starts that all bits are cleared.  (The GC itself clears all
    // the bits by enumerating every series in the series pool during the
    // sweeping phase.)
    //
    // !!! With more series bits now available, the dual purpose is something
    // that should be reexamined--so long as bits are free, why reuse if it
    // creates more risk?
    //
    OPT_SER_MARK = 1 << 0,

    // `OPT_SER_MANAGED` indicates that a series is managed by garbage
    // collection.  If this bit is not set, then during the GC's sweeping
    // phase the simple fact that it hasn't been SER_MARK'd won't be enough
    // to let it be considered for freeing.
    //
    // See MANAGE_SERIES for details on the lifecycle of a series (how it
    // starts out manually managed, and then must either become managed or be
    // freed before the evaluation that created it ends).
    //
    OPT_SER_MANAGED = 1 << 1,

    // `OPT_SER_ARRAY` indicates that this is a series of REBVAL values, and
    // is suitable for using as the payload of an ANY-ARRAY! value.  When a
    // series carries this bit, that means that if it is also SER_MANAGED
    // then the garbage collector will process its transitive closure to
    // make sure all the values it contains (and the values its references
    // contain) do not have series GC'd out from under them.
    //
    // (In R3-Alpha, whether a series was an array or not was tested by if
    // its width was sizeof(REBVAL).  The Ren-C approach allows for the
    // creation of series that contain items that incidentally happen to be
    // the same size as a REBVAL, while not actually being REBVALs.)
    //
    OPT_SER_ARRAY = 1 << 2,

    // `OPT_SER_CONTEXT` indicates that this series represents the "varlist"
    // of a context.  A second series can be reached from it via the `->misc`
    // field in the series node, which is a second array known as a "keylist".
    //
    // See notes on REBCON for further details about what a context is.
    //
    OPT_SER_CONTEXT = 1 << 3,

    // `OPT_SER_PARAMLIST` indicates that this series is an array that
    // represents the parameter list of a function.
    //
    // !!! Due to some changes in the workings related to FRAME!, it may be
    // that this flag will not be needed.
    //
    OPT_SER_PARAMLIST = 1 << 4,

    // `OPT_SER_LOCKED` indicates that the series size or values cannot be
    // modified.  This check is honored by some layers of abstraction, but
    // if one manages to get a raw pointer into a value in the series data
    // then by that point it cannot be enforced.
    //
    // !!! Could the 'writable' flag be used for this in the debug build,
    // if the locking process went through and cleared writability...then
    // put it back if the series were unlocked?
    //
    // This is related to the feature in PROTECT (OPT_TYPESET_LOCKED) which
    // protects a certain variable in a context from being changed.  Yet
    // it is distinct as it's a protection on a series itself--which ends
    // up affecting all variable content with that series in the payload.
    //
    OPT_SER_LOCKED = 1 << 5,

    // `OPT_SER_FIXED_SIZE` indicates that the size is fixed, and the series
    // cannot be expanded or contracted.  Values within the series are still
    // writable, assuming OPT_SER_LOCKED isn't set.
    //
    // !!! Is there checking in all paths?  Do series contractions check this?
    //
    // One important reason for ensuring a series is fixed size is to avoid
    // the possibility of the data pointer being reallocated.  This allows
    // code to ignore the usual rule that it is unsafe to hold a pointer to
    // a value inside the series data.
    //
    // !!! Strictly speaking, OPT_SER_NO_RELOCATE could be a different thing
    // from fixed size... if there would be a reason to reallocate besides
    // changing size (such as memory compaction).
    //
    OPT_SER_FIXED_SIZE  = 1 << 6,

    // `OPT_SER_POWER_OF_2` is flagged when an allocation size was rounded to
    // a power of 2.  This flag was introduced in Ren-C when accounting was
    // added to make sure the system's notion of how much memory allocation
    // was outstanding would balance out to zero by the time of exiting the
    // interpreter.
    //
    // The problem was that the allocation size was measured in terms of the
    // number of elements.  If the elements themselves were not the size of
    // a power of 2, then to get an even power-of-2 size of memory allocated
    // the memory block would not be an even multiple of the element size.
    // Rather than track the actual memory allocation size as a 32-bit number,
    // a single bit flag remembering that the allocation was a power of 2
    // was enough to recreate the number to balance accounting at free time.
    //
    // !!! The rationale for why series were ever allocated to a power of 2
    // should be revisited.  Current conventional wisdom suggests that asking
    // for the amount of memory you need and not using powers of 2 is
    // generally a better idea:
    //
    // http://stackoverflow.com/questions/3190146/
    //
    OPT_SER_POWER_OF_2  = 1 << 7,

    // `OPT_SER_EXTERNAL` indicates that when the series was created, the
    // `->data` pointer was poked in by the creator.  It takes responsibility
    // for freeing it, so don't free() on GC.
    //
    // !!! It's not clear what the lifetime management of data used in this
    // way is.  If the external system receives no notice when Rebol is done
    // with the data and GC's the series, how does it know when it's safe
    // to free the data or not?  The feature is not used by the core or
    // Ren-Cpp, but by relatively old extensions...so there may be no good
    // answer in the case of those clients (likely either leaks or crashes).
    //
    OPT_SER_EXTERNAL = 1 << 8
};

struct Reb_Series_Dynamic {
    //
    // `data` is the "head" of the series data.  It may not point directly at
    // the memory location that was returned from the allocator if it has
    // bias included in it.
    //
    REBYTE *data;

    // `len` is one past end of useful data.
    //
    REBCNT len;

    // `rest` is the total number of units from bias to end.  Having a
    // slightly weird name draws attention to the idea that it's not really
    // the "capacity", just the "rest of the capacity after the bias".
    //
    REBCNT rest;

    // This is the 4th pointer on 32-bit platforms which could be used for
    // something when a series is dynamic.  Previously the bias was not
    // a full REBCNT but was limited in range to 16 bits or so.  But if
    // it were here then it would free up a number of flags for the
    // series, which would be helpful as they are necessary
    //
    REBCNT bias;

#if defined(__LP64__) || defined(__LLP64__)
    //
    // The Reb_Series_Dynamic is used in Reb_Series inside of a union with a
    // REBVAL.  On 64-bit machines this will leave one unused 32-bit slot
    // (which will couple with the previous REBCNT) and one naturally aligned
    // 64-bit pointer.  These could be used for some enhancement that would
    // be available per-dynamic-REBSER on 64-bit architectures.
    //
    REBCNT unused_32;
    void *unused_64;
#endif

};

struct Reb_Series {

    union {
        //
        // If the series does not fit into the REBSER node, then it must be
        // dynamically allocated.  This is the tracking structure for that
        // dynamic data allocation.
        //
        struct Reb_Series_Dynamic dynamic;

        // !!! Not yet implemented, but 0 or 1 length series (and maybe other
        // lengths) can be held directly in the series node, with the misc
        // deliberately set to either NULL or another pointer value in order
        // to serve as an implicit terminator.  Coming soon.
        //
        struct Reb_Value values[1];
    } content;

    union {
        REBCNT size;    // used for vectors and bitsets
        REBSER *hashlist; // MAP datatype uses this
        REBARR *keylist; // used by CONTEXT
        struct {
            REBCNT wide:16;
            REBCNT high:16;
        } area;
        REBOOL negated; // for bitsets (can't be EXT flag on just one value)
    } misc;

    //
    // `info` is the information about the series which needs to be known
    // even if it is not using a dynamic allocation.  So even if the alloc
    // size, length, and bias aren't relevant...the series flags need to
    // be known...including the flag of whether this is a dynamic series
    // node or not!
    //
    REBCNT info;

#if defined(__LP64__) || defined(__LLP64__)
    //
    // We need to make sure the next position is naturally aligned.  32-bit
    // platforms it will be, but on 64-bit platforms it won't.  This means
    // that there is an unused 32-bit quantity in each series on 64-bit
    // platforms, similar to the unused 32-bit quantity in each value on
    // 64-bit platforms.  It might be useful for some kind of enhancement
    // in caching or otherwise that a 64-bit build could offer...
    //
    REBCNT unused;
#endif

#if !defined(NDEBUG)
    REBINT *guard; // intentionally alloc'd and freed for use by Panic_Series
#endif
};

// "Series Panics" will (hopefully) trigger an alert under memory tools
// like address sanitizer and valgrind that indicate the call stack at the
// moment of allocation of a series.  Then you should have TWO stacks: the
// one at the call of the Panic, and one where that series was alloc'd.
//
//    THIS FEATURE IS MENTIONED UP TOP BECAUSE IT IS VERY, VERY USEFUL!
//
#if !defined(NDEBUG)
    #define Panic_Series(s) \
        Panic_Series_Debug((s), __FILE__, __LINE__);
#endif

#define SERIES_REST(s)   ((s)->content.dynamic.rest)
#define SERIES_FLAGS(s)  ((s)->info)
#define SERIES_WIDE(s)   (((s)->info) & 0xff)

#define SERIES_LEN(s)           ((s)->content.dynamic.len + 0)
#define SET_SERIES_LEN(s,l)     ((s)->content.dynamic.len = (l))

// Raw access does not demand that the caller know the contained type.  So
// for instance a generic debugging routine might just want a byte pointer
// but have no element type pointer to pass in.
//
#define SERIES_DATA_RAW(s)      ((s)->content.dynamic.data + 0) // Lvalue!
#define SERIES_AT_RAW(s,i)      (SERIES_DATA_RAW(s) + (SERIES_WIDE(s) * i))

//
// In general, requesting a pointer into the series data requires passing in
// a type which is the correct size for the series.  A pointer is given back
// to that type.
//
// Note that series indexing in C is zero based.  So as far as SERIES is
// concerned, `SERIES_HEAD(t, s)` is the same as `SERIES_AT(t, s, 0)`
//

#define SERIES_AT(t,s,i) \
    (assert(SERIES_WIDE(s) == sizeof(t)), cast(t*, SERIES_AT_RAW((s), (i))))

#define SERIES_HEAD(t,s) \
    SERIES_AT(t, (s), 0)

#define SERIES_TAIL(t,s) \
    SERIES_AT(t, (s), SERIES_LEN(s))

#define SERIES_LAST(t,s) \
    (assert(SERIES_LEN(s) != 0), SERIES_AT(t, (s), SERIES_LEN(s) - 1))

// The pooled allocator for REBSERs has an enumeration function where all
// nodes can be visited, and this is used by the garbage collector.  This
// includes nodes that have never been allocated or have been freed, so
// "in-band" inside the REBSER there must be some way to tell if a node is
// live or not.
//
// When the pool is initially allocated it is memset() to zero, hence the
// signal must be some field or bit being zero that is not otherwise used.
// The signal currently used is the "width" being zero.  The only downside
// of this is that it means the sizes range from 1-255, whereas if 0 was
// available the width could always be incremented by 1 and range 1-256.
//
#define SERIES_FREED(s)  (0 == SERIES_WIDE(s))

//
// Series size measurements:
//
// SERIES_TOTAL - bytes of memory allocated (including bias area)
// SERIES_SPACE - bytes of series (not including bias area)
// SERIES_USED - bytes being used, including terminator
//

#define SERIES_TOTAL(s) \
    ((SERIES_REST(s) + SERIES_BIAS(s)) * SERIES_WIDE(s))

#define SERIES_SPACE(s) \
    (SERIES_REST(s) * SERIES_WIDE(s))

#define SERIES_USED(s) \
    ((SERIES_LEN(s) + 1) * SERIES_WIDE(s))

// Returns space that a series has available (less terminator):
#define SERIES_FULL(s) (SERIES_LEN(s) + 1 >= SERIES_REST(s))
#define SERIES_AVAIL(s) (SERIES_REST(s) - (SERIES_LEN(s) + 1))
#define SERIES_FITS(s,n) ((SERIES_LEN(s) + (n) + 1) <= SERIES_REST(s))

// Flag used for extending series at tail:
#define AT_TAIL ((REBCNT)(~0))  // Extend series at tail

//
// Bias is empty space in front of head:
//

#define SERIES_BIAS(s) \
    cast(REBCNT, ((s)->content.dynamic.bias >> 16) & 0xffff)

#define MAX_SERIES_BIAS 0x1000 \

#define SERIES_SET_BIAS(s,b) \
    ((s)->content.dynamic.bias = \
        ((s)->content.dynamic.bias & 0xffff) | (b << 16))

#define SERIES_ADD_BIAS(s,b) \
    ((s)->content.dynamic.bias += (b << 16))

#define SERIES_SUB_BIAS(s,b) \
    ((s)->content.dynamic.bias -= (b << 16))

//
// Series flags
//

#define SERIES_SET_FLAG(s, f) \
    cast(void, (SERIES_FLAGS(s) |= ((f) << 8)))

#define SERIES_CLR_FLAG(s, f) \
    cast(void, (SERIES_FLAGS(s) &= ~((f) << 8)))

#define SERIES_GET_FLAG(s, f) \
    LOGICAL(SERIES_FLAGS(s) & ((f) << 8))

#define Is_Array_Series(s) SERIES_GET_FLAG((s), OPT_SER_ARRAY)

#define FAIL_IF_LOCKED_SERIES(s) \
    if (SERIES_GET_FLAG(s, OPT_SER_LOCKED)) fail (Error(RE_LOCKED))

//
// Optimized expand when at tail (but, does not reterminate)
//

#define EXPAND_SERIES_TAIL(s,l) \
    do { \
        if (SERIES_FITS((s), (l))) (s)->content.dynamic.len += (l); \
        else Expand_Series((s), AT_TAIL, (l)); \
    } while (0)

#define RESIZE_SERIES(s,l) \
    do { \
        (s)->content.dynamic.len = 0; \
        if (!SERIES_FITS((s), (l))) Expand_Series((s), AT_TAIL, (l)); \
        (s)->content.dynamic.len = 0; \
    } while (0)

//
// Termination
//

#define RESET_SERIES(s) \
    ((s)->content.dynamic.len = 0, TERM_SERIES(s))

#define RESET_TAIL(s) \
    ((s)->content.dynamic.len = 0)

// Clear all and clear to tail:
//
#define CLEAR_SEQUENCE(s) \
    do { \
        assert(!Is_Array_Series(s)); \
        CLEAR(SERIES_DATA_RAW(s), SERIES_SPACE(s)); \
    } while (0)

#define TERM_SEQUENCE(s) \
    do { \
        assert(!Is_Array_Series(s)); \
        memset(SERIES_AT_RAW((s), SERIES_LEN(s)), 0, SERIES_WIDE(s)); \
    } while (0)

#ifdef NDEBUG
    #define ASSERT_SERIES_TERM(s) cast(void, 0)
#else
    #define ASSERT_SERIES_TERM(s) Assert_Series_Term_Core(s)
#endif

#ifdef NDEBUG
    #define ASSERT_SERIES(s) cast(void, 0)
#else
    #define ASSERT_SERIES(s) \
        do { \
            if (Is_Array_Series(s)) \
                ASSERT_ARRAY(AS_ARRAY(s)); \
            else \
                ASSERT_SERIES_TERM(s); \
        } while (0)
#endif

// This is a rather expensive check for whether a REBVAL* lives anywhere in
// series memory, and hence may be relocated.  It can be useful for certain
// stress tests to try and catch cases where values that should not be
// living in a series are passed to some routines.
//
#ifdef NDEBUG
    #define ASSERT_NOT_IN_SERIES_DATA(p) NOOP
#else
    #define ASSERT_NOT_IN_SERIES_DATA(v) \
        Assert_Not_In_Series_Data_Debug(v, TRUE)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  SERIES MANAGED MEMORY
//
//=////////////////////////////////////////////////////////////////////////=//
//
// When a series is allocated by the Make_Series routine, it is not initially
// visible to the garbage collector.  To keep from leaking it, then it must
// be either freed with Free_Series or delegated to the GC to manage with
// MANAGE_SERIES.
//
// (In debug builds, there is a test at the end of every Rebol function
// dispatch that checks to make sure one of those two things happened for any
// series allocated during the call.)
//
// The implementation of MANAGE_SERIES is shallow--it only sets a bit on that
// *one* series, not any series referenced by values inside of it.  This
// means that you cannot build a hierarchical structure that isn't visible
// to the GC and then do a single MANAGE_SERIES call on the root to hand it
// over to the garbage collector.  While it would be technically possible to
// deeply walk the structure, the efficiency gained from pre-building the
// structure with the managed bit set is significant...so that's how deep
// copies and the scanner/load do it.
//
// (In debug builds, if any unmanaged series are found inside of values
// reachable by the GC, it will raise an alert.)
//

#define MANAGE_SERIES(s) \
    Manage_Series(s)

#define ENSURE_SERIES_MANAGED(s) \
    (SERIES_GET_FLAG((s), OPT_SER_MANAGED) \
        ? NOOP \
        : MANAGE_SERIES(s))

#ifdef NDEBUG
    #define ASSERT_SERIES_MANAGED(s) \
        NOOP

    #define ASSERT_VALUE_MANAGED(v) \
        NOOP
#else
    #define ASSERT_SERIES_MANAGED(s) \
        do { \
            if (!SERIES_GET_FLAG((s), OPT_SER_MANAGED)) \
                Panic_Series(s); \
        } while (0)

    #define ASSERT_VALUE_MANAGED(v) \
        assert(Is_Value_Managed((v), TRUE))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  GUARDING SERIES FROM GARBAGE COLLECTION
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The garbage collector can run anytime the evaluator runs (and also when
// ports are used).  So if a series has had MANAGE_SERIES run on it, the
// potential exists that any C pointers that are outstanding may "go bad"
// if the series wasn't reachable from the root set.  This is important to
// remember any time a pointer is held across a call that runs arbitrary
// user code.
//
// This simple stack approach allows pushing protection for a series, and
// then can release protection only for the last series pushed.  A parallel
// pair of macros exists for pushing and popping of guard status for values,
// to protect any series referred to by the value's contents.  (Note: This can
// only be used on values that do not live inside of series, because there is
// no way to guarantee a value in a series will keep its address besides
// guarding the series AND locking it from resizing.)
//
// The guard stack is not meant to accumulate, and must be cleared out
// before a command ends.
//

#define PUSH_GUARD_SERIES(s) \
    Guard_Series_Core(s)

#define DROP_GUARD_SERIES(s) \
    do { \
        GC_Series_Guard->content.dynamic.len--; \
        assert((s) == \
            *SERIES_AT(REBSER*, GC_Series_Guard, SERIES_LEN(GC_Series_Guard)) \
        ); \
    } while (0)


//=////////////////////////////////////////////////////////////////////////=//
//
//  BINARY and STRING series
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The current implementation of Rebol's ANY-STRING! type has two different
// series widths that are used.  One is the BYTE_SIZED() series which encodes
// ASCII in the low bits, and Latin-1 extensions in the range 0x80 - 0xFF.
// So long as a codepoint can fit in this range, the string can be stored in
// single bytes:
//
// https://en.wikipedia.org/wiki/Latin-1_Supplement_(Unicode_block)
//
// (Note: This is not to be confused with the other "byte-width" encoding,
// which is UTF-8.  Rebol series routines are not set up to handle insertions
// or manipulations of UTF-8 encoded data in a Reb_Any_String payload at
// this time...it is a format used only in I/O.)
//
// The second format that is used puts codepoints into a 16-bit REBUNI-sized
// element.  If an insertion of a string or character into a byte sized
// string cannot be represented in 0xFF or lower, then the target string will
// be "widened"--doubling the storage space taken and requiring updating of
// the character data in memory.  At this time there are no "in-place"
// cases where a string is reduced from REBUNI to byte sized, but operations
// like Copy_String_Slimming() will scan a source string to see if a byte-size
// copy can be made from a REBUNI-sized one without loss of information.
//
// Byte-sized series are also used by the BINARY! datatype.  There is no
// technical difference between such series used as strings or used as binary,
// the difference comes from being marked REB_BINARY or REB_STRING in the
// header of the value carrying the series.
//
// For easier type-correctness, the series macros are given with names BIN_XXX
// and UNI_XXX.  There aren't distinct data types for the series themselves,
// just REBSER* is used.  Hence BIN_LEN() and UNI_LEN() aren't needed as you
// could just use SERIES_LEN(), but it helps a bit for readability...and an
// assert is included to ensure the size matches up.
//

// Is it a byte-sized series?
//
// !!! This trick in R3-Alpha "works because no other odd size allowed".  Is
// it worth it to prohibit odd sizes for this trick?  An assertion that the
// size is not odd was added to Make_Series; reconsider if this becomes an
// issue at some point.
//
#define BYTE_SIZE(s)    LOGICAL(((s)->info) & 1)

//
// BIN_XXX: Binary or byte-size string seres macros
//

#define BIN_AT(s,n)     SERIES_AT(REBYTE, (s), (n))
#define BIN_HEAD(s)     SERIES_HEAD(REBYTE, (s))
#define BIN_TAIL(s)     SERIES_TAIL(REBYTE, (s))
#define BIN_LAST(s)     SERIES_LAST(REBYTE, (s))

#define BIN_LEN(s)      (assert(BYTE_SIZE(s)), SERIES_LEN(s))

#define SET_BIN_END(s,n) (*BIN_AT(s,n) = 0)

//
// UNI_XXX: Unicode string series macros
//

#define UNI_LEN(s) \
    (assert(SERIES_WIDE(s) == sizeof(REBUNI), SERIES_LEN(s))

#define SET_UNI_LEN(s,l) \
    (assert(SERIES_WIDE(s) == sizeof(REBUNI), SET_SERIES_LEN((s), (l)))

#define UNI_AT(s,n)     SERIES_AT(REBUNI, (s), (n))
#define UNI_HEAD(s)     SERIES_HEAD(REBUNI, (s))
#define UNI_TAIL(s)     SERIES_TAIL(REBUNI, (s))
#define UNI_LAST(s)     SERIES_LAST(REBUNI, (s))

#define UNI_TERM(s)     (*UNI_TAIL(s) = 0)
#define UNI_RESET(s)    (UNI_HEAD(s)[(s)->tail = 0] = 0)

//
// Get a char, from either byte or unicode string:
//

#define GET_ANY_CHAR(s,n) \
    cast(REBUNI, BYTE_SIZE(s) ? BIN_HEAD(s)[n] : UNI_HEAD(s)[n])

#define SET_ANY_CHAR(s,n,c) \
    (BYTE_SIZE(s) \
        ? (BIN_HEAD(s)[n]=(cast(REBYTE, (c)))) \
        : (UNI_HEAD(s)[n]=(cast(REBUNI, (c)))) \
    )


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBARR (a.k.a. "Rebol Array")
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A "Rebol Array" is a series of REBVAL values which is terminated by an
// END marker.  While many operations are shared in common with REBSER, the
// (deliberate) type incompatibility requires either a cast with ARRAY_SERIES
// or use of a wrapper macro from this list.
//
// !!! Write more about the special concerns of arrays here.
//

struct Reb_Array {
    struct Reb_Series series;
};

// These do REBSER <=> REBARR coercion.  Although it's desirable to make
// them type incompatible for most purposes, some operations require treating
// one kind of pointer as the other (and they are both Reb_Series)
//
// !!! See notes on AS_CONTEXT about the dodginess of how this is currently
// done.  But also see the note that it's something that could just be
// disabled by making arrays and series synonyms in "non-type-check" builds.
//
#define AS_ARRAY(s)         (cast(REBARR*, (s)))
#define ARRAY_SERIES(a)     (&(a)->series)

// HEAD, TAIL, and LAST refer to specific value pointers in the array.  An
// empty array should have an END marker in its head slot, and since it has
// no last value then ARRAY_LAST should not be called (this is checked in
// debug builds).  A fully constructed array should always have an END
// marker in its last slot, which is one past the last position that is
// valid for writing a full REBVAL.
//

#define ARRAY_AT(a, n)      SERIES_AT(REBVAL, ARRAY_SERIES(a), (n))
#define ARRAY_HEAD(a)       SERIES_HEAD(REBVAL, ARRAY_SERIES(a))
#define ARRAY_TAIL(a)       SERIES_TAIL(REBVAL, ARRAY_SERIES(a))
#define ARRAY_LAST(a)       SERIES_LAST(REBVAL, ARRAY_SERIES(a))

// As with an ordinary REBSER, a REBARR has separate management of its length
// and its terminator.  Many routines seek to control these independently for
// performance reasons (for better or worse).
//
#define ARRAY_LEN(a) \
    (assert(Is_Array_Series(ARRAY_SERIES(a))), SERIES_LEN(ARRAY_SERIES(a)))

#define SET_ARRAY_LEN(a,l) \
    (assert(Is_Array_Series(ARRAY_SERIES(a))), \
        SET_SERIES_LEN(ARRAY_SERIES(a), (l)))

//
// !!! Write more about termination in series documentation.
//

#define TERM_ARRAY(a) \
    SET_END(ARRAY_TAIL(a))

#define RESET_ARRAY(a) \
    (SET_ARRAY_LEN((a), 0), TERM_ARRAY(a))

#define TERM_SERIES(s) \
    Is_Array_Series(s) \
        ? cast(void, TERM_ARRAY(AS_ARRAY(s))) \
        : cast( \
            void, memset(SERIES_AT_RAW(s, SERIES_LEN(s)), 0, SERIES_WIDE(s)))

// Setting and getting array flags is common enough to want a macro for it
// vs. having to extract the ARRAY_SERIES to do it each time.
//
#define ARRAY_SET_FLAG(a,f)     SERIES_SET_FLAG(ARRAY_SERIES(a), (f))
#define ARRAY_CLR_FLAG(a,f)     SERIES_CLR_FLAG(ARRAY_SERIES(a), (f))
#define ARRAY_GET_FLAG(a,f)     SERIES_GET_FLAG(ARRAY_SERIES(a), (f))

#define FAIL_IF_LOCKED_ARRAY(a) \
    FAIL_IF_LOCKED_SERIES(ARRAY_SERIES(a))

#define PUSH_GUARD_ARRAY(a) \
    PUSH_GUARD_SERIES(ARRAY_SERIES(a))

#define DROP_GUARD_ARRAY(a) \
    DROP_GUARD_SERIES(ARRAY_SERIES(a))

#define MANAGE_ARRAY(array) \
    MANAGE_SERIES(ARRAY_SERIES(array))

#define ENSURE_ARRAY_MANAGED(array) \
    ENSURE_SERIES_MANAGED(ARRAY_SERIES(array))

#define Append_Value(a,v) \
    (*Alloc_Tail_Array((a)) = *(v), NOOP)

#define Copy_Values_Len_Shallow(v,l) \
    Copy_Values_Len_Extra_Shallow((v), (l), 0)

#define Copy_Array_Shallow(a) \
    Copy_Array_At_Shallow((a), 0)

#define Copy_Array_Deep_Managed(a) \
    Copy_Array_At_Extra_Deep_Managed((a), 0, 0)

#define Copy_Array_At_Deep_Managed(a,i) \
    Copy_Array_At_Extra_Deep_Managed((a), (i), 0)

#define Copy_Array_At_Shallow(a,i) \
    Copy_Array_At_Extra_Shallow((a), (i), 0)

#define Copy_Array_Extra_Shallow(a,e) \
    Copy_Array_At_Extra_Shallow((a), 0, (e))

#define Free_Array(a) \
    Free_Series(ARRAY_SERIES(a))

#ifdef NDEBUG
    #define ASSERT_ARRAY(s) cast(void, 0)

    #define ASSERT_ARRAY_MANAGED(array) \
        NOOP
#else
    #define ASSERT_ARRAY(s) Assert_Array_Core(s)

    #define ASSERT_ARRAY_MANAGED(array) \
        ASSERT_SERIES_MANAGED(ARRAY_SERIES(array))

    #define Panic_Array(a) \
        Panic_Series(ARRAY_SERIES(a))

    #define Debug_Array(a) \
        Debug_Series(ARRAY_SERIES(a))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBCON (a.k.a. "Context")
//
//=////////////////////////////////////////////////////////////////////////=//
//
// In Rebol terminology, a "context" is an abstraction which gives two
// parallel arrays, whose indices line up in a correspondence:
//
// * "keylist" - an array that contains TYPESET! values, but which have a
//   symbol ID encoded as an extra piece of information for that key.
//
// * "varlist" - an array of equal length to the keylist, which holds an
//   arbitrary REBVAL in each position that corresponds to its key.
//
// There is an additional aspect of a context which is that it holds two
// extra values in the 0 slot of each array (hence indexing the keys and
// vars is done 1-based).
//

struct Reb_Context {
    struct Reb_Array varlist; // keylist is held in ->misc.keylist
};

#ifdef NDEBUG
    #define ASSERT_CONTEXT(c) cast(void, 0)
#else
    #define ASSERT_CONTEXT(c) Assert_Context_Core(c)
#endif

// Series-to-Frame cocercion
//
// !!! This cast is of a pointer to a type to a pointer to a struct with
// just that type in it may not technically be legal in the standard w.r.t.
// strict aliasing.  Review this mechanic.  There's probably a legal way
// of working around it.  But if worst comes to worst, it can be disabled
// and the two can be made equivalent--except when doing a build for
// type checking purposes.
//
#ifdef NDEBUG
    #define AS_CONTEXT(s)       cast(REBCON*, (s))
#else
    // Put a debug version here that asserts.
    #define AS_CONTEXT(s)       cast(REBCON*, (s))
#endif

// In the gradual shift to where FRAME! can be an ANY-CONTEXT (even though
// it's only one series with its data coming out of the stack) we can
// discern it based on whether the type in the first slot is an
// ANY-FUNCTION!.  Should never be a closure.
//
#define IS_FRAME_CONTEXT(c) \
    ARRAY_GET_FLAG(AS_ARRAY(c), OPT_SER_PARAMLIST)

// Special property: keylist pointer is stored in the misc field of REBSER
//
#define CONTEXT_VARLIST(c) \
    (IS_FRAME_CONTEXT(c) \
        ? NULL /* won't ever have a series...lives in chunk stack */ \
        : &(c)->varlist)

#define CONTEXT_KEYLIST(c) \
    (IS_FRAME_CONTEXT(c) \
        ? AS_ARRAY(c) \
        : ARRAY_SERIES(CONTEXT_VARLIST(c))->misc.keylist)

#define INIT_CONTEXT_KEYLIST(c,k) \
    do { \
        assert(!IS_FRAME_CONTEXT(c)); \
        ARRAY_SERIES(CONTEXT_VARLIST(c))->misc.keylist = (k); \
    } while (0)

// The keys and vars are accessed by positive integers starting at 1.  If
// indexed access is used then the debug build will check to be sure that
// the indexing is legal.  To get a pointer to the first key or value
// regardless of length (e.g. will be an END if 0 keys/vars) use HEAD
//
#define CONTEXT_KEYS_HEAD(c)    ARRAY_AT(CONTEXT_KEYLIST(c), 1)
#define CONTEXT_VARS_HEAD(c)    ARRAY_AT(CONTEXT_VARLIST(c), 1)
#ifdef NDEBUG
    #define CONTEXT_KEY(c,n)    ARRAY_AT(CONTEXT_KEYLIST(c), (n))
    #define CONTEXT_VAR(c,n)    ARRAY_AT(CONTEXT_VARLIST(c), (n))
#else
    #define CONTEXT_KEY(c,n)    CONTEXT_KEY_Debug((c), (n))
    #define CONTEXT_VAR(c,n)    CONTEXT_VAR_Debug((c), (n))
#endif
#define CONTEXT_KEY_SYM(c,n)    VAL_TYPESET_SYM(CONTEXT_KEY((c), (n)))
#define CONTEXT_KEY_CANON(c,n)  VAL_TYPESET_CANON(CONTEXT_KEY((c), (n)))

// Navigate from context to context components.  Note that the context's
// "length" does not count the [0] cell of either the varlist or the keylist.
// Hence it must subtract 1.  Internally to the context building code, the
// real length of the two series must be accounted for...so the 1 gets put
// back in, but most clients are only interested in the number of keys/values
// (and getting an answer for the length back that was the same as the length
// requested in context creation).
//
#define CONTEXT_LEN(c)          (ARRAY_LEN(CONTEXT_VARLIST(c)) - 1)
#define CONTEXT_VALUE(c)        ARRAY_HEAD(CONTEXT_VARLIST(c))
#define CONTEXT_ROOTKEY(c)      ARRAY_HEAD(CONTEXT_KEYLIST(c))
#define CONTEXT_TYPE(c)         VAL_TYPE(CONTEXT_VALUE(c))
#define CONTEXT_SPEC(c)         VAL_CONTEXT_SPEC(CONTEXT_VALUE(c))
#define CONTEXT_BODY(c)         VAL_CONTEXT_BODY(CONTEXT_VALUE(c))

#define FAIL_IF_LOCKED_CONTEXT(c) \
    FAIL_IF_LOCKED_ARRAY(CONTEXT_VARLIST(c))

#define FREE_CONTEXT(c) \
    do { \
        Free_Array(CONTEXT_KEYLIST(c)); \
        Free_Array(CONTEXT_VARLIST(c)); \
    } while (0)

#define PUSH_GUARD_CONTEXT(c) \
    PUSH_GUARD_ARRAY(CONTEXT_VARLIST(c)) // varlist points to/guards keylist

#define DROP_GUARD_CONTEXT(c) \
    DROP_GUARD_ARRAY(CONTEXT_VARLIST(c))

#ifdef NDEBUG
    #define MANAGE_CONTEXT(c) \
        (MANAGE_ARRAY(CONTEXT_VARLIST(c)), \
            MANAGE_ARRAY(CONTEXT_KEYLIST(c)))

    #define ENSURE_CONTEXT_MANAGED(c) \
        (ARRAY_GET_FLAG(CONTEXT_VARLIST(c), OPT_SER_MANAGED) \
            ? NOOP \
            : MANAGE_CONTEXT(c))
#else
    //
    // Debug build includes testing that the managed state of the context and
    // its word series is the same for the "ensure" case.  It also adds a
    // few assert macros.
    //
    #define MANAGE_CONTEXT(c) \
        Manage_Context_Debug(c)

    #define ENSURE_CONTEXT_MANAGED(c) \
        ((ARRAY_GET_FLAG(CONTEXT_VARLIST(c), OPT_SER_MANAGED) \
        && ARRAY_GET_FLAG(CONTEXT_KEYLIST(c), OPT_SER_MANAGED)) \
            ? NOOP \
            : MANAGE_CONTEXT(c))

    #define Panic_Context(c) \
        Panic_Array(CONTEXT_VARLIST(c))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBFUN (a.k.a. "Func")
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Using a technique strongly parallel to CONTEXT, a function is identified
// by a series which acts as its paramlist, in which the 0th element is an
// ANY-FUNCTION! value.  Unlike a CONTEXT, a FUNC does not have values of its
// own... only parameter definitions (or "params").  The arguments ("args")
// come from finding a function instantiation on the stack.
//

struct Reb_Func {
    struct Reb_Array paramlist;
};

#ifdef NDEBUG
    #define AS_FUNC(s)     cast(REBFUN*, (s))
#else
    // Put a debug version here that asserts.
    #define AS_FUNC(s)     cast(REBFUN*, (s))
#endif

#define FUNC_PARAMLIST(f)       (&(f)->paramlist)

#define FUNC_NUM_PARAMS(f)      (ARRAY_LEN(FUNC_PARAMLIST(f)) - 1)
#define FUNC_PARAMS_HEAD(f)     ARRAY_AT(FUNC_PARAMLIST(f), 1)
#ifdef NDEBUG
    #define FUNC_PARAM(f,n)     ARRAY_AT(FUNC_PARAMLIST(f), (n))
#else
    #define FUNC_PARAM(f,n)     FUNC_PARAM_Debug((f), (n))
#endif
#define FUNC_PARAM_SYM(f,n)     VAL_TYPESET_SYM(FUNC_PARAM((f), (n)))

#define FUNC_VALUE(f)           ARRAY_HEAD(FUNC_PARAMLIST(f))
#define FUNC_SPEC(f)            (FUNC_VALUE(f)->payload.any_function.spec)
#define FUNC_CODE(f)            (FUNC_VALUE(f)->payload.any_function.impl.code)
#define FUNC_BODY(f)            (FUNC_VALUE(f)->payload.any_function.impl.body)
#define FUNC_ACT(f)             (FUNC_VALUE(f)->payload.any_function.impl.act)
#define FUNC_INFO(f)            (FUNC_VALUE(f)->payload.any_function.impl.info)


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBMAP (a.k.a. "Rebol Map")
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Maps are implemented as a light hashing layer on top of an array.  The
// hash indices are stored in the series node's "misc", while the values are
// retained in pairs as `[key val key val key val ...]`.
//
// When there are too few values to warrant hashing, no hash indices are
// made and the array is searched linearly.  This is indicated by the hashlist
// being NULL.
//
// Though maps are not considered a series in the "ANY-SERIES!" value sense,
// they are implemented using series--and hence are in %sys-series.h, at least
// until a better location for the definition is found.
//

struct Reb_Map {
    struct Reb_Array pairlist; // hashlist is held in ->misc.hashlist
};

#define MAP_PAIRLIST(m)         (&(m)->pairlist)
#define MAP_HASHLIST(m)         (ARRAY_SERIES(&(m)->pairlist)->misc.hashlist)
#define MAP_HASHES(m)           SERIES_HEAD(MAP_HASHLIST(m))

// !!! Should there be a MAP_LEN()?  Current implementation has NONE in
// slots that are unused, so can give a deceptive number.  But so can
// objects with hidden fields, locals in paramlists, etc.

#define AS_MAP(s)               (*cast(REBMAP**, &(s)))

#ifdef NDEBUG
    #define VAL_MAP(v)          AS_MAP(VAL_ARRAY(v))
#else
    #define VAL_MAP(v)          (*VAL_MAP_Ptr_Debug(v))
#endif
