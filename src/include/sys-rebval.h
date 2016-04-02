//
//  File: %sys-rebval.h
//  Summary: {Definitions for the Rebol Boxed Value Struct (REBVAL)}
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

//
// Note: Forward declarations are in %reb-defs.h
//

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
// Note also that a trick which attempted to put a header into each payload
// won't work.  There would be no way to generically adjust the bits of the
// header without picking a specific instance of the union to do it in, which
// would invalidate the payload for any other type.
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
//    unsigned specific:16;     // flags that are specific to this REBVAL kind
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
#define NOT_END_MASK \
    cast(REBUPT, 0x01)

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
#ifdef NDEBUG
    //
    // Though in the abstract it is desirable to have a way to protect an
    // individual value from writing even in the release build, this is
    // a debug-only check...it makes every value header initialization have
    // to do two writes instead of one (one to format, then later to write
    // and check that it is properly formatted space)
    //
#else
    // We want to be assured that we are not trying to take the type of a
    // value that is actually an END marker, because end markers chew out only
    // one bit--the rest of the REBUPT bits besides the bottom two may be
    // anything necessary for the purpose.
    //
    #define WRITABLE_MASK_DEBUG \
        cast(REBUPT, 0x02)
#endif

// The type mask comes up a bit and it's a fairly obvious constant, so this
// hardcodes it for obviousness.  High 6 bits of the lowest byte.
//
#define HEADER_TYPE_MASK \
    cast(REBUPT, 0xFC)


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
    // Both NONE! and LOGIC!'s false state are FALSE? ("conditionally false").
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
    // When a REBVAL slot wishes to signal that it is a "throw" (e.g. a
    // RETURN, BREAK, CONTINUE or generic THROW signal), this bit is set on
    // that cell.
    //
    // The bit being set does not mean the cell contains the thrown quantity
    // (e.g. it would not be the `1020` in `throw 1020`)  That evaluator
    // thread enters a modal "thrown state", and it's the state which hold
    // the value--which must be processed (or converted into an error) before
    // another throw occurs.
    //
    // Instead the bit indicates that the cell contains a value indicating
    // the label, or "name", of the throw.  Having the label quickly available
    // in the slot being bubbled up makes it easy for recipients to decide if
    // they are interested in throws of that type or not.
    //
    // R3-Alpha code would frequently forget to check for thrown values, and
    // wind up acting as if they did not happen.  In addition to enforcing that
    // all thrown values are handled by entering a "thrown state" for the
    // interpreter, all routines that can potentially return thrown values
    // have been adapted to return a boolean and adopt the XXX_Throws()
    // naming convention:
    //
    //     if (XXX_Throws()) {
    //        /* handling code */
    //     }
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
    VALUE_FLAG_RELATIVE = 1 << (GENERAL_VALUE_BIT + 4),

    // `VALUE_FLAG_ARRAY` is an acceleration of the ANY_ARRAY() test, which
    // also helps keep this file's COPY_VALUE from being dependent on
    // VAL_TYPE() which is provided in %sys-value.h
    //
    VALUE_FLAG_ARRAY = 1 << (GENERAL_VALUE_BIT + 5),

    // `VALUE_FLAG_EVALUATED` is a somewhat dodgy-yet-important concept.
    // This is that some functions wish to be sensitive to whether or not
    // their argument came as a literal in source or as a product of an
    // evaluation.  While all values carry the bit, it is only guaranteed
    // to be meaningful on arguments in function frames...though it is
    // valid on any result at the moment of taking it from Do_Core().
    //
    VALUE_FLAG_EVALUATED = 1 << (GENERAL_VALUE_BIT + 6)
};


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
// NONE!, LOGIC!, and BAR!.
//
// In addition to the file and line number where the assignment was made,
// the "tick count" of the DO loop is also saved.  This means that it can
// be possible in a repro case to find out which evaluation step produced
// the value--and at what place in the source.  Repro cases can be set to
// break on that tick count, if it is deterministic.
//

#if !defined(NDEBUG)
    struct Reb_Track {
        const char *filename;
        int line;
        REBUPT count;
    };
#endif

struct Reb_Datatype {
    enum Reb_Kind kind;
    REBARR *spec;
};

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

struct Reb_Decimal {
    //
    // See notes on Reb_Integer for why this is needed (handles case of when
    // `value` is 64-bit, and the platform is 32-bit.)
    //
    REBUPT padding;

    REBDEC dec;
};

struct Reb_Money {
    deci amount;
};

typedef struct reb_ymdz {
#ifdef ENDIAN_LITTLE
    REBINT zone:7; // +/-15:00 res: 0:15
    REBCNT day:5;
    REBCNT month:4;
    REBCNT year:16;
#else
    REBCNT year:16;
    REBCNT month:4;
    REBCNT day:5;
    REBINT zone:7; // +/-15:00 res: 0:15
#endif
} REBYMD;

typedef union reb_date {
    REBYMD date;
    REBCNT bits;
} REBDAT;

struct Reb_Time {
    REBI64 nanoseconds;
    REBDAT date;
};

typedef struct Reb_Tuple {
    REBYTE tuple[12];
} REBTUP;

union Reb_Binding_Target {
    REBFUN *relative; // for VALUE_FLAG_RELATIVE
    REBCTX *specific; // for !VALUE_FLAG_RELATIVE
};

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

struct Reb_Symbol {
    REBCNT canon; // Index of the canonical (first) word
    REBCNT alias; // Index to next alias form
    REBCNT name; // Index into PG_Word_Names string
};

struct Reb_Typeset {
    REBSYM sym; // Symbol (if a key of object or function param)

    // Note: `sym` is first so that the value's 32-bit Reb_Flags header plus
    // the 32-bit REBCNT will pad `bits` to a REBU64 alignment boundary

    REBU64 bits; // One bit for each DATATYPE! (use with FLAGIT_64)
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
        REBCTX *spec; // used by REB_MODULE

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
    const REBVAL *param;

    // Similar to the param, the arg is only good for the lifetime of the
    // FRAME!...but even less so, because VARARGS! can (currently) be
    // overwritten with another value in the function frame at any point.
    // Despite this, we proxy the VALUE_FLAG_EVALUATED from the last TAKE
    // onto the argument to reflect its *argument* status.
    //
    REBVAL *arg;
};

struct Reb_Handle {
    union {
        CFUNC *code;
        void *data;
        REBUPT number;
    } thing;
};

typedef struct Reb_Library_Handle {
    void * fd;
    REBUPT flags;
} REBLHL; // sizeof(REBLHL) % 8 must equal 0 on both 64-bit and 32-bit builds

struct Reb_Library {
    REBLHL *handle;
    REBARR *spec;
};

typedef struct Reb_Struct {
    REBARR *spec;
    REBSER *fields;    // fields definition
    REBSER *data;
} REBSTU;

struct Reb_Routine_Info {
    union {
        struct {
            REBLHL *lib;
            CFUNC *funcptr;
        } rot;
        struct {
            void *closure; // actually `ffi_closure*` (see RIN_CLOSURE)
            REBFUN *func;
            void *dispatcher;
        } cb;
    } info;
    void *cif; // actually `ffi_cif*` (see RIN_CIF)
    REBSER *arg_types; // index 0 is the return type
    REBARR *arg_structs; // for struct arguments
    REBSER *extra_mem; // extra memory that needs to be freed
    REBCNT flags; // !!! 32-bit...should it use REBFLGS for 64-bit on 64-bit?
    REBINT abi; // actually `ffi_abi` (see RIN_ABI)

    //REBUPT padding; // sizeof(Reb_Routine_Info) % 8 must be 0 for Make_Node()
};

#pragma pack() // set back to default (was set to 4 at start of file)
    #include "reb-gob.h"
#pragma pack(4) // resume packing with 4

struct Reb_Gob {
    REBGOB *gob;
    REBCNT index;
};

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

#if !defined(NDEBUG)
    struct Reb_Track track; // debug only (for void/trash, NONE!, LOGIC!, BAR!)
#endif

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

    struct Reb_Library library;
    struct Reb_Struct structure; // It's STRUCT!, but 'struct' is a C keyword

    struct Reb_Event event;
    struct Reb_Gob gob;

    struct Reb_Symbol symbol; // internal

    struct Reb_All all;
};

struct Reb_Value
{
    struct Reb_Value_Header header;
    union Reb_Value_Payload payload;
};



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
// The debug build puts REB_MAX in the type slot of a REB_END, to help to
// distinguish it from the 0 that signifies an unset TRASH.  This means that
// any writable value can be checked to ensure it is an actual END marker
// and not "uninitialized".  This trick can only be used so long as REB_MAX
// is 63 or smaller (ensured by an assertion at startup ATM.
//

#define END_CELL \
    c_cast(const REBVAL*, &PG_End_Cell)

#define IS_END_MACRO(v) \
    LOGICAL((v)->header.bits % 2 == 0) // == NOT(bits & NOT_END_MASK)

#ifdef NDEBUG
    #define IS_END(v) \
        IS_END_MACRO(v)

    inline static void SET_END(RELVAL *v) {
        v->header.bits = 0;
    }
#else
    // Note: These must be macros (that don't need IS_END_Debug or
    // SET_END_Debug defined until used at a callsite) because %tmp-funcs.h
    // cannot be included until after REBSER and other definitions that
    // depend on %sys-rebval.h have been defined.  (Or they could be manually
    // forward-declared here.)
    //
    #define IS_END(v) \
        IS_END_Debug((v), __FILE__, __LINE__)

    #define SET_END(v) \
        SET_END_Debug((v), __FILE__, __LINE__)
#endif

#define NOT_END(v) \
    NOT(IS_END(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  REBVAL ("fully specified" value) and RELVAL ("possibly relative" value)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A relative value is the identical struct to Reb_Value, but is allowed to
// have the relative bit set.  Hence a relative value pointer can point to a
// specific value, but a relative word or array cannot be pointed to by a
// plain REBVAL*.  The RELVAL-vs-REBVAL distinction is purely commentary
// in the C build, but the C++ build makes REBVAL a type derived from RELVAL.
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

inline static REBOOL IS_RELATIVE(const RELVAL *v) {
    return LOGICAL(v->header.bits & VALUE_FLAG_RELATIVE);
}

#if defined(__cplusplus)
    //
    // Take special advantage of the fact that C++ can help catch when we are
    // trying to see if a REBVAL is specific or relative (it will always
    // be specific, so the call is likely in error).  In the C build, they
    // are the same type so there will be no error.
    //
    REBOOL IS_RELATIVE(const REBVAL *v);
#endif

#define IS_SPECIFIC(v) \
    NOT(IS_RELATIVE(v))

inline static REBFUN *VAL_RELATIVE(const RELVAL *v) {
    assert(IS_RELATIVE(v));
    return v->payload.any_target.relative; // any_target is read-only, see note
}

#define VAL_SPECIFIC_MACRO(v) \
    ((v)->payload.any_target.specific) // any_target is read-only, see note

#ifdef NDEBUG
    #define VAL_SPECIFIC(v) \
        VAL_SPECIFIC_MACRO(v)
#else
    #define VAL_SPECIFIC(v) \
        VAL_SPECIFIC_Debug(v)
#endif

// When you have a RELVAL* (e.g. from a REBARR) that you "know" to be specific,
// the KNOWN macro can be used for that.  Checks to make sure in debug build.
//
// Use for: "invalid conversion from 'Reb_Value*' to 'Reb_Specific_Value*'"

inline static const REBVAL *const_KNOWN(const RELVAL *value) {
    assert(IS_SPECIFIC(value));
    return cast(const REBVAL*, value); // we asserted it's actually specific
}

inline static REBVAL *KNOWN(RELVAL *value) {
    assert(IS_SPECIFIC(value));
    return cast(REBVAL*, value); // we asserted it's actually specific
}

inline static const RELVAL *const_REL(const REBVAL *v) {
    return cast(const RELVAL*, v); // cast w/input restricted to REBVAL
}

inline static RELVAL *REL(REBVAL *v) {
    return cast(RELVAL*, v); // cast w/input restricted to REBVAL
}

#ifdef NDEBUG
    #define SPECIFIED NULL
#else
    #define SPECIFIED \
        cast(REBCTX*, 0xF10F10F1) // helps catch uninitialized locations
#endif

// !!! temporary - used to document any sites where one is not sure if the
// value is specific, to aid in finding them to review
//
#define GUESSED SPECIFIED


// This can be used to turn a RELVAL into a REBVAL.  If the RELVAL is
// indeed relative and needs to be made specific to be put into the
// REBVAL, then the specifier is used to do that.  Debug builds assert
// that the function in the specifier indeed matches the target in
// the relative value (because relative values in an array may only
// be relative to the function that deep copied them, and that is the
// only kind of specifier you can use with them).
//
// NOTE: The reason this is written to specifically intialize the `specific`
// through the union member of the remaining type is to stay on the right
// side of the standard.  While *reading* a common leading field out of
// different union members is legal regardless of who wrote it last,
// *writing* a common leading field will invalidate the ensuing fields of
// other union types besides the one it was written through (!)  See notes
// in Reb_Value's structure definition.
//
inline static void COPY_VALUE_CORE(
    REBVAL *dest,
    const RELVAL *src,
    REBCTX *specifier
) {
    if (src->header.bits & VALUE_FLAG_RELATIVE) {
        dest->header.bits = src->header.bits & ~VALUE_FLAG_RELATIVE;
        if (src->header.bits & VALUE_FLAG_ARRAY) {
            dest->payload.any_series.target.specific = specifier;
            dest->payload.any_series.series
                = src->payload.any_series.series;
            dest->payload.any_series.index
                = src->payload.any_series.index;
        }
        else { // must be ANY-WORD! (checked by COPY_VALUE_Debug)
            dest->payload.any_word.place.binding.target.specific
                = specifier;
            dest->payload.any_word.place.binding.index
                = src->payload.any_word.place.binding.index;
            dest->payload.any_word.sym = src->payload.any_word.sym;
        }
    }
    else {
        dest->header = src->header;
        dest->payload = src->payload;
    }
}


#ifdef NDEBUG
    #define COPY_VALUE(dest,src,specifier) \
        COPY_VALUE_CORE(SINK(dest),(src),(specifier))
#else
    #define COPY_VALUE(dest,src,specifier) \
        COPY_VALUE_Debug(SINK(dest),(src),(specifier))
#endif

#ifdef NDEBUG
    #define ASSERT_NO_RELATIVE(array,deep) NOOP
#else
    #define ASSERT_NO_RELATIVE(array,deep) \
        Assert_No_Relative((array),(deep))
#endif

#pragma pack() // set back to default (was set to 4 at start of file)
