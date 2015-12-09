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


// Temporary forward declaration until header reordering
struct Reb_Series;
typedef struct Reb_Series REBSER;

struct Reb_Array;
typedef struct Reb_Array REBARR;

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
    SER_MARK        = 1 << 0,   // was found during GC mark scan.
    SER_FRAME       = 1 << 1,   // object frame (unsets legal, has key series)
    SER_LOCK        = 1 << 2,   // size is locked (do not expand it)
    SER_EXTERNAL    = 1 << 3,   // ->data is external, don't free() on GC
    SER_MANAGED     = 1 << 4,   // series is managed by garbage collection
    SER_ARRAY       = 1 << 5,   // is sizeof(REBVAL) wide and has valid values
    SER_PROTECT     = 1 << 6,   // protected from modification
    SER_POWER_OF_2  = 1 << 7    // true alloc size is rounded to power of 2
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
    REBCNT will_be_bias_and_something_else;
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
        /*struct Reb_Value values[1];*/ // disabled until header reordering
    } content;

    union {
        REBCNT size;    // used for vectors and bitsets
        REBSER *hashlist; // MAP datatype uses this
        REBARR *keylist; // used by FRAME
        struct {
            REBCNT wide:16;
            REBCNT high:16;
        } area;
        REBFLG negated; // for bitsets (can't be EXT flag on just one value)
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

    #ifdef SERIES_LABELS
        const REBYTE *label;       // identify the series
    #endif
#endif
};

#define SERIES_REST(s)   ((s)->content.dynamic.rest)
#define SERIES_FLAGS(s)  ((s)->info)
#define SERIES_WIDE(s)   (((s)->info) & 0xff)
#define SERIES_DATA(s)   ((s)->content.dynamic.data + 0) // +0 => Lvalue!
#define SERIES_AT(s,i)   (SERIES_DATA(s) + (SERIES_WIDE(s) * i))

#define SERIES_LEN(s)           ((s)->content.dynamic.len + 0)
#define SET_SERIES_LEN(s,l)     ((s)->content.dynamic.len = (l))

