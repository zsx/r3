//
//  File: %sys-rebval.h
//  Summary: {Definitions for the Rebol Boxed Value Struct (REBVAL)}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
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
// REB_BLOCK, REB_STRING, etc.  Then there are flags which are for general
// purposes that could apply equally well to any type of value (including
// whether the value should have a new-line after it when molded out inside
// of a block).  Followed by that are bits which are custom to each type (for
// instance whether a key in an object is hidden or not).
//
// Obviously, an arbitrary long string won't fit into the remaining 3*32 bits,
// or even 3*64 bits!  You can fit the data for an INTEGER or DECIMAL in that
// (at least until they become arbitrary precision) but it's not enough for
// a generic BLOCK! or a FUNCTION! (for instance).  So the remaining bits
// often will point to one or more Rebol "nodes" (see %sys-series.h for an
// explanation of REBSER, REBARR, REBCTX, and REBMAP.)
//
// So the next part of the structure is the "Extra".  This is the size of one
// pointer, which sits immediately after the header (that's also the size of
// one pointer).
//
// This sets things up for the "Payload"--which is the size of two pointers.
// It is broken into a separate structure at this position so that on 32-bit
// platforms, it can be aligned on a 64-bit boundary (assuming the REBVAL's
// starting pointer was aligned on a 64-bit boundary to start with).  This is
// important for 64-bit value processing on 32-bit platforms, which will
// either be slow or crash if reads of 64-bit floating points/etc. are done
// on unaligned locations.
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// * Forward declarations are in %reb-defs.h
//
// * See %sys-rebnod.h for an explanation of FLAGIT_LEFT.  This file defines
//   those flags which are common to every value of every type.  Due to their
//   scarcity, they are chosen carefully.
//


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_THROWN
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This is how a REBVAL signals that it is a "throw" (e.g. a RETURN, BREAK,
// CONTINUE or generic THROW signal).
//
// The bit being set does not mean the cell contains the thrown quantity
// (e.g. it would not be the `1020` in `throw 1020`)  The evaluator thread
// enters a modal "thrown state", and it's the state which holds the value.
// It must be processed (or trigger an error) before another throw occurs.
//
// What the bit actually indicates is a cell containing the "label" or "name"
// of the throw.  Having the label quickly available in the slot being bubbled
// up makes it easy for recipients to decide if they are interested in throws
// of that type or not--after which they can request the thrown value.
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
#define VALUE_FLAG_THROWN \
    NODE_FLAG_SPECIAL


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_FALSEY
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This flag is used as a quick cache on BLANK! or LOGIC! false values.
// These are the only two values that return true from the FALSE? native
// (a.k.a. "conditionally false").  All other types are TRUE?.
//
// Because of this cached bit, LOGIC! does not need to store any data in its
// payload... its data of being true or false is already covered by this
// header bit.
//
// !!! Since tests for conditional truth or falsehood are extremely common
// (not just in IF and EITHER, but in CASE and ANY and many other constructs),
// it seems like a good optimization.  But it is a cache and could be done
// with a slightly more expensive test.  Given the scarcity of header bits in
// the modern codebase, this optimization may need to be sacrificed to
// reclaim the bit for a "higher purpose".
//
#define VALUE_FLAG_FALSEY \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 0)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_LINE
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This is a line marker bit, such that when the value is molded it will put a
// newline before the value.  (The details are a little more subtle than that,
// because an ANY-PATH! could not be LOADed back if this were allowed.)
//
// The bit is set initially by what the scanner detects, and then left to the
// user's control after that.
//
// !!! The native `new-line` is used set this, which has a somewhat poor
// name considering its similarity to `newline` the line feed char.
//
#define VALUE_FLAG_LINE \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 1)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_RELATIVE
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This flag is used to indicate a value that needs to have a specific context
// added into it before it can have its bits copied--or used for some other
// purposes.
//
// An ANY-WORD! is relative if it refers to a local or argument of a function,
// and has its bits resident in the deep copy of that function's body.
//
// An ANY-ARRAY! in the deep copy of a function body must be relative also to
// the same function if it contains any instances of such relative words.
//
#define VALUE_FLAG_RELATIVE \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 2)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_UNEVALUATED
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Some functions wish to be sensitive to whether or not their argument came
// as a literal in source or as a product of an evaluation.  While all values
// carry the bit, it is only guaranteed to be meaningful on arguments in
// function frames...though it is valid on any result at the moment of taking
// it from Do_Core().
//
// It is in the negative sense because the act of requesting it is uncommon,
// e.g. from the QUOTE operator.  So most Init_Blank() or other assignment
// should default to being "evaluative".
//
// !!! This concept is somewhat dodgy and experimental, but it shows promise
// in addressing problems like being able to give errors if a user writes
// something like `if [x > 2] [print "true"]` vs. `if x > 2 [print "true"]`,
// while still tolerating `item: [a b c] | if item [print "it's an item"]`. 
// That has a lot of impact for the new user experience.
//
#define VALUE_FLAG_UNEVALUATED \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 3)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_STACK
//
//=////////////////////////////////////////////////////////////////////////=//
//
// When writing to a value cell, it is sometimes necessary to know how long
// that cell will "be alive".  This is important if there is some stack-based
// transient structure in the source cell, which would need to be converted
// into something longer-lived if the destination cell will outlive it.
//
// Hence cells must be formatted to say whether they are VALUE_FLAG_STACK or
// not, before any writing can be done to them.  If they are not then they
// are presumed to be indefinite lifetime (e.g. cells resident inside of an
// array managed by the garbage collector).
//
// But if a cell is marked with VALUE_FLAG_STACK, that means it is expected
// that scanning *backwards* in memory will find a specially marked REB_FRAME
// cell, which will lead to the frame to whose lifetime the cell is bound.
//
// !!! This feature is a work in progress.
//
#define VALUE_FLAG_STACK \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 4)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_ENFIXED
//
//=////////////////////////////////////////////////////////////////////////=//
//
// In R3-Alpha and Rebol2, there was a special kind of function known as an
// OP! which would acquire its first argument from the left hand side.  In
// Ren-C, there is only one kind of function, but it's possible to tag a
// particular function value cell in a context as being "enfixed", hence it
// will acquire its first argument from the left.
//
// This bit is not copied by Move_Value.  As a result, if you say something
// like `foo: :+`, foo will contain the non-enfixed form of the function.
//
// !!! The feature of not carrying over enfixedness in assignment was designed
// as part of the "OneFunction" initiative, to try and make it so that when
// something like a SORT function was passed a comparator, it would not have
// to worry about that function being infix or not.  However, the addition of
// the <tight> parameter convention throws in a potential wrench to the idea
// that callees can somehow ignore variances in how functions process their
// arguments.  It may be that this should be a function flag, and carried
// over normally...but conservatively the feature is implemented like this.
//

#define VALUE_FLAG_ENFIXED \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 5)


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE_FLAG_PROTECTED
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Values can carry a user-level protection bit.  The bit is not copied by
// Move_Value(), and hence reading a protected value and writing it to
// another location will not propagate the protectedness from the original
// value to the copy.
//

#define VALUE_FLAG_PROTECTED \
    FLAGIT_LEFT(GENERAL_VALUE_BIT + 6)


// v-- BEGIN TYPE SPECIFIC BITS HERE


#define TYPE_SPECIFIC_BIT \
    (GENERAL_VALUE_BIT + 7)


// Technically speaking, this only needs to use 6 bits of the rightmost byte
// to store the type.  So using a full byte wastes 2 bits.  However, the
// performance advantage of not needing to mask to do VAL_TYPE() is worth
// it...also there may be a use for 256 types (even though the type bitsets
// are only 64-bits at the moment)
//
#define HEADERIZE_KIND(kind) \
    FLAGBYTE_RIGHT(kind)



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
    };
#endif

struct Reb_Datatype {
    enum Reb_Kind kind;
    REBARR *spec;
};

// !!! In R3-alpha, the money type was implemented under a type called "deci".
// The payload for a deci was more than 64 bits in size, which meant it had
// to be split across the separated union components in Ren-C.  (The 64-bit
// aligned "payload" and 32-bit aligned "extra" were broken out independently,
// so that setting one union member would not disengage the other.)

struct Reb_Money {
    unsigned m1:32; /* significand, continuation */
    unsigned m2:23; /* significand, highest part */
    unsigned s:1;   /* sign, 0 means nonnegative, 1 means nonpositive */
    int e:8;        /* exponent */
};

// !!! This structure varies the layout based on endianness, so that when it
// is seen throuh the .bits field of the REBDAT union, a later date will
// have a value that will be greater (>) than an earlier date.  This should
// be reviewed for standards compliance; masking and shifting is generally
// safer than bit field union tricks.
//
typedef struct reb_ymdz {
#ifdef ENDIAN_LITTLE
    int zone:7; // +/-15:00 res: 0:15
    unsigned day:5;
    unsigned month:4;
    unsigned year:16;
#else
    unsigned year:16;
    unsigned month:4;
    unsigned day:5;
    int zone:7; // +/-15:00 res: 0:15
#endif
} REBYMD;

typedef union reb_date {
    REBYMD date;
    REBCNT bits; // !!! alias used for hashing date, is this standards-legal? 
} REBDAT;

// The same payload is used for TIME! and DATE!.  The extra bits needed by
// DATE! (as REBYMD) fit into 32 bits, so can live in the ->extra field,
// which is the size of a platform pointer.
//
struct Reb_Time {
    REBI64 nanoseconds;
};

typedef struct Reb_Tuple {
    REBYTE tuple[8];
} REBTUP;


struct Reb_Any_Series {
    //
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

struct Reb_Typeset {
    REBU64 bits; // One bit for each DATATYPE! (use with FLAGIT_KIND)
};


struct Reb_Any_Word {
    //
    // This is the word's non-canonized spelling.  It is a UTF-8 string.
    //
    REBSTR *spelling;

    // Index of word in context (if word is bound, e.g. `binding` is not NULL)
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


struct Reb_Function {
    //
    // `paramlist` is a Rebol Array whose 1..NUM_PARAMS values are all
    // TYPESET! values, with an embedded symbol (a.k.a. a "param") as well
    // as other bits, including the parameter class (PARAM_CLASS).  This
    // is the list that is processed to produce WORDS-OF, and which is
    // consulted during invocation to fulfill the arguments
    //
    // In addition, its [0]th element contains a FUNCTION! value which is
    // self-referentially the function itself.  This means that the paramlist
    // can be passed around as a single pointer from which a whole REBVAL
    // for the function can be found (although this value is archetypal, and
    // loses the `binding` property--which must be preserved other ways)
    //
    // The `link.meta` field of the paramlist holds a meta object (if any)
    // that describes the function.  This is read by help.
    //
    REBARR *paramlist;

    // `body_holder` is an optimized "singular" REBSER, the size of exactly
    // one value.  This is because the information for a function body is an
    // array in the majority of function instances, and also because it can
    // standardize the native dispatcher code in the REBARR's series "misc"
    // field.  This gives two benefits: no need for a switch on the function's
    // type to figure out the dispatcher, and also to move the dispatcher out
    // of the REBVAL itself into something that can be revectored or "hooked"
    // for all instances of the function.
    //
    // PLAIN FUNCTIONS: body is a BLOCK!, the body of the function, obviously
    // NATIVES: body is "equivalent code for native" (if any) in help
    // ACTIONS: body is a WORD! for the verb of the action (OPEN, APPEND, etc)
    // SPECIALIZATIONS: body is a 1-element array containing a FRAME!
    // CALLBACKS: body a HANDLE! (REBRIN*)
    // ROUTINES: body a HANDLE! (REBRIN*)
    //
    // The `link.underlying` field of the body_holder may point to the
    // specialization whose frame should be used to set the default values
    // for the arguments during a call.  Or it will point directly to the
    // function whose paramlist should be used in the frame pushed.  This is
    // different in hijackers, adapters, and chainers.
    //
    REBARR *body_holder;
};

struct Reb_Any_Context {
    //
    // `varlist` is a Rebol Array that from 1..NUM_VARS contains REBVALs
    // representing the stored values in the context.
    //
    // As with the `paramlist` of a FUNCTION!, the varlist uses the [0]th
    // element specially.  It stores a copy of the ANY-CONTEXT! value that
    // refers to itself.
    //
    // The `keylist` is held in the varlist's Reb_Series.link field, and it
    // may be shared with an arbitrary number of other contexts.  Changing
    // the keylist involves making a copy if it is shared.
    //
    // REB_MODULE depends on a property stored in the "meta" Reb_Series.link
    // field of the keylist, which is another object's-worth of data *about*
    // the module's contents (e.g. the processed header)
    //
    REBARR *varlist;

    // A single FRAME! can go through multiple phases of evaluation, some of
    // which should expose more fields than others.  For instance, when you
    // specialize a function that has 10 parameters so it has only 8, then
    // the specialization frame should not expose the 2 that have been
    // removed.  It's as if the WORDS-OF the spec is shorter than the actual
    // length which is used.
    //
    // Hence, each independent value that holds a frame must remember the
    // function whose "view" it represents.  This field is only applicable
    // to frames, and so it could be used for something else on other types
    //
    // Note that the binding on a FRAME! can't be used for this purpose,
    // because it's already used to hold the binding of the function it
    // represents.  e.g. if you have a definitional return value with a
    // binding, and try to MAKE FRAME! on it, the paramlist alone is not
    // enough to remember which specific frame that function should exit.
    //
    REBFUN *phase;
};


// The order in which refinements are defined in a function spec may not match
// the order in which they are mentioned on a path.  As an efficiency trick,
// a word on the data stack representing a refinement usage request can be
// mutated to store the pointer to its `param` and `arg` positions, so that
// they may be returned to after the later-defined refinement has had its
// chance to take the earlier fulfillments.
//
struct Reb_Varargs {
    //
    // If the extra->binding of the varargs is not UNBOUND, it represents the
    // frame in which this VARARGS! was tied to a parameter.  This 0-based
    // offset can be used to find the param the varargs is tied to, in order
    // to know whether it is quoted or not (and its name for error delivery).
    //
    // It can also find the arg.  Similar to the param, the arg is only good
    // for the lifetime of the FRAME! in extra->binding...but even less so,
    // because VARARGS! can (currently) be overwritten with another value in
    // the function frame at any point.  Despite this, we proxy the
    // VALUE_FLAG_UNEVALUATED from the last TAKE to reflect its status.
    //
    REBCNT param_offset;

    // Data source for the VARARGS!.  This can come from a frame (and is often
    // the same as extra->binding), or from an array if MAKE ARRAY! is the
    // source of the variadic data.
    // 
    REBARR *feed;
};


// This is an internal type, used to memoize the location of a refinement
// which was invoked by the path but out of order from the refinement order
// in the function definition.  Because these can only exist on the stack
// they are given a REB_0 type, as opposed to having their own REB_XXX type.
//
struct Reb_Pickup {
    const REBVAL *param;
    REBVAL *arg;
};


// Handles hold a pointer and a size...which allows them to stand-in for
// a binary REBSER.
//
// Since a function pointer and a data pointer aren't necessarily the same
// size, the data has to be a union.
//
// Note that the ->extra field of the REBVAL may contain a singular REBARR
// that is leveraged for its GC-awareness.
//
struct Reb_Handle {
    union {
        void *pointer;
        CFUNC *cfunc;
    } data;

    REBUPT length;
};


// Meta information in singular->link.meta
// File descriptor in singular->misc.fd
//
struct Reb_Library {
    REBARR *singular; // singular array holding this library value
};

typedef REBARR REBLIB;


// The general FFI direction is to move it so that it is "baked in" less,
// and represents an instance of a generalized extension mechanism (like GOB!
// should be).  On that path, a struct's internals are simplified to being
// just an array:
//
// [0] is a specification array which contains all the information about
// the structure's layout, regardless of what offset it would find itself at
// inside of a data blob.  This includes the total size, and arrays of
// field definitions...essentially, the validated spec.  It also contains
// a HANDLE! which contains the FFI-type.
//
// [1] is the content BINARY!.  The VAL_INDEX of the binary indicates the
// offset within the struct.  See notes in ADDR-OF from the FFI about how
// the potential for memory instability of content pointers may not be a
// match for the needs of an FFI interface.
//
struct Reb_Struct {
    REBARR *stu; // [0] is canon self value, ->misc.schema is schema
    REBSER *data; // binary data series (may be shared with other structs)
};

// To help document places in the core that are complicit in the "extension
// hack", alias arrays being used for the FFI to another name.
//
typedef REBARR REBSTU;
typedef REBARR REBFLD;


#include "reb-gob.h"

struct Reb_Gob {
    REBGOB *gob;
    REBCNT index;
};


// Reb_All is a structure type designed specifically for getting at
// the underlying bits of whichever union member is in effect inside
// the Reb_Value_Data.  This is not actually legal, although if types
// line up in unions it could be possibly be made "more legal":
//
//     http://stackoverflow.com/questions/11639947/
//
struct Reb_All {
    REBUPT bits[2];
};


//=////////////////////////////////////////////////////////////////////////=//
//
//  VALUE CELL DEFINITION (`struct Reb_Value`)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The value is defined to have the header, "extra", and payload.  Having
// the header come first is taken advantage of by the trick for allowing
// a single REBUPT-sized value (32-bit on 32 bit builds, 64-bit on 64-bit
// builds) be examined to determine if a value is an END marker or not.
//
// Conceptually speaking, one might think of the "extra" as being part of
// the payload.  But it is broken out into a separate union.  This is because
// the `binding` property is written using common routines for several
// different types.  If the common routine picked just one of the payload
// unions to initialize, it would "disengage" the other unions.
//
// (C permits *reading* of common leading elements from another union member,
// even if that wasn't the last union used to write it.  But all bets are off
// for other unions if you *write* a leading member through another one.
// For longwinded details: http://stackoverflow.com/a/11996970/211160 )
//
// Another aspect of breaking out the "extra" is so that on 32-bit platforms,
// the starting address of the payload is on a 64-bit alignment boundary.
// See Reb_Integer, Reb_Decimal, and Reb_Typeset for examples where the 64-bit
// quantity requires things like REBDEC to have 64-bit alignment.  At time of
// writing, this is necessary for the "C-to-Javascript" emscripten build to
// work.  It's also likely preferred by x86.
//
// (Note: The reason why error-causing alignments were ever possible at all
// was due to a #pragma pack(4) that was used in R3-Alpha...Ren-C removed it.)
//

union Reb_Value_Extra {
    //
    // The binding will be either a REBFUN (relative to a function) or a
    // REBCTX (specific to a context).  ARRAY_FLAG_VARLIST can be
    // used to tell which it is.
    //
    // ANY-WORD!: binding is the word's binding
    //
    // ANY-ARRAY!: binding is the relativization or specifier for the REBVALs
    // which can be found inside of the frame (for recursive resolution
    // of ANY-WORD!s)
    //
    // FUNCTION!: binding is the instance data for archetypal invocation, so
    // although all the RETURN instances have the same paramlist, it is
    // the binding which is unique to the REBVAL specifying which to exit
    //
    // ANY-CONTEXT!: if a FRAME!, the binding carries the instance data from
    // the function it is for.  So if the frame was produced for an instance
    // of RETURN, the keylist only indicates the archetype RETURN.  Putting
    // the binding back together can indicate the instance.
    //
    // VARARGS!: the binding is the frame context where the variadic parameter
    // lives (or NULL if it was made with MAKE VARARGS! and hasn't been
    // passed through a parameter yet).
    //
    REBARR *binding;

    // The remaining properties are the "leftovers" of what won't fit in the
    // payload for other types.  If those types have a quanitity that requires
    // 64-bit alignment, then that gets the priority for being in the payload,
    // with the "Extra" pointer-sized item here.

    REBSTR *key_spelling; // if typeset is key of object or function parameter
    REBDAT date; // time's payload holds the nanoseconds, this is the date
    REBCNT struct_offset; // offset for struct in the possibly shared series

    // !!! Biasing Ren-C to helping solve its technical problems led the
    // REBEVT stucture to get split up.  The "eventee" is now in the extra
    // field, while the event payload is elsewhere.  This brings about a long
    // anticipated change where REBEVTs would need to be passed around in
    // clients as REBVAL-sized entities.
    //
    // See also rebol_devreq->requestee

    union Reb_Eventee eventee;

    unsigned m0:32; // !!! significand, lowest part - see notes on Reb_Money

    // There are two types of HANDLE!, and one version leverages the GC-aware
    // ability of a REBSER to know when no references to the handle exist and
    // call a cleanup function.  The GC-aware variant allocates a "singular"
    // array, which is the exact size of a REBSER and carries the canon data.
    // If the cheaper kind that's just raw data and no callback, this is NULL.
    //
    REBARR *singular;

#if !defined(NDEBUG)
    REBUPT do_count; // used by track payloads
#endif
};

union Reb_Value_Payload {
    struct Reb_All all;

#if !defined(NDEBUG)
    struct Reb_Track track; // debug only for void/trash, BLANK!, LOGIC!, BAR!
#endif

    REBUNI character; // It's CHAR! (for now), but 'char' is a C keyword
    REBI64 integer;
    REBDEC decimal;

    REBVAL *pair; // actually a "pairing" pointer
    struct Reb_Money money;
    struct Reb_Handle handle;
    struct Reb_Time time;
    struct Reb_Tuple tuple;
    struct Reb_Datatype datatype;
    struct Reb_Typeset typeset;

    struct Reb_Library library;
    struct Reb_Struct structure; // It's STRUCT!, but 'struct' is a C keyword

    struct Reb_Event event;
    struct Reb_Gob gob;

    // These use `specific` or `relative` in `binding`, based on IS_RELATIVE()

    struct Reb_Any_Word any_word;
    struct Reb_Any_Series any_series;
    struct Reb_Function function;
    struct Reb_Any_Context any_context;
    struct Reb_Varargs varargs;

    // This is only used on the data stack as an internal type by the
    // evaluator, in order to find where not-yet-used refinements are, with
    // REB_0 (REB_0_PICKUP) as the type.
    //
    struct Reb_Pickup pickup;
};

struct Reb_Value
{
    struct Reb_Header header;
    union Reb_Value_Extra extra;
    union Reb_Value_Payload payload;
};


//=////////////////////////////////////////////////////////////////////////=//
//
//  Cell Reset and Copy Masks
//
//=////////////////////////////////////////////////////////////////////////=//
//
// It's important for operations that write to cells not to overwrite *all*
// the bits in the header, because some of those bits give information about
// the nature of the cell's storage and lifetime.  Similarly, if bits are
// being copied from one cell to another, those header bits must be masked
// out to avoid corrupting the information in the target cell.
//
// !!! Future optimizations may put the integer stack level of the cell in
// the header in the unused 32 bits for the 64-bit build.  That would also
// be kept in this mask.
//
// Additionally, operations that copy need to not copy any of those bits that
// are owned by the cell, plus additional bits that would be reset in the
// cell if overwritten but not copied.  For now, this is why `foo: :+` does
// not make foo an enfixed operation.
//
// Note that this will clear NODE_FLAG_FREE, so it should be checked by the
// debug build before resetting.
//

#define CELL_MASK_RESET \
    (NODE_FLAG_NODE | NODE_FLAG_CELL \
        | NODE_FLAG_MANAGED | VALUE_FLAG_STACK)

#define CELL_MASK_COPY \
    ~(CELL_MASK_RESET \
        | VALUE_FLAG_ENFIXED | VALUE_FLAG_PROTECTED | VALUE_FLAG_UNEVALUATED)


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

#define REB_MAX_VOID REB_MAX // there is no VOID! datatype, use REB_MAX

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
        // No required functionality should be implemented via the constructor
        // but optional debug features can be added.
        //
        Reb_Specific_Value () {
        }

        // The destructor checks that all REBVALs wound up with NODE_FLAG_CELL
        // set on them.  This would be done by DECLARE_LOCAL () if a stack
        // value, and by the Make_Series() construction for SERIES_FLAG_ARRAY.
        //
        ~Reb_Specific_Value() {
            assert(header.bits & NODE_FLAG_CELL);

            enum Reb_Kind kind = cast(enum Reb_Kind, RIGHT_8_BITS(header.bits));
            assert(
                header.bits & NODE_FLAG_FREE
                    ? kind == REB_MAX_VOID + 1
                    : kind <= REB_MAX_VOID
            );
        }

        // Overwriting one REBVAL* with another REBVAL* cannot be done with
        // a direct assignment, such as `*dest = *src;`  Instead one is
        // supposed to use `Move_Value(dest, src);` because the copying needs
        // to be sensitive to the nature of the target slot.  If that slot
        // is at a higher stack level than the source (or persistent in an
        // array) then special handling is necessary to make sure any stack
        // constrained pointers are "reified" 
        //
        // !!! Note that "= delete" only works in C++11, and can be achieved
        // less clearly but still work just by making assignment and copying
        // constructors private.
    private:
        Reb_Specific_Value (Reb_Specific_Value const & other);
        void operator= (Reb_Specific_Value const &rhs);
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
    //assert(NOT(GET_SER_FLAG(v->extra.binding, ARRAY_FLAG_VARLIST)));
    return cast(REBFUN*, v->extra.binding);
}

inline static REBCTX *VAL_SPECIFIC_COMMON(const RELVAL *v) {
    assert(IS_SPECIFIC(v));
    //assert(
    //    v->extra.binding == SPECIFIED
    //    || GET_SER_FLAG(v->extra.binding, ARRAY_FLAG_VARLIST)
    //);
    return cast(REBCTX*, v->extra.binding);
}

#ifdef NDEBUG
    #define VAL_SPECIFIC(v) \
        VAL_SPECIFIC_COMMON(v)
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

#define SPECIFIED NULL


#ifdef NDEBUG
    #define ASSERT_NO_RELATIVE(array,deep) NOOP
#else
    #define ASSERT_NO_RELATIVE(array,deep) \
        Assert_No_Relative((array),(deep))
#endif
