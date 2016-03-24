//
//  File: %sys-value.h
//  Summary: {Definitions for the Rebol Value Struct (REBVAL) and Helpers}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
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
// REBVAL is the structure/union for all Rebol values. It's designed to be
// four C pointers in size (so 16 bytes on 32-bit platforms and 32 bytes
// on 64-bit platforms).  Operation will be most efficient with those sizes,
// and there are checks on boot to ensure that `sizeof(REBVAL)` is the
// correct value for the platform.  But from a mechanical standpoint, the
// system should be *able* to work even if the size is different.
//
// Of the four 32-or-64-bit slots that each value has, the first is used for
// the value's "Header".  This includes the data type, such as REB_INTEGER,
// REB_BLOCK, REB_STRING, etc.  Then there are 8 flags which are for general
// purposes that could apply equally well to any type of value (including
// whether the value should have a new-line after it when molded out inside
// of a block).  There are 8 bits which are custom to each type--for
// instance whether a key in an object is hidden or not.  Then there are
// 8 bits currently reserved for future use.
//
// The remaining content of the REBVAL struct is the "Payload".  It is the
// size of three (void*) pointers, and is used to hold whatever bits that
// are needed for the value type to represent itself.  Perhaps obviously,
// an arbitrarily long string will not fit into 3*32 bits, or even 3*64 bits!
// You can fit the data for an INTEGER or DECIMAL in that (at least until
// they become arbitrary precision) but it's not enough for a generic BLOCK!
// or a FUNCTION! (for instance).  So those pointers are used to point to
// things, and often they will point to one or more Rebol Series (see
// %sys-series.h for an explanation of REBSER, REBARR, REBCTX, and REBMAP.)
//
// While some REBVALs are in C stack variables, most reside in the allocated
// memory block for a Rebol series.  The memory block for a series can be
// resized and require a reallocation, or it may become invalid if the
// containing series is garbage-collected.  This means that many pointers to
// REBVAL are unstable, and could become invalid if arbitrary user code
// is run...this includes values on the data stack, which is implemented as
// a series under the hood.  (See %sys-stack.h)
//
// A REBVAL in a C stack variable does not have to worry about its memory
// address becoming invalid--but by default the garbage collector does not
// know that value exists.  So while the address may be stable, any series
// it has in the payload might go bad.  Use PUSH_GUARD_VALUE() to protect a
// stack variable's payload, and then DROP_GUARD_VALUE() when the protection
// is not needed.  (You must always drop the last guard pushed.)
//
// For a means of creating a temporary array of GC-protected REBVALs, see
// the "chunk stack" in %sys-stack.h.  This is used when building function
// argument frames, which means that the REBVAL* arguments to a function
// accessed via ARG() will be stable as long as the function is running.
//

//
// Note: Forward declarations are in %reb-defs.h
//

#ifndef VALUE_H
#define VALUE_H

// The definition of the REBVAL struct has a header followed by a payload.
// On 32-bit platforms the header is 32 bits, and on 64-bit platforms it is
// 64-bits.  However, even on 32-bit platforms, some payloads contain 64-bit
// quantities (doubles or 64-bit integers).  By default, the compiler would
// pad a payload with one 64-bit quantity and one 32-bit quantity to 128-bits,
// which would not leave room for the header (if REBVALs are to be 128-bits).
//
// So since R3-Alpha, a `#pragma pack` of 4 is requested for this file:
//
//     http://stackoverflow.com/questions/3318410/
//
// It is restored to the default via `#pragma pack()` at the end of the file.
// (Note that pack(push) and pack(pop) are not supported by older compilers.)
//
// Compilers are free to ignore pragmas (or error on them).  Also, this
// packing subverts the automatic alignment handling of the compiler.  So if
// the manually packed structures do not position 64-bit values on 64-bit
// alignments, there can be problems.  On x86 this is generally just slower
// reads and writes, but on more esoteric platforms (like the C-to-Javascript
// translator "emscripten") some instances do not work at all.
//
// Hence REBVAL payloads that contain quantities that need 64-bit alignment
// put those *after* a platform-pointer sized field, even if that field is
// just padding.  On 32-bit platforms this will pair with the header to make
// enough space to get to a 64-bit alignment
//
// If #pragma pack is disabled, a REBVAL will wind up being 160 bits.  This
// won't give ideal performance, and will trigger an assertion in
// Assert_Basics.  But the code *should* be able to work otherwise if that
// assertion is removed.
//
#pragma pack(4)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE HEADER (`struct Reb_Value_Header`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The layout of the header corresponds to the following bitfield
// structure on big endian machines:
//
//    unsigned specific:16;     // flags that can apply to any REBVAL kind
//    unsigned general:8;       // flags that can apply to any kind of REBVAL
//    unsigned kind:6;          // underlying system datatype (64 kinds)
//    unsigned settable:1;      // for debug build only--"formatted" to write
//    unsigned not_end:1;       // not an end marker
//
// Due to a desire to be able to assign all the header bits in one go
// with a native-platform-sized int, this is done with bit masking.
// Using bitfields would bring in questions of how smart the
// optimizer is, as well as the endianness of the underlyling machine.
//
// We use REBUPT (Rebol's pre-C99 compatibility type for uintptr_t,
// which is just uintptr_t from C99 on).  Only the low 32 bits are used
// on 64-bit machines in order to make sure all the features work on
// 32-bit machines...but could be used for some optimization or caching
// purpose to enhance the 64-bit build.  No such uses implemented yet.
//

struct Reb_Value_Header {
    REBUPT bits;
};

// `NOT_END_MASK`
//
// If set, it means this is *not* an end marker.  The bit has been picked
// strategically to be in the negative sense, and in the lowest bit position.
// This means that any even-valued unsigned integer REBUPT value can be used
// to implicitly signal an end.
//
// If this bit is 0, it means that *no other header bits are valid*, as it
// may contain arbitrary data used for non-REBVAL purposes.
//
// Note that the value doing double duty as a number for one purpose and an
// END marker as another *must* be another REBUPT.  It cannot be a pointer
// (despite being guaranteed-REBUPT-sized, and despite having a value that
// is 0 when you mod it by 2.  So-called "type-punning" is unsafe with a
// likelihood of invoking "undefined behavior", while it's the compiler's
// responsibility to guarantee that pointers to memory of the same type of
// data be compatibly read-and-written.
//
#define NOT_END_MASK cast(REBUPT, 0x01)

#define GENERAL_VALUE_BIT 8
#define TYPE_SPECIFIC_BIT 16

// `WRITABLE_MASK_DEBUG`
//
// This is for the debug build, to make it safer to use the implementation
// trick of NOT_END_MASK.  It indicates the slot is "REBVAL sized", and can
// be written into--including to be written with SET_END().
//
// It's again a strategic choice--the 2nd lowest bit and in the negative.
// This means any REBUPT value whose % 4 within a container doing
// double-duty as an implicit terminator for the contained values can
// trigger an alert if the values try to overwrite it.
//
#if defined(__cplusplus) && !defined(NDEBUG)
    //
    // We want to be assured that we are not trying to take the type of a
    // value that is actually an END marker, because end markers chew out only
    // one bit--the rest of the REBUPT bits besides the bottom two may be
    // anything necessary for the purpose.
    //
    // Only the C++ build honors the writability mask, because REBVAL is
    // defined to a struct with a constructor that initializes the cell.
    // If the C build wanted to also get the debug check, every REBVAL
    // stack initialization would have to set it explicitly:
    //
    //     REBVAL value;
    //     VAL_INIT_WRITABLE_DEBUG(&value); // C++ REBVAL does in constructor
    //     SET_BLANK(value);
    //
    // In practice this was too unwieldy, so enhanced debugging of "mystery
    // bugs" from stray writes should likely be done in a C++ build.
    //
    #define WRITABLE_MASK_DEBUG \
        cast(REBUPT, 0x02)
#else
    //
    // Though in the abstract it is desirable to have a way to protect an
    // individual value from writing even in the release build, this is
    // a debug-only check...it makes every value header initialization have
    // to do two writes instead of one (one to format, then later to write
    // and check that it is properly formatted space)
    //
    // !!! We define the WRITABLE_MASK_DEBUG as 0 for the C build, though the
    // specific-binding branch is organized to not have it at all
    //
    #define WRITABLE_MASK_DEBUG \
        cast(REBUPT, 0x00)
#endif

// The type mask comes up a bit and it's a fairly obvious constant, so this
// hardcodes it for obviousness.  High 6 bits of the lowest byte.
//
#define HEADER_TYPE_MASK cast(REBUPT, 0xFC)


//=////////////////////////////////////////////////////////////////////////=//
//
//  END marker (not a value type, only writes `struct Reb_Value_Flags`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Historically Rebol arrays were always one value longer than their maximum
// content, and this final slot was used for a special REBVAL called END!.
// Like a null terminator in a C string, it was possible to start from one
// point in the series and traverse to find the end marker without needing
// to maintain a count.  Rebol series store their length also--but it's
// faster and more general to use the terminator.
//
// Ren-C changed this so that end is not a data type, but rather seeing a
// header slot with the lowest bit set to 0.  (See NOT_END_MASK for
// an explanation of this choice.)  The upshot is that a data structure
// designed to hold Rebol arrays is able to terminate an array at full
// capacity with a pointer-sized integer with the lowest 2 bits clear, and
// use the rest of the bits for other purposes.  (See WRITABLE_MASK_DEBUG
// for why it's the low 2 bits and not just the lowest bit.)
//
// This means not only is a full REBVAL not needed to terminate, the sunk cost
// of an existing 32-bit or 64-bit number (depending on platform) can be used
// to avoid needing even 1/4 of a REBVAL for a header to terminate.  (See the
// `size` field in `struct Reb_Chunk` from %sys-stack.h for a simple example
// of the technique.)
//
// !!! Because Rebol Arrays (REBARR) have both a length and a terminator, it
// is important to keep these in sync.  R3-Alpha sought to give code the
// freedom to work with unterminated arrays if the cost of writing terminators
// was not necessary.  Ren-C pushed back against this to try and be more
// uniform to get the invariants under control.  A formal balance is still
// being sought of when terminators will be required and when they will not.
//

#define IS_END_MACRO(v) \
    LOGICAL((v)->header.bits % 2 == 0)

#ifdef NDEBUG
    #define IS_END(v)       IS_END_MACRO(v)
#else
    #define IS_END(v)       IS_END_Debug((v), __FILE__, __LINE__)
#endif

#define NOT_END(v)          NOT(IS_END(v))

#ifdef NDEBUG
    #define SET_END(v)      ((v)->header.bits = 0)
#else
    //
    // The slot we are trying to write into must have at least been formatted
    // in the debug build INIT_CELL_WRITABLE_IF_DEBUG().  Otherwise it could be a
    // pointer with its low bit clear, doing double-duty as an IS_END(),
    // marker...which we cannot overwrite...not even with another END marker.
    //
    #define SET_END(v) \
        (ASSERT_CELL_WRITABLE_IF_DEBUG((v), __FILE__, __LINE__), \
            (v)->header.bits = WRITABLE_MASK_DEBUG | REB_MAX)
#endif

// Pointer to a global END marker.  Though this global value is allocated to
// the size of a REBVAL, only the header is initialized.  This means if any
// of the value payload is accessed, it will trip memory checkers like
// Valgrind or Address Sanitizer to warn of the mistake.
//
#define END_CELL (&PG_End_Cell)


//=////////////////////////////////////////////////////////////////////////=//
//
//  GENERAL FLAGS common to every REBVAL
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The value option flags are 8 individual bitflags which apply to every
// value of every type.  Due to their scarcity, they are chosen carefully.
//

enum {
    // `VALUE_FLAG_FALSE`
    //
    // Both BLANK! and LOGIC!'s false state are FALSE? ("conditionally false").
    // All other types are TRUE?.  To make checking FALSE? and TRUE? faster,
    // this bit is set when creating NONE! or FALSE.  As a result, LOGIC!
    // does not need to store any data in its payload... its data of being
    // true or false is already covered by this header bit.
    //
    VALUE_FLAG_FALSE = 1 << (GENERAL_VALUE_BIT + 0),

    // `VALUE_FLAG_LINE`
    //
    // If the line marker bit is 1, then when the value is molded it will put
    // a newline before the value.  The logic is a bit more subtle than that,
    // because an ANY-PATH! could not be LOADed back if this were allowed.
    // The bit is set initially by what the scanner detects, and then left
    // to the user's control after that.
    //
    // !!! The native `new-line` is used set this, which has a somewhat
    // poor name considering its similarity to `newline` the line feed char.
    //
    VALUE_FLAG_LINE = 1 << (GENERAL_VALUE_BIT + 1),

    // `VALUE_FLAG_THROWN`
    //
    // The thrown bit is being phased out, as the concept of a value itself
    // being "thrown" does not make a lot of sense, compared to the idea
    // that the evaluator itself is in a "throwing state".  If a thrown bit
    // can get on a value, then one has to worry about that value getting
    // copied and showing up somewhere that it doesn't make sense.
    //
    // Originally, R3-Alpha did not have a thrown bit on values, rather the
    // throw itself was represented as a certain kind of ERROR! value.  Ren-C
    // modifications extended THROW to allow a /NAME that could be a full
    // REBVAL (instead of a selection from a limited set of words).  This
    // made it possible to identify a throw by an object, function,
    // fully bound word, etc.
    //
    // But even after the change, the "thrown-ness" was still a property
    // of the "throw-name REBVAL".  By virtue of being a property on a
    // value *it could be dropped on the floor and ignored*.  There were
    // countless examples of this originating in the code.
    //
    // As part of the process of stamping out the idea that thrownness comes
    // from a value, all routines that can potentially return thrown values
    // have been adapted to return a boolean and adopt the XXX_Throws()
    // naming convention, so one can write:
    //
    //     if (XXX_Throws()) {
    //        /* handling code */
    //     }
    //
    // This forced every caller to consciously have a code path dealing with
    // potentially thrown values, reigning in the previous problems.  At
    // time of writing, the situation is much more under control, and natives
    // return a flag indicating that they wish to throw a value vs. return
    // one.  This is checked redundantly against the value bit for now, but
    // it is likely that the bit will be removed in favor of pushing the
    // responsibility into the evaluator state.
    //
    VALUE_FLAG_THROWN = 1 << (GENERAL_VALUE_BIT + 2),

    // `VALUE_FLAG_RELATIVE` is used to indicate a value that needs to have
    // a specific context added into it before it can have its bits copied
    // or used for some purposes.  An ANY-WORD! is relative if it refers to
    // a local or argument of a function, and has its bits resident in the
    // deep copy of that function's body.  An ANY-ARRAY! in the deep copy
    // of a function body must be relative also to the same function if
    // it contains any instances of such relative words.
    //
    // !!! The feature of specific binding is a work in progress, and only
    // bits of the supporting implementation changes are committed into the
    // master branch at a time.
    //
    VALUE_FLAG_RELATIVE = 1 << (GENERAL_VALUE_BIT + 4),

    // `VALUE_FLAG_EVALUATED` is a somewhat dodgy-yet-unavoidable concept.
    // This is that some functions wish to be sensitive to whether or not
    // their argument came as a literal in source or as a product of an
    // evaluation.  While all values carry the bit, it is only guaranteed
    // to be meaningful on arguments in function frames...though it is
    // valid on any result at the moment of taking it from Do_Core().
    //
    VALUE_FLAG_EVALUATED = 1 << (GENERAL_VALUE_BIT + 5)
};

// VALUE_FLAG_XXX flags are applicable to all types.  Type-specific flags are
// named things like TYPESET_FLAG_XXX or WORD_FLAG_XXX and only apply to the
// type that they reference.  Both use these XXX_VAL_FLAG accessors.
//
#ifdef NDEBUG
    #define SET_VAL_FLAG(v,f) \
        ((v)->header.bits |= (f))

    #define GET_VAL_FLAG(v,f) \
        LOGICAL((v)->header.bits & (f))

    #define CLEAR_VAL_FLAG(v,f) \
        ((v)->header.bits &= ~cast(REBUPT, (f)))
#else
    // For safety in the debug build, all the type-specific flags include
    // their type as part of the flag.  This type is checked first, and then
    // masked out to use the single-bit-flag value which is intended.  The
    // check uses the bits of an exemplar type to identify the category
    // (e.g. REB_FUNCTION for ANY-FUNCTION!, REB_OBJECT for ANY-CONTEXT!)

    #define SET_VAL_FLAG(v,f) \
        (Assert_Flags_Are_For_Value((v), (f)), \
            (v)->header.bits |= ((f) & ~HEADER_TYPE_MASK))

    #define GET_VAL_FLAG(v,f) \
        (Assert_Flags_Are_For_Value((v), (f)), \
            LOGICAL((v)->header.bits & ((f) & ~HEADER_TYPE_MASK)))

    #define CLEAR_VAL_FLAG(v,f) \
        (Assert_Flags_Are_For_Value((v), (f)), \
            (v)->header.bits &= ~cast(REBUPT, (f) & ~HEADER_TYPE_MASK))
#endif

//
// Setting and clearing multiple flags works, so these names make that "clear"
//

#define SET_VAL_FLAGS(v,f) \
    SET_VAL_FLAG((v), (f))

#define CLEAR_VAL_FLAGS(v,f) \
    CLEAR_VAL_FLAG((v), (f))


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE "KIND" (1 out of 64 different foundational types)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Every value has 6 bits reserved for its VAL_TYPE().  The reason only 6
// are used is because low-level TYPESET!s are only 64-bits (so they can fit
// into a REBVAL payload, along with a key symbol to represent a function
// parameter).  If there were more types, they couldn't be flagged in a
// typeset that fit in a REBVAL under that constraint.
//
// VAL_TYPE() should obviously not be called on uninitialized memory.  But
// it should also not be called on an END marker, as those markers only
// guarantee the low bit as having Rebol-readable-meaning.  In debug builds,
// this is asserted by VAL_TYPE_Debug.
//

#ifdef NDEBUG
    #define VAL_TYPE(v) \
        cast(enum Reb_Kind, (v)->header.bits & HEADER_TYPE_MASK)
#else
    #define VAL_TYPE(v) \
        VAL_TYPE_Debug((v), __FILE__, __LINE__)
#endif

// For the processor's convenience, the 64 basic Rebol types are shifted to
// the left by 2 bits.  This makes their range go from 0..251 instead of
// from 0..63.  The choice to do this is because most of the type the types
// are just used in comparisons or switch statements, and it's cheaper to
// not have to shift out the 2 low bits that are used for END and WRITABLE
// flags in the header most of the time.
//
// However, to index into a zero based array with 64 elements, the shift
// needs to be done.  If that's required these defines adjust for the shift.
// See also REB_MAX_0
//
#define KIND_FROM_0(z) cast(enum Reb_Kind, (z) << 2)
#define TO_0_FROM_KIND(k) (cast(REBCNT, (k)) >> 2)
#define VAL_TYPE_0(v) TO_0_FROM_KIND(VAL_TYPE(v))

// SET_TYPE_BITS only sets the type, with other header bits intact.  This
// should be used when you are sure that the new type payload is in sync with
// the type and bits (for instance, changing from one ANY-WORD! type to
// another, the binding needs to be in sync with the header bits)
//
// NOTE: The header MUST already be valid and initialized to use this!  For
// fresh value creation, one wants to use VAL_RESET_HEADER to clear bits and
// set the type.
//
// !!! Is it worth the effort to add a debugging flag into the value to
// disallow calling this routine if VAL_RESET_HEADER has not been run, or
// are there too few instances to be worth it and is _BITS enough a hint?
//
#define VAL_SET_TYPE_BITS(v,t) \
    ((v)->header.bits &= ~cast(REBUPT, HEADER_TYPE_MASK), \
        (v)->header.bits |= cast(REBUPT, (t)))


//=////////////////////////////////////////////////////////////////////////=//
//
//  CELL WRITABILITY AND SETUP
//
//=////////////////////////////////////////////////////////////////////////=//
//
// VAL_RESET_HEADER clears out the header and sets it to a new type (and also
// sets the option bits indicating the value is *not* an END marker, and
// that the value is a full cell which can be written to).
//
// The debug build includes an extra check that the value we are about
// to write the header of is actually a full REBVAL-sized slot...and not
// just an implicit END marker that's really doing double-duty as some
// internal pointer of a container structure.
//
// The debug build also requires that value slots which are to be written
// to via VAL_RESET_HEADER() be marked writable.  Series and other value
// containers do this automatically, but if you make a REBVAL as a stack
// variable then it will have to be done before any write can happen.
//

#define VAL_RESET_HEADER_CORE(v,kind) \
    ((v)->header.bits = NOT_END_MASK | cast(REBUPT, (kind)))

#ifdef NDEBUG
    #define ASSERT_CELL_WRITABLE_IF_DEBUG(v) \
        NOOP

    #define INIT_CELL_WRITABLE_IF_DEBUG(v) \
        NOOP

    #define MARK_CELL_WRITABLE_IF_DEBUG(v) \
        NOOP

    #define MARK_CELL_UNWRITABLE_IF_DEBUG(v) \
        NOOP

    #define VAL_RESET_HEADER(v,t) \
        VAL_RESET_HEADER_CORE((v), (t))
#else
    #ifdef __cplusplus
        #define ASSERT_CELL_WRITABLE_IF_DEBUG(v,file,line) \
            Assert_Cell_Writable((v), (file), (line))

        #define INIT_CELL_WRITABLE_IF_DEBUG(v) \
            ((v)->header.bits = WRITABLE_MASK_DEBUG) // trashes completely

        #define MARK_CELL_WRITABLE_IF_DEBUG(v) \
            ((v)->header.bits |= WRITABLE_MASK_DEBUG) // just adds bit

        #define MARK_CELL_UNWRITABLE_IF_DEBUG(v) \
            ((v)->header.bits &= ~cast(REBUPT, WRITABLE_MASK_DEBUG))
    #else
        #define ASSERT_CELL_WRITABLE_IF_DEBUG(v,file,line) \
            NOOP

        #define INIT_CELL_WRITABLE_IF_DEBUG(v) \
            NOOP

        #define MARK_CELL_WRITABLE_IF_DEBUG(v) \
            NOOP

        #define MARK_CELL_UNWRITABLE_IF_DEBUG(v) \
            NOOP
    #endif

    #define VAL_RESET_HEADER(v,k) \
        VAL_RESET_HEADER_Debug((v), (k), __FILE__, __LINE__)
#endif

// !!! SET_ZEROED is a macro-capture of a dodgy behavior of R3-Alpha,
// which was to assume that clearing the payload of a value and then setting
// the header made it the `zero?` of that type.  Review uses.
//
#define SET_ZEROED(v,t) \
    (VAL_RESET_HEADER((v),(t)), \
        CLEAR(&(v)->payload, sizeof(union Reb_Value_Payload)))


//=////////////////////////////////////////////////////////////////////////=//
//
//  TRACK payload (not a value type, only in DEBUG)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// `struct Reb_Track` is the value payload in debug builds for any REBVAL
// whose VAL_TYPE() doesn't need any information beyond the header.  This
// offers a chance to inject some information into the payload to help
// know where the value originated.  It is used by voids (and void trash),
// BLANK!, LOGIC!, and BAR!.
//
// In addition to the file and line number where the assignment was made,
// the "tick count" of the DO loop is also saved.  This means that it can
// be possible in a repro case to find out which evaluation step produced
// the value--and at what place in the source.  Repro cases can be set to
// break on that tick count, if it is deterministic.
//

// !!! If we're not using TRACK_EMPTY_PAYLOADS, should this POISON_MEMORY()
// on the payload to catch invalid reads?  Trash values don't hang around
// that long, except for the values in the extra "->rest" capacity of series.
// Would that be too many memory poisonings to handle efficiently?
//
#define TRACK_EMPTY_PAYLOADS

#if !defined(NDEBUG)
    #ifdef TRACK_EMPTY_PAYLOADS
        struct Reb_Track {
            const char *filename;
            int line;
            REBCNT count;
        };

        #define SET_TRACK_PAYLOAD(v) \
            ( \
                (v)->payload.track.filename = __FILE__, \
                (v)->payload.track.line = __LINE__, \
                (v)->payload.track.count = TG_Do_Count, \
                NOOP \
            )

        #define VAL_TRACK_FILE(v)       ((v)->payload.track.filename)
        #define VAL_TRACK_LINE(v)       ((v)->payload.track.line)
        #define VAL_TRACK_COUNT(v)      ((v)->payload.track.count)
    #else
        #define SET_TRACK_PAYLOAD(v) NOOP
    #endif
#endif



//=////////////////////////////////////////////////////////////////////////=//
//
//  VOID or TRASH (fits in header bits, may use `struct Reb_Track`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Though there was an "unset! datatype" in Rebol2 and R3-Alpha, Ren-C does
// not allow "reified values of type unset!".  Instead it has voids--which
// are a transient product of evaluation (e.g. the result of `do []`).  The
// bit pattern for the value is also used in the varlists of contexts like
// OBJECT! or FRAME! to denote a variable that is not currently set, but
// it is said that the *variable* is set or unset.
//
// Since they have no data payload, voids in the debug build use Reb_Track
// as their structure to show where-and-when they were assigned.  This is
// helpful because void is the default initialization value for several
// constructs.
//
// In the debug build, there is a special flag that if it is not set, the
// unset is assumed to be "trash".  This is what's used to fill memory that
// in the release build would be uninitialized data.  To prevent it from being
// inspected while it's in an invalid state, VAL_TYPE used on a trash value
// will assert in the debug build.
//
// IS_TRASH_DEBUG() can be used to test for trash, but in debug builds only.
// The macros for setting trash will compile in both debug and release builds,
// though an unsafe trash will be a NOOP in release builds.  (So the "trash"
// will be uninitialized memory, in that case.)  A safe trash set turns into
// a regular unset in release builds.
//

#ifdef NDEBUG
    #define SET_VOID(v) \
        VAL_RESET_HEADER((v), REB_0)

    #define SET_TRASH_IF_DEBUG(v) NOOP

    #define SET_TRASH_SAFE(v) SET_VOID(v)

    #define SINK(v) cast(REBVAL*, (v))
#else
    enum {
        VOID_FLAG_NOT_TRASH = (1 << TYPE_SPECIFIC_BIT) | REB_0,

        // By default, the garbage collector will alert if a "trash unset"
        // is not overwritten by the time it sees it.  But some cases work with
        // GC-visible locations and want the GC to ignore a transitional trash.
        // For these cases use SET_TRASH_GC_SAFE().
        //
        VOID_FLAG_SAFE_TRASH = (2 << TYPE_SPECIFIC_BIT) | REB_0
    };

    #define SET_VOID(v) \
        (VAL_RESET_HEADER((v), REB_0), \
        SET_VAL_FLAG((v), VOID_FLAG_NOT_TRASH), \
        SET_TRACK_PAYLOAD(v))

    // Special type check...we don't want to use VAL_TYPE() here directly
    // because VAL_TYPE is supposed to assert on trash
    //
    #define IS_TRASH_DEBUG(v) \
        (((v)->header.bits & HEADER_TYPE_MASK) == REB_0 \
        && !(((v)->header.bits & VOID_FLAG_NOT_TRASH)))

    #define SET_TRASH_IF_DEBUG(v) \
        ( \
            VAL_RESET_HEADER((v), REB_0), /* don't set NOT_TRASH flag */ \
            SET_TRACK_PAYLOAD(v) \
        )

    #define SET_TRASH_SAFE(v) \
        ( \
            VAL_RESET_HEADER((v), REB_0), \
            SET_VAL_FLAG((v), VOID_FLAG_SAFE_TRASH), \
            SET_TRACK_PAYLOAD(v) \
        )

    #define SINK(v) \
        Sink_Debug((v), __FILE__, __LINE__)
#endif

// To help avoid confusion which might suggest there is a VOID! datatype, the
// name of the REB_XXX type is REB_0, and the test is special.
//
#define IS_VOID(v) LOGICAL(VAL_TYPE(v) == REB_0)

// Pointer to a global protected void cell that can be used when a read-only
// "absence of value" bit pattern is needed.
//
#define VOID_CELL (&PG_Void_Cell[0])

// The debug build has a concept of "safe trash" which is really just an
// unset that is meant to be overwritten before it is ever read.  Only the
// GC is willing to tolerate them, but they will trigger an alarm if any
// other code sees them (error during VAL_TYPE or IS_XXX)
//
#ifdef NDEBUG
    #define IS_VOID_OR_SAFE_TRASH(v) \
        IS_VOID(v)
#else
    #define IS_VOID_OR_SAFE_TRASH(v) \
        ((IS_TRASH_DEBUG(v) && GET_VAL_FLAG((v), VOID_FLAG_SAFE_TRASH)) \
        || IS_VOID(v))
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  BAR! and LIT-BAR! (fits in header bits, may use `struct Reb_Track`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The "expression barrier" is denoted by a lone vertical bar `|`, and it
// has the special property of being rejected for argument fulfillment, but
// ignored at interstitial positions.  So:
//
//     append [a b c] | [d e f] print "Hello"   ;-- will cause an error
//     append [a b c] [d e f] | print "Hello"   ;-- is legal
//
// This makes it similar to an void in behavior, but given its specialized
// purpose unlikely to conflict as being needed to be passed as an actual
// parameter.  Literal unsets in source are treated differently by the
// evaluator than unsets in variables or coming from the result of a function
// call, so that `append [a b c] something-that-returns-a-bar` is legal.
//
// The other loophole for making barriers is the LIT-BAR!, which can allow
// passing a BAR! by value.  So `append [a b c] '|` would work.
//

#ifdef NDEBUG
    #define SET_BAR(v) \
        VAL_RESET_HEADER((v), REB_BAR)

    #define SET_LIT_BAR(v) \
        VAL_RESET_HEADER((v), REB_LIT_BAR)
#else
    #define SET_BAR(v) \
        (VAL_RESET_HEADER((v), REB_BAR), SET_TRACK_PAYLOAD(v))

    #define SET_LIT_BAR(v) \
        (VAL_RESET_HEADER((v), REB_LIT_BAR), SET_TRACK_PAYLOAD(v))
#endif

#define BAR_VALUE (&PG_Bar_Value[0])


//=////////////////////////////////////////////////////////////////////////=//
//
//  BLANK! (unit type - fits in header bits, may use `struct Reb_Track`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Unlike a void cell, blank values are inactive.  They do not cause errors
// when they are used in situations like the condition of an IF statement.
// Instead they are considered to be false--like the LOGIC! #[false] value.
// So blank is considered to be the other "conditionally false" value.
//
// Only those two values are conditionally false in Rebol, and testing for
// conditional truth and falsehood is frequent.  Hence in addition to its
// type, BLANK! also carries a header bit that can be checked for conditional
// falsehood, to save on needing to separately test the type.
//
// Though having tracking information for a blank is less frequently useful
// than for a void, it's there in debug builds just in case it ever serves a
// purpose, as the REBVAL payload is not being used.
//

#ifdef NDEBUG
    #define SET_BLANK(v) \
        ((v)->header.bits = VALUE_FLAG_FALSE | NOT_END_MASK | REB_BLANK)
#else
    #define SET_BLANK(v) \
        (ASSERT_CELL_WRITABLE_IF_DEBUG((v), __FILE__, __LINE__), \
            (v)->header.bits = VALUE_FLAG_FALSE | \
            NOT_END_MASK | WRITABLE_MASK_DEBUG | REB_BLANK, \
        SET_TRACK_PAYLOAD(v))
#endif

#define BLANK_VALUE (&PG_Blank_Value[0])


//=////////////////////////////////////////////////////////////////////////=//
//
//  LOGIC! (type and value fits in header bits, may use `struct Reb_Track`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A logic can be either true or false.  For purposes of optimization, logical
// falsehood is indicated by one of the value option bits in the header--as
// opposed to in the value payload.  This means it can be tested quickly and
// that a single check can test for both BLANK! and logic false.
//
// Conditional truth and falsehood allows an interpretation where a NONE!
// is a "falsey" value as well as logic false.  Unsets are neither
// conditionally true nor conditionally false, and so debug builds will
// complain if you try to determine which it is.  (It likely means a mistake
// was made in skipping a formal decision-point regarding whether an unset
// should represent an "opt out" or an error.)
//
// Due to no need for payload, it goes ahead and includes a TRACK payload
// in debug builds.
//

#ifdef NDEBUG
    #define SET_TRUE(v) \
        ((v)->header.bits = REB_LOGIC | NOT_END_MASK)

    #define SET_FALSE(v) \
        ((v)->header.bits = REB_LOGIC | NOT_END_MASK \
            | VALUE_FLAG_FALSE)
#else
    #define SET_TRUE(v) \
        (ASSERT_CELL_WRITABLE_IF_DEBUG((v), __FILE__, __LINE__), \
            (v)->header.bits = REB_LOGIC | NOT_END_MASK \
                | WRITABLE_MASK_DEBUG, \
         SET_TRACK_PAYLOAD(v)) // compound

    #define SET_FALSE(v) \
        (ASSERT_CELL_WRITABLE_IF_DEBUG((v), __FILE__, __LINE__), \
            (v)->header.bits = REB_LOGIC | NOT_END_MASK \
            | WRITABLE_MASK_DEBUG | VALUE_FLAG_FALSE, \
         SET_TRACK_PAYLOAD(v))  // compound
#endif

#define SET_LOGIC(v,n)  ((n) ? SET_TRUE(v) : SET_FALSE(v))
#define VAL_LOGIC(v)    NOT(GET_VAL_FLAG((v), VALUE_FLAG_FALSE))

#ifdef NDEBUG
    #define IS_CONDITIONAL_FALSE(v) \
        GET_VAL_FLAG((v), VALUE_FLAG_FALSE)
#else
    // In a debug build, we want to make sure that void cells are never asked
    // about their conditional truth or falsehood; they are neither.
    //
    #define IS_CONDITIONAL_FALSE(v) \
        IS_CONDITIONAL_FALSE_Debug(v)
#endif

#define IS_CONDITIONAL_TRUE(v) NOT(IS_CONDITIONAL_FALSE(v))

#define FALSE_VALUE (&PG_False_Value[0])
#define TRUE_VALUE (&PG_True_Value[0])


//
// Rebol Symbol
//
// !!! Historically Rebol used an unsigned 32-bit integer as a "symbol ID".
// These symbols did not participate in garbage collection and had to be
// looked up in a table to get their values.  Ren-C is moving toward adapting
// REBSERs to be able to store words and canon words, as well as GC them.
// This starts moving the types to be the size of a platform pointer.
//

typedef REBUPT REBSYM;


/***********************************************************************
**
**  DATATYPE - Datatype or pseudo-datatype
**
**  !!! Consider rename to TYPE! once legacy TYPE? calls have been
**  converted to TYPE-OF.  Also consider a model where there are
**  user types, and hence TYPE? may be able to return more than just
**  one out of a set of 64 things.
**
***********************************************************************/

struct Reb_Datatype {
    enum Reb_Kind kind;
    REBARR  *spec;
//  REBINT  min_type;
//  REBINT  max_type;
};

#define VAL_TYPE_KIND(v)    ((v)->payload.datatype.kind)
#define VAL_TYPE_KIND_0(v)  TO_0_FROM_KIND(VAL_TYPE_KIND(v))

#define VAL_TYPE_SPEC(v)    ((v)->payload.datatype.spec)

// %words.r is arranged so that symbols for types are at the start
// Although REB_0 is 0 and the 0 REBCNT used for symbol IDs is reserved
// for "no symbol"...this is okay, because void is not a value type and
// should not have a symbol.
//
#define IS_KIND_SYM(s)      ((s) < REB_MAX_0)
#define KIND_FROM_SYM(s)    cast(enum Reb_Kind, KIND_FROM_0(s))
#define SYM_FROM_KIND(k) \
    cast(REBSYM, TO_0_FROM_KIND(k))
#define VAL_TYPE_SYM(v)     SYM_FROM_KIND((v)->payload.datatype.kind)

//#define   VAL_MIN_TYPE(v) ((v)->payload.datatype.min_type)
//#define   VAL_MAX_TYPE(v) ((v)->payload.datatype.max_type)


/***********************************************************************
**
**  NUMBERS - Integer and other simple scalars
**
***********************************************************************/

struct Reb_Integer {
    //
    // On 32-bit platforms, this payload structure begins after a 32-bit
    // header...hence not a 64-bit aligned location.  Since a REBUPT is
    // 32-bits on 32-bit platforms and 64-bit on 64-bit, putting one here
    // right after the header ensures `value` will be on a 64-bit boundary.
    //
    // (At time of writing, this is necessary for the "C-to-Javascript"
    // emscripten build to work.  It's also likely preferred by x86.)
    //
    REBUPT padding;

    REBI64 i64;
};

// !!! Add checking
//
#define VAL_INT32(v)    cast(REBINT, (v)->payload.integer.i64)
#define VAL_UNT32(v)    cast(REBCNT, (v)->payload.integer.i64)

#ifdef NDEBUG
    #define VAL_INT64(v)    ((v)->payload.integer.i64)
#else
    #define VAL_INT64(v)    (*VAL_INT64_Ptr_Debug(v))
#endif

#define SET_INTEGER(v,n) \
    (VAL_RESET_HEADER(v, REB_INTEGER), (v)->payload.integer.i64 = (n))

#define MAX_CHAR        0xffff
#define VAL_CHAR(v)     ((v)->payload.character)
#define SET_CHAR(v,n) \
    (VAL_RESET_HEADER((v), REB_CHAR), VAL_CHAR(v) = (n), NOOP)

#define IS_NUMBER(v) \
    (VAL_TYPE(v) == REB_INTEGER || VAL_TYPE(v) == REB_DECIMAL)


/***********************************************************************
**
**  DECIMAL -- Implementation-wise, a 'double'-precision floating
**  point number in C (typically 64-bit).
**
***********************************************************************/

struct Reb_Decimal {
    //
    // See notes on Reb_Integer for why this is needed (handles case of when
    // `value` is 64-bit, and the platform is 32-bit.)
    //
    REBUPT padding;

    REBDEC dec;
};

#ifdef NDEBUG
    #define VAL_DECIMAL(v)  ((v)->payload.decimal.dec)
#else
    #define VAL_DECIMAL(v)  (*VAL_DECIMAL_Ptr_Debug(v))
#endif

// !!! Several parts of the code want to access the decimal as "bits", where
// those bits are cast as a 64-bit integer.  This uses casting, which is bad,
// but even worse was that it used to use the disengaged integer state of
// the union.  (so it was calling VAL_INTEGER() on a MONEY! or a DECIMAL!).
// This at least makes it clear what's happening.
//
#ifdef NDEBUG
    #define VAL_DECIMAL_BITS(v) (*cast(REBI64*, &(v)->payload.decimal.dec))
#else
    #define VAL_DECIMAL_BITS(v) \
        (*cast(REBI64*, VAL_DECIMAL_Ptr_Debug(v)))
#endif

#define SET_DECIMAL(v,n) \
    (VAL_RESET_HEADER(v, REB_DECIMAL), (v)->payload.decimal.dec = (n))

#define SET_PERCENT(v,n) \
    (VAL_RESET_HEADER(v, REB_PERCENT), (v)->payload.decimal.dec = (n))


/***********************************************************************
**
**  MONEY -- Includes denomination and amount
**
**  !!! The naming of "deci" used by MONEY! as "decimal" is a very
**  bad overlap with DECIMAL! and also not very descriptive of what
**  the properties of a "deci" are.  Also, to be a useful money
**  abstraction it should store the currency type, e.g. the three
**  character ISO 4217 code (~15 bits to store)
**
**      https://en.wikipedia.org/wiki/ISO_4217
**
***********************************************************************/

struct Reb_Money {
    deci amount;
};

#define VAL_MONEY_AMOUNT(v)     ((v)->payload.money.amount)
#define SET_MONEY_AMOUNT(v,n) \
    (VAL_RESET_HEADER((v), REB_MONEY), VAL_MONEY_AMOUNT(v) = (n), NOOP)


/***********************************************************************
**
**  DATE and TIME
**
***********************************************************************/

typedef struct reb_ymdz {
#ifdef ENDIAN_LITTLE
    REBINT zone:7;  // +/-15:00 res: 0:15
    REBCNT day:5;
    REBCNT month:4;
    REBCNT year:16;
#else
    REBCNT year:16;
    REBCNT month:4;
    REBCNT day:5;
    REBINT zone:7;  // +/-15:00 res: 0:15
#endif
} REBYMD;

typedef union reb_date {
    REBYMD date;
    REBCNT bits;
} REBDAT;

struct Reb_Time {
    REBI64 time;    // time in nanoseconds
    REBDAT date;
};

#define VAL_TIME(v) ((v)->payload.time.time)
#define TIME_SEC(n) ((REBI64)(n) * 1000000000L)

#define MAX_SECONDS (((i64)1<<31)-1)
#define MAX_HOUR    (MAX_SECONDS / 3600)
#define MAX_TIME    ((REBI64)MAX_HOUR * HR_SEC)

#define NANO        1.0e-9
#define SEC_SEC     ((REBI64)1000000000L)
#define MIN_SEC     (60 * SEC_SEC)
#define HR_SEC      (60 * 60 * SEC_SEC)

#define SEC_TIME(n)  ((n) * SEC_SEC)
#define MIN_TIME(n)  ((n) * MIN_SEC)
#define HOUR_TIME(n) ((n) * HR_SEC)

#define SECS_IN(n) ((n) / SEC_SEC)
#define VAL_SECS(n) (VAL_TIME(n) / SEC_SEC)

#define DEC_TO_SECS(n) (i64)(((n) + 5.0e-10) * SEC_SEC)

#define SECS_IN_DAY 86400
#define TIME_IN_DAY (SEC_TIME((i64)SECS_IN_DAY))

#define NO_TIME     MIN_I64

#define MAX_YEAR        0x3fff

#define VAL_DATE(v)     ((v)->payload.time.date)
#define VAL_YEAR(v)     ((v)->payload.time.date.date.year)
#define VAL_MONTH(v)    ((v)->payload.time.date.date.month)
#define VAL_DAY(v)      ((v)->payload.time.date.date.day)
#define VAL_ZONE(v)     ((v)->payload.time.date.date.zone)

#define ZONE_MINS 15
#define ZONE_SECS (ZONE_MINS*60)
#define MAX_ZONE (15 * (60/ZONE_MINS))

#define SET_TIME(v, t) \
    (VAL_RESET_HEADER(v, REB_TIME), VAL_TIME(v)=(t))

/***********************************************************************
**
**  TUPLE
**
***********************************************************************/

typedef struct Reb_Tuple {
    REBYTE tuple[12];
} REBTUP;

#define VAL_TUPLE(v)        ((v)->payload.tuple.tuple + 1)
#define VAL_TUPLE_LEN(v)    ((v)->payload.tuple.tuple[0])
#define VAL_TUPLE_DATA(v)   ((v)->payload.tuple.tuple)
#define MAX_TUPLE 10
#define SET_TUPLE(v, t) \
    (VAL_RESET_HEADER(v, REB_TIME), \
    memcpy(VAL_TUPLE_DATA(v), t, sizeof(VAL_TUPLE_DATA(v))))



/***********************************************************************
**
**  PAIR
**
***********************************************************************/

#define VAL_PAIR(v)     ((v)->payload.pair)
#define VAL_PAIR_X(v)   ((v)->payload.pair.x)
#define VAL_PAIR_Y(v)   ((v)->payload.pair.y)
#define VAL_PAIR_X_INT(v) ROUND_TO_INT((v)->payload.pair.x)
#define VAL_PAIR_Y_INT(v) ROUND_TO_INT((v)->payload.pair.y)

#define SET_PAIR(v,x,y) \
    (VAL_RESET_HEADER(v, REB_PAIR),VAL_PAIR_X(v)=(x),VAL_PAIR_Y(v)=(y))


/****************************************************************************
**
**  ANY-SERIES!
**
*****************************************************************************/

union Reb_Binding_Target {
    REBFUN *relative; // for VALUE_FLAG_RELATIVE
    REBCTX *specific; // for !VALUE_FLAG_RELATIVE
};

#define IS_RELATIVE(v) \
    GET_VAL_FLAG((v), VALUE_FLAG_RELATIVE)

#define IS_SPECIFIC(v) \
    NOT(IS_RELATIVE(v))

//
// Only read VAL_RELATIVE and VAL_SPECIFIC through the generic `any_target`.
// Write via the main payload types (see the notes in Reb_Value_Payload)
//

#define VAL_RELATIVE(v) \
    (assert(IS_RELATIVE(v)), \
        (v)->payload.any_target.relative)

#ifdef NDEBUG
    #define VAL_SPECIFIC(v)     ((v)->payload.any_target.specific)
#else
    #define VAL_SPECIFIC(v)     VAL_SPECIFIC_Debug(v)
#endif


struct Reb_Any_Series {
    //
    // `specifier` is used in ANY-ARRAY! payloads.  It is a pointer to a FRAME!
    // context which indicates where relatively-bound ANY-WORD! values which
    // are in the series data can be looked up to get their variable values.
    // If the array does not contain any relatively bound words then it is
    // okay for this to be NULL.
    //
    union Reb_Binding_Target target;

    // `series` represents the actual physical underlying data, which is
    // essentially a vector of equal-sized items.  The length of the item
    // (the series "width") is kept within the REBSER abstraction.  See the
    // file %sys-series.h for notes.
    //
    REBSER *series;

    // `index` is the 0-based position into the series represented by this
    // ANY-VALUE! (so if it is 0 then that means a Rebol index of 1).
    //
    // It is possible that the index could be to a point beyond the range of
    // the series.  This is intrinsic, because the series can be modified
    // through other values and not update the others referring to it.  Hence
    // VAL_INDEX() must be checked, or the routine called with it must.
    //
    // !!! Review that it doesn't seem like these checks are being done
    // in a systemic way.  VAL_LEN_AT() bounds the length at the index
    // position by the physical length, but VAL_ARRAY_AT() doesn't check.
    //
    REBCNT index;
};

#define VAL_SPECIFIER(v) \
    (assert(ANY_ARRAY(v)), VAL_SPECIFIC(v))

#define INIT_ARRAY_SPECIFIC(v,context) \
    (assert(IS_SPECIFIC(v)), \
        (v)->payload.any_series.target.specific = (context))

#define INIT_ARRAY_RELATIVE(v,func) \
    (assert(IS_RELATIVE(v)), \
        (v)->payload.any_series.target.relative = (func))

#ifdef NDEBUG
    #define VAL_SERIES(v)   ((v)->payload.any_series.series)
#else
    #define VAL_SERIES(v)   VAL_SERIES_Debug(v)
#endif

#define INIT_VAL_SERIES(v,s) \
    (assert(!Is_Array_Series(s)), (v)->payload.any_series.series = (s))

#define INIT_VAL_ARRAY(v,s) \
    ((v)->payload.any_series.target.specific = SPECIFIED, \
    (v)->payload.any_series.series = ARR_SERIES(s))

#define VAL_INDEX(v)        ((v)->payload.any_series.index)
#define VAL_LEN_HEAD(v)     SER_LEN(VAL_SERIES(v))
#define VAL_LEN_AT(v)       (Val_Series_Len_At(v))

#define IS_EMPTY(v)         (VAL_INDEX(v) >= VAL_LEN_HEAD(v))

#define VAL_RAW_DATA_AT(v) \
    SER_AT_RAW(SER_WIDE(VAL_SERIES(v)), VAL_SERIES(v), VAL_INDEX(v))

// Ren-C introduced const REBVAL* usage, but propagating const vs non
// const REBSER pointers didn't show enough benefit to be worth the
// work in supporting them (at this time).  Mutability cast needed.
//
#define VAL_MAP(v) \
    (assert(IS_MAP(v)), \
        AS_MAP(m_cast(REBSER*, (v)->payload.any_series.series)))

// Note: These macros represent things that used to sometimes be functions,
// and sometimes were not.  They could be done without a function call, but
// that would then make them unsafe to use with side-effects:
//
//     Val_Init_Block(Alloc_Tail_Array(parent), child);
//
// The repetitition of the value parameter would lead to the allocation
// running multiple times.  Hence we Caps_Words_With_Underscore to name
// these macros to indicate they are safe by not duplicating their args.
// If erring on the side of caution and making a function call turns out
// to be a problem in profiling, then on a case-by-case basis those
// bottlenecks can be replaced with something more like:
//
//     VAL_RESET_HEADER(value, REB_XXX);
//     ENSURE_SERIES_MANAGED(series);
//     VAL_SERIES(value) = series;
//     VAL_INDEX(value) = index;
//
// (Or perhaps just use proper inlining and support it in those builds.)

#define Val_Init_Series_Index(v,t,s,i) \
    Val_Init_Series_Index_Core(SINK(v), (t), (s), (i), SPECIFIED)

#define Val_Init_Series(v,t,s) \
    Val_Init_Series_Index((v), (t), (s), 0)


/***********************************************************************
**
**  BINARY! (uses `struct Reb_Any_Series`)
**
***********************************************************************/

#define VAL_BIN(v)              BIN_HEAD(VAL_SERIES(v))
#define VAL_BIN_HEAD(v)         BIN_HEAD(VAL_SERIES(v))
#define VAL_BIN_AT(v)           BIN_AT(VAL_SERIES(v), VAL_INDEX(v))
#define VAL_BIN_TAIL(v)         BIN_AT(VAL_SERIES(v), VAL_SERIES(v)->tail)

// !!! RE: VAL_BIN_AT_HEAD() see remarks on VAL_ARRAY_AT_HEAD()
//
#define VAL_BIN_AT_HEAD(v,n)    BIN_AT(VAL_SERIES(v), (n))

#define VAL_BYTE_SIZE(v) (BYTE_SIZE(VAL_SERIES(v)))

#define Val_Init_Binary(v,s) \
    Val_Init_Series((v), REB_BINARY, (s))


/***********************************************************************
**
**  ANY-STRING! (uses `struct Reb_Any_Series`)
**
***********************************************************************/

#define VAL_STR_IS_ASCII(v) \
    (VAL_BYTE_SIZE(v) && All_Bytes_ASCII(VAL_BIN_AT(v), VAL_LEN_AT(v)))

#define Val_Init_String(v,s) \
    Val_Init_Series((v), REB_STRING, (s))

#define Val_Init_File(v,s) \
    Val_Init_Series((v), REB_FILE, (s))

#define Val_Init_Tag(v,s) \
    Val_Init_Series((v), REB_TAG, (s))

// Arg is a unicode value:
#define VAL_UNI(v)      UNI_HEAD(VAL_SERIES(v))
#define VAL_UNI_HEAD(v) UNI_HEAD(VAL_SERIES(v))
#define VAL_UNI_AT(v)   UNI_AT(VAL_SERIES(v), VAL_INDEX(v))

#define VAL_ANY_CHAR(v) GET_ANY_CHAR(VAL_SERIES(v), VAL_INDEX(v))


/***********************************************************************
**
**  ANY-ARRAY! (uses `struct Reb_Any_Series`)
**
***********************************************************************/

// These operations do not need to take the value's index position into
// account; they strictly operate on the array series
//
// Must use `(old_style)cast_here` because we may-or-may-not be casting away
// constness in the process, e.g. a series extracted from a const REBVAL.
//
#define VAL_ARRAY(v) \
    (assert(ANY_ARRAY(v)), AS_ARRAY((v)->payload.any_series.series))

#define VAL_ARRAY_HEAD(v)       ARR_HEAD(VAL_ARRAY(v))
#define VAL_ARRAY_TAIL(v)       ARR_AT(VAL_ARRAY(v), VAL_ARRAY_LEN_AT(v))

// These array operations take the index position into account.  The use
// of the word AT with a missing index is a hint that the index is coming
// from the VAL_INDEX() of the value itself.
//
#define VAL_ARRAY_AT(v)         ARR_AT(VAL_ARRAY(v), VAL_INDEX(v))
#define VAL_ARRAY_LEN_AT(v)     VAL_LEN_AT(v)

// !!! VAL_ARRAY_AT_HEAD() is a leftover from the old definition of
// VAL_ARRAY_AT().  Unlike SKIP in Rebol, this definition did *not* take
// the current index position of the value into account.  It rather extracted
// the array, counted rom the head, and disregarded the index entirely.
//
// The best thing to do with it is probably to rewrite the use cases to
// not need it.  But at least "AT HEAD" helps communicate what the equivalent
// operation in Rebol would be...and you know it's not just giving back the
// head because it's taking an index.  So  it looks weird enough to suggest
// looking here for what the story is.
//
#define VAL_ARRAY_AT_HEAD(v,n) \
    ARR_AT(VAL_ARRAY(v), (n))

#define VAL_TERM_ARRAY(v)       TERM_ARRAY(VAL_ARRAY(v))

#define Val_Init_Array_Index(v,t,a,i) \
    Val_Init_Series_Index((v), (t), ARR_SERIES(a), (i))

#define Val_Init_Array(v,t,a) \
    Val_Init_Array_Index((v), (t), (a), 0)

#define Val_Init_Block_Index(v,a,i) \
    Val_Init_Array_Index((v), REB_BLOCK, (a), (i))

#define Val_Init_Block(v,s) \
    Val_Init_Block_Index((v), (s), 0)

#define EMPTY_BLOCK     ROOT_EMPTY_BLOCK
#define EMPTY_ARRAY     VAL_ARRAY(ROOT_EMPTY_BLOCK)

#define EMPTY_STRING \
    ROOT_EMPTY_STRING

/***********************************************************************
**
**  IMAGES, QUADS - RGBA
**
***********************************************************************/

#define QUAD_LEN(s)         SER_LEN(s)

#define QUAD_HEAD(s)        SER_DATA_RAW(s)
#define QUAD_SKIP(s,n)      (QUAD_HEAD(s) + ((n) * 4))
#define QUAD_TAIL(s)        (QUAD_HEAD(s) + (QUAD_LEN(s) * 4))

#define IMG_SIZE(s)         ((s)->misc.size)
#define IMG_WIDE(s)         ((s)->misc.area.wide)
#define IMG_HIGH(s)         ((s)->misc.area.high)
#define IMG_DATA(s)         SER_DATA_RAW(s)

#define VAL_IMAGE_HEAD(v)   QUAD_HEAD(VAL_SERIES(v))
#define VAL_IMAGE_TAIL(v)   QUAD_SKIP(VAL_SERIES(v), VAL_LEN_HEAD(v))
#define VAL_IMAGE_DATA(v)   QUAD_SKIP(VAL_SERIES(v), VAL_INDEX(v))
#define VAL_IMAGE_BITS(v)   cast(REBCNT*, VAL_IMAGE_HEAD(v))
#define VAL_IMAGE_WIDE(v)   (IMG_WIDE(VAL_SERIES(v)))
#define VAL_IMAGE_HIGH(v)   (IMG_HIGH(VAL_SERIES(v)))
#define VAL_IMAGE_LEN(v)    VAL_LEN_AT(v)

#define Val_Init_Image(v,s) \
    Val_Init_Series((v), REB_IMAGE, (s));

//tuple to image! pixel order bytes
#define TO_PIXEL_TUPLE(t) \
    TO_PIXEL_COLOR(VAL_TUPLE(t)[0], VAL_TUPLE(t)[1], VAL_TUPLE(t)[2], \
        VAL_TUPLE_LEN(t) > 3 ? VAL_TUPLE(t)[3] : 0xff)

//tuple to RGBA bytes
#define TO_COLOR_TUPLE(t) \
    TO_RGBA_COLOR(VAL_TUPLE(t)[0], VAL_TUPLE(t)[1], VAL_TUPLE(t)[2], \
        VAL_TUPLE_LEN(t) > 3 ? VAL_TUPLE(t)[3] : 0xff)


/***********************************************************************
**
**  BIT_SET -- Bit sets
**
***********************************************************************/

#define VAL_BITSET(v)   VAL_SERIES(v)

#define VAL_BIT_DATA(v) VAL_BIN(v)

#define SET_BIT(d,n)    ((d)[(n) >> 3] |= (1 << ((n) & 7)))
#define CLR_BIT(d,n)    ((d)[(n) >> 3] &= ~(1 << ((n) & 7)))
#define IS_BIT(d,n)     LOGICAL((d)[(n) >> 3] & (1 << ((n) & 7)))

#define Val_Init_Bitset(v,s) \
    Val_Init_Series((v), REB_BITSET, (s))


/***********************************************************************
**
**  SYMBOLS -- Used only for symbol tables
**
***********************************************************************/

struct Reb_Symbol {
    REBCNT  canon;  // Index of the canonical (first) word
    REBCNT  alias;  // Index to next alias form
    REBCNT  name;   // Index into PG_Word_Names string
};

// Arg is value:
#define VAL_SYM_NINDEX(v)   ((v)->payload.symbol.name)
#define VAL_SYM_NAME(v)     (BIN_HEAD(PG_Word_Names) + VAL_SYM_NINDEX(v))
#define VAL_SYM_CANON(v)    ((v)->payload.symbol.canon)
#define VAL_SYM_ALIAS(v)    ((v)->payload.symbol.alias)

// Return the CANON value for a symbol number:
#define SYMBOL_TO_CANON(sym) \
    VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, sym))

// Return the CANON value for a word value:
#define WORD_TO_CANON(w) \
    VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, VAL_WORD_SYM(w)))

// Is it the same symbol? Quick check, then canon check:
#define SAME_SYM(s1,s2) \
    ((s1) == (s2) \
    || ( \
        VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, (s1))) \
        == VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, (s2))) \
    ))


/***********************************************************************
**
**  WORDS -- All word related types
**
***********************************************************************/

#ifdef NDEBUG
    #define WORD_FLAG_X 0
#else
    #define WORD_FLAG_X REB_WORD // interpreted to mean ANY-WORD!
#endif

enum {
    // `WORD_FLAG_BOUND` answers whether a word is bound, but it may be
    // relatively bound if `VALUE_FLAG_RELATIVE` is set.  In that case, it
    // does not have a context pointer but rather a function pointer, that
    // must be combined with more information to get the FRAME! where the
    // word should actually be looked up.
    //
    // If VALUE_FLAG_RELATIVE is set, then WORD_FLAG_BOUND must also be set.
    //
    WORD_FLAG_BOUND = (1 << (TYPE_SPECIFIC_BIT + 0)) | WORD_FLAG_X,

    // A special kind of word is used during argument fulfillment to hold
    // a refinement's word on the data stack, augmented with its param
    // and argument location.  This helps fulfill "out-of-order" refinement
    // usages more quickly without having to do two full arglist walks.
    //
    WORD_FLAG_PICKUP = (1 << (TYPE_SPECIFIC_BIT + 1)) | WORD_FLAG_X
};

struct Reb_Binding {
    //
    // The context to look in to find the word's value.  It is valid if the
    // word has been bound, and null otherwise.
    //
    // Note that if the ANY-CONTEXT! to which this word is bound is a FRAME!,
    // then that frame may no longer be on the stack.  Hence the space for
    // the variable is no longer allocated.  Tracking mechanisms must be used
    // to make sure the word can keep track of this fact.
    //
    // !!! Tracking mechanism is currently under development.
    //
    // Also note that the expense involved in doing a lookup to a context
    // that is a FRAME! is greater than one that is not (such as an OBJECT!)
    // This is because in the current implementation, the stack must be
    // walked to find the required frame.
    //
    union Reb_Binding_Target target;

    // Index of word in context (if word is bound, e.g. `context` is not NULL)
    //
    // !!! Intended logic is that if the index is positive, then the word
    // is looked for in the context's pooled memory data pointer.  If the
    // index is negative or 0, then it's assumed to be a stack variable,
    // and looked up in the call's `stackvars` data.
    //
    // But now there are no examples of contexts which have both pooled
    // and stack memory, and the general issue of mapping the numbers has
    // not been solved.  However, both pointers are available to a context
    // so it's awaiting some solution for a reasonably-performing way to
    // do the mapping from [1 2 3 4 5 6] to [-3 -2 -1 0 1 2] (or whatever)
    //
    REBINT index;
};

union Reb_Any_Word_Place {
    struct Reb_Binding binding;

    // The order in which refinements are defined in a function spec may
    // not match the order in which they are mentioned on a path.  As an
    // efficiency trick, a word on the data stack representing a refinement
    // usage request is able to store the pointer to its `param` and `arg`
    // positions, so that they may be returned to after the later-defined
    // refinement has had its chance to take the earlier fulfillments.
    //
    struct {
        const RELVAL *param;
        REBVAL *arg;
    } pickup;
};

struct Reb_Any_Word {
    union Reb_Any_Word_Place place;

    // Index of the word's symbol
    //
    // Note: Future expansion plans are to have symbol entries tracked by
    // pointer and garbage collected, likely as series nodes.  A full pointer
    // sized value is required here.
    //
    REBSYM sym;
};

#define IS_WORD_BOUND(v) \
    GET_VAL_FLAG((v), WORD_FLAG_BOUND)

#define IS_WORD_UNBOUND(v) \
    NOT(IS_WORD_BOUND(v))

#define VAL_WORD_SYM(v) \
    (assert(ANY_WORD(v)), (v)->payload.any_word.sym)
#define INIT_WORD_SYM(v,s) \
    (assert(ANY_WORD(v)), (v)->payload.any_word.sym = (s))

#define VAL_WORD_INDEX(v) \
    (assert(ANY_WORD(v)), (v)->payload.any_word.place.binding.index)

#ifdef NDEBUG
    #define INIT_WORD_INDEX(v,i) \
        ((v)->payload.any_word.place.binding.index = (i))
#else
    #define INIT_WORD_INDEX(v,i) \
        INIT_WORD_INDEX_Debug((v),(i))
#endif

#define VAL_WORD_CONTEXT(v) \
    (assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND)), VAL_SPECIFIC(v))

#define INIT_WORD_CONTEXT(v,context) \
    (assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND) && context != SPECIFIED), \
        ENSURE_SERIES_MANAGED(CTX_SERIES(context)), \
        assert(GET_ARR_FLAG(CTX_KEYLIST(context), SERIES_FLAG_MANAGED)), \
        (v)->payload.any_word.place.binding.target.specific = (context))

#define INIT_WORD_FUNC(v,func) \
    (assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND)), \
        (v)->payload.any_word.place.binding.target.relative = (func))

#define VAL_WORD_FUNC(v) \
    (assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND)), VAL_RELATIVE(v))

#define IS_SAME_WORD(v, n) \
    (IS_WORD(v) && VAL_WORD_CANON(v) == n)

#ifdef NDEBUG
    #define UNBIND_WORD(v) \
        CLEAR_VAL_FLAGS((v), WORD_FLAG_BOUND | VALUE_FLAG_RELATIVE)
#else
    #define UNBIND_WORD(v) \
        (CLEAR_VAL_FLAGS((v), WORD_FLAG_BOUND | VALUE_FLAG_RELATIVE), \
        (v)->payload.any_word.place.binding.index = 0)
#endif

#define VAL_WORD_CANON(v) \
    VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, VAL_WORD_SYM(v)))

#define VAL_WORD_NAME(v) \
    VAL_SYM_NAME(ARR_AT(PG_Word_Table.array, VAL_WORD_SYM(v)))

#define VAL_WORD_NAME_STR(v)    BIN_HEAD(VAL_WORD_NAME(v))

#define Val_Init_Word(out,kind,sym) \
    Val_Init_Word_Core((out), (kind), (sym))


//=////////////////////////////////////////////////////////////////////////=//
//
//  TYPESET! (`struct Reb_Typeset`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A typeset is a collection of up to 63 types, implemented as a bitset.
// The 0th type corresponds to REB_TRASH and can be used to indicate another
// property of the typeset (though no such uses exist yet).
//
// !!! The limit of only being able to hold a set of 63 types is a temporary
// one, as user-defined types will require a different approach.  Hence the
// best way to look at the bitset for built-in types is as an optimization
// for type-checking the common parameter cases.
//
// Though available to the user to manipulate directly as a TYPESET!, REBVALs
// of this category have another use in describing the fields of objects
// ("KEYS") or parameters of function frames ("PARAMS").  When used for that
// purpose, they not only list the legal types...but also hold a symbol for
// naming the field or parameter.
//
// !!! At present, a TYPESET! created with MAKE TYPESET! cannot set the
// internal symbol.  Nor can it set the extended flags, though that might
// someday be allowed with a syntax like:
//
//      make typeset! [<hide> <quote> <protect> string! integer!]
//

enum Reb_Param_Class {
    PARAM_CLASS_0 = 0, // reserve to catch uninitialized cases

    // `PARAM_CLASS_NORMAL` is cued by an ordinary WORD! in the function spec
    // to indicate that you would like that argument to be evaluated normally.
    //
    //     >> foo: function [a] [print [{a is} a]
    //
    //     >> foo 1 + 2
    //     a is 3
    //
    // Special outlier EVAL/ONLY can be used to subvert this:
    //
    //     >> eval/only :foo 1 + 2
    //     a is 1
    //     ** Script error: + operator is missing an argument
    //
    PARAM_CLASS_NORMAL = 0x01,

    // `PARAM_CLASS_HARD_QUOTE` is cued by a GET-WORD! in the function spec
    // dialect.  It indicates that a single value of  content at the callsite
    // should be passed through *literally*, without any evaluation:
    //
    //     >> foo: function [:a] [print [{a is} a]
    //
    //     >> foo 1 + 2
    //     a is 1
    //
    //     >> foo (1 + 2)
    //     a is (1 + 2)
    //
    PARAM_CLASS_HARD_QUOTE = 0x02, // GET-WORD! in spec

    // `PARAM_CLASS_REFINEMENT`
    //
    PARAM_CLASS_REFINEMENT = 0x03,

    // `PARAM_CLASS_LOCAL` is a "pure" local, which will be set to void by
    // argument fulfillment.  It is indicated by a SET-WORD! in the function
    // spec, or by coming after a <local> tag in the function generators.
    //
    // !!! Initially these were indicated with TYPESET_FLAG_HIDDEN.  That
    // would allow the PARAM_CLASS to fit in just two bits (if there were
    // no debug-purpose PARAM_CLASS_0) and free up a scarce typeset flag.
    // But is it the case that hiding and localness should be independent?
    //
    PARAM_CLASS_LOCAL = 0x04,

    // PARAM_CLASS_RETURN acts like a pure local, but is pre-filled with a
    // definitionally-scoped function value that takes 1 arg and returns it.
    //
    PARAM_CLASS_RETURN = 0x05,

    // `PARAM_CLASS_SOFT_QUOTE` is cued by a LIT-WORD! in the function spec
    // dialect.  It quotes with the exception of GROUP!, GET-WORD!, and
    // GET-PATH!...which will be evaluated:
    //
    //     >> foo: function ['a] [print [{a is} a]
    //
    //     >> foo 1 + 2
    //     a is 1
    //
    //     >> foo (1 + 2)
    //     a is 3
    //
    // Although possible to implement soft quoting with hard quoting, it is
    // a convenient way to allow callers to "escape" a quoted context when
    // they need to.
    //
    // Note: Value chosen for PCLASS_ANY_QUOTE_MASK in common with hard quote
    //
    PARAM_CLASS_SOFT_QUOTE = 0x06,

    // `PARAM_CLASS_LEAVE` acts like a pure local, but is pre-filled with a
    // definitionally-scoped function value that takes 0 args and returns void
    //
    PARAM_CLASS_LEAVE = 0x07,

    PARAM_CLASS_MAX
};

#define PCLASS_ANY_QUOTE_MASK = 0x02

#define PCLASS_MASK (cast(REBUPT, 0x07) << TYPE_SPECIFIC_BIT)


#ifdef NDEBUG
    #define TYPESET_FLAG_X 0
#else
    #define TYPESET_FLAG_X REB_TYPESET // interpreted to mean ANY-TYPESET!
#endif

// Option flags used with GET_VAL_FLAG().  These describe properties of
// a value slot when it's constrained to the types in the typeset
//
enum {
    // Can't be changed (set with PROTECT)
    //
    TYPESET_FLAG_LOCKED = (1 << (TYPE_SPECIFIC_BIT + 3)) | TYPESET_FLAG_X,

    // Can't be reflected (set with PROTECT/HIDE) or local in spec as `foo:`
    //
    TYPESET_FLAG_HIDDEN = (1 << (TYPE_SPECIFIC_BIT + 4)) | TYPESET_FLAG_X,

    // Can't be bound to beyond the current bindings.
    //
    // !!! This flag was implied in R3-Alpha by TYPESET_FLAG_HIDDEN.  However,
    // the movement of SELF out of being a hardcoded keyword in the binding
    // machinery made it start to be considered as being a by-product of the
    // generator, and hence a "userspace" word (like definitional return).
    // To avoid disrupting all object instances with a visible SELF, it was
    // made hidden...which worked until a bugfix restored the functionality
    // of checking to not bind to hidden things.  UNBINDABLE is an interim
    // solution to separate the property of bindability from visibility, as
    // the SELF solution shakes out--so that SELF may be hidden but bind.
    //
    TYPESET_FLAG_UNBINDABLE = (1 << (TYPE_SPECIFIC_BIT + 5)) | TYPESET_FLAG_X,

    // !!! <durable> is the working name for the property of a function
    // argument or local to have its data survive after the call is over.
    // Much of the groundwork has been laid to allow this to be specified
    // individually for each argument, but the feature currently is "all
    // or nothing"--and implementation-wise corresponds to what R3-Alpha
    // called CLOSURE!, with the deep-copy-per-call that entails.
    //
    // Hence if this property is applied, it will be applied to *all* of
    // a function's arguments.
    //
    TYPESET_FLAG_DURABLE = (1 << (TYPE_SPECIFIC_BIT + 6)) | TYPESET_FLAG_X,

    // !!! This does not need to be on the typeset necessarily.  See the
    // VARARGS! type for what this is, which is a representation of the
    // capture of an evaluation position. The type will also be checked but
    // the value will not be consumed.
    //
    // Note the important distinction, that a variadic parameter and taking
    // a VARARGS! type are different things.  (A function may accept a
    // variadic number of VARARGS! values, for instance.)
    //
    TYPESET_FLAG_VARIADIC = (1 << (TYPE_SPECIFIC_BIT + 7)) | TYPESET_FLAG_X,

    // !!! In R3-Alpha, there were only 8 type-specific bits...with the
    // remaining bits "reserved for future use".  This goes over the line
    // with a 9th type-specific bit, which may or may not need review.
    // It could just be that more type-specific bits is the future use.

    // Endability is distinct from optional, and it means that a parameter is
    // willing to accept being at the end of the input.  This means either
    // an infix dispatch's left argument is missing (e.g. `do [+ 5]`) or an
    // ordinary argument hit the end (e.g. the trick used for `>> help` when
    // the arity is 1 usually as `>> help foo`)
    //
    TYPESET_FLAG_ENDABLE = (1 << (TYPE_SPECIFIC_BIT + 8)) | TYPESET_FLAG_X,

    // For performance, a cached PROTECTED_OR_LOOKAHEAD or'd flag could make
    // it so that each SET doesn't have to clear out the flag.  See
    // notes on that in variable setting.
    //
    TYPESET_FLAG_LOOKBACK = (1 << (TYPE_SPECIFIC_BIT + 9)) | TYPESET_FLAG_X
};

struct Reb_Typeset {
    REBSYM sym;         // Symbol (if a key of object or function param)

    // Note: `sym` is first so that the value's 32-bit Reb_Flags header plus
    // the 32-bit REBCNT will pad `bits` to a REBU64 alignment boundary

    REBU64 bits;        // One bit for each DATATYPE! (use with FLAGIT_64)
};

// Operations when typeset is done with a bitset (currently all typesets)

#define VAL_TYPESET_BITS(v) ((v)->payload.typeset.bits)

#define TYPE_CHECK(v,n) \
    LOGICAL(VAL_TYPESET_BITS(v) & FLAGIT_KIND(n))

#define TYPE_SET(v,n) \
    ((VAL_TYPESET_BITS(v) |= FLAGIT_KIND(n)), NOOP)

#define EQUAL_TYPESET(v,w) \
    (VAL_TYPESET_BITS(v) == VAL_TYPESET_BITS(w))


// Symbol is SYM_0 unless typeset in object keylist or func paramlist

#define VAL_TYPESET_SYM(v) ((v)->payload.typeset.sym)

#define VAL_TYPESET_CANON(v) \
    VAL_SYM_CANON(ARR_AT(PG_Word_Table.array, VAL_TYPESET_SYM(v)))

// Word number array (used by Bind_Table):
#define WORDS_HEAD(w) \
    SER_HEAD(REBINT, (w))

#define WORDS_LAST(w) \
    (WORDS_HEAD(w) + SER_LEN(w) - 1) // (tail never zero)

// Useful variation of FLAGIT_64 for marking a type, as they now involve
// shifting and possible abstraction vs. simply being 0..63
//
#define FLAGIT_KIND(t)          (cast(REBU64, 1) << TO_0_FROM_KIND(t))


#define VAL_PARAM_CLASS(v) \
    (assert(IS_TYPESET(v)), cast(enum Reb_Param_Class, \
        ((v)->header.bits & PCLASS_MASK) >> TYPE_SPECIFIC_BIT))

#define INIT_VAL_PARAM_CLASS(v,c) \
    ((v)->header.bits &= ~PCLASS_MASK, \
    (v)->header.bits |= ((c) << TYPE_SPECIFIC_BIT))


//=////////////////////////////////////////////////////////////////////////=//
//
// ANY-CONTEXT! (`struct Reb_Any_Context`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The Reb_Any_Context is the basic struct used currently for OBJECT!,
// MODULE!, ERROR!, and PORT!.  It builds upon the context datatype REBCTX,
// which permits the storage of associated KEYS and VARS.  (See the comments
// on `struct Reb_Context` that are in %sys-series.h).
//
// Contexts coordinate with words, which can have their VAL_WORD_CONTEXT()
// set to a context's series pointer.  Then they cache the index of that
// word's symbol in the context's keylist, for a fast lookup to get to the
// corresponding var.  The key is a typeset which has several EXT flags
// controlling behaviors like whether the var is protected or hidden.
//
// !!! This "caching" mechanism is not actually "just a cache".  Once bound
// the index is treated as permanent.  This is why objects are "append only"
// because disruption of the index numbers would break the extant words
// with index numbers to that position.  Ren-C intends to undo this by
// paying for the check of the symbol number at the time of lookup, and if
// it does not match consider it a cache miss and re-lookup...adjusting the
// index inside of the word.  For efficiency, some objects could be marked
// as not having this property, but it may be just as efficient to check
// the symbol match as that bit.
//
// Frame key/var indices start at one, and they leave two REBVAL slots open
// in the 0 spot for other uses.  With an ANY-CONTEXT!, the use for the
// "ROOTVAR" is to store a canon value image of the ANY-CONTEXT!'s REBVAL
// itself.  This trick allows a single REBSER* to be passed around rather
// than the REBVAL struct which is 4x larger, yet still reconstitute the
// entire REBVAL if it is needed.
//

struct Reb_Any_Context {
    //
    // Unless it copies the keylist, a context cannot uniquely describe a
    // "special" instance of an archetype function it is bound to.  This
    // field needs to be combined with the FUNC_VALUE of a frame context
    // to get the full definition.
    //
    REBARR *exit_from;

    // `varlist` is a Rebol Array that from 1..NUM_VARS contains REBVALs
    // representing the stored values in the context.
    //
    // As with the `paramlist` of a FUNCTION!, the varlist uses the [0]th
    // element specially.  It stores a copy of the ANY-CONTEXT! value that
    // refers to itself.
    //
    // The `keylist` is held in the varlist's Reb_Series.misc field, and it
    // may be shared with an arbitrary number of other contexts.  Changing
    // the keylist involves making a copy if it is shared.
    //
    // REB_MODULE depends on a property stored in the "meta" miscellaneous
    // field of the keylist, which is another object's-worth of data *about*
    // the module's contents (e.g. the processed header)
    //
    REBCTX *context; // !!! TBD: change to REBARR*, rename as varlist

    union {
        // Used by REB_FRAME.  This is the call that corresponded to the
        // FRAME! at the time it was created.  The pointer becomes invalid if
        // the call ends, so the flags on the context must be consulted to
        // see if it indicates the stack is over before using.
        //
        // Note: This is technically just a cache, as the stack could be
        // walked to find it given the frame.
        //
        struct Reb_Frame *frame;
    } more;
};

#define VAL_CONTEXT(v) \
    (assert(ANY_CONTEXT(v)), (v)->payload.any_context.context)

#define INIT_VAL_CONTEXT(v,c) \
    ((v)->payload.any_context.context = (c))

#define VAL_CONTEXT_META(v) \
    (ARR_SERIES(CTX_KEYLIST((v)->payload.any_context.context))->misc.meta)

#define VAL_CONTEXT_EXIT_FROM(v) \
    ((v)->payload.any_context.exit_from)

#define VAL_CONTEXT_STACKVARS(v) \
    cast(REBVAL*, (v)->payload.any_context.more.frame->stackvars)

#define VAL_CONTEXT_STACKVARS_LEN(v) \
    (assert(ANY_CONTEXT(v)), \
        CHUNK_LEN_FROM_VALUES(VAL_CONTEXT_STACKVARS(v)))

#define VAL_CONTEXT_FRAME(v)        ((v)->payload.any_context.more.frame)

// Convenience macros to speak in terms of object values instead of the context
//
#define VAL_CONTEXT_VAR(v,n)        CTX_VAR(VAL_CONTEXT(v), (n))
#define VAL_CONTEXT_KEY(v,n)        CTX_KEY(VAL_CONTEXT(v), (n))
#define VAL_CONTEXT_KEY_SYM(v,n)    CTX_KEY_SYM(VAL_CONTEXT(v), (n))

// The movement of the SELF word into the domain of the object generators
// means that an object may wind up having a hidden SELF key (and it may not).
// Ultimately this key may well occur at any position.  While user code is
// discouraged from accessing object members by integer index (`pick obj 1`
// is an error), system code has historically relied upon this.
//
// During a transitional period where all MAKE OBJECT! constructs have a
// "real" SELF key/var in the first position, there needs to be an adjustment
// to the indexing of some of this system code.  Some of these will be
// temporary, because not all objects will need a definitional SELF (just as
// not all functions need a definitional RETURN).  Exactly which require it
// and which do not remains to be seen, so this macro helps review the + 1
// more easily than if it were left as just + 1.
//
#define SELFISH(n) (n + 1)

#define Val_Init_Context(out,kind,context) \
    Val_Init_Context_Core(SINK(out), (kind), (context))

#define Val_Init_Object(v,c) \
    Val_Init_Context((v), REB_OBJECT, (c))


/***********************************************************************
**
**  PORTS - External series interface
**
***********************************************************************/

#define Val_Init_Port(v,c) \
    Val_Init_Context((v), REB_PORT, (c))


//=////////////////////////////////////////////////////////////////////////=//
//
// ERROR! (uses `struct Reb_Any_Context`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Errors are a subtype of ANY-CONTEXT! which follow a standard layout.
// That layout is in %boot/sysobj.r as standard/error.
//
// Historically errors could have a maximum of 3 arguments, with the fixed
// names of `arg1`, `arg2`, and `arg3`.  They would also have a numeric code
// which would be used to look up a a formatting block, which would contain
// a block for a message with spots showing where the args were to be inserted
// into a message.  These message templates can be found in %boot/errors.r
//
// Ren-C is exploring the customization of user errors to be able to provide
// arbitrary named arguments and message templates to use them.  It is
// a work in progress, but refer to the FAIL native, the corresponding
// `fail()` C macro inside the source, and the various routines in %c-error.c
//

#define ERR_VARS(e)     cast(ERROR_VARS*, CTX_VARS_HEAD(e))
#define ERR_NUM(e)      cast(REBCNT, VAL_INT32(&ERR_VARS(e)->code))

#define VAL_ERR_VARS(v)     ERR_VARS(VAL_CONTEXT(v))
#define VAL_ERR_NUM(v)      ERR_NUM(VAL_CONTEXT(v))

#define Val_Init_Error(v,c) \
    Val_Init_Context((v), REB_ERROR, (c))


/***********************************************************************
**
**  FUNCTIONS - Natives, actions, operators, and user functions
**
**  NOTE: make-headers.r will skip specs with the "REBNATIVE(" in them
**  REBTYPE macros are used and expanded in tmp-funcs.h
**
***********************************************************************/

#ifdef NDEBUG
    #define FUNC_FLAG_X 0
#else
    #define FUNC_FLAG_X REB_FUNCTION // interpreted to mean ANY-FUNCTION!
#endif

enum {
    // RETURN will always be in the last paramlist slot (if present)
    //
    FUNC_FLAG_RETURN = (1 << (TYPE_SPECIFIC_BIT + 0)) | FUNC_FLAG_X,

    // LEAVE will always be in the last paramlist slot (if present)
    //
    FUNC_FLAG_LEAVE = (1 << (TYPE_SPECIFIC_BIT + 1)) | FUNC_FLAG_X,

    // A function may act as a barrier on its left (so that it cannot act
    // as an input argument to another function).
    //
    // Given the "greedy" nature of infix, a function with arguments cannot
    // be stopped from infix consumption on its right--because the arguments
    // would consume them.  Only a function with no arguments is able to
    // trigger an error when used as a left argument.  This is the ability
    // given to lookback 0 arity functions, known as "punctuators".
    //
    FUNC_FLAG_PUNCTUATES = (1 << (TYPE_SPECIFIC_BIT + 2)) | FUNC_FLAG_X,

#if !defined(NDEBUG)
    //
    // TRUE-valued refinements, NONE! for unused args
    //
    FUNC_FLAG_LEGACY = (1 << (TYPE_SPECIFIC_BIT + 3)) | FUNC_FLAG_X,
#endif

    FUNC_FLAG_NO_COMMA // needed for proper comma termination of this list
};


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
typedef REB_R (*REBACT)(struct Reb_Frame *frame_, REBCNT a);
#define REBTYPE(n) \
    REB_R T_##n(struct Reb_Frame *frame_, REBCNT action)

// PORT!-action function
typedef REB_R (*REBPAF)(struct Reb_Frame *frame_, REBCTX *p, REBCNT a);

// COMMAND! function
typedef REB_R (*CMD_FUNC)(REBCNT n, REBSER *args);

typedef struct Reb_Routine_Info REBRIN;

struct Reb_Function {
    //
    // Definitionally-scoped returns introduced the idea of there being a
    // unique property on a per-REBVAL instance for the natives RETURN and
    // LEAVE, in order to identify that instance of the "archetypal" natives
    // relative to a specific frame or function.  This idea has overlap with
    // the Reb_Binding_Target, and may be unified for this common cell slot.
    //
    REBARR *exit_from;

    // `paramlist` is a Rebol Array whose 1..NUM_PARAMS values are all
    // TYPESET! values, with an embedded symbol (a.k.a. a "param") as well
    // as other bits, including the parameter class (PARAM_CLASS).  This
    // is the list that is processed to produce WORDS-OF and which is
    // consulted during invocation to fulfill the arguments.
    //
    // In addition, its [0]th element contains a FUNCTION! value which is
    // self-referentially the function itself.  This means that the paramlist
    // can be passed around as a single pointer from which a whole REBVAL
    // for the function can be found.
    //
    // The `misc` field of the function series is `spec`, which contains data
    // passed to MAKE FUNCTION! to create the `paramlist`.  After the
    // paramlist has been generated, it is not generally processed by the
    // code and remains mostly to be scanned by user mode code to make HELP.
    // As "metadata" it is not kept in the canon value itself so it can be
    // updated or augmented by functions like `redescribe` without worrying
    // about all the Reb_Function REBVALs that are extant.
    //
    REBFUN *func; // !!! TBD: change to REBARR*, rename as paramlist

    // `body` is always a REBARR--even an optimized "singular" one that is
    // only the size of one REBSER.  This is because the information for a
    // function body is an array in the majority of function instances, and
    // also because it can standardize the native dispatcher code in the
    // REBARR's series "misc" field.  This gives two benefits: no need for
    // a switch on the function's type to figure out the dispatcher, and also
    // to move the dispatcher out of the REBVAL itself into something that
    // can be revectored or "hooked" for all instances of the function.
    //
    // PLAIN FUNCTIONS: body array is... the body of the function, obviously
    // NATIVES: body is "equivalent code for native" (if any) in help
    // ACTIONS: body is a 1-element array containing an INTEGER!
    // SPECIALIZATIONS: body is a 1-element array containing a FRAME!
    // CALLBACKS: body is a 1-element array with a HANDLE! (REBRIN*)
    // ROUTINES: body is a 1-element array with a HANDLE! (REBRIN*)
    //
    REBARR *body;
};

/* argument is of type REBVAL* */
#ifdef NDEBUG
    #define VAL_FUNC(v)             ((v)->payload.function.func + 0)
#else
    #define VAL_FUNC(v)             VAL_FUNC_Debug(v)
#endif

#define VAL_FUNC_PARAMLIST(v)       FUNC_PARAMLIST(VAL_FUNC(v))
#define VAL_FUNC_META(v) \
    (ARR_SERIES(FUNC_PARAMLIST((v)->payload.function.func))->misc.meta)

#define VAL_FUNC_NUM_PARAMS(v)      FUNC_NUM_PARAMS(VAL_FUNC(v))
#define VAL_FUNC_PARAMS_HEAD(v)     FUNC_PARAMS_HEAD(VAL_FUNC(v))
#define VAL_FUNC_PARAM(v,p)         FUNC_PARAM(VAL_FUNC(v), (p))

#define VAL_FUNC_DISPATCH(v) \
    cast(REBNAT, ARR_SERIES((v)->payload.function.body)->misc.dispatch)

#define IS_FUNCTION_PLAIN(v) \
    (VAL_FUNC_DISPATCH(v) == &Plain_Dispatcher)

#define IS_FUNCTION_ACTION(v) \
    (VAL_FUNC_DISPATCH(v) == &Action_Dispatcher)

#define IS_FUNCTION_COMMAND(v) \
    (VAL_FUNC_DISPATCH(v) == &Command_Dispatcher)

#define IS_FUNCTION_SPECIALIZER(v) \
    (VAL_FUNC_DISPATCH(v) == &Specializer_Dispatcher)

#define IS_FUNCTION_ADAPTER(v) \
    (VAL_FUNC_DISPATCH(v) == &Adapter_Dispatcher)

#define IS_FUNCTION_CHAINER(v) \
    (VAL_FUNC_DISPATCH(v) == &Chainer_Dispatcher)

#define IS_FUNCTION_RIN(v) \
    (VAL_FUNC_DISPATCH(v) == &Routine_Dispatcher)

#define IS_FUNCTION_HIJACKER(v) \
    (VAL_FUNC_DISPATCH(v) == &Hijacker_Dispatcher)

#define VAL_FUNC_BODY(v) \
    (ARR_HEAD((v)->payload.function.body) + 0)

#define VAL_FUNC_ACT(v) \
    cast(REBCNT, VAL_INT32(VAL_FUNC_BODY(v)))

#define VAL_FUNC_INFO(v) \
    cast(REBRIN*, VAL_HANDLE_DATA(VAL_FUNC_BODY(v)))

#define VAL_FUNC_EXEMPLAR(v) \
    KNOWN(VAL_FUNC_BODY(v))

#define VAL_FUNC_EXIT_FROM(v) ((v)->payload.function.exit_from)

// !!! At the moment functions are "all durable" or "none durable" w.r.t. the
// survival of their arguments and locals after the call.  This corresponds
// to the CLOSURE! and FUNCTION! distinction for the moment, and brings
// about two things: specific binding *and* durability.
//
#define IS_FUNC_DURABLE(f) \
    LOGICAL(VAL_FUNC_NUM_PARAMS(f) != 0 \
        && GET_VAL_FLAG(VAL_FUNC_PARAM((f), 1), TYPESET_FLAG_DURABLE))

// Native values are stored in an array at boot time.  This is a convenience
// accessor for getting the "FUNC" portion of the native--e.g. the paramlist.
// It should compile to be as efficient as fetching any global pointer.

#define NAT_VALUE(name) \
    (&Natives[N_##name##_ID])

#define NAT_FUNC(name) \
    (NAT_VALUE(name)->payload.function.func)


//=////////////////////////////////////////////////////////////////////////=//
//
// VARARGS! (`struct Reb_Varargs`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A VARARGS! represents a point for parameter gathering inline at the
// callsite of a function.  The point is located *after* that function has
// gathered all of its arguments and started running.  It is implemented by
// holding a reference to a reified FRAME! series, which allows it to find
// the point of a running evaluation (as well as to safely check for when
// that call is no longer on the stack, and can't provide data.)
//
// A second VARARGS! form is implemented as a thin proxy over an ANY-ARRAY!.
// This mimics the interface of feeding forward through those arguments, to
// allow for "parameter packs" that can be passed to variadic functions.
//
// When the bits of a payload of a VARARG! are copied from one item to
// another, they are still maintained in sync.  TAKE-ing a vararg off of one
// is reflected in the others.  This means that the "indexor" position of
// the vararg is located through the frame pointer.  If there is no frame,
// then a single element array (the `array`) holds an ANY-ARRAY! value that
// is shared between the instances, to reflect the state.
//

#ifdef NDEBUG
    #define VARARGS_FLAG_X 0
#else
    #define VARARGS_FLAG_X REB_VARARGS
#endif

enum {
    // Was made with a call to MAKE VARARGS! with data from an ANY-ARRAY!
    // If that is the case, it does not use the varargs payload at all,
    // rather it uses the Reb_Any_Series payload.
    //
    VARARGS_FLAG_NO_FRAME = (1 << (TYPE_SPECIFIC_BIT + 0)) | VARARGS_FLAG_X
};

struct Reb_Varargs {
    //
    // This is a FRAME! which was on the stack at the time when the VARARGS!
    // was created.  However it may no longer be on the stack--and once it is
    // not, it cannot respond to requests to be advanced.  The frame keeps
    // a flag to test for this as long as this VARARGS! is GC-managed.
    //
    // Note that this frame reuses its EVAL slot to hold a value for any
    // "sub-varargs" that is being processed, so that is the first place it
    // looks to get the next item.
    //
    union {
        REBARR *varlist; // might be an in-progress varlist if not managed

        REBARR *array1; // for MAKE VARARGS! to share a reference on an array
    } feed;

    // For as long as the VARARGS! can be used, the function it is applying
    // will be alive.  Assume that the locked paramlist won't move in memory
    // (evaluation would break if so, anyway) and hold onto the TYPESET!
    // describing the parameter.  Each time a value is fetched from the EVAL
    // then type check it for convenience.  Use ANY-VALUE! if not wanted.
    //
    // Note: could be a parameter index in the worst case scenario that the
    // array grew, revisit the rules on holding pointers into paramlists.
    //
    const RELVAL *param;

    // Similar to the param, the arg is only good for the lifetime of the
    // FRAME!...but even less so, because VARARGS! can (currently) be
    // overwritten with another value in the function frame at any point.
    // Despite this, we proxy the VALUE_FLAG_EVALUATED from the last TAKE
    // onto the argument to reflect its *argument* status.
    //
    REBVAL *arg;
};

#define VAL_VARARGS_FRAME_CTX(v) \
    (assert(GET_ARR_FLAG( \
        (v)->payload.varargs.feed.varlist, SERIES_FLAG_MANAGED)), \
    AS_CONTEXT((v)->payload.varargs.feed.varlist))

#define VAL_VARARGS_ARRAY1(v) ((v)->payload.varargs.feed.array1)

#define VAL_VARARGS_PARAM(v) ((v)->payload.varargs.param)
#define VAL_VARARGS_ARG(v) ((v)->payload.varargs.arg)

// The subfeed is either the varlist of the frame of another varargs that is
// being chained at the moment, or the `array1` of another varargs.  To
// be visible for all instances of the same vararg, it can't live in the
// payload bits--so it's in the `eval` slot of a frame or the misc slot
// of the array1.
//
#define SUBFEED_ADDR_OF_FEED(a) \
    (GET_ARR_FLAG((a), ARRAY_FLAG_CONTEXT_VARLIST) \
        ? &CTX_FRAME(AS_CONTEXT(a))->cell.subfeed \
        : &ARR_SERIES(a)->misc.subfeed)


/***********************************************************************
**
**  HANDLE
**
**  Type for holding an arbitrary code or data pointer inside
**  of a Rebol data value.  What kind of function or data is not
**  known to the garbage collector, so it ignores it.
**
**  !!! Review usages of this type where they occur
**
***********************************************************************/

struct Reb_Handle {
    union {
        CFUNC *code;
        void *data;
    } thing;
};

#define VAL_HANDLE_CODE(v) \
    ((v)->payload.handle.thing.code)

#define VAL_HANDLE_DATA(v) \
    ((v)->payload.handle.thing.data)

#define SET_HANDLE_CODE(v,c) \
    (VAL_RESET_HEADER((v), REB_HANDLE), VAL_HANDLE_CODE(v) = (c))

#define SET_HANDLE_DATA(v,d) \
    (VAL_RESET_HEADER((v), REB_HANDLE), VAL_HANDLE_DATA(v) = (d))

// !!! The logic used to be an I32 but now it's folded in as a value flag
// Usages of this should be reviewed.
//
#define VAL_I32(v) ((v)->payload.rebcnt)


/***********************************************************************
**
**  LIBRARY -- External library management structures
**
***********************************************************************/

typedef struct Reb_Library_Handle {
    void * fd;
    REBUPT flags;
} REBLHL; // sizeof(REBLHL) % 8 must equal 0 on both 64-bit and 32-bit builds

struct Reb_Library {
    REBLHL *handle;
    REBARR *spec;
};

#define LIB_FD(v)           ((v)->fd)
#define LIB_FLAGS(v)        ((v)->flags)

#define VAL_LIB(v)          ((v)->payload.library)
#define VAL_LIB_SPEC(v)     ((v)->payload.library.spec)
#define VAL_LIB_HANDLE(v)   ((v)->payload.library.handle)
#define VAL_LIB_FD(v)       ((v)->payload.library.handle->fd)
#define VAL_LIB_FLAGS(v)    ((v)->payload.library.handle->flags)

enum {
    LIB_MARK = 1,       // library was found during GC mark scan.
    LIB_USED = 1 << 1,
    LIB_CLOSED = 1 << 2
};

#define LIB_SET_FLAG(s, f)  (LIB_FLAGS(s) |= (f))
#define LIB_CLR_FLAG(s, f)  (LIB_FLAGS(s) &= ~(f))
#define LIB_GET_FLAG(s, f) (LIB_FLAGS(s) &  (f))

#define MARK_LIB(s)    LIB_SET_FLAG(s, LIB_MARK)
#define UNMARK_LIB(s)  LIB_CLR_FLAG(s, LIB_MARK)
#define IS_MARK_LIB(s) LIB_GET_FLAG(s, LIB_MARK)

#define USE_LIB(s)     LIB_SET_FLAG(s, LIB_USED)
#define UNUSE_LIB(s)   LIB_CLR_FLAG(s, LIB_USED)
#define IS_USED_LIB(s) LIB_GET_FLAG(s, LIB_USED)

#define IS_CLOSED_LIB(s)    LIB_GET_FLAG(s, LIB_CLOSED)
#define CLOSE_LIB(s)        LIB_SET_FLAG(s, LIB_CLOSED)
#define OPEN_LIB(s)         LIB_CLR_FLAG(s, LIB_CLOSED)

/***********************************************************************
**
**  STRUCT -- C Structures
**
***********************************************************************/

typedef struct Reb_Struct {
    REBARR  *spec;
    REBSER  *fields;    // fields definition
    REBSER  *data;
} REBSTU;

#define VAL_STRUCT(v)           ((v)->payload.structure)
#define VAL_STRUCT_SPEC(v)      ((v)->payload.structure.spec)
#define VAL_STRUCT_FIELDS(v)    ((v)->payload.structure.fields)
#define VAL_STRUCT_DATA(v)      ((v)->payload.structure.data)
#define VAL_STRUCT_DP(v)        (BIN_HEAD(VAL_STRUCT_DATA(v)))


/***********************************************************************
**
**  ROUTINE -- External library routine structures
**
***********************************************************************/
struct Reb_Routine_Info {
    union {
        struct {
            REBLHL  *lib;
            CFUNC *funcptr;
        } rot;
        struct {
            void *closure;
            REBFUN *func;
            void *dispatcher;
        } cb;
    } info;
    void    *cif;
    REBSER  *arg_types; /* index 0 is the return type, */
    REBARR  *fixed_args;
    REBARR  *all_args;
    REBARR  *arg_structs; /* for struct arguments */
    REBSER  *extra_mem; /* extra memory that needs to be free'ed */
    REBCNT  flags; // !!! 32-bit...should it use REBFLGS for 64-bit on 64-bit?
    REBINT  abi;

    REBUPT padding; // sizeof(Reb_Routine_Info) % 8 must be 0 for Make_Node()
};

typedef REBFUN REBROT;

enum {
    ROUTINE_MARK = 1,       // routine was found during GC mark scan.
    ROUTINE_USED = 1 << 1,
    ROUTINE_CALLBACK = 1 << 2, //this is a callback
    ROUTINE_VARIADIC = 1 << 3 //this is a FFI function with a va_list interface
};

/* argument is REBFCN */

#define ROUTINE_META(v)             FUNC_META(v)
#define ROUTINE_INFO(v)             FUNC_INFO(v)
#define ROUTINE_PARAMLIST(v)        FUNC_PARAMLIST(v)
#define ROUTINE_FUNCPTR(v)          (ROUTINE_INFO(v)->info.rot.funcptr)
#define ROUTINE_LIB(v)              (ROUTINE_INFO(v)->info.rot.lib)
#define ROUTINE_ABI(v)              (ROUTINE_INFO(v)->abi)
#define ROUTINE_FFI_ARG_TYPES(v)    (ROUTINE_INFO(v)->arg_types)
#define ROUTINE_FIXED_ARGS(v)       (ROUTINE_INFO(v)->fixed_args)
#define ROUTINE_ALL_ARGS(v)         (ROUTINE_INFO(v)->all_args)
#define ROUTINE_FFI_ARG_STRUCTS(v)  (ROUTINE_INFO(v)->arg_structs)
#define ROUTINE_EXTRA_MEM(v)        (ROUTINE_INFO(v)->extra_mem)
#define ROUTINE_CIF(v)              (ROUTINE_INFO(v)->cif)
#define ROUTINE_RVALUE(v)           VAL_STRUCT(ARR_HEAD(ROUTINE_FFI_ARG_STRUCTS(v)))
#define ROUTINE_CLOSURE(v)          (ROUTINE_INFO(v)->info.cb.closure)
#define ROUTINE_DISPATCHER(v)       (ROUTINE_INFO(v)->info.cb.dispatcher)
#define CALLBACK_FUNC(v)            (ROUTINE_INFO(v)->info.cb.func)

/* argument is REBRIN */

#define RIN_FUNCPTR(v)              ((v)->info.rot.funcptr)
#define RIN_LIB(v)                  ((v)->info.rot.lib)
#define RIN_CLOSURE(v)              ((v)->info.cb.closure)
#define RIN_FUNC(v)                 ((v)->info.cb.func)
#define RIN_ARGS_STRUCTS(v)         ((v)->arg_structs)
#define RIN_RVALUE(v)               VAL_STRUCT(ARR_HEAD(RIN_ARGS_STRUCTS(v)))

#define ROUTINE_FLAGS(s)       ((s)->flags)
#define ROUTINE_SET_FLAG(s, f) (ROUTINE_FLAGS(s) |= (f))
#define ROUTINE_CLR_FLAG(s, f) (ROUTINE_FLAGS(s) &= ~(f))
#define ROUTINE_GET_FLAG(s, f) LOGICAL(ROUTINE_FLAGS(s) & (f))

#define IS_CALLBACK_ROUTINE(s) ROUTINE_GET_FLAG(s, ROUTINE_CALLBACK)

/* argument is REBVAL */
#define VAL_ROUTINE(v)              VAL_FUNC(v)
#define VAL_ROUTINE_META(v)         VAL_FUNC_META(v)
#define VAL_ROUTINE_INFO(v)         VAL_FUNC_INFO(v)
#define VAL_ROUTINE_PARAMLIST(v)    VAL_FUNC_PARAMLIST(v)
#define VAL_ROUTINE_FUNCPTR(v)      (VAL_ROUTINE_INFO(v)->info.rot.funcptr)
#define VAL_ROUTINE_LIB(v)          (VAL_ROUTINE_INFO(v)->info.rot.lib)
#define VAL_ROUTINE_ABI(v)          (VAL_ROUTINE_INFO(v)->abi)
#define VAL_ROUTINE_FFI_ARG_TYPES(v)    (VAL_ROUTINE_INFO(v)->arg_types)
#define VAL_ROUTINE_FIXED_ARGS(v)   (VAL_ROUTINE_INFO(v)->fixed_args)
#define VAL_ROUTINE_ALL_ARGS(v)     (VAL_ROUTINE_INFO(v)->all_args)
#define VAL_ROUTINE_FFI_ARG_STRUCTS(v)  (VAL_ROUTINE_INFO(v)->arg_structs)
#define VAL_ROUTINE_EXTRA_MEM(v)    (VAL_ROUTINE_INFO(v)->extra_mem)
#define VAL_ROUTINE_CIF(v)          (VAL_ROUTINE_INFO(v)->cif)

#define VAL_ROUTINE_RVALUE(v) \
    VAL_STRUCT(ARR_HEAD(VAL_ROUTINE_INFO(v)->arg_structs))

#define VAL_ROUTINE_CLOSURE(v)      (VAL_ROUTINE_INFO(v)->info.cb.closure)
#define VAL_ROUTINE_DISPATCHER(v)   (VAL_ROUTINE_INFO(v)->info.cb.dispatcher)
#define VAL_CALLBACK_FUNC(v)        (VAL_ROUTINE_INFO(v)->info.cb.func)


/***********************************************************************
**
**  EVENT
**
***********************************************************************/

#define VAL_EVENT_TYPE(v)   ((v)->payload.event.type)  //(VAL_EVENT_INFO(v) & 0xff)
#define VAL_EVENT_FLAGS(v)  ((v)->payload.event.flags) //((VAL_EVENT_INFO(v) >> 16) & 0xff)
#define VAL_EVENT_WIN(v)    ((v)->payload.event.win)   //((VAL_EVENT_INFO(v) >> 24) & 0xff)
#define VAL_EVENT_MODEL(v)  ((v)->payload.event.model)
#define VAL_EVENT_DATA(v)   ((v)->payload.event.data)
#define VAL_EVENT_TIME(v)   ((v)->payload.event.time)
#define VAL_EVENT_REQ(v)    ((v)->payload.event.eventee.req)

#define VAL_EVENT_SER(v)    ((v)->payload.event.eventee.ser)

#define IS_EVENT_MODEL(v,f) (VAL_EVENT_MODEL(v) == (f))

#define SET_EVENT_INFO(val, type, flags, win) \
    VAL_EVENT_TYPE(val)=type, VAL_EVENT_FLAGS(val)=flags, VAL_EVENT_WIN(val)=win
    //VAL_EVENT_INFO(val) = (type | (flags << 16) | (win << 24))

#define VAL_EVENT_X(v)      ((REBINT) (short) (VAL_EVENT_DATA(v) & 0xffff))
#define VAL_EVENT_Y(v)      ((REBINT) (short) ((VAL_EVENT_DATA(v) >> 16) & 0xffff))
#define VAL_EVENT_XY(v)     (VAL_EVENT_DATA(v))
#define SET_EVENT_XY(v,x,y) VAL_EVENT_DATA(v) = ((y << 16) | (x & 0xffff))

#define VAL_EVENT_KEY(v)    (VAL_EVENT_DATA(v) & 0xffff)
#define VAL_EVENT_KCODE(v)  ((VAL_EVENT_DATA(v) >> 16) & 0xffff)
#define SET_EVENT_KEY(v,k,c) VAL_EVENT_DATA(v) = ((c << 16) + k)

#define IS_KEY_EVENT(type)  0


/***********************************************************************
**
**  GOBS - Graphic Objects
**
***********************************************************************/

#pragma pack() // set back to default (was set to 4 at start of file)
    #include "reb-gob.h"
#pragma pack(4) // resume packing with 4

struct Reb_Gob {
    REBGOB *gob;
    REBCNT index;
};

#define VAL_GOB(v)          ((v)->payload.gob.gob)
#define VAL_GOB_INDEX(v)    ((v)->payload.gob.index)
#define SET_GOB(v,g) \
    (VAL_RESET_HEADER(v, REB_GOB), VAL_GOB(v) = g, VAL_GOB_INDEX(v) = 0)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE PAYLOAD DEFINITION (`struct Reb_Value`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The value is defined to have the header first, and then the value.  Having
// the header come first is taken advantage of by the trick for allowing
// a single REBUPT-sized value (32-bit on 32 bit builds, 64-bit on 64-bit
// builds) be examined to determine if a value is an END marker or not.
//
// One aspect of having the header before the payload is that on 32-bit
// platforms, this means that the starting address of the payload is not on
// a 64-bit alignment boundary.  This means placing a quantity that needs
// 64-bit alignment at the start of a payload can cause problems on some
// platforms with strict alignment requirements.
//
// (Note: The reason why this can happen at all is due to the #pragma pack(4)
// that is put in effect at the top of this file.)
//
// See Reb_Integer, Reb_Decimal, and Reb_Typeset for examples where the 64-bit
// quantity is padded by a pointer or pointer-sized-REBUPT to compensate.
//

// Reb_All is a structure type designed specifically for getting at
// the underlying bits of whichever union member is in effect inside
// the Reb_Value_Data.  This is not actually legal, although if types
// line up in unions it could be possibly be made "more legal":
//
//     http://stackoverflow.com/questions/11639947/
//
struct Reb_All {
#if defined(__LP64__) || defined(__LLP64__)
    REBCNT bits[6];
#else
    REBCNT bits[3];
#endif
};

#define VAL_ALL_BITS(v) ((v)->payload.all.bits)

union Reb_Value_Payload {
    struct Reb_All all;

    REBUNI character; // It's CHAR! (for now), but 'char' is a C keyword

    struct Reb_Integer integer;
    struct Reb_Decimal decimal;

    struct Reb_Pair pair;
    struct Reb_Money money;
    struct Reb_Handle handle;
    struct Reb_Time time;
    struct Reb_Tuple tuple;
    struct Reb_Datatype datatype;
    struct Reb_Typeset typeset;

    // Both Any_Word and Any_Series have a Reb_Binding_Target as the first
    // item in their payloads.  Since Reb_Value_Payload is a union, C permits
    // the *reading* of common leading elements from another union member,
    // even if that wasn't the last union written.  While this has been a
    // longstanding informal assumption in C, the standard has adopted it:
    //
    //     http://stackoverflow.com/a/11996970/211160
    //
    // So `any_target` can be used to read the binding target out of either
    // an ANY-WORD! or ANY-SERIES! payload.  To be on the safe side, *writing*
    // should likely be specifically through `any_word` or `any_series`.
    //
    union Reb_Binding_Target any_target;
        struct Reb_Any_Word any_word;
        struct Reb_Any_Series any_series;

    struct Reb_Any_Context any_context;

    struct Reb_Function function;

    struct Reb_Varargs varargs;

    REBCNT rebcnt; // !!! used by extensions, review
    struct Reb_Library library;
    struct Reb_Struct structure; // It's STRUCT!, but 'struct' is a C keyword

    struct Reb_Event event;
    struct Reb_Gob gob;

    struct Reb_Symbol symbol; // internal

#if !defined(NDEBUG) && defined(TRACK_EMPTY_PAYLOADS)
    struct Reb_Track track; // debug only (for void/trash, NONE!, LOGIC!, BAR!)
#endif
};

struct Reb_Value
{
    struct Reb_Value_Header header;
    union Reb_Value_Payload payload;
};

#pragma pack() // set back to default (was set to 4 at start of file)


#ifdef __cplusplus
    struct Reb_Specific_Value : public Reb_Value {
    #if !defined(NDEBUG)
        //
        // In C++11, it is now formally legal to add constructors to types
        // without interfering with their "standard layout" properties, or
        // making them uncopyable with memcpy(), etc.  For the rules, see:
        //
        //     http://stackoverflow.com/a/7189821/211160
        //
        // In the debug C++ build there is an extra check of writability. This
        // makes sure that stack variables holding REBVAL are marked with that.
        // It also means that the check can only be performed by the C++ build,
        // otherwise there would need to be a manual set of this bit on every
        // stack variable.
        //
        Reb_Specific_Value () {
            header.bits = WRITABLE_MASK_DEBUG;
        }
    #endif
    };
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBVAL ("fully specified" value) and RELVAL ("possibly relative" value)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// RELVAL exists to help quarantine the bit patterns for relative words into
// the deep-copied-body of the function they are for.  To actually look them
// up, they must be paired with a FRAME! matching the actual instance of the
// running function on the stack they correspond to.  Once made specific,
// a word may then be freely copied into any REBVAL slot.
//
// In addition to ANY-WORD!, an ANY-ARRAY! can also be relative, if it is
// part of the deep-copied function body.  The reason that arrays must be
// relative too is in case they contain relative words.  If they do, then
// recursion into them must carry forward the resolving "specifier" pointer
// to be combined with any relative words that are seen later.
//
// A relative value pointer is allowed to point to a specific value, but a
// relative word or array cannot be pointed to by a REBVAL*.
//

// When you have a RELVAL* (e.g. from a REBARR) that you "know" to be specific,
// this macro can be used for that.  Checks to make sure in debug build.
//
// Use for: "invalid conversion from 'Reb_Value*' to 'Reb_Specific_Value*'"
//
#ifdef NDEBUG
    #define const_KNOWN(v)      cast(const REBVAL*, (v))
    #define KNOWN(v)            cast(REBVAL*, (v))
#else
    #define const_KNOWN(v)      const_KNOWN_Debug(v)
    #define KNOWN(v)            KNOWN_Debug(v)
#endif

// To make it easier to look for places that think they aren't dealing
// with any relative values, pass this as the specifier instead of NULL.
//
#define SPECIFIED cast(REBCTX*, 0xF10F10F1)
#define GUESSED SPECIFIED // document sites where you're not sure it's specific

// A very small subset of code wants to be able to work with a relative
// function body in an unbound way.  If it were not able to, then the debug
// dumping of a relativized function body's values would have to copy (which
// would undermine the debug operation by doing a complex operation) or
// the system would have to be generally tolerant of "lying" specifiers.
// Instead, you can combine the array with ARCHETYPED.
//
// !!! Review this and if it can be made debug build only.
//
#define ARCHETYPED cast(REBCTX*, 0xB10B10B1)

// This can be used to turn a RELVAL into a REBVAL.  If the RELVAL is
// indeed relative and needs to be made specific to be put into the
// REBVAL, then the specifier is used to do that.  Debug builds assert
// that the function in the specifier indeed matches the target in
// the relative value (because relative values in an array may only
// be relative to the function that deep copied them, and that is the
// only kind of specifier you can use with them).
//
#define COPY_VALUE_MACRO(dest,src,specifier) \
    do { \
        if (IS_RELATIVE(src)) { \
            (dest)->header.bits = (src)->header.bits & ~VALUE_FLAG_RELATIVE; \
            if (ANY_ARRAY(src)) { \
                (dest)->payload.any_series.target.specific = (specifier); \
                (dest)->payload.any_series.series \
                    = (src)->payload.any_series.series; \
                (dest)->payload.any_series.index \
                    = (src)->payload.any_series.index; \
            } \
            else { \
                assert(ANY_WORD(src)); \
                (dest)->payload.any_word.place.binding.target.specific \
                    = (specifier); \
                (dest)->payload.any_word.place.binding.index \
                    = (src)->payload.any_word.place.binding.index; \
                (dest)->payload.any_word.sym = (src)->payload.any_word.sym; \
            } \
        } \
        else { \
            (dest)->header = (src)->header; \
            (dest)->payload = (src)->payload; \
        } \
    } while (0)


#ifdef NDEBUG
    #define COPY_VALUE(dest,src,specifier) \
        COPY_VALUE_Release(SINK(dest),(src),(specifier))
#else
    #define COPY_VALUE(dest,src,specifier) \
        COPY_VALUE_Debug(SINK(dest),(src),(specifier))
#endif

#ifdef NDEBUG
    #define ASSERT_NO_RELATIVE(array,deep) \
        NOOP
#else
    #define ASSERT_NO_RELATIVE(array,deep) \
        Assert_No_Relative((array),(deep))
#endif


// In the C++ build, defining this overload that takes a REBVAL* instead of
// a RELVAL*, and then not defining it...will tell you that you do not need
// to use COPY_VALUE.  Just say `*dest = *src` if your source is a REBVAL!
//
#ifdef __cplusplus
void COPY_VALUE_Debug(REBVAL *dest, const REBVAL *src, REBCTX *specifier);
#endif

//=////////////////////////////////////////////////////////////////////////=//
//
//  DEBUG PROBE AND PANIC
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The PROBE macro can be used in debug builds to mold a REBVAL much like the
// Rebol `probe` operation.  PROBE_MSG can add a message:
//
//     REBVAL *v = Some_Value_Pointer();
//
//     PROBE(v);
//     PROBE_MSG(v, "the v value debug dump label");
//
// In order to make it easier to find out where a piece of debug spew is
// coming from, the file and line number will be output as well.
//
// PANIC_VALUE causes a crash on a value, while trying to provide information
// that could identify where that value was assigned.  If it is resident
// in a series and you are using Address Sanitizer or Valgrind, then it should
// cause a crash that pinpoints the stack where that array was allocated.
//

#if !defined(NDEBUG)
    #define PROBE(v) \
        Probe_Core_Debug(NULL, __FILE__, __LINE__, (v))

    #define PROBE_MSG(v, m) \
        Probe_Core_Debug((m), __FILE__, __LINE__, (v))

    #define PANIC_VALUE(v) \
        Panic_Value_Debug((v), __FILE__, __LINE__)
#endif

#define Panic_Value(v) \
    PANIC_VALUE(v)


//=////////////////////////////////////////////////////////////////////////=//
//
//  GUARDING
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Some REBVALs contain one or more series that need to be guarded.  With
// PUSH_GUARD_VALUE() it is possible to not worry about what series are in
// a value, as it will take care of it if there are any.  As with series
// guarding, the last value guarded must be the first one you DROP_GUARD on.
//

#define PUSH_GUARD_VALUE(v) \
    Guard_Value_Core(v)

#define DROP_GUARD_VALUE(v) \
    do { \
        GC_Value_Guard->content.dynamic.len--; \
        assert((v) == cast(RELVAL **, \
            GC_Value_Guard->content.dynamic.data)[ \
                GC_Value_Guard->content.dynamic.len \
            ]); \
    } while (0)


#endif // %sys-value.h
