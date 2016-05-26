//
//  File: %c-value.c
//  Summary: "Generic REBVAL Support Services and Debug Routines"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2016 Rebol Open Source Contributors
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
// These are routines to support the macros and definitions in %sys-value.h
// which are not specific to any given type.  For the type-specific code,
// see files with names like %t-word.c, %t-logic.c, %t-integer.c...
//

#include "sys-core.h"


#if !defined(NDEBUG)

//
//  Panic_Value: C
//
// This is a debug-only "error generator", which will hunt through all the
// series allocations and panic on the series that contains the value (if
// it can find it).  This will allow those using Address Sanitizer or
// Valgrind to know a bit more about where the value came from.
//
// Additionally, if it happens to be trash, UNSET!, LOGIC!, BAR!, or NONE!
// it will dump out where the initialization happened if that information
// was stored.
//
ATTRIBUTE_NO_RETURN void Panic_Value(const REBVAL *value)
{
    REBSER *containing = Try_Find_Containing_Series_Debug(value);

#ifdef TRACK_EMPTY_PAYLOADS
    switch (value->header.bits & HEADER_TYPE_MASK) {
    case REB_TRASH:
    case REB_UNSET:
    case REB_NONE:
    case REB_LOGIC:
    case REB_BAR:
        Debug_Fmt(
            "REBVAL init on tick #%d at %s:%d",
            value->payload.track.count,
            value->payload.track.filename,
            value->payload.track.line
        );
        break;
    }
#endif

    Debug_Fmt("Kind=%d", cast(int, value->header.bits & HEADER_TYPE_MASK));

    if (containing) {
        Debug_Fmt("Containing series for value pointer found, panicking it:");
        Panic_Series(containing);
    }

    Debug_Fmt("No containing series for value...panicking to make stack dump");
    Panic_Array(EMPTY_ARRAY);
}


//
//  Assert_Cell_Writable: C
//
// If this check fails, then you're either writing to memory you shouldn't,
// or are writing to an "unformatted" stack value.  For instance:
//
//     REBVAL value;
//     SET_INTEGER(&value, 10);
//
// For REBVALs that don't live in series, you need to do:
//
//     REBVAL value;
//     VAL_INIT_WRITABLE_DEBUG(&value);
//     SET_INTEGER(&value, 10);
//
// The check helps avoid very bad catastrophies that might ensue if "implicit
// end markers" could be overwritten.  These are the ENDs that are actually
// pointers doing double duty inside a data structure, and there is no REBVAL
// storage backing the position.
//
// (A fringe benefit is catching writes to other unanticipated locations.)
//
void Assert_Cell_Writable(const REBVAL *v, const char *file, int line)
{
/*
    // REBVALs should not be written at addresses that do not match the
    // alignment of the processor.  This checks modulo the size of an unsigned
    // integer the same size as a platform pointer (REBUPT => uintptr_t)
    //
    assert(cast(REBUPT, (v)) % sizeof(REBUPT) == 0);

    if (NOT((v)->header.bits & WRITABLE_MASK_DEBUG)) {
        Debug_Fmt("Non-writable value found at %s:%d", file, line);
        Panic_Value(v);
    }
*/
}


//
//  IS_END_Debug: C
//
// The debug build puts REB_MAX in the type slot of a REB_END, to help to
// distinguish it from the 0 that signifies REB_TRASH.  This means that
// any writable value can be checked to ensure it is an actual END marker
// and not "uninitialized".  This trick can only be used so long as REB_MAX
// is 63 or smaller (ensured by an assertion at startup ATM.
//
// Note: a non-writable value (e.g. a pointer) could have any bit pattern in
// the type slot.  So only check if it's a Rebol-initialized value slot...
// and then, tolerate "GC safe trash" (an unset in release)
//
REBOOL IS_END_Debug(const REBVAL *v) {
    if (
        ((v)->header.bits & WRITABLE_MASK_DEBUG)
        && ((v)->header.bits & HEADER_TYPE_MASK) == REB_TRASH
        && NOT(GET_VAL_FLAG((v), TRASH_FLAG_SAFE))
    ) {
        Debug_Fmt("IS_END() called on value marked as uninitialized (TRASH!)");
        Panic_Value(v);
    }

    return LOGICAL((v)->header.bits % 2 == 0);
}


//
//  IS_CONDITIONAL_FALSE_Debug: C
//
// Variant of IS_CONDITIONAL_FALSE() macro for the debug build which checks to
// ensure you never call it on an UNSET!
//
REBOOL IS_CONDITIONAL_FALSE_Debug(const REBVAL *v)
{
    if (IS_END(v) || IS_VOID(v) || IS_TRASH_DEBUG(v)) {
        Debug_Fmt("Conditional true/false test on END or UNSET or TRASH");
        Panic_Value(v);
    }

    return GET_VAL_FLAG(v, VALUE_FLAG_FALSE);
}


//
//  VAL_TYPE_Debug: C
//
// Variant of VAL_TYPE() macro for the debug build which checks to ensure that
// you never call it on an END marker or on REB_TRASH.
//
enum Reb_Kind VAL_TYPE_Debug(const REBVAL *v, const char *file, int line)
{
    if (IS_END(v)) {
        //
        // Seeing a bit pattern that has the low bit to 0 may be a purposeful
        // end signal, or it could be something that's garbage data and just
        // happens to have its zero bit set.  Since half of all possible
        // bit patterns are even, it's more worth it than usual to point out.
        //
        Debug_Fmt("END marker (or garbage) in VAL_TYPE(), %s:%d", file, line);
        Panic_Value(v);
    }
    if (IS_TRASH_DEBUG(v)) {
        Debug_Fmt("Unexpected TRASH in VAL_TYPE(), %s:%d", file, line);
        Panic_Value(v);
    }
    return cast(enum Reb_Kind, (v)->header.bits & HEADER_TYPE_MASK);
}


//
//  Assert_Flags_Are_For_Value: C
//
// This check is used by GET_VAL_FLAG, SET_VAL_FLAG, CLEAR_VAL_FLAG to avoid
// accidentally checking or setting a type-specific flag on the wrong type
// of value in the debug build.
//
void Assert_Flags_Are_For_Value(const REBVAL *v, REBUPT f) {
    if ((f & HEADER_TYPE_MASK) == 0)
        return; // flag applies to any value (or trash)

    if ((f & HEADER_TYPE_MASK) == REB_FUNCTION) {
        assert(IS_FUNCTION(v));
    }
    else if ((f & HEADER_TYPE_MASK) == REB_OBJECT) {
        assert(ANY_CONTEXT(v));
    }
    else if ((f & HEADER_TYPE_MASK) == REB_WORD) {
        assert(ANY_WORD(v));
    }
}


//
//  VAL_SPECIFIC_Debug: C
//
REBCTX *VAL_SPECIFIC_Debug(const REBVAL *v)
{
    assert(IS_SPECIFIC(v));
    return (v)->payload.any_target.specific;
}


//
//  INIT_WORD_INDEX_Debug: C
//
void INIT_WORD_INDEX_Debug(REBVAL *v, REBCNT i)
{
    assert(ANY_WORD(v));
    assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND));
    if (IS_RELATIVE(v))
        assert(
            SAME_SYM(VAL_WORD_SYM(v), FUNC_PARAM_SYM(VAL_WORD_FUNC(v), i))
        );
    else
        assert(SAME_SYM(
            VAL_WORD_SYM(v), CTX_KEY_SYM(VAL_WORD_CONTEXT(v), i))
        );
    (v)->payload.any_word.place.binding.index = (i);
}


//  Probe_Core_Debug: C
//
// Debug function for outputting a value.  Done as a function instead of just
// a macro due to how easy it is with va_lists to order the types of the
// parameters wrong.  :-/
//
void Probe_Core_Debug(
    const char *msg,
    const char *file,
    int line,
    const REBVAL *val
) {
    if (msg)
        Debug_Fmt("\n** PROBE_MSG(\"%s\") %s:%d\n%r\n", msg, file, line, val);
    else
        Debug_Fmt("\n** PROBE() %s:%d\n%r\n", file, line, val);
}

#endif
